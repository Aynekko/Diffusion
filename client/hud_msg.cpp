/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  hud_msg.cpp
//
#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "r_weather.h"
#include "r_efx.h"
#include "event_api.h"
#include "r_quakeparticle.h"
#include "enginecallback.h"
#include "pm_materials.h"
#include "build.h"
#include "discord.h"

extern bool g_bDuckToggled;

// CHud message handlers
DECLARE_HUDMESSAGE( Logo );
DECLARE_HUDMESSAGE( HUDColor );
DECLARE_HUDMESSAGE( Weapons );
DECLARE_HUDMESSAGE( RainData );
DECLARE_HUDMESSAGE( SetBody );
DECLARE_HUDMESSAGE( SetSkin );
DECLARE_HUDMESSAGE( ResetHUD );
DECLARE_HUDMESSAGE( InitHUD );
DECLARE_HUDMESSAGE( ViewMode );
DECLARE_HUDMESSAGE( Particle );
DECLARE_HUDMESSAGE( KillPart );
DECLARE_HUDMESSAGE( SetFOV );
DECLARE_HUDMESSAGE( Concuss );
DECLARE_HUDMESSAGE( GameMode );
DECLARE_HUDMESSAGE( MusicFade );
DECLARE_HUDMESSAGE( WeaponAnim );
DECLARE_HUDMESSAGE( KillDecals );
DECLARE_HUDMESSAGE( StudioDecal );
DECLARE_HUDMESSAGE( SetupBones );
DECLARE_HUDMESSAGE( TempEnt );
DECLARE_HUDMESSAGE( WaterSplash );
DECLARE_HUDMESSAGE( ServerName );

int CHud :: InitHUDMessages( void )
{
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( Weapons );
	HOOK_MESSAGE( HUDColor );
	HOOK_MESSAGE( Particle );
	HOOK_MESSAGE( KillPart );
	HOOK_MESSAGE( RainData ); 
	HOOK_MESSAGE( SetBody );
	HOOK_MESSAGE( SetSkin );
	HOOK_MESSAGE( MusicFade );
	HOOK_MESSAGE( WeaponAnim );
	HOOK_MESSAGE( KillDecals );
	HOOK_MESSAGE( StudioDecal );
	HOOK_MESSAGE( SetupBones );
	HOOK_MESSAGE( TempEnt );
	HOOK_MESSAGE( WaterSplash );
	HOOK_MESSAGE( ServerName );

	m_flFOV = 0.0f;
	m_iHUDColor = 0x0046A9FF; // 70,169,255 for Diffusion
	
	CVAR_REGISTER( "zoom_sensitivity_ratio", "1.2", 0 );
	default_fov = CVAR_REGISTER( "default_fov", "90", FCVAR_ARCHIVE );
	m_pCvarDraw = CVAR_REGISTER( "hud_draw", "1", 0 );
	m_pSpriteList = NULL;

	// clear any old HUD list
	if( m_pHudList )
	{
		HUDLIST *pList;

		while( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	m_flTime = 1.0;

	return 1;
}

int CHud :: MsgFunc_ResetHUD( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	bool ResetHUD = (READ_BYTE() > 0);
	END_READ();

	if( !ResetHUD )
		return 1;
	
	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p )
			pList->p->Reset();
		pList = pList->pNext;
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	g_bDuckToggled = false;
	
	return 1;
}

int CHud :: MsgFunc_Logo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	// update logo data
	m_iLogo = READ_BYTE();

	END_READ();

	return 1;
}

int CHud :: MsgFunc_Weapons( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	// update weapon bits
	READ_BYTES( m_iWeaponBits, MAX_WEAPON_BYTES );

	END_READ();

	return 1;
}

int CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	return 1;
}

int CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p )
			pList->p->InitHUDData();
		pList = pList->pNext;
	}

	return 1;
}

int CHud::MsgFunc_SetFOV( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int newfov = READ_BYTE();
	SetFOV( newfov );
	END_READ();
	return 1;
}

void CHud::SetFOV( int newfov )
{
	float def_fov = CVAR_GET_FLOAT( "default_fov" );

	if( !IsZooming ) // FIXME not sure about this condition, but zooming only works this way
	{
		if( newfov == 0 )
			m_flFOV = def_fov;
		else
			m_flFOV = newfov;
	}

	if( m_flFOV == def_fov )
		m_flMouseSensitivity = 0; // reset to saved sensitivity
	else
	{
		m_flMouseSensitivity = CVAR_GET_FLOAT( "sensitivity" );

		// scale by zoom ratio
		m_flMouseSensitivity *= ((float)newfov / def_fov) * CVAR_GET_FLOAT( "zoom_sensitivity_ratio" );
	}
}

int CHud::MsgFunc_HUDColor( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	m_iHUDColor = READ_LONG();
	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_GameMode( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	m_Teamplay = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_Damage( const char *pszName, int iSize, void *pbuf )
{
	int	armor, blood;
	Vector	from;
	float	count;

	BEGIN_READ( pszName, pbuf, iSize );

	armor = READ_BYTE();
	blood = READ_BYTE();

	for( int i = 0; i < 3; i++ )
		from[i] = READ_COORD();

	count = (blood * 0.5f) + (armor * 0.5f);
	if( count < 10 ) count = 10;

	END_READ();

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	m_iConcussionEffect = READ_BYTE();

	if( m_iConcussionEffect )
		m_StatusIcons.EnableIcon("dmg_concuss", 255, 160, 0 );
	else m_StatusIcons.DisableIcon( "dmg_concuss" );

	END_READ();

	return 1;
}

int CHud :: MsgFunc_SetBody( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	gEngfuncs.GetViewModel()->curstate.body = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetSkin( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	gEngfuncs.GetViewModel()->curstate.skin = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_WeaponAnim( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int sequence = READ_BYTE();
	int body = READ_BYTE();
	localanim_WeaponID = READ_BYTE();

	END_READ();

	cl_entity_t *view = GET_VIEWMODEL();

	// call weaponanim with same body
	// diffusion - check if the animation can be played local (kind of a prediction)
	// if it can, then we already played it, so skip it.
	if( !CheckForLocalWeaponShootAnimation( sequence ) )
		gEngfuncs.pfnWeaponAnim( sequence, body );

	view->curstate.framerate = 1.0f;
#if 0
	// just for test Glow Shell effect
	view->curstate.renderfx = kRenderFxGlowShell;
	view->curstate.rendercolor.r = 255;
	view->curstate.rendercolor.g = 255;
	view->curstate.rendercolor.b = 255;
	view->curstate.renderamt = 100;
#endif	
	return 1;
}

int CHud :: MsgFunc_Particle( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int entindex = READ_SHORT();
	char *sz = READ_STRING();
	int attachment = READ_BYTE();

	COM_DefaultExtension( sz, ".aur" );

	UTIL_CreateAurora( GET_ENTITY( entindex ), sz, attachment );

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_KillPart( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int entindex = READ_SHORT();

	UTIL_RemoveAurora( GET_ENTITY( entindex ));

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_KillDecals( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int entityIndex = READ_SHORT();

	// diffusion - studiodecalsfpslossissue
//	if( g_fRenderInitialized )
//		g_StudioRenderer.RemoveAllDecals( entityIndex );

	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( entityIndex );

	if( g_fRenderInitialized && ent->model && ent->model->type == mod_brush )
		REMOVE_BSP_DECALS( ent->model );

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_StudioDecal( const char *pszName, int iSize, void *pbuf )
{
	return 1; // diffusion - studiodecalsfpslossissue
	
	Vector vecEnd, vecSrc;

	if( !g_fRenderInitialized )
		return 1;

	BEGIN_READ( pszName, pbuf, iSize );

	vecEnd.x = READ_COORD();
	vecEnd.y = READ_COORD();
	vecEnd.z = READ_COORD();
	vecSrc.x = READ_COORD();
	vecSrc.y = READ_COORD();
	vecSrc.z = READ_COORD();
	int decalIndex = READ_SHORT();
	int entityIndex = READ_SHORT();
	int modelIndex = READ_SHORT();
	int flags = READ_BYTE();

	modelstate_t state;
	state.sequence = READ_SHORT();
	state.frame = READ_SHORT();
	state.blending[0] = READ_BYTE();
	state.blending[1] = READ_BYTE();
	state.controller[0] = READ_BYTE();
	state.controller[1] = READ_BYTE();
	state.controller[2] = READ_BYTE();
	state.controller[3] = READ_BYTE();
	state.body = READ_BYTE();
	state.skin = READ_BYTE();
	int cacheID = READ_SHORT();

	cl_entity_t *ent = GET_ENTITY( entityIndex );

	if( !ent )
	{
		// something very bad happens...
		ALERT( at_error, "StudioDecal: ent == NULL\n" );
		END_READ();

		return 1;
	}

	entity_state_t savestate = ent->curstate;
	Vector origin = ent->origin;
	Vector angles = ent->angles;

	int decalTexture = gEngfuncs.pEfxAPI->Draw_DecalIndex( decalIndex );
	if( !ent->model && modelIndex != 0 )
		ent->model = IEngineStudio.GetModelByIndex( modelIndex );

	// setup model pose for decal shooting
	ent->curstate.sequence = state.sequence;
	ent->curstate.frame = (float)state.frame * (1.0f / 8.0f);
	ent->curstate.blending[0] = state.blending[0];
	ent->curstate.blending[1] = state.blending[1];
	ent->curstate.controller[0] = state.controller[0];
	ent->curstate.controller[1] = state.controller[1];
	ent->curstate.controller[2] = state.controller[2];
	ent->curstate.controller[3] = state.controller[3];
	ent->curstate.body = state.body;
	ent->curstate.skin = state.skin;

	if( cacheID )
	{
		// tell the code about vertex lighting
		SetBits( ent->curstate.iuser1, CF_STATIC_ENTITY );
		ent->curstate.iuser3 = cacheID;
	}

//	double start = Sys_DoubleTime();

	R_StudioDecalShoot( decalTexture, ent, vecSrc, vecEnd, flags, &state );

//	ALERT( at_console, "ShootDecal: buildtime %g\n", Sys_DoubleTime() - start );

	// restore original state
	ent->curstate = savestate;
	ent->origin = origin;
	ent->angles = angles;

	END_READ();

	return 1;
}

int CHud :: MsgFunc_SetupBones( const char *pszName, int iSize, void *pbuf )
{
	static Vector pos[MAXSTUDIOBONES];
	static Radian ang[MAXSTUDIOBONES];

	BEGIN_READ( pszName, pbuf, iSize );
		int entityIndex = READ_SHORT();
		int boneCount = READ_BYTE();
		for( int i = 0; i < boneCount; i++ )
		{
			pos[i].x = (float)READ_SHORT() * (1.0f/128.0f);
			pos[i].y = (float)READ_SHORT() * (1.0f/128.0f);
			pos[i].z = (float)READ_SHORT() * (1.0f/128.0f);
			ang[i].x = (float)READ_SHORT() * (1.0f/512.0f);
			ang[i].y = (float)READ_SHORT() * (1.0f/512.0f);
			ang[i].z = (float)READ_SHORT() * (1.0f/512.0f);
		}
	END_READ();	

	cl_entity_t *ent = GET_ENTITY( entityIndex );

	if( !ent )
	{
		// something very bad happens...
		ALERT( at_error, "SetupBones: ent == NULL\n" );

		return 1;
	}

	R_StudioSetBonesExternal( ent, pos, ang );

	return 1;
}

int CHud :: MsgFunc_RainData( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	if( g_fRenderInitialized )
		R_ParseRainMessage();

	END_READ();	

	return 1;
}

int CHud :: MsgFunc_MusicFade( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	if( g_fRenderInitialized )
		MUSIC_FADE_VOLUME( (float)READ_SHORT() / 100.0f );

	END_READ();

	return 1;
}

void DCL( int index )
{

}

//=======================================================================================
// diffusion - temp ents which I moved from engine to here
//=======================================================================================
int CHud::MsgFunc_TempEnt( const char *pszName, int iSize, void *pbuf )
{
	Vector pos, pos2, pos3, ang;
	int Message, Model, Color, Value, Count, Body;
	int entityIndex, decalIndex;
	TEMPENTITY *pTemp;
	int Mode, Flags, Weapon, Startframe;
	float Scale, Framerate, Life, Speed, r, g, b, a, noise;
	pmtrace_t trace;
	
	BEGIN_READ( pszName, pbuf, iSize );

	Message = READ_BYTE();

	switch( Message )
	{
	case TE_CLIENTSOUND:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		entityIndex = READ_SHORT();
		int snd = READ_BYTE();
		Mode = READ_BYTE();
		Flags = READ_BYTE(); // low ammo volume
		R_ClientSound( pos, entityIndex, snd, Mode, Flags );
	}
	break;

	case TE_PLAYERPARAMS: // hack to avoid creating new message :)
	{
		gHUD.CanJump = (READ_BYTE() > 0);
		gHUD.CanSprint = (READ_BYTE() > 0);
		gHUD.CanMove = (READ_BYTE() > 0);
		gHUD.HUDSuitOffline = (READ_BYTE() > 0);
		gHUD.CanSelectEmptyWeapon = (READ_BYTE() > 0);
		gHUD.InCar = (READ_BYTE() > 0);
		gHUD.BreathingEffect = (READ_BYTE() > 0);
		gHUD.WigglingEffect = (READ_BYTE() > 0);
		bool ShieldOn = (READ_BYTE() > 0);
		gHUD.PlayingDrums = (READ_BYTE() > 0);
		gHUD.WeaponLowered = (READ_BYTE() > 0);
		gHUD.CanShoot = (READ_BYTE() > 0);
		gHUD.DrunkLevel = READ_BYTE();
		gHUD.m_DroneBars.CanUseDrone = (READ_BYTE() > 0);

		if( !gHUD.ShieldOn && ShieldOn ) // play turn on sound
			gEngfuncs.pEventAPI->EV_PlaySound( gEngfuncs.GetLocalPlayer()->index, NULL, CHAN_STATIC, "player/shield_on.wav", VOL_NORM, 0, 0, PITCH_NORM );

		gHUD.ShieldOn = ShieldOn;
	}
	break;

	case TE_SUNLIGHT_SCALE:
	{
		tr.sunlightscale = READ_SHORT() * 0.01f;
	}
	break;

	case TE_BLOODSPRITE:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		pos2.x = READ_BYTE(); // color
		pos2.y = READ_BYTE();
		pos2.z = READ_BYTE();
		pos3.x = READ_COORD(); // direction
		pos3.y = READ_COORD();
		pos3.z = READ_COORD();
		Scale = READ_BYTE();
		g_pParticles.BloodParticle( 0, pos, Scale, pos2, pos3 );
	}
	break;

	case TE_CARPARAMS:
	{
		static int oldgear = 0;
		gHUD.m_ScreenEffects.Gear = READ_BYTE();

		// don't jerk fov when downshifting
		if( gHUD.m_ScreenEffects.Gear != 8 && gHUD.m_ScreenEffects.Gear > oldgear )
			gHUD.CarAddFovMult = 0.5f;

		oldgear = gHUD.m_ScreenEffects.Gear;
	}
	break;

	case TE_DRONEPARAMS:
	{
		gHUD.DroneColor.x = READ_BYTE();
		gHUD.DroneColor.y = READ_BYTE();
		gHUD.DroneColor.z = READ_BYTE();
		gHUD.m_DroneBars.DroneHealth = READ_SHORT();
		gHUD.m_DroneBars.DroneAmmo = READ_SHORT();
		gHUD.m_DroneBars.DroneDeployed = (READ_BYTE() > 0);
		gHUD.m_DroneBars.DroneDistance = READ_SHORT();
	}
	break;

	case TE_BUBBLES:
	case TE_BUBBLETRAIL:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		pos2.x = READ_COORD();
		pos2.y = READ_COORD();
		pos2.z = READ_COORD();
		Scale = READ_COORD();	// water height
	//	modelIndex = MSG_ReadShort( &buf );
		Count = READ_BYTE();
		float vel = READ_COORD();
		if( Message == TE_BUBBLES )
			R_Bubbles( pos, pos2, Scale, Count, vel );
		else 
			R_BubbleTrail( pos, pos2, Scale, Count, vel );
	}
	break;

	case TE_NPCCLIP:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		Model = READ_BYTE();
		R_EmptyClipNPC( Model, pos );
	}
	break;

	case TE_BEAMPARTICLES:
	{
		entityIndex = READ_SHORT();
		if( UTIL_IsLocal(entityIndex))
			pos = GET_VIEWMODEL()->attachment[0];
		else // other player
			pos = GET_ENTITY( entityIndex )->attachment[0];
		pos2.x = READ_COORD();
		pos2.y = READ_COORD();
		pos2.z = READ_COORD();
		g_pParticles.Beamring( 0, pos, pos2 );
	}
	break;

	case TE_BEAMENTPOINT:
	{
		entityIndex = READ_SHORT();
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		Model = READ_SHORT();
		Startframe = READ_BYTE();
		Framerate = (float)READ_BYTE() * 0.1f;
		Life = (float)READ_BYTE() * 0.1f;
		Scale = (float)READ_BYTE() * 0.1f;
		noise = (float)READ_BYTE() * 0.01f;
		r = (float)READ_BYTE() / 255.0f;
		g = (float)READ_BYTE() / 255.0f;
		b = (float)READ_BYTE() / 255.0f;
		a = (float)READ_BYTE() / 255.0f;
		Speed = (float)READ_BYTE() * 0.1f;
		gEngfuncs.pEfxAPI->R_BeamEntPoint( entityIndex, pos, Model, Life, Scale, noise, a, Speed, Startframe, Framerate, r, g, b );

		// add a second noisy white beam
		gEngfuncs.pEfxAPI->R_BeamEntPoint( entityIndex, pos, Model, Life, Scale * 0.5f, 0.05f, a, Speed, Startframe, Framerate, 1, 1, 1 );
	}
	break;

	case TE_SMOKE:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		Model = READ_SHORT();
		Scale = READ_BYTE() * 0.1;
		Framerate = READ_BYTE();
		Value = READ_BYTE(); // pos rand
		g_pParticles.SmokeParticles( 0, pos, 1, 0, Scale * 2, Value );
		// old
	//	pTemp = gEngfuncs.pEfxAPI->R_DefaultSprite( pos, Model, Framerate );
	//	R_Sprite_Smoke( pTemp, Scale, 0 );
	}
	break;

	case TE_SMOKEGRENADE:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		int Count = READ_BYTE();
		float Speed = READ_BYTE() * 0.01; // speed
		Scale = READ_BYTE();
		int posRand = READ_BYTE();
		g_pParticles.Smoke( 0, 0, pos, g_vecZero, Count, Speed, Scale, posRand );
		gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_STATIC, "weapons/smoke_grenade.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	}
	break;

	case TE_FIRE:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		Model = READ_SHORT();
		Scale = READ_BYTE() * 0.1;
		Framerate = READ_BYTE();
		pTemp = gEngfuncs.pEfxAPI->R_DefaultSprite( pos, Model, Framerate );
		R_Sprite_Smoke( pTemp, Scale, 1 );
	//	if( g_pParticles.m_pAllowParticles->value > 0 )
			g_pParticles.SmokeParticles( 0, pos, 1, 0.5 ); // doing smoke anyway, too :)
	}
	break;

	case TE_DLIGHT:
	{
		plight_t *dl = CL_AllocPlight( 0 );

		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();

		dl->radius = (float)(READ_BYTE() * 10.0f);

		dl->color.r = READ_BYTE();
		dl->color.g = READ_BYTE();
		dl->color.b = READ_BYTE();

		float dietime = (float)(READ_BYTE() * 0.1f);
		if( dietime == 0.0f ) // diffusion - HACKHACK if we receive 0
			dietime = 0.05;
		dl->die = tr.time + dietime;

		dl->decay = (float)(READ_BYTE() * 10.0f);
		dl->brightness = READ_BYTE() * 0.01f;

		bool Shadows = (READ_BYTE() > 0);
		if( !Shadows )
			dl->flags |= CF_NOSHADOWS;

		dl->pointlight = true;
		dl->projectionTexture = tr.whiteCubeTexture;

		R_SetupLightProjection( dl, pos, g_vecZero, dl->radius, 90.0f );
		R_SetupLightAttenuationTexture( dl );
	}
	break;

	case TE_SPARKS:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
	//	gEngfuncs.pEfxAPI->R_SparkShower( pos );
		g_pParticles.CreateEffect( 0, "env_spark.part1", pos, g_vecZero );
		g_pParticles.CreateEffect( 0, "env_spark.part2", pos, g_vecZero );
	}
	break;

	case TE_EXPLOSION:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		Model = READ_SHORT();
		Scale = READ_BYTE() * 0.1;
		Framerate = READ_BYTE();
		Flags = READ_BYTE();
		R_Explosion( pos, Model, Scale, Framerate, Flags );
	//	g_pParticles.ExplosionOrg = pos;
	}
	break;

	case TE_GUNSHOTDECAL:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		entityIndex = READ_SHORT();
		decalIndex = READ_BYTE();
		Scale = 3.0f; // scale is inverted
		if( decalIndex == 185 ) // DECAL_GUNSHOT5 - knife
			Scale = 7.0f;

		if( decalIndex == 185 ) // DECAL_GUNSHOT5 - knife
			gEngfuncs.pEfxAPI->R_FireCustomDecal( gEngfuncs.pEfxAPI->Draw_DecalIndex( decalIndex), entityIndex, 0, pos, 0, Scale );
		else
		{
			/*
			int bulletdecal = 0; 
			switch( RANDOM_LONG(0,3) )
			{
			case 0: bulletdecal = LOAD_TEXTURE( "gfx/decals/bullethole1.dds", NULL, 0, 0 ); break;
			case 1: bulletdecal = LOAD_TEXTURE( "gfx/decals/bullethole2.dds", NULL, 0, 0 ); break;
			case 2: bulletdecal = LOAD_TEXTURE( "gfx/decals/bullethole3.dds", NULL, 0, 0 ); break;
			case 3: bulletdecal = LOAD_TEXTURE( "gfx/decals/bullethole4.dds", NULL, 0, 0 ); break;
			}
			gEngfuncs.pEfxAPI->R_FireCustomDecal( bulletdecal, entityIndex, 0, pos, 0, 100.0f );
			*/
			gEngfuncs.pEfxAPI->R_FireCustomDecal( gEngfuncs.pEfxAPI->Draw_DecalIndex( decalIndex ), entityIndex, 0, pos, 0, Scale );

			// particle
			gEngfuncs.pEventAPI->EV_SetTraceHull( 1 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( tr.viewparams.vieworg, pos, 0, -1, &trace );
			const char *decal_tex = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, pos + trace.plane.normal, pos - trace.plane.normal );
			char chTextureType = CHAR_TEX_NONE;
			char szbuffer[64];

			if( decal_tex )
			{
				// strip leading '-0' or '+0~' or '{' or '!'
				if( *decal_tex == '-' || *decal_tex == '+' )
					decal_tex += 2;

				if( *decal_tex == '{' || *decal_tex == '!' || *decal_tex == '~' || *decal_tex == ' ' )
					decal_tex++;
				// '}}'
				Q_strncpy( szbuffer, decal_tex, sizeof( szbuffer ));
				szbuffer[CBTEXTURENAMEMAX - 1] = 0;

				// get texture type
				chTextureType = PM_FindTextureType( szbuffer );
			//	gEngfuncs.Con_Printf( "%c\n", chTextureType );
			}

			int gunshot_particle = 1; // 1 - dust, 2 - metal sparks
			switch( chTextureType )
			{
			case CHAR_TEX_GLASS:
			case CHAR_TEX_FLESH:
			case CHAR_TEX_VENT:
			case CHAR_TEX_NONE:
				gunshot_particle = 0;
				break;
			case CHAR_TEX_COMPUTER:
			case CHAR_TEX_METAL:
				gunshot_particle = 2;
				break;
			}

			if( gunshot_particle > 0 )
			{
				CQuakePart bullet_p = InitializeParticle();
				if( gunshot_particle == 1 )
				{
					bullet_p.m_vecOrigin = pos;
					bullet_p.m_vecVelocity = trace.plane.normal * 40;
					bullet_p.m_vecColor = Vector( 0.5, 0.5, 0.5 );
					bullet_p.m_flAlphaVelocity = RANDOM_FLOAT( -1.5f, -0.75f );
					bullet_p.m_flRadius = 2;
					bullet_p.m_flRadiusVelocity = 30;
					bullet_p.m_flRotation = RANDOM_LONG( 0, 360 );
					bullet_p.m_flRotationVelocity = 25;
					bullet_p.m_flDistance = 1000;
					bullet_p.ParticleType = TYPE_SMOKE;
					g_pParticles.AddParticle( &bullet_p, g_pParticles.m_hSmoke, FPART_NOTWATER | FPART_VERTEXLIGHT );
				}
				else if( gunshot_particle == 2 )
				{
					g_pParticles.BulletParticles( 0, pos, trace.plane.normal );
				}
			}
		}
	}
	break;

	case TE_USERTRACER:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		pos2.x = READ_COORD(); // velocity vector
		pos2.y = READ_COORD();
		pos2.z = READ_COORD();
		Life = READ_BYTE() * 0.1;
		Color = READ_BYTE();
		Scale = READ_BYTE() * 0.1;
		gEngfuncs.pEfxAPI->R_UserTracerParticle( pos, pos2, Life, Color, Scale, 0, NULL );
	}
	break;

	case TE_BOAT_TRAIL:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		pos2.y = READ_COORD(); // angle
		Scale = READ_BYTE() * 0.005;
		Framerate = READ_BYTE(); // boat speed 0-255

		int WaterTrail = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/effects/watertrail.spr" );

		pTemp = gEngfuncs.pEfxAPI->R_TempSprite( pos, Vector( 0, 0, 0 ), Scale, WaterTrail, kRenderTransAdd, 0, (Framerate / 255.0f), 0.1, FTENT_FADEIN | FTENT_FADEOUT );

		if( pTemp )
		{
			pTemp->entity.baseline.fuser1 = 0.0f; // for FTENT_FADEIN - starting renderamt
			pTemp->entity.baseline.fuser2 = tr.time; // for FTENT_FADEIN - spawn time (always tr.time)
			pTemp->entity.baseline.fuser3 = 2.5f; // for FTENT_FADEIN - fade in speed (more - faster)
			pTemp->fadeSpeed = 1.0f;
			pTemp->entity.curstate.rendercolor.r = 255;
			pTemp->entity.curstate.rendercolor.g = 255;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.angles = Vector( 90, pos2.y, 0 );
		}
	}
	break;

	case TE_SCREENSHAKE:
	{
		gHUD.shake.amplitude = (float)(word)READ_SHORT() * (1.0f / 4096.0f);
		gHUD.shake.duration = (float)(word)READ_SHORT() * (1.0f / 4096.0f);
		gHUD.shake.frequency = (float)(word)READ_SHORT() * (1.0f / 256.0f);
		gHUD.shake.time = tr.time + Q_max( gHUD.shake.duration, 0.01f );
		gHUD.shake.next_shake = 0.0f; // apply immediately
	}
	break;

	case TE_WEAPONSHAKE:
	{
		Weapon = READ_BYTE();
		Mode = READ_BYTE(); // primary or secondary attack?
		R_MakeWeaponShake( Weapon, Mode );
	}
	break;

	case TE_CLIENTEVENT:
	{
		Value = 5000 + READ_BYTE();
		entityIndex = READ_SHORT();
		cl_entity_s *ent = GET_ENTITY( entityIndex );
		if( !ent )
			break;
		Q_strncpy( tr.studioevent[entityIndex].options, "50", sizeof( tr.studioevent[entityIndex].options ));
		tr.studioevent[entityIndex].event = Value;
	}
	break;

	case TE_PLAYER_GLITCH:
	{
		gHUD.GlitchAmount = READ_BYTE() * 0.1f;
		gHUD.GlitchHoldTime = tr.time + READ_BYTE();
	}
	break;

	case TE_ACHIEVEMENT:
	{
		int ach_number = READ_BYTE();
		Value = READ_LONG();
		Mode = READ_BYTE();
		if( gHUD.m_StatusIconsAchievement.bAchievements )
		{
			switch( Mode )
			{
			case 0: gHUD.m_StatusIconsAchievement.ach_data.value[ach_number] += Value; break;
			case 1: gHUD.m_StatusIconsAchievement.ach_data.value[ach_number] -= Value; break;
			case 2: gHUD.m_StatusIconsAchievement.ach_data.value[ach_number] = Value; break;
			}
		//	ConPrintf( "Achievement received: %i, value received: %i\n", ach_number, Value );
		}
	//	else
	//		ConPrintf( "Achievement received: %i, value received: %i. NOT ALLOWED\n", ach_number, Value );
	}
	break;

	case TE_UNREALSOUND:
	{
		Mode = READ_BYTE();
		if( 1 ) // TODO - add cvar
		{
			switch( Mode )
			{
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/spree_killingspree.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/spree_dominating.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 3: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/spree_rampage.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 4: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/spree_godlike.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 5: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/spree_wickedsick.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 6: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_headshot.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 7: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_spree_ended.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 8: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_doublekill.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 9: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_triplekill.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 10: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_multikill.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 11: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_megakill.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 12: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_ultrakill.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			case 13: gEngfuncs.pEventAPI->EV_PlaySound( 0, NULL, CHAN_STATIC, "server/snd_monsterkill.wav", VOL_NORM, 0, 0, PITCH_NORM ); break;
			}
		}
	}
	break;

	case TE_MODEL: // this is literally only being used for gun shells
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		pos2.x = READ_COORD();
		pos2.y = READ_COORD();
		pos2.z = READ_COORD();
		ang.x = 0.0f;
		ang.y = READ_ANGLE(); // yaw angle
		ang.z = 0.0f;
		Model = READ_SHORT();
		Flags = READ_BYTE();	// sound flags
		Life = (float)(READ_BYTE() * 0.1f);
		Body = READ_BYTE(); // shell type
		R_GunShell( pos, pos2, ang, Life, Model, Body, Flags );
	}
	break;

	case TE_STEP_PARTICLE:
	{
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		Model = READ_BYTE();
		g_pParticles.Smoke( 0, Model, pos, Vector( 0, 0, 5 ), 1, 0.15f, 10, 1 );
	}
	break;

	case TE_TRIGGERTIMER:
	{
		gHUD.m_TriggerTimer.enabled = READ_BYTE();
		if( gHUD.m_TriggerTimer.enabled == 0 )
		{
			gHUD.m_TriggerTimer.message[0] = '\0';
			break;
		}
		const char *pText = READ_STRING();
		// try to find the message from titles.txt
		client_textmessage_t *TimerText = TextMessageGet( pText );
		// if found, replace the message with titles.txt text; make sure it's not too long or game will freeze
		if( TimerText && strlen( TimerText->pMessage ) < sizeof(gHUD.m_TriggerTimer.message) )
			Q_strncpy( gHUD.m_TriggerTimer.message, TimerText->pMessage, sizeof( gHUD.m_TriggerTimer.message ));
		else // use as is
			Q_strncpy( gHUD.m_TriggerTimer.message, pText, sizeof( gHUD.m_TriggerTimer.message ));
		gHUD.m_TriggerTimer.timer = READ_SHORT();
		if( gHUD.m_TriggerTimer.enabled > 1 )
			gHUD.m_TriggerTimer.critical = true;
		else
			gHUD.m_TriggerTimer.critical = false;
	}
	break;
	}

	END_READ();
	
	return 1;
}

//=======================================================================================
// diffusion - do watersplash
//=======================================================================================
int CHud::MsgFunc_WaterSplash( const char *pszName, int iSize, void *pbuf )
{
	Vector pos;
	int Type = 0;
	
	BEGIN_READ( pszName, pbuf, iSize );
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	Type = READ_BYTE();
	END_READ();

	g_pParticles.WaterRingParticle( 0, pos );

	switch( Type )
	{
	default:
	case 0:
		g_pParticles.CreateEffect( 0, "very_small_splash", pos, g_vecZero );
		break;
	case 1:
		g_pParticles.CreateEffect( 0, "small_splash", pos, g_vecZero );
		break;
	case 2:
		g_pParticles.CreateEffect( 0, "big_splash", pos, g_vecZero );
		break;
	case 3:
		g_pParticles.CreateEffect( 0, "bigger_splash", pos, g_vecZero );
		break;
	}

	if( Type == 1 )
	{
		if( (tr.viewparams.vieworg - pos).Length() < 100 )
		{
			ScreenDrips_OverrideTime = tr.time + 1;
			ScreenDrips_CurVisibility = 0.5f;
			ScreenDrips_DripIntensity = 0.5f;
			ScreenDrips_Visible = true;
		}
	}
	else if( Type == 2 || Type == 3 )
	{
		if( (tr.viewparams.vieworg - pos).Length() < 200 )
		{
			gHUD.ScreenDrips_OverrideTime = tr.time + 2;
			gHUD.ScreenDrips_CurVisibility = 1.0f;
			gHUD.ScreenDrips_DripIntensity = 0.75f;
			gHUD.ScreenDrips_Visible = true;
		}
	}

	switch( RANDOM_LONG( 0, 2 ) )
	{
	case 0: gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_STATIC, "items/water_splash/water_splash1.wav", 0.7, 0.8, 0, RANDOM_LONG( 95, 110 ) ); break;
	case 1: gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_STATIC, "items/water_splash/water_splash2.wav", 0.7, 0.8, 0, RANDOM_LONG( 95, 110 ) ); break;
	case 2: gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_STATIC, "items/water_splash/water_splash3.wav", 0.7, 0.8, 0, RANDOM_LONG( 95, 110 ) ); break;
	}
	
	return 1;
}

int CHud::MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	Q_strncpy( m_szServerName, READ_STRING(), sizeof( m_szServerName ) );
	m_szServerName[sizeof( m_szServerName ) - 1] = 0;
#if XASH_64BIT && XASH_WIN32
	discord_integration::update();
#endif
	return 0;
}
