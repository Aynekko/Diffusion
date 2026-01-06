/*
r_debug.cpp - visual debug routines
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
#include "mathlib.h"
#include "r_world.h"

// some simple helpers to draw a cube in the special way the ambient visualization wants
static float *CubeSide( const vec3_t pos, float size, int vert )
{
	static vec3_t	side;

	side = pos;
	side[0] += (vert & 1) ? -size : size;
	side[1] += (vert & 2) ? -size : size;
	side[2] += (vert & 4) ? -size : size;

	return side;
}

static void CubeFace( const vec3_t org, int v0, int v1, int v2, int v3, float size, const byte *color )
{
	unsigned int scale = tr.lightstylevalue[0];
	unsigned int unclamped[3];
	int col[3];

	unclamped[0] = TEXTURE_TO_TEXGAMMA( color[0] ) * scale;
	unclamped[1] = TEXTURE_TO_TEXGAMMA( color[1] ) * scale;
	unclamped[2] = TEXTURE_TO_TEXGAMMA( color[2] ) * scale;

	col[0] = Q_min((unclamped[0] >> 7), 255 );
	col[1] = Q_min((unclamped[1] >> 7), 255 );
	col[2] = Q_min((unclamped[2] >> 7), 255 );

//	pglColor3ub( col[0], col[1], col[2] );
	pglColor3ub( color[0], color[1], color[2] );
	pglVertex3fv( CubeSide( org, size, v0 ));
	pglVertex3fv( CubeSide( org, size, v1 ));
	pglVertex3fv( CubeSide( org, size, v2 ));
	pglVertex3fv( CubeSide( org, size, v3 ));
}
		
void R_RenderLightProbe( mlightprobe_t *probe )
{
	float	rad = 4.0f;

	pglBegin( GL_QUADS );

	CubeFace( probe->origin, 4, 6, 2, 0, rad, probe->cube.color[0] );
	CubeFace( probe->origin, 7, 5, 1, 3, rad, probe->cube.color[1] );
	CubeFace( probe->origin, 0, 1, 5, 4, rad, probe->cube.color[2] );
	CubeFace( probe->origin, 3, 2, 6, 7, rad, probe->cube.color[3] );
	CubeFace( probe->origin, 2, 3, 1, 0, rad, probe->cube.color[4] );
	CubeFace( probe->origin, 4, 5, 7, 6, rad, probe->cube.color[5] );

	pglEnd ();
}

/*
=============
DrawLightProbes
=============
*/
void DrawLightProbes( void )
{
	int		i, j;
	mlightprobe_t	*probe;
	mworldleaf_t	*leaf;

	GL_Blend( GL_FALSE );

	GL_Texture2D( GL_FALSE );

	// draw the all visible probes
	for( i = 1; i < world->numleafs; i++ )
	{
		leaf = &world->leafs[i];

		if( !CHECKVISBIT( RI->visbytes, leaf->cluster ))
			continue;	// not visible from player

		for( j = 0; j < leaf->num_lightprobes; j++ )
		{
			probe = leaf->ambient_light + j;
			R_RenderLightProbe( probe );
		}
	}

	pglColor3f( 1.0f, 1.0f, 1.0f );
	GL_Texture2D( GL_TRUE );
}

/*
================
DrawNormals
================
*/
void R_DrawNormals( void )
{
	if( !r_show_normals->value )
		return;

	Vector temp;
	const float vecLen = 4.0f;

	GL_Texture2D( GL_FALSE );
	GL_Blend( GL_FALSE );
	pglBegin( GL_LINES );

	for( int i = 0; i < tr.num_draw_surfaces; i++ )
	{
		msurface_t *surf = tr.draw_surfaces[i].surface;
		mextrasurf_t *es = surf->info;

		if( FBitSet( surf->flags, SURF_WATER | SURF_DRAWSKY ) )
			continue;

		bvert_t *mv = &world->vertexes[es->firstvertex];

		for( int j = 0; j < es->numverts; j++, mv++ )
		{
			pglColor3f( 0.0f, 0.0f, 1.0f );
			pglVertex3fv( mv->vertex );
			temp = mv->vertex + mv->normal * vecLen;
			pglVertex3fv( temp );
		}
	}

	pglEnd();
	GL_Texture2D( GL_TRUE );
}

/*
================
DrawTangentSpaces

Draws vertex tangent spaces for debugging
================
*/
void R_DrawTangentSpaces( void )
{
	if( !r_show_tbn->value )
		return;

	float temp[3];
	const float vecLen = 4.0f;

	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	GL_Blend( GL_FALSE );
	pglBegin( GL_LINES );

	for( int i = 0; i < tr.num_draw_surfaces; i++ )
	{
		msurface_t *surf = tr.draw_surfaces[i].surface;
		mextrasurf_t *es = surf->info;

		if( FBitSet( surf->flags, SURF_WATER | SURF_DRAWSKY ) )
			continue;

		bvert_t *mv = &world->vertexes[es->firstvertex];

		for( int j = 0; j < es->numverts; j++, mv++ )
		{
			pglColor3f( 1.0f, 0.0f, 0.0f );
			pglVertex3fv( mv->vertex );
			VectorMA( mv->vertex, vecLen, Vector( mv->tangent ), temp );
			pglVertex3fv( temp );

			pglColor3f( 0.0f, 1.0f, 0.0f );
			pglVertex3fv( mv->vertex );
			VectorMA( mv->vertex, vecLen, Vector( mv->binormal ), temp );
			pglVertex3fv( temp );

			pglColor3f( 0.0f, 0.0f, 1.0f );
			pglVertex3fv( mv->vertex );
			VectorMA( mv->vertex, vecLen, Vector( mv->normal ), temp );
			pglVertex3fv( temp );
		}
	}

	pglEnd();
}

void DrawWireFrame( void )
{
	if( !CVAR_TO_BOOL( r_wireframe ))
		return;

	GL_Blend( GL_TRUE );
	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	GL_Color4f( 1.0f, 1.0f, 1.0f, 0.99f );

	GL_DepthTest( GL_FALSE );
	pglEnable( GL_LINE_SMOOTH );
	GL_Texture2D( GL_FALSE );
	pglEnable( GL_POLYGON_SMOOTH );
	pglHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
	pglHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );

	int	i;
	for( i = 0; i < tr.num_draw_surfaces; i++ )
	{
		msurface_t *surf = tr.draw_surfaces[i].surface;
		mextrasurf_t *es = surf->info;
		bvert_t *v;

		if( FBitSet( surf->flags, SURF_WATER ))
		{
			for( int j = 0; j < es->numindexes; j += 3 )
			{
				pglBegin( GL_TRIANGLES );
				v = &world->vertexes[es->firstvertex + es->indexes[j+0]];
				pglVertex3fv( v->vertex + v->normal * 0.1f );
				v = &world->vertexes[es->firstvertex + es->indexes[j+1]];
				pglVertex3fv( v->vertex + v->normal * 0.1f );
				v = &world->vertexes[es->firstvertex + es->indexes[j+2]];
				pglVertex3fv( v->vertex + v->normal * 0.1f );
				pglEnd();
			}
		}
		else
		{
			pglBegin( GL_POLYGON );
			for( int j = 0; j < es->numverts; j++ )
			{
				v = &world->vertexes[es->firstvertex + j];
				pglVertex3fv( v->vertex + v->normal * 0.1f );
			}
			pglEnd();
		}
	}

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglDisable( GL_POLYGON_SMOOTH );
	pglDisable( GL_LINE_SMOOTH );
	GL_Texture2D( GL_TRUE );
	GL_DepthTest( GL_TRUE );
	GL_Blend( GL_FALSE );
}

// debug thing
void R_DrawScissorRectangle( float x, float y, float w, float h )
{
	GL_Texture2D( GL_FALSE );
	GL_Setup2D();

	pglColor3f( 1, 0.5, 0 );

	pglBegin( GL_LINES );
		pglVertex2f( x, y );
		pglVertex2f( x + w, y );
		pglVertex2f( x, y );
		pglVertex2f( x, y + h );
		pglVertex2f( x + w, y );
		pglVertex2f( x + w, y + h );
		pglVertex2f( x, y + h );
		pglVertex2f( x + w, y + h );
	pglEnd();

	GL_Texture2D( GL_TRUE );
	GL_Setup3D();
}

void DBG_DrawBBox( const Vector &mins, const Vector &maxs, Vector color )
{	
	Vector bbox[8];
	int i;

	for( i = 0; i < 8; i++ )
	{
  		bbox[i][0] = ( i & 1 ) ? mins[0] : maxs[0];
  		bbox[i][1] = ( i & 2 ) ? mins[1] : maxs[1];
  		bbox[i][2] = ( i & 4 ) ? mins[2] : maxs[2];
	}

	GL_Color4f( color.x / 255.0f, color.y / 255.0f, color.z / 255.0f, 1.0f );
	GL_Texture2D( GL_FALSE );
	pglBegin( GL_LINES );

	for( i = 0; i < 2; i += 1 )
	{
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i*2+0] );
		pglVertex3fv( bbox[i*2+1] );
		pglVertex3fv( bbox[i*2+4] );
		pglVertex3fv( bbox[i*2+5] );
	}

	pglEnd();

	GL_Texture2D( GL_TRUE );
}

void R_DrawLightScissors( void )
{
	plight_t *pl = cl_plights;

	for( int i = 0; i < MAX_PLIGHTS; i++, pl++ )
	{
		if( pl->die < tr.time || !pl->radius || pl->culled )
			continue;

		RI->currentlight = pl;

		if( R_CullBox( pl->absmin, pl->absmax ))
			continue;

		if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ) )
			continue;

		if( !UTIL_IsLocal( pl->key ) || FBitSet( RI->params, RP_THIRDPERSON ))
			pl->frustum.DrawFrustumDebug();

		R_DrawScissorRectangle( pl->x, pl->y, pl->w, pl->h );
	}

	RI->currentlight = NULL;
}

void R_DrawRenderPasses( int passnum )
{
	passnum = bound( 0, passnum, MAX_SUBVIEW_TEXTURES - 1 );

	if( tr.subviewTextures[passnum].texframe != tr.realframecount )
		return; // not used

	GL_Setup2D();
	GL_Bind( GL_TEXTURE0, tr.subviewTextures[passnum].texturenum );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex2f( 0.0f, 0.0f );
		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex2f( glState.width, 0.0f );
		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex2f( glState.width, glState.height );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex2f( 0.0f, glState.height );
	pglEnd();
	GL_Setup3D();
}

bool ValidateFBO( void )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ) )
		return false;

	// check FBO status
	GLenum status = pglCheckFramebufferStatus( GL_FRAMEBUFFER_EXT );

	switch( status )
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
		return true;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
		ALERT( at_error, "ValidateFBO: attachment is NOT complete\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
		ALERT( at_error, "ValidateFBO: no image is attached to FBO\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
		ALERT( at_error, "ValidateFBO: attached images have different dimensions\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
		ALERT( at_error, "ValidateFBO: color attached images have different internal formats\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
		ALERT( at_error, "ValidateFBO: draw buffer incomplete\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
		ALERT( at_error, "ValidateFBO: read buffer incomplete\n" );
		return false;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		ALERT( at_error, "ValidateFBO: unsupported by current FBO implementation\n" );
		return false;
	default:
		ALERT( at_error, "ValidateFBO: unknown error\n" );
		return false;
	}
}