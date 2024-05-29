/*
r_misc.cpp - renderer misceallaneous
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

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "r_particle.h"
#include "r_quakeparticle.h"
#include "entity_types.h"
#include "event_api.h"
#include "r_weather.h"
#include "r_efx.h"
#include "pm_defs.h"
#include "r_shader.h"
#include "r_view.h"
#include "r_world.h"
#include "r_cvars.h"
#include "triangleapi.h"
#include "discord.h"
#include "build.h"

/*
used iuser3 "flags":
-660 func_smokevolume
-661 prop_explosive_barrel
-662 player's drone
-663 env_bubbles
-666 item_flare
-667 monster on fire
-668 dust particles from car wheel
-669 cable
-670 turned on monitor
*/

#define DEFAULT_SMOOTHNESS	0.35f
#define FILTER_SIZE		2

#define FLASHLIGHT_KEY -1
#define FLASHLIGHT_MIRROR_KEY -2
#define FLASHLIGHT_DIFFUSE_KEY -3
#define MUZZLEFLASH_LIGHT_KEY -4

//===============================================================
// Flashlight: players' and monsters' flashlight management
//===============================================================
void SetupFlashlight( cl_entity_t *pEnt )
{
	Vector v_origin, v_angles;
	static Vector s_origin = tr.viewparams.vieworg; // just for init
	static Vector s_angles = tr.viewparams.viewangles;
	Vector forward, right, up;
	int FlashlightFOV = 65;
	int FlashlightRadius = 600;
	static int FlashlightTexture = -1;
	if( FlashlightTexture == -1 )
	{
		FlashlightTexture = LOAD_TEXTURE( "gfx/flashlight.dds", NULL, 0, TF_SPOTLIGHT );
		if( !FlashlightTexture ) // not found or not supported by hardware
			FlashlightTexture = tr.spotlightTexture;
	}

	// do not multiply flashlights if game is paused
	if( tr.time == tr.oldtime )
		return;

	// dynamic point light for testing
	if( r_testdlight->value > 0 )
	{
		plight_t *dltest = CL_AllocPlight( FLASHLIGHT_KEY );
		gEngfuncs.GetViewAngles( v_angles );
		gEngfuncs.pfnAngleVectors( v_angles, tr.viewparams.forward, tr.viewparams.right, NULL );

		dltest->pointlight = true;
		dltest->projectionTexture = tr.whiteCubeTexture;
		// this needs to disable self-shadow
		dltest->entindex = pEnt->index;
		if( !r_flashlightlockposition->value ) // allow lighting yourself if this is set
			dltest->effect = 1;

		if( r_flashlightlockposition->value && !(gHUD.m_iKeyBits & IN_RELOAD) )
			v_origin = s_origin;
		else
		{
			v_origin = pEnt->origin + tr.viewparams.forward * 25;
			s_origin = v_origin;
		}

		dltest->color.r = dltest->color.g = dltest->color.b = 255;
		dltest->die = tr.time;

		if( r_shadowquality->value < 2 )
			dltest->flags |= CF_NOSHADOWS;

		R_SetupLightProjection( dltest, v_origin, g_vecZero, r_testdlight->value, 90.0f );
		R_SetupLightAttenuationTexture( dltest );
		return;
	}

	plight_t *pl;

	if( UTIL_IsLocal( pEnt->index ) ) // local player
	{
		pl = CL_AllocPlight( FLASHLIGHT_KEY );
		pl->brightness = 1.1f;
		pl->projectionTexture = FlashlightTexture;
		if( r_shadowquality->value < 1 ) // shadows on very low, disable
			pl->flags |= CF_NOSHADOWS;
		
		if( r_flashlightlockposition->value && !(gHUD.m_iKeyBits & IN_RELOAD) )
		{
			v_origin = s_origin;
			v_angles = s_angles;
			gEngfuncs.pfnAngleVectors( v_angles, tr.viewparams.forward, tr.viewparams.right, tr.viewparams.up );
		}
		else
		{
			gEngfuncs.GetViewAngles( v_angles );
			gEngfuncs.pfnAngleVectors( v_angles, tr.viewparams.forward, tr.viewparams.right, tr.viewparams.up );
			s_angles = v_angles;
			v_origin = gEngfuncs.GetLocalPlayer()->origin;
			v_origin.z = tr.viewparams.vieworg.z; // this coord seems to be falling behind a couple of frames, but it looks better when crouching
			v_origin -= tr.viewparams.forward * 15; // move plight behind player's back so the shadows would move when rotating mouse
			v_origin.z -= 6; // drop down plight to have longer shadows
			s_origin = v_origin;
		}
		FlashlightFOV = 50;

		// flickering when low battery.
		static float flickertime = 0;
		static float deviation = 0;
		static float CachedBrightness = 0;
		if( flickertime > gHUD.m_flTime ) // possible saverestore issues
			flickertime = 0;
		if( gHUD.m_Flash.m_flBat < 0.1 )
		{
			if( (gHUD.m_flTime > flickertime + deviation) )
			{
				pl->brightness = CachedBrightness = RANDOM_FLOAT( 0.3, 0.6 );
				flickertime = gHUD.m_flTime;
				deviation = RANDOM_FLOAT( 0.02, 0.2 );
			}
			else
				pl->brightness = CachedBrightness;
		}
		else
			pl->brightness = 0.75 + gHUD.m_Flash.m_flBat * 0.5;

		pl->brightness *= gHUD.m_Flash.m_flTurnOn;

		if( r_shadowquality->value > 1 ) // don't allow diffused light on low and medium shadow settings
		{
			// kind of a diffuse light?..
			plight_t *pld = CL_AllocPlight( FLASHLIGHT_DIFFUSE_KEY );
			pld->effect = 1;
			pld->entindex = pEnt->index;
			pld->flags |= CF_NOSHADOWS | CF_NOGRASSLIGHTING;
			R_SetupLightProjection( pld, v_origin, v_angles, FlashlightRadius * 0.5, 60 + 50 * gHUD.m_Flash.m_flTurnOn );
			R_SetupLightProjectionTexture( pld, pEnt );
			R_SetupLightAttenuationTexture( pld );
			pld->color.r = pld->color.g = pld->color.b = 40;
			pld->die = tr.time; // die at next frame
		}

		// mirror
		pmtrace_t ptr;
		Vector vecEnd = v_origin + (tr.viewparams.forward * 700.0f);
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( v_origin, vecEnd, PM_NORMAL, -1, &ptr );
		const char *texName = gEngfuncs.pEventAPI->EV_TraceTexture( ptr.ent, v_origin, vecEnd );
		if( r_mirrorquality->value && texName && (!Q_strnicmp( texName, "reflect", 7 ) || !Q_strnicmp( texName, "!reflect", 8 )) )
		{
			plight_t *plm = CL_AllocPlight( FLASHLIGHT_MIRROR_KEY );
			if( 1 )//r_shadowquality->value < 1 ) // shadows on very low, disable
				plm->flags |= CF_NOSHADOWS;
			plm->flags |= CF_NOGRASSLIGHTING;
			plm->effect = 1;
			plm->projectionTexture = FlashlightTexture;
			// calculate angles of reflected light
			Vector vecDir = (ptr.endpos - tr.viewparams.vieworg).Normalize();
			float n = -DotProduct( ptr.plane.normal, vecDir );
			vecDir = 2.0 * ptr.plane.normal * n + vecDir;
			Vector MirAng;
			VectorAngles( vecDir, MirAng );
			float MirRad = FlashlightRadius * (1 - ptr.fraction);
			plm->brightness = pl->brightness * (1 - ptr.fraction);
			float MirDist = (ptr.endpos - tr.viewparams.vieworg).Length();
		//	R_SetupLightProjection( plm, ptr.endpos, MirAng, MirRad, FlashlightFOV );
			R_SetupLightProjection( plm, ptr.endpos - vecDir.Normalize() * MirDist, MirAng, MirRad, FlashlightFOV );
			R_SetupLightAttenuationTexture( plm );
			plm->color.r = plm->color.g = plm->color.b = 255;
			plm->die = tr.time; // die at next frame
		}
	}
	else if( pEnt->curstate.effects & EF_MONSTERFLASHLIGHT ) // monster flashlight
	{
		pl = CL_AllocPlight( pEnt->index );
		pl->brightness = 1.1f;
		if( r_shadowquality->value < 1 ) // shadows on very low, disable
			pl->flags |= CF_NOSHADOWS;
		v_origin = R_StudioAttachmentOrigin( pEnt, 1 );
		v_angles = pEnt->angles;
		v_angles += 5 * sin( tr.time );
		pl->projectionTexture = FlashlightTexture;

		// diffuse light
		plight_t *dld = CL_AllocPlight( -pEnt->index );
		dld->pointlight = true;
		dld->projectionTexture = tr.whiteCubeTexture;
		dld->flags |= CF_NOSHADOWS | CF_NOGRASSLIGHTING;
		gEngfuncs.pfnAngleVectors( v_angles, forward, NULL, NULL );
		v_origin = v_origin + forward * 40;
		dld->radius = 80;
		dld->color.r = dld->color.g = dld->color.b = 30;
		dld->die = tr.time;
		R_SetupLightProjection( dld, v_origin, g_vecZero, dld->radius, 90.0f );
		R_SetupLightAttenuationTexture( dld );
	}
	else if( pEnt == tr.pDrone ) // drone's flashlight - works only in 1st-person mode
	{
		pl = CL_AllocPlight( pEnt->index );
		pl->flags |= CF_NOSHADOWS;
		v_angles = pEnt->angles;
		v_origin = pEnt->origin + Vector( 0, 0, 16.0f );
		FlashlightFOV = 60;
		FlashlightRadius = 450;
		pl->projectionTexture = tr.spotlightTexture;
		// xenon light
		pl->color.r = 183;
		pl->color.g = 192;
		pl->color.b = 215;
	}
	else // other players, in multiplayer
	{
		pl = CL_AllocPlight( pEnt->index );
		pl->brightness = 1.1f;
		if( r_shadowquality->value < 1 ) // shadows on very low, disable
			pl->flags |= CF_NOSHADOWS;
		v_angles = pEnt->angles;
		v_angles.x = -v_angles.x * 3; // ??? stupid quake bug?
		if( pEnt->curstate.effects & EF_UPSIDEDOWN )
			v_angles.x = -v_angles.x;
		gEngfuncs.pfnAngleVectors( v_angles, forward, NULL, NULL );
		v_origin = pEnt->origin - forward * 20;
		v_origin.z -= 6;
		FlashlightFOV = 50;
		pl->projectionTexture = FlashlightTexture;
	}

	// copy the entity number - we don't need light and shadows
	// for our own player model or weapon from our own flashlight
	pl->entindex = pEnt->index;
	if( !r_flashlightlockposition->value ) // allow lighting yourself if this is set
		pl->effect = 1; // diffusion - just a flag that this is a flashlight

	R_SetupLightProjection( pl, v_origin, v_angles, FlashlightRadius, FlashlightFOV );
	R_SetupLightAttenuationTexture( pl );

	pl->color.r = pl->color.g = pl->color.b = 255;
	pl->die = tr.time; // die at next frame
}

/*
========================
HUD_AddEntity

Return 0 to filter entity from visible list for rendering
========================
*/

int HUD_AddEntity( int type, struct cl_entity_s* ent, const char* modelname )
{		
	if( g_fRenderInitialized )
	{
		// use engine renderer
		if( gl_renderer->value == 0 )
			return 1;

		if( ent->curstate.effects & EF_SCREEN )
		{
			ent->curstate.movetype = 0; // clear movetype for screen (engine sets this to MOVETYPE_FOLLOW because we use aiment. Argh!)
			if( !FBitSet(world->features, WORLD_HAS_SCREENS) ) // set it here if it wasn't set (happens with studio screens)
				SetBits( world->features, WORLD_HAS_SCREENS );
		}

		if( ent->curstate.effects & EF_SKYCAMERA )
		{
			// found env_sky
			tr.sky_camera = ent;
			return 0;
		}

		if( (ent->curstate.effects & EF_SKIPPVS) && (ent->curstate.renderfx == kRenderFxFullbright) ) // hack to find this entity
		{
			// found shader modifier
			tr.shader_modifier = ent;
			return 0;
		}

		if( g_iUser1 )
		{
			//		gHUD.m_Spectator.AddOverviewEntity(type, ent, modelname);
			if( (g_iUser1 == OBS_IN_EYE/* || gHUD.m_Spectator.m_pip->value == INSET_IN_EYE*/) && ent->index == g_iUser2 )
				return 0;	// don't draw the player we are following in eye
		}

		if( type == ET_BEAM )
			return 1; // let the engine draw beams // diffusionFIXME - beams don't work in 3D skybox, obviously

		float DistanceFade = R_ComputeFadingDistance( ent );	
		if( DistanceFade <= 0.0f )
			return 0; // distance-culled

		if( !tr.nullmodelindex )
			tr.nullmodelindex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/null.spr" );

		// experimental
		if( 0 )//r_fade_props->value == 2 && tr.fadeblend[ent->index] == 1.0f )
		{
			float x, y, w, h;
			if( R_ScissorForAABB( ent->curstate.origin + ent->curstate.mins, ent->curstate.origin + ent->curstate.maxs, &x, &y, &w, &h ) )
			{
				//	ConPrintf( "%i %i %i %i\n", (int)x, (int)y, (int)w, (int)h );
			//	R_DrawScissorRectangle( x, y, w, h );
				if( w < 100 && h < 100 )
				{
					float val = w;
					if( h > w ) val = h;
					val = bound( 50, val, 60 );
					tr.fadeblend[ent->index] = RemapVal( val, 50, 60, 0, 1 );
				//	ConPrintf( "%f\n", tr.fadeblend[ent->index] );

					// keep the custom render mode intact, only switch to Fade if Normal or Solid (those can't fade)
					if( tr.fadeblend[ent->index] < 1.0f )
					{
						if( (ent->curstate.rendermode == kRenderNormal) || (ent->curstate.rendermode == kRenderTransAlpha) )
							ent->curstate.rendermode = kRenderFade; // this is a 100% clone of Texture, the only difference is light brightness (Texture has much less - see BMODEL_KRENDERTRANSTEXTURE)
					}

					tr.fadeblend[ent->index] = bound( 0, tr.fadeblend[ent->index], 1.0f );
					if( tr.fadeblend[ent->index] <= 0.0f )
						return 0;
				}
			}
		}

		if( !R_AddEntity( ent, type ) )
			return 0;

		// apply effects
		if( ent->curstate.effects & EF_BRIGHTFIELD )
		{
			if( tr.time != tr.oldtime ) // diffusion - fix overflow
				gEngfuncs.pEfxAPI->R_EntityParticles( ent );
		}

		// add in muzzleflash effect
		if( ent->curstate.effects & EF_MUZZLEFLASH )
			ent->curstate.effects &= ~EF_MUZZLEFLASH;

		// add light effect
		if( ent->curstate.effects & EF_LIGHT )
		{
			plight_t* dl = CL_AllocPlight( ent->index );
			dl->pointlight = true;
			dl->projectionTexture = tr.whiteCubeTexture;
			dl->die = tr.time;	// die at next frame
			dl->color.r = 100;
			dl->color.g = 100;
			dl->color.b = 100;
			R_SetupLightProjection( dl, ent->origin, g_vecZero, 200, 90 );
			R_SetupLightAttenuationTexture( dl );
			dl->flags |= CF_NOSHADOWS;
			dl->brightness *= DistanceFade;

			gEngfuncs.pEfxAPI->R_RocketFlare( ent->origin );
		}
		
		if( ent->curstate.effects & EF_MONSTERFLASHLIGHT ) // monster flashlight
			SetupFlashlight( ent );

		if( ent->curstate.iuser3 == -668 ) // dust particles from car wheel
		{
			if((gHUD.m_flTimeDelta > 0) && (ent->curstate.fuser1 > 10.0f) && ((int)ent->curstate.fuser1 % 5 == 0) )
			{
				float dust_speed = (300 * g_fFrametime) / (1 + ent->curstate.fuser1 * 0.002);
				float dust_scale = bound( 10, ent->curstate.fuser1 * 0.02, 40 ) + RANDOM_LONG(-3,3);
				float dust_randomize_pos = bound( 5, ent->curstate.fuser1 * 0.02, 12);
				int type = ent->curstate.iuser1; // 0 asphalt, 1 sand, 2 dirt, 3 watersplash
				if( type == 3 )
					g_pParticles.CreateEffect( 0, "small_splash", ent->curstate.origin, g_vecZero );
				else
					g_pParticles.Smoke( ent->index, type, ent->curstate.origin - Vector(0,0,ent->curstate.iuser2 * 0.25), g_vecZero, 1, dust_speed, dust_scale, dust_randomize_pos, 0, Vector(1,1,1), 1000 );
			}
		}

		if( ent->curstate.renderfx == kRenderFxParticle )
		{
			if( ent->curstate.fuser1 <= 0.0f )
				return 0; // something bad happened with the timing (check delta.lst?)

			int ParticleEntIndex = ent->index;
			switch( ent->curstate.iuser1 )
			{
			default:
			case 0: // sparks
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "env_spark.part1", ent->curstate.origin, g_vecZero );
				g_pParticles.CreateEffect( ParticleEntIndex, "env_spark.part2", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 1: // metal_spark
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "metal_spark", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 2: // wood_gibs
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "wood_gibs", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 3: // test_smoke
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "test_smoke", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 4: // green_leaves
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "green_leaves", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 5: // clouds
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "clouds", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 6: // clouds2
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "clouds2", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 7: // water drop
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.WaterDrop( ParticleEntIndex, ent->curstate.origin );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			case 8: // clouds_night
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					break;
				g_pParticles.CreateEffect( ParticleEntIndex, "clouds_night", ent->curstate.origin, g_vecZero );
				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser1;
				break;
			}
		}

		// env_particle_line
		if( ent->curstate.renderfx == kRenderFxParticleLine )
		{
			if( ent->curstate.renderamt > 0 && tr.time > tr.ParticleTime[ent->index] )
			{
				if( ent->curstate.iuser2 > 0 ) // particle allowed distance
					g_pParticles.WaterDripLine( ent->curstate.origin, ent->curstate.vuser1, ent->curstate.iuser2 );
				else
					g_pParticles.WaterDripLine( ent->curstate.origin, ent->curstate.vuser1 );
				tr.ParticleTime[ent->index] = tr.time + ( 1.0f / (0.1f + (ent->curstate.renderamt * 0.2f)) );
			}
		}
		
		// add dimlight
		if( ent->curstate.effects & EF_DIMLIGHT )
		{
			plight_t *dl;

			if( type == ET_PLAYER || ent == tr.pDrone )
			{
				SetupFlashlight( ent );
			}
			else if( ent->curstate.iuser3 == -666 ) // item_flare!
			{
				dl = CL_AllocPlight( ent->index );
				dl->pointlight = true;
				dl->projectionTexture = tr.whiteCubeTexture;
				dl->die = tr.time;	// die at next frame
				dl->color.r = 255;
				dl->color.g = RANDOM_LONG( 32, 64 );
				dl->color.b = RANDOM_LONG( 32, 64 );
				R_SetupLightProjection( dl, ent->origin + Vector( 0, 0, 3 ), g_vecZero, ent->curstate.iuser2 * 5, 90 );
				R_SetupLightAttenuationTexture( dl );
				dl->brightness *= ent->curstate.iuser2 * 0.008 * DistanceFade;
			}
			else if( ent->model->type == mod_brush && ent->curstate.iuser3 == -660 ) // func_smokevolume!
			{
				if( ent->curstate.fuser3 <= 0.0f )
					return 0; // something bad happened with the timing (check delta.lst?)
				
				int ParticleEntIndex = ent->index;
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					return 0;
				
				bool IsSmoke = ((ent->curstate.iuser1 == 0) || (ent->curstate.iuser1 == 2));
				bool IsDustMotes = ((ent->curstate.iuser1 == 1) || (ent->curstate.iuser1 == 3));
				bool IsWaterfall = (ent->curstate.iuser1 == 4);

				Vector SmV_Org = ent->curstate.origin + (ent->curstate.mins + ent->curstate.maxs) * 0.5f;
				// size
				Vector SmV_Size = ent->curstate.maxs - ent->curstate.mins;

				float Density = ent->curstate.fuser2;
				if( Density <= 0.0f )
				{
					if( IsSmoke )
						Density = 33.0f;
					else if( IsDustMotes )
						Density = 10.0f;
					else if( IsWaterfall )
						Density = 40.0f;
				}

				int Count = (SmV_Size.x *SmV_Size.y + SmV_Size.y * SmV_Size.z + SmV_Size.z * SmV_Size.x) / (3 * Density * Density);
				if( Count < 1 ) Count = 1;
				Vector vecSpot;
				int i, j;
				float SmV_Alpha = ent->curstate.renderamt / 255.0f;
				int SmV_Distance = ent->curstate.iuser2;
				float SmV_Scale = ent->curstate.fuser1;
				Vector Color = Vector( ent->curstate.rendercolor.r, ent->curstate.rendercolor.g, ent->curstate.rendercolor.b ) / 255.0f;

				if( SmV_Alpha <= 0.0f )
					SmV_Alpha = 0.1f;

				int Contents = 0;

				for( i = 0; i < Count; i++ )
				{
					for( j = 0; j < 32; j++ )
					{
						// fill up the box with stuff
						vecSpot[0] = SmV_Org.x + RANDOM_FLOAT( -0.5f, 0.5f ) * SmV_Size.x;
						vecSpot[1] = SmV_Org.y + RANDOM_FLOAT( -0.5f, 0.5f ) * SmV_Size.y;
						vecSpot[2] = SmV_Org.z + RANDOM_FLOAT( -0.5f, 0.5f ) * SmV_Size.z;

						Contents = POINT_CONTENTS( vecSpot );
						if( Contents == CONTENTS_WATER )
							continue;

						if( Contents != CONTENTS_SOLID )
							break; // valid spot
					}

					// test distance here
					if( SmV_Distance > 0 )
					{
						if( (vecSpot - tr.viewparams.vieworg).Length() > SmV_Distance )
							continue;
					}

					if( IsWaterfall )
						g_pParticles.Waterfall( ParticleEntIndex, vecSpot, ent->curstate.vuser1, ent->curstate.vuser2, Color, SmV_Scale, SmV_Alpha, SmV_Distance );
					else
						g_pParticles.SmokeVolume( ParticleEntIndex, ent->curstate.iuser1, vecSpot, ent->curstate.vuser1, ent->curstate.vuser2, Color, SmV_Scale, SmV_Alpha, SmV_Distance );
				}

				tr.ParticleTime[ParticleEntIndex] = tr.time + ent->curstate.fuser3;

				return 0; // don't draw this entity
			}
			else if( ent->model->type == mod_brush && ent->curstate.iuser3 == -663 ) // env_bubbles!
			{
				if( ent->curstate.fuser2 <= 0.0f )
					return 0; // something bad happened with the timing (check delta.lst?)

				int ParticleEntIndex = ent->index;
				if( tr.time < tr.ParticleTime[ParticleEntIndex] )
					return 0;

				int B_Distance = ent->curstate.iuser4;
				int Count = (int)ent->curstate.fuser1 + 1;
				int Density = Count * 3 + 6;
				float maxHeight = ent->curstate.maxs.z - ent->curstate.mins.z;
				int width = ent->curstate.maxs.x - ent->curstate.mins.x;
				int depth = ent->curstate.maxs.y - ent->curstate.mins.y;

				Vector B_Org;
				for( int i = 0; i < Count; i++ )
				{
					B_Org.x = ent->curstate.mins.x + RANDOM_LONG( 0, width - 1 );
					B_Org.y = ent->curstate.mins.y + RANDOM_LONG( 0, depth - 1 );
					B_Org.z = ent->curstate.mins.z;
					float vertical_speed = RANDOM_LONG( 80, 140 );

					g_pParticles.Bubble( ParticleEntIndex, B_Org, vertical_speed, B_Distance, tr.time + (maxHeight / vertical_speed) - 0.1f, ent->curstate.fuser3 );
				}

				// set next time for the bubbles to appear
				float m_bubble_frequency = ent->curstate.fuser2;
				if( m_bubble_frequency > 19.0f )
					m_bubble_frequency = 0.5f;
				else
					m_bubble_frequency = 2.5f - (0.1f * m_bubble_frequency);

				tr.ParticleTime[ParticleEntIndex] = tr.time + m_bubble_frequency;

				return 0; // don't draw this entity
			}
			else
			{
				dl = CL_AllocPlight( ent->index );
				dl->pointlight = true;
				dl->projectionTexture = tr.whiteCubeTexture;
				dl->die = tr.time;	// die at next frame
				dl->color.r = 255;
				dl->color.g = 255;
				dl->color.b = 255;
				float radius = RANDOM_LONG( 250, 300 );
				if( ent->curstate.iuser3 == -667 ) // monster is on fire - disable shadows, looks bad
					dl->flags |= CF_NOSHADOWS;
				if( ent->curstate.iuser3 == -661 ) // prop_explosive_barrel
				{
					dl->color.r = 230;
					dl->color.g = 180;
					dl->color.b = 100;
					radius *= 0.5;
					ent->curstate.renderfx = kRenderFxFullbrightNoShadows;
				}
				R_SetupLightProjection( dl, ent->origin + Vector(0,0,20), g_vecZero, radius, 90 );
				R_SetupLightAttenuationTexture( dl );
				dl->brightness *= DistanceFade;
			}
		}

		if( ent->curstate.effects & EF_BRIGHTLIGHT )
		{
			plight_t* dl = CL_AllocPlight( 0 );
			dl->pointlight = true;
			dl->projectionTexture = tr.whiteCubeTexture;
			Vector org = ent->origin;
			org.z += 16;
			dl->die = tr.time; // die at next frame
			dl->color.r = 255;
			dl->color.g = 255;
			dl->color.b = 255;

			if( type == ET_PLAYER )
				dl->radius = 430;
			else dl->radius = gEngfuncs.pfnRandomLong( 400, 430 );

			R_SetupLightProjection( dl, org, g_vecZero, dl->radius, 90 );
			R_SetupLightAttenuationTexture( dl );
			dl->brightness *= DistanceFade;
		}

		// projected light can be attached like as normal dlight
		if( ent->curstate.effects & EF_PROJECTED_LIGHT )
		{
			plight_t* pl = CL_AllocPlight( ent->index );
			float factor = 1.0f;

			if( ent->curstate.renderfx )
			{
				factor = tr.lightstylevalue[ent->curstate.renderfx] * (1.0f / 255.0f);
				factor = bound( 0.0f, factor, 1.0f );
			}

			if( ent->curstate.rendercolor.r == 0 && ent->curstate.rendercolor.g == 0 && ent->curstate.rendercolor.b == 0 )
				pl->color.r = pl->color.g = pl->color.b = 255;
			else
			{
				pl->color.r = ent->curstate.rendercolor.r;
				pl->color.g = ent->curstate.rendercolor.g;
				pl->color.b = ent->curstate.rendercolor.b;
			}

			pl->color.r *= factor;
			pl->color.g *= factor;
			pl->color.b *= factor;
			
			float radius = ent->curstate.iuser3 ? ent->curstate.iuser3 : 500; // default light radius
			float fov = ent->curstate.iuser2 ? ent->curstate.iuser2 : 50;
			pl->die = tr.time; // die at next frame
			pl->flags = ent->curstate.iuser1;
			pl->brightness = ent->curstate.fuser1;
			pl->entindex = ent->index;

			Vector origin, angles;

			R_GetLightVectors( ent, origin, angles );
			R_SetupLightProjectionTexture( pl, ent );
			R_SetupLightProjection( pl, origin, angles, radius, fov );
			R_SetupLightAttenuationTexture( pl, -1 );
			pl->brightness *= DistanceFade;
		}

		// dynamic light can be attached like normal dlight
		if( ent->curstate.effects & EF_DYNAMIC_LIGHT )
		{
			plight_t* pl = CL_AllocPlight( ent->index );
			float factor = 1.0f;

			if( ent->curstate.renderfx )
			{
			//	factor = tr.lightstylevalue[ent->curstate.renderfx] * (1.0f / 255.0f);
				ent->curstate.renderamt /= 2; // hack!
				factor = CL_FxBlend( ent ) * (1.0f / 255.0f);
				factor = bound( 0.0f, factor, 1.0f );
			}

			if( ent->curstate.rendercolor.r == 0 && ent->curstate.rendercolor.g == 0 && ent->curstate.rendercolor.b == 0 )
			{
				pl->color.r = pl->color.g = pl->color.b = 255;
			}
			else
			{
				pl->color.r = ent->curstate.rendercolor.r;
				pl->color.g = ent->curstate.rendercolor.g;
				pl->color.b = ent->curstate.rendercolor.b;
			}

			pl->color.r *= factor;
			pl->color.g *= factor;
			pl->color.b *= factor;

			pl->brightness = ent->curstate.fuser1;
			
			float radius = ent->curstate.scale ? (ent->curstate.scale * 8.0f) : 300; // default light radius
			pl->die = tr.time; // die at next frame
			pl->flags = ent->curstate.iuser1;
			pl->projectionTexture = tr.whiteCubeTexture;
			pl->pointlight = true;
			pl->entindex = ent->index;

			Vector origin, angles;

			origin = ent->origin;

			if( pl->flags & CF_NOLIGHT_IN_SOLID )
			{
				pmtrace_t	tr;

				// test the lights who stuck in the solid geometry
				gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
				gEngfuncs.pEventAPI->EV_PlayerTrace( origin, origin, PM_STUDIO_IGNORE, -1, &tr );

				// an experimental feature for point lights
				if( tr.allsolid ) radius = 0.0f;
			}

			if( radius != 0.0f )
			{
				// diffusion - pointlight doesn't need angles
				R_SetupLightProjection( pl, origin, g_vecZero, radius, 90.0f );
				R_SetupLightAttenuationTexture( pl );
				pl->brightness *= DistanceFade;
			}
			else 
				pl->radius = 0.0f; // light in solid
		}

		if( ent->curstate.effects & EF_SCREENMOVIE )
			R_UpdateCinSound( ent ); // update cin sound properly

		if( ent->curstate.effects & EF_ROTATING )
		{
			if( ent->model->type == mod_studio )
				FuncRotatingClient( ent );
		}
		
		// diffusion - setup the laser spot's local origin
		if( ent->curstate.effects & EF_MYLASERSPOT )
		{
			// it's our laser
			if( ent->curstate.owner == gEngfuncs.GetLocalPlayer()->index )
			{
				Vector v_angles, PlayerOrg;
				gEngfuncs.GetViewAngles( v_angles );
				gEngfuncs.pfnAngleVectors( v_angles, tr.viewparams.forward, NULL, NULL );
				PlayerOrg = tr.viewparams.vieworg + tr.viewparams.simvel * 2 * g_fFrametime; // have to compensate with velocity because it's falling behind
				pmtrace_t *LaserDotTrace = gEngfuncs.PM_TraceLine( PlayerOrg, PlayerOrg + tr.viewparams.forward * 1500, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
				ent->origin = LaserDotTrace->endpos;
			}

			ent->curstate.scale += RANDOM_FLOAT( -0.025, 0.025 );
		}

		if( ent->model->type == mod_studio )
		{
			if( ent->model->flags & STUDIO_ROTATE )
				ent->angles[1] = anglemod( 100 * gEngfuncs.GetClientTime() );

			if( ent->model->flags & STUDIO_GIB )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 2 );
			else if( ent->model->flags & STUDIO_ZOMGIB )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 4 );
			else if( ent->model->flags & STUDIO_TRACER )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 3 );
			else if( ent->model->flags & STUDIO_TRACER2 )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 5 );
			else if( ent->model->flags & STUDIO_ROCKET )
			{
				plight_t* dl = CL_AllocPlight( ent->index );
				dl->pointlight = true;
				dl->flags |= CF_NOSHADOWS;
				dl->projectionTexture = tr.whiteCubeTexture;
				dl->color.r = 255;
				dl->color.g = 255;
				dl->color.b = 255;

				// HACKHACK: get radius from head entity
				if( ent->curstate.rendermode != kRenderNormal )
					dl->radius = max( 0, ent->curstate.renderamt - 55 );
				else dl->radius = 200;
				dl->die = tr.time;

				R_SetupLightProjection( dl, ent->origin, g_vecZero, dl->radius, 90.0f );
				R_SetupLightAttenuationTexture( dl );

				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 0 );
			}
			else if( ent->model->flags & STUDIO_GRENADE )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 1 );
			else if( ent->model->flags & STUDIO_TRACER3 )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 6 );
		}
		return 0;
	}

	return 1;
}

void R_MuzzleDynLight( const struct cl_entity_s *e, Vector origin, int WeaponID )
{
	if( RI->params & RP_SHADOWPASS )
		return;

	if( cl_muzzlelight->value <= 0 )
		return;
	
	// don't create two lights
	if( RP_LOCALCLIENT( e ) && !(RI->params & RP_THIRDPERSON) )
		return;
	
	plight_t *dl = CL_AllocPlight( MUZZLEFLASH_LIGHT_KEY );

	if( e == GET_VIEWMODEL() )
	{
		dl->pointlight = false; // use projector like L4D
		dl->projectionTexture = tr.spotlightTexture;
		if( tr.lowmemory )
			dl->flags |= CF_NOSHADOWS;
		Vector v_angles, Forward;
		gEngfuncs.GetViewAngles( v_angles );
		gEngfuncs.pfnAngleVectors( v_angles, Forward, NULL, NULL );
		origin -= Forward * 15;
		origin.x += RANDOM_FLOAT( -2.5f, 2.5f );
		origin.y += RANDOM_FLOAT( -2.5f, 2.5f );
		origin.z += RANDOM_LONG( -5, 5 );
	}
	else
	{
		dl->pointlight = true;
		dl->projectionTexture = tr.whiteCubeTexture;
		dl->flags |= CF_NOSHADOWS;
	}

	dl->die = tr.time + 0.05f;
	
	// default parameters - monsters (and other players...?)
	int R = 255;
	int G = 255;
	int B = 180;
	int Radius = 100;
	float Brightness = 1.25f;
	
	switch( WeaponID )
	{
	case WEAPON_GAUSS:
		R = 70;
		G = 169;
		B = 255;
	break;
	case WEAPON_AR2:
		R = 70;
		G = 169;
		B = 255;
	break;
	case WEAPON_SNIPER:
		Radius = 200;
		Brightness = 1.8f;
	break;
	case WEAPON_BERETTA:
	case WEAPON_FIVESEVEN:
		Radius = 130;
	break;
	case WEAPON_HKMP5:
		Radius = 130;
	break;
	case WEAPON_MRC:
		Radius = 150;
	break;
	case WEAPON_SHOTGUN:
	case WEAPON_SHOTGUN_XM:
	case WEAPON_DEAGLE:
		Radius = 170;
	break;
	}

	int deviation = 25;
	R += RANDOM_LONG( -deviation, deviation );
	G += RANDOM_LONG( -deviation, deviation );
	B += RANDOM_LONG( -deviation, deviation );

	dl->color.r = bound( 0, R, 255 );
	dl->color.g = bound( 0, G, 255 );
	dl->color.b = bound( 0, B, 255 );
	dl->brightness = Brightness;
	dl->effect = 2; // muzzleflash light
	dl->entindex = gEngfuncs.GetLocalPlayer()->index;

	if( e == GET_VIEWMODEL() )
	{
		R_SetupLightProjection( dl, origin, e->curstate.angles, Radius * 3.0f, 120.0f );
		dl->lightFalloff = 2.5f;
	}
	else
	{
		R_SetupLightProjection( dl, origin, g_vecZero, Radius, 90.0f );
		R_SetupLightAttenuationTexture( dl );
	}
}

//==============================================================
// diffusion - client-side simple rotating entity (EF_ROTATING)
//==============================================================
void FuncRotatingClient( cl_entity_t *e )
{
	/*
	vuser1 - direction
	iuser1 - on/off state
	iuser2 - speed
	*/
	
	if( (tr.time != tr.oldtime) && (e->curstate.iuser1 == 1) ) // do not rotate when paused or off
	{
		e->baseline.angles.x += e->curstate.iuser2 * g_fFrametime * e->curstate.vuser1.x;
		e->baseline.angles.y += e->curstate.iuser2 * g_fFrametime * e->curstate.vuser1.y;
		e->baseline.angles.z += e->curstate.iuser2 * g_fFrametime * e->curstate.vuser1.z;
	}

	if( e->baseline.angles.x > 359 ) e->baseline.angles.x = 0;
	if( e->baseline.angles.y > 359 ) e->baseline.angles.y = 0;
	if( e->baseline.angles.z > 359 ) e->baseline.angles.z = 0;
	if( e->baseline.angles.x < -359 ) e->baseline.angles.x = 0;
	if( e->baseline.angles.y < -359 ) e->baseline.angles.y = 0;
	if( e->baseline.angles.z < -359 ) e->baseline.angles.z = 0;

	e->angles = e->baseline.angles;
}

/*
=========================
HUD_TxferLocalOverrides

The server sends us our origin with extra precision as part of the clientdata structure, not during the normal
playerstate update in entity_state_t.  In order for these overrides to eventually get to the appropriate playerstate
structure, we need to copy them into the state structure at this point.
=========================
*/
void HUD_TxferLocalOverrides( struct entity_state_s* state, const struct clientdata_s* client )
{
	state->origin = client->origin;
	state->velocity = client->velocity;

	// Spectator
	state->iuser1 = client->iuser1;
	state->iuser2 = client->iuser2;

	// Duck prevention
	state->iuser3 = client->iuser3;

	// Fire prevention
	state->iuser4 = client->iuser4;

	// always have valid PVS message
	r_currentMessageNum = state->messagenum;
}

/*
=========================
HUD_ProcessPlayerState

We have received entity_state_t for this player over the network.  We need to copy appropriate fields to the
playerstate structure
=========================
*/
void HUD_ProcessPlayerState( struct entity_state_s* dst, const struct entity_state_s* src )
{
	// Copy in network data
	dst->origin = src->origin;
	dst->angles = src->angles;

	dst->velocity = src->velocity;
	dst->basevelocity = src->basevelocity;

	dst->frame = src->frame;
	dst->modelindex = src->modelindex;
	dst->skin = src->skin;
	dst->effects = src->effects;
	dst->weaponmodel = src->weaponmodel;
	dst->movetype = src->movetype;
	dst->sequence = src->sequence;
	dst->animtime = src->animtime;

	dst->solid = src->solid;

	dst->rendermode = src->rendermode;
	dst->renderamt = src->renderamt;
	dst->rendercolor.r = src->rendercolor.r;
	dst->rendercolor.g = src->rendercolor.g;
	dst->rendercolor.b = src->rendercolor.b;
	dst->renderfx = src->renderfx;

	dst->framerate = src->framerate;
	dst->body = src->body;

	dst->friction = src->friction;
	dst->gravity = src->gravity;
	dst->gaitsequence = src->gaitsequence;
	dst->usehull = src->usehull;
	dst->playerclass = src->playerclass;
	dst->team = src->team;
	dst->colormap = src->colormap;
	dst->spectator = src->spectator;

	memcpy( &dst->controller[0], &src->controller[0], 4 * sizeof( byte ) );
	memcpy( &dst->blending[0], &src->blending[0], 2 * sizeof( byte ) );

	// Save off some data so other areas of the Client DLL can get to it
	cl_entity_t* player = GET_LOCAL_PLAYER(); // Get the local player's index

	if( dst->number == player->index )
	{
		// always have valid PVS message
		r_currentMessageNum = src->messagenum;
		g_iPlayerClass = dst->playerclass;
		g_iTeamNumber = dst->team;
		g_iUser1 = src->iuser1;
		g_iUser2 = src->iuser2;
		g_iUser3 = src->iuser3;

#ifdef XASH_64BIT
		discord_integration::set_spectating( g_iUser1 > 0 );
#endif
	}
}

/*
=========================
HUD_TxferPredictionData

Because we can predict an arbitrary number of frames before the server responds with an update, we need to be able to copy client side prediction data in
 from the state that the server ack'd receiving, which can be anywhere along the predicted frame path ( i.e., we could predict 20 frames into the future and the server ack's
 up through 10 of those frames, so we need to copy persistent client-side only state from the 10th predicted frame to the slot the server
 update is occupying.
=========================
*/
void HUD_TxferPredictionData( entity_state_t* ps, const entity_state_t* pps, clientdata_t* pcd, const clientdata_t* ppcd, weapon_data_t* wd, const weapon_data_t* pwd )
{
	ps->oldbuttons = pps->oldbuttons;
	ps->flFallVelocity = pps->flFallVelocity;
	ps->iStepLeft = pps->iStepLeft;
	ps->playerclass = pps->playerclass;

	pcd->viewmodel = ppcd->viewmodel;
	pcd->m_iId = ppcd->m_iId;
	pcd->ammo_shells = ppcd->ammo_shells;
	pcd->ammo_nails = ppcd->ammo_nails;
	pcd->ammo_cells = ppcd->ammo_cells;
	pcd->ammo_rockets = ppcd->ammo_rockets;
	pcd->m_flNextAttack = ppcd->m_flNextAttack;
	pcd->fov = ppcd->fov;
	pcd->weaponanim = ppcd->weaponanim;
	pcd->tfstate = ppcd->tfstate;
	pcd->maxspeed = ppcd->maxspeed;

	pcd->deadflag = ppcd->deadflag;
	
	// Spectator
	pcd->iuser1 = ppcd->iuser1;
	pcd->iuser2 = ppcd->iuser2;

	// Duck prevention
	pcd->iuser3 = ppcd->iuser3;

	if( gEngfuncs.IsSpectateOnly() )
	{
		// in specator mode we tell the engine who we want to spectate and how
		// iuser3 is not used for duck prevention (since the spectator can't duck at all)
		pcd->iuser1 = g_iUser1;	// observer mode
		pcd->iuser2 = g_iUser2; // first target
		pcd->iuser3 = g_iUser3; // second target
	}

	// Fire prevention
	pcd->iuser4 = ppcd->iuser4;

	pcd->fuser2 = ppcd->fuser2;
	pcd->fuser3 = ppcd->fuser3;

	pcd->vuser1 = ppcd->vuser1;
	pcd->vuser2 = ppcd->vuser2;
	pcd->vuser3 = ppcd->vuser3;
	pcd->vuser4 = ppcd->vuser4;

	memcpy( wd, pwd, 32 * sizeof( weapon_data_t ) );
}

/*
=========================
HUD_CreateEntities

Gives us a chance to add additional entities to the render this frame
=========================
*/
void HUD_CreateEntities( void )
{
	// e.g., create a persistent cl_entity_t somewhere.
	// Load an appropriate model into it ( gEngfuncs.CL_LoadModel )
	// Call gEngfuncs.CL_CreateVisibleEntity to add it to the visedicts list
#if 0
	if( FBitSet( world->features, WORLD_HAS_SCREENS | WORLD_HAS_PORTALS | WORLD_HAS_MIRRORS ) )
		HUD_AddEntity( ET_PLAYER, GET_LOCAL_PLAYER(), GET_LOCAL_PLAYER()->model->name );
#endif
}

//======================
//	DRAW BEAM EVENT
//
// special effect for displacer
//======================
void HUD_DrawBeam( void )
{
	cl_entity_t* view = GET_VIEWMODEL();
	int m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/plasma.spr" );

	gEngfuncs.pEfxAPI->R_BeamEnts( view->index | 0x1000, view->index | 0x2000, m_iBeam, 1.05f, 0.8f, 0.5f, 0.5f, 0.6f, 0, 10, 2, 10, 0 );
	gEngfuncs.pEfxAPI->R_BeamEnts( view->index | 0x1000, view->index | 0x3000, m_iBeam, 1.05f, 0.8f, 0.5f, 0.5f, 0.6f, 0, 10, 2, 10, 0 );
	gEngfuncs.pEfxAPI->R_BeamEnts( view->index | 0x1000, view->index | 0x4000, m_iBeam, 1.05f, 0.8f, 0.5f, 0.5f, 0.6f, 0, 10, 2, 10, 0 );
}

//======================
//	Eject Shell
//
// eject specified shell from viewmodel
// NOTE: shell model must be precached on a server
//======================
void HUD_EjectShell( const struct mstudioevent_s* event, const struct cl_entity_s* entity )
{
	if( entity != GET_VIEWMODEL() )
		return; // can eject shells only from viewmodel

	Vector view_ofs, ShellOrigin, ShellVelocity, forward, right, up;
	Vector angles = Vector( 0, entity->angles[YAW], 0 );
	Vector origin = entity->origin;

	float fR, fU;

	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( event->options );

	if( !shell )
	{
		ALERT( at_error, "model %s not precached\n", event->options );
		return;
	}

	int i;
	for( i = 0; i < 3; i++ )
	{
		if( angles[i] < -180 ) angles[i] += 360;
		else if( angles[i] > 180 ) angles[i] -= 360;
	}

	angles.x = -angles.x;
	AngleVectors( angles, forward, right, up );

	fR = RANDOM_FLOAT( 50, 70 );
	fU = RANDOM_FLOAT( 100, 150 );

	for( i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = tr.viewparams.simvel[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i] = tr.viewparams.vieworg[i] + up[i] * -12 + forward[i] * 20 + right[i] * 4;
	}

	gEngfuncs.pEfxAPI->R_TempModel( ShellOrigin, ShellVelocity, angles, RANDOM_LONG( 5, 10 ), shell, TE_BOUNCE_SHELL );
}

//========================================================================
// Force a certain sprite for the local player.
// this is full of hacks. TODO - fix all models and make better system
//========================================================================
void GetMuzzleflashSprite( const cl_entity_t *e, int type, int &modelIndex, float &scale, float &dieoffset )
{
	scale = 0.1f;

	int WeaponID = gHUD.m_Ammo.WeaponID;

	switch( WeaponID )
	{
	case WEAPON_MRC:
		if( type == -1 ) // secondary attack, specified in qc
		{
			modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash_ar_secondary.spr" );
			scale = 0.05f;
			dieoffset = 0.15f;
		}
		else
			modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash1.spr" );
	break;

	case WEAPON_BERETTA:
	case WEAPON_HKMP5:
	case WEAPON_FIVESEVEN:
	case WEAPON_G36C:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash2.spr" );
	break;

	case WEAPON_DEAGLE:
	case WEAPON_SHOTGUN:
	case WEAPON_SHOTGUN_XM:
	case WEAPON_SNIPER:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash2.spr" );
		scale = 0.2f;
	break;

	case WEAPON_GAUSS:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash_alien1.spr" );
		scale = 0.2f;
	break;

	case WEAPON_AR2:
		if( type == -1 )
		{
			modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash_alien2.spr" );
			dieoffset = 0.25f;
			scale = 0.2f;
		}
		else
		{
			modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash_alien1.spr" );
			scale = 0.15f;
		}
	break;

	default:
	break;
	}
}

void R_MuzzleFlash( const cl_entity_t *e, int Attachment, int type )
{
	if( RI->params & RP_SHADOWPASS )
		return;
	
	TEMPENTITY *pTemp;
	int modelIndex;
	int	flags = 0;
	float scale;
	int Aiment = e->index;

	if( RP_NORMALPASS() )
	{
		if( e == GET_VIEWMODEL() )
		{
			if( gHUD.IsZoomed )
				return;
			flags |= EF_NOREFLECT;
			Aiment = -1; // diffusion HACKHACK - so the muzzleflash for viewmodel would be recognized properly
		}
		else if( RP_LOCALCLIENT( e ) )
			flags |= EF_REFLECTONLY;
	}
	else
	{
		if( RP_LOCALCLIENT( e ) )
			flags |= EF_REFLECTONLY;
	}

	float dieoffset = 0.0f;

	// I specify it...
	if( e == GET_VIEWMODEL() )
		GetMuzzleflashSprite( e, type, modelIndex, scale, dieoffset );
	else // choose default parameters, the old way
	{
		if( type == -1 ) // this is a secondary attack, skip it... (specified in qc)
			return;
		
		modelIndex = (type % 10) % MAX_MUZZLEFLASH;
		scale = (type / 10) * 0.05f;
		switch( modelIndex )
		{
		default:
		case 0: modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash1.spr" ); break;
		case 1: modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash2.spr" ); break;
		case 3: modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash3.spr" ); break;
		}
	}

	if( !modelIndex ) return;
	
	// must set position for right culling on render
	if( !(pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh( (float *)&e->attachment[Attachment], IEngineStudio.GetModelByIndex( modelIndex ) )) )
		return;

	pTemp->flags |= FTENT_SPRANIMATE;
	if( flags & EF_REFLECTONLY )
		pTemp->entity.curstate.rendermode = kRenderTransAdd; // glow muzzleflash doesn't appear in mirror. wtf?
	else
		pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->die = tr.time + 0.05f + dieoffset; // die at next frame
	pTemp->entity.angles[2] = RANDOM_LONG( 0, 359 );
	pTemp->entity.curstate.scale = scale;
	pTemp->entity.curstate.framerate = 50;

	// muzzleflash must be properly attached to the model, for viewmodel I use hack "-1 for aiment"
	pTemp->entity.curstate.aiment = Aiment;
	pTemp->entity.curstate.movetype = MOVETYPE_FOLLOW;
	pTemp->entity.curstate.body = Attachment + 1;

	gEngfuncs.CL_CreateVisibleEntity( ET_TEMPENTITY, &pTemp->entity );
	pTemp->entity.curstate.effects |= flags; // CL_CreateVisibleEntity clears 'effects' field, so we need to add it here
}

/*
=========================
HUD_StudioEvent

The entity's studio model description indicated an event was
fired during this frame, handle the event by it's tag ( e.g., muzzleflash, sound )
=========================
*/
void HUD_StudioEvent( const struct mstudioevent_s* event, const struct cl_entity_s* entity )
{
	if( RI->params & RP_SHADOWPASS )
		return;
	
	Vector velocity = entity->curstate.velocity;
	int WeaponID = 0;

	if( entity == GET_VIEWMODEL() )
	{
		velocity = gEngfuncs.GetLocalPlayer()->curstate.velocity;
		WeaponID = gHUD.m_Ammo.WeaponID;
	}
	Vector forw;

	switch( event->event )
	{
	case 5001:
		R_MuzzleFlash( entity, 0, Q_atoi( event->options ));
		R_MuzzleDynLight( entity, (float *)&entity->attachment[0], WeaponID );
		g_pParticles.GunSmoke( 0, (float *)&entity->attachment[0], velocity, WeaponID );
		if( WeaponID == WEAPON_SHOTGUN || WeaponID == WEAPON_SHOTGUN_XM )
			g_pParticles.CreateEffect( 0, "env_spark.part2", entity->attachment[0], g_vecZero );
		break;
	case 5002:
		if( !(RI->params & RP_SHADOWPASS) )
			gEngfuncs.pEfxAPI->R_SparkEffect( (float *)&entity->attachment[0], Q_atoi( event->options ), -100, 100 );
		break;
	case 5003: // muzzleflash with no light
		R_MuzzleFlash( entity, 0, Q_atoi( event->options ) );
		g_pParticles.GunSmoke( 0, (float *)&entity->attachment[0], velocity, WeaponID );
		break;
	case 5004: // client side sound		
		gEngfuncs.pfnPlaySoundByNameAtLocation( (char *)event->options, 1.0, (float *)&entity->attachment[0] );
		break;
	case 5005: // client side sound with random pitch		
		gEngfuncs.pEventAPI->EV_PlaySound( entity->index, (float *)&entity->attachment[0], CHAN_WEAPON, (char *)event->options,
			RANDOM_FLOAT( 0.7f, 0.9f ), ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );
		break;
	case 5006: // only smoke
		g_pParticles.GunSmoke( 0, (float *)&entity->attachment[0], velocity, WeaponID );
		break;
	case 5011:
		R_MuzzleFlash( entity, 1, Q_atoi( event->options ) );
		R_MuzzleDynLight( entity, (float *)&entity->attachment[1], WeaponID );
		g_pParticles.GunSmoke( 0, (float *)&entity->attachment[1], velocity, WeaponID );
		break;
	case 5021:
		R_MuzzleFlash( entity, 2, Q_atoi( event->options ) );
		R_MuzzleDynLight( entity, (float *)&entity->attachment[2], WeaponID );
		g_pParticles.GunSmoke( 0, (float *)&entity->attachment[2], velocity, WeaponID );
		break;
	case 5031:
		R_MuzzleFlash( entity, 3, Q_atoi( event->options ) );
		R_MuzzleDynLight( entity, (float *)&entity->attachment[3], WeaponID );
		g_pParticles.GunSmoke( 0, (float *)&entity->attachment[3], velocity, WeaponID );
		break;
	case 5040:
		// make aurora for origin
		UTIL_CreateAurora( (cl_entity_t*)entity, event->options, 0, 0.0f );
		break;
	case 5041:
		// make aurora for attachment #1
		UTIL_CreateAurora( (cl_entity_t*)entity, event->options, 1, 0.0f );
		break;
	case 5042:
		// make aurora for attachment #2
		UTIL_CreateAurora( (cl_entity_t*)entity, event->options, 2, 0.0f );
		break;
	case 5043:
		// make aurora for attachment #3
		UTIL_CreateAurora( (cl_entity_t*)entity, event->options, 3, 0.0f );
		break;
	case 5044:
		// make aurora for attachment #4
		UTIL_CreateAurora( (cl_entity_t*)entity, event->options, 4, 0.0f );
		break;
	case 5050: // Special event for displacer
		HUD_DrawBeam();
		break;
	case 5060:
		HUD_EjectShell( event, entity );
		break;
	case 5070:
		R_EmptyClip( WeaponID );
		break;
	default:
		ALERT( at_aiconsole, "Unknown event %i with options %i\n", event->event, event->options );
		break;
	}
}

/*
========================
LoadHeightMap

parse heightmap pixels and remap it
to real layer count
========================
*/
bool LoadHeightMap( indexMap_t* im, int numLayers )
{
	unsigned int* src;
	int		i, tex;
	int		depth = 1;

	if( numLayers <= 0 )
		return false;

	// loading heightmap and keep the source pixels
	if( !(tex = LOAD_TEXTURE( im->name, NULL, 0, TF_KEEP_SOURCE | TF_EXPAND_SOURCE )) )
		return false;

	if( (src = (unsigned int*)GET_TEXTURE_DATA( tex )) == NULL )
	{
		ALERT( at_error, "LoadHeightMap: couldn't get source pixels for %s\n", im->name );
		FREE_TEXTURE( tex );
		return false;
	}

	im->gl_diffuse_id = LOAD_TEXTURE( im->diffuse, NULL, 0, 0 );

	int width = RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, tex );
	int height = RENDER_GET_PARM( PARM_TEX_SRC_HEIGHT, tex );

	im->pixels = (byte*)Mem_Alloc( width * height );
	im->numLayers = bound( 1, numLayers, 255 );
	im->height = height;
	im->width = width;

	for( i = 0; i < (im->width * im->height); i++ )
	{
		byte rawHeight = (src[i] & 0xFF);
		im->maxHeight = Q_max( (16 * (int)ceil( rawHeight / 16 )), im->maxHeight );
	}

	// merge layers count
	im->numLayers = (im->maxHeight / 16) + 1;
	depth = Q_max( (int)Q_ceil( (float)im->numLayers / 4.0f ), 1 );

	// clamp to layers count
	for( i = 0; i < (im->width * im->height); i++ )
		im->pixels[i] = ((src[i] & 0xFF) * (im->numLayers - 1)) / im->maxHeight;

	size_t lay_size = im->width * im->height * 4;
	size_t img_size = lay_size * depth;
	byte* layers = (byte*)Mem_Alloc( img_size );
	byte* pixels = (byte*)src;

	for( int x = 0; x < im->width; x++ )
	{
		for( int y = 0; y < im->height; y++ )
		{
			float weights[MAX_LANDSCAPE_LAYERS];

			memset( weights, 0, sizeof( weights ) );

			for( int pos_x = 0; pos_x < FILTER_SIZE; pos_x++ )
			{
				for( int pos_y = 0; pos_y < FILTER_SIZE; pos_y++ )
				{
					int img_x = (x - (FILTER_SIZE / 2) + pos_x + im->width) % im->width;
					int img_y = (y - (FILTER_SIZE / 2) + pos_y + im->height) % im->height;

					float rawHeight = (float)(src[img_y * im->width + img_x] & 0xFF);
					float curLayer = (rawHeight * (im->numLayers - 1)) / (float)im->maxHeight;

					if( curLayer != (int)curLayer )
					{
						byte layer0 = (int)floor( curLayer );
						byte layer1 = (int)ceil( curLayer );
						float factor = curLayer - (int)curLayer;
						weights[layer0] += (1.0 - factor) * (1.0 / (FILTER_SIZE * FILTER_SIZE));
						weights[layer1] += (factor) * (1.0 / (FILTER_SIZE * FILTER_SIZE));
					}
					else
					{
						weights[(int)curLayer] += (1.0 / (FILTER_SIZE * FILTER_SIZE));
					}
				}
			}

			// encode layers into RGBA channels
			layers[lay_size * 0 + (y * im->width + x) * 4 + 0] = weights[0] * 255;
			layers[lay_size * 0 + (y * im->width + x) * 4 + 1] = weights[1] * 255;
			layers[lay_size * 0 + (y * im->width + x) * 4 + 2] = weights[2] * 255;
			layers[lay_size * 0 + (y * im->width + x) * 4 + 3] = weights[3] * 255;

			if( im->numLayers <= 4 ) continue;

			layers[lay_size * 1 + ((y * im->width + x) * 4 + 0)] = weights[4] * 255;
			layers[lay_size * 1 + ((y * im->width + x) * 4 + 1)] = weights[5] * 255;
			layers[lay_size * 1 + ((y * im->width + x) * 4 + 2)] = weights[6] * 255;
			layers[lay_size * 1 + ((y * im->width + x) * 4 + 3)] = weights[7] * 255;

			if( im->numLayers <= 8 ) continue;

			layers[lay_size * 2 + ((y * im->width + x) * 4 + 0)] = weights[8] * 255;
			layers[lay_size * 2 + ((y * im->width + x) * 4 + 1)] = weights[9] * 255;
			layers[lay_size * 2 + ((y * im->width + x) * 4 + 2)] = weights[10] * 255;
			layers[lay_size * 2 + ((y * im->width + x) * 4 + 3)] = weights[11] * 255;

			if( im->numLayers <= 12 ) continue;

			layers[lay_size * 3 + ((y * im->width + x) * 4 + 0)] = weights[12] * 255;
			layers[lay_size * 3 + ((y * im->width + x) * 4 + 1)] = weights[13] * 255;
			layers[lay_size * 3 + ((y * im->width + x) * 4 + 2)] = weights[14] * 255;
			layers[lay_size * 3 + ((y * im->width + x) * 4 + 3)] = weights[15] * 255;
		}
	}

	// release source texture
	FREE_TEXTURE( tex );

	tex = CREATE_TEXTURE_ARRAY( im->name, im->width, im->height, depth, layers, TF_CLAMP | TF_HAS_ALPHA );
	Mem_Free( layers );

	im->gl_heightmap_id = tex;

	return true;
}

/*
========================
LoadTerrainLayers

loading all the landscape layers
into texture arrays
========================
*/
bool LoadTerrainLayers( layerMap_t* lm, int numLayers )
{
	char* texnames[MAX_LANDSCAPE_LAYERS];
	char* normalmaps[MAX_LANDSCAPE_LAYERS];
	char* ptr, buffer[1024];
	char* ptr_n, buffer_n[1024];
	size_t nameLen = 64;
	int	i;

	memset( buffer, 0, sizeof( buffer ) ); // list must be null terminated

	// initialize names array
	for( i = 0, ptr = buffer; i < MAX_LANDSCAPE_LAYERS; i++, ptr += nameLen )
		texnames[i] = ptr;

	memset( buffer_n, 0, sizeof( buffer_n ) );
	for( i = 0, ptr_n = buffer_n; i < MAX_LANDSCAPE_LAYERS; i++, ptr_n += nameLen )
		normalmaps[i] = ptr_n;

	// process diffuse textures
	int normalmap_count = 0;
	lm->gl_normalmap_id = 0;
	for( i = 0; i < numLayers; i++ )
	{
		Q_snprintf( texnames[i], nameLen, "textures/%s", lm->pathes[i] );

		// diffusion - search for texture id, copy name to material table, load parameters
		int tex_id = LOAD_TEXTURE( texnames[i], NULL, 0, 0 );
		char tex_name[64];
		COM_FileBase( lm->pathes[i], tex_name ); // strip extension
		Q_snprintf( tr.materials[tex_id].name, sizeof( tr.materials[tex_id].name ), "%s", tex_name );
		LoadMaterialSettingsForTexture( tex_id );
		lm->DynlightScale[i] = tr.materials[tex_id].DynlightScale;
		lm->GlossScale[i] = tr.materials[tex_id].GlossScale;
		lm->GlossSmoothness[i] = tr.materials[tex_id].GlossSmoothness;
		lm->EmbossScale[i] = tr.materials[tex_id].EmbossScale;

		if( tr.materials[tex_id].normalmap_name[0] != '\0' )
		{
			Q_snprintf( normalmaps[i], nameLen, "%s", tr.materials[tex_id].normalmap_name );
			normalmap_count++;
		}
	}

	if( (lm->gl_diffuse_id = LOAD_TEXTURE_ARRAY( (const char**)texnames, 0 )) == 0 )
		return false;

	// make sure all layers have normalmaps
	if( normalmap_count == numLayers )
		lm->gl_normalmap_id = LOAD_TEXTURE_ARRAY( (const char **)normalmaps, TF_NORMALMAP );
	else if( normalmap_count > 0 && normalmap_count < numLayers )
		ConPrintf( "^3Warning:^7 not all landscape layers have normalmaps!\n" );

	return true;
}

/*
========================
R_FreeLandscapes

free the landscape definitions
========================
*/
void R_FreeLandscapes( void )
{
	for( int i = 0; i < world->num_terrains; i++ )
	{
		terrain_t* terra = &world->terrains[i];
		indexMap_t* im = &terra->indexmap;
		layerMap_t* lm = &terra->layermap;

		if( im->pixels ) Mem_Free( im->pixels );

		if( lm->gl_diffuse_id )
			FREE_TEXTURE( lm->gl_diffuse_id );

		if( lm->gl_normalmap_id )
			FREE_TEXTURE( lm->gl_normalmap_id );

		if( im->gl_diffuse_id )
			FREE_TEXTURE( im->gl_diffuse_id );

		FREE_TEXTURE( im->gl_heightmap_id );
	}

	if( world->terrains )
		Mem_Free( world->terrains );
	world->num_terrains = 0;
	world->terrains = NULL;
}

/*
========================
R_LoadLandscapes

load the landscape definitions
========================
*/
void R_LoadLandscapes( const char* filename )
{
	char filepath[256];

	Q_snprintf( filepath, sizeof( filepath ), "maps/%s_land.txt", filename );

	char* afile = (char*)gEngfuncs.COM_LoadFile( filepath, 5, NULL );
	if( !afile ) return;

	ALERT( at_aiconsole, "loading %s\n", filepath );

	char* pfile = afile;
	char token[256];
	int depth = 0;

	// count materials
	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( Q_strlen( token ) > 1 )
			continue;

		if( token[0] == '{' )
		{
			depth++;
		}
		else if( token[0] == '}' )
		{
			world->num_terrains++;
			depth--;
		}
	}

	if( depth > 0 ) ALERT( at_warning, "%s: EOF reached without closing brace\n", filepath );
	if( depth < 0 ) ALERT( at_warning, "%s: EOF reached without opening brace\n", filepath );

	world->terrains = (terrain_t*)Mem_Alloc( sizeof( terrain_t ) * world->num_terrains );
	pfile = afile; // start real parsing

	int current = 0;

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( current >= world->num_terrains )
		{
			ALERT( at_error, "landscape parse is overrun %d > %d\n", current, world->num_terrains );
			break;
		}

		terrain_t* terra = &world->terrains[current];

		// read the landscape name
		Q_strncpy( terra->name, token, sizeof( terra->name ) );
		terra->texScale = 1.0f;

		// read opening brace
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( token[0] != '{' )
		{
			ALERT( at_error, "found %s when expecting {\n", token );
			break;
		}

		while( pfile != NULL )
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "EOF without closing brace\n" );
				goto land_getout;
			}

			// description end goto next material
			if( token[0] == '}' )
			{
				current++;
				break;
			}
			else if( !Q_stricmp( token, "indexMap" ) )
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'indexMap'\n" );
					goto land_getout;
				}

				Q_strncpy( terra->indexmap.name, token, sizeof( terra->indexmap.name ) );
			}
			else if( !Q_stricmp( token, "diffuseMap" ) )
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'diffuseMap'\n" );
					goto land_getout;
				}

				Q_strncpy( terra->indexmap.diffuse, token, sizeof( terra->indexmap.diffuse ) );
			}
			else if( !Q_strnicmp( token, "layer", 5 ) )
			{
				int	layerNum = Q_atoi( token + 5 );

				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'layer'\n" );
					goto land_getout;
				}

				if( layerNum < 0 || layerNum >( MAX_LANDSCAPE_LAYERS - 1 ) )
				{
					ALERT( at_error, "%s is out of range. Ignored\n", token );
				}
				else
				{
					Q_strncpy( terra->layermap.pathes[layerNum], token, sizeof( terra->layermap.pathes[0] ) );
					COM_FileBase( token, terra->layermap.names[layerNum] );
				}

				terra->numLayers = Q_max( terra->numLayers, layerNum + 1 );
			}
			else if( !Q_stricmp( token, "tessSize" ) )
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'tessSize'\n" );
					goto land_getout;
				}
			}
			else if( !Q_stricmp( token, "texScale" ) )
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'texScale'\n" );
					goto land_getout;
				}

				terra->texScale = Q_atof( token );
				terra->texScale = 1.0 / (bound( 0.000001f, terra->texScale, 16.0f ));
			}
			else ALERT( at_warning, "Unknown landscape token %s\n", token );
		}

		if( LoadHeightMap( &terra->indexmap, terra->numLayers ) )
		{
			// NOTE: layers may be missed
			LoadTerrainLayers( &terra->layermap, terra->numLayers );
			terra->valid = true; // all done
		}
	}

land_getout:
	gEngfuncs.COM_FreeFile( afile );
	ALERT( at_console, "%d landscapes parsed\n", current );
}

/*
========================
R_FindTerrain

find the terrain description
========================
*/
terrain_t* R_FindTerrain( const char* texname )
{
	for( int i = 0; i < world->num_terrains; i++ )
	{
		if( !Q_stricmp( texname, world->terrains[i].name ) && world->terrains[i].valid )
			return &world->terrains[i];
	}

	return NULL;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
void ED_ParseEdict( char** pfile )
{
	int	vertex_light_cache = -1;
	char	modelname[64];
	char	token[2048];

	// go through all the dictionary pairs
	while( 1 )
	{
		char	keyname[256];

		// parse key
		if( (*pfile = COM_ParseFile( *pfile, token )) == NULL )
			HOST_ERROR( "ED_ParseEdict: EOF without closing brace\n" );

		if( token[0] == '}' ) break; // end of desc

		Q_strncpy( keyname, token, sizeof( keyname ) );

		// parse value	
		if( (*pfile = COM_ParseFile( *pfile, token )) == NULL )
			HOST_ERROR( "ED_ParseEdict: EOF without closing brace\n" );

		if( token[0] == '}' )
			HOST_ERROR( "ED_ParseEdict: closing brace without data\n" );

		// ignore attempts to set key ""
		if( !keyname[0] ) continue;

		// ignore attempts to set value ""
		if( !token[0] ) continue;

		// only two fields that we needed
		if( !Q_strcmp( keyname, "model" ) )
			Q_strncpy( modelname, token, sizeof( modelname ) );

		if( !Q_strcmp( keyname, "vlight_cache" ) )
			vertex_light_cache = Q_atoi( token );
	}

	if( vertex_light_cache <= 0 || vertex_light_cache >= MAX_LIGHTCACHE )
		return;

	// deal with light cache
	g_StudioRenderer.CreateMeshCacheVL( modelname, vertex_light_cache - 1 );
}

/*
==================
GL_InitVertexLightCache

create VBO cache for vertex-lit studio models
==================
*/
void GL_InitVertexLightCache( void )
{
	char* entities = worldmodel->entities;
	static char	worldname[64];
	char		token[2048];

	if( !Q_stricmp( world->name, worldname ) && !world->ignore_restart_check )
		return; // just a restart

	world->ignore_restart_check = false;
	Q_strncpy( worldname, world->name, sizeof( worldname ) );

	RI->currententity = GET_ENTITY( 0 ); // to have something valid here

	// parse ents to find vertex light cache
	while( (entities = COM_ParseFile( entities, token )) != NULL )
	{
		if( token[0] != '{' )
			HOST_ERROR( "ED_LoadFromFile: found %s when expecting {\n", token );

		ED_ParseEdict( &entities );
	}
}

/*
==================
R_NewMap

Called always when map is changed or restarted
==================
*/
void R_NewMap( void )
{
	int	i, j;
	model_t* m;

	if( g_pParticleSystems )
		g_pParticleSystems->ClearSystems();

	g_pParticles.Clear();

	if( !g_fRenderInitialized )
		return;

	// reset some world variables
	for( i = 1; i < MAX_MODELS; i++ )
	{
		if( (m = IEngineStudio.GetModelByIndex( i )) == NULL )
			continue;

		if( m->name[0] == '*' || m->type != mod_brush )
			continue;

		RI->currentmodel = m;

		for( j = 0; j < m->numsurfaces; j++ )
		{
			msurface_t* fa = m->surfaces + j;
			texture_t* tex = fa->texinfo->texture;
			mextrasurf_t* info = fa->info;

			memset( info->subtexture, 0, sizeof( info->subtexture ) );
			info->checkcount = -1;
		}
	}

	// clear weather system
	R_ResetWeather();

	CL_ClearPlights();

	// don't flush shaders for each map - save time to recompile
	if( num_glsl_programs >= (MAX_GLSL_PROGRAMS * 0.75f) )
		GL_FreeUberShaders();

	tr.realframecount = 0;
	RI->viewleaf = NULL; // it's may be data from previous map

	// setup the skybox sides
	for( i = 0; i < 6; i++ )
		tr.skyboxTextures[i] = RENDER_GET_PARM( PARM_TEX_SKYBOX, i );

	v_intermission_spot = NULL;
	//	tr.glsl_valid_sequence++; // refresh shader cache
	tr.num_cin_used = 0;

	g_StudioRenderer.VidInit();

	GL_InitVertexLightCache();

	if( tr.buildtime > 0.0 )
	{
		ALERT( at_aiconsole, "total build time %g\n", tr.buildtime );
		tr.buildtime = 0.0;
	}

	// diffusion - reset time
	tr.time = GET_CLIENT_TIME();
	tr.oldtime = GET_CLIENT_OLDTIME();

	// setup special flags
	for( int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *info = surf->info;

		// clear the in-game set flags
		ClearBits( surf->flags, SURF_NODRAW | SURF_NODLIGHT );

		memset( info->subtexture, 0, sizeof( info->subtexture ) );
		info->cubemap = NULL;
		info->checkcount = -1;
	}

	// we need to reapply cubemaps to surfaces after restart level
	if( world->num_cubemaps > 0 )
		world->loading_cubemaps = true;

	memset( tr.cableSwayIntensity, 0.0f, sizeof( tr.cableSwayIntensity ) );
	memset( tr.cableSwayPhase, 0.0f, sizeof( tr.cableSwayPhase ) );

	gHUD.m_ScreenEffects.SaveIcon_Reset = true;
	gHUD.m_ScreenEffects.ShouldDrawGameSaved = false;

	tr.pDrone = NULL;

	for( int i = 0; i < 8192; i++ )
	{
		if( tr.studio_screen_tex[i] > 0 )
			FREE_TEXTURE( tr.studio_screen_tex[i] );
	}

	memset( tr.studio_screen_tex, 0, sizeof( tr.studio_screen_tex ) );
}

void R_VidInit( void )
{
	int	i;

	if( !g_fRenderInitialized )
		return;

	// get the actual screen size
	glState.width = RENDER_GET_PARM( PARM_SCREEN_WIDTH, 0 );
	glState.height = RENDER_GET_PARM( PARM_SCREEN_HEIGHT, 0 );

	// release old subview textures
	for( i = 0; i < MAX_SUBVIEW_TEXTURES; i++ )
	{
		if( !tr.subviewTextures[i].texturenum ) break;
		FREE_TEXTURE( tr.subviewTextures[i].texturenum );
	}

	for( i = 0; i < tr.num_framebuffers; i++ )
	{
		if( !tr.frame_buffers[i].init ) break;
		R_FreeFrameBuffer( i );
	}

	memset( tr.subviewTextures, 0, sizeof( tr.subviewTextures ) );
	memset( tr.frame_buffers, 0, sizeof( tr.frame_buffers ) );

	R_ResetShadowTextures();

	tr.nullmodelindex = 0;
	tr.num_framebuffers = 0;
	tr.num_subview_used = 0;
	tr.reset_gamma_frame = 0;
	tr.glsl_valid_sequence++; // refresh shader cache

	InitPostTextures();

//	R_InitBloomTextures();

	g_StudioRenderer.VidInit();
}

void R_Sprite_Smoke( TEMPENTITY *pTemp, float scale, int mode )
{
	int	iColor;

	if( !pTemp ) return;

	iColor = RANDOM_LONG( 20, 35 );
	if( mode == 1 )
		pTemp->entity.curstate.rendermode = kRenderTransAdd;
	else
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 30;
	pTemp->entity.curstate.rendercolor.r = iColor;
	pTemp->entity.curstate.rendercolor.g = iColor;
	pTemp->entity.curstate.rendercolor.b = iColor;
	pTemp->entity.origin[2] += 20;
	pTemp->entity.curstate.scale = scale;
}

void R_BubbleTrail( const Vector start, const Vector end, float height, int count, float speed )
{
	float dist;
	Vector origin;
	int	i;

	for( i = 0; i < count; i++ )
	{
		dist = RANDOM_FLOAT( 0, 1.0 );
		VectorLerp( start, dist, end, origin );
		float vertical_speed = RANDOM_LONG( 80, 140 );

		g_pParticles.Bubble( 0, origin, vertical_speed, 1000, tr.time + ((height - (origin.z - start.z)) / vertical_speed) - 0.1f, 0 );
	}
}

void R_Bubbles( const Vector mins, const Vector maxs, float height, int count, float speed )
{
	Vector origin;
	int i;

	for( i = 0; i < count; i++ )
	{
		origin.x = RANDOM_LONG( mins.x, maxs.x );
		origin.y = RANDOM_LONG( mins.y, maxs.y );
		origin.z = RANDOM_LONG( mins.z, maxs.z );

		float vertical_speed = RANDOM_LONG( 80, 140 );

		g_pParticles.Bubble( 0, origin, vertical_speed, 1000, tr.time + ((height - (origin.z - mins.z)) / vertical_speed) - 0.1f, 0 );
	}
}

void R_MakeWaterSplash( Vector vecSrc, Vector vecEnd, int Type )
{
	if( !(POINT_CONTENTS( vecEnd ) == CONTENTS_WATER && POINT_CONTENTS( vecSrc ) != CONTENTS_WATER) )
		return;

	float len = (vecEnd - vecSrc).Length();

	// Делим пополам
	Vector vecTemp = Vector( (vecEnd.x + vecSrc.x) * 0.5f, (vecEnd.y + vecSrc.y) * 0.5f, (vecEnd.z + vecSrc.z) * 0.5f );

	if( len <= 1 )
	{
		vecTemp.z += 4; // !!! have to do this or particle won't spawn...

		switch( Type )
		{
		case 0:
			g_pParticles.WaterRingParticle( 0, vecTemp );
			g_pParticles.CreateEffect( 0, "very_small_splash", vecTemp, g_vecZero );
			break;
		case 1:
			g_pParticles.WaterRingParticle( 0, vecTemp );
			g_pParticles.CreateEffect( 0, "small_splash", vecTemp, g_vecZero );
			break;
		case 2:
			g_pParticles.WaterRingParticle( 0, vecTemp );
			g_pParticles.CreateEffect( 0, "big_splash", vecTemp, g_vecZero );

			if( (tr.viewparams.vieworg - vecTemp).Length() < 200 )
			{
				gHUD.ScreenDrips_OverrideTime = tr.time + 2;
				gHUD.ScreenDrips_CurVisibility = 1.0f;
				gHUD.ScreenDrips_DripIntensity = 0.75f;
				gHUD.ScreenDrips_Visible = true;
			}
			break;
		case 3: // small drip
			g_pParticles.WaterRingParticle( 0, vecTemp, 0.25f );
			break;
		}
	}
	else
	{
		if( POINT_CONTENTS( vecTemp ) == CONTENTS_WATER )
			R_MakeWaterSplash( vecSrc, vecTemp, Type );
		else
			R_MakeWaterSplash( vecTemp, vecEnd, Type );
	}
}

void R_Explosion( Vector pos, int model, float scale, float framerate, int flags )
{	
	const char *cl_explode_sounds[] =
	{
		"weapons/explode3.wav",
		"weapons/explode4.wav",
		"weapons/explode5.wav",
	};

	const char *cl_explode_distant_sounds[] =
	{
		"weapons/explode3_dist.wav",
		"weapons/explode4_dist.wav",
		"weapons/explode5_dist.wav",
	};

	if( scale != 0.0f )
	{	
		if( !FBitSet( flags, TE_EXPLFLAG_NOPARTICLES ) )
			g_pParticles.ExplosionParticles( 0, pos );

		// create explosion sprite
		// diffusion - BUGBUG: particles must go before R_Sprite_Explode, otherwise they are not visible!
	//	gEngfuncs.pEfxAPI->R_Sprite_Explode( gEngfuncs.pEfxAPI->R_DefaultSprite( pos, model, framerate ), scale, flags );
		
		if( !FBitSet( flags, TE_EXPLFLAG_NODLIGHTS ) )
		{
			plight_t *dl;

			// big flash
			dl = CL_AllocPlight( 0 );
			dl->pointlight = true;
			dl->projectionTexture = tr.whiteCubeTexture;
			if( r_shadowquality->value < 2 )
				dl->flags |= CF_NOSHADOWS;
			dl->radius = 300;
			dl->color.r = 250;
			dl->color.g = 250;
			dl->color.b = 150;
			dl->die = tr.time + 0.05f;
			dl->decay = 80;
			dl->brightness = 1.5;
			R_SetupLightProjection( dl, pos, g_vecZero, dl->radius, 90.0f );
			R_SetupLightAttenuationTexture( dl );

			// red glow
			dl = CL_AllocPlight( 0 );
			dl->pointlight = true;
			dl->projectionTexture = tr.whiteCubeTexture;
			dl->flags |= CF_NOSHADOWS | CF_NOGRASSLIGHTING;
			dl->radius = 150;
			dl->color.r = 255;
			dl->color.g = 190;
			dl->color.b = 40;
			dl->die = tr.time + 1.0f;
			dl->decay = 200;
			R_SetupLightProjection( dl, pos, g_vecZero, dl->radius, 90.0f );
			R_SetupLightAttenuationTexture( dl );

			int SprGlow = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/glow01.spr" );
			TEMPENTITY *pTemp = gEngfuncs.pEfxAPI->R_TempSprite( pos, Vector( 0, 0, 0 ), RANDOM_FLOAT( 5, 8 ), SprGlow, kRenderGlow, kRenderFxNoDissipation, 0.5, 0.1, FTENT_FADEOUT );
			if( pTemp )
			{
				pTemp->entity.curstate.rendercolor.r = 250;
				pTemp->entity.curstate.rendercolor.g = 100;
				pTemp->entity.curstate.rendercolor.b = 0;
			}
		}
	}

	if( flags != TE_EXPLFLAG_NOSOUND )
	{
		int sndnum = RANDOM_LONG( 0, 2 );

		gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_STATIC, cl_explode_sounds[sndnum], VOL_NORM, 0.5, 0, PITCH_NORM );
		int Length = (pos - tr.viewparams.vieworg).Length();
		if( Length > 1000 )
		{
			float vol = (Length * 0.001) - 0.8; // should be 0.2 and above because starting from 1000
			vol = bound( 0.1, vol, 1 );
			gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_STATIC, cl_explode_distant_sounds[sndnum], VOL_NORM, 0.1, 0, PITCH_NORM );
		}
	}

	R_MakeWaterSplash( pos + Vector( 0, 0, 512 ), pos, 2 );
}

//==========================================================================================
// diffusion - an attempt to emulate "distant sound" effect for weapons, since there's no other choice
// (or I don't know how to do it)
//==========================================================================================
void R_ClientSound( Vector pos, int entindex, int sndnum, int type, int LowAmmoVolume )
{
	char *sndname[5] = { NULL, NULL, NULL, NULL, NULL };
	char *sndname_d[5] = { NULL, NULL, NULL, NULL, NULL };
	int num = -1;
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	int Channel = CHAN_WEAPON;
	bool localanim = (LocalWeaponAnims() && UTIL_IsLocal( entindex ) && entindex == player->index && !CL_IsThirdPerson());
	float attenuation = 0.6f;

	switch( sndnum )
	{
	case WEAPON_BERETTA:
		if( localanim ) // shooting sound plays with client animation
			break;
		sndname[0] = "weapons/pistol1.wav";
		sndname[1] = "weapons/pistol2.wav";
		sndname[2] = "weapons/pistol3.wav";
		sndname_d[0] = "weapons/pistol1_d.wav";
		sndname_d[1] = "weapons/pistol2_d.wav";
		sndname_d[2] = "weapons/pistol3_d.wav";
		num = RANDOM_LONG( 0, 2 );
	break;
	case WEAPON_HKMP5:
		if( localanim )
			break;
		sndname[0] = "weapons/mp5-1.wav";
		sndname[1] = "weapons/mp5-2.wav";
		sndname[2] = "weapons/mp5-3.wav";
		sndname_d[0] = "weapons/mp5-1_d.wav";
		sndname_d[1] = "weapons/mp5-2_d.wav";
		sndname_d[2] = "weapons/mp5-3_d.wav";
		num = RANDOM_LONG( 0, 2 );
	break;
	case WEAPON_MRC:
		if( localanim )
			break;
		if( type == 1 )
		{
			sndname[0] = "weapons/glauncher.wav";
			sndname_d[0] = "weapons/glauncher_d.wav";
			num = 0;
		}
		else
		{
			sndname[0] = "weapons/hks1.wav";
			sndname[1] = "weapons/hks2.wav";
			sndname[2] = "weapons/hks3.wav";
			sndname_d[0] = "weapons/hks1_d.wav";
			sndname_d[1] = "weapons/hks2_d.wav";
			sndname_d[2] = "weapons/hks3_d.wav";
			num = RANDOM_LONG( 0, 2 );
		}
	break;
	case WEAPON_AR2:
		if( localanim )
			break;
		if( type == 1 )
		{
			sndname[0] = "weapons/ar2_secondary.wav";
			sndname_d[0] = "weapons/ar2_secondary_d.wav";
			num = 0;
		}
		else
		{
			sndname[0] = "drone/aliendrone_shoot1.wav";
			sndname[1] = "drone/aliendrone_shoot2.wav";
			sndname[2] = "drone/aliendrone_shoot3.wav";
			sndname[3] = "drone/aliendrone_shoot4.wav";
			sndname_d[0] = "drone/aliendrone_shoot1_d.wav";
			sndname_d[1] = "drone/aliendrone_shoot2_d.wav";
			sndname_d[2] = "drone/aliendrone_shoot3_d.wav";
			sndname_d[3] = "drone/aliendrone_shoot4_d.wav";
			num = RANDOM_LONG( 0, 3 );
		}
	break;
	case WEAPON_CROSSBOW:
		if( localanim )
			break;
		sndname[0] = "weapons/xbow_fire1.wav";
		num = 0;
	break;
	case WEAPON_DEAGLE:
		if( localanim )
			break;
		sndname[0] = "weapons/deagle_shot1.wav";
		sndname_d[0] = "weapons/deagle_shot1_d.wav";
		num = 0;
	break;
	case WEAPON_FIVESEVEN:
		if( localanim )
			break;
		sndname[0] = "weapons/fiveseven_shoot.wav";
		sndname_d[0] = "weapons/fiveseven_shoot_d.wav";
		num = 0;
	break;
	case WEAPON_G36C:
		if( localanim )
			break;
		sndname[0] = "weapons/g36c_shoot.wav";
		num = 0;
		attenuation = 1.25f;
	break;
	case WEAPON_SHOTGUN:
		if( localanim )
			break;
		if( type == 1 )
		{
			sndname[0] = "weapons/shotgun_dbl.wav";
			sndname_d[0] = "weapons/shotgun_dbl_d.wav";
			num = 0;
		}
		else
		{
			sndname[0] = "weapons/shotgun_single.wav";
			sndname_d[0] = "weapons/shotgun_single_d.wav";
			num = 0;
		}
	break;
	case WEAPON_SHOTGUN_XM:
		if( localanim )
			break;
		sndname[0] = "weapons/xm_fire.wav";
		sndname_d[0] = "weapons/xm_fire_d.wav";
		num = 0;
	break;
	case WEAPON_SNIPER:
		if( localanim )
			break;
		sndname[0] = "weapons/sniper_wpn_fire.wav";
		sndname_d[0] = "weapons/sniper_wpn_fire_d.wav";
		num = 0;
	break;
	case WEAPON_GAUSS:
	//	if( localanim )
	//		break;
		sndname[0] = "weapons/gausniper.wav";
		sndname_d[0] = "weapons/gausniper_d.wav";
		num = 0;
		Channel = CHAN_ITEM; // use different channel to not interrupt it while charging
		break;
	case WEAPON_RPG:
		if( localanim )
			break;
		sndname[0] = "weapons/rocketfire1.wav";
		num = 0;
	break;
	case 248: // apc projectile shooting
		sndname[0] = "car/apc_fire.wav";
		sndname_d[0] = "car/apc_fire_d.wav";
		num = 0;
	break;
	case 249: // weapon switch sound
		sndname[0] = "common/wpn_select1.wav";
		sndname[1] = "common/wpn_select2.wav";
		sndname[2] = "common/wpn_select3.wav";
		Channel = CHAN_STATIC;
		num = RANDOM_LONG( 0, 2 );
	break;
	case 250: // ammo pickup sound
		sndname[0] = "items/9mmclip1.wav";
		Channel = CHAN_STATIC;
		num = 0;
	break;
	case 251: // gun pickup sound (or default)
		sndname[0] = "items/gunpickup2.wav";
		Channel = CHAN_STATIC;
		num = 0;
	break;
	case 252: // monster_human_grunt 9mmAR
		sndname[0] = "hgrunt/gr_mgun1.wav";
		sndname[1] = "hgrunt/gr_mgun2.wav";
		sndname_d[0] = "hgrunt/gr_mgun1_d.wav";
		sndname_d[1] = "hgrunt/gr_mgun2_d.wav";
		num = RANDOM_LONG( 0, 1 );
		break;
	case 253: // monster_security_drone
		sndname[0] = "drone/drone_shoot1.wav";
		sndname[1] = "drone/drone_shoot2.wav";
		sndname[2] = "drone/drone_shoot3.wav";
		sndname_d[0] = "drone/drone_shoot1_d.wav";
		sndname_d[1] = "drone/drone_shoot2_d.wav";
		sndname_d[2] = "drone/drone_shoot3_d.wav";
		num = RANDOM_LONG( 0, 2 );
	break;
	case 254: // monster_human_grunt shotgun
		sndname[0] = "weapons/shotgun_npc.wav";
		sndname_d[0] = "weapons/shotgun_npc_d.wav";
		num = 0;
	break;
	}

	if( num == -1 )
		return;

	int pitch = RANDOM_LONG( 98, 103 );

	// play normal sound
	if( sndname[num] != NULL )
		gEngfuncs.pEventAPI->EV_PlaySound( entindex, pos, Channel, sndname[num], VOL_NORM, attenuation, 0, pitch );

	// on the distance of 1000 units and more, start blending in a distant sound with effects
	if( sndname_d[num] != NULL )
	{
		int Length = (pos - tr.viewparams.vieworg).Length();
		if( Length > 1000 )
		{
			float vol = (Length * 0.001) - 0.8; // should be 0.2 and above because starting from 1000
			vol = bound( 0.1, vol, 1 );
			// must be different channel so both sounds can be heard
			gEngfuncs.pEventAPI->EV_PlaySound( entindex, pos, CHAN_STATIC, sndname_d[num], vol, 0.15, 0, pitch );
		}
	}

	if( LowAmmoVolume > 0 )
		PlayLowAmmoSound( entindex, pos, LowAmmoVolume );
}

//==========================================================================================
// diffusion - shake screen from shooting
//==========================================================================================
void R_MakeWeaponShake( int Weapon, int Mode, bool Override )
{
	if( LocalWeaponAnims() && !Override ) // skip server message if we already played shooting animation locally
		return;

	switch( Weapon )
	{
	case WEAPON_DEAGLE:
		gHUD.shake.amplitude = 2;
		gHUD.shake.frequency = 100;
		gHUD.shake.duration = 0.5;
		break;
	case WEAPON_SHOTGUN:
		if( Mode == 1 )
		{
			gHUD.shake.amplitude = 2;
			gHUD.shake.frequency = 100;
			gHUD.shake.duration = 0.5;
		}
		else
		{
			gHUD.shake.amplitude = 1;
			gHUD.shake.frequency = 80;
			gHUD.shake.duration = 0.5;
		}
		break;
	case WEAPON_SHOTGUN_XM:
		gHUD.shake.amplitude = 1;
		gHUD.shake.frequency = 100;
		gHUD.shake.duration = 0.5;
		break;
	case WEAPON_AR2:
		if( Mode == 1 )
		{
			gHUD.shake.amplitude = 12;
			gHUD.shake.frequency = 150;
			gHUD.shake.duration = 0.5;
		}
		else
		{
			gHUD.shake.amplitude = 2;
			gHUD.shake.frequency = 100;
			gHUD.shake.duration = 0.2;
		}
		break;
	case WEAPON_MRC:
		if( Mode == 1 )
		{
			gHUD.shake.amplitude = 5;
			gHUD.shake.frequency = 150;
			gHUD.shake.duration = 0.5;
		}
		else
		{
			gHUD.shake.amplitude = 2;
			gHUD.shake.frequency = 100;
			gHUD.shake.duration = 0.2;
		}
		break;
	case WEAPON_G36C:
		gHUD.shake.amplitude = 1;
		gHUD.shake.frequency = 100;
		gHUD.shake.duration = 0.2;
		break;
//	case WEAPON_GAUSS:
//		gHUD.shake.amplitude = 4;
//		gHUD.shake.frequency = 100;
//		gHUD.shake.duration = 0.2;
//		break;
	case WEAPON_HKMP5:
		gHUD.shake.amplitude = 1;
		gHUD.shake.frequency = 100;
		gHUD.shake.duration = 0.2;
		break;
	case WEAPON_SNIPER:
		gHUD.shake.amplitude = 5;
		gHUD.shake.frequency = 100;
		gHUD.shake.duration = 0.7;
		break;
	default:
		return;
	}

	gHUD.shake.time = tr.time + Q_max( gHUD.shake.duration, 0.01f );
	gHUD.shake.next_shake = 0.0f; // apply immediately
}

void PlayLowAmmoSound( int entindex, Vector origin, int Volume )
{
	if( Volume <= 0 )
		return;

	float vol = 0.25f + (2.0f / Volume);
	if( vol > 1.0f ) vol = 1.0f;

	gEngfuncs.pEventAPI->EV_PlaySound( entindex, origin, CHAN_STATIC, "weapons/lowammo.wav", vol, 1.75, 0, RANDOM_LONG( 98, 103 ) );
}

TEMPENTITY *R_GunShell( const Vector pos, const Vector dir, const Vector angles, float life, int modelIndex, int body, int soundtype )
{
	// alloc a new tempent
	TEMPENTITY *pTemp;
	model_t *pmodel;

	if( (pmodel = MOD_HANDLE( modelIndex )) == NULL )
		return NULL;

	pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc( (float*)&pos, pmodel );
	if( !pTemp ) return NULL;

	pTemp->flags = (FTENT_COLLIDEALL | FTENT_GRAVITY | FTENT_COLLIDE_STUDIOIGNORE);
	pTemp->entity.baseline.origin = dir;
	pTemp->entity.angles = tr.cached_viewangles;

	// keep track of shell type
	switch( soundtype )
	{
	case TE_BOUNCE_SHELL:
		pTemp->hitSound = BOUNCE_SHELL;
		pTemp->entity.baseline.angles[0] = RANDOM_FLOAT( -512, 511 );
		pTemp->entity.baseline.angles[2] = RANDOM_FLOAT( -255, 255 );
		pTemp->flags |= FTENT_ROTATE;
		break;
	case TE_BOUNCE_SHOTSHELL:
		pTemp->hitSound = BOUNCE_SHOTSHELL;
		pTemp->entity.baseline.angles[0] = RANDOM_FLOAT( -512, 511 );
		pTemp->entity.baseline.angles[2] = RANDOM_FLOAT( -255, 255 );
		pTemp->flags |= FTENT_ROTATE | FTENT_SLOWGRAVITY;
		break;
	}

	pTemp->entity.curstate.body = body;

	// disable shadows for gunshells
	pTemp->entity.baseline.renderfx = kRenderFxNoShadows;
	pTemp->entity.curstate.renderfx = kRenderFxNoShadows;

	// fade distance
	pTemp->entity.curstate.iuser4 = 500;

	pTemp->die = tr.time + life;

	return pTemp;
}

TEMPENTITY *R_EmptyClip( int WeaponID )
{
	// can't spawn anymore empty clips until we reload.
	if( gHUD.emptyclipspawned[WeaponID] )
		return NULL;
	
	// alloc a new tempent
	TEMPENTITY *pTemp;
	model_t *pmodel;
	int modelIndex = 0;

	// make sure the clip model is precached on server in the weapon code
	switch( WeaponID )
	{
	case WEAPON_G36C:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/weapons/w_g36c_clip.mdl" );
		break;
	case WEAPON_HKMP5:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/weapons/w_mp5_clip.mdl" );
		break;
	case WEAPON_DEAGLE:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/weapons/w_deagle_clip.mdl" );
		break;
	case WEAPON_BERETTA:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/weapons/w_beretta_clip.mdl" );
		break;
	case WEAPON_FIVESEVEN:
		modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/weapons/w_fiveseven_clip.mdl" );
		break;
	default:
		break;
	}

	if( modelIndex <= 0 )
		return NULL;

	if( (pmodel = MOD_HANDLE( modelIndex )) == NULL )
		return NULL;

	Vector pos, vel, forward, right, up;
	AngleVectors( tr.cached_viewangles, forward, right, up );
	pos = tr.viewparams.vieworg - up * 20 + forward * 10;
	vel = Vector( 0, 0, -100 ) + forward * 25;

	pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc( (float *)&pos, pmodel );
	if( !pTemp ) return NULL;

	pTemp->flags = (FTENT_COLLIDEALL | FTENT_GRAVITY | FTENT_ROTATE | FTENT_COLLIDE_STUDIOIGNORE);
	pTemp->entity.baseline.origin = vel;
	pTemp->entity.angles = tr.cached_viewangles;

	pTemp->hitSound = BOUNCE_EMPTYCLIP;
	pTemp->entity.baseline.angles[0] = RANDOM_FLOAT( -512, 511 );
	pTemp->entity.baseline.angles[2] = RANDOM_FLOAT( -255, 255 );

	pTemp->entity.curstate.body = 1; // empty clip

	// disable shadows for gun clips
	pTemp->entity.baseline.renderfx = kRenderFxNoShadows;
	pTemp->entity.curstate.renderfx = kRenderFxNoShadows;

	// fade distance
	pTemp->entity.curstate.iuser4 = 500;

	pTemp->die = tr.time + 180; // 3 minutes

	pTemp->entity.baseline.effects = -10; // this is a hack specified for dropped clip sound - see CL_TempEntPlaySound

	// clip has been spawned. make sure we don't spam them by skipping animation
	gHUD.emptyclipspawned[WeaponID] = true;

	return pTemp;
}