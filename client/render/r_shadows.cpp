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

GLuint sh_framebuffer;
int ShadowQualityLevel = 0;
int ShadowViewport = 512;
bool FBOsupported = false;
CFrustum CurrentPassFrustum;

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

	if( tr.lowmemory )
		ShadowViewport = 256;

	FBOsupported = GL_Support( R_FRAMEBUFFER_OBJECT );

	if( !FBOsupported )
	{
		ShadowViewport = bound( 256, ShadowViewport, 512 );
		ConPrintf( "FBO not supported, shadow quality reset to (%ip)\n", ShadowViewport );
		return;
	}

	// create 3 shadow textures to decrease flashlight first lag
	for( i = 0; i < 4; i++ )
	{
		if( !tr.shadowTextures[i] )
		{
			char txName[16];
			Q_snprintf( txName, sizeof( txName ), "*shadow%i", i );
			tr.shadowTextures[i] = CREATE_TEXTURE( txName, ShadowViewport, ShadowViewport, NULL, TF_SHADOW );
		}
	}

	// create 1 shadow cubemap too
	if( !tr.shadowCubemaps[0] )
	{
		char txName[16];
		Q_snprintf( txName, sizeof( txName ), "*shadowCM0" );
		tr.shadowCubemaps[0] = CREATE_TEXTURE( txName, ShadowViewport, ShadowViewport, NULL, TF_SHADOW_CUBEMAP );
		if( !tr.shadowCubemaps[0] )
			tr.omni_shadows_notsupport = true; // texture not supported
	}

	ConPrintf( "Shadow textures reset to quality %i (%ip)\n", ShadowQualityLevel, ShadowViewport );
}

//==============================================================================
// R_AllocateShadowFramebuffer: setup FBO for shadow textures
//==============================================================================
int R_AllocateShadowFramebuffer( plight_t *pl, int side = 0 )
{
	int i;
	int texture = 0;

	if( pl->pointlight )
	{
		if( !sh_framebuffer )
			pglGenFramebuffers( 1, &sh_framebuffer );
		
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

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, sh_framebuffer );
		pglDrawBuffer( GL_NONE );
		pglReadBuffer( GL_NONE );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side, RENDER_GET_PARM( PARM_TEX_TEXNUM, texture ), 0 );
	}
	else
	{
		i = tr.num_shadows_used;

		if( !sh_framebuffer )
			pglGenFramebuffers( 1, &sh_framebuffer );

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

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, sh_framebuffer );
		pglDrawBuffer( GL_NONE );
		pglReadBuffer( GL_NONE );
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
	{
		RI->frustum.InitProjection( matrix4x4( RI->vieworg, RI->viewangles ), 0.1f, pl->radius, 90.0f, 90.0f );
		
		// cull each side of a pointlight separately to prevent unnecessary rendering
		pl->shadow_side_culled[split] = false;
		if( !RI->frustum.Intersect( &CurrentPassFrustum ))
			pl->shadow_side_culled[split] = true;
	}
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

	GL_Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
//	pglEnable( GL_POLYGON_OFFSET_FILL );
	GL_Texture2D( GL_FALSE );
	GL_DepthMask( GL_TRUE );
//	pglPolygonOffset( 1.0f, 2.0f );
	GL_DepthTest( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_Blend( GL_FALSE );
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
	GL_Texture2D( GL_TRUE );
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

	if( FBitSet( pl->flags, CF_ONLYFORCEDSHADOWS ) )
		return;

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

		if( pl->effect == 2 )
		{
			// no self-shadow from your own muzzleflash
			if( RI->currententity->index == pl->entindex )
				continue;
		}

		// this model has indicated to not make shadows
		if( (RI->currententity->curstate.renderfx == kRenderFxNoShadows) || (RI->currententity->curstate.renderfx == kRenderFxFullbrightNoShadows) )
			continue;

		if( (pl->flags & CF_ONLYFORCEDSHADOWS) && (RI->currententity->curstate.renderfx != kRenderFxForceShadow) )
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
				if( RI->currententity->index != pl->entindex )
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
		ALERT( at_error, "R_RenderShadowScene: NULL worldmodel\n" );
		return;
	}

	RI->viewport[2] = RI->viewport[3] = ShadowViewport;

	if( FBOsupported )
		pl->shadowTexture = R_AllocateShadowFramebuffer( pl );

	r_stats.num_passes++;
	R_ShadowPassSetupFrame( pl );
	R_ShadowPassSetupGL( pl );
	pglEnable( GL_SCISSOR_TEST );
	pglScissor( RI->viewport[0], RI->viewport[1], RI->viewport[2], RI->viewport[3] );
	pglClear( GL_DEPTH_BUFFER_BIT );
	pglDisable( GL_SCISSOR_TEST );

	R_MarkLeaves();
	R_ShadowPassDrawWorld( pl );
	R_ShadowPassDrawSolidEntities( pl );

	R_ShadowPassEndGL();

	if( FBOsupported )
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, glState.frameBuffer == -1 ? 0 : glState.frameBuffer );
	else
		pl->shadowTexture = R_AllocateShadowTexture();
}

void R_RenderShadowCubeSide( plight_t *pl, int side )
{
	if( tr.omni_shadows_notsupport )
		return;
	
	RI->params = RP_SHADOWPASS;

	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;

	if( !worldmodel )
	{
		ALERT( at_error, "R_RenderShadowCubeSide: NULL worldmodel\n" );
		return;
	}

	RI->viewport[2] = RI->viewport[3] = ShadowViewport;

	if( FBOsupported )
		pl->shadowTexture = R_AllocateShadowFramebuffer( pl, side );

	R_ShadowPassSetupFrame( pl, side );

	// only render this side if it's actually in view, otherwise leave the image blank
	if( !pl->shadow_side_culled[side] )
	{
		R_ShadowPassSetupGL( pl, side );
		pglClear( GL_DEPTH_BUFFER_BIT );

		R_MarkLeaves();
		R_ShadowPassDrawWorld( pl );
		R_ShadowPassDrawSolidEntities( pl );

		//	R_DrawParticles( false );

		R_ShadowPassEndGL();
	}

	if( FBOsupported )
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, glState.frameBuffer == -1 ? 0 : glState.frameBuffer );
	else
		pl->shadowTexture = R_AllocateShadowCubemap( side );
}

void R_RenderShadowmaps(void)
{	
	if (R_FullBright() || !CVAR_TO_BOOL( gl_shadows ) || tr.fGamePaused || tr.shadows_notsupport)
		return;

	if( IsBuildingCubemaps() )
		return;
	
	// check for dynamic lights (exclude those with "no shadows" set)
	if( !R_CountPlights( true ) ) 
		return;

	CurrentPassFrustum = RI->frustum;

	R_PushRefState();

	if( ShadowQualityLevel != (int)r_shadowquality->value )
	{
		ShadowQualityLevel = (int)r_shadowquality->value;
		R_ResetShadowTextures();
	}

	for( int i = 0; i < MAX_PLIGHTS; i++ )
	{
		plight_t *pl = &cl_plights[i];

		if( FBitSet( pl->flags, CF_NOSHADOWS ) )
			continue;

		if( pl->die < tr.time || !pl->radius || pl->culled )
			continue;

		if( !UTIL_IsLocal( pl->entindex ) ) // do not perform culling for this player's flashlight - it's always visible
		{
			if( R_CullBox( pl->absmin, pl->absmax ) )
				continue;

			if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ) )
				continue;
		}

		if( pl->pointlight && tr.omni_shadows_notsupport )
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