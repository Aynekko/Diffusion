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
#include "r_view.h"
#include "r_cvars.h"

void AngleMatrix( const Vector &angles, matrix3x4 &matrix )
{
	float sr, sp, sy, cr, cp, cy;

	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
	SinCos( DEG2RAD( angles[ROLL] ), &sr, &cr );

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp * cy;
	matrix[1][0] = cp * sy;
	matrix[2][0] = -sp;

	float crcy = cr * cy;
	float crsy = cr * sy;
	float srcy = sr * cy;
	float srsy = sr * sy;
	matrix[0][1] = sp * srcy - crsy;
	matrix[1][1] = sp * srsy + crcy;
	matrix[2][1] = sr * cp;

	matrix[0][2] = (sp * crcy + srsy);
	matrix[1][2] = (sp * crsy - srcy);
	matrix[2][2] = cr * cp;

	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}

#define TARGET_SIZE32	32
#define TARGET_SIZE		128
#define TARGET_SIZE256	256
#define TARGET_SIZE512	512

bool ApplyGaussBlur = false;
bool ApplyDamageMonochrome = false;
int ScreenWaterTexture = 0;
int ScreenAO = 0;
int RotateMap = 0;
int LuminanceTex = 0;
int exposure_storage_texture[2];
uint exposure_storage_fbo[2];

void InitAutoExposure(void)
{
	// init average luminance
	const int mipmap_count = 1 + floor( log2( Q_max( glState.width, glState.height ) ) );

	if( tr.avg_luminance_texture )
	{
		FREE_TEXTURE( tr.avg_luminance_texture );
		tr.avg_luminance_texture = 0;
	}

	if( !tr.avg_luminance_texture )
		tr.avg_luminance_texture = CREATE_TEXTURE( "*avg_luminance_texture", glState.width, glState.height, NULL, TF_COLORBUFFER ); // was TF_CLAMP | TF_HAS_ALPHA | TF_ARB_16BIT

	GL_Bind( GL_TEXTURE0, tr.avg_luminance_texture );
	pglGenerateMipmap( GL_TEXTURE_2D );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	GL_Bind( GL_TEXTURE0, 0 );

	// mips
	for( int i = 0; i < mipmap_count; ++i )
	{
		if( tr.avg_luminance_fbo_mip[i] == 0 )
			pglGenFramebuffers( 1, &tr.avg_luminance_fbo_mip[i] );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.avg_luminance_fbo_mip[i] );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM(PARM_TEX_TEXNUM, tr.avg_luminance_texture), i );
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

void InitSSAO( void )
{
	if( gl_ssao->value <= 0 )
	{
		if( ScreenAO )
		{
			FREE_TEXTURE( ScreenAO );
			ScreenAO = 0;
		}

		return;
	}

	int width = glState.width;
	int height = glState.height;

	if( tr.lowmemory )
	{
		width = glState.width / 4;
		height = glState.height / 4;
	}
	else
	{
		switch( (int)gl_ssao->value )
		{
		case 1:
			width = glState.width / 4;
			height = glState.height / 4;
			break;
		case 2:
			width = glState.width / 2;
			height = glState.height / 2;
			break;
		}
	}
	
	if( ScreenAO )
	{
		FREE_TEXTURE( ScreenAO );
		ScreenAO = 0;
	}

	if( !ScreenAO )
		ScreenAO = CREATE_TEXTURE( "*screenao", width, height, NULL, TF_COLORBUFFER );
}

void InitPostTextures( void )
{
	if( tr.screen_depth )
	{
		FREE_TEXTURE( tr.screen_depth );
		tr.screen_depth = 0;
	}
	
	if( !tr.screen_depth )
		tr.screen_depth = CREATE_TEXTURE( "*screendepth", glState.width, glState.height, NULL, TF_DEPTHBUFFER ); 

	if( tr.screen_color )
	{
		FREE_TEXTURE( tr.screen_color );
		tr.screen_color = 0;
	}

	if( !tr.screen_color )
		tr.screen_color = CREATE_TEXTURE( "*screencolor", glState.width, glState.height, NULL, TF_COLORBUFFER );

	if( tr.target_rgb[0] )
	{
		FREE_TEXTURE( tr.target_rgb[0] );
		tr.target_rgb[0] = 0;
	}
	
	if( !tr.target_rgb[0] )
	{
		if( tr.lowmemory )
			tr.target_rgb[0] = CREATE_TEXTURE( "*target0", TARGET_SIZE, TARGET_SIZE, NULL, TF_SCREEN ); // 128
		else
			tr.target_rgb[0] = CREATE_TEXTURE( "*target0", TARGET_SIZE512, TARGET_SIZE512, NULL, TF_SCREEN );
	}

	if( ScreenWaterTexture )
	{
		FREE_TEXTURE( ScreenWaterTexture );
		ScreenWaterTexture = 0;
	}

	if( !ScreenWaterTexture )
		ScreenWaterTexture = LOAD_TEXTURE( "sprites/effects/screenwater.dds", NULL, 0, 0 );

	InitSSAO();

	if( RotateMap )
	{
		FREE_TEXTURE( RotateMap );
		RotateMap = 0;
	}

	if( !RotateMap )
		RotateMap = LOAD_TEXTURE( "sprites/effects/rotate.tga", NULL, 0, 0 );	

	// ----------------- bloom -------------------
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
	if( !tr.screen_fbo_mip[0] )
		pglGenFramebuffers( 6, tr.screen_fbo_mip );

	for( int i = 0; i < 6; i++ )
	{
		if( tr.screen_fbo_mip[i] <= 0 )
			pglGenFramebuffers( 1, &tr.screen_fbo_mip[i] );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.screen_fbo_mip[i] );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM(PARM_TEX_TEXNUM, tr.screen_fbo_texture_color), i + 1 );
	}

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	// -----------------------------------------

	InitAutoExposure();
}

void RenderFSQ( int wide, int tall )
{
	float screenWidth = (float)wide;
	float screenHeight = (float)tall;

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
	if( !CVAR_TO_BOOL( gl_sunshafts ) || !tr.screen_depth || !tr.screen_color || !tr.target_rgb[0] )
		return;

	if( !CheckShader( glsl.genSunShafts ) || !CheckShader( glsl.drawSunShafts ))
		return;

	if( !CheckShader( glsl.blurShader[0] ) || !CheckShader( glsl.blurShader[1] ))
		return;

	float Brightness = 0.0f; // don't worry, shader sets to 1.0 in this case

	if( tr.shader_modifier != NULL ) // mapper set custom settings
	{
		Brightness = tr.shader_modifier->curstate.fuser1;
		if( Brightness == 0.0f )
			return; // mapper disabled this shader in the modifier
	}

	float OverriddenBrightness = 1.0f;
	if( gl_sunshafts_brightness->value > 0 )
	{
		OverriddenBrightness = gl_sunshafts_brightness->value;
		if( Brightness == 0.0f )
			Brightness = 1.0f;
	}

	float blur = gl_sunshafts_blur->value;
	float zFar = Q_max( 256.0f, RI->farClip );
	Vector skyColor = tr.sky_ambient * (1.0f / 128.0f) * r_lighting_modulate->value;
	Vector skyVec = tr.sky_normal.Normalize();
	Vector sunPos = RI->vieworg + skyVec * 1000.0;

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

	// request screen color
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	// request screen depth
	GL_Bind( GL_TEXTURE0, tr.screen_depth );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	// set target viewport
	pglViewport( 0, 0, TargetSize, TargetSize );

	// generate shafts
	GL_BindShader( glsl.genSunShafts );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_zFar, zFar );

	GL_Bind( GL_TEXTURE0, tr.screen_depth );
	GL_Bind( GL_TEXTURE1, tr.screen_color );
	RenderFSQ( glState.width, glState.height );

	// request target copy
	GL_Bind( GL_TEXTURE0, tr.target_rgb[0] );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, TargetSize, TargetSize );
#if 1
	pglViewport( 0, 0, TargetSize, TargetSize );
	
	// combine normal and blurred scenes
	GL_BindShader( glsl.blurShader[0] );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );
	
	// request target copy
	GL_Bind( GL_TEXTURE0, tr.target_rgb[0] );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, TargetSize, TargetSize );

	GL_BindShader( glsl.blurShader[1] );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );
	
	// request target copy
	GL_Bind( GL_TEXTURE0, tr.target_rgb[0] );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, TargetSize, TargetSize );
#endif
	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.drawSunShafts );
	ASSERT( RI->currentshader != NULL );

	pglUniform3fARB( RI->currentshader->u_LightOrigin, view.x / glState.width, view.y / glState.height, view.z );
	pglUniform3fARB( RI->currentshader->u_LightDiffuse, skyColor.x, skyColor.y, skyColor.z );
	pglUniform1fARB( RI->currentshader->u_SS_Brightness, Brightness * OverriddenBrightness );

	GL_Bind( GL_TEXTURE0, tr.screen_color );
	GL_Bind( GL_TEXTURE1, tr.target_rgb[0] );
	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_sunshafts_pass++;
}

void MotionBlur(void)
{
	if( gHUD.DrunkLevel > 0 )
		goto allow_drunk;
	
	bool AllowBlur = CVAR_TO_BOOL( r_blur );
	
	if( !AllowBlur )
		return;

	float PlayerVelocity = tr.viewparams.simvel.Length();
	int Threshold = CVAR_GET_FLOAT( "r_blur_threshold" );

	// player is in car
	if( gEngfuncs.GetLocalPlayer()->curstate.vuser1.y > 0.0f )
		PlayerVelocity = gEngfuncs.GetLocalPlayer()->curstate.vuser1.y;

	if( Threshold > PlayerVelocity * 0.01 )
		return;

	float Accum = PlayerVelocity * 0.0001 - Threshold * 0.01;
	Accum = bound( 0, Accum, 0.2 );

allow_drunk:
	if( gHUD.DrunkLevel > 0 )
		Accum = gHUD.DrunkLevel * 0.1f;

	Vector org;
	R_TransformDeviceToScreen( g_vecZero, org ); // FIXME just center of the screen...

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.MotionBlur );
	ASSERT( RI->currentshader != NULL );

	pglUniform3fARB( RI->currentshader->u_MB_Velocity, org.x / glState.width, org.y / glState.height, org.z );
	pglUniform1fARB( RI->currentshader->u_Accum, Accum );
	GL_Bind( GL_TEXTURE0, tr.screen_color );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_motionblur_pass++;
}

void GaussBlur( void )
{
	static float skyblur, screenblur, pausedblur, waterblur;

	bool AllowBlur = CVAR_TO_BOOL( r_blur );

	if( !AllowBlur )
	{
		screenblur = 0;
		waterblur = 0;
	}

	// blur when paused (menu is shown)
	if( RP_NORMALPASS() && !CVAR_GET_FLOAT( "cl_background" ) )
	{
		if( tr.time == tr.oldtime || CVAR_TO_BOOL( ui_is_active ) )
			pausedblur += 0.5 * g_fFrametime;
		else
			pausedblur -= 0.5 * g_fFrametime;

		pausedblur = bound( 0, pausedblur, 0.1 );

		if( pausedblur > 0 )
		{
			RenderGaussBlur( pausedblur, pausedblur );
			return;
		}
	}

	if( RP_NORMALPASS() && AllowBlur )
	{
		if( tr.viewparams.waterlevel > 2 )
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
		
		pmtrace_t ptr;
		Vector PlayerOrg, PlayerAngles, VecEnd, Forward;

		PlayerOrg = tr.viewparams.vieworg;
		PlayerAngles = tr.viewparams.viewangles;
		gEngfuncs.pfnAngleVectors( PlayerAngles, Forward, NULL, NULL );
		VecEnd = PlayerOrg + Forward * 50000;
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
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.blurShader[0] );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur1, blur2 );	// set blur factor

	RenderFSQ( glState.width, glState.height );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );
	
	GL_BindShader( glsl.blurShader[1] );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur1, blur2 );	// set blur factor
	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_gaussblur_pass++;
}

void HorizontalBlur( void )
{
	float YawSpeed;
	static float Accum = 0.0f;

	if( Accum == 0.0f ) // make sure it's faded out
	{
		if( !CVAR_TO_BOOL( r_blur ) )
			return;

		if( RI->params & RP_THIRDPERSON )
			return;

		// no blur if < 30 fps
		if( g_fFrametime > (1.0f / 30.0f) )
			return;

		// do not allow this blur if we look too much up or down
		if( (tr.viewparams.viewangles.x < -45) || (tr.viewparams.viewangles.x > 45) )
			return;
	}

	float CurYaw = tr.viewparams.viewangles.y;
	while( CurYaw > 180.0f )
		CurYaw -= 360.0f;
	while( CurYaw < -180.0f )
		CurYaw += 360.0f;

	float PrevYaw = PrevViewAngles.y;
	while( PrevYaw > 180.0f )
		PrevYaw -= 360.0f;
	while( PrevYaw < -180.0f )
		PrevYaw += 360.0f;

	// this hopefully disables the circle-strafing blur - copied from source sdk
//-----------------------------------------------------------------------------------------------------------
	matrix3x4 mCurrentBasisVectors;
	AngleMatrix( tr.viewparams.viewangles, mCurrentBasisVectors );
	Vector vPositionChange = PrevViewOrg - tr.viewparams.vieworg;

	Vector vCurrentSideVec = Vector( mCurrentBasisVectors[0][1], mCurrentBasisVectors[1][1], mCurrentBasisVectors[2][1] );
	float flSideDotMotion = DotProduct( vCurrentSideVec, vPositionChange );
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

	float Strength = CVAR_GET_FLOAT( "r_blur_strength" );

	YawSpeed = fabs( (PrevYaw - CurYaw) / g_fFrametime );

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
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.HorizontalBlur );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_Accum, Accum );	// set blur factor

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_horizontalblur_pass++;
}

void ScreenWater(void)
{
	float Speed = gEngfuncs.GetLocalPlayer()->curstate.vuser1.x;

	if( tr.time == tr.oldtime ) // paused
		Speed = 0.0f;

	if( Speed <= 0.0f )
		return;

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.ScreenWater );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_RealTime, tr.time );
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );
	pglUniform1fARB( RI->currentshader->u_Accum, Speed );

	GL_Bind( GL_TEXTURE0, tr.screen_color );
	GL_Bind( GL_TEXTURE1, ScreenWaterTexture );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_screenwater_pass++;
}

void Glitch( void )
{
	if( gHUD.GlitchAmount > 0.0f )
		gHUD.GlitchAmount = CL_UTIL_Approach( 0.0f, gHUD.GlitchAmount, g_fFrametime );
	
	if( gHUD.GlitchAmount <= 0.0f )
		return;

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.Glitch );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_RealTime, tr.time );
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );
	pglUniform1fARB( RI->currentshader->u_Accum, gHUD.GlitchAmount );

	GL_Bind( GL_TEXTURE0, tr.screen_color );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_glitch_pass++;
}

void Monochrome( void )
{
	float MonochromeState = gEngfuncs.GetLocalPlayer()->curstate.vuser1.z;

	static float DmgMonochrome = 0.0f;
	static float DmgMonochromeTime = 0.0f;
	float CurrentMonochrome = 0.0f;

	if( ApplyDamageMonochrome )
	{
		DmgMonochrome = 1.0f;
		DmgMonochromeTime = tr.time;
		ApplyDamageMonochrome = false;
	}

	if( DmgMonochromeTime > tr.time )
	{
		DmgMonochromeTime = 0;
		DmgMonochrome = 0;
	}

	if( MonochromeState > 0.0f ) // user plays in monochrome, so only use this
	{
		CurrentMonochrome = MonochromeState;
		DmgMonochromeTime = 0;
		DmgMonochrome = 0;
	}
	else // no permanent monochrome. so only enable it when player was damaged
	{
		if( DmgMonochrome > 0.0f && DmgMonochrome <= 1.0f )
		{
			if( tr.time > DmgMonochromeTime + 1.5 )
				DmgMonochrome -= 0.5 * g_fFrametime;
			CurrentMonochrome = DmgMonochrome;
		}
	}
	
	// apply monochrome anyway if near death, but only when not in camera, and only if has suit
	if( gHUD.HasWeapon( WEAPON_SUIT ) 
		&& !(gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERUSINGCAMERA)
		&& !(gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERDRONECONTROL )
		&& !g_iUser1
		&& !( gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH ) )
	{
		if( gHUD.m_HealthVisual.m_iHealthVisual < 25 )
		{
			CurrentMonochrome = 4.0f / ((float)gHUD.m_HealthVisual.m_iHealthVisual + 0.1f);
		}
	}

	CurrentMonochrome = bound( 0, CurrentMonochrome, 1 );

	if( CurrentMonochrome <= 0.0f )
		return;

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.Monochrome );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_Accum, CurrentMonochrome );

	GL_Bind( GL_TEXTURE0, tr.screen_color );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_monochrome_pass++;
}

void SSAO( void )
{
	if( tr.ssao_params_changed )
	{
		InitSSAO();
		tr.ssao_params_changed = false;
	}

	if( !CVAR_TO_BOOL( gl_ssao ) )
		return;

	bool Debug = false;
	if( CVAR_TO_BOOL( gl_ssao_debug ) )
		Debug = true;

	float blur = 0.075f;
	float zFar = RI->farClip;// Q_max( 256.0f, RI->farClip );

	// request screen color
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	// request screen depth
	GL_Bind( GL_TEXTURE0, tr.screen_depth );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	// set target viewport
	int width = glState.width;
	int height = glState.height;

	if( tr.lowmemory )
	{
		width = glState.width / 4;
		height = glState.height / 4;
	}
	else
	{
		switch( (int)gl_ssao->value )
		{
		case 1:
			width = glState.width / 4;
			height = glState.height / 4;
			break;
		case 2:
			width = glState.width / 2;
			height = glState.height / 2;
			break;
		}
	}

	pglViewport( 0, 0, width, height );

	// generate SSAO
	GL_BindShader( glsl.genSSAO );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_zFar, zFar );
	GL_Bind( GL_TEXTURE0, tr.screen_depth );
	GL_Bind( GL_TEXTURE1, RotateMap );
	RenderFSQ( glState.width, glState.height );

	// RequestScreenAO
	GL_Bind( GL_TEXTURE0, ScreenAO );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	// blur AO pass
	GL_BindShader( glsl.blurShader[0] );
	ASSERT( RI->currentshader != NULL );
	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );

	// RequestScreenAO
	GL_Bind( GL_TEXTURE0, ScreenAO );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	GL_BindShader( glsl.blurShader[1] );
	ASSERT( RI->currentshader != NULL );
	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );

	// RequestScreenAO
	GL_Bind( GL_TEXTURE0, ScreenAO );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	GL_BindShader( glsl.blurShader[0] );
	ASSERT( RI->currentshader != NULL );
	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );

	// RequestScreenAO
	GL_Bind( GL_TEXTURE0, ScreenAO );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	GL_BindShader( glsl.blurShader[1] );
	ASSERT( RI->currentshader != NULL );
	pglUniform2fARB( RI->currentshader->u_BlurFactor, blur, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );

	// RequestScreenAO
	GL_Bind( GL_TEXTURE0, ScreenAO );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	// do final pass
	pglViewport( 0, 0, glState.width, glState.height );
	GL_BindShader( glsl.drawSSAO );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_GenericCondition, Debug );
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	GL_Bind( GL_TEXTURE1, ScreenAO );
	RenderFSQ( glState.width, glState.height );

	// unbind shader
	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_ssao_pass++;
}

void Bloom( void )
{
	if( !CVAR_TO_BOOL( r_bloom ) )
		return;

	if( IsBuildingCubemaps() ) // no bloom in cubemaps
		return;

	GL_Bind( GL_TEXTURE0, tr.screen_fbo_texture_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.BlurMip );
	ASSERT( RI->currentshader != NULL );

	int w = glState.width;
	int h = glState.height;

	// render and blur mips
	for( int i = 0; i < 6; i++ )
	{
		w /= 2;
		h /= 2;

		pglViewport( 0, 0, w, h );

		pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)w, 1.0f / (float)h );
		pglUniform1fARB( RI->currentshader->u_MipLod, (float)i );
		pglUniform4fARB( RI->currentshader->u_TexCoordClamp, 0.0f, 0.0f, 1.0f, 1.0f );
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
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
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
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
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

	if( R_FullBright() )
		return;

	const int mipmap_count = 1 + floor( log2( Q_max( glState.width, glState.height ) ) );

	// render luminance to first mip
	GL_Bind( GL_TEXTURE0, tr.avg_luminance_texture );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

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

	for( int i = 1; i <= mipmap_count; i++ )
	{
		mip_width /= 2;
		mip_height /= 2;

		mip_width = Q_max( mip_width, 1 );
		mip_height = Q_max( mip_height, 1 );

		pglViewport( 0, 0, mip_width, mip_height );

		pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );
		pglUniform1fARB( RI->currentshader->u_MipLod, (float)(i - 1) );
		pglUniform4fARB( RI->currentshader->u_TexCoordClamp, 0.0f, 0.0f, 1.0f, 1.0f );
		pglUniform1fARB( RI->currentshader->u_BloomFirstPass, 0.0f );

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, tr.avg_luminance_fbo_mip[i] );
		RenderFSQ( glState.width, glState.height );
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 );
	GL_CleanUpTextureUnits( 0 );
	pglViewport( 0, 0, glState.width, glState.height );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	GL_BindShader( NULL );

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	int ExposureTexture = RenderExposureStorage();

	GL_BindShader( glsl.ToneMap );
	ASSERT( RI->currentshader != NULL );

	GL_Bind( GL_TEXTURE0, tr.screen_color );
	GL_Bind( GL_TEXTURE1, ExposureTexture ); // u_HDRExposure	
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );
	pglUniform1fARB( RI->currentshader->u_MipLod, (float)(mipmap_count - 1) );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_SelectTexture( glConfig.max_texture_units - 1 );
	GL_CleanUpTextureUnits( 0 );
}

void Enhance( void )
{
	// I wanted to make cvars but decided to leave it as is
	bool Saturate = true;
	bool Sharpen = true;

	if( !Saturate && !Sharpen )
		return;

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.Enhance );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)(glState.width), 1.0f / (float)(glState.height) );
	pglUniform1fARB( RI->currentshader->u_GenericCondition, (float)Saturate );
	pglUniform1fARB( RI->currentshader->u_GenericCondition2, (float)Sharpen );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );
}

void WaterDrops( void )
{
	// player is in car
	if( gEngfuncs.GetLocalPlayer()->curstate.vuser1.y > 0.0f )
		gHUD.ScreenDrips_Visible = false;

	bool Override = (gHUD.ScreenDrips_OverrideTime > tr.time);

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
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.WaterDrops );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_RealTime, tr.time );
	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, (float)(glState.width), (float)(glState.height) );
	pglUniform1fARB( RI->currentshader->u_Accum, gHUD.ScreenDrips_CurVisibility );
	pglUniform1fARB( RI->currentshader->u_LerpFactor, gHUD.ScreenDrips_DripIntensity );

	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	r_stats.sh_waterdrops_pass++;
}