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

#define MAX_CABLE_VERTS 65536
Vector CableVertexesArray[MAX_CABLE_VERTS];
byte CableColorArray[MAX_CABLE_VERTS][4];
int cable_numverts;

void HUD_DrawNormalTriangles( void )
{
	gEngfuncs.pTriAPI->Begin(TRI_POLYGON);
	gEngfuncs.pTriAPI->End();

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
		if( !Mod_CheckBoxVisible( absmin, absmax ) )
			continue;
		if( R_CullModel( RI->currententity, absmin, absmax ) )
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

	// prepare the cable arrays
	cable_numverts = 0;

	for( int i = 0; i < tr.num_trans_entities; i++ )
	{
		RI->currententity = tr.trans_entities[i];
		RI->currentmodel = RI->currententity->model;

		if( RI->currententity->curstate.iuser3 != -669 )
			continue;

		SET_CURRENT_ENTITY( RI->currententity );

		Vector absmin = RI->currententity->origin + RI->currententity->curstate.mins;
		Vector absmax = RI->currententity->origin + RI->currententity->curstate.maxs;
		if( !Mod_CheckBoxVisible( absmin, absmax ) )
			continue;
		if( R_CullModel( RI->currententity, absmin, absmax ) )
			continue;

		if( r_drawentities->value == 7 )
			DBG_DrawBBox( absmin, absmax );

		R_SetupCable( RI->currententity );
	}
	
	R_RenderCables();

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
			falldepth += sin( SwayPhase * ((float( (e->index % 4) + 1 ) * 2.0f / 4.0f)) ) * ((falldepth * tr.cableSwayIntensity[e->index]) / 10.0f);

		// Calculate dropping point
		VectorSubtract( vposition2, vposition1, vdirection );
		VectorMA( vposition1, 0.5, vdirection, vmidpoint );
		vdroppoint = Vector( vmidpoint[0], vmidpoint[1], vmidpoint[2] - falldepth );

		Vector vParticlePoint = g_vecZero;
		int ParticlePointNum = -1;
		if( bMakeWaterDrops && cablenum == WaterdropCable )
			ParticlePointNum = RANDOM_LONG( 1, isegments - 1 );

		for( int i = 0; i < inumpoints; i++ )
		{
			float f = (float)i / (float)isegments;
			vpoints[i][0] = vposition1[0] * ((1 - f) * (1 - f)) + vdroppoint[0] * ((1 - f) * f * 2) + vposition2[0] * (f * f);
			vpoints[i][1] = vposition1[1] * ((1 - f) * (1 - f)) + vdroppoint[1] * ((1 - f) * f * 2) + vposition2[1] * (f * f);
			vpoints[i][2] = vposition1[2] * ((1 - f) * (1 - f)) + vdroppoint[2] * ((1 - f) * f * 2) + vposition2[2] * (f * f);
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
			if( j == 0 ){ VectorSubtract( vpoints[0], vpoints[1], vTangent ); }
			else { VectorSubtract( vpoints[0], vpoints[j], vTangent ); }

			VectorSubtract( vpoints[j], RI->vieworg, vDir );
			vRight = CrossProduct( vTangent, -vDir ); vRight = vRight.Normalize();

			VectorMA( vpoints[j], fwidth, vRight, vVertex );
			if( tr.cableSwayIntensity[e->index] > 0.0f ) // sway intensity
			{
				if( j != 0 && j != inumpoints - 1 ) // bacontsu - dont animate last and first vertex
					vVertex = vVertex + (right * 1.5f * tr.cableSwayIntensity[e->index] * 5 * cos( SwayPhase * (float( (e->index % 4) + 1 ) * 2.0f / 4.0f) + j * 0.2f ));
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
					vVertex = vVertex + (right * 1.5f * tr.cableSwayIntensity[e->index] * 5 * cos( SwayPhase * (float( (e->index % 4) + 1 ) * 2.0f / 4.0f) + j * 0.2f ));
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

	r_stats.c_cables++;
}