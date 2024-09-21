//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "utils.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "r_particle.h"
#include "r_quakeparticle.h"
#include "r_weather.h"
#include "r_local.h"
#include "edict.h"
#include "r_sprite.h"
#include "r_shader.h"

#define MAX_CABLE_VERTS 65536
Vector CableVertexesArray[MAX_CABLE_VERTS];
byte CableColorArray[MAX_CABLE_VERTS][4];
int cable_numverts;

#define MAX_VOLUMETRIC_VERTS 65536
#define MAX_LIGHT_SPRITES 50 // per 1 entity - do not increase: sunflower constant has only 50 elements
Vector VolumetricVertexesArray[MAX_VOLUMETRIC_VERTS];
byte VolumetricColorArray[MAX_VOLUMETRIC_VERTS][4];
Vector2D VolumetricTexCoordsArray[MAX_VOLUMETRIC_VERTS];
int volumetric_numverts;

void HUD_DrawNormalTriangles( void )
{
	// prepare the cable arrays
	cable_numverts = 0;
	
	for( int i = 0; i < tr.num_solid_entities; i++ )
	{
		RI->currententity = tr.solid_entities[i];
		RI->currentmodel = RI->currententity->model;

		if( RI->currententity->curstate.iuser3 != -669 )
			continue;

		SET_CURRENT_ENTITY( RI->currententity );

		Vector absmin = RI->currententity->origin + RI->currententity->curstate.mins;
		Vector absmax = RI->currententity->origin + RI->currententity->curstate.maxs;
		
		if( R_CullModel( RI->currententity, absmin, absmax ) )
			continue;
		if( !Mod_CheckBoxVisible( absmin, absmax ) )
			continue;

		if( r_drawentities->value == 7 )
			DBG_DrawBBox( absmin, absmax );

		R_SetupCable( RI->currententity );
	}
	
	R_RenderCables();

	g_pParticles.Update();
}

void HUD_DrawTransparentTriangles( void )
{
	if( !g_fRenderInitialized )
	{
		gEngfuncs.GetViewAngles( (float *)RI->viewangles );
		tr.time = GET_CLIENT_TIME();
		tr.oldtime = GET_CLIENT_OLDTIME();
	}

	if( !tr.fCustomRendering )
	{
		AngleVectors( RI->viewangles, RI->vforward, RI->vright, RI->vup );
		tr.frametime = tr.time - tr.oldtime;
	}

	// prepare the cable and volumetrics arrays
	cable_numverts = 0;
	volumetric_numverts = 0;

	for( int i = 0; i < tr.num_trans_entities; i++ )
	{
		RI->currententity = tr.trans_entities[i];
		RI->currentmodel = RI->currententity->model;

		if( RI->currententity->curstate.iuser3 == -669 )
		{
			SET_CURRENT_ENTITY( RI->currententity );

			Vector absmin = RI->currententity->origin + RI->currententity->curstate.mins;
			Vector absmax = RI->currententity->origin + RI->currententity->curstate.maxs;

			if( R_CullModel( RI->currententity, absmin, absmax ) )
				continue;
			if( !Mod_CheckBoxVisible( absmin, absmax ) )
				continue;

			if( r_drawentities->value == 7 )
				DBG_DrawBBox( absmin, absmax );

			R_SetupCable( RI->currententity );
		}
		else if( RI->currententity->curstate.iuser3 == -664 )
		{
			SET_CURRENT_ENTITY( RI->currententity );

			// FIXME no culling here yet...

			R_SetupVolumetricLight( RI->currententity );
		}
	}
	
	R_RenderCables();
	R_RenderVolumetricLights();

	if( g_pParticleSystems )
		g_pParticleSystems->UpdateSystems();

	g_pParticles.Update();

	if( g_fRenderInitialized )
		R_DrawWeather();
}

//==========================================================================
// R_SetupCable
// diffusion - thanks to Bacontsu for the cable rendering code!
//==========================================================================
void R_SetupCable( cl_entity_t *e )
{
	if( e->index >= 8192 )
		return; // ¯\_(ツ)_/¯

	if( RI->params & RP_SHADOWPASS )
	{
		if (e->curstate.renderfx == kRenderFxNoShadows)
			return;

		if( RI->currentlight && (RI->currentlight->flags & CF_ONLYFORCEDSHADOWS) && (e->curstate.renderfx != kRenderFxForceShadow) )
			return;
	}
	else
	{
		if( e->curstate.renderfx == kRenderFxOnlyShadows )
			return;
	}
	
	Vector vdroppoint;
	Vector vposition1;
	Vector vposition2;
	Vector vdirection;
	Vector vmidpoint;
	
	float TargetSway = e->curstate.fuser3;
	int NumberOfCables = (int)e->curstate.vuser2.y;
	
	// origin of the cable can move when parented. if so, add shaking
	float Speed = (e->curstate.origin - e->prevstate.origin).Length() / g_fFrametime;
	TargetSway *= 1.0f + (Speed * 0.05f);

	// approach faster when shaking starts, but slower when coming to a halt
	float ApproachSpeed = (e->curstate.fuser3 > tr.cableSwayIntensity[e->index]) ? g_fFrametime * (0.1f + 0.5f * fabs( tr.cableSwayIntensity[e->index] - e->curstate.fuser3 )) : (g_fFrametime * 0.1f);
	tr.cableSwayIntensity[e->index] = CL_UTIL_Approach( TargetSway, tr.cableSwayIntensity[e->index], ApproachSpeed );
	if( (tr.time != tr.oldtime) && !(RI->params & RP_SHADOWPASS) ) // don't animate when paused or in shadowpass
		tr.cableSwayPhase[e->index] += g_fFrametime * tr.cableSwayIntensity[e->index];

	float falldepth = e->curstate.fuser1;
	if( falldepth <= 0.0f )
		falldepth = 0.001f;

	float fwidth = e->curstate.fuser2;

	Vector vpoints[50];
	int isegments = bound( 3, e->curstate.iuser2, 49 );

	int inumpoints = isegments + 1;
	float RenderAmt = (float)CL_FxBlend(e) * tr.fadeblend[e->index];
	vposition1 = e->curstate.origin; // start cable
	vposition2 = e->curstate.vuser1; // end cable

	bool bMakeWaterDrops = (e->curstate.vuser2.x > 0.01f); // rate: 0 - 255
	// choose a cable for waterdrop
	int WaterdropCable = 0;
	if( bMakeWaterDrops && NumberOfCables > 1 )
		WaterdropCable = RANDOM_LONG( 1, NumberOfCables );
	const float DeSync[] = { 0.0f, 0.666f, -0.42f, -0.834f, 0.444f, -0.711f };

	for( int cablenum = 0; cablenum < NumberOfCables; cablenum++ )
	{
		if( cable_numverts >= MAX_CABLE_VERTS - (inumpoints * 2) )
			break; // array full
		
		float SwayPhase = tr.cableSwayPhase[e->index];
		
		if( cablenum > 0 )
		{
			falldepth += e->curstate.vuser2.z * cablenum;
			SwayPhase += DeSync[bound( 0, cablenum % sizeof( DeSync ) - 1, sizeof( DeSync ) - 1 )];
		}

		if( tr.cableSwayIntensity[e->index] > 0.0f ) // sway intensity
			falldepth += sin( SwayPhase * ((float( (e->index % 4) + 1 ) * 0.5f)) ) * ((falldepth * tr.cableSwayIntensity[e->index]) * 0.1f);

		// Calculate dropping point
		vdirection = vposition2 - vposition1;
		VectorMA( vposition1, 0.5, vdirection, vmidpoint );
		vdroppoint = Vector( vmidpoint[0], vmidpoint[1], vmidpoint[2] - falldepth );

		Vector vParticlePoint = g_vecZero;
		int ParticlePointNum = -1;
		if( bMakeWaterDrops && cablenum == WaterdropCable )
			ParticlePointNum = RANDOM_LONG( 1, isegments - 1 );

		for( int i = 0; i < inumpoints; i++ )
		{
			float f = (float)i / (float)isegments;
			vpoints[i] = vposition1 * ((1.0f - f) * (1.0f - f)) + vdroppoint * ((1.0f - f) * f * 2.0f) + vposition2 * (f * f);
			if( i == ParticlePointNum ) // randomly choosen point for water drop particle
				vParticlePoint = vpoints[i];
		}

		Vector vTangent, vDir, vRight, vVertex;

		// find cable's right vector
		Vector forward = vposition2 - vposition1;
		forward.z = 0;
		forward = forward.Normalize();

		Vector right, angles;
		VectorAngles( forward, angles );
		AngleVectors( angles, NULL, right, NULL );

		for( int j = 0; j < inumpoints; j++ )
		{
			if( j == 0 ){ vTangent = vpoints[0] - vpoints[1]; }
			else { vTangent = vpoints[j-1] - vpoints[j]; }

			vDir = vpoints[j] - RI->vieworg;
			vRight = CrossProduct( vTangent, -vDir ); vRight = vRight.Normalize();

			VectorMA( vpoints[j], fwidth, vRight, vVertex );
			if( tr.cableSwayIntensity[e->index] > 0.0f ) // sway intensity
			{
				if( j != 0 && j != inumpoints - 1 ) // bacontsu - dont animate last and first vertex
					vVertex = vVertex + (right * 1.5f * tr.cableSwayIntensity[e->index] * 5.f * cos( SwayPhase * (float( (e->index % 4) + 1 ) * 0.5f) + j * 0.2f ));
			}
			CableVertexesArray[cable_numverts] = vVertex;
			CableColorArray[cable_numverts][0] = e->curstate.rendercolor.r;
			CableColorArray[cable_numverts][1] = e->curstate.rendercolor.g;
			CableColorArray[cable_numverts][2] = e->curstate.rendercolor.b;
			CableColorArray[cable_numverts][3] = RenderAmt;
			cable_numverts++;

			// repeat the first vertex
			if( j == 0 )
			{
				CableVertexesArray[cable_numverts] = vVertex;
				CableColorArray[cable_numverts][0] = e->curstate.rendercolor.r;
				CableColorArray[cable_numverts][1] = e->curstate.rendercolor.g;
				CableColorArray[cable_numverts][2] = e->curstate.rendercolor.b;
				CableColorArray[cable_numverts][3] = RenderAmt;
				cable_numverts++;
			}

			VectorMA( vpoints[j], -fwidth, vRight, vVertex );
			if( tr.cableSwayIntensity[e->index] > 0.0f ) // sway intensity
			{
				if( j != 0 && j != inumpoints - 1 ) // bacontsu - dont animate last and first vertex
					vVertex = vVertex + (right * 1.5f * tr.cableSwayIntensity[e->index] * 5.f * cos( SwayPhase * (float( (e->index % 4) + 1 ) * 0.5f) + j * 0.2f ));
			}
			CableVertexesArray[cable_numverts] = vVertex;
			CableColorArray[cable_numverts][0] = e->curstate.rendercolor.r;
			CableColorArray[cable_numverts][1] = e->curstate.rendercolor.g;
			CableColorArray[cable_numverts][2] = e->curstate.rendercolor.b;
			CableColorArray[cable_numverts][3] = RenderAmt;
			cable_numverts++;
		}

		// repeat the last vertex
		CableVertexesArray[cable_numverts] = vVertex;
		CableColorArray[cable_numverts][0] = e->curstate.rendercolor.r;
		CableColorArray[cable_numverts][1] = e->curstate.rendercolor.g;
		CableColorArray[cable_numverts][2] = e->curstate.rendercolor.b;
		CableColorArray[cable_numverts][3] = RenderAmt;
		cable_numverts++;

		// is it time to create water particle?
		if( bMakeWaterDrops && cablenum == WaterdropCable ) // rate: 0 - 255
		{
			if( tr.time >= tr.ParticleTime[e->index] )
			{
				g_pParticles.WaterDrop( e->index, vParticlePoint );
				tr.ParticleTime[e->index] = tr.time + (1.0f / (0.1f + (e->curstate.vuser2.x * 0.2f)));
			}
		}
	}
}

void R_RenderCables( void )
{
	if( cable_numverts <= 0 )
		return;
	
	GL_Texture2D( GL_FALSE );
	GL_Cull( GL_NONE );
	if( glState.drawTrans )
	{
		GL_DepthMask( GL_FALSE );
		GL_Blend( GL_TRUE );
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else
	{
		GL_Blend( GL_FALSE );
		GL_DepthTest( GL_TRUE );
		GL_DepthMask( GL_TRUE );
	}

	if( tr.fogEnabled )
	{
		GL_BindShader( glsl.genericFog );
		GL_Bind( GL_TEXTURE0, tr.whiteTexture ); // need to bind something otherwise cables can disappear
		pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
	}

	pglBindVertexArray( NULL );
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, sizeof( Vector ), CableVertexesArray );
	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, CableColorArray );

	pglDrawArrays( GL_TRIANGLE_STRIP, 0, cable_numverts );

	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );

	GL_Texture2D( GL_TRUE );
	GL_DepthMask( GL_TRUE );
	GL_Blend( GL_FALSE );
	GL_Cull( GL_FRONT );

	if( tr.fogEnabled )
		GL_BindShader( GL_NONE );

	r_stats.c_cables++;
	r_stats.dip_count++;
}

//============================================================================================
// diffusion - sprite-based volumetric light
//============================================================================================
const Vector2D sunflower[50] =
{
	Vector2D( 0.000, 0.000 ),
	Vector2D( -0.019, -0.068 ),
	Vector2D( -0.105, 0.064 ),
	Vector2D( 0.116, 0.108 ),
	Vector2D( 0.086, -0.167 ),
	Vector2D( -0.208, -0.043 ),
	Vector2D( 0.016, 0.235 ),
	Vector2D( 0.241, -0.086 ),
	Vector2D( -0.158, -0.224 ),
	Vector2D( -0.184, 0.227 ),
	Vector2D( 0.283, 0.123 ),
	Vector2D( 0.044, -0.322 ),
	Vector2D( -0.337, 0.046 ),
	Vector2D( 0.141, 0.325 ),
	Vector2D( 0.286, -0.232 ),
	Vector2D( -0.312, -0.220 ),
	Vector2D( -0.132, 0.372 ),
	Vector2D( 0.406, 0.028 ),
	Vector2D( -0.085, -0.411 ),
	Vector2D( -0.383, 0.198 ),
	Vector2D( 0.302, 0.323 ),
	Vector2D( 0.236, -0.388 ),
	Vector2D( -0.448, -0.125 ),
	Vector2D( 0.000, 0.476 ),
	Vector2D( 0.468, -0.131 ),
	Vector2D( -0.258, -0.424 ),
	Vector2D( -0.346, 0.370 ),
	Vector2D( 0.458, 0.237 ),
	Vector2D( 0.107, -0.515 ),
	Vector2D( -0.534, 0.037 ),
	Vector2D( 0.182, 0.513 ),
	Vector2D( 0.452, -0.319 ),
	Vector2D( -0.437, -0.355 ),
	Vector2D( -0.228, 0.524 ),
	Vector2D( 0.575, 0.079 ),
	Vector2D( -0.080, -0.583 ),
	Vector2D( -0.548, 0.238 ),
	Vector2D( 0.382, 0.470 ),
	Vector2D( 0.354, -0.502 ),
	Vector2D( -0.586, -0.208 ),
	Vector2D( -0.043, 0.629 ),
	Vector2D( 0.625, -0.130 ),
	Vector2D( -0.298, -0.573 ),
	Vector2D( -0.477, 0.447 ),
	Vector2D( 0.565, 0.343 ),
	Vector2D( 0.180, -0.644 ),
	Vector2D( -0.676, 0.001 ),
	Vector2D( 0.185, 0.658 ),
	Vector2D( 0.591, -0.359 ),
	Vector2D( -0.510, -0.477 ),
};

void R_SetupVolumetricLight( cl_entity_t *e )
{
	if( !tr.volumetric_light_texture )
		return;
	
	Vector vTangent, vDir, vRight, vVertex;
	Vector v_angles = e->curstate.angles;
	Vector ang_forward, ang_right, ang_up;
	gEngfuncs.pfnAngleVectors( v_angles, ang_forward, ang_right, ang_up );

	float light_length = e->curstate.vuser1.x;
	float light_width = e->curstate.vuser1.y;
	int num_sprites = e->curstate.vuser1.z;

	// this way we can shape our cone how we want
	float start_cone_scale = e->curstate.vuser2.x; // distance between sprites at start point
	float end_cone_scale = e->curstate.vuser2.y; // distance between sprites at ending point

	Vector vmidpoint; // average midpoint of the cone
	vmidpoint = e->curstate.origin + ang_forward * light_length * 0.5f;
	float flFade = 1.0f;
	float flDist = (RI->vieworg - vmidpoint).Length();
	float dist_check = light_length; // full length

	bool spawn_particles = (e->curstate.vuser2.z == 1.0f) && (tr.time >= tr.ParticleTime[e->index]);

	num_sprites = bound( 1, num_sprites, MAX_LIGHT_SPRITES );

	for( int i = 0; i < num_sprites; i++ )
	{
		Vector vposition1 = e->curstate.origin + ang_up * sunflower[i].x * 100 * start_cone_scale + ang_right * sunflower[i].y * 100 * start_cone_scale;
		Vector vposition2 = e->curstate.origin + ang_forward * light_length + ang_up * sunflower[i].x * 100 * end_cone_scale + ang_right * sunflower[i].y * 100 * end_cone_scale;

		// find right vector
		Vector forward = (vposition2 - vposition1).Normalize();

		// spawn a particle
		if( spawn_particles ) // 3 - fullbright dustmotes
		{
			float ParticleDist = RANDOM_FLOAT( light_length * 0.1f, light_length * 0.75f );
			Vector Color = Vector( e->curstate.rendercolor.r, e->curstate.rendercolor.g, e->curstate.rendercolor.b ) / 255.0f;
			// remapval on ParticleDist is controlling alpha: closer to light emission means brighter particle
			g_pParticles.SmokeVolume( e->index, 3, vposition1 + forward * ParticleDist, g_vecZero, g_vecZero, Color, 0.0f, 1.0f - RemapVal( ParticleDist, light_length * 0.1f, light_length * 0.75f, 0.0f, 1.0f ), 1000 );
		}

		forward.z = 0;

		vTangent = vposition1 - vposition2;
		vDir = vposition1 - RI->vieworg;
		vRight = CrossProduct( vTangent, -vDir ); vRight = vRight.Normalize();

		float fader = 1.0f;
		float DotP = 1.0f;
		if( flDist <= dist_check )
		{
			DotP = fabs( DotProduct( RI->vforward, ang_right ) );
			fader = RemapVal( flDist, 0, dist_check, 0, 1 );
		}
		flFade = DotP + fader;
		flFade = bound( 0, flFade, 1 );
		float transparency = flFade * (float)CL_FxBlend( e ) * tr.fadeblend[e->index];

		VectorMA( vposition1, light_width, vRight, vVertex );
		VolumetricColorArray[volumetric_numverts][0] = e->curstate.rendercolor.r;
		VolumetricColorArray[volumetric_numverts][1] = e->curstate.rendercolor.g;
		VolumetricColorArray[volumetric_numverts][2] = e->curstate.rendercolor.b;
		VolumetricColorArray[volumetric_numverts][3] = transparency;
		VolumetricTexCoordsArray[volumetric_numverts] = Vector2D( 0.0f, 0.0f );
		VolumetricVertexesArray[volumetric_numverts] = vVertex;
		volumetric_numverts++;

		// repeat first vertex
		VolumetricColorArray[volumetric_numverts][0] = e->curstate.rendercolor.r;
		VolumetricColorArray[volumetric_numverts][1] = e->curstate.rendercolor.g;
		VolumetricColorArray[volumetric_numverts][2] = e->curstate.rendercolor.b;
		VolumetricColorArray[volumetric_numverts][3] = transparency;
		VolumetricTexCoordsArray[volumetric_numverts] = Vector2D( 0.0f, 0.0f );
		VolumetricVertexesArray[volumetric_numverts] = vVertex;
		volumetric_numverts++;

		VectorMA( vposition1, -light_width, vRight, vVertex );
		VolumetricColorArray[volumetric_numverts][0] = e->curstate.rendercolor.r;
		VolumetricColorArray[volumetric_numverts][1] = e->curstate.rendercolor.g;
		VolumetricColorArray[volumetric_numverts][2] = e->curstate.rendercolor.b;
		VolumetricColorArray[volumetric_numverts][3] = transparency;
		VolumetricTexCoordsArray[volumetric_numverts] = Vector2D( 1.0f, 0.0f );
		VolumetricVertexesArray[volumetric_numverts] = vVertex;
		volumetric_numverts++;

		vTangent = vposition1 - vposition2;
		vDir = vposition2 - RI->vieworg;
		vRight = CrossProduct( vTangent, -vDir ); vRight = vRight.Normalize();
		VectorMA( vposition2, light_width, vRight, vVertex );
		VolumetricColorArray[volumetric_numverts][0] = e->curstate.rendercolor.r;
		VolumetricColorArray[volumetric_numverts][1] = e->curstate.rendercolor.g;
		VolumetricColorArray[volumetric_numverts][2] = e->curstate.rendercolor.b;
		VolumetricColorArray[volumetric_numverts][3] = transparency;
		VolumetricTexCoordsArray[volumetric_numverts] = Vector2D( 0.0f, 1.0f );
		VolumetricVertexesArray[volumetric_numverts] = vVertex;
		volumetric_numverts++;

		VectorMA( vposition2, -light_width, vRight, vVertex );
		VolumetricColorArray[volumetric_numverts][0] = e->curstate.rendercolor.r;
		VolumetricColorArray[volumetric_numverts][1] = e->curstate.rendercolor.g;
		VolumetricColorArray[volumetric_numverts][2] = e->curstate.rendercolor.b;
		VolumetricColorArray[volumetric_numverts][3] = transparency;
		VolumetricTexCoordsArray[volumetric_numverts] = Vector2D( 1.0f, 1.0f );
		VolumetricVertexesArray[volumetric_numverts] = vVertex;
		volumetric_numverts++;

		// repeat last vertex
		VolumetricColorArray[volumetric_numverts][0] = e->curstate.rendercolor.r;
		VolumetricColorArray[volumetric_numverts][1] = e->curstate.rendercolor.g;
		VolumetricColorArray[volumetric_numverts][2] = e->curstate.rendercolor.b;
		VolumetricColorArray[volumetric_numverts][3] = transparency;
		VolumetricTexCoordsArray[volumetric_numverts] = Vector2D( 1.0f, 1.0f );
		VolumetricVertexesArray[volumetric_numverts] = vVertex;
		volumetric_numverts++;
	}

	if( spawn_particles )
		tr.ParticleTime[e->index] = tr.time + RANDOM_FLOAT(2.5f, 3.5f);
}

void R_RenderVolumetricLights( void )
{
	if( volumetric_numverts <= 0 )
		return;
#if 0 // animated sprite
	int texture = SPR_Load( "sprites/volumetric.spr" );

	model_t *sprite = (struct model_s *)gEngfuncs.GetSpritePointer( texture );
	if( !sprite )
		return;
	msprite_t *m_pSpriteHeader = (msprite_t *)sprite->cache.data;
	if( !m_pSpriteHeader )
		return;

	// animate
	float frame = (int)(tr.time * 10) % m_pSpriteHeader->numframes;
	if( !gEngfuncs.pTriAPI->SpriteTexture( sprite, (int)frame ) )
		return;
#else
	GL_Bind( GL_TEXTURE0, tr.volumetric_light_texture );
#endif

	GL_Cull( GL_NONE );
	GL_Blend( GL_TRUE );
	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE );
	GL_DepthMask( GL_FALSE );

	pglBindVertexArray( NULL );
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, sizeof( Vector ), VolumetricVertexesArray );
	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, VolumetricColorArray );
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, 0, VolumetricTexCoordsArray );

	pglDrawArrays( GL_TRIANGLE_STRIP, 0, volumetric_numverts );

	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	
	GL_Blend( GL_FALSE );
	GL_Cull( GL_FRONT );

	r_stats.dip_count++;
}