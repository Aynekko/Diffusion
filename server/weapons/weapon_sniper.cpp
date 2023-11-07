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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "player.h"
#include "entities/soundent.h"
#include "game/gamerules.h"

#define SNIPER_MID_ZOOM 30
#define SNIPER_MAX_ZOOM 10

class CSniperRifle : public CBasePlayerWeapon
{
	DECLARE_CLASS( CSniperRifle, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_SNIPER + 1; }
	int GetItemInfo( ItemInfo *p );
	int AddToPlayer( CBasePlayer *pPlayer );
	void Holster( void );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int SecondaryAmmoIndex( void );
	BOOL Deploy( void );
	void Reload( void );
	void WeaponIdle( void );
	void ResetZoom( void );
	float m_flNextAnimTime;

	float m_iShotsFired;
	bool m_bDelayFire;

	int m_iShell;
};

LINK_ENTITY_TO_CLASS( weapon_sniper, CSniperRifle );


//=========================================================
//=========================================================
int CSniperRifle::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}

void CSniperRifle::ResetZoom( void )
{
	if( m_pPlayer->ZoomState > 0 )
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );
	
	m_pPlayer->ZoomState = 0;
	m_pPlayer->m_flFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_pPlayer->ZoomState );
		WRITE_BYTE( WEAPON_SNIPER );
	MESSAGE_END();
}

void CSniperRifle::Holster(void)
{
	// cancel any zooming in progress
	ResetZoom();

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	
	BaseClass::Holster();
}

void CSniperRifle::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_sniper.mdl" );
	m_iId = WEAPON_SNIPER;

	m_iDefaultAmmo = SNIPER_DEFAULT_GIVE; // two rounds

	FallInit();// get ready to fall down.
}

void CSniperRifle::Precache( void )
{
	PRECACHE_MODEL( "models/v_sniper.mdl" );
	PRECACHE_MODEL( "models/w_sniper.mdl" );
	PRECACHE_MODEL( "models/p_sniper.mdl" );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell TE_MODEL

	PRECACHE_MODEL( "models/w_9mmARclip.mdl" );
	
	PRECACHE_SOUND( "weapons/g36c_deploy.wav" );
	PRECACHE_SOUND( "weapons/g36c_safety.wav" );
	PRECACHE_SOUND( "weapons/sniper_magrelease.wav" );
	PRECACHE_SOUND( "weapons/sniper_magout.wav" );
	PRECACHE_SOUND( "weapons/sniper_magin.wav" );
	PRECACHE_SOUND( "weapons/sniper_boltpull.wav" );
	PRECACHE_SOUND( "weapons/sniper_boltrelease.wav" );
	PRECACHE_SOUND( "weapons/sniper_wpn_fire.wav" );
	PRECACHE_SOUND( "weapons/sniper_wpn_fire_d.wav" );
}

int CSniperRifle::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "sniperammo";
	p->iMaxAmmo1 = SNIPER_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SNIPER_MAX_CLIP;
	p->iSlot = WPN_SLOT_SNIPER;
	p->iPosition = WPN_POS_SNIPER;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SNIPER;
	p->iWeight = RPG_WEIGHT;

	return 1;
}

int CSniperRifle::AddToPlayer( CBasePlayer *pPlayer )
{
	if( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CSniperRifle::Deploy()
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = gpGlobals->time + SNIPER_DEPLOY_TIME;
	m_flNextSecondaryAttack = gpGlobals->time + SNIPER_DEPLOY_TIME;
	return DefaultDeploy( "models/v_sniper.mdl", "models/p_sniper.mdl", SNIPER_DRAW, "bow" );
}

void CSniperRifle::PrimaryAttack()
{
	// don't fire underwater
	if( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if( m_iClip <= 0 )
	{
		CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if( m_iClip <= 2 )
		LowAmmoMsg( m_pPlayer );

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

	SendWeaponAnim( SNIPER_SHOOT );
	m_flNextAnimTime = gpGlobals->time + 1;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/sniper_wpn_fire.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xf ) );
	PlayClientSound( m_pPlayer, WEAPON_SNIPER );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity()
		+ gpGlobals->v_right * RANDOM_FLOAT( 50, 70 )
		+ gpGlobals->v_up * RANDOM_FLOAT( 100, 150 )
		+ gpGlobals->v_forward * 25;

	EjectBrass( m_pPlayer->EyePosition()
		+ gpGlobals->v_up * -12
		+ gpGlobals->v_forward * 20
		+ gpGlobals->v_right * 4, vecShellVelocity,
		m_pPlayer->GetAbsAngles().y, m_iShell, TE_BOUNCE_SHELL );

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	MakeWeaponShake( m_pPlayer, WEAPON_SNIPER, 0 );

	float Cone = m_pPlayer->pev->velocity.Length() * 0.0005;

	Cone = bound( 0.001, Cone, 0.10 );

	int AddNonZoomedPunch = 0;
	if( m_pPlayer->ZoomState == 0 ) // non-zoomed player always gets a punch.
		AddNonZoomedPunch = 5;

	if( Cone < 0.0025 )
	{
		if( m_pPlayer->pev->flags & FL_DUCKING )
			Cone = 0.001;
		else
			Cone = 0.0025;
	}

	Cone += AddNonZoomedPunch * 0.01;

	m_pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector( Cone, Cone, Cone ), 16384, BULLET_PLAYER_12MM, 0, DMG_WPN_SNIPER );

//	m_pPlayer->AchievementStats[ACH_BULLETSFIRED]++;
	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, 0 );

	m_pPlayer->pev->punchangle.x = Cone * RANDOM_LONG( 25,30 ) + AddNonZoomedPunch + RANDOM_FLOAT( 1.5f, 2.0f );
	m_pPlayer->pev->punchangle.y = -Cone * RANDOM_LONG( 25,30 ) + AddNonZoomedPunch + RANDOM_FLOAT( 0.5f, 1.0f );

	if( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flNextPrimaryAttack = gpGlobals->time + SNIPER_NEXT_PA_TIME;

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
}

void CSniperRifle::SecondaryAttack( void )
{
	CLIENT_COMMAND( m_pPlayer->edict(), "-attack2\n" );

	if( !IS_DEDICATED_SERVER() )
	{
		if( CVAR_GET_FLOAT( "default_fov" ) <= SNIPER_MAX_ZOOM )
		{
			ALERT( at_error, "weapon_sniper can't zoom, default_fov is too low!\n" );
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
			WRITE_BYTE( WEAPON_SNIPER );
		MESSAGE_END();
		m_pPlayer->m_flFOV = SNIPER_MID_ZOOM;
	}
	else if( m_pPlayer->ZoomState == 1 ) // zoom to max level (fov 10)
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 2; // zooming in, second step
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_SNIPER );
		MESSAGE_END();
		m_pPlayer->m_flFOV = SNIPER_MAX_ZOOM;
	}
	else if( m_pPlayer->ZoomState == 2 ) // fully zoomed, unzoom fast
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 3; // zooming out
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_SNIPER );
		MESSAGE_END();
		m_pPlayer->ZoomState = 0; // hopefully we zoomed out by now. reset to default state
		m_pPlayer->m_flFOV = 0;
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );
	}
	
	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
//	m_flNextPrimaryAttack = gpGlobals->time + 0.5;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
}

void CSniperRifle::Reload( void )
{
	CLIENT_COMMAND( m_pPlayer->edict(), "-reload\n" );

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == SNIPER_MAX_CLIP )
		return;

	if( m_pPlayer->ZoomState > 0 )
		ResetZoom();

	if( !m_iClip )
		DefaultReload( SNIPER_MAX_CLIP, SNIPER_RELOAD_EMPTY, SNIPER_RELOADEMPTY_TIME );
	else
		DefaultReload( SNIPER_MAX_CLIP, SNIPER_RELOAD, SNIPER_RELOAD_TIME );
}

void CSniperRifle::WeaponIdle( void )
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if( m_flTimeWeaponIdle > gpGlobals->time )
		return;

//	SendWeaponAnim( ANIM_IDLE );

//	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );// how long till we do this again.
}



//==============================================================
// Ammo
//==============================================================
class CSniperAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CSniperAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{
		Precache();
		SET_MODEL( ENT( pev ), "models/w_9mmARclip.mdl" );
		CBasePlayerAmmo::Spawn();
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/w_9mmARclip.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		int bResult = (pOther->GiveAmmo( AMMO_SNIPER_GIVE, "sniperammo", SNIPER_MAX_CARRY ) != -1);

		if( bResult )
		//	EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
			PlayPickupSound( pOther );

		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_sniper, CSniperAmmo );
