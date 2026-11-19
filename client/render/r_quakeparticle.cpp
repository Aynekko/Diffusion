/*
gl_rpart.cpp - quake-like particle system
Copyright (C) 2014 Uncle Mike

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
#include "const.h"
#include <mathlib.h>
#include "r_local.h"
#include "com_model.h"
#include "r_quakeparticle.h"
#include "pm_defs.h"
#include "event_api.h"
#include "triangleapi.h"
#include "r_sprite.h"
#include "pm_movevars.h"
#include "weaponinfo.h"
#include "r_shader.h"

#define MAX_PARTICLE_VERTICES 65536

CQuakePartSystem g_pParticles;

static Vector m_vertexarray[MAX_PARTICLE_VERTICES];
static byte	m_colorarray[MAX_PARTICLE_VERTICES][4];
static Vector2D	m_coordsarray[MAX_PARTICLE_VERTICES];
static word	m_indexarray[MAX_PARTICLE_VERTICES];
static int m_iNumVerts, m_iNumIndex;

// diffusion - now particles are rendering through RangeElements, fast as heck! :)
// TODO - is it possible to allocate those memblock automatically for each texture?..

MemBlock<CQuakePart>ParticleArray_Default( MAX_PARTICLES ); // TYPE_DEFAULT
MemBlock<CQuakePart>ParticleArray_Dustmote( MAX_PARTICLES ); // TYPE_DUSTMOTE
MemBlock<CQuakePart>ParticleArray_Sparks( MAX_PARTICLES ); // TYPE_SPARKS
MemBlock<CQuakePart>ParticleArray_Smoke( MAX_PARTICLES ); // TYPE_SMOKE
MemBlock<CQuakePart>ParticleArray_Waterring( MAX_PARTICLES ); // TYPE_WATERRING
MemBlock<CQuakePart>ParticleArray_Sand( MAX_PARTICLES ); // TYPE_SAND
MemBlock<CQuakePart>ParticleArray_Dirt( MAX_PARTICLES ); // TYPE_DIRT
MemBlock<CQuakePart>ParticleArray_Fireball( MAX_PARTICLES ); // TYPE_FIREBALL
MemBlock<CQuakePart>ParticleArray_Blood( MAX_PARTICLES ); // TYPE_BLOOD
MemBlock<CQuakePart>ParticleArray_Bubbles( MAX_PARTICLES ); // TYPE_BUBBLES
MemBlock<CQuakePart>ParticleArray_Beamring( MAX_PARTICLES ); // TYPE_BEAMRING
MemBlock<CQuakePart>ParticleArray_WaterDrop( MAX_PARTICLES ); // TYPE_WATERDROP
MemBlock<CQuakePart>ParticleArray_SingleDrop( MAX_PARTICLES ); // TYPE_SINGLEDROP
MemBlock<CQuakePart>ParticleArray_WaterFall( MAX_PARTICLES ); // TYPE_WATERFALL
MemBlock<CQuakePart>ParticleArray_Sparkles( MAX_PARTICLES ); // TYPE_SPARKLES
static int partcounter = 0;

//=============================================================================
// gun smoke trail
//=============================================================================
#define GUNSMOKE_ADDITIVE 0	// 1 = additive (like the sprite version), 0 = alpha-blended grey smoke
#define GUNSMOKE_SUBDIV 6
#define GUNSMOKE_DRAW_MAX (GUNSMOKE_MAX_POINTS * GUNSMOKE_SUBDIV)

static const float GUNSMOKE_HEAT = 50.0f;	// heat needed to start the smoke
static const float GUNSMOKE_COOL_TIME = 0.9f; // a pause this long resets the heat
static const float GUNSMOKE_BURN_TIME = 1.2f; // base drift-off duration
static const float GUNSMOKE_BURN_MAX = 4.0f;
static const float GUNSMOKE_PAUSE = 0.55f; // trigger must be up this long before smoke starts
static const float GUNSMOKE_HEAT_MULT = 1.0f;
static const float GUNSMOKE_LIFE = 1.05f; // life of a ribbon point
static const float GUNSMOKE_RISE = 26.0f; // units/s upward
static const float GUNSMOKE_WOBBLE = 20.0f; // sideways curl by the end of life
static const float GUNSMOKE_SAMPLE_DT = 0.035f;
static const float GUNSMOKE_LINK_DIST = 32.0f; // farther apart than this = separate ribbons
static const float GUNSMOKE_V_PER_UNIT = 0.1f; // texture tiles per world unit (1 tile per 10 units)
static const float GUNSMOKE_ALPHA_SCALE = 2.0f; // alpha-blend mode only: brightness -> alpha

struct GunSmokeVertex_t
{
	Vector	p;
	float	w;	// half-width
	float	a;	// brightness
	float	v;	// texture V
	bool	brk;	// start a new strip at this vertex
};

static GunSmokeVertex_t s_gunSmokeVerts[GUNSMOKE_DRAW_MAX];

//===============================================================================
// InitializeParticle: set default parameters for a single particle
//===============================================================================
CQuakePart InitializeParticle( void )
{
	CQuakePart dst;

	dst.m_vecOrigin = g_vecZero; // position for current frame
	dst.m_vecLastOrg = g_vecZero; // position from previous frame
	dst.m_vecVelocity = g_vecZero;	// linear velocity
	dst.m_vecAccel = g_vecZero;
	dst.m_vecColor = Vector( 1, 1, 1 );
	dst.m_vecColorVelocity = g_vecZero;
	dst.m_flAlpha = 1.0f;
	dst.m_flAlphaVelocity = 0;
	dst.m_flRadius = 1.0f;
	dst.m_flRadiusVelocity = 0;
	dst.m_flRadiusMax = 0;
	dst.m_flLength = 1.0f;
	dst.m_flLengthVelocity = 0;
	dst.m_flRotation = 0; // texture ROLL angle
	dst.m_flRotationVelocity = 0;
	dst.m_flBounceFactor = 0;
	dst.m_flDistance = 0; // don't allow particle over a certain distance
	dst.m_flStartAlpha = 0;
	dst.m_vecAddedVelocity = g_vecZero;
	dst.m_flDieTime = 0;
	dst.m_flSinSpeed = 0;
	dst.m_vecView = g_vecZero;
	dst.m_flLastThink = 0.0f;

	// only used with flag FPART_DISTANCESCALE
	dst.m_flMinScale = 1.0f;
	dst.m_flMaxScale = 1.0f;

	dst.m_hTexture = 0;
	dst.ParticleType = TYPE_CUSTOM;
	dst.m_iFlags = 0;
	dst.newLight = 0.0f;
	dst.EntIndex = 0; // pointer to entity which emitted this particle

	return dst;
}

void CQuakePartSystem::DrawParticles( MemBlock<CQuakePart> &ParticleArray )
{				
	if( !ParticleArray.StartPass() )
		return;

	Vector Forward, Right, Up, ViewVec;
	Vector axis[3], verts[4];
	Vector absmin, absmax;
	
	CQuakePart *curParticle = ParticleArray.GetCurrent();

	if( ParticleArray.IsClear() )
		return; // no objects to draw

	if( glState.drawTrans && FBitSet( curParticle->m_iFlags, FPART_OPAQUE ) )
		return;
	else if( !glState.drawTrans && !FBitSet( curParticle->m_iFlags, FPART_OPAQUE ) )
		return;

	if( FBitSet( curParticle->m_iFlags, FPART_SKYBOX ) && !(RI->params & RP_SKYPORTALVIEW) )
		return;
	else if( (RI->params & RP_SKYPORTALVIEW) && !FBitSet( curParticle->m_iFlags, FPART_SKYBOX ) )
		return;

	// draw the particle
	if( !curParticle->m_hTexture )
		return;

	if( FBitSet( curParticle->m_iFlags, FPART_TWOSIDE ) || FBitSet( curParticle->m_iFlags, FPART_FLOATING_ORIENTED ) ) // some of them will be visible from underwater
		GL_Cull( GL_NONE );
	else
		GL_Cull( GL_FRONT );

	bool UseAdditive = FBitSet( curParticle->m_iFlags, FPART_ADDITIVE );

	m_iNumVerts = m_iNumIndex = 0;

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 0, m_vertexarray );

	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, 0, m_coordsarray );

	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_colorarray );

	float gravity = tr.movevars->gravity * 0.0075f; // diffusion - the value must be fixed - frametime is jumping
	Vector org, org2, org3, vel;

	GL_Bind( GL_TEXTURE0, curParticle->m_hTexture );

	if( glState.drawTrans )
	{
		if( UseAdditive )
			GL_BlendFunc( GL_SRC_ALPHA, GL_ONE );
		else
			GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else
	{
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		GL_AlphaTest( GL_TRUE );
		GL_AlphaFunc( GL_GREATER, 0.25f );
	}

	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	while( curParticle != NULL )
	{
		if( (m_iNumVerts + 4) >= MAX_PARTICLE_VERTICES )
		{
			// it's time to flush
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ) )
				pglDrawRangeElementsEXT( GL_QUADS, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			else pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			r_stats.c_total_tris += (m_iNumVerts / 2);
			m_iNumVerts = m_iNumIndex = 0;
			r_stats.dip_count++;
		}

		// fadeout
		float time = (tr.time - curParticle->m_flTime);
		float time2 = time * time;

		const float dt = tr.time - curParticle->m_flLastThink;
		const bool bThink = (dt > 0.0f);
		if( bThink )
			curParticle->m_flLastThink = tr.time;

		float curAlpha = curParticle->m_flAlpha + curParticle->m_flAlphaVelocity * time;
		float curRadius = curParticle->m_flRadius + curParticle->m_flRadiusVelocity * time;
		if( curParticle->m_flRadiusMax > 0 && curRadius > curParticle->m_flRadiusMax )
			curRadius = curParticle->m_flRadiusMax;
		float curLength = curParticle->m_flLength + curParticle->m_flLengthVelocity * time;
		float curRotation = curParticle->m_flRotation + curParticle->m_flRotationVelocity * time;
		Vector curColor = curParticle->m_vecColor + curParticle->m_vecColorVelocity * time;
		curColor.x = bound( 0.0f, curColor.x, 1.0f );
		curColor.y = bound( 0.0f, curColor.y, 1.0f );
		curColor.z = bound( 0.0f, curColor.z, 1.0f );
		
		if( bThink && curParticle->ParticleType == TYPE_DUSTMOTE )
		{
			// add velocity for nearby dustmotes
			for( int i = 1; i <= tr.viewparams.maxclients; i++ )
			{
				// find the closest guy to this dustmote
				cl_entity_t *ent = GET_ENTITY( i );
				if( !ent )
					continue;
				if( (ent->curstate.origin - curParticle->m_vecOrigin).Length() < 150 )
				{
					curParticle->m_vecAddedVelocity += ent->curstate.velocity * 0.001;
					curParticle->m_vecAddedVelocity.x = bound( -100, curParticle->m_vecAddedVelocity.x, 100 );
					curParticle->m_vecAddedVelocity.y = bound( -100, curParticle->m_vecAddedVelocity.y, 100 );
					curParticle->m_vecAddedVelocity.z = bound( -100, curParticle->m_vecAddedVelocity.z, 100 );
					break;
				}
			}
		}

		// special case for waterfall
		if( bThink && curParticle->ParticleType == TYPE_WATERFALL )
		{
			curParticle->m_vecVelocity.x = CL_UTIL_Approach( 0.0f, curParticle->m_vecVelocity.x, 50 * g_fFrametime );
			curParticle->m_vecVelocity.y = CL_UTIL_Approach( 0.0f, curParticle->m_vecVelocity.y, 50 * g_fFrametime );
		}

		if( (curParticle->m_iFlags & FPART_DISTANCESCALE) && (curParticle->m_flRadiusVelocity == 0.0f) )
		{
			float DistanceToParticle = (tr.viewparams.vieworg - curParticle->m_vecOrigin).Length();
			DistanceToParticle = bound( 0.1, DistanceToParticle, 1000 ) / 1000;
			float FOVfactor = bound( 15, RI->fov_x, 70 ) / 70; // for FOV above 70 scale isn't affected
			curRadius = curParticle->m_flRadius * DistanceToParticle * 10 * FOVfactor;
			curRadius = bound( curParticle->m_flMinScale, curRadius, curParticle->m_flMaxScale );
		}

		int contents = CONTENTS_SOLID; // bruh, goto might skip initializing this variable below, so use solid

		if( FBitSet( curParticle->m_iFlags, FPART_FADEIN ) )
		{
			curAlpha = curParticle->m_flStartAlpha - curParticle->m_flAlphaVelocity * time;
		}
		else
		{
			curAlpha = curParticle->m_flAlpha + curParticle->m_flAlphaVelocity * time;
			if( curAlpha <= 0.0f || curRadius <= 0.0f || curLength <= 0.0f || (curParticle->m_flDieTime > 0.0f && tr.time > curParticle->m_flDieTime) )
			{
				if( curParticle->EntIndex > 0 )
					ParticleCountPerEnt[curParticle->EntIndex]--;
				ParticleArray.DeleteCurrent();
				partcounter--;
				goto skip_particle;
			}
		}

		curParticle->m_vecVelocity += curParticle->m_vecAddedVelocity;

		org.x = curParticle->m_vecOrigin.x + curParticle->m_vecVelocity.x * time + curParticle->m_vecAccel.x * time2;
		org.y = curParticle->m_vecOrigin.y + curParticle->m_vecVelocity.y * time + curParticle->m_vecAccel.y * time2;
		org.z = curParticle->m_vecOrigin.z + curParticle->m_vecVelocity.z * time + curParticle->m_vecAccel.z * time2 * gravity;
		
		if( FBitSet( curParticle->m_iFlags, FPART_FADEIN ) )
		{
			if( curAlpha >= curParticle->m_flAlpha && time > 0.0f )
			{
				curParticle->m_vecVelocity = (org - curParticle->m_vecOrigin) / time;
				curParticle->m_vecOrigin = org;
				curParticle->m_flRadius = curParticle->m_flRadius + curParticle->m_flRadiusVelocity * time;
				curParticle->m_flLength = curParticle->m_flLength + curParticle->m_flLengthVelocity * time;
				curParticle->m_flRotation = curParticle->m_flRotation + curParticle->m_flRotationVelocity * time;
				ClearBits( curParticle->m_iFlags, FPART_FADEIN );
				curParticle->m_flTime = tr.time;
			}
		}

		if( FBitSet( curParticle->m_iFlags, FPART_SINEWAVE ) )
		{
			float spd = curParticle->m_vecVelocity.Length() * 0.0005f;
			org.x += sin( spd * curParticle->m_flSinSpeed + tr.time ) * curParticle->m_flSinSpeed;
			org.y += sin( spd * curParticle->m_flSinSpeed + (tr.time * 5.5f) + 0.7f ) * curParticle->m_flSinSpeed * 0.8f;
		}

		// special case for waterfall
		if( curParticle->ParticleType == TYPE_WATERFALL )
		{
			Vector test = org - Vector( 0, 0, curLength * 2.0f );
			if( POINT_CONTENTS( test ) == CONTENTS_WATER )
			{
				curParticle->m_flAlphaVelocity = -1.5f;
			//	if( curParticle->EntIndex > 0 )
			//		ParticleCountPerEnt[curParticle->EntIndex]--;
			//	ParticleArray.DeleteCurrent();
			//	partcounter--;
			//	goto skip_particle;
			}
		}

		contents = POINT_CONTENTS( org );

		if( FBitSet( curParticle->m_iFlags, FPART_NOTINSOLID ) && contents == CONTENTS_SOLID )
		{
			// can't have particles in solids
			if( curParticle->EntIndex > 0 )
				ParticleCountPerEnt[curParticle->EntIndex]--;

			// special case
			if( curParticle->ParticleType == TYPE_WATERDROP )
				g_pParticles.WaterLandingDroplet( 0, curParticle->m_vecLastOrg );

			ParticleArray.DeleteCurrent();
			partcounter--;
			goto skip_particle;
		}
		else if( FBitSet( curParticle->m_iFlags, FPART_UNDERWATER ) )
		{
			// underwater particle
			if( contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA )
			{			
				// not underwater
				if( curParticle->EntIndex > 0 )
					ParticleCountPerEnt[curParticle->EntIndex]--;
				ParticleArray.DeleteCurrent();
				partcounter--;
				goto skip_particle;
			}
		}
		else if( FBitSet( curParticle->m_iFlags, FPART_NOTWATER ) && (contents == CONTENTS_WATER) )
		{
			if( curParticle->EntIndex > 0 )
				ParticleCountPerEnt[curParticle->EntIndex]--;

			// special case
			if( curParticle->ParticleType == TYPE_WATERDROP )
			{
				Vector splash = R_MakeWaterSplash( curParticle->m_vecOrigin, org, 3 );
				g_pParticles.WaterLandingDroplet( 0, splash.IsNull() ? curParticle->m_vecLastOrg : splash );
			}

			// not underwater
			ParticleArray.DeleteCurrent();
			partcounter--;
			goto skip_particle;
		}

		if( FBitSet( curParticle->m_iFlags, FPART_FRICTION ) )
		{
			// water friction affected particle
			if( contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA )
			{
				// add friction		
				switch( contents )
				{
				case CONTENTS_WATER:
					curParticle->m_vecVelocity *= 0.25f;
					curParticle->m_vecAccel *= 0.25f;
					break;
				case CONTENTS_SLIME:
					curParticle->m_vecVelocity *= 0.20f;
					curParticle->m_vecAccel *= 0.20f;
					break;
				case CONTENTS_LAVA:
					curParticle->m_vecVelocity *= 0.10f;
					curParticle->m_vecAccel *= 0.10f;
					break;
				}

				// don't add friction again
				curParticle->m_iFlags &= ~FPART_FRICTION;
				curLength = 1.0f;

				// reset
				curParticle->m_flTime = tr.time;
				curParticle->m_vecColor = curColor;
				curParticle->m_flAlpha = curAlpha;
				curParticle->m_flRadius = curRadius;
				curParticle->m_vecOrigin = org;
				curParticle->m_flRotation = curRotation;

				// don't stretch
				curParticle->m_iFlags &= ~FPART_STRETCH;
				curParticle->m_flLengthVelocity = 0.0f;
				curParticle->m_flLength = curLength;
			}
		}

		if( FBitSet( curParticle->m_iFlags, FPART_BOUNCE ) )
		{
			// bouncy particle
			pmtrace_t pmtrace;
			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( curParticle->m_vecLastOrg, org, PM_STUDIO_IGNORE, -1, &pmtrace );

			if( pmtrace.fraction != 1.0f )
			{
				// reflect velocity
				time = tr.time - (g_fFrametime + g_fFrametime * pmtrace.fraction);
				time = (time - curParticle->m_flTime);

				vel.x = curParticle->m_vecVelocity.x;
				vel.y = curParticle->m_vecVelocity.y;
				vel.z = curParticle->m_vecVelocity.z + curParticle->m_vecAccel.z * gravity * time;

				float d = DotProduct( vel, pmtrace.plane.normal ) * 2.0f;
				curParticle->m_vecVelocity = vel - pmtrace.plane.normal * d;
				curParticle->m_vecVelocity *= bound( 0.0f, curParticle->m_flBounceFactor, 1.0f );

				// check for stop or slide along the plane
				if( pmtrace.plane.normal.z > 0.0f && curParticle->m_vecVelocity.z < 1.0f )
				{
					if( pmtrace.plane.normal.z >= 0.7f )
					{
						curParticle->m_vecVelocity = g_vecZero;
						curParticle->m_vecAccel = g_vecZero;
						curParticle->m_iFlags &= ~FPART_BOUNCE;
					}
					else
					{
						// FIXME: check for new plane or free fall
						float dot = DotProduct( curParticle->m_vecVelocity, pmtrace.plane.normal );
						curParticle->m_vecVelocity += (pmtrace.plane.normal * -dot);

						dot = DotProduct( curParticle->m_vecAccel, pmtrace.plane.normal );
						curParticle->m_vecAccel += (pmtrace.plane.normal * -dot);
					}
				}

				org = pmtrace.endpos;
				curLength = 1.0f;

				// reset
				curParticle->m_flTime = tr.time;
				curParticle->m_vecColor = curColor;
				curParticle->m_flAlpha = curAlpha;
				curParticle->m_flRadius = curRadius;
				curParticle->m_vecOrigin = org;
				curParticle->m_flRotation = curRotation;

				// don't stretch
				curParticle->m_iFlags &= ~FPART_STRETCH;
				curParticle->m_flLengthVelocity = 0.0f;
				curParticle->m_flLength = curLength;
			}
		}

		// save current origin if needed
		org2 = curParticle->m_vecLastOrg;
		if( RP_NORMALPASS() )
			curParticle->m_vecLastOrg = org;

		if( FBitSet( curParticle->m_iFlags, FPART_INSTANT ) )
		{
			// instant particle
			curParticle->m_flAlphaVelocity = 0.0f;
			curParticle->m_flAlpha = 0.0f;
		}

		// float on water
		if( !FBitSet( curParticle->m_iFlags, FPART_AFLOAT ) && (FBitSet( curParticle->m_iFlags, FPART_FLOATING ) || FBitSet( curParticle->m_iFlags, FPART_FLOATING_ORIENTED )) )
		{
			if( contents == CONTENTS_WATER )
			{
				curParticle->m_vecVelocity.z = 0; // stop moving
				curParticle->m_vecAccel.z = 0;

				while( POINT_CONTENTS( org ) == CONTENTS_WATER )
					org.z += 1;

				curParticle->m_vecOrigin.z = org.z;
				curParticle->m_iFlags |= FPART_AFLOAT;
			}
		}

		if( curRadius == 1.0f )
		{
			float scale = 0.0f;

			// hack a scale up to keep quake particles from disapearing
			scale += (org.x - RI->vieworg.x) * RI->vforward.x;
			scale += (org.y - RI->vieworg.y) * RI->vforward.y;
			scale += (org.z - RI->vieworg.z) * RI->vforward.z;
			if( scale >= 20.0f ) curRadius = 1.0f + scale * 0.004f;
		}	

		// clear any added velocity this frame
		curParticle->m_vecVelocity -= curParticle->m_vecAddedVelocity;
		if( bThink )
		{
			curParticle->m_vecAddedVelocity.x = CL_UTIL_Approach( 0, curParticle->m_vecAddedVelocity.x, g_fFrametime );
			curParticle->m_vecAddedVelocity.y = CL_UTIL_Approach( 0, curParticle->m_vecAddedVelocity.y, g_fFrametime );
			curParticle->m_vecAddedVelocity.z = CL_UTIL_Approach( 0, curParticle->m_vecAddedVelocity.z, g_fFrametime );
		}

		// experimental stuff
		/*
		if( curParticle->ParticleType == TYPE_SMOKE )
		{
			if( g_pParticles.ExplosionOrg != g_vecZero )
			{
				float dist = (org - g_pParticles.ExplosionOrg).Length();
				if( dist < 200 )
				{
					curParticle->AlphaDownScale = 1.0f - (100.0f / (1.0f + dist));
				}
			}

			if( curParticle->AlphaDownScale < 1.0f )
				curParticle->AlphaDownScale += g_fFrametime * 0.1;

			curParticle->AlphaDownScale = bound( 0.01f, curParticle->AlphaDownScale, 1.0f );
			curAlpha *= curParticle->AlphaDownScale;
		}*/

		// cull invisible parts
		if( R_CullSphere( org, curRadius ) )
		{
			ParticleArray.MoveNext();
			curParticle = ParticleArray.GetCurrent();
			r_stats.c_particles_culled++;
			continue;
		}

		// vertex lit particle
		if( FBitSet( curParticle->m_iFlags, FPART_VERTEXLIGHT ) || FBitSet( curParticle->m_iFlags, FPART_VERTEXLIGHT_INSTANT ) )
		{
			Vector light;
			float fLight;
			// gather static lighting
			gEngfuncs.pTriAPI->LightAtPoint( org, light );
			light *= (1.0f / 255.0f);

			// gather dynamic lighting
			light += R_LightsForPoint( org, curRadius );

			// renormalize lighting
			float f = Q_max( Q_max( light.x, light.y ), light.z );
			if( f > 1.0f ) light *= (1.0f / f);

			// diffusion - use average and clip, so it won't become too dark...
			fLight = bound( 0.25f, (light.x + light.y + light.z) / 3.0f, 1.0f );

			if( FBitSet( curParticle->m_iFlags, FPART_VERTEXLIGHT_INSTANT ) )
				curColor *= fLight;	// multiply to diffuse
			else
			{
				if( curParticle->newLight <= 0.0f )
					curParticle->newLight = fLight;
				else if( bThink )
					curParticle->newLight = CL_UTIL_Approach( fLight, curParticle->newLight, g_fFrametime * 3 );
		
				curColor *= curParticle->newLight;
			}
		}

		if( RI->params & RP_SKYPORTALVIEW )
			curColor *= 0.5f * (1.0f + tr.sunlightscale);

		curColor.x = bound( 0.0f, curColor.x, 1.0f );
		curColor.y = bound( 0.0f, curColor.y, 1.0f );
		curColor.z = bound( 0.0f, curColor.z, 1.0f );

		// prepare to draw
		if( curLength != 1.0f )
		{
			// find orientation vectors
			axis[0] = RI->vieworg - org;
			axis[1] = org2 - org;
			axis[2] = CrossProduct( axis[0], axis[1] );

			axis[1] = axis[1].Normalize();
			axis[2] = axis[2].Normalize();

			// find normal
			axis[0] = CrossProduct( axis[1], axis[2] );
			axis[0] = axis[0].Normalize();

			org3 = org + (axis[1] * -curLength);
			axis[2] *= curRadius;

			// setup vertexes
			verts[0] = org3 - axis[2];
			verts[1] = org3 + axis[2];
			verts[2] = org + axis[2];
			verts[3] = org - axis[2];
		}
		else
		{
			// diffusion - look at player, but don't rotate with view rotation
			if( !curParticle->m_vecView.IsNull() )
				VectorAngles( curParticle->m_vecView, ViewVec );
			else if( FBitSet( curParticle->m_iFlags, FPART_AFLOAT ) && FBitSet( curParticle->m_iFlags, FPART_FLOATING_ORIENTED ) ) // look at the ceiling
				VectorAngles( Vector( 0, 0, -1 ), ViewVec );
			else
				VectorAngles( org - RI->vieworg, ViewVec );

			gEngfuncs.pfnAngleVectors( ViewVec, Forward, Right, Up );

			// Rotate it around its normal
			RotatePointAroundVector( axis[1], Forward, Right, curRotation );
			axis[2] = CrossProduct( Forward, axis[1] );

			// the normal should point at the viewer
			axis[0] = -Forward;

			// Scale the axes by radius
			axis[1] *= curRadius;
			axis[2] *= curRadius;

			verts[0] = org + axis[1] - axis[2];
			verts[1] = org + axis[1] + axis[2];
			verts[2] = org - axis[1] + axis[2];
			verts[3] = org - axis[1] - axis[2];
		}

		{ // move out of the skip_particle label's scope...
			const byte alphaByte = (byte)(bound( 0.0f, curAlpha, 1.0f ) * 255.0f);
			const byte r = (byte)(curColor.x * 255.0f);
			const byte g = (byte)(curColor.y * 255.0f);
			const byte b = (byte)(curColor.z * 255.0f);

			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = r;
			m_colorarray[m_iNumVerts][1] = g;
			m_colorarray[m_iNumVerts][2] = b;
			m_colorarray[m_iNumVerts][3] = alphaByte;
			m_vertexarray[m_iNumVerts] = verts[0];
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = r;
			m_colorarray[m_iNumVerts][1] = g;
			m_colorarray[m_iNumVerts][2] = b;
			m_colorarray[m_iNumVerts][3] = alphaByte;
			m_vertexarray[m_iNumVerts] = verts[1];
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = r;
			m_colorarray[m_iNumVerts][1] = g;
			m_colorarray[m_iNumVerts][2] = b;
			m_colorarray[m_iNumVerts][3] = alphaByte;
			m_vertexarray[m_iNumVerts] = verts[2];
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = r;
			m_colorarray[m_iNumVerts][1] = g;
			m_colorarray[m_iNumVerts][2] = b;
			m_colorarray[m_iNumVerts][3] = alphaByte;
			m_vertexarray[m_iNumVerts] = verts[3];
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;
		}

		ParticleArray.MoveNext();

	skip_particle:
		curParticle = ParticleArray.GetCurrent();
	}

	if( m_iNumVerts && m_iNumIndex )
	{
		// flush any remaining verts
		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ) )
			pglDrawRangeElementsEXT( GL_QUADS, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
		else
			pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
		r_stats.c_total_tris += (int)(m_iNumVerts * 0.5f);
		r_stats.dip_count++;
	}

	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );

	if( !glState.drawTrans )
	{
		GL_AlphaTest( GL_FALSE );
		GL_AlphaFunc( GL_GREATER, DEFAULT_ALPHATEST );
	}
}

bool CQuakePart::Evaluate( float gravity )
{
	if( glState.drawTrans && FBitSet( m_iFlags, FPART_OPAQUE ) )
		return true;
	else if( !glState.drawTrans && !FBitSet( m_iFlags, FPART_OPAQUE ) )
		return true;
	
	if( FBitSet( m_iFlags, FPART_SKYBOX ) && !(RI->params & RP_SKYPORTALVIEW) )
		return true;
	else if( (RI->params & RP_SKYPORTALVIEW) && !FBitSet( m_iFlags, FPART_SKYBOX ) )
		return true;
	
	Vector org, org2, org3, vel;

	float time = (tr.time - m_flTime);
	float time2 = time * time;

	const float dt = tr.time - m_flLastThink;
	const bool bThink = (dt > 0.0f);
	if( bThink )
		m_flLastThink = tr.time;

	float curAlpha;
	float curRadius = m_flRadius + m_flRadiusVelocity * time;
	if( m_flRadiusMax > 0 && curRadius > m_flRadiusMax )
		curRadius = m_flRadiusMax;
	float curLength = m_flLength + m_flLengthVelocity * time;
	float curRotation = m_flRotation + m_flRotationVelocity * time;

	if( bThink && ParticleType == TYPE_DUSTMOTE )
	{
		// add velocity for nearby dustmotes
		for( int i = 1; i <= tr.viewparams.maxclients; i++ )
		{
			// find the closest guy to this dustmote
			cl_entity_t *ent = GET_ENTITY( i );
			if( !ent )
				continue;
			if( (ent->curstate.origin - m_vecOrigin).Length() < 150 )
			{
				m_vecAddedVelocity += ent->curstate.velocity * 0.001;
				m_vecAddedVelocity.x = bound( -100, m_vecAddedVelocity.x, 100 );
				m_vecAddedVelocity.y = bound( -100, m_vecAddedVelocity.y, 100 );
				m_vecAddedVelocity.z = bound( -100, m_vecAddedVelocity.z, 100 );
				break;
			}
		}
	}

	if( (m_iFlags & FPART_DISTANCESCALE) && (m_flRadiusVelocity == 0.0f) )
	{
		float DistanceToParticle = (tr.viewparams.vieworg - m_vecOrigin).Length();
		DistanceToParticle = bound( 0.1, DistanceToParticle, 1000 ) / 1000;
		float FOVfactor = bound( 15, RI->fov_x, 70 ) / 70; // for FOV above 70 scale isn't affected
		curRadius = m_flRadius * DistanceToParticle * 10 * FOVfactor;
		curRadius = bound( m_flMinScale, curRadius, m_flMaxScale );
	}

	if( FBitSet( m_iFlags, FPART_FADEIN ) )
	{
		curAlpha = m_flStartAlpha - m_flAlphaVelocity * time;
	}
	else
	{
		curAlpha = m_flAlpha + m_flAlphaVelocity * time;

		if( curAlpha <= 0.0f || curRadius <= 0.0f || curLength <= 0.0f || (m_flDieTime > 0.0f && tr.time > m_flDieTime) )
		{
			// faded out
			return false;
		}
	}

	Vector curColor = m_vecColor + m_vecColorVelocity * time;
	curColor.x = bound( 0.0f, curColor.x, 1.0f );
	curColor.y = bound( 0.0f, curColor.y, 1.0f );
	curColor.z = bound( 0.0f, curColor.z, 1.0f );

	m_vecVelocity += m_vecAddedVelocity;

	org.x = m_vecOrigin.x + m_vecVelocity.x * time + m_vecAccel.x * time2;
	org.y = m_vecOrigin.y + m_vecVelocity.y * time + m_vecAccel.y * time2;
	org.z = m_vecOrigin.z + m_vecVelocity.z * time + m_vecAccel.z * time2 * gravity;

	if( FBitSet( m_iFlags, FPART_FADEIN ) )
	{
		if( curAlpha >= m_flAlpha && time > 0.0f )
		{
			m_vecVelocity = (org - m_vecOrigin) / time;
			m_vecOrigin = org;
			m_flRadius = m_flRadius + m_flRadiusVelocity * time;
			m_flLength = m_flLength + m_flLengthVelocity * time;
			m_flRotation = m_flRotation + m_flRotationVelocity * time;
			ClearBits( m_iFlags, FPART_FADEIN );
			m_flTime = tr.time;
		}
	}

	if( FBitSet( m_iFlags, FPART_SINEWAVE ) )
	{
		float spd = m_vecVelocity.Length() * 0.0005f;
		org.x += sin( spd * m_flSinSpeed + tr.time ) * m_flSinSpeed;
		org.y += sin( spd * m_flSinSpeed + (tr.time * 5.5f) + 0.7f ) * m_flSinSpeed * 0.8f;
	}

	int contents = POINT_CONTENTS( org );

	if( FBitSet( m_iFlags, FPART_NOTINSOLID ) && contents == CONTENTS_SOLID )
	{
		// can't have particles in solids
		return false;
	}
	else if( FBitSet( m_iFlags, FPART_UNDERWATER ) )
	{
		// underwater particle
		if( contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA )
			return false; // not underwater
	}
	else if( FBitSet( m_iFlags, FPART_NOTWATER ) && (contents == CONTENTS_WATER) )
		return false;

	if( FBitSet( m_iFlags, FPART_FRICTION ) )
	{
		// water friction affected particle
		if( contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA )
		{
			// add friction		
			switch( contents )
			{
			case CONTENTS_WATER:
				m_vecVelocity *= 0.25f;
				m_vecAccel *= 0.25f;
				break;
			case CONTENTS_SLIME:
				m_vecVelocity *= 0.20f;
				m_vecAccel *= 0.20f;
				break;
			case CONTENTS_LAVA:
				m_vecVelocity *= 0.10f;
				m_vecAccel *= 0.10f;
				break;
			}

			// don't add friction again
			m_iFlags &= ~FPART_FRICTION;
			curLength = 1.0f;

			// reset
			m_flTime = tr.time;
			m_vecColor = curColor;
			m_flAlpha = curAlpha;
			m_flRadius = curRadius;
			m_vecOrigin = org;
			m_flRotation = curRotation;

			// don't stretch
			m_iFlags &= ~FPART_STRETCH;
			m_flLengthVelocity = 0.0f;
			m_flLength = curLength;
		}
	}

	if( FBitSet( m_iFlags, FPART_BOUNCE ) )
	{
		// bouncy particle
		pmtrace_t pmtrace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( m_vecLastOrg, org, PM_STUDIO_IGNORE, -1, &pmtrace );

		if( pmtrace.fraction != 1.0f )
		{
			// reflect velocity
			time = tr.time - (g_fFrametime + g_fFrametime * pmtrace.fraction);
			time = (time - m_flTime);

			vel.x = m_vecVelocity.x;
			vel.y = m_vecVelocity.y;
			vel.z = m_vecVelocity.z + m_vecAccel.z * gravity * time;

			float d = DotProduct( vel, pmtrace.plane.normal ) * 2.0f;
			m_vecVelocity = vel - pmtrace.plane.normal * d;
			m_vecVelocity *= bound( 0.0f, m_flBounceFactor, 1.0f );

			// check for stop or slide along the plane
			if( pmtrace.plane.normal.z > 0.0f && m_vecVelocity.z < 1.0f )
			{
				if( pmtrace.plane.normal.z >= 0.7f )
				{
					m_vecVelocity = g_vecZero;
					m_vecAccel = g_vecZero;
					m_iFlags &= ~FPART_BOUNCE;
				}
				else
				{
					// FIXME: check for new plane or free fall
					float dot = DotProduct( m_vecVelocity, pmtrace.plane.normal );
					m_vecVelocity += (pmtrace.plane.normal * -dot);

					dot = DotProduct( m_vecAccel, pmtrace.plane.normal );
					m_vecAccel += (pmtrace.plane.normal * -dot);
				}
			}

			org = pmtrace.endpos;
			curLength = 1.0f;

			// reset
			m_flTime = tr.time;
			m_vecColor = curColor;
			m_flAlpha = curAlpha;
			m_flRadius = curRadius;
			m_vecOrigin = org;
			m_flRotation = curRotation;

			// don't stretch
			m_iFlags &= ~FPART_STRETCH;
			m_flLengthVelocity = 0.0f;
			m_flLength = curLength;
		}
	}

	// save current origin if needed
	org2 = m_vecLastOrg;
	if( RP_NORMALPASS() )
		m_vecLastOrg = org;

	if( FBitSet( m_iFlags, FPART_INSTANT ) )
	{
		// instant particle
		m_flAlphaVelocity = 0.0f;
		m_flAlpha = 0.0f;
	}

	// float on water
	if( !FBitSet( m_iFlags, FPART_AFLOAT ) && (FBitSet(m_iFlags, FPART_FLOATING) || FBitSet( m_iFlags, FPART_FLOATING_ORIENTED )) )
	{
		if( contents == CONTENTS_WATER )
		{
			m_vecVelocity.z = 0; // stop moving
			m_vecAccel.z = 0;

			while( POINT_CONTENTS( org ) == CONTENTS_WATER )
				org.z += 1;

			m_vecOrigin.z = org.z;
			m_iFlags |= FPART_AFLOAT;
		}
	}

	if( curRadius == 1.0f )
	{
		float scale = 0.0f;

		// hack a scale up to keep quake particles from disapearing
		scale += (org.x - RI->vieworg.x) * RI->vforward.x;
		scale += (org.y - RI->vieworg.y) * RI->vforward.y;
		scale += (org.z - RI->vieworg.z) * RI->vforward.z;
		if( scale >= 20.0f ) curRadius = 1.0f + scale * 0.004f;
	}

	// clear any added velocity this frame
	m_vecVelocity -= m_vecAddedVelocity;
	if( bThink )
	{
		m_vecAddedVelocity.x = CL_UTIL_Approach( 0, m_vecAddedVelocity.x, g_fFrametime );
		m_vecAddedVelocity.y = CL_UTIL_Approach( 0, m_vecAddedVelocity.y, g_fFrametime );
		m_vecAddedVelocity.z = CL_UTIL_Approach( 0, m_vecAddedVelocity.z, g_fFrametime );
	}
	
	// cull invisible parts
	if( R_CullSphere( org, curRadius ) )
	{
		r_stats.c_particles_culled++;
		return true;
	}

	// vertex lit particle
	if( FBitSet( m_iFlags, FPART_VERTEXLIGHT ) || FBitSet( m_iFlags, FPART_VERTEXLIGHT_INSTANT ) )
	{
		Vector light;
		float fLight;
		// gather static lighting
		gEngfuncs.pTriAPI->LightAtPoint( org, light );
		light *= (1.0f / 255.0f);

		// gather dynamic lighting
		light += R_LightsForPoint( org, curRadius );

		// renormalize lighting
		float f = Q_max( Q_max( light.x, light.y ), light.z );
		if( f > 1.0f ) light *= (1.0f / f);

		// diffusion - use average and clip, so it won't become too dark...
		fLight = bound( 0.25f, (light.x + light.y + light.z) / 3.0f, 1.0f );

		if( FBitSet( m_iFlags, FPART_VERTEXLIGHT_INSTANT ) )
			curColor *= fLight;	// multiply to diffuse
		else
		{
			if( newLight <= 0.0f )
				newLight = fLight;
			else if( bThink )
				newLight = CL_UTIL_Approach( fLight, newLight, g_fFrametime * 3 );

			curColor *= newLight;
		}
	}

	if( RI->params & RP_SKYPORTALVIEW )
		curColor *= 0.5f * (1.0f + tr.sunlightscale);

	curColor.x = bound( 0.0f, curColor.x, 1.0f );
	curColor.y = bound( 0.0f, curColor.y, 1.0f );
	curColor.z = bound( 0.0f, curColor.z, 1.0f );

	/*
	if( ParticleType == TYPE_SMOKE )
	{
		if( g_pParticles.ExplosionOrg != g_vecZero )
		{
			float dist = (org - g_pParticles.ExplosionOrg).Length();
			if( dist < 200 )
			{
				AlphaDownScale = 1.0f - (100.0f / (1.0f + dist));
			}
		}

		if( AlphaDownScale < 1.0f )
			AlphaDownScale += g_fFrametime * 0.1;

		AlphaDownScale = bound( 0.01f, AlphaDownScale, 1.0f );
		curAlpha *= AlphaDownScale;
	}*/

	Vector axis[3], verts[4];
	Vector absmin, absmax;

	// prepare to draw
	if( curLength != 1.0f )
	{
		// find orientation vectors
		axis[0] = RI->vieworg - org;
		axis[1] = org2 - org;
		axis[2] = CrossProduct( axis[0], axis[1] );

		axis[1] = axis[1].Normalize();
		axis[2] = axis[2].Normalize();

		// find normal
		axis[0] = CrossProduct( axis[1], axis[2] );
		axis[0] = axis[0].Normalize();

		org3 = org + (axis[1] * -curLength);
		axis[2] *= curRadius;

		// setup vertexes
		verts[0] = org3 - axis[2];
		verts[1] = org3 + axis[2];
		verts[2] = org + axis[2];
		verts[3] = org - axis[2];
	}
	else
	{
		// diffusion - look at player, but don't rotate with view rotation
		Vector ViewVec;
		if( !m_vecView.IsNull() )
			VectorAngles( m_vecView, ViewVec );
		else if( FBitSet( m_iFlags, FPART_AFLOAT ) && FBitSet( m_iFlags, FPART_FLOATING_ORIENTED ) ) // look at the ceiling
			VectorAngles( Vector( 0, 0, -1 ), ViewVec );
		else
			VectorAngles( org - RI->vieworg, ViewVec );
		Vector Forward, Right, Up;
		gEngfuncs.pfnAngleVectors( ViewVec, Forward, Right, Up );

		// Rotate it around its normal
		RotatePointAroundVector( axis[1], Forward, Right, curRotation );
		axis[2] = CrossProduct( Forward, axis[1] );

		// the normal should point at the viewer
		axis[0] = -Forward;

		// Scale the axes by radius
		axis[1] *= curRadius;
		axis[2] *= curRadius;

		verts[0] = org + axis[1] - axis[2];
		verts[1] = org + axis[1] + axis[2];
		verts[2] = org - axis[1] + axis[2];
		verts[3] = org - axis[1] - axis[2];
	}

	ClearBounds( absmin, absmax );
	for( int i = 0; i < 4; i++ )
		AddPointToBounds( verts[i], absmin, absmax );

	if( FBitSet( m_iFlags, FPART_TWOSIDE ) || (FBitSet( m_iFlags, FPART_AFLOAT ) && FBitSet( m_iFlags, FPART_FLOATING_ORIENTED )) ) // visible from underwater
		GL_Cull( GL_NONE );
	else
		GL_Cull( GL_FRONT );

	if( glState.drawTrans )
	{
		if( FBitSet( m_iFlags, FPART_ADDITIVE ) )
			GL_BlendFunc( GL_SRC_ALPHA, GL_ONE );
		else
			GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else
	{
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		GL_AlphaTest( GL_TRUE );
		GL_AlphaFunc( GL_GREATER, 0.25f );
	}

	// draw the particle
	GL_Bind( GL_TEXTURE0, m_hTexture );

	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if( FBitSet( m_iFlags, FPART_ADDITIVE ) )
		GL_Color4f( 1.0f, 1.0f, 1.0f, curAlpha );
	else GL_Color4f( curColor.x, curColor.y, curColor.z, curAlpha );

	pglBegin( GL_QUADS );
	pglTexCoord2f( 0.0f, 1.0f );
	pglVertex3fv( verts[0] );

	pglTexCoord2f( 0.0f, 0.0f );
	pglVertex3fv( verts[1] );

	pglTexCoord2f( 1.0f, 0.0f );
	pglVertex3fv( verts[2] );

	pglTexCoord2f( 1.0f, 1.0f );
	pglVertex3fv( verts[3] );
	pglEnd();

	if( !glState.drawTrans )
	{
		GL_AlphaTest( GL_FALSE );
		GL_AlphaFunc( GL_GREATER, DEFAULT_ALPHATEST );
	}

	r_stats.dip_count++;

	return true;
}

CQuakePartSystem :: CQuakePartSystem( void )
{
	memset( m_pParticles, 0, sizeof( CQuakePart ) * MAX_PARTICLES );
	memset( tr.ParticleTime, 0, sizeof( tr.ParticleTime ) );

	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;

	ParticleArray_Default.Clear();
	ParticleArray_Dustmote.Clear();
	ParticleArray_Sparks.Clear();
	ParticleArray_Smoke.Clear();
	ParticleArray_Waterring.Clear();
	ParticleArray_Sand.Clear();
	ParticleArray_Dirt.Clear();
	ParticleArray_Fireball.Clear();
	ParticleArray_Blood.Clear();
	ParticleArray_Bubbles.Clear();
	ParticleArray_Beamring.Clear();
	ParticleArray_WaterDrop.Clear();
	ParticleArray_SingleDrop.Clear();
	ParticleArray_WaterFall.Clear();
	ParticleArray_Sparkles.Clear();

	partcounter = 0;

	memset( ParticleCountPerEnt, 0, sizeof( ParticleCountPerEnt ) );

//	ExplosionOrg = g_vecZero;
}

CQuakePartSystem :: ~CQuakePartSystem( void )
{

}

void CQuakePartSystem :: Clear( void )
{
	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;

	for( int i = 0; i < MAX_PARTICLES; i++ )
		m_pParticles[i].pNext = &m_pParticles[i+1];

	m_pParticles[MAX_PARTICLES-1].pNext = NULL;

	memset( tr.ParticleTime, 0, sizeof( tr.ParticleTime ) );

	// loading TE shaders
	m_hDefaultParticle = FIND_TEXTURE( "*particle" );	// quake particle
	m_hSparks = LOAD_TEXTURE( "gfx/particles/spark", NULL, 0, TF_CLAMP );
	m_hSmoke = LOAD_TEXTURE( "gfx/particles/smoke", NULL, 0, TF_CLAMP );
	m_hWaterRing = LOAD_TEXTURE( "gfx/particles/waterring", NULL, 0, TF_CLAMP );
	m_hBlood = LOAD_TEXTURE( "gfx/particles/blood", NULL, 0, TF_CLAMP );
	m_hBloodSplat = LOAD_TEXTURE( "gfx/particles/blood_splat", NULL, 0, TF_CLAMP );
	m_hSand = LOAD_TEXTURE( "gfx/particles/sandparticle", NULL, 0, TF_CLAMP );
	m_hDirt = LOAD_TEXTURE( "gfx/particles/dirtparticle", NULL, 0, TF_CLAMP );
	m_hDustMote = LOAD_TEXTURE( "gfx/particles/dustmote", NULL, 0, TF_CLAMP );
	m_hExplosion = LOAD_TEXTURE( "gfx/particles/explosion", NULL, 0, TF_CLAMP );
	m_hBubble = LOAD_TEXTURE( "gfx/particles/bubble", NULL, 0, TF_CLAMP );
	m_hBeamRing = LOAD_TEXTURE( "gfx/particles/beamring", NULL, 0, TF_CLAMP );
	m_hRainDrop = LOAD_TEXTURE( "gfx/particles/raindrop2", NULL, 0, 0 );
	m_hSingleDrop = LOAD_TEXTURE( "gfx/particles/singledrop", NULL, 0, 0 );
	m_hWaterFall = LOAD_TEXTURE( "gfx/particles/splash", NULL, 0, 0 );
	m_hGunSmoke = LOAD_TEXTURE( "sprites/gunsmoke.dds", NULL, 0, 0 );
	ResetGunSmoke();

	ParsePartInfos( "data/particles.txt" );

	ParticleArray_Default.Clear();
	ParticleArray_Dustmote.Clear();
	ParticleArray_Sparks.Clear();
	ParticleArray_Smoke.Clear();
	ParticleArray_Waterring.Clear();
	ParticleArray_Sand.Clear();
	ParticleArray_Dirt.Clear();
	ParticleArray_Fireball.Clear();
	ParticleArray_Blood.Clear();
	ParticleArray_Bubbles.Clear();
	ParticleArray_Beamring.Clear();
	ParticleArray_WaterDrop.Clear();
	ParticleArray_SingleDrop.Clear();
	ParticleArray_WaterFall.Clear();
	ParticleArray_Sparkles.Clear();

	partcounter = 0;

	memset( ParticleCountPerEnt, 0, sizeof( ParticleCountPerEnt ) );

//	ExplosionOrg = g_vecZero;
}

void CQuakePartSystem :: ParsePartInfos( const char *filename )
{
	// we parse our effects each call of VidInit because we need to keep textures and sprites an actual

	ConPrintf( "loading %s\n", filename );

	char *afile = (char *)gEngfuncs.COM_LoadFile( (char *)filename, 5, NULL );

	if( !afile )
	{
		ConPrintf( "Cannot open file %s\n", filename );
		return;
	}

	char *pfile = afile;
	char token[256];
	m_iNumPartInfo = 0;

	memset( &m_pPartInfo, 0, sizeof( m_pPartInfo ));

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( m_iNumPartInfo >= MAX_PARTINFOS )
		{
			ConPrintf( "particle effects info limit exceeded %d > %d\n", m_iNumPartInfo, MAX_PARTINFOS );
			break;
		}

		CQuakePartInfo *pCur = &m_pPartInfo[m_iNumPartInfo];

		// read the effect name
		Q_strncpy( pCur->m_szName, token, sizeof( pCur->m_szName ));

		// read opening brace
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( token[0] != '{' )
		{
			ConPrintf( "found %s when expecting {\n", token );
			break;
		}

		if( !ParsePartInfo( pCur, pfile ))
			break; // something bad happens

	}

	gEngfuncs.COM_FreeFile( afile );
	ConPrintf( "%d effects parsed\n", m_iNumPartInfo );
}

CQuakePartInfo *CQuakePartSystem :: FindPartInfo( const char *name )
{
	for( int i = 0; i <	m_iNumPartInfo; i++ )
	{
		if( !Q_stricmp( m_pPartInfo[i].m_szName, name ))
			return &m_pPartInfo[i];
	}

	return NULL;
}

//===================================================================================
// CreateEffect: find an effect in data/particles.txt by name and create it
// Always goes into TYPE_CUSTOM, and is slow in performance
//===================================================================================
void CQuakePartSystem :: CreateEffect( int EntIndex, const char *name, const Vector &origin, const Vector &normal )
{
	if( !g_fRenderInitialized )
		return;

	if( !name || !*name )
		return;
	
	CQuakePartInfo *pInfo = FindPartInfo( name );

	if( !pInfo )
	{
		ConPrintf( "QuakeParticle: couldn't find effect '%s'\n", name );
		return;
	}
	
	// sparks
	int flags = pInfo->flags;
	
	if( FBitSet( flags, FPART_NOTWATER ) && POINT_CONTENTS( (float *)&origin ) == CONTENTS_WATER )
		return;

	// don't alloc too many particles
	int count = pInfo->count.Random();
	count = bound( 1, count, 1024 );

	int m_hTexture = m_hDefaultParticle;
	CQuakePart src = InitializeParticle();

	for( int i = 0; i < count; i++ )
	{
		if( pInfo->m_pSprite )
		{
			int frame = pInfo->frame.Random();
			frame = bound( 0, frame, pInfo->m_pSprite->numframes - 1 );
			mspriteframe_t *pframe = R_GetSpriteFrame( pInfo->m_pSprite, frame );
			if( pframe ) m_hTexture = pframe->gl_texturenum;
		}
		else
			m_hTexture = pInfo->m_hTexture;

		if( pInfo->normal == NORMAL_OFFSET || pInfo->normal == NORMAL_OFS_DIR )
		{
			src.m_vecOrigin.x = origin.x + pInfo->offset[0].Random() * normal.x;
			src.m_vecOrigin.y = origin.y + pInfo->offset[1].Random() * normal.y;
			src.m_vecOrigin.z = origin.z + pInfo->offset[2].Random() * normal.z;
		}
		else
		{
			src.m_vecOrigin.x = origin.x + pInfo->offset[0].Random();
			src.m_vecOrigin.y = origin.y + pInfo->offset[1].Random();
			src.m_vecOrigin.z = origin.z + pInfo->offset[2].Random();
		}

		if( pInfo->normal == NORMAL_DIRECTION || pInfo->normal == NORMAL_OFS_DIR )
		{
			src.m_vecVelocity.x = normal.x * pInfo->velocity[0].Random();
			src.m_vecVelocity.y = normal.y * pInfo->velocity[1].Random();
			src.m_vecVelocity.z = normal.z * pInfo->velocity[2].Random();
		}
		else
		{
			src.m_vecVelocity.x = pInfo->velocity[0].Random();
			src.m_vecVelocity.y = pInfo->velocity[1].Random();
			src.m_vecVelocity.z = pInfo->velocity[2].Random();
		}

		src.m_vecAccel.x = pInfo->accel[0].Random();
		src.m_vecAccel.y = pInfo->accel[1].Random();
		src.m_vecAccel.z = pInfo->accel[2].Random();
		src.m_vecColor.x = pInfo->color[0].Random();
		src.m_vecColor.y = pInfo->color[1].Random();
		src.m_vecColor.z = pInfo->color[2].Random();
		src.m_vecColorVelocity.x = pInfo->colorVel[0].Random();
		src.m_vecColorVelocity.y = pInfo->colorVel[1].Random();
		src.m_vecColorVelocity.z = pInfo->colorVel[2].Random();
		src.m_flAlpha = pInfo->alpha.Random();
		src.m_flAlphaVelocity = pInfo->alphaVel.Random();
		src.m_flRadius = pInfo->radius.Random();
		src.m_flRadiusVelocity = pInfo->radiusVel.Random();
		src.m_flLength = pInfo->length.Random();
		src.m_flLengthVelocity = pInfo->lengthVel.Random();
		src.m_flRotation = pInfo->rotation.Random();
		src.m_flRotationVelocity = pInfo->rotationVel.Random();
		src.m_flBounceFactor = pInfo->bounce.Random();
		src.EntIndex = EntIndex;
		
		if( !AddParticle( &src, m_hTexture, flags ))
			break; // out of particles?
	}
}

bool CQuakePartSystem :: ParseRandomVector( char *&pfile, RandomRange out[3] )
{
	char token[256];
	int i;

	for( i = 0; i < 3 && pfile != NULL; i++ )
	{
		pfile = COM_ParseLine( pfile, token );
		out[i] = RandomRange( token );
	}

	return (i == 3) ? true : false;
}

int CQuakePartSystem :: ParseParticleFlags( char *pfile )
{
	char token[256];
	int iFlags = 0;

	if( !pfile || !*pfile )
		return iFlags;

	while( pfile != NULL )
	{
		pfile = COM_ParseLine( pfile, token );

		if( !Q_stricmp( token, "Bounce" )) 
			iFlags |= FPART_BOUNCE;
		else if( !Q_stricmp( token, "Friction" )) 
			iFlags |= FPART_FRICTION;
		else if( !Q_stricmp( token, "Light" )) 
			iFlags |= FPART_VERTEXLIGHT;
		else if( !Q_stricmp( token, "LightInstant" ) )
			iFlags |= FPART_VERTEXLIGHT_INSTANT;
		else if( !Q_stricmp( token, "Stretch" )) 
			iFlags |= FPART_STRETCH;
		else if( !Q_stricmp( token, "Underwater" )) 
			iFlags |= FPART_UNDERWATER;
		else if( !Q_stricmp( token, "Instant" )) 
			iFlags |= FPART_INSTANT;
		else if( !Q_stricmp( token, "Additive" )) 
			iFlags |= FPART_ADDITIVE;
		else if( !Q_stricmp( token, "NotInWater" )) 
			iFlags |= FPART_NOTWATER;
		else if( !Q_stricmp( token, "FadeIn" ) )
			iFlags |= FPART_FADEIN;
		else if( !Q_stricmp( token, "Skybox" ) )
			iFlags |= FPART_SKYBOX;
		else if( !Q_stricmp( token, "Floating" ) )
			iFlags |= FPART_FLOATING;
		else if( !Q_stricmp( token, "FloatingOriented" ) )
			iFlags |= FPART_FLOATING_ORIENTED;
		else if( !Q_stricmp( token, "Opaque" ) )
			iFlags |= FPART_OPAQUE;
		else if( !Q_stricmp( token, "NotInSolid" ) )
			iFlags |= FPART_NOTINSOLID;
		else if( pfile && token[0] != '|' )
			ALERT( at_warning, "unknown value %s for 'flags'\n", token );
	}

	return iFlags;
}

bool CQuakePartSystem :: ParsePartInfo( CQuakePartInfo *info, char *&pfile )
{
	char token[256];

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile )
		{
			ALERT( at_error, "EOF without closing brace\n" );
			return false;
		}

		// description end goto next material
		if( token[0] == '}' )
		{
			m_iNumPartInfo++;
			return true;
		}
		else if( !Q_stricmp( token, "texture" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'texture'\n" );
				break;
			}

			const char *ext = COM_FileExtension( token );

			if( !Q_stricmp( ext, "tga" ) || !Q_stricmp( ext, "dds" ))
				info->m_hTexture = LOAD_TEXTURE( token, NULL, 0, TF_CLAMP );
			else if( !Q_stricmp( ext, "spr" ))
				info->m_pSprite = (model_t *)gEngfuncs.GetSpritePointer( SPR_Load( token ));

			if( !info->m_hTexture && !info->m_pSprite )
			{
				ALERT( at_error, "couldn't load texture for effect %s. using default particle\n", info->m_szName );
				info->m_hTexture = m_hDefaultParticle;
			}
		}
		else if( !Q_stricmp( token, "offset" ))
		{
			if( !ParseRandomVector( pfile, info->offset ))
			{
				ALERT( at_error, "hit EOF while parsing 'offset'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "velocity" ))
		{
			if( !ParseRandomVector( pfile, info->velocity ))
			{
				ALERT( at_error, "hit EOF while parsing 'velocity'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "accel" ))
		{
			if( !ParseRandomVector( pfile, info->accel ))
			{
				ALERT( at_error, "hit EOF while parsing 'accel'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "color" ))
		{
			if( !ParseRandomVector( pfile, info->color ))
			{
				ALERT( at_error, "hit EOF while parsing 'color'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "colorVelocity" ))
		{
			if( !ParseRandomVector( pfile, info->colorVel ))
			{
				ALERT( at_error, "hit EOF while parsing 'colorVelocity'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "alpha" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'alpha'\n" );
				break;
			}

			info->alpha = RandomRange( token );
		}
		else if( !Q_stricmp( token, "alphaVelocity" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'alphaVelocity'\n" );
				break;
			}

			info->alphaVel = RandomRange( token );
		}
		else if( !Q_stricmp( token, "radius" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'radius'\n" );
				break;
			}

			info->radius = RandomRange( token );
		}
		else if( !Q_stricmp( token, "radiusVelocity" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'radiusVelocity'\n" );
				break;
			}

			info->radiusVel = RandomRange( token );
		}
		else if( !Q_stricmp( token, "length" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'length'\n" );
				break;
			}

			info->length = RandomRange( token );
		}
		else if( !Q_stricmp( token, "lengthVelocity" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'lengthVelocity'\n" );
				break;
			}

			info->lengthVel = RandomRange( token );
		}
		else if( !Q_stricmp( token, "rotation" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'rotation'\n" );
				break;
			}

			info->rotation = RandomRange( token );
		}
		else if( !Q_stricmp( token, "rotationVelocity" ) )
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'rotationVelocity'\n" );
				break;
			}

			info->rotationVel = RandomRange( token );
		}
		else if( !Q_stricmp( token, "bounceFactor" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'bounceFactor'\n" );
				break;
			}

			info->bounce = RandomRange( token );
		}
		else if( !Q_stricmp( token, "frame" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'frame'\n" );
				break;
			}

			info->frame = RandomRange( token );
		}
		else if( !Q_stricmp( token, "count" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'count'\n" );
				break;
			}

			info->count = RandomRange( token );
		}
		else if( !Q_stricmp( token, "flags" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'flags'\n" );
				break;
			}

			info->flags = ParseParticleFlags( token );
		}
		else if( !Q_stricmp( token, "useNormal" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'useNormal'\n" );
				break;
			}

			if( !Q_stricmp( token, "ignore" ))
				info->normal = NORMAL_IGNORE;
			else if( !Q_stricmp( token, "ofs" ))
				info->normal = NORMAL_OFFSET;
			else if( !Q_stricmp( token, "dir" ))
				info->normal = NORMAL_DIRECTION;
			else if( !Q_stricmp( token, "ofs+dir" ))
				info->normal = NORMAL_OFS_DIR;				
			else if( !Q_stricmp( token, "dir+ofs" ))
				info->normal = NORMAL_OFS_DIR;
			else ALERT( at_warning, "Unknown 'useNormal' key '%s'\n", token );
		}
		else ALERT( at_warning, "Unknown effects token %s\n", token );
	}

	return true;
}

void CQuakePartSystem :: FreeParticle( CQuakePart *pCur )
{
	if( pCur->EntIndex > 0 )
		ParticleCountPerEnt[pCur->EntIndex]--;

	partcounter--;

	pCur->pNext = m_pFreeParticles;
	m_pFreeParticles = pCur;
}

CQuakePart *CQuakePartSystem :: AllocParticle( void )
{
	CQuakePart *pCur;

	if( !m_pFreeParticles )
	{
		ConPrintf( "Overflow %d particles\n", MAX_PARTICLES );
		return NULL;
	}

	pCur = m_pFreeParticles;
	m_pFreeParticles = pCur->pNext;
	pCur->pNext = m_pActiveParticles;
	m_pActiveParticles = pCur;

	return pCur;
}
	
void CQuakePartSystem :: Update( void )
{
	if( IsBuildingCubemaps() || (RI->params & RP_SHADOWPASS) || (RI->params & RP_MIRRORVIEW) )
		return;

	if( glState.drawTrans )
		GL_DepthMask( GL_FALSE );
	else
		GL_DepthMask( GL_TRUE );

	pglBindVertexArray( GL_FALSE ); // just in case

	GL_Blend( GL_TRUE );

	if( tr.fogEnabled )
	{
		GL_BindShader( glsl.genericFogUseAlpha );
		pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
	}

	DrawParticles( ParticleArray_Default );
	DrawParticles( ParticleArray_Dustmote );
	DrawParticles( ParticleArray_Sparks );
	DrawParticles( ParticleArray_Smoke );
	DrawParticles( ParticleArray_Waterring );
	DrawParticles( ParticleArray_Sand );
	DrawParticles( ParticleArray_Dirt );
	DrawParticles( ParticleArray_Fireball );
	DrawParticles( ParticleArray_Blood );
	DrawParticles( ParticleArray_Bubbles );
	DrawParticles( ParticleArray_Beamring );
	DrawParticles( ParticleArray_WaterDrop );
	DrawParticles( ParticleArray_SingleDrop );
	DrawParticles( ParticleArray_WaterFall );
	DrawParticles( ParticleArray_Sparkles );

	// draw particles from txt-file (through glbegin-end...)
	CQuakePart *pCur, *pNext;
	CQuakePart *pActive = NULL, *pTail = NULL;
	
//	float gravity = g_fFrametime * tr.movevars->gravity; gEngfuncs.Con_NPrintf( 1, "ft %f\n", g_fFrametime );
	float gravity = tr.movevars->gravity * 0.0075f; // diffusion - the value must be fixed - frametime is jumping

	for( pCur = m_pActiveParticles; pCur != NULL; pCur = pNext )
	{
		// grab next now, so if the particle is freed we still have it
		pNext = pCur->pNext;

		if( !pCur->Evaluate( gravity ) )
		{
			FreeParticle( pCur );
			continue;
		}

		pCur->pNext = NULL;

		if( !pTail )
		{
			pActive = pTail = pCur;
		}
		else
		{
			pTail->pNext = pCur;
			pTail = pCur;
		}
	}

	m_pActiveParticles = pActive;

	r_stats.c_particles_total = partcounter;

	DrawGunSmoke();

	GL_Blend( GL_FALSE );
	if( glState.drawTrans )
		GL_DepthMask( GL_TRUE );

	if( tr.fogEnabled )
		GL_BindShader( NULL );

//	ExplosionOrg = g_vecZero;
}

void CQuakePartSystem::ResetGunSmoke( void )
{
	m_iGunSmokeCount = 0;
	m_iGunSmokeRun = 0;
	m_flGunSmokeHeat = 0.0f;
	m_flGunSmokeLastShot = -100.0f;
	m_flGunSmokeUntil = -1.0f;
	m_flGunSmokeLastSample = -1.0f;
	m_flGunSmokeLastTime = 0.0f;
}

//=============================================================================
// AddGunSmoke: every shot adds heat. The smoke is what drifts off the barrel AFTER the burst:
// a shot never starts it, and a shot while it burns cuts it. It starts in DrawGunSmoke once
// the trigger has been up for GUNSMOKE_PAUSE seconds.
//=============================================================================
void CQuakePartSystem::AddGunSmoke( float damage )
{
	if( !CVAR_TO_BOOL( cl_gunsmoke ) )
		return; // don't accept any more smoke but let existing fade out

	const float now = tr.time;

	if( now - m_flGunSmokeLastShot > GUNSMOKE_COOL_TIME )
		m_flGunSmokeHeat = 0.0f; // a pause: the barrel cooled, a slow tap never builds smoke

	m_flGunSmokeHeat += (damage > 0.0f) ? damage : 1.0f;
	m_flGunSmokeLastShot = now;

	if( now < m_flGunSmokeUntil )
		m_flGunSmokeUntil = now;
}

void CQuakePartSystem::DrawGunSmoke( void )
{
	if( !glState.drawTrans || !RP_NORMALPASS() || !m_hGunSmoke )
		return;

	const float now = tr.time;
	static int CachedWpnID = WEAPON_NONE;
	if( CachedWpnID == WEAPON_NONE )
		CachedWpnID = gHUD.m_Ammo.WeaponID;

	// reset smoke trail if switched weapon
	if( gHUD.m_Ammo.WeaponID != CachedWpnID )
	{
		ResetGunSmoke();
		CachedWpnID = gHUD.m_Ammo.WeaponID;
	}

	// saverestore issues
	if( now < m_flGunSmokeLastTime )
		ResetGunSmoke();

	m_flGunSmokeLastTime = now;

	// drop the points that have fully faded (the array is oldest-first)
	int drop = 0;
	while( drop < m_iGunSmokeCount && (now - m_GunSmoke[drop].born) > GUNSMOKE_LIFE )
		drop++;

	if( drop > 0 )
	{
		const int left = m_iGunSmokeCount - drop;
		for( int i = 0; i < left; i++ )
			m_GunSmoke[i] = m_GunSmoke[i + drop];
		m_iGunSmokeCount = left;
	}

	// start the smoke once the trigger has been released for GUNSMOKE_PAUSE seconds
	if( now >= m_flGunSmokeUntil && m_flGunSmokeHeat >= GUNSMOKE_HEAT && (now - m_flGunSmokeLastShot) >= GUNSMOKE_PAUSE )
	{
		// more heat than the threshold = a longer drift-off
		// the heat is SPENT
		const float over = (m_flGunSmokeHeat - GUNSMOKE_HEAT) / GUNSMOKE_HEAT;
		float burn = GUNSMOKE_BURN_TIME * (1.0f + over * GUNSMOKE_HEAT_MULT);
		if( burn > GUNSMOKE_BURN_MAX )
			burn = GUNSMOKE_BURN_MAX;

		m_flGunSmokeUntil = now + burn;
		m_flGunSmokeHeat = 0.0f;
		m_iGunSmokeRun++; // a fresh ribbon
	}

	// sample the local muzzle while the smoke burns
	if( now < m_flGunSmokeUntil )
	{
		cl_entity_t *vm = gEngfuncs.GetViewModel();

		if( vm && vm->model && (m_flGunSmokeLastSample < 0.0f || now - m_flGunSmokeLastSample > GUNSMOKE_SAMPLE_DT) )
		{
			const Vector muzzle = R_StudioAttachmentOrigin( vm, 0 );

			if( m_iGunSmokeCount < GUNSMOKE_MAX_POINTS )
			{
				GunSmokePoint_t *pt = &m_GunSmoke[m_iGunSmokeCount];
				const GunSmokePoint_t *prev = m_iGunSmokeCount ? (pt - 1) : NULL;

				pt->pos = muzzle;
				pt->born = now;
				pt->wid = RANDOM_FLOAT( 0.40f, 0.65f );
				pt->run = m_iGunSmokeRun;

				if( prev )
				{
					// the curl direction WALKS smoothly instead of jumping to a new random angle
					pt->seed = prev->seed + RANDOM_FLOAT( -0.09f, 0.09f );
					pt->dist = (prev->run == m_iGunSmokeRun) ? (prev->dist + (muzzle - prev->pos).Length()) : 0.0f;
				}
				else
				{
					pt->seed = RANDOM_FLOAT( 0.0f, 1.0f );
					pt->dist = 0.0f;
				}

				m_iGunSmokeCount++;
			}

			m_flGunSmokeLastSample = now;
		}
	}

	if( m_iGunSmokeCount < 2 )
		return;

	// build the curve: every segment is subdivided, and the rise/curl are evaluated at the
	// interpolated age and seed, so the column bends smoothly between samples
	int n = 0;
	bool prevLinked = false; // is point i linked to point i-1?

	for( int i = 0; i < m_iGunSmokeCount && n < GUNSMOKE_DRAW_MAX - GUNSMOKE_SUBDIV; i++ )
	{
		const GunSmokePoint_t &p0 = m_GunSmoke[i];
		const GunSmokePoint_t *p1 = (i + 1 < m_iGunSmokeCount) ? &m_GunSmoke[i + 1] : NULL;
		const bool link = p1 && (p1->run == p0.run) && ((p1->pos - p0.pos).Length() < GUNSMOKE_LINK_DIST);
		const int sub = link ? GUNSMOKE_SUBDIV : 1;

		for( int k = 0; k < sub; k++ )
		{
			const float t = (float)k / (float)sub;

			float age = now - p0.born;
			Vector base = p0.pos;
			float seed = p0.seed;
			float wid = p0.wid;
			float dist = p0.dist;

			if( link )
			{
				age += ((now - p1->born) - age) * t;
				base = p0.pos + (p1->pos - p0.pos) * t;
				seed = p0.seed + (p1->seed - p0.seed) * t;
				wid = p0.wid + (p1->wid - p0.wid) * t;
				dist = p0.dist + (p1->dist - p0.dist) * t;
			}

			if( age < 0.0f )
				age = 0.0f;

			const float f = Q_min( age / GUNSMOKE_LIFE, 1.0f );
			const float fin = (age < 0.12f) ? (age / 0.12f) : 1.0f; // a soft birth: nothing pops in

			Vector q = base;
			q.z += GUNSMOKE_RISE * age;

			const float ang = seed * 6.2831853f;
			const float curl = GUNSMOKE_WOBBLE * f * f * f; // young = straight, old = curling
			q.x += cos( ang ) * curl;
			q.y += sin( ang ) * curl;

			GunSmokeVertex_t &vert = s_gunSmokeVerts[n++];
			vert.p = q;
			vert.w = wid * (0.35f + 1.80f * f) * fin;	// narrow at the muzzle, wider with age
			vert.a = 0.31f * (1.0f - f) * fin;		// ~80/255 at its brightest
			vert.v = dist * GUNSMOKE_V_PER_UNIT;
			vert.brk = (i > 0 && k == 0 && !prevLinked);	// new strip if previous point is not linked to this one
		}

		prevLinked = link;
	}

	if( n < 2 )
		return;

	// depth mask is off and blend is on already
	GL_Bind( GL_TEXTURE0, m_hGunSmoke );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	GL_AlphaTest( GL_FALSE );
	GL_Cull( GL_NONE );

#if GUNSMOKE_ADDITIVE
	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE );
	const float tint = 1.0f;
#else
	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	// grey smoke, lit by the world around the newest point
	Vector light;
	gEngfuncs.pTriAPI->LightAtPoint( s_gunSmokeVerts[n - 1].p, light );
	const float lit = bound( 0.25f, (light.x + light.y + light.z) / (3.0f * 255.0f), 1.0f );
	const float tint = 0.8f * lit;
#endif

	// the ribbon: the side vector is perpendicular to the local tangent and the direction to the
	// eye, so the width stays constant whichever way the barrel travels
	bool open = false;

	for( int j = 0; j < n; j++ )
	{
		const GunSmokeVertex_t &vert = s_gunSmokeVerts[j];

		if( vert.brk && open )
		{
			pglEnd();
			open = false;
		}

		if( !open )
		{
			pglBegin( GL_TRIANGLE_STRIP );
			open = true;
		}

		Vector dir( 0, 0, 0 );
		if( j > 0 && !vert.brk )
			dir = vert.p - s_gunSmokeVerts[j - 1].p;
		if( j + 1 < n && !s_gunSmokeVerts[j + 1].brk )
			dir = dir + (s_gunSmokeVerts[j + 1].p - vert.p);
		if( dir.Length() < 0.001f )
			dir = Vector( 0, 0, 1 );
		dir = dir.Normalize();

		Vector side = CrossProduct( dir, RI->vieworg - vert.p );
		if( side.Length() < 0.001f )
			side = RI->vright;
		side = side.Normalize();

		const float w = vert.w;
#if GUNSMOKE_ADDITIVE
		GL_Color4f( vert.a, vert.a, vert.a, 1.0f );
#else
		GL_Color4f( tint, tint, tint, Q_min( vert.a * GUNSMOKE_ALPHA_SCALE, 1.0f ) );
#endif
		pglTexCoord2f( 0.0f, vert.v );
		pglVertex3f( vert.p.x + side.x * w, vert.p.y + side.y * w, vert.p.z + side.z * w );
		pglTexCoord2f( 1.0f, vert.v );
		pglVertex3f( vert.p.x - side.x * w, vert.p.y - side.y * w, vert.p.z - side.z * w );
	}

	if( open )
		pglEnd();

	GL_Cull( GL_FRONT );
}

bool CQuakePartSystem :: AddParticle( CQuakePart *src, int texture, int flags )
{
	if( !src )
		return false;

	// diffusion - particle is too far, don't allow it
	if( src->m_flDistance > 0 )
	{
		if( (RI->vieworg - src->m_vecOrigin).Length() > src->m_flDistance )
			return false;
	}

	if( FBitSet( flags, FPART_NOTWATER ) && POINT_CONTENTS( src->m_vecOrigin ) == CONTENTS_WATER )
		return false;

	if( partcounter >= MAX_PARTICLES )
	{
		ConPrintf( "AddParticle: overflow\n" );
		return false;
	}

	int entIndex = src->EntIndex;
	if( entIndex < 0 || entIndex >= (int)(sizeof( ParticleCountPerEnt ) / sizeof( ParticleCountPerEnt[0] )) )
		entIndex = 0;

	if( entIndex > 0 && ParticleCountPerEnt[entIndex] >= MAX_PARTICLES_PER_ENTITY )
		return false;

	CQuakePart *dst = NULL;
	
	switch( src->ParticleType )
	{
	default:	
	case TYPE_DEFAULT:
		dst = ParticleArray_Default.Allocate();
		break;
	case TYPE_SMOKE:
		dst = ParticleArray_Smoke.Allocate();
		break;
	case TYPE_DUSTMOTE:
		dst = ParticleArray_Dustmote.Allocate();
		break;
	case TYPE_SPARKS:
		dst = ParticleArray_Sparks.Allocate();
		break;
	case TYPE_WATERRING:
		dst = ParticleArray_Waterring.Allocate();
		break;
	case TYPE_SAND:
		dst = ParticleArray_Sand.Allocate();
		break;
	case TYPE_DIRT:
		dst = ParticleArray_Dirt.Allocate();
		break;
	case TYPE_FIREBALL:
		dst = ParticleArray_Fireball.Allocate();
		break;
	case TYPE_BLOOD:
		dst = ParticleArray_Blood.Allocate();
		break;
	case TYPE_BUBBLES:
		dst = ParticleArray_Bubbles.Allocate();
		break;
	case TYPE_BEAMRING:
		dst = ParticleArray_Beamring.Allocate();
		break;
	case TYPE_WATERDROP:
		dst = ParticleArray_WaterDrop.Allocate();
		break;
	case TYPE_SINGLEDROP:
		dst = ParticleArray_SingleDrop.Allocate();
		break;
	case TYPE_WATERFALL:
		dst = ParticleArray_WaterFall.Allocate();
		break;
	case TYPE_SPARKLES:
		dst = ParticleArray_Sparkles.Allocate();
		break;
	case TYPE_CUSTOM:
		dst = AllocParticle();
		break;
	}

	if( !dst )
		return false;

	CQuakePart *savedNext = dst->pNext;
	*dst = *src;
	dst->pNext = savedNext;

	dst->m_hTexture = texture ? texture : m_hDefaultParticle;
	dst->m_flTime = tr.time;
	dst->m_iFlags = flags;
	dst->m_vecLastOrg = dst->m_vecOrigin;
	dst->newLight = 0.0f;
	dst->EntIndex = entIndex;
	dst->m_flLastThink = tr.time;

	if( FBitSet( flags, FPART_FADEIN ) && dst->m_flAlphaVelocity >= 0.0f )
		ClearBits( dst->m_iFlags, FPART_FADEIN );

	partcounter++;
	if( entIndex > 0 )
		ParticleCountPerEnt[entIndex]++;

	return true;
}

void CQuakePartSystem :: ExplosionParticles( int EntIndex, const Vector &pos )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();
	int flags;
	int i;

	flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION|FPART_NOTWATER);

	for( i = 0; i < 128; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_LONG( -16, 16 );
		src.m_vecOrigin.y = pos.y + RANDOM_LONG( -16, 16 );
		src.m_vecOrigin.z = pos.z + RANDOM_LONG( -16, 16 );
		src.m_vecVelocity.x = RANDOM_LONG( -256, 256 );
		src.m_vecVelocity.y = RANDOM_LONG( -256, 256 );
		src.m_vecVelocity.z = RANDOM_LONG( -256, 256 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -60 + RANDOM_FLOAT( -30, 30 );
		src.m_vecColor = Vector( 1, 1, 1 );
		src.m_vecColorVelocity = Vector( 0, 0, 0 );
		src.m_flAlphaVelocity = -1.0;
		src.m_flRadius = 0.5 + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flLength = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flLengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flBounceFactor = 0.2;
		src.m_flDistance = 1500;
		src.ParticleType = TYPE_SPARKS;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSparks, flags ))
			break;
	}

	// smoke
	src = InitializeParticle();

	flags = (FPART_VERTEXLIGHT | FPART_NOTWATER);

	for( i = 0; i < 5; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.m_vecColorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.m_flRadius = 30 + RANDOM_FLOAT( -15, 15 );
		src.m_flRadiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.m_flRotation = RANDOM_LONG( 0, 360 );
		src.ParticleType = TYPE_SMOKE;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSmoke, flags ))
			break;
	}

	// smoke ring
	src = InitializeParticle();

	flags = (FPART_VERTEXLIGHT | FPART_NOTWATER);

	for( i = 0; i < 50; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -75, 75 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -75, 75 );
		src.m_vecVelocity.z = 0;
		src.m_vecAccel = -0.2 * src.m_vecVelocity;
		src.m_vecColor = Vector( 0.2, 0.2, 0.2 );
		src.m_vecColorVelocity = Vector( 0, 0, 0 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.m_flRadius = 20 + RANDOM_FLOAT( -7, 7 );
		src.m_flRotation = RANDOM_LONG( 0, 360 );
		src.m_flRotationVelocity = RANDOM_LONG( -15, 15);
		src.m_flDistance = 1500;
		src.ParticleType = TYPE_SMOKE;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSmoke, flags ) )
			break;
	}

	// explosion fireballs
	src = InitializeParticle();

	flags = FPART_NOTWATER;

	for( i = 0; i < 30; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -7.5f, 7.5f );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -7.5f, 7.5 );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( 0, 30 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -25, 25 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -25, 25 );
		src.m_vecVelocity.z = RANDOM_FLOAT( 30, 60 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = RANDOM_FLOAT( -1.25, -0.85 );
		src.m_flRadius = 15 + RANDOM_FLOAT( 0, 25 );
		src.m_flRadiusVelocity = RANDOM_FLOAT( 1, 4 );
		src.m_flRotation = RANDOM_LONG( 0, 359 );
		src.m_flRotationVelocity = RANDOM_LONG( -15, 15 );
		src.ParticleType = TYPE_FIREBALL;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hExplosion, flags ) )
			break;
	}
}

void CQuakePartSystem :: SparkParticles( int EntIndex, const Vector &org, const Vector &dir )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
	{
		src.m_vecOrigin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.m_flAlphaVelocity = -8.0;
		src.m_flRadius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flLength = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flLengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flBounceFactor = 0.2;
		src.ParticleType = TYPE_SPARKS;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSparks, flags ))
			break;
	}
}

/*
=================
CL_RicochetSparks
=================
*/
void CQuakePartSystem :: RicochetSparks( int EntIndex, const Vector &org, float scale )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
	{
		src.m_vecOrigin.x = org[0] + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -60, 60 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.m_flAlphaVelocity = -8.0;
		src.m_flRadius = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flLength = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flLengthVelocity = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flBounceFactor = 0.2;
		src.ParticleType = TYPE_SPARKS;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSparks, flags ))
			break;
	}
}

void CQuakePartSystem :: SmokeParticles( int EntIndex, const Vector &pos, int count, float speed, float scale, float pos_rand )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();

	// smoke
	int flags = FPART_VERTEXLIGHT;

	if( speed == 0 )
		speed = 0.15;

	if( scale == 0 )
		scale = 30;

	for( int i = 0; i < count; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -pos_rand, pos_rand );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -pos_rand, pos_rand );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -pos_rand, pos_rand );
		src.m_vecVelocity.x = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.m_vecColor = Vector( 0, 0, 0 );
		src.m_vecColorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = -speed;
		src.m_flRadius = scale + RANDOM_FLOAT( -scale/2, scale/2 );
		src.m_flRadiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.m_flRotation = RANDOM_LONG( 0, 360 );
		src.ParticleType = TYPE_SMOKE;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSmoke, flags ))
			break;
	}
}

void CQuakePartSystem::Smoke( int EntIndex, int Particle, const Vector &pos, const Vector &vel, int count, float speed, float scale, float posRand, float velRand, Vector Color, float Distance, float Alpha )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();

	// smoke
	int flags = FPART_VERTEXLIGHT;

	switch( Particle )
	{
	default:
	case 0:
		Particle = m_hSmoke;
		break;
	case 1:
		Particle = m_hSand;
		break;
	case 2:
		Particle = m_hDirt;
		break;
	}

	for( int i = 0; i < count; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecVelocity.x = vel.x + RANDOM_FLOAT( -velRand, velRand );
		src.m_vecVelocity.y = vel.y + RANDOM_FLOAT( -velRand, velRand );
		src.m_vecVelocity.z = vel.z + RANDOM_FLOAT( -velRand, velRand );
		src.m_vecColor = Color;
		src.m_flAlpha = Alpha;
		src.m_flAlphaVelocity = -speed;
		if( Particle == m_hDirt )
			src.m_flAlphaVelocity *= 0.5f;
		src.m_flRadius = scale;
		src.m_flRadiusVelocity = 7.0f;
		src.m_flRotation = RANDOM_LONG( 0, 359 );
		src.m_flRotationVelocity = RANDOM_LONG( -15, 15 );
		src.m_flDistance = Distance;
		src.EntIndex = EntIndex;
		if( Particle == m_hSmoke )
			src.ParticleType = TYPE_SMOKE;
		else if( Particle == m_hSand )
			src.ParticleType = TYPE_SAND;
		else if( Particle == m_hDirt )
			src.ParticleType = TYPE_DIRT;

		if( !AddParticle( &src, Particle, flags ) )
			break;
	}
}

void CQuakePartSystem::SmokeVolume( int EntIndex, int Particle, const Vector &pos, const Vector &PushVelocity, const Vector &PushVelocityRand, const Vector &Color, float Scale, float Alpha, int Distance )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();

	int flags = FPART_FADEIN;
	float posRand = 20.0f;
	float ScaleRand = 0.0f;

	switch( Particle )
	{
	default:
	case 0:
		Particle = m_hSmoke;
		flags |= FPART_VERTEXLIGHT;
		break;
	case 1:
		Particle = m_hDustMote;
		flags |= FPART_VERTEXLIGHT;
		break;
	case 2: // fullbright
		Particle = m_hSmoke;
		break;
	case 3: // fullbright
		Particle = m_hDustMote;
		break;
	}

	if( !Color.IsNull() )
		src.m_vecColor = Color;

	if( Particle == m_hSmoke )
	{
		if( !Scale )
			Scale = RANDOM_FLOAT( 30, 40 );
		else
		{
			ScaleRand = Scale * 0.1;
			Scale = RANDOM_FLOAT( Scale - ScaleRand, Scale + ScaleRand );
		}
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecVelocity = PushVelocity;
		if( PushVelocityRand.x != 0.0f )
			src.m_vecVelocity.x += RANDOM_FLOAT( -PushVelocityRand.x, PushVelocityRand.x );
		if( PushVelocityRand.y != 0.0f )
			src.m_vecVelocity.y += RANDOM_FLOAT( -PushVelocityRand.y, PushVelocityRand.y );
		if( PushVelocityRand.z != 0.0f )
			src.m_vecVelocity.z += RANDOM_FLOAT( -PushVelocityRand.z, PushVelocityRand.z );
		src.m_flAlpha = Alpha;
		src.m_flAlphaVelocity = -RANDOM_FLOAT( 0.3f, 0.666f ) * Alpha;
		src.m_flRadius = Scale;
		src.m_flRotation = RANDOM_LONG( 1, 360 );
		src.m_flDistance = Distance;
		src.ParticleType = TYPE_SMOKE;
		src.EntIndex = EntIndex;

		AddParticle( &src, Particle, flags );
	}
	else if( Particle == m_hDustMote )
	{
		posRand = 5.0f;

		flags |= FPART_DISTANCESCALE;

		if( !Scale )
			Scale = RANDOM_FLOAT( 0.15, 0.40 );
		else
		{
			ScaleRand = Scale * 0.1;
			Scale = RANDOM_FLOAT( Scale - ScaleRand, Scale + ScaleRand );
		}
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -posRand, posRand );
		src.m_vecVelocity.x = PushVelocity.x;
		src.m_vecVelocity.y = PushVelocity.y;
		src.m_vecVelocity.z = PushVelocity.z + RANDOM_FLOAT(-5, 5);
		if( PushVelocityRand.x != 0.0f )
			src.m_vecVelocity.x += RANDOM_FLOAT( -PushVelocityRand.x, PushVelocityRand.x );
		if( PushVelocityRand.y != 0.0f )
			src.m_vecVelocity.y += RANDOM_FLOAT( -PushVelocityRand.y, PushVelocityRand.y );
		if( PushVelocityRand.z != 0.0f )
			src.m_vecVelocity.z += RANDOM_FLOAT( -PushVelocityRand.z, PushVelocityRand.z );
		src.m_flAlpha = Alpha;
		src.m_flAlphaVelocity = -RANDOM_FLOAT( 0.4f, 0.6f ) * Alpha;
		src.m_flRadius = Scale;
		src.m_flMinScale = Scale * 0.1;
		src.m_flMaxScale = Scale * 2.0;
		src.m_flRotation = RANDOM_LONG( 1, 360 );
		src.m_flDistance = Distance;
		src.ParticleType = TYPE_DUSTMOTE;
		src.EntIndex = EntIndex;

		AddParticle( &src, Particle, flags );
	}
}

void CQuakePartSystem :: GunSmoke( int EntIndex, const Vector &pos, Vector vel, int WeaponID )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();

	// defaults
	int count = 2;

	if( !vel )
		vel = g_vecZero;

	vel *= 0.25;

	switch( WeaponID )
	{
	case WEAPON_SNIPER:
	case WEAPON_GAUSS:
		count = 3;
	break;
	case WEAPON_SHOTGUN:
	case WEAPON_SHOTGUN_XM:
		count = 5;
	break;
	}

	// smoke
	int flags = FPART_VERTEXLIGHT | FPART_NOTWATER;

	for( int i = 0; i < count; i++ )
	{
		switch( WeaponID )
		{
		case WEAPON_SNIPER:
		case WEAPON_GAUSS:
			src.m_flRadius = RANDOM_FLOAT( 20, 30 );
		break;
		case WEAPON_SHOTGUN:
		case WEAPON_SHOTGUN_XM:
			src.m_flRadius = RANDOM_FLOAT( 5, 10 );
		break;
		default:
			src.m_flRadius = RANDOM_FLOAT( 3.5f, 6.5f ); // scale
		break;
		}
		
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -0.1f, 0.1f );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -0.1f, 0.1f );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -0.1f, 0.1f );
		src.m_vecVelocity.x = vel.x * RANDOM_FLOAT( 0.85f, 1.15f ) + RANDOM_FLOAT( -5.1f, 5.1f );
		src.m_vecVelocity.y = vel.y * RANDOM_FLOAT( 0.85f, 1.15f ) + RANDOM_FLOAT( -5.1f, 5.1f );
		src.m_vecVelocity.z = vel.z * RANDOM_FLOAT( 0.85f, 1.15f ) + RANDOM_FLOAT( -5.1f, 5.1f );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = RANDOM_FLOAT( -0.2f, -0.4f );
		src.m_flDistance = 1000;
		src.m_flRadiusVelocity = 2.0f + RANDOM_FLOAT( -0.5, 0.5 );
		src.m_flRotation = RANDOM_LONG( 0, 360 );
		src.m_flRotationVelocity = RANDOM_LONG(-50,50);
		src.ParticleType = TYPE_SMOKE;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSmoke, flags ))
			break;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CQuakePartSystem :: BulletParticles( int EntIndex, const Vector &org, const Vector &dir )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();
	int cnt, count, i;

	count = RANDOM_LONG( 4, 10 );
	cnt = POINT_CONTENTS( (float *)&org );

	if( cnt == CONTENTS_WATER )
		return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION|FPART_ADDITIVE);

	for( i = 0; i < count; i++ )
	{
		src.m_vecOrigin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.m_flAlphaVelocity = -3.0;
		src.m_flRadius = 0.8 + RANDOM_FLOAT( -0.3, 0.3 );
		src.m_flLength = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flLengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flBounceFactor = 0.2;
		src.m_flDistance = 2000;
		src.ParticleType = TYPE_SPARKS;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSparks, flags ))
			break;
	}

	// smoke
	src = InitializeParticle();

	flags = FPART_VERTEXLIGHT;

	for( i = 0; i < 3; i++ )
	{
		src.m_vecOrigin.x = org[0] + dir[0] * 5 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + dir[1] * 5 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + dir[2] * 5 + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -2.5, 2.5 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -2.5, 2.5 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -2.5, 2.5 ) + (25 + RANDOM_FLOAT( -5, 5 ));
		src.m_vecColor = Vector( 0.4, 0.4, 0.4 );
		src.m_vecColorVelocity = Vector( 0.2, 0.2, 0.2 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = -(0.4 + RANDOM_FLOAT( 0, 0.2 ));
		src.m_flRadius = 3 + RANDOM_FLOAT( -1.5, 1.5 );
		src.m_flRadiusVelocity = 5 + RANDOM_FLOAT( -2.5, 2.5 );
		src.m_flRotation = RANDOM_LONG( 0, 360 );
		src.m_flDistance = 1500;
		src.ParticleType = TYPE_SMOKE;
		src.EntIndex = EntIndex;

		if( !AddParticle( &src, m_hSmoke, flags ))
			break;
	}
}

void CQuakePartSystem::WaterRingParticle( int EntIndex, const Vector &pos, float size )
{
	if( !g_fRenderInitialized )
		return;

	CQuakePart src = InitializeParticle();

	// water ring
	int flags = FPART_VERTEXLIGHT | FPART_FLOATING_ORIENTED | FPART_AFLOAT;

	src.m_vecOrigin = pos;
	src.m_flAlphaVelocity = -0.75;
	src.m_flRadius = RANDOM_FLOAT(10,20) * size;
	src.m_flRadiusVelocity = (25 + RANDOM_FLOAT( 0, 10 )) * size;
	src.m_flRotation = RANDOM_LONG( 0, 359 );
	src.m_flRotationVelocity = RANDOM_FLOAT(-25.0,25.0);
	src.ParticleType = TYPE_WATERRING;
	src.EntIndex = EntIndex;

	AddParticle( &src, m_hWaterRing, flags );
}

void CQuakePartSystem::BloodParticle( int EntIndex, const Vector &pos, float Scale, Vector Color, Vector Direction )
{
	if( !g_fRenderInitialized )
		return;

	CQuakePart src = InitializeParticle();

	int flags = FPART_FLOATING_ORIENTED | FPART_VERTEXLIGHT;

	src.m_vecOrigin = pos;
	src.m_vecVelocity = Direction * 100;
	src.m_vecVelocity.z = RANDOM_FLOAT( 40 + Scale * 10, 80 + Scale * 10 );
	src.m_vecAccel = Vector( 0, 0, -30 );
	src.m_vecColor = Color / 255;
	src.m_flAlphaVelocity = -1.0;
	src.m_flRadius = 0.01;
	src.m_flRadiusVelocity = 20 + Scale * 5;
	src.m_flRotation = RANDOM_LONG( 0, 359 );
	src.m_flRotationVelocity = RANDOM_FLOAT( -100.0, 100.0 );
	src.ParticleType = TYPE_BLOOD;
	src.EntIndex = EntIndex;

	AddParticle( &src, m_hBlood, flags );

	src = InitializeParticle();

	flags = FPART_VERTEXLIGHT | FPART_DISTANCESCALE;

	src.m_vecOrigin = pos;
	src.m_vecColor = Color / 255;
	src.m_flAlphaVelocity = -2.0;
	src.m_flRadius = Scale + RANDOM_FLOAT( -Scale * 0.1f, Scale * 0.1f );
	src.m_flRotation = RANDOM_LONG( 0, 359 );
	src.ParticleType = TYPE_CUSTOM;
	src.EntIndex = EntIndex;
	src.m_flMinScale = Scale * 0.01f;
	src.m_flMaxScale = Scale * 1.2f;

	AddParticle( &src, m_hBloodSplat, flags );
}

void CQuakePartSystem::Bubble( int EntIndex, const Vector &pos, float Speed, int Distance, float DieTime, float SinSpeed, Vector CustomVelocity, int CustomFlags )
{
	if( !g_fRenderInitialized )
		return;
	
	CQuakePart src = InitializeParticle();

	int flags = FPART_SINEWAVE | FPART_OPAQUE;
	if( CustomFlags ) flags |= CustomFlags;

	float Scale = 7.5f / RANDOM_FLOAT( 2.0f, 5.0f );

	src.m_vecOrigin = pos;
	if( CustomVelocity != g_vecZero )
		src.m_vecVelocity = CustomVelocity;
	else
		src.m_vecVelocity.z = Speed; // default half-life behavior
	src.m_flRadius = Scale;
	src.m_flDistance = Distance;
	src.ParticleType = TYPE_BUBBLES;
	src.EntIndex = EntIndex;
	src.m_flDieTime = DieTime;
	src.m_flSinSpeed = SinSpeed * RANDOM_FLOAT(0.05f, 0.15f);

	AddParticle( &src, m_hBubble, flags );
}

void CQuakePartSystem::Beamring( int EntIndex, const Vector &start, const Vector &end )
{
	if( !g_fRenderInitialized )
		return;

	CQuakePart src = InitializeParticle();

	int flags = FPART_TWOSIDE;

	// get direction and distance
	Vector ViewVec;
	Vector forward;
	VectorAngles( end - start, ViewVec );
	gEngfuncs.pfnAngleVectors( ViewVec, forward, NULL, NULL );
	float dist = (start - end).Length();

	float Scale = 10;

	src.m_flAlphaVelocity = -0.75f;
	src.m_flRadius = Scale;
	src.m_flRadiusVelocity = 50;
	src.ParticleType = TYPE_BEAMRING;
	src.EntIndex = EntIndex;
	src.m_vecView = end - start;

	// add the rest of the rings
	// draw starting from end point
	float dist_orig = dist;
	while( dist > 0.1f )
	{
		src.m_vecOrigin = start + forward * dist;
		dist *= 0.85f;
		// some empirical trickery...
		src.m_flRadius = RemapVal( dist, 0, dist_orig, 0, Scale );
		src.m_flRadius = bound( 1.0, src.m_flRadius, Scale );
		src.m_flRotation++;
		src.m_flRotationVelocity += 100;
		src.m_flRadiusVelocity *= 0.8f;
		if( src.m_flAlphaVelocity > -2.0f )
			src.m_flAlphaVelocity *= 1.025f;

		AddParticle( &src, m_hBeamRing, flags );
	}
}

void CQuakePartSystem::WaterDripLine( const Vector &start, const Vector &end, int Distance )
{
	if( !g_fRenderInitialized )
		return;

	// get direction and distance
	Vector ViewVec;
	Vector forward;
	VectorAngles( end - start, ViewVec );
	gEngfuncs.pfnAngleVectors( ViewVec, forward, NULL, NULL );
	float dist = (start - end).Length();

	CQuakePart src = InitializeParticle();
	src.ParticleType = TYPE_WATERDROP;
	src.m_vecOrigin = start + forward * RANDOM_FLOAT( 0.0f, dist );
	src.m_vecAccel = Vector( 0, 0, -tr.movevars->gravity );
	src.m_flDistance = Distance;
	int flags = FPART_NOTINSOLID | FPART_NOTWATER | FPART_VERTEXLIGHT_INSTANT;

	AddParticle( &src, m_hRainDrop, flags );
}

void CQuakePartSystem::WaterDrop( int EntIndex, const Vector &pos )
{
	if( !g_fRenderInitialized )
		return;

	CQuakePart src = InitializeParticle();
	src.ParticleType = TYPE_WATERDROP;
	src.m_vecOrigin = pos;
	src.m_vecAccel = Vector( 0, 0, -tr.movevars->gravity );
	src.EntIndex = EntIndex;
	int flags = FPART_NOTINSOLID | FPART_NOTWATER | FPART_VERTEXLIGHT_INSTANT;

	AddParticle( &src, m_hRainDrop, flags );
}

//=============================================================================
// WaterLandingDroplet: splash small drips around when a single drip lands
//=============================================================================
void CQuakePartSystem::WaterLandingDroplet( int EntIndex, const Vector &pos )
{
	if( !g_fRenderInitialized )
		return;

	int count = RANDOM_LONG( 6, 10 );

	CQuakePart src;

	while( count > 0 )
	{
		src = InitializeParticle();
		src.ParticleType = TYPE_SINGLEDROP;
		src.m_vecOrigin = pos;
		src.m_vecVelocity = Vector( RANDOM_LONG( -15, 15 ), RANDOM_LONG( -15, 15 ), RANDOM_LONG( 15, 25 ) );
		src.m_vecAccel = Vector( 0, 0, RANDOM_FLOAT(-10.0f, -7.0f) );
		src.m_flAlphaVelocity = RANDOM_FLOAT( -4.0f, -3.0f );
		src.m_flRadius = RANDOM_FLOAT( 0.05f, 0.30f );
		src.m_flLength = 0.1f;
		src.m_flLengthVelocity = RANDOM_LONG( 20, 35 );
		src.EntIndex = EntIndex;
		src.m_flDistance = 666;
		int flags = FPART_NOTWATER | FPART_STRETCH | FPART_VERTEXLIGHT_INSTANT;

		if( !AddParticle( &src, m_hSingleDrop, flags ) )
			break;

		count--;
	}
}

//=============================================================================
// Waterfall: func_smokevolume uses this
// creates a splash particle which falls down and grows in scale
// can be initialized with velocity
//=============================================================================
void CQuakePartSystem::Waterfall( int EntIndex, const Vector &pos, const Vector &PushVelocity, const Vector &PushVelocityRand, const Vector &Color, float Scale, float Alpha, int Distance )
{
	if( !g_fRenderInitialized )
		return;

	CQuakePart src = InitializeParticle();

	int flags = (FPART_FADEIN | FPART_NOTINSOLID | FPART_NOTWATER | FPART_STRETCH);
	float posRand = 20.0f;
	float ScaleRand = 0.0f;

	if( !Color.IsNull() )
		src.m_vecColor = Color;

	if( !Scale )
		Scale = RANDOM_FLOAT( 30, 40 );
	else
	{
		ScaleRand = Scale * 0.1;
		Scale = RANDOM_FLOAT( Scale - ScaleRand, Scale + ScaleRand );
	}
	src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -posRand, posRand );
	src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -posRand, posRand );
	src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -posRand, posRand );
	src.m_vecVelocity = PushVelocity; // special case for TYPE_WATERFALL - x and y will be scaled down to 0 during drawing
	if( PushVelocityRand.x != 0.0f )
		src.m_vecVelocity.x += RANDOM_FLOAT( -PushVelocityRand.x, PushVelocityRand.x );
	if( PushVelocityRand.y != 0.0f )
		src.m_vecVelocity.y += RANDOM_FLOAT( -PushVelocityRand.y, PushVelocityRand.y );
	if( PushVelocityRand.z != 0.0f )
		src.m_vecVelocity.z += RANDOM_FLOAT( -PushVelocityRand.z, PushVelocityRand.z );
	src.m_vecAccel = Vector( 0, 0, -tr.movevars->gravity * 0.1f );
	src.m_flAlpha = Alpha;
	src.m_flAlphaVelocity = -0.5 * Alpha;
	src.m_flRadius = Scale;
	src.m_flRadiusVelocity = 50;
	src.m_flRadiusMax = 150;
	src.m_flLength = Scale;
	src.m_flLengthVelocity = 300;
	src.m_flDistance = Distance;
	src.ParticleType = TYPE_WATERFALL;
	src.EntIndex = EntIndex;

	AddParticle( &src, m_hWaterFall, flags );
}

//=============================================================================
// Sparkle: used on alien robo projectiles
//=============================================================================
void CQuakePartSystem::Sparkle( int EntIndex, const Vector &pos, const Vector &color )
{
	if( !g_fRenderInitialized )
		return;

	CQuakePart src = InitializeParticle();
	src.ParticleType = TYPE_SPARKLES;
	src.m_vecOrigin = pos + Vector( RANDOM_LONG( -5, 5 ), RANDOM_LONG( -5, 5 ), RANDOM_LONG( -5, 5 ) );
	src.m_vecVelocity = Vector( RANDOM_LONG( -10, 10 ), RANDOM_LONG( -10, 10 ), RANDOM_LONG( -10, 10 ) );
	src.m_flAlphaVelocity = -3.0f;
	src.m_flRadius = RANDOM_FLOAT( 0.05f, 0.15f );
	src.m_flRadiusVelocity = 10.0f;
	src.m_flLength = 0.1f;
	src.m_flLengthVelocity = 15.0f;
	src.EntIndex = EntIndex;
	src.m_flDistance = 1000;
	src.m_vecColor = color * RANDOM_FLOAT( 0.8f, 1.0f );
	int flags = FPART_NOTWATER | FPART_STRETCH | FPART_ADDITIVE;

	AddParticle( &src, tr.whiteTexture, flags );
}
