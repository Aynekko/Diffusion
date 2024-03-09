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

static void CableSetRender( bool start )
{
	if( start )
	{
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
	}
	else
	{
		GL_Texture2D( GL_TRUE );
		GL_DepthMask( GL_TRUE );
		GL_Blend( GL_FALSE );
		GL_Cull( GL_FRONT );
	}
}

void HUD_DrawNormalTriangles( void )
{
	gEngfuncs.pTriAPI->Begin(TRI_POLYGON);
	gEngfuncs.pTriAPI->End();

	bool CableRender = false;
	
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

		if( !CableRender )
		{
			CableSetRender( true );
			CableRender = true;
		}

		if( r_drawentities->value == 7 )
			DBG_DrawBBox( absmin, absmax );

		R_DrawCable( RI->currententity );
	}
	
	if( CableRender )
		CableSetRender( false );

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

	bool CableRender = false;

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

		if( !CableRender )
		{
			CableSetRender( true );
			CableRender = true;
		}

		if( r_drawentities->value == 7 )
			DBG_DrawBBox( absmin, absmax );

		R_DrawCable( RI->currententity );
	}
	
	if( CableRender )
		CableSetRender( false );

	if( g_pParticleSystems )
		g_pParticleSystems->UpdateSystems();

	g_pParticles.Update();

	if( g_fRenderInitialized )
		R_DrawWeather();
}

//==========================================================================
// R_DrawCable
// diffusion - thanks to Bacontsu for the cable rendering code!
//==========================================================================
void R_DrawCable( cl_entity_t *e )
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
	if( tr.cableSwayIntensity[e->index] > 0.0f ) // sway intensity
		falldepth += sin( tr.cableSwayPhase[e->index] * ((float( (e->index % 4) + 1 ) * 2.0f / 4.0f)) ) * ((falldepth * tr.cableSwayIntensity[e->index]) / 10.0f);

	float fwidth = e->curstate.fuser2;

	Vector vpoints[50];
	int isegments = bound( 3, e->curstate.iuser2, 49 );

	int inumpoints = isegments + 1;
	Vector Color;
	Color.x = (float)e->curstate.rendercolor.r / 255.0f;
	Color.y = (float)e->curstate.rendercolor.g / 255.0f;
	Color.z = (float)e->curstate.rendercolor.b / 255.0f;
	float RenderAmt = ((float)CL_FxBlend(e) / 255.0f) * tr.fadeblend[e->index];
	vposition1 = e->curstate.origin; // start cable
	vposition2 = e->curstate.vuser1; // end cable

	// Calculate dropping point
	VectorSubtract( vposition2, vposition1, vdirection );
	VectorMA( vposition1, 0.5, vdirection, vmidpoint );
	vdroppoint = Vector( vmidpoint[0], vmidpoint[1], vmidpoint[2] - falldepth );

	bool bMakeWaterDrops = (e->curstate.vuser2.x > 0.01f); // rate: 0 - 255
	Vector vParticlePoint = g_vecZero;
	int ParticlePointNum = -1; 
	if( bMakeWaterDrops )
		ParticlePointNum = RANDOM_LONG( 1, isegments );

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

	// find cable's right fector
	Vector forward = vposition2 - vposition1;
	forward.z = 0;
	forward = forward.Normalize();

	Vector right, angles;
	VectorAngles( forward, angles );
	AngleVectors( angles, NULL, right, NULL );

	pglBegin( GL_TRIANGLE_STRIP );
	for( int j = 0; j < inumpoints; j++ )
	{
		if( j == 0 ){ VectorSubtract( vpoints[0], vpoints[1], vTangent ); }
		else { VectorSubtract( vpoints[0], vpoints[j], vTangent ); }

		VectorSubtract( vpoints[j], RI->vieworg, vDir );
		vRight = CrossProduct( vTangent, -vDir ); vRight = vRight.Normalize();

		GL_Color4f( Color.x, Color.y, Color.z, RenderAmt );

		VectorMA( vpoints[j], fwidth, vRight, vVertex );
		if( tr.cableSwayIntensity[e->index] > 0.0f ) // sway intensity
		{
			if( j != 0 && j != inumpoints - 1 ) // bacontsu - dont animate last and first vertex
				vVertex = vVertex + (right * 1.5f * tr.cableSwayIntensity[e->index] * 5 * cos( tr.cableSwayPhase[e->index] * (float( (e->index % 4) + 1 ) * 2.0f / 4.0f) + j * 0.2f ));
		}
		pglVertex3fv( vVertex );

		VectorMA( vpoints[j], -fwidth, vRight, vVertex );
		if( tr.cableSwayIntensity[e->index] > 0.0f ) // sway intensity
		{
			if( j != 0 && j != inumpoints - 1 ) // bacontsu - dont animate last and first vertex
				vVertex = vVertex + (right * 1.5f * tr.cableSwayIntensity[e->index] * 5 * cos( tr.cableSwayPhase[e->index] * (float( (e->index % 4) + 1 ) * 2.0f / 4.0f) + j * 0.2f ));
		}
		pglVertex3fv( vVertex );
	}
	pglEnd();

	// is it time to create water particle?
	if( bMakeWaterDrops ) // rate: 0 - 255
	{
		if( tr.time >= tr.ParticleTime[e->index] )
		{
			g_pParticles.WaterDrop( e->index, vParticlePoint );
			tr.ParticleTime[e->index] = tr.time + (1.0f / (0.1f + (e->curstate.vuser2.x * 0.2f)));
		}
	}

	r_stats.c_cables++;
}