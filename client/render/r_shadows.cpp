/*
r_shadows.cpp - render shadowmaps for directional lights
Copyright (C) 2012 Uncle Mike

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
#include "r_cvars.h"
#include "mathlib.h"
#include "event_api.h"
#include "r_studio.h"
#include "r_sprite.h"

static Vector light_sides[] =
{
Vector( 0.0f,   0.0f,  90.0f ),		// GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 
Vector( 0.0f, 180.0f, -90.0f ),		// GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB 
Vector( 0.0f,  90.0f,   0.0f ),		// GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB
Vector( 0.0f, 270.0f, 180.0f ),		// GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB 
Vector( -90.0f, 180.0f, -90.0f ),	// GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB
Vector( 90.0f,   0.0f,  90.0f ),	// GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
};

static GLuint framebuffer[MAX_SHADOWS]; // projector
static GLuint framebufferCM[MAX_SHADOWS]; // pointlight
static int ShadowQualityLevel = 0;
static int ShadowViewport = 512;
static bool FBOsupported = false;

/*
=============================================================

	SHADOW RENDERING

=============================================================
*/

//==============================================================================
// R_ResetShadowTextures: rebuild shadow textures if the size was changed
//==============================================================================
void R_ResetShadowTextures( void )
{
	int	i;

	for( i = 0; i < MAX_SHADOWS; i++ )
	{
		if( !tr.shadowTextures[i] ) break;
		FREE_TEXTURE( tr.shadowTextures[i] );
	}

	for( i = 0; i < MAX_SHADOWS; i++ )
	{
		if( !tr.shadowCubemaps[i] ) break;
		FREE_TEXTURE( tr.shadowCubemaps[i] );
	}

	memset( tr.shadowTextures, 0, sizeof( tr.shadowTextures ) );
	memset( tr.shadowCubemaps, 0, sizeof( tr.shadowCubemaps ) );

	ShadowViewport = 512;

	switch( ShadowQualityLevel )
	{
	case 0: ShadowViewport = 256; break;
	case 1: ShadowViewport = 512; break;
	case 2: ShadowViewport = 1024; break;
	case 3: ShadowViewport = 2048; break;
	case 4: ShadowViewport = 4096; break;
	}

	if( !FBOsupported )
	{
		ShadowViewport = bound( 256, ShadowViewport, 512 );
		ConPrintf( "FBO not supported, shadow quality reset to (%ip)\n", ShadowViewport );
		return;
	}

	ConPrintf( "Shadow textures reset to quality %i (%ip)\n", ShadowQualityLevel, ShadowViewport );
}

//==============================================================================
// R_AllocateShadowFramebuffer: setup FBO for shadow textures
//==============================================================================
int R_AllocateShadowFramebuffer( plight_t *pl, int side = 0 )
{
	int i, texture;

	if( pl->pointlight )
	{
		if( !framebufferCM[0] )
			pglGenFramebuffers( MAX_SHADOWS, framebufferCM );
		
		if( side != 0 )
		{
			i = (tr.num_CM_shadows_used - 1);

			if( i >= MAX_SHADOWS )
			{
				ConPrintf( "ERROR: R_AllocateShadowFramebuffer: shadow cubemaps limit exceeded!\n" );
				return 0; // disable
			}

			texture = tr.shadowCubemaps[i];

			if( !tr.shadowCubemaps[i] )
			{
				ConPrintf( "ERROR: R_AllocateShadowFramebuffer: cubemap not initialized!\n" );
				return 0; // disable
			}
		}
		else
		{
			i = tr.num_CM_shadows_used;

			if( i >= MAX_SHADOWS )
			{
				ConPrintf( "ERROR: R_AllocateShadowFramebuffer: shadow cubemaps limit exceeded!\n" );
				return 0; // disable
			}

			texture = tr.shadowCubemaps[i];
			tr.num_CM_shadows_used++;

			if( !tr.shadowCubemaps[i] )
			{
				char txName[16];

				Q_snprintf( txName, sizeof( txName ), "*shadowCM%i", i );

				tr.shadowCubemaps[i] = CREATE_TEXTURE( txName, ShadowViewport, ShadowViewport, NULL, TF_SHADOW_CUBEMAP );
				texture = tr.shadowCubemaps[i];
			}
		}

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, framebufferCM[i] );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side, RENDER_GET_PARM( PARM_TEX_TEXNUM, texture ), 0 );
	}
	else
	{
		i = tr.num_shadows_used;

		if( !framebuffer[0] )
			pglGenFramebuffers( MAX_SHADOWS, framebuffer );

		if( i >= MAX_SHADOWS )
		{
			ALERT( at_error, "R_AllocateShadowFramebuffer: shadow textures limit exceeded!\n" );
			return 0; // disable
		}

		texture = tr.shadowTextures[i];
		tr.num_shadows_used++;

		if( !tr.shadowTextures[i] )
		{
			char txName[16];

			Q_snprintf( txName, sizeof( txName ), "*shadow%i", i );

			tr.shadowTextures[i] = CREATE_TEXTURE( txName, ShadowViewport, ShadowViewport, NULL, TF_SHADOW );
			texture = tr.shadowTextures[i];
		}

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, framebuffer[i] );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, texture ), 0 );
	}

	ValidateFBO();

	return texture;
}

//==============================================================================
// R_AllocateShadowTexture: shadow texture for projected light (spotlight, flashlight)
//==============================================================================
int R_AllocateShadowTexture( void )
{
	int i = tr.num_shadows_used;

	if( i >= MAX_SHADOWS )
	{
		ConPrintf( "ERROR: R_AllocateShadowTexture: shadow textures limit exceeded!\n" );
		return 0; // disable
	}

	int texture = tr.shadowTextures[i];
	tr.num_shadows_used++;

	if( !tr.shadowTextures[i] )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*shadow%i", i );

		tr.shadowTextures[i] = CREATE_TEXTURE( txName, ShadowViewport, ShadowViewport, NULL, TF_SHADOW );
		texture = tr.shadowTextures[i];
	}

	GL_Bind( GL_TEXTURE0, texture );
	pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, RI->viewport[0], RI->viewport[1], ShadowViewport, ShadowViewport, 0 );

	return texture;
}

//==============================================================================
// R_AllocateShadowCubemap: shadow texture for point light
//==============================================================================
int R_AllocateShadowCubemap( int side )
{
	int texture = 0;
	int i;

	if( side != 0 )
	{
		i = (tr.num_CM_shadows_used - 1);

		if( i >= MAX_SHADOWS )
		{
			ConPrintf( "ERROR: R_AllocateShadowCubemap: shadow cubemaps limit exceeded!\n" );
			return 0; // disable
		}

		texture = tr.shadowCubemaps[i];

		if( !tr.shadowCubemaps[i] )
		{
			ConPrintf( "ERROR: R_AllocateShadowCubemap: cubemap not initialized!\n" );
			return 0; // disable
		}
	}
	else
	{
		i = tr.num_CM_shadows_used;

		if( i >= MAX_SHADOWS )
		{
			ConPrintf( "ERROR: R_AllocateShadowCubemap: shadow cubemaps limit exceeded!\n" );
			return 0; // disable
		}

		texture = tr.shadowCubemaps[i];
		tr.num_CM_shadows_used++;

		if( !tr.shadowCubemaps[i] )
		{
			char txName[16];

			Q_snprintf( txName, sizeof( txName ), "*shadowCM%i", i );

			tr.shadowCubemaps[i] = CREATE_TEXTURE( txName, ShadowViewport, ShadowViewport, NULL, TF_SHADOW_CUBEMAP );
			texture = tr.shadowCubemaps[i];
		}
	}

	GL_Bind( GL_TEXTURE0, texture );
	pglCopyTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side, 0, GL_DEPTH_COMPONENT, RI->viewport[0], RI->viewport[1], RI->viewport[2], RI->viewport[3], 0 );

	return texture;
}

/*
===============
R_ShadowPassSetupFrame
===============
*/
static void R_ShadowPassSetupFrame( plight_t *pl, int split = 0 )
{
	if( pl->pointlight )
	{
		RI->viewangles = light_sides[split]; // this is a cube side
	}
	else
	{
		// build the transformation matrix for the given view angles
		RI->viewangles[0] = anglemod( pl->angles[0] );
		RI->viewangles[1] = anglemod( pl->angles[1] );
		RI->viewangles[2] = anglemod( pl->angles[2] );
		AngleVectors( RI->viewangles, RI->vforward, RI->vright, RI->vup );
	}

	RI->vieworg = pl->origin;

	// setup the screen FOV
	RI->fov_x = pl->fov;
	RI->fov_y = pl->fov;

	if( !pl->pointlight )
	{
		if( pl->flags & CF_ASPECT3X4 )
			RI->fov_y = pl->fov * (5.0f / 4.0f);
		else if( pl->flags & CF_ASPECT4X3 )
			RI->fov_y = pl->fov * (4.0f / 5.0f);
		else RI->fov_y = pl->fov;
	}

	// setup frustum
	if( pl->pointlight )
		RI->frustum.InitProjection( matrix4x4( RI->vieworg, RI->viewangles ), 0.1f, pl->radius, 90.0f, 90.0f );
	else
		RI->frustum = pl->frustum;

	if(!( RI->params & RP_OLDVIEWLEAF ))
		R_FindViewLeaf();

	RI->currentlight = pl;
}

/*
=============
R_ShadowPassSetupGL
=============
*/
static void R_ShadowPassSetupGL( const plight_t *pl, int split = 0 )
{
	if( pl->pointlight )
	{
		RI->worldviewMatrix.CreateModelview();
		RI->worldviewMatrix.ConcatRotate( -light_sides[split].z, 1, 0, 0 );
		RI->worldviewMatrix.ConcatRotate( -light_sides[split].x, 0, 1, 0 );
		RI->worldviewMatrix.ConcatRotate( -light_sides[split].y, 0, 0, 1 );
		RI->worldviewMatrix.ConcatTranslate( -pl->origin.x, -pl->origin.y, -pl->origin.z );
		RI->projectionMatrix = pl->projectionMatrix;
	}
	else
	{
		// matrices already computed
		RI->worldviewMatrix = pl->modelviewMatrix;
		RI->projectionMatrix = pl->projectionMatrix;
		RI->worldviewProjectionMatrix = RI->projectionMatrix.Concat( RI->worldviewMatrix );
	}

	pglViewport( RI->viewport[0], RI->viewport[1], RI->viewport[2], RI->viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI->projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->worldviewMatrix );

	GL_Cull( GL_FRONT );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	pglEnable( GL_POLYGON_OFFSET_FILL );
	pglDisable( GL_TEXTURE_2D );
	pglDepthMask( GL_TRUE );
	pglPolygonOffset( 1.0f, 2.0f );
	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_BLEND );
}

/*
=============
R_ShadowPassEndGL
=============
*/
static void R_ShadowPassEndGL( void )
{
	pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglEnable( GL_TEXTURE_2D );
	pglPolygonOffset( -1, -2 );
	GL_BindShader( GL_FALSE );
}

/*
=============
R_ShadowPassDrawWorld
=============
*/
static void R_ShadowPassDrawWorld( plight_t *pl )
{
	if( FBitSet( pl->flags, CF_NOWORLD_PROJECTION ))
		return;	// no worldlight, no worldshadows

	R_DrawWorldShadowPass();
}

void R_ShadowPassDrawSolidEntities( plight_t *pl )
{
	int	i;

	glState.drawTrans = false;

	HUD_DrawNormalTriangles(); // cables

	// draw solid entities only
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		RI->currententity = tr.solid_entities[i];
		RI->currentmodel = RI->currententity->model;

		if( RI->currentmodel->type == mod_sprite )
			continue;

		// this model has indicated to not make shadows
		if( (RI->currententity->curstate.renderfx == kRenderFxNoShadows) || (RI->currententity->curstate.renderfx == kRenderFxFullbrightNoShadows) )
			continue;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI->currententity );

		switch( RI->currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModelShadow( RI->currententity );
			break;
		case mod_studio:
		{
			if( (pl->flags & CF_ONLYBRUSHSHADOWS) && (RI->currententity->curstate.renderfx != kRenderFxForceShadow) )
				break;
			
			// diffusion - don't draw shadows for your own player/weapon model from your own flashlight
			if( pl->effect == 1 )
			{
				if( RI->currententity->index != pl->entitynum )
					R_DrawStudioModel( RI->currententity );
			}
			else
				R_DrawStudioModel( RI->currententity );
		}
		break;
		default:
			break;
		}
	}

	// diffusion - we draw only transalpha brushes here. but I changed them to opaque mode, so they were already drawn.
	// draw trans entities only
	/*
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		RI->currententity = tr.trans_entities[i];
		RI->currentmodel = RI->currententity->model;

		if( RI->currentmodel->type != mod_brush )
			continue;

		if( RI->currententity->curstate.rendermode != kRenderTransAlpha )
			continue;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI->currententity );

		R_DrawBrushModelShadow( RI->currententity );
	}*/

	// solid particles
	R_DrawParticles( false );
}

/*
================
R_RenderShadowScene

RI->refdef must be set before the first call
fast version of R_RenderScene: no colors, no texcords etc
================
*/
void R_RenderShadowScene( plight_t *pl )
{
	RI->params = RP_SHADOWPASS;
	
	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;

	if( !worldmodel )
	{
		ALERT( at_error, "R_RenderShadowView: NULL worldmodel\n" );
		return;
	}

	RI->viewport[2] = RI->viewport[3] = ShadowViewport;

	if( FBOsupported )
		pl->shadowTexture[0] = R_AllocateShadowFramebuffer( pl );

	r_stats.num_passes++;
	R_ShadowPassSetupFrame( pl );
	R_ShadowPassSetupGL( pl );
	pglClear( GL_DEPTH_BUFFER_BIT );

	R_MarkLeaves();
	R_ShadowPassDrawWorld( pl );
	R_ShadowPassDrawSolidEntities( pl );

	R_ShadowPassEndGL();

	if( FBOsupported )
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, glState.frameBuffer == -1 ? 0 : glState.frameBuffer );
	else
		pl->shadowTexture[0] = R_AllocateShadowTexture();
}

void R_RenderShadowCubeSide( plight_t *pl, int side )
{
	RI->params = RP_SHADOWPASS;

	RI->viewport[2] = RI->viewport[3] = ShadowViewport;

	if( FBOsupported )
		pl->shadowTexture[0] = R_AllocateShadowFramebuffer( pl, side );

	R_ShadowPassSetupFrame( pl, side );
	R_ShadowPassSetupGL( pl, side );
	pglClear( GL_DEPTH_BUFFER_BIT );

	R_MarkLeaves();
	R_ShadowPassDrawWorld( pl );
	R_ShadowPassDrawSolidEntities( pl );
	//	R_DrawParticles( false );
	R_ShadowPassEndGL();

	if( FBOsupported )
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, glState.frameBuffer == -1 ? 0 : glState.frameBuffer );
	else
		pl->shadowTexture[0] = R_AllocateShadowCubemap( side );
}

void R_RenderShadowmaps(void)
{
	if (R_FullBright() || !CVAR_TO_BOOL(r_shadows) || tr.fGamePaused || tr.shadows_notsupport)
		return;

	if( FBitSet( RI->params, RP_ENVVIEW ) )
		return;
	
	// check for dynamic lights
	if (!R_CountPlights(true)) return;

	R_PushRefState();

	FBOsupported = GL_Support( R_FRAMEBUFFER_OBJECT );

	if( ShadowQualityLevel != (int)r_shadowquality->value )
	{
		ShadowQualityLevel = (int)r_shadowquality->value;
		R_ResetShadowTextures();
	}

	for (int i = 0; i < MAX_PLIGHTS; i++)
	{
		plight_t* pl = &cl_plights[i];

		if( FBitSet( pl->flags, CF_NOSHADOWS ) )
			continue;

		if (pl->die < tr.time || !pl->radius || pl->culled)
			continue;

		if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ) )
			continue;

		if( R_CullBox( pl->absmin, pl->absmax ) )
			continue;

		if( pl->pointlight )
		{
			for( int j = 0; j < 6; j++ )
			{
				R_RenderShadowCubeSide( pl, j );
				R_ResetRefState();
			}
		}
		else
		{
			R_RenderShadowScene( pl );
			R_ResetRefState();
		}

		r_stats.c_shadow_passes++;
	}

	R_PopRefState();
}