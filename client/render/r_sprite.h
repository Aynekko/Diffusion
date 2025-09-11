/*
r_sprite.h - sprite model rendering
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef R_SPRITE_H
#define R_SPRITE_H

#include "sprite.h"

// for sprites with a single frame only! - batch rendering (glow etc.)
#define MAX_SPRITE_VERTS 65536
#define MAX_SPRITE_BATCHES 64

struct sprite_batch_t
{
	int texture;
	Vector SpriteVertexesArray[MAX_SPRITE_VERTS];
	byte SpriteColorArray[MAX_SPRITE_VERTS][4];
	Vector2D SpriteTexCoordsArray[MAX_SPRITE_VERTS];
	unsigned short numverts;
};

/*
====================
CSpriteModelRenderer

====================
*/
class CSpriteModelRenderer
{
public:
	// Construction/Destruction
	CSpriteModelRenderer( void );
	virtual ~CSpriteModelRenderer( void );

	// Initialization
	void Init( void );
	void VidInit( void );
	void ResetRenderCache( void );
	sprite_batch_t SpriteBatchGlow[MAX_SPRITE_BATCHES];
	sprite_batch_t SpriteBatchAdditive[MAX_SPRITE_BATCHES];
	void RenderSpritesBatch( void );

	virtual void SpriteDrawModel( void );

	virtual void SpriteDrawModelShadowPass( void );
private:
	// Get Sprite description for frame
	virtual mspriteframe_t *GetSpriteFrame( int frame, float yaw );

	// Get interpolated Sprite description for frame
	virtual float GetSpriteFrameInterpolant( mspriteframe_t **oldframe, mspriteframe_t **curframe );

	virtual void SpriteComputeOrigin( cl_entity_t *e );

	virtual void SpriteComputeBBox( cl_entity_t *e, Vector bbox[8] );

	virtual bool CullSpriteModel( void );

	virtual float GlowSightDistance( void );

	virtual float SpriteGlowBlend( int rendermode, int renderfx, float &scale );

	virtual int SpriteOccluded( float &pscale );

	virtual void DrawSpriteQuad( mspriteframe_t *frame, const Vector &org, const Vector &right, const Vector &up, float scale );

	void DrawLighting( mspriteframe_t *frame, const Vector &org, const Vector &right, const Vector &up, const Vector &light, float scale, float a );

	virtual int SpriteHasLightmap( int texFormat );

	virtual int SpriteAllowLerping( cl_entity_t *e, msprite_t *psprite );

	inline void *Mod_Extradata( model_t *mod )
	{
		if( mod && mod->type == mod_sprite )
			return mod->cache.data;
		return NULL;
	}

	Vector			sprite_origin;
	Vector			sprite_absmin, sprite_absmax;
	// Do interpolation?
	int			m_fDoInterp;

	// Cvars that sprite model code needs to reference
	cvar_t			*m_pCvarLerping;	// Use lerping for animation?
	cvar_t			*m_pCvarLighting;	// lighting mode
	cvar_t			*m_pCvarTraceGlow;

	// The entity which we are currently rendering.
	cl_entity_t		*m_pCurrentEntity;		

	// The model for the entity being rendered
	model_t			*m_pRenderModel;

	// Current model rendermode
	int			m_iRenderMode;

	// Pointer to header block for sprite model data
	msprite_t			*m_pSpriteHeader;

	int cached_sprite_texture; // rendering cache

	// engine stuff (backend)
public:
	// Draw generic spritemodel
	void	DrawSpriteModelInternal( cl_entity_t *e );
	mspriteframe_t *GetSpriteFrame( const model_t *m_pSpriteModel, int frame );
};

extern CSpriteModelRenderer g_SpriteRenderer;

// implementation of drawing funcs
inline void R_DrawSpriteModel( cl_entity_t *e ) { g_SpriteRenderer.DrawSpriteModelInternal( e ); }
inline mspriteframe_t *R_GetSpriteFrame( const model_t *m_pSpriteModel, int frame )
{
	return g_SpriteRenderer.GetSpriteFrame( m_pSpriteModel, frame );
} 

#endif// R_SPRITE_H