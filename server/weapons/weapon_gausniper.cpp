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

// Gauss edited for Diffusion mod

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "player.h"
#include "entities/soundent.h"
#include "shake.h"
#include "game/gamerules.h"

#define GAUSS_PRIMARY_CHARGE_VOLUME	256 // how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME	450 // how loud gauss is when discharged

#define GAUSS_MID_ZOOM 30
#define GAUSS_MAX_ZOOM 10

enum gauss_e
{
	GAUSS_IDLE = 0,
	GAUSS_IDLE2,
	GAUSS_FIDGET,
	GAUSS_SPINUP,
	GAUSS_SPIN,
	GAUSS_FIRE,
	GAUSS_FIRE2,
	GAUSS_HOLSTER,
	GAUSS_DRAW
};

class CGauss : public CBasePlayerWeapon
{
	DECLARE_CLASS( CGauss, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_GAUSS + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	BOOL Deploy( void );
	void Holster( void );

	void PrimaryAttack( void );
	void Attack( void );
	void SecondaryAttack( void );
	void Reload( void );
	void WeaponIdle( void );
	void ResetZoom( void );
	void UpdateHUD( bool Warning = false );
	bool ReflectMirror( TraceResult &tr, CBaseEntity *e );

	int m_fInAttack;	
	float m_flStartCharge;
	float m_flPlayAftershock;
	void StartFire( void );
	void Fire( Vector vecOrigSrc, Vector vecDirShooting, float flDamage );
	float GetFullChargeTime( void );
	int m_iBalls;
	int m_iGlow;
	int m_iBeam;
	int m_iSoundState; // don't save this
	bool WarningSoundPlayed;

	float m_flNextAmmoBurn;// while charging, when to absorb another unit of player's ammo?

	int AmmoTaken; // the ammo taken for the charge is counted here, then released while shot

	float nReflect; // reflection degree - 0.5 default, 0.1 for mirrors
};

BEGIN_DATADESC( CGauss )
	DEFINE_FIELD( m_fInAttack, FIELD_INTEGER ),
	DEFINE_FIELD( m_flStartCharge, FIELD_TIME ),
	DEFINE_FIELD( m_flPlayAftershock, FIELD_TIME ),
	DEFINE_FIELD( m_flNextAmmoBurn, FIELD_TIME ),
	DEFINE_FIELD( WarningSoundPlayed, FIELD_BOOLEAN ),
	DEFINE_FIELD( AmmoTaken, FIELD_INTEGER ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( weapon_gausniper, CGauss );

float CGauss::GetFullChargeTime( void )
{
	return 1.5;
}

void CGauss::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_GAUSS;
	SET_MODEL(ENT(pev), "models/w_gausniper.mdl");

	m_iDefaultAmmo = GAUSS_DEFAULT_GIVE;

	WarningSoundPlayed = false;

	AmmoTaken = 0;

	FallInit();// get ready to fall down.
}

void CGauss::Precache( void )
{
	PRECACHE_MODEL("models/w_gausniper.mdl");
	PRECACHE_MODEL("models/v_gausniper.mdl");
	PRECACHE_MODEL("models/p_gausniper.mdl");

	PRECACHE_SOUND( "weapons/crystal_reload.wav" ); // Precaching reloading sound
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/gausniper.wav");
	PRECACHE_SOUND("weapons/crystal_empty.wav"); // New dry sound
	PRECACHE_SOUND("weapons/gauss_loop.wav");
	PRECACHE_SOUND("weapons/gauss_overcharge.wav");
	PRECACHE_SOUND( "weapons/gauss_zap1.wav" );
	PRECACHE_SOUND( "weapons/gauss_zap2.wav" );
	PRECACHE_SOUND( "weapons/gauss_zap3.wav" );
	PRECACHE_SOUND( "weapons/gauss_zap4.wav" );
	PRECACHE_SOUND( "weapons/gauss_cancel.wav" );
	
	m_iGlow = PRECACHE_MODEL( "sprites/hotglow.spr" );
	m_iBalls = PRECACHE_MODEL( "sprites/hotglow.spr" );
	m_iBeam = PRECACHE_MODEL( "sprites/smoke.spr" );
}

int CGauss::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

int CGauss::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->iMaxClip = WEAPON_NOCLIP;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iSlot = WPN_SLOT_GAUSS;
	p->iPosition = WPN_POS_GAUSS;
	p->iId = m_iId = WEAPON_GAUSS;
	p->iFlags = 0;
	p->iWeight = GAUSS_WEIGHT;

	return 1;
}

void CGauss::ResetZoom( void )
{
	if( m_pPlayer->ZoomState > 0 )
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );

	m_pPlayer->ZoomState = 0;
	m_pPlayer->m_flFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_pPlayer->ZoomState );
		WRITE_BYTE( WEAPON_GAUSS );
	MESSAGE_END();
}

BOOL CGauss::Deploy( void )
{
	m_flPlayAftershock = 0.0;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	return DefaultDeploy( "models/v_gausniper.mdl", "models/p_gausniper.mdl", GAUSS_DRAW, "gauss" );
}

void CGauss::Holster( void )
{
	// cancel any zooming in progress
	ResetZoom();
	
	AmmoTaken = 0;
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	SendWeaponAnim( GAUSS_HOLSTER );
	m_fInAttack = 0;
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 0, ATTN_NORM);

	// need to send "charge" to reset weapon skin on client
	UpdateHUD();

	BaseClass::Holster();
}

void CGauss::PrimaryAttack( void )
{
	Attack();
	/*
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM);
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		return;
	}

	if (m_iClip <= 0)
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM);
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		return;
	}

	if( m_iClip <= 15)
		LowAmmoMsg(m_pPlayer);

	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;

	m_fPrimaryFire = TRUE;

	m_iClip--;

	StartFire();
	m_fInAttack = 0;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0;
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.25;  //diffusion - was 0.2
	*/
}

void CGauss::UpdateHUD( bool Warning )
{
	int Charge = AmmoTaken;
	if( Charge > 9 )
		Charge = 9; // HACKHACK!!! the full charge depends from fps and varies from 9 to 11. Assume it's always 9...

	MESSAGE_BEGIN( MSG_ONE, gmsgGaussHUD, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] ); // current ammo count
		WRITE_BYTE( Charge );
		WRITE_BYTE( Warning );
	MESSAGE_END();
}

void CGauss::Attack( void )
{
	// don't fire underwater
	if ((m_pPlayer->pev->waterlevel == 3) || (m_pPlayer->m_afButtonPressed & IN_RELOAD) )
	{
		if ( (m_fInAttack != 0) || (m_pPlayer->m_afButtonPressed & IN_RELOAD) )
		{
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_cancel.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			SendWeaponAnim( GAUSS_IDLE );
			m_fInAttack = 0;
			AmmoTaken = 0;
			WarningSoundPlayed = false;
			CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
			// send HUD update to disable charge
			UpdateHUD();
		}
		else
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM);

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.5;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM);
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		UpdateHUD();
		return;
	}

	// do not show when zoomed, to not obstruct the hud
	if( (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 15) && (m_pPlayer->ZoomState == 0))
		LowAmmoMsg(m_pPlayer);
	
	if (m_fInAttack == 0)
	{
		// take one ammo just to start the spin
		// diffusion - we take ammo silently, it will be subtracted only after firing
		AmmoTaken++;
		m_flNextAmmoBurn = gpGlobals->time;

		// spin up
		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;
		
	//	SendWeaponAnim( GAUSS_SPINUP );
		m_fInAttack = 1;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
		m_flStartCharge = gpGlobals->time;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_loop.wav",1.0 , ATTN_NORM, 0, 100 );
		m_iSoundState = SND_CHANGE_PITCH;
		UpdateHUD();
	}
	else if (m_fInAttack == 1)
	{
		if (m_flTimeWeaponIdle < gpGlobals->time)
			m_fInAttack = 2;
	}
	else
	{
		if( AmmoTaken == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			// out of ammo! force the gun to fire
			StartFire();
			m_fInAttack = 0;
			m_flTimeWeaponIdle = gpGlobals->time + 1.0;
			m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
			return;
		}

		// during the charging process, eat one bit of ammo every once in a while
		if ( gpGlobals->time > m_flNextAmmoBurn && m_flNextAmmoBurn != -1 )
		{
			AmmoTaken++;
			m_flNextAmmoBurn = gpGlobals->time + 0.1;
			UpdateHUD();
		}
		
		if ( gpGlobals->time - m_flStartCharge >= GetFullChargeTime() )
			m_flNextAmmoBurn = -1; // don't eat any more ammo after gun is fully charged.

	//	int pitch = (gpGlobals->time - m_flStartCharge) * (150 / GetFullChargeTime()) + 100;
		int pitch = (gpGlobals->time - m_flStartCharge) * (150 / GetFullChargeTime() * 2);
		if (pitch > 250) 
			pitch = 250;

		if (m_iSoundState == 0)
			ALERT( at_console, "sound state %d\n", m_iSoundState );

		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_loop.wav", 0.65, ATTN_NORM, m_iSoundState, pitch);
		
		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions

		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;
		
		// m_flTimeWeaponIdle = gpGlobals->time + 0.1;

		if ((m_flStartCharge < gpGlobals->time - 7) && (WarningSoundPlayed == false) )
		{
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/gauss_overcharge.wav", 1.0, ATTN_NORM, 0, 100);
			WarningSoundPlayed = true;
			UpdateHUD( WarningSoundPlayed ); // make it flash with red
		}

		if (m_flStartCharge < gpGlobals->time - 10)
		{
			// Player charged up too long. Zap him.
			switch( RANDOM_LONG( 0, 3 ) )
			{
			case 0: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/gauss_zap1.wav", 1.0, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 25 ) ); break;
			case 1: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/gauss_zap2.wav", 1.0, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 25 ) ); break;
			case 2: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/gauss_zap3.wav", 1.0, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 25 ) ); break;
			case 3: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/gauss_zap4.wav", 1.0, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 25 ) ); break;
			}
			
			m_fInAttack = 0;
			m_flTimeWeaponIdle = gpGlobals->time + 1.0;
			m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0;
			m_pPlayer->TakeDamage( VARS(eoNullEntity), VARS(eoNullEntity), 50, DMG_SHOCK );
			CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
	
			SendWeaponAnim( GAUSS_IDLE );

			WarningSoundPlayed = false;
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= AmmoTaken;
			AmmoTaken = 0;
			UpdateHUD();
			ResetZoom();
			// HACKHACK - do this fade here to override previous fade in ResetZoom...
			UTIL_ScreenFade( m_pPlayer, Vector( 70, 169, 255 ), 2, 0.5, 128, FFADE_IN );
			UTIL_ScreenShakeLocal( m_pPlayer, m_pPlayer->GetAbsOrigin(), 6, 100.0, 2, 0, true );

			// Player may have been killed and this weapon dropped, don't execute any more code after this!
			return;
		}
	}
}

void CGauss::SecondaryAttack(void)
{
	CLIENT_COMMAND( m_pPlayer->edict(), "-attack2\n" );

	if( !IS_DEDICATED_SERVER() )
	{
		if( CVAR_GET_FLOAT( "default_fov" ) <= GAUSS_MAX_ZOOM )
		{
			ALERT( at_error, "weapon_gausniper can't zoom, default_fov is too low!\n" );
			return;
		}
	}

	if( m_pPlayer->ZoomState == 0 ) // not zoomed, zoom to first level (fov 30)
	{
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.25, 0, 255, 0x0000 );
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 1; // zooming in, first step
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_GAUSS );
		MESSAGE_END();
		m_pPlayer->m_flFOV = GAUSS_MID_ZOOM;
	}
	else if( m_pPlayer->ZoomState == 1 ) // zoom to max level (fov 10)
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 2; // zooming in, second step
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_GAUSS );
		MESSAGE_END();
		m_pPlayer->m_flFOV = GAUSS_MAX_ZOOM;
	}
	else if( m_pPlayer->ZoomState == 2 ) // fully zoomed, unzoom fast
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 3; // zooming out
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_GAUSS );
		MESSAGE_END();
		m_pPlayer->ZoomState = 0; // hopefully we zoomed out by now. reset to default state
		m_pPlayer->m_flFOV = 0;
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );
	}

	UpdateHUD( WarningSoundPlayed );

	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
}

void CGauss::Reload( void )
{
	// diffusion - this was the first thing I ever did in coding...time flies
	/*
	// BUGBUG - without this, reloading still triggers, idk why
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == GAUSS_MAX_CLIP)
		return;

	AmmoTaken = 0; // just in case
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_reload.wav", 1.0, ATTN_NORM);
	DefaultReload( GAUSS_MAX_CLIP, GAUSS_RELOAD, 3 );
	*/
}

//=========================================================
// StartFire- since all of this code has to run and then 
// call Fire(), it was easier at this point to rip it out 
// of weaponidle() and make its own function then to try to
// merge this into Fire(), which has some identical variable names 
//=========================================================
void CGauss::StartFire( void )
{
	UTIL_ScreenShakeLocal( m_pPlayer, m_pPlayer->GetAbsOrigin(), AmmoTaken, 100.0, 0.2, 0, true );
//	MakeWeaponShake( m_pPlayer, WEAPON_GAUSS, 0 ); // I didn't finish the "predicting" on this weapon
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= AmmoTaken;
	AmmoTaken = 0;
	WarningSoundPlayed = false;
	UpdateHUD( WarningSoundPlayed ); // update after the values changed
	
	float flDamage;
	
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( ); // + gpGlobals->v_up * -8 + gpGlobals->v_right * 8;
	
	if (gpGlobals->time - m_flStartCharge > GetFullChargeTime())
		flDamage = DMG_WPN_GAUSS_FULL;
	else
		flDamage = DMG_WPN_GAUSS_FULL * ((gpGlobals->time - m_flStartCharge) / GetFullChargeTime() );

	//ALERT ( at_console, "Time:%f Damage:%f\n", gpGlobals->time - m_flStartCharge, flDamage );

	float flZVel = m_pPlayer->GetAbsVelocity().z;

	m_pPlayer->SetAbsVelocity( m_pPlayer->GetAbsVelocity() - gpGlobals->v_forward * flDamage * 1 );

/*		if ( !g_pGameRules->IsDeathmatch() )
	{
		// in deathmatch, gauss can pop you up into the air. Not in single play.
		Vector vecVelocity = m_pPlayer->GetAbsVelocity();
		vecVelocity.z = flZVel;
		m_pPlayer->SetAbsVelocity( vecVelocity );
	}
*/
	SendWeaponAnim( GAUSS_FIRE );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	STOP_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_loop.wav" );
	// diffusion - use different channel to not interrupt it while charging
//	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/gauss2.wav", 0.5 + flDamage * (1.0 / 400.0), ATTN_NORM, 0, 85 + RANDOM_LONG(0,0x1f)); 
	PlayClientSound( m_pPlayer, WEAPON_GAUSS );
	// time until aftershock 'static discharge' sound
	m_flPlayAftershock = gpGlobals->time + RANDOM_FLOAT(0.3, 0.8);

	Fire( vecSrc, vecAiming, flDamage );

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();
}

bool CGauss::ReflectMirror( TraceResult &tr, CBaseEntity *e )
{
	if( UTIL_GetModelType( e->pev->modelindex ) != mod_brush )
		return false;

	// diffusion - I'm not sure about those positions...
	Vector vecStart = tr.vecEndPos + tr.vecPlaneNormal * -2;	// mirror thickness
	Vector vecEnd = tr.vecEndPos + tr.vecPlaneNormal * 2;
	const char *pTextureName = TRACE_TEXTURE( e->edict(), vecStart, vecEnd );

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "reflect", 7 ) )
		return true; // valid surface

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "!reflect", 8 ) )
		return true; // valid surface

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "lasrefl", 7 ) )
		return true; // diffusion - hack to allow reflection without rendering mirror

	return false;
}

void CGauss::Fire( Vector vecOrigSrc, Vector vecDir, float flDamage )
{
	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;

	Vector vecDeviate = g_vecZero;
	if( m_pPlayer->ZoomState == 0 ) // non-zoomed player can't have a straight shot
		vecDeviate = Vector( RANDOM_LONG( -200, 200 ), RANDOM_LONG( -200, 200 ), RANDOM_LONG( -200, 200 ) );

	Vector vecSrc = vecOrigSrc;
	Vector vecDest = vecSrc + vecDir * 8192 + vecDeviate;
	edict_t		*pentIgnore;
	TraceResult tr, beam_tr;
	float flMaxFrac = 1.0;
	int	nTotal = 0;
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int	nMaxHits = 10;

	pentIgnore = ENT( m_pPlayer->pev );

	// ALERT( at_console, "%f %f\n", tr.flFraction, flMaxFrac );

	while (flDamage > 3 && nMaxHits > 0)   // diffusion - was flDamage > 10
	{
		nMaxHits--;

		// ALERT( at_console, "." );
		UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

		if (tr.fAllSolid)
			break;

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if (pEntity == NULL)
			break;

		int Brightness = flDamage;
		Brightness = bound( 100, Brightness, 255 );

		if (fFirstBeam)
		{
			m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
			fFirstBeam = 0;

			Vector tmpSrc = vecSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;

			// don't draw beam until the damn thing looks like it's coming out of the barrel
			// draw beam
			
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
				WRITE_BYTE( TE_BEAMENTPOINT );
				WRITE_SHORT( m_pPlayer->entindex() + 0x1000 );
				WRITE_COORD( tr.vecEndPos.x);
				WRITE_COORD( tr.vecEndPos.y);
				WRITE_COORD( tr.vecEndPos.z);
				WRITE_SHORT( m_iBeam );
				WRITE_BYTE( 0 ); // startframe
				WRITE_BYTE( 0 ); // framerate
				WRITE_BYTE( 1 ); // life

				WRITE_BYTE( 10 );  // width

				WRITE_BYTE( 0 );   // noise

					// always white, and intensity based on charge
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				
				WRITE_BYTE( Brightness );	// brightness

				WRITE_BYTE( 0 );		// speed
			MESSAGE_END();
			
			nTotal += 26;
		}
		else
		{
			// draw beam
			
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_BEAMPOINTS);
				WRITE_COORD( vecSrc.x);
				WRITE_COORD( vecSrc.y);
				WRITE_COORD( vecSrc.z);
				WRITE_COORD( tr.vecEndPos.x);
				WRITE_COORD( tr.vecEndPos.y);
				WRITE_COORD( tr.vecEndPos.z);
				WRITE_SHORT( m_iBeam );
				WRITE_BYTE( 0 ); // startframe
				WRITE_BYTE( 0 ); // framerate
				WRITE_BYTE( 1 ); // life

				WRITE_BYTE( 25 );  // width

				WRITE_BYTE( 0 );   // noise

				// secondary shot is always white, and intensity based on charge
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				
				WRITE_BYTE( Brightness );	// brightness

				WRITE_BYTE( 0 );		// speed
			MESSAGE_END();
			
			nTotal += 26;
		}

		if (pEntity->pev->takedamage)
		{
			ClearMultiDamage();
			pEntity->TraceAttack( m_pPlayer->pev, flDamage, vecDir, &tr, DMG_SHOCK );
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
		}

		// ALERT( at_console, "%s\n", STRING( pEntity->pev->classname ));

		if ( pEntity->ReflectGauss() )
		{
			nReflect = 0.6; // 60 degrees
			
			if( ReflectMirror( tr, pEntity ) )
				nReflect = 0.98;
			
			pentIgnore = NULL;

			float n = -DotProduct(tr.vecPlaneNormal, vecDir);
		//	ALERT( at_console, "reflect %f\n", n );
			if (n < nReflect) 
			{
				// reflect
				Vector r;
			
				r = 2.0 * tr.vecPlaneNormal * n + vecDir;
				flMaxFrac = flMaxFrac - tr.flFraction;
				vecDir = r;
				vecSrc = tr.vecEndPos + vecDir * 8;
				vecDest = vecSrc + vecDir * 8192;

				// explode a bit
				m_pPlayer->RadiusDamage( tr.vecEndPos, pev, m_pPlayer->pev, flDamage * n, flDamage * n * 2.5, CLASS_NONE, DMG_BLAST );

				// bounce wall glow
				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x);		// pos
					WRITE_COORD( tr.vecEndPos.y);
					WRITE_COORD( tr.vecEndPos.z);
					WRITE_SHORT( m_iGlow );				// model
					WRITE_BYTE( flDamage * n * 0.5 );	// life * 10
					WRITE_BYTE( 2 );					// size * 10
					WRITE_BYTE( flDamage * n );			// brightness
				MESSAGE_END();

				nTotal += 13;
/*
				// balls
				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_SPRITETRAIL );// TE_RAILTRAIL);
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
					WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
					WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
					WRITE_SHORT( m_iBalls );		// model
					WRITE_BYTE( n * flDamage * 0.3 );				// count
					WRITE_BYTE( 10 );				// life * 10
					WRITE_BYTE( RANDOM_LONG( 1, 2 ) );				// size * 10
					WRITE_BYTE( 10 );				// amplitude * 0.1
					WRITE_BYTE( 20 );				// speed * 100
				MESSAGE_END();
*/
				nTotal += 21;

				// lose energy
				if (n == 0) n = 0.1;
				flDamage = flDamage * (1 - n);
			}
			else
			{
				// tunnel
				DecalGunshot( &tr, BULLET_MONSTER_12MM );

				// entry wall glow
				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x);	// pos
					WRITE_COORD( tr.vecEndPos.y);
					WRITE_COORD( tr.vecEndPos.z);
					WRITE_SHORT( m_iGlow );		// model
					WRITE_BYTE( 60 );				// life * 10
					WRITE_BYTE( 10 );				// size * 10
					WRITE_BYTE( flDamage );			// brightness
				MESSAGE_END();
				nTotal += 13;

				// limit it to one hole punch
				if (fHasPunched)
					break;
				fHasPunched = 1;

// DIFFUSION - I have removed the ability for gauss to punch through the wall with the secondary attack. 


				// try punching through wall if secondary attack (primary is incapable of breaking through)
//				if ( !m_fPrimaryFire )
//				{
//					UTIL_TraceLine( tr.vecEndPos + vecDir * 8, vecDest, dont_ignore_monsters, pentIgnore, &beam_tr);
//					if (!beam_tr.fAllSolid)
//					{
//						// trace backwards to find exit point
//						UTIL_TraceLine( beam_tr.vecEndPos, tr.vecEndPos, dont_ignore_monsters, pentIgnore, &beam_tr);
//
//						float n = (beam_tr.vecEndPos - tr.vecEndPos).Length( );
//
//						if (n < flDamage)
//						{
//							if (n == 0) n = 1;
//							flDamage -= n;
//
//							// ALERT( at_console, "punch %f\n", n );
//
//							// absorption balls
//							MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
//								WRITE_BYTE( TE_SPRITETRAIL );// TE_RAILTRAIL);
//								WRITE_COORD( tr.vecEndPos.x );
//								WRITE_COORD( tr.vecEndPos.y );
//								WRITE_COORD( tr.vecEndPos.z );
//								WRITE_COORD( tr.vecEndPos.x - vecDir.x );
//								WRITE_COORD( tr.vecEndPos.y - vecDir.y );
//								WRITE_COORD( tr.vecEndPos.z - vecDir.z );
//								WRITE_SHORT( m_iBalls );		// model
//								WRITE_BYTE( 3 );				// count
//								WRITE_BYTE( 10 );				// life * 10
//								WRITE_BYTE( RANDOM_LONG( 1, 2 ) );				// size * 10
//								WRITE_BYTE( 10 );				// amplitude * 0.1
//								WRITE_BYTE( 1 );				// speed * 100
//							MESSAGE_END();
//							nTotal += 21;
//
//							// exit blast damage
//							m_pPlayer->RadiusDamage( beam_tr.vecEndPos + vecDir * 8, pev, m_pPlayer->pev, flDamage, CLASS_NONE, DMG_BLAST );
//							CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), NORMAL_EXPLOSION_VOLUME, 3.0 );
//
//							DecalGunshot( &beam_tr, BULLET_MONSTER_12MM );
//							nTotal += 19;
//
//							// exit wall glow
//							MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, beam_tr.vecEndPos );
//								WRITE_BYTE( TE_GLOWSPRITE );
//								WRITE_COORD( beam_tr.vecEndPos.x);	// pos
//								WRITE_COORD( beam_tr.vecEndPos.y);
//								WRITE_COORD( beam_tr.vecEndPos.z);
//								WRITE_SHORT( m_iGlow );		// model
//								WRITE_BYTE( 60 );				// life * 10
//								WRITE_BYTE( 10 );				// size * 10
//								WRITE_BYTE( flDamage );			// brightness
//							MESSAGE_END();
//							nTotal += 13;
//
//							// balls
//							MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, beam_tr.vecEndPos );
//								WRITE_BYTE( TE_SPRITETRAIL );// TE_RAILTRAIL);
//								WRITE_COORD( beam_tr.vecEndPos.x );
//								WRITE_COORD( beam_tr.vecEndPos.y );
//								WRITE_COORD( beam_tr.vecEndPos.z );
//								WRITE_COORD( beam_tr.vecEndPos.x + vecDir.x );
//								WRITE_COORD( beam_tr.vecEndPos.y + vecDir.y );
//								WRITE_COORD( beam_tr.vecEndPos.z + vecDir.z );
//								WRITE_SHORT( m_iBalls );		// model
//								WRITE_BYTE( flDamage * 0.3 );				// count
//								WRITE_BYTE( 10 );				// life * 10
//								WRITE_BYTE( RANDOM_LONG( 1, 2 ) );				// size * 10
//								WRITE_BYTE( 20 );				// amplitude * 0.1
//								WRITE_BYTE( 40 );				// speed * 100
//							MESSAGE_END();
//							nTotal += 21;
//
//							vecSrc = beam_tr.vecEndPos + vecDir;
//						}
//					}
//					else      
//					{
//						 //ALERT( at_console, "blocked %f\n", n );
//						flDamage = 0;
//					} 
//				}    
//				else
//				{
//					//ALERT( at_console, "blocked solid\n" );
//				

						// slug doesn't punch through ever with primary 
						// fire, so leave a little glowy bit and make some balls
						MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
							WRITE_BYTE( TE_GLOWSPRITE );
							WRITE_COORD( tr.vecEndPos.x);	// pos
							WRITE_COORD( tr.vecEndPos.y);
							WRITE_COORD( tr.vecEndPos.z);
							WRITE_SHORT( m_iGlow );		// model
							WRITE_BYTE( 20 );				// life * 10
							WRITE_BYTE( 3 );				// size * 10
							WRITE_BYTE( 200 );			// brightness
						MESSAGE_END();

/*
						MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
							WRITE_BYTE( TE_SPRITETRAIL );// TE_RAILTRAIL);
							WRITE_COORD( tr.vecEndPos.x );
							WRITE_COORD( tr.vecEndPos.y );
							WRITE_COORD( tr.vecEndPos.z );
							WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
							WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
							WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
							WRITE_SHORT( m_iBalls );		// model
							WRITE_BYTE( 8 );				// count
							WRITE_BYTE( 6 );				// life * 10
							WRITE_BYTE( RANDOM_LONG( 1, 2 ) );	// size * 10
							WRITE_BYTE( 10 );				// amplitude * 0.1
							WRITE_BYTE( 20 );				// speed * 100
						MESSAGE_END();
						*/
					flDamage = 0;
//				}

			}
		}
		else
		{
			vecSrc = tr.vecEndPos + vecDir;
			pentIgnore = ENT( pEntity->pev );
		}
	}

	// ALERT( at_console, "%d bytes\n", nTotal );
}

void CGauss::WeaponIdle( void )
{
	ResetEmptySound( );

//	pev->skin = RANDOM_LONG(0,8);
//	ALERT( at_console, "%i\n", pev->skin );

	// play aftershock static discharge (no aftershock sounds in Diffusion)
/*	if (m_flPlayAftershock && m_flPlayAftershock < gpGlobals->time)
	{
		switch (RANDOM_LONG(0,3))
		{
		case 0:	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystalidle1.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 1:	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystalidle2.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 2:	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystalidle3.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 3:	break; // no sound
		}
		m_flPlayAftershock = 0.0;
	}
*/
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_fInAttack != 0)
	{
		StartFire();
		m_fInAttack = 0;
		m_flTimeWeaponIdle = gpGlobals->time + 2.0;
	}
/*	else
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.5)
		{
			iAnim = GAUSS_IDLE;
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
		}
		else if (flRand <= 0.75)
		{
			iAnim = GAUSS_IDLE;
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
		}
	}
*/
}

class CGaussAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CGaussAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_gaussammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_gaussammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_gaussclip, CGaussAmmo );