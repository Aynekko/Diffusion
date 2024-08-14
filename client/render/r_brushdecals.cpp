/*
r_decals.cpp - decal surfaces rendering
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
#include "pm_movevars.h"
#include "r_shader.h"
#include "r_world.h"
#include <algorithm>

#define MAX_DECALCLIPVERT 32
#define MAX_RENDER_DECALS 4096

static struct decalarray_s
{
	Vector4D texcoord0;
	Vector4D texcoord1;
	Vector4D texcoord2;
	byte lightstyles[4];
	Vector position;
} decal_verts[MAX_DECALCLIPVERT];

static GLuint decal_vao;
static GLuint decal_vbo;
int num_render_decals = 0;
decal_t *decal_list[MAX_RENDER_DECALS];
short cached_decal_texture = -1;
int cached_brush_texture = -1;
int cached_lightmap = -1;
word cached_shader = -1;

static void R_CreateDecalsVAO()
{
	const int decalSize = sizeof(struct decalarray_s);
	pglGenVertexArrays(1, &decal_vao);
	pglGenBuffersARB(1, &decal_vbo);
	pglBindVertexArray(decal_vao);
	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, decal_vbo);

	pglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, decalSize, (void*)offsetof(decalarray_s, texcoord0));
	pglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD0);
	pglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, decalSize, (void*)offsetof(decalarray_s, texcoord1));
	pglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD1);
	pglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, decalSize, (void*)offsetof(decalarray_s, texcoord2));
	pglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD2);
	pglVertexAttribPointerARB(ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, GL_FALSE, decalSize, (void*)offsetof(decalarray_s, lightstyles));
	pglEnableVertexAttribArrayARB(ATTR_INDEX_LIGHT_STYLES);
	pglVertexAttribPointerARB(ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, decalSize, (void*)offsetof(decalarray_s, position));
	pglEnableVertexAttribArrayARB(ATTR_INDEX_POSITION);
	pglBindVertexArray(0);
}

/*
================
R_DrawProjectDecal
================
*/
void R_DrawProjectDecal(decal_t* decal, float* v, int numVerts)
{
	GL_Bind(GL_TEXTURE0, decal->texture);

	pglBegin(GL_POLYGON);
	for (int i = 0; i < numVerts; i++, v += VERTEXSIZE)
	{
		Vector av = RI->objectMatrix.VectorTransform(Vector(v));
		pglTexCoord2f(v[3], v[4]);
		pglVertex3fv(av);
	}
	pglEnd();
}

int SortDecals( const decal_t *a, const decal_t *b )
{	
	// sort priority
	// 1. shaders
	if( a->shaderNum != b->shaderNum )
		return a->shaderNum > b->shaderNum;

	// 2. texture number
	if( a->texture != b->texture )
		return a->texture > b->texture;

	// 3. lightmap texture number
	if( a->psurface->info->lightmaptexturenum != b->psurface->info->lightmaptexturenum )
		return a->psurface->info->lightmaptexturenum > b->psurface->info->lightmaptexturenum;

	return 0;
}

//============================================================================================
// DrawSingleDecal: render decal from the list
//============================================================================================
bool DrawSingleDecal( decal_t* decal, bool project )
{
	int	numVerts;
	float* v, s, t;
	Vector4D	lmcoord0, lmcoord1;

	// check for valid
	if (!decal->psurface)
		return false; // bad decal?

	cl_entity_t* e = GET_ENTITY(decal->entityIndex);
	v = DECAL_SETUP_VERTS(decal, decal->psurface, decal->texture, &numVerts);

	if (numVerts > MAX_DECALCLIPVERT) // engine limit > renderer limit
		numVerts = MAX_DECALCLIPVERT;

	if (project)
	{
		R_DrawProjectDecal(decal, v, numVerts);
		return true;
	}

	if (cached_shader != decal->shaderNum)
	{
		GL_BindShader(&glsl_programs[decal->shaderNum]);
		cached_shader = decal->shaderNum;

		// update transformation matrix
		gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];
		pglUniformMatrix4fvARB( RI->currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );

		pglUniform1fvARB( RI->currentshader->u_LightStyleValues, MAX_LIGHTSTYLES, &tr.lightstyles[0] );
		pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
	}

	mextrasurf_t* es = decal->psurface->info;
	bvert_t* mv = &world->vertexes[es->firstvertex];
	msurface_t* surf = decal->psurface;
	mtexinfo_t* tex = surf->texinfo;
	Vector normal = FBitSet(surf->flags, SURF_PLANEBACK) ? -surf->plane->normal : surf->plane->normal;
	int decalFlags = 0;
	
	if( cached_decal_texture != decal->texture )
	{
		GL_Bind( GL_TEXTURE0, decal->texture );
		cached_decal_texture = decal->texture;
	}

	if( cached_brush_texture != surf->texinfo->texture->gl_texturenum )
	{
		GL_Bind( GL_TEXTURE1, surf->texinfo->texture->gl_texturenum );
		cached_brush_texture = surf->texinfo->texture->gl_texturenum;
	}

	if( cached_lightmap != tr.lightmaps[es->lightmaptexturenum].lightmap )
	{
		GL_Bind( GL_TEXTURE2, tr.lightmaps[es->lightmaptexturenum].lightmap );
		cached_lightmap = tr.lightmaps[es->lightmaptexturenum].lightmap;
	}

	r_stats.c_total_tris += (numVerts - 2);

	// prepare decal_array for rendering
	for (int i = 0; i < numVerts; i++, v += VERTEXSIZE)
	{
		s = (DotProductA(v, tex->vecs[0]) + tex->vecs[0][3]) / tex->texture->width;
		t = (DotProductA(v, tex->vecs[1]) + tex->vecs[1][3]) / tex->texture->height;
		R_LightmapCoords(surf, v, lmcoord0, 0); // styles 0-1
		R_LightmapCoords(surf, v, lmcoord1, 2); // styles 2-3
		decal_verts[i].texcoord0.Init(v[3], v[4], s, t);
		decal_verts[i].texcoord1 = lmcoord0;
		decal_verts[i].texcoord2 = lmcoord1;
		memcpy(decal_verts[i].lightstyles, surf->styles, 4);

		if (!CVAR_TO_BOOL(r_polyoffset))
			decal_verts[i].position = Vector(v) + normal * 0.05;
		else decal_verts[i].position = v;
	}

	pglBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(decal_verts[0]) * numVerts, decal_verts, GL_STATIC_DRAW_ARB);
	pglDrawArrays(GL_POLYGON, 0, numVerts);

	r_stats.c_decals++;

	return true;
}

//============================================================================================
// PrepareSurfaceDecals: check each surface of the bmodel and add decals to a single list
//============================================================================================
void PrepareSurfaceDecals(msurface_t* fa, bool project)
{
	if( !fa->pdecals )
		return;

	if( num_render_decals == MAX_RENDER_DECALS )
		return;

	decal_t *p = fa->pdecals;

	while( p )
	{
		if( num_render_decals == MAX_RENDER_DECALS )
			break;
			
		if( p->texture )
		{
			p->shaderNum = GL_UberShaderForBmodelDecal( p );
			decal_list[num_render_decals++] = p;
			p = p->pnext;
		}
	}
}

//============================================================================================
// DrawDecalsBatch: draw all decals for specified bmodel
//============================================================================================
void DrawDecalsBatch(void)
{
	cl_entity_t* e;
	int	i;

	if (!tr.num_draw_decals)
		return;

	if( !decal_vao )
		R_CreateDecalsVAO();

	// reset cache
	cached_decal_texture = -1;
	cached_brush_texture = -1;
	cached_lightmap = -1;
	cached_shader = -1;
	num_render_decals = 0;

	e = RI->currententity;
	ASSERT(e != NULL);

	if( e->curstate.rendermode == kRenderFade )
		return;
	
	for( i = 0; i < tr.num_draw_decals; i++ )
		PrepareSurfaceDecals( tr.draw_decals[i], false );

	if( num_render_decals == 0 )
		return;

	std::sort( decal_list, decal_list + num_render_decals, SortDecals );

	GL_AlphaTest( GL_TRUE );
	GL_Blend( GL_TRUE );
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(GL_FALSE);

	if ((e->curstate.rendermode == kRenderTransTexture) || (e->curstate.rendermode == kRenderTransAdd))
		GL_Cull(GL_NONE);

	if (CVAR_TO_BOOL(r_polyoffset))
	{
		pglEnable(GL_POLYGON_OFFSET_FILL);
		pglPolygonOffset(-1.0f, -r_polyoffset->value);
	}

	pglBindVertexArray( decal_vao );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, decal_vbo );
	
	for( i = 0; i < num_render_decals; i++ )
		DrawSingleDecal( decal_list[i], false );

	pglBindVertexArray( 0 );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );

	/*
	if (R_CountPlights())
	{
		RI->objectMatrix = matrix4x4(e->origin, e->angles);	// FIXME

		for (i = 0; i < MAX_PLIGHTS; i++)
		{
			plight_t* pl = &cl_plights[i];

			if (pl->die < tr.time || !pl->radius || pl->culled)
				continue;

			if (!R_BeginDrawProjectionGLSL(pl, 0.5f))
				continue;

			for (int k = 0; k < tr.num_draw_decals; k++)
				PrepareSurfaceDecals(tr.draw_decals[k], false, true);

			R_EndDrawProjectionGLSL();
		}
	}*/

	GL_DepthMask(GL_TRUE);
	GL_Blend( GL_FALSE );
	GL_AlphaTest( GL_FALSE );

	if (CVAR_TO_BOOL(r_polyoffset))
		pglDisable(GL_POLYGON_OFFSET_FILL);

	if ((e->curstate.rendermode == kRenderTransTexture) || (e->curstate.rendermode == kRenderTransAdd) || (e->curstate.rendermode == kRenderFade) )
		GL_Cull(GL_FRONT);

	GL_SelectTexture(glConfig.max_texture_units - 1); // force to cleanup all the units
	GL_CleanUpTextureUnits(0);
	GL_BindShader(NULL);
	
	tr.num_draw_decals = 0;
}