/*
r_postprocess.cpp - post-processing effects
Copyright (C) 2018 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "mathlib.h"
#include "r_shader.h"
#include "pm_defs.h"
#include "event_api.h"
#include "r_cvars.h"
#include "r_world.h"
#include "triangleapi.h"

#define TARGET_SIZE32	32
#define TARGET_SIZE		128
#define TARGET_SIZE256	256
#define TARGET_SIZE512	512

bool ApplyGaussBlur = false;
static int ScreenWaterTexture = 0;
static int ScreenAO1 = 0;
static int ScreenAO2 = 0;
static int target_rgb[2];
static int LuminanceTex = 0;
static int exposure_storage_texture[2];
static GLuint exposure_storage_fbo[2];
static int noise_texture = 0;
static int SMAA_AreaTex = 0;
static int SMAA_SearchTex = 0;
static int SMAA_AlbedoTex = 0;
static int SMAA_EdgeTex = 0;
static int SMAA_BlendTex = 0;
static GLuint SMAA_fbo;
static GLuint SSAOfbo;
static GLuint sunshafts_fbo;
static const float fAOScale = 0.5f;

// the scene renders into these instead of the default framebuffer, so post effects
// can ping-pong between two textures rather than copying the screen for every pass
static int pp_color[2];
static int pp_depth;
static GLuint pp_scene_fbo[2];	// color + depth, the scene keeps drawing here
static GLuint pp_post_fbo[2];	// color only, so passes can sample pp_depth
static GLuint pp_bloom_fbo;
static GLuint pp_native_fbo;
static int pp_cur;
static bool pp_active;

void RenderFSQ( int wide, int tall );

static GLuint PP_CreateFBO( int color, int depth )
{
	GLuint fbo;

	pglGenFramebuffers( 1, &fbo );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, color ), 0 );
	if( depth )
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, depth ), 0 );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglReadBuffer( GL_COLOR_ATTACHMENT0_EXT );
	ValidateFBO();

	return fbo;
}

static void PP_DeleteFBO( GLuint &fbo )
{
	if( fbo )
		pglDeleteFramebuffers( 1, &fbo );
	fbo = 0;
}

static void InitSceneTarget( void )
{
	for( int i = 0; i < 2; i++ )
	{
		PP_DeleteFBO( pp_scene_fbo[i] );
		PP_DeleteFBO( pp_post_fbo[i] );

		if( pp_color[i] )
			FREE_TEXTURE( pp_color[i] );
		pp_color[i] = 0;
	}

	PP_DeleteFBO( pp_native_fbo );

	if( pp_depth )
		FREE_TEXTURE( pp_depth );
	pp_depth = 0;

	pp_active = false;

	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return;

	pp_depth = CREATE_TEXTURE( "*scenedepth", glState.width, glState.height, NULL, TF_DEPTHBUFFER );

	for( int i = 0; i < 2; i++ )
	{
		pp_color[i] = CREATE_TEXTURE( va( "*scenecolor%d", i ), glState.width, glState.height, NULL, TF_COLORBUFFER );

		// same sampling as tr.screen_color, which the effects used to read
		GL_Bind( GL_TEXTURE0, pp_color[i] );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

		pp_scene_fbo[i] = PP_CreateFBO( pp_color[i], pp_depth );
		pp_post_fbo[i] = PP_CreateFBO( pp_color[i], 0 );
	}

	GL_Bind( GL_TEXTURE0, 0 );

	if( tr.screen_color_native )
		pp_native_fbo = PP_CreateFBO( tr.screen_color_native, 0 );

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
}

GLuint R_MainFramebuffer( void )
{
	return pp_active ? pp_scene_fbo[pp_cur] : 0;
}

void R_BeginSceneTarget( void )
{
	pp_cur = 0;
	pp_active = CVAR_TO_BOOL( gl_scenefbo ) && pp_scene_fbo[0] && !IsBuildingCubemaps();

	if( !pp_active )
		return;

	glState.frameBuffer = FBO_MAIN;
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, pp_scene_fbo[pp_cur] );
	GL_DepthMask( GL_TRUE );
	pglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	pglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}

void R_PresentSceneTarget( void )
{
	if( !pp_active )
		return;

	pp_active = false;
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );

	GL_Setup2D();
	GL_BindShader( NULL );
	GL_Bind( GL_TEXTURE0, pp_color[pp_cur] );
	RenderFSQ( glState.width, glState.height );
	GL_CleanUpTextureUnits( 0 );
}

// subviews like mirrors draw into their own framebuffers, those still need the old copies
static bool PP_Enabled( void )
{
	return pp_active && glState.frameBuffer == FBO_MAIN;
}

static int PP_Capture( int texture, int unit )
{
	if( PP_Enabled( ))
	{
		GL_Bind( unit, pp_color[pp_cur] );
		return pp_color[pp_cur];
	}

	GL_Bind( unit, texture );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );
	return texture;
}

static int PP_CaptureDepth( int unit )
{
	if( !PP_Enabled( ))
		return PP_Capture( tr.screen_depth, unit );

	GL_Bind( unit, pp_depth );
	return pp_depth;
}

static void PP_CaptureInto( int texture, GLuint fbo, int unit )
{
	if( PP_Enabled( ))
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo );
		pglViewport( 0, 0, glState.width, glState.height );
		GL_BindShader( NULL );
		GL_Bind( GL_TEXTURE0, pp_color[pp_cur] );
		RenderFSQ( glState.width, glState.height );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));
		GL_Bind( unit, texture );
		return;
	}

	GL_Bind( unit, texture );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );
}

static void PP_BeginPass( void )
{
	if( PP_Enabled( ))
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, pp_post_fbo[pp_cur ^ 1] );
}

static void PP_EndPass( void )
{
	if( !PP_Enabled( ))
		return;

	pp_cur ^= 1;
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, pp_scene_fbo[pp_cur] );
}

static void InitAutoExposure(void)
{
	// init average luminance
	const int mipmap_count = 1 + floor( log2( Q_max( glState.width, glState.height ) ) );

	if( tr.avg_luminance_texture )
	{
		FREE_TEXTURE( tr.avg_luminance_texture );
		tr.avg_luminance_texture = 0;
	}

	if( !tr.avg_luminance_texture )
		tr.avg_luminance_texture = CREATE_TEXTURE( "*avg_luminance_texture", glState.width, glState.height, NULL, TF_SCREEN );

	GL_Bind( GL_TEXTURE0, tr.avg_luminance_texture );
	pglGenerateMipmap( GL_TEXTURE_2D );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	GL_Bind( GL_TEXTURE0, 0 );

	// mips
	for( int i = 0; i < mipmap_count; i++ )
	{
		if( tr.avg_luminance_fbo_mip[i] == 0 )
			pglGenFramebuffers( 1, &tr.avg_luminance_fbo_mip[i] );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.avg_luminance_fbo_mip[i] );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM(PARM_TEX_TEXNUM, tr.avg_luminance_texture), i );
		ValidateFBO();
	}

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	
	const Vector color = Vector( 0.8f, 0.0f, 0.0f );

	for( int i = 0; i < 2; ++i )
	{
		if( exposure_storage_texture[i] )
		{
			FREE_TEXTURE( exposure_storage_texture[i] );
			exposure_storage_texture[i] = 0;
		}

		exposure_storage_texture[i] = CREATE_TEXTURE( va( "*exposure_storage%d", i ), 1, 1, NULL, TF_IMAGE | TF_ARB_16BIT );

		GL_Bind( GL_TEXTURE0, exposure_storage_texture[i] );
		pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGB, GL_FLOAT, &color.x );
		GL_Bind( GL_TEXTURE0, 0 );

		if( exposure_storage_fbo[i] == 0 )
			pglGenFramebuffers( 1, &exposure_storage_fbo[i] );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, exposure_storage_fbo[i] );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM(PARM_TEX_TEXNUM, exposure_storage_texture[i]), 0 );
	}

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
}

void InitPostEffects( void )
{
	
}

static void InitSSAO( void )
{	
	if( ScreenAO1 )
	{
		FREE_TEXTURE( ScreenAO1 );
		ScreenAO1 = 0;
	}

	if( !ScreenAO1 )
		ScreenAO1 = CREATE_TEXTURE( "*screenao1", glState.width * fAOScale, glState.height * fAOScale, NULL, TF_SCREEN );

	if( ScreenAO2 )
	{
		FREE_TEXTURE( ScreenAO2 );
		ScreenAO2 = 0;
	}

	if( !ScreenAO2 )
		ScreenAO2 = CREATE_TEXTURE( "*screenao2", glState.width * fAOScale, glState.height * fAOScale, NULL, TF_SCREEN );

	if( !SSAOfbo )
		pglGenFramebuffers( 1, &SSAOfbo );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, SSAOfbo );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, ScreenAO1 ), 0 ); // stub for init
	ValidateFBO();
}

static void InitSMAA( void )
{
	SMAA_AreaTex = LOAD_TEXTURE( "gfx/SMAA_AreaTex", NULL, 0, 0 );
	if( SMAA_AreaTex )
	{
		GL_Bind( GL_TEXTURE0, SMAA_AreaTex );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		GL_Bind( GL_TEXTURE0, 0 );
	}

	SMAA_SearchTex = LOAD_TEXTURE( "gfx/SMAA_SearchTex", NULL, 0, 0 );
	if( SMAA_SearchTex )
	{
		GL_Bind( GL_TEXTURE0, SMAA_SearchTex );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		GL_Bind( GL_TEXTURE0, 0 );
	}

	// runs before the upscale, at the rendering resolution
	const int w = glState.width;
	const int h = glState.height;

	if( SMAA_AlbedoTex )
	{
		FREE_TEXTURE( SMAA_AlbedoTex );
		SMAA_AlbedoTex = 0;
	}

	if( !SMAA_AlbedoTex )
	{
		SMAA_AlbedoTex = CREATE_TEXTURE( "*SMAA_AlbedoTex", w, h, NULL, 0 );
		GL_Bind( GL_TEXTURE0, SMAA_AlbedoTex );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_FLOAT, 0 );
		GL_Bind( GL_TEXTURE0, 0 );
	}

	if( SMAA_EdgeTex )
	{
		FREE_TEXTURE( SMAA_EdgeTex );
		SMAA_EdgeTex = 0;
	}

	if( !SMAA_EdgeTex )
	{
		SMAA_EdgeTex = CREATE_TEXTURE( "*SMAA_EdgeTex", w, h, NULL, 0 );
		GL_Bind( GL_TEXTURE0, SMAA_EdgeTex );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		pglTexImage2D( GL_TEXTURE_2D, 0, GL_RG8, w, h, 0, GL_RGBA, GL_FLOAT, 0 );
		GL_Bind( GL_TEXTURE0, 0 );
	}

	if( SMAA_BlendTex )
	{
		FREE_TEXTURE( SMAA_BlendTex );
		SMAA_BlendTex = 0;
	}

	if( !SMAA_BlendTex )
	{
		SMAA_BlendTex = CREATE_TEXTURE( "*SMAA_BlendTex", w, h, NULL, 0 );
		GL_Bind( GL_TEXTURE0, SMAA_BlendTex );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_FLOAT, 0 );
		GL_Bind( GL_TEXTURE0, 0 );
	}

	if( !SMAA_fbo )
		pglGenFramebuffers( 1, &SMAA_fbo );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, SMAA_fbo );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, SMAA_BlendTex ), 0 ); // stub for init
	ValidateFBO();

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
}

static void InitBloom( void )
{
	if( tr.screen_fbo_texture_color )
	{
		FREE_TEXTURE( tr.screen_fbo_texture_color );
		tr.screen_fbo_texture_color = 0;
	}

	if( !tr.screen_fbo_texture_color )
		tr.screen_fbo_texture_color = CREATE_TEXTURE( "*screen_temp_fbo_texture_color", glState.width, glState.height, NULL, TF_COLORBUFFER ); // was TF_HAS_ALPHA | TF_ARB_16BIT | TF_CLAMP | TF_ARB_FLOAT

	GL_Bind( GL_TEXTURE0, tr.screen_fbo_texture_color );
	pglGenerateMipmap( GL_TEXTURE_2D );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	GL_Bind( GL_TEXTURE0, 0 );

	// mips
	for( int i = 0; i < 6; i++ )
	{
		if( !tr.screen_fbo_mip[i] )
			pglGenFramebuffers( 1, &tr.screen_fbo_mip[i] );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.screen_fbo_mip[i] );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, tr.screen_fbo_texture_color ), i + 1 );
		ValidateFBO();
	}

	PP_DeleteFBO( pp_bloom_fbo );
	if( GL_Support( R_FRAMEBUFFER_OBJECT ))
		pp_bloom_fbo = PP_CreateFBO( tr.screen_fbo_texture_color, 0 );

	// may run mid-frame through tr.bDoInitBloom
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));

	tr.bDoInitBloom = false;
}

void InitPostTextures( void )
{
	// sunshafts shader
	if( target_rgb[0] )
	{
		FREE_TEXTURE( target_rgb[0] );
		target_rgb[0] = 0;
	}
	
	if( !target_rgb[0] )
	{
		if( tr.lowmemory )
			target_rgb[0] = CREATE_TEXTURE( "*target0", TARGET_SIZE, TARGET_SIZE, NULL, TF_SCREEN ); // 128
		else
			target_rgb[0] = CREATE_TEXTURE( "*target0", TARGET_SIZE512, TARGET_SIZE512, NULL, TF_SCREEN );
	}

	if( target_rgb[1] )
	{
		FREE_TEXTURE( target_rgb[1] );
		target_rgb[1] = 0;
	}

	if( !target_rgb[1] )
	{
		if( tr.lowmemory )
			target_rgb[1] = CREATE_TEXTURE( "*target1", TARGET_SIZE, TARGET_SIZE, NULL, TF_SCREEN ); // 128
		else
			target_rgb[1] = CREATE_TEXTURE( "*target1", TARGET_SIZE512, TARGET_SIZE512, NULL, TF_SCREEN );
	}

	if( !sunshafts_fbo )
		pglGenFramebuffers( 1, &sunshafts_fbo );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, sunshafts_fbo );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, target_rgb[0] ), 0 ); // stub for init
	ValidateFBO();

	if( ScreenWaterTexture )
	{
		FREE_TEXTURE( ScreenWaterTexture );
		ScreenWaterTexture = 0;
	}

	if( !ScreenWaterTexture )
		ScreenWaterTexture = LOAD_TEXTURE( "sprites/effects/screenwater.dds", NULL, 0, 0 );

	InitSSAO();

	InitBloom();

	InitAutoExposure();

	InitSMAA();

	InitSceneTarget();

	noise_texture = LOAD_TEXTURE( "gfx/noise.dds", NULL, 0, 0 );
}

void RenderFSQ( int wide, int tall )
{
	const float screenWidth = (float)wide;
	const float screenHeight = (float)tall;

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex2f( 0.0f, 0.0f );
		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex2f( screenWidth, 0.0f );
		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex2f( screenWidth, screenHeight );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex2f( 0.0f, screenHeight );
	pglEnd();
}

void RenderSunShafts( void )
{
	if( !CVAR_TO_BOOL( gl_sunshafts ) || !tr.screen_depth || !tr.screen_color || !target_rgb[0] )
		return;

	if( !CheckShader( glsl.genSunShafts ) || !CheckShader( glsl.drawSunShafts ) || !CheckShader( glsl.blurShader ) )
		return;

	if( !(world->features & WORLD_HAS_SKYBOX) )
		return; // don't waste time on tracing

	if( !tr.bSkySurfFound )
		return;

	if( gHUD.Weather_Intensity > 0.99f )
		return;

	float Brightness = 1.0f;

	if( tr.shader_modifier != NULL ) // mapper set custom settings
	{
		Brightness = tr.shader_modifier->curstate.fuser1;
		if( Brightness == 0.0f )
			return; // mapper disabled this shader in the modifier
	}

	if( gl_sunshafts_brightness->value > 0.0f )
	{
		// scale the brightness on this particular map
		Brightness *= gl_sunshafts_brightness->value;
	}

	const float ScaledWeatherBrightness = 1.0f - gHUD.Weather_Intensity;
	Brightness *= ScaledWeatherBrightness;

	const float blur = gl_sunshafts_blur->value;
	const float zFar = Q_max( 256.0f, RI->farClip );
	Vector skyColor = tr.sky_ambient * (1.0f / 128.0f) * r_lighting_modulate->value;
	const Vector skyVec = tr.sky_normal.Normalize();
	const Vector sunPos = RI->vieworg + skyVec * 1000.0;

	int TargetSize = TARGET_SIZE512;
	if( tr.lowmemory )
		TargetSize = TARGET_SIZE;

	if( skyVec == g_vecZero ) return;

	ColorNormalize( skyColor, skyColor );

	Vector ndc, view;

	// project sunpos to screen 
	R_TransformWorldToDevice( sunPos, ndc );
	R_TransformDeviceToScreen( ndc, view );

	view.z = DotProduct( -skyVec, RI->vforward );

	if( view.z < 0.01f ) return; // fade out

	// another multiplier...sigh
	if( gl_sunshafts_adaptive->value > 0 && RP_NORMALPASS() )
	{
		static float next_light_update = 0;
		static float lighting = 1.0f;
		static float PlayerLightingMultiplier = 1.0f;
		if( next_light_update > tr.time + 0.2f )
			next_light_update = 0;
		if( tr.time > next_light_update )
		{
			int maxLoops = 4;
			int ignoreent = -1;	// first, ignore no entity
			cl_entity_t *ent = NULL;
			Vector vecStart = tr.viewparams.vieworg;
			vecStart.z += 8.0f;
			Vector vecEnd = vecStart - skyVec * 100000;
			pmtrace_t trace;
			while( maxLoops > 0 )
			{
				gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
				gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, ignoreent, &trace );

				if( trace.ent <= 0 ) break; // we hit the world or nothing, stop trace

				ent = GET_ENTITY( PM_GetPhysEntInfo( trace.ent ) );
				if( ent == NULL ) break;

				if( ent->curstate.rendermode == kRenderNormal )
					break;

				if( ent->curstate.rendermode == kRenderTransTexture && ent->curstate.renderamt == 0 ) // invisible brush "collision", so it's likely a model
					break;

				if( ent->curstate.rendermode == kRenderTransColor && ent->curstate.renderamt == 255 )
					break;

				// if close enough to end pos, stop, otherwise continue trace
				if( (vecEnd - trace.endpos).Length() < 1.0f )
					break;
				else
				{
					ignoreent = trace.ent;	// ignore last hit entity
					vecStart = trace.endpos;
				}
				maxLoops--;
			}

			if( POINT_CONTENTS( trace.endpos ) == CONTENTS_SKY )
				PlayerLightingMultiplier = 1.0f;
			else // player likely in the shadow - calculate lighting around player
			{
				Vector light;
				gEngfuncs.pTriAPI->LightAtPoint( tr.viewparams.vieworg, light );
				light *= (1.0f / 255.0f);
				// gather dynamic lighting
			//	light += R_LightsForPoint( RI->vieworg, 256 );
				lighting = light.Length();
				PlayerLightingMultiplier = 1.5f * (1.0f - lighting);
				PlayerLightingMultiplier *= PlayerLightingMultiplier;
				PlayerLightingMultiplier += 0.75f;
			}

			next_light_update = tr.time + 0.2f;
		}
		static float mul = 0.0f;
		mul = lerp( mul, PlayerLightingMultiplier, g_fFrametime * 3.f );
		Brightness *= mul;
	//	gEngfuncs.Con_NPrintf( 1, "PlayerLightingMultiplier %f, total %f\n", mul, Brightness );
	}

	// request screen color
	const int screenColor = PP_Capture( tr.screen_color, GL_TEXTURE1 );

	// request screen depth
	PP_CaptureDepth( GL_TEXTURE0 );

	// set target viewport
	pglViewport( 0, 0, TargetSize, TargetSize );

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, sunshafts_fbo );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, target_rgb[0] ), 0 );

	// generate shafts
	GL_BindShader( glsl.genSunShafts );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_zFar, zFar );

	RenderFSQ( glState.width, glState.height );

#if 1
	// request target copy
	GL_Bind( GL_TEXTURE0, target_rgb[0] );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, target_rgb[1] ), 0 );

	// combine normal and blurred scenes
	GL_BindShader( glsl.blurShader );
	ASSERT( RI->currentshader != NULL );

	pglUniform3fARB( RI->currentshader->u_BlurFactor, blur, 0.0f, 1.0f );	// set blur factor X
	RenderFSQ( glState.width, glState.height );
	
	// request target copy
	GL_Bind( GL_TEXTURE0, target_rgb[1] );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, target_rgb[0] ), 0 );

	pglUniform3fARB( RI->currentshader->u_BlurFactor, 0.0f, blur, 0.0f );	// set blur factor Y
	RenderFSQ( glState.width, glState.height );
#endif
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.drawSunShafts );
	ASSERT( RI->currentshader != NULL );

	Vector4D sunshafts_params[2];
	// light origin + brightness
	sunshafts_params[0] = Vector4D( view.x / glState.width, view.y / glState.height, view.z, Brightness );
	// light diffuse
	sunshafts_params[1] = Vector4D( skyColor.x, skyColor.y, skyColor.z, 0.0f );
	pglUniform4fvARB( RI->currentshader->u_Sunshafts, 2, &sunshafts_params[0][0] );

	GL_Bind( GL_TEXTURE0, screenColor );
	GL_Bind( GL_TEXTURE1, target_rgb[0] );
	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_sunshafts_pass++;
}

void MotionBlur(void)
{
	static float drunk_amount = 0.0f;
	float Accum;

	if( gHUD.DrunkLevel > 0 || drunk_amount > 0.0f )
	{
		drunk_amount = CL_UTIL_Approach( gHUD.DrunkLevel * 0.1f, drunk_amount, g_fFrametime );
		Accum = drunk_amount;
	}
	else
	{
		if( !CVAR_TO_BOOL( r_blur ) )
			return;

		float PlayerVelocity = tr.viewparams.simvel.Length();
		const int Threshold = CVAR_GET_FLOAT( "r_blur_threshold" );

		// player is in car
		if( gEngfuncs.GetLocalPlayer()->curstate.vuser1.y > 0.0f )
			PlayerVelocity = gEngfuncs.GetLocalPlayer()->curstate.vuser1.y;

		if( Threshold > PlayerVelocity * 0.01 )
			return;

		Accum = PlayerVelocity * 0.0001 - Threshold * 0.01;
		Accum = bound( 0, Accum, 0.2 );
	}

	Vector org;
	R_TransformDeviceToScreen( g_vecZero, org ); // FIXME just center of the screen...

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.MotionBlur );
	ASSERT( RI->currentshader != NULL );

	pglUniform3fARB( RI->currentshader->u_MotionBlur, org.x / glState.width, org.y / glState.height, Accum );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_motionblur_pass++;
}

void GaussBlur( void )
{
	static float skyblur, screenblur, pausedblur, waterblur;

	const bool AllowBlur = CVAR_TO_BOOL( r_blur );

	if( !AllowBlur )
	{
		screenblur = 0;
		waterblur = 0;
		skyblur = 0;
	}

	if( IsBuildingCubemaps() )
		return;

	// this is asked by UI when saving the game, to get unblurred save preview image
	// it will always reset to 0 at the end of every frame
	if( CVAR_TO_BOOL( ui_forcenoblur ) )
		return;

	// blur when paused (menu is shown)
	if( RP_NORMALPASS() )
	{
		if( cl_background->value )
		{
			if( tr.time == tr.oldtime )
				pausedblur = 0.1f;
			else
				pausedblur -= 0.25f * g_fFrametime;
		}
		else
		{
			if( !CVAR_TO_BOOL( ui_videooptions_active ) && (tr.time == tr.oldtime || CVAR_TO_BOOL( ui_is_active )) )
				pausedblur += 0.5f * g_fFrametime;
			else
				pausedblur -= 0.5f * g_fFrametime;
		}

		pausedblur = bound( 0.0f, pausedblur, 0.1f );

		if( pausedblur > 0 )
		{
			RenderGaussBlur( pausedblur, pausedblur );
			return;
		}
	}

	if( RP_NORMALPASS() && AllowBlur )
	{
		if( tr.viewparams.waterlevel > 2 || (gHUD.InCar && (POINT_CONTENTS( tr.viewparams.vieworg ) == CONTENTS_WATER)) )
			waterblur += 0.1 * g_fFrametime;
		else
			waterblur -= 0.1 * g_fFrametime;

		waterblur = bound( 0, waterblur, 0.04 );

		RenderGaussBlur( waterblur, waterblur );

		if( waterblur > 0 )
		{
			screenblur = 0.0f; // need to reset the other blur
			return;
		}
	}

	if( RI->params & RP_SKYPORTALVIEW ) // FIXME only works when 3D sky is present
	{
		if( tr.shader_modifier != NULL )
		{
			// mapper disabled the 3D sky blur on this map
			if( tr.shader_modifier->curstate.iuser1 == 1 )
				return;
		}

		if( !tr.bSkySurfFound )
			return; // no sky surface was found this frame, don't waste time tracing

		if( !AllowBlur )
			return; // performance for old cards
		
		pmtrace_t ptr;
		Vector PlayerOrg, PlayerAngles, VecEnd, Forward;

		PlayerOrg = tr.viewparams.vieworg;
		PlayerAngles = tr.viewparams.viewangles;
		gEngfuncs.pfnAngleVectors( PlayerAngles, Forward, NULL, NULL );
		VecEnd = PlayerOrg + Forward * 100000;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( PlayerOrg, VecEnd, PM_GLASS_IGNORE, -1, &ptr );
		
		if( POINT_CONTENTS(ptr.endpos) == CONTENTS_SKY )
			skyblur -= 0.1 * g_fFrametime;
		else
			skyblur += 0.1 * g_fFrametime;

		skyblur = bound( 0, skyblur, 0.02 );
		RenderGaussBlur( skyblur, skyblur );
	}
	
	// full-screen blur when taking heavy damage
	if( RP_NORMALPASS() && AllowBlur )
	{
		if( gHUD.IsZooming )
			screenblur = 0.1;
		
		if( ApplyGaussBlur )
		{
			screenblur = gHUD.m_Health.damageTaken * 0.1;
			ApplyGaussBlur = false;
		}

		if( gHUD.IsZoomed && gHUD.m_CrosshairStatic.ZoomBlur > 0.0f )
		{
			screenblur = gHUD.m_CrosshairStatic.ZoomBlur;
		}

		if( screenblur > 0 )
			screenblur -= 0.75 * g_fFrametime;

		screenblur = bound( 0, screenblur, 0.5 );
		RenderGaussBlur( screenblur, screenblur );
	}
}

void RenderGaussBlur( float blur1, float blur2 )
{
	if( (blur1 <= 0.0f) && (blur2 <= 0.0f) )
		return;

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.blurShader );
	ASSERT( RI->currentshader != NULL );

	pglUniform3fARB( RI->currentshader->u_BlurFactor, blur1, 0.0f, 1.0f );	// set blur factor X

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglUniform3fARB( RI->currentshader->u_BlurFactor, 0.0f, blur2, 0.0f );	// set blur factor Y
	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_gaussblur_pass++;
}

void HorizontalBlur( void )
{
	static float Accum = 0.0f;

	if( Accum == 0.0f ) // make sure it's faded out
	{
		if( !CVAR_TO_BOOL( r_blur ) )
			return;

		if( RI->params & RP_THIRDPERSON )
			return;

		// no blur if < 30 fps
		if( g_fFrametime > 0.0333f )
			return;

		// do not allow this blur if we look too much up or down
		if( (tr.viewparams.viewangles.x < -45) || (tr.viewparams.viewangles.x > 45) )
			return;
	}

	float CurYaw = tr.viewparams.viewangles.y;
	if( CurYaw > 180.0f )
		CurYaw -= 360.0f;
	if( CurYaw < -180.0f )
		CurYaw += 360.0f;

	float PrevYaw = PrevViewAngles.y;
	if( PrevYaw > 180.0f )
		PrevYaw -= 360.0f;
	if( PrevYaw < -180.0f )
		PrevYaw += 360.0f;

	// this hopefully disables the circle-strafing blur - copied from source sdk
//-----------------------------------------------------------------------------------------------------------
	matrix3x4 mCurrentBasisVectors;
	AngleMatrix( tr.viewparams.viewangles, mCurrentBasisVectors );
	const Vector vPositionChange = PrevViewOrg - tr.viewparams.vieworg;
	const Vector vCurrentSideVec = Vector( mCurrentBasisVectors[0][1], mCurrentBasisVectors[1][1], mCurrentBasisVectors[2][1] );
	const float flSideDotMotion = DotProduct( vCurrentSideVec, vPositionChange );
	float flYawDiffOriginal = PrevYaw - CurYaw;
	if( ((PrevYaw - CurYaw > 180.0f) || (PrevYaw - CurYaw < -180.0f)) &&
		((PrevYaw + CurYaw > -180.0f) && (PrevYaw + CurYaw < 180.0f)) )
		flYawDiffOriginal = PrevYaw + CurYaw;

	float flYawDiffAdjusted = flYawDiffOriginal + (flSideDotMotion / 3.0f);

	// Make sure the adjustment only lessens the effect, not magnify it or reverse it
	if( flYawDiffOriginal < 0.0f )
		flYawDiffAdjusted = bound( flYawDiffOriginal, flYawDiffAdjusted, 0.0f );
	else
		flYawDiffAdjusted = bound( 0.0f, flYawDiffAdjusted, flYawDiffOriginal );
//-----------------------------------------------------------------------------------------------------------

	const float Strength = CVAR_GET_FLOAT( "r_blur_strength" );
	const float YawSpeed = fabs( (PrevYaw - CurYaw) / g_fFrametime );

	if( (YawSpeed > 10) && (fabs( flYawDiffAdjusted ) > 3) )
		Accum += YawSpeed * 0.01 * Strength * 0.005;// g_fFrametime;   hmm, 0.005 worked better than using frametime. some magic, I guess.
	else
	{
		if( Accum > 0 )
			Accum -= 20 * g_fFrametime;
	}

	Accum = bound( 0, Accum, 1.0 );

	if( Accum < 0.01f )
		return;

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.HorizontalBlur );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_Accum, Accum );	// set blur factor

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_horizontalblur_pass++;
}

void ScreenWater(void)
{
	float Speed = gEngfuncs.GetLocalPlayer()->curstate.vuser1.x;

	// special condition...
	if( gHUD.InCar && (POINT_CONTENTS( tr.viewparams.vieworg ) == CONTENTS_WATER) )
		Speed = 0.5f;

	if( tr.time == tr.oldtime ) // paused
		Speed = 0.0f;

	if( Speed <= 0.0f )
		return;

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.ScreenWater );
	ASSERT( RI->currentshader != NULL );

	pglUniform4fARB( RI->currentshader->u_ScreenWater, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height), tr.time, Speed );

	GL_Bind( GL_TEXTURE1, ScreenWaterTexture );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_screenwater_pass++;
}

void Glitch( void )
{
	if( gHUD.GlitchAmount > 0.0f && tr.time > gHUD.GlitchHoldTime )
		gHUD.GlitchAmount = CL_UTIL_Approach( 0.0f, gHUD.GlitchAmount, g_fFrametime );
	
	if( gHUD.GlitchAmount <= 0.0f )
		return;

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.Glitch );
	ASSERT( RI->currentshader != NULL );

	pglUniform4fARB( RI->currentshader->u_Glitch, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height), tr.time, gHUD.GlitchAmount );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_glitch_pass++;
}

void Monochrome( void )
{
	const float MonochromeState = gEngfuncs.GetLocalPlayer()->curstate.vuser1.z;

	float CurrentMonochrome = 0.0f;

	if( MonochromeState > 0.0f ) // user plays in monochrome, so only use this
		CurrentMonochrome = MonochromeState;
	
	// apply monochrome anyway if near death, but only when not in camera, and only if has suit
	if( gHUD.HasWeapon( WEAPON_SUIT ) 
		&& !(gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERUSINGCAMERA)
		&& !(gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERDRONECONTROL )
		&& !g_iUser1
		&& !( gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH ) )
	{
		if( gHUD.m_HealthVisual.m_iHealth < 25 )
		{
			CurrentMonochrome = 4.0f / ((float)gHUD.m_HealthVisual.m_iHealth + 0.1f);
		}
	}

	CurrentMonochrome = bound( 0, CurrentMonochrome, 1 );

	if( CurrentMonochrome <= 0.0f )
		return;

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.Monochrome );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_Accum, CurrentMonochrome );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_monochrome_pass++;
}

void SSAO( void )
{
	if( (!CVAR_TO_BOOL( gl_ssao ) && !CVAR_TO_BOOL( gl_hbao )) || tr.lowmemory )
		return;

	const bool Debug = CVAR_TO_BOOL( gl_ssao_debug );
	const bool UseHBAO = (gl_hbao->value > 0);

	const float zFar = RI->farClip;

	// request screen color
	const int screenColor = PP_Capture( tr.screen_color, GL_TEXTURE0 );

	// request screen depth
	const int screenDepth = PP_CaptureDepth( GL_TEXTURE0 );

	// set target viewport
	const int width = glState.width * fAOScale;
	const int height = glState.height * fAOScale;

	pglViewport( 0, 0, width, height );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, SSAOfbo );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, ScreenAO1 ), 0 );

	if( !UseHBAO ) // SSAO
	{
		// generate SSAO
		GL_BindShader( glsl.genSSAO );
		ASSERT( RI->currentshader != NULL );

		pglUniform1fARB( RI->currentshader->u_zFar, zFar );
	}
	else // HBAO
	{
		// generate HBAO
		GL_BindShader( glsl.genHBAO );
		ASSERT( RI->currentshader != NULL );

		Vector2D hbao_params[5];
		// focal length
		const float tanHalfFovY = tanf( DEG2RAD( RI->fov_y ) * 0.5f );
		hbao_params[0].x = 1.0f / tanHalfFovY * ((float)glState.height / (float)glState.width);
		hbao_params[0].y = 1.0f / tanHalfFovY;

		Vector2D InvFocalLength;
		InvFocalLength.x = 1.0f / hbao_params[0].x;
		InvFocalLength.y = 1.0f / hbao_params[0].y;
		// UVToViewA
		hbao_params[1].x = -2.0f * InvFocalLength.x;
		hbao_params[1].y = -2.0f * InvFocalLength.y;
		// UVToViewB
		hbao_params[2].x = 1.0f * InvFocalLength.x;
		hbao_params[2].y = 1.0f * InvFocalLength.y;
		// LinMAD
		const float znear = Z_NEAR;
		hbao_params[3].x = (znear - zFar) / (2.0f * znear * zFar);
		hbao_params[3].y = (znear + zFar) / (2.0f * znear * zFar);
		// screen resolution
		hbao_params[4].x = glState.width;
		hbao_params[4].y = glState.height;

		pglUniform2fvARB( RI->currentshader->u_HBAOParams, 5, &hbao_params[0][0] );
	}

	RenderFSQ( glState.width, glState.height );

	GL_Bind( GL_TEXTURE0, ScreenAO1 );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, ScreenAO2 ), 0 );

	// blur AO pass X
	GL_BindShader( glsl.BilateralBlur );
	GL_Bind( GL_TEXTURE1, screenDepth );
	ASSERT( RI->currentshader != NULL );
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / glState.width, 0.0f ); // screen size inv
	pglUniform1fARB( RI->currentshader->u_zFar, zFar );
	RenderFSQ( glState.width, glState.height );
	
	GL_Bind( GL_TEXTURE0, ScreenAO2 );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, ScreenAO1 ), 0 );

	// blur AO pass Y
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 0.0f, 1.0f / glState.height ); // screen size inv
	RenderFSQ( glState.width, glState.height );

	// back to base FBO
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));

	// do final pass
	pglViewport( 0, 0, glState.width, glState.height );
	GL_BindShader( glsl.drawSSAO );
	ASSERT( RI->currentshader != NULL );

	if( tr.fogEnabled )
		pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
	else
		pglUniform4fARB( RI->currentshader->u_FogParams, 0.0f, 0.0f, 0.0f, 0.0f );
	pglUniform1fARB( RI->currentshader->u_GenericCondition, Debug );
	pglUniform1fARB( RI->currentshader->u_zFar, zFar );

	GL_Bind( GL_TEXTURE0, screenColor );
	GL_Bind( GL_TEXTURE1, ScreenAO1 );
	GL_Bind( GL_TEXTURE2, screenDepth );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	// unbind shader
	GL_BindShader( NULL );
	GL_CleanupAllTextureUnits();

	r_stats.sh_ssao_pass++;
}

void Bloom( void )
{
	if( !CVAR_TO_BOOL( r_bloom ) )
		return;

	if( IsBuildingCubemaps() ) // no bloom in cubemaps
		return;

	if( !tr.screen_fbo_texture_color )
		InitBloom();

	if( tr.bDoInitBloom ) // forced to restart for whatever reasons
		InitBloom();

	PP_CaptureInto( tr.screen_fbo_texture_color, pp_bloom_fbo, GL_TEXTURE0 );

	GL_BindShader( glsl.BlurMip );
	ASSERT( RI->currentshader != NULL );

	int w = glState.width;
	int h = glState.height;

	pglUniform4fARB( RI->currentshader->u_TexCoordClamp, 0.0f, 0.0f, 1.0f, 1.0f );

	// render and blur mips
	for( int i = 0; i < 6; i++ )
	{
		w /= 2;
		h /= 2;

		w = Q_max( w, 1 );
		h = Q_max( h, 1 );

		pglViewport( 0, 0, w, h );

		pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)w, 1.0f / (float)h );
		pglUniform1fARB( RI->currentshader->u_MipLod, (float)i );
		pglUniform1fARB( RI->currentshader->u_BloomFirstPass, (i < 1) ? 1 : 0 );

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.screen_fbo_mip[i] );
		RenderFSQ( glState.width, glState.height );
	}

	w = glState.width / 2;
	h = glState.height / 2;

	pglViewport( 0, 0, w, h );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.screen_fbo_mip[0] );

	GL_BindShader( glsl.Bloom );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, (float)w, (float)h );
	RenderFSQ( glState.width, glState.height );

	// blend screen image & first mip together
	pglViewport( 0, 0, glState.width, glState.height );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));
	GL_BindShader( NULL );
	GL_Blend( GL_TRUE );
	GL_BlendFunc( GL_ONE, GL_ONE );
	pglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 1.0 );
	RenderFSQ( glState.width, glState.height );
	pglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0.0 );
	GL_Blend( GL_FALSE );
	GL_CleanUpTextureUnits( 0 );
	GL_BindShader( NULL );
}

int RenderExposureStorage(void)
{
	static int Index = 0;
	const int sourceIndex = Index % 2;
	const int destIndex = (Index + 1) % 2;
	const int mipmap_count = 1 + floor( log2( Q_max( glState.width, glState.height ) ) );

	GL_BindShader( glsl.GenExposure );
	ASSERT( RI->currentshader != NULL );

	Index = (Index + 1) % 2;

	GL_Bind( GL_TEXTURE0, tr.avg_luminance_texture );
	GL_Bind( GL_TEXTURE1, exposure_storage_texture[sourceIndex] );
	pglUniform1fARB( RI->currentshader->u_MipLod, (float)(mipmap_count - 1) );
	pglUniform1fARB( RI->currentshader->u_TimeDelta, (float)g_fFrametime );

	pglViewport( 0, 0, 1, 1 );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, exposure_storage_fbo[destIndex] );
	RenderFSQ( glState.width, glState.height );

	// restore GL state
	pglViewport( 0, 0, glState.width, glState.height );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));
	GL_SelectTexture( glConfig.max_texture_units - 1 );
	GL_CleanUpTextureUnits( 0 );

	return exposure_storage_texture[destIndex];
}

void ToneMap(void)
{
	if( !CVAR_TO_BOOL( gl_exposure ) )
		return;

	if( IsBuildingCubemaps() ) // no exposure change in cubemaps
		return;

	if( R_FullBright() || r_lightmap->value )
		return;

	const int mipmap_count = 1 + floor( log2( Q_max( glState.width, glState.height ) ) );

	// render luminance to first mip
	PP_Capture( tr.avg_luminance_texture, GL_TEXTURE0 );

	GL_BindShader( glsl.GenLuminance );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.avg_luminance_fbo_mip[0] );
	RenderFSQ( glState.width, glState.height );

	// downscale luminance to other mips
	GL_BindShader( glsl.BlurMip );
	ASSERT( RI->currentshader != NULL );

	int mip_width = glState.width;
	int mip_height = glState.height;

	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );
	pglUniform4fARB( RI->currentshader->u_TexCoordClamp, 0.0f, 0.0f, 1.0f, 1.0f );
	pglUniform1fARB( RI->currentshader->u_BloomFirstPass, 0.0f );

	for( int i = 1; i < mipmap_count; i++ )
	{
		mip_width /= 2;
		mip_height /= 2;

		mip_width = Q_max( mip_width, 1 );
		mip_height = Q_max( mip_height, 1 );

		pglViewport( 0, 0, mip_width, mip_height );

		pglUniform1fARB( RI->currentshader->u_MipLod, (float)(i - 1) );

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.avg_luminance_fbo_mip[i] );
		RenderFSQ( glState.width, glState.height );
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 );
	GL_CleanUpTextureUnits( 0 );
	pglViewport( 0, 0, glState.width, glState.height );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));

	// capture screen
	const int screenColor = PP_Capture( tr.screen_color, GL_TEXTURE0 );

	const int ExposureTexture = RenderExposureStorage();

	GL_BindShader( glsl.ToneMap );
	ASSERT( RI->currentshader != NULL );

	GL_Bind( GL_TEXTURE0, screenColor );
	GL_Bind( GL_TEXTURE1, ExposureTexture ); // u_HDRExposure
	const float UseTonemap = (gl_tonemap->value > 0) ? 1.0f : 0.0f;
	pglUniform1fARB( RI->currentshader->u_GenericCondition, UseTonemap );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_SelectTexture( glConfig.max_texture_units - 1 );
	GL_CleanUpTextureUnits( 0 );
}

void Enhance( void )
{
	if( IsBuildingCubemaps() )
		return;

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.Enhance );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );
}

void FSR( void )
{
	if( gl_fsr->value <= 0.0f )
		return;

	if( IsBuildingCubemaps() )
		return;

	const int w = RENDER_GET_PARM( PARM_SCREEN_WIDTH, 0 );
	const int h = RENDER_GET_PARM( PARM_SCREEN_HEIGHT, 0 );

	GL_Setup2D();

	const bool scene = PP_Enabled();
	int upscaled = tr.screen_color_native;
	if( gl_renderscale->value >= 1.0f )
		upscaled = scene ? pp_color[pp_cur ^ 1] : tr.screen_color;

	// get low resolution screen copy
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	if( scene )
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, gl_renderscale->value < 1.0f ? pp_native_fbo : pp_post_fbo[pp_cur ^ 1] );

	// render through full native viewport
	pglViewport( 0, 0, w, h );

	GL_BindShader( glsl.FSR_easu );
	ASSERT( RI->currentshader != NULL );

	// native screen size (not inverted)
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, w, h );
	// rendering resolution
	pglUniform2fARB( RI->currentshader->u_GenericCondition, (float)(glState.width), (float)(glState.height) );

	RenderFSQ( w, h );

	// get native full screen copy
	GL_Bind( GL_TEXTURE0, upscaled );
	if( scene )
	{
		// sharpening writes the final image straight to the screen
		pp_active = false;
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	}
	else
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h );

	GL_BindShader( glsl.FSR_rcas );
	ASSERT( RI->currentshader != NULL );

	// native screen size (not inverted)
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, w, h );

	RenderFSQ( w, h );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );
}

void SMAA( void )
{
	if( gl_smaa->value <= 0 )
		return;

	const int w = glState.width;
	const int h = glState.height;

	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglPushMatrix();
	pglLoadIdentity();
	pglOrtho( 0, w, h, 0, -99999, 99999 );
	pglMatrixMode( GL_MODELVIEW );
	pglPushMatrix();
	pglLoadIdentity();

	// 1: edge detection

	const int albedo = PP_Capture( SMAA_AlbedoTex, GL_TEXTURE0 );

	pglViewport( 0, 0, w, h );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, SMAA_fbo );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, SMAA_EdgeTex ), 0 );

	GL_BindShader( glsl.SMAAEdgeDetect );
	ASSERT( RI->currentshader != NULL );

	pglUniform4fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)w, 1.0f / (float)h, (float)w, (float)h );
	RenderFSQ( w, h );
	
	// 2: blending weights

	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, SMAA_BlendTex ), 0 );

	GL_BindShader( glsl.SMAABlendWeight );
	ASSERT( RI->currentshader != NULL );

	GL_Bind( GL_TEXTURE0, SMAA_EdgeTex );
	GL_Bind( GL_TEXTURE1, SMAA_SearchTex );
	GL_Bind( GL_TEXTURE2, SMAA_AreaTex );

	pglUniform4fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)w, 1.0f / (float)h, (float)w, (float)h );
	RenderFSQ( w, h );
	
	// back to base FBO
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, R_MainFramebuffer( ));

	// 3: neighbor blending

	GL_BindShader( glsl.SMAANeighborBlend );
	ASSERT( RI->currentshader != NULL );

	GL_Bind( GL_TEXTURE0, albedo );
	GL_Bind( GL_TEXTURE1, SMAA_BlendTex );

	pglUniform4fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)w, 1.0f / (float)h, (float)w, (float)h );
	PP_BeginPass();
	RenderFSQ( w, h );
	PP_EndPass();

	// debug
//	GL_Bind( GL_TEXTURE0, SMAA_BlendTex );
//	RenderFSQ( w, h );

	GL_BindShader( NULL );
	GL_CleanupAllTextureUnits();

	pglMatrixMode( GL_PROJECTION );
	pglPopMatrix();
	pglMatrixMode( GL_MODELVIEW );
	pglPopMatrix();
}

void DownScale( void )
{
	if( gl_renderscale->value >= 1.0f )
		return;

	GL_Setup2D();

	const float w = RENDER_GET_PARM( PARM_SCREEN_WIDTH, 0 );
	const float h = RENDER_GET_PARM( PARM_SCREEN_HEIGHT, 0 );

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	if( PP_Enabled( ))
	{
		pp_active = false;
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	}

	pglViewport( 0, 0, w, h );
	pglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	pglClear( GL_COLOR_BUFFER_BIT );

	RenderFSQ( glState.width, glState.height );

	GL_CleanUpTextureUnits( 0 );
}

void LensFlare( void )
{
	if( gl_lensflare->value <= 0 )
		return;
	
	if( !(world->features & WORLD_HAS_SKYBOX) )
		return; // don't waste time on tracing

	// mapper disabled the lensflare on this map
	if( tr.shader_modifier && tr.shader_modifier->curstate.iuser2 == 1 )
		return;

	if( !tr.bSkySurfFound )
		return; // no sky surface was found this frame, don't waste time tracing

	if( gHUD.Weather_Intensity >= 0.99f )
		return; // complete storm (rain/snow), no lensflare for you!

	Vector sunangles, sundir, suntarget;
	Vector forward, right, up, screen;

	// Sun position
	VectorAngles( -tr.sky_normal, sunangles );

	const Vector v_angles = tr.viewparams.viewangles;
	const Vector v_origin = tr.viewparams.vieworg;
	AngleVectors( v_angles, forward, NULL, NULL );
	AngleVectors( sunangles, sundir, NULL, NULL );
	suntarget = v_origin + sundir * 135000; // maps can be this big :)

	const float DotP = DotProduct( forward, sundir );
	if( DotP < 0.0f )
		gHUD.LensFlareAlpha = 0.0f;

	if( gHUD.LensFlareAlpha <= 0.0f && DotP < 0.8f )
		return;

	int maxLoops = 4;
	int ignoreent = -1;	// first, ignore no entity
	cl_entity_t *ent = NULL;
	Vector vecStart = v_origin;
	pmtrace_t trace;
	while( maxLoops > 0 )
	{
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, suntarget, PM_TRACELINE_PHYSENTSONLY, ignoreent, &trace );

		if( trace.ent <= 0 ) break; // we hit the world or nothing, stop trace

		ent = GET_ENTITY( PM_GetPhysEntInfo( trace.ent ) );
		if( ent == NULL ) break;

		if( ent->curstate.rendermode == kRenderNormal )
			break;

		if( ent->curstate.rendermode == kRenderTransTexture && ent->curstate.renderamt == 0 ) // invisible brush "collision", so it's likely a model
			break;

		if( ent->curstate.rendermode == kRenderTransColor && ent->curstate.renderamt == 255 )
			break;

		// if close enough to end pos, stop, otherwise continue trace
		if( (suntarget - trace.endpos).Length() < 1.0f )
			break;
		else
		{
			ignoreent = trace.ent;	// ignore last hit entity
			vecStart = trace.endpos;
		}
		maxLoops--;
	}

	if( POINT_CONTENTS( trace.endpos ) == CONTENTS_SKY )
		gHUD.LensFlareAlpha += 5.0f * g_fFrametime;
	else
		gHUD.LensFlareAlpha -= 10.0f * g_fFrametime;

	gHUD.LensFlareAlpha = bound( 0.0f, gHUD.LensFlareAlpha, 1.0f );

	if( gHUD.LensFlareAlpha <= 0.01f )
		return;

	Vector ndc, view;
	// project sunpos to screen 
	R_TransformWorldToDevice( suntarget, ndc );
	R_TransformDeviceToScreen( ndc, view );

	GL_Blend( GL_TRUE );
	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE ); // additive

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.LensFlare );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, (float)(glState.width), (float)(glState.height) );
	pglUniform1fARB( RI->currentshader->u_Accum, gHUD.LensFlareAlpha * pow( DotP, 10 ) * (1.0f - gHUD.Weather_Intensity) * tr.sunlightscale );
	pglUniform2fARB( RI->currentshader->u_LightOrigin, view.x, view.y );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );
	GL_Blend( GL_FALSE );
}

void WaterDrops( void )
{
	// player is in car
	if( gEngfuncs.GetLocalPlayer()->curstate.vuser1.y > 0.0f )
		gHUD.ScreenDrips_Visible = false;

	const bool Override = (gHUD.ScreenDrips_OverrideTime > tr.time);

	// do not allow water drips if we look too much down
	if( !Override && tr.viewparams.viewangles.x > 30 )
		gHUD.ScreenDrips_Visible = false;

	// https://www.youtube.com/watch?v=Bu8bH2P37kY
	if( tr.viewparams.waterlevel > 2 )
	{
		gHUD.ScreenDrips_CurVisibility = 0.0f;
		return;
	}
	
	if( gHUD.ScreenDrips_Visible )
	{
		if( gHUD.ScreenDrips_CurVisibility < 1.0f )
			gHUD.ScreenDrips_CurVisibility += 0.25 * g_fFrametime;
	}
	else
	{
		if( gHUD.ScreenDrips_CurVisibility > 0.0f )
			gHUD.ScreenDrips_CurVisibility -= 0.75f * g_fFrametime; // fade out fast
	}

	gHUD.ScreenDrips_CurVisibility = bound( 0, gHUD.ScreenDrips_CurVisibility, 1 );

	if( gHUD.ScreenDrips_CurVisibility <= 0.0f )
		return;

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.WaterDrops );
	ASSERT( RI->currentshader != NULL );

	Vector waterdrops_params[2];
	// screensizeinv + realtime
	waterdrops_params[0] = Vector( (float)glState.width, (float)glState.height, tr.time );
	// visibility + intensity
	waterdrops_params[1] = Vector( gHUD.ScreenDrips_CurVisibility, gHUD.ScreenDrips_DripIntensity, 0.0f );
	pglUniform3fvARB( RI->currentshader->u_WaterDrops, 2, &waterdrops_params[0][0] );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_waterdrops_pass++;
}

void DroneScreenShader( void )
{
	if( !(gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERDRONECONTROL) )
		return;

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERUSINGCAMERA )
		return;

	const float out_amount = 1.5f - (gHUD.m_DroneBars.DroneHealth / (float)DRONE_MAX_HEALTH);

	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	GL_Bind( GL_TEXTURE1, noise_texture );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.DroneScreen );
	ASSERT( RI->currentshader != NULL );

	pglUniform4fARB( RI->currentshader->u_DroneScreen, (float)(glState.width), (float)(glState.height), tr.time, out_amount );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );
}

void HeatDistortionShader( void )
{
	if( gl_heateffect_force->value <= 0 )
	{
		if( tr.viewparams.waterlevel == 3 )
			return;

		if( !tr.shader_modifier || tr.shader_modifier->curstate.sequence <= 0 )
			return;
	}

	const float angle = fabs( tr.viewparams.viewangles.x );

	// do not allow this effect if we look too much up or down
	if( angle > 45 )
		return;

	// otherwise scale it
	const float scale = 1.0f - (angle / 45.0f);
	
	// capture screen
	PP_Capture( tr.screen_color, GL_TEXTURE0 );

	// request screen depth
	PP_CaptureDepth( GL_TEXTURE1 );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.HeatDistortion );
	ASSERT( RI->currentshader != NULL );

	pglUniform4fARB( RI->currentshader->u_GenericCondition, (float)(glState.width), (float)(glState.height), tr.time, scale );

	PP_BeginPass();
	RenderFSQ( glState.width, glState.height );
	PP_EndPass();

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );
}
