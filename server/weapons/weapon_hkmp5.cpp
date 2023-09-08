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

enum WeaponHKMP5_e
{
	HKMP5_IDLE = 0,
	HKMP5_RELOAD,
	HKMP5_RELOAD_NOSHOT,
	HKMP5_DRAW,
	HKMP5_FIRE1,
	HKMP5_FIRE2,
	HKMP5_FIRE3,
};

class CWeaponHKMP5 : public CBasePlayerWeapon
{
	DECLARE_CLASS( CWeaponHKMP5, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_HKMP5 + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	BOOL Deploy( void );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;

	float m_iShotsFired;
	bool m_bDelayFire;

	int m_iShell;
};

LINK_ENTITY_TO_CLASS( weapon_hkmp5, CWeaponHKMP5 );


void CWeaponHKMP5::Spawn( void )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/w_mp5.mdl");
	m_iId = WEAPON_HKMP5;

	m_iDefaultAmmo = MP5_DEFAULT_GIVE; // two rounds

	FallInit();// get ready to fall down.
}

void CWeaponHKMP5::Precache( void )
{
	PRECACHE_MODEL("models/v_mp5.mdl");
	PRECACHE_MODEL("models/w_mp5.mdl");
	PRECACHE_MODEL("models/p_mp5.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shellTE_MODEL

	PRECACHE_MODEL("models/w_9mmARclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND("weapons/mp5_clipin.wav");
	PRECACHE_SOUND("weapons/mp5_clipout.wav");
	PRECACHE_SOUND("weapons/mp5_slideback.wav");

	PRECACHE_SOUND ("weapons/mp5-1.wav");
	PRECACHE_SOUND ("weapons/mp5-2.wav");
	PRECACHE_SOUND ("weapons/mp5-3.wav");
	PRECACHE_SOUND( "weapons/mp5-1_d.wav" );
	PRECACHE_SOUND( "weapons/mp5-2_d.wav" );
	PRECACHE_SOUND( "weapons/mp5-3_d.wav" );

	PRECACHE_SOUND ("weapons/357_cock1.wav");

	PRECACHE_MODEL("sprites/muzzleflash1.spr");
}

int CWeaponHKMP5::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "hkmp5ammo";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = MP5_MAX_CLIP;
	p->iSlot = WPN_SLOT_HKMP5;
	p->iPosition = WPN_POS_HKMP5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_HKMP5;
	p->iWeight = MP5_WEIGHT;

	return 1;
}

int CWeaponHKMP5::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CWeaponHKMP5::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1;
	return DefaultDeploy( "models/v_mp5.mdl", "models/p_mp5.mdl", HKMP5_DRAW, "mp5" );
}

void CWeaponHKMP5::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

	SendWeaponAnim( HKMP5_FIRE1 + RANDOM_LONG(0,2));
	m_flNextAnimTime = gpGlobals->time + 0.2;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	switch( RANDOM_LONG(0,2) )
//	{
//	case 0:	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/mp5-1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));	break;
//	case 1:	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/mp5-2.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));	break;
//	case 2: EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/mp5-3.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));	break;
//	}
	PlayClientSound( m_pPlayer, WEAPON_HKMP5, 0, (m_iClip <= 12 ? m_iClip : 0) );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	
	Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity() 
							 + gpGlobals->v_right * RANDOM_FLOAT(50,70) 
							 + gpGlobals->v_up * RANDOM_FLOAT(100,150) 
							 + gpGlobals->v_forward * 25;

	EjectBrass ( m_pPlayer->EyePosition()
					+ gpGlobals->v_up * -12 
					+ gpGlobals->v_forward * 20 
					+ gpGlobals->v_right * 6, vecShellVelocity,
					m_pPlayer->GetAbsAngles().y, m_iShell, TE_BOUNCE_SHELL); 
	
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	// diffusion - a bit of realism: precision changes at different walking speeds, + small shake :)
	MakeWeaponShake( m_pPlayer, WEAPON_HKMP5, 0 );
	
	float Cone = m_pPlayer->pev->velocity.Length() * 0.00025;
	
	Cone = bound( 0.02, Cone, 0.08 );

	if( Cone < 0.03 )
	{
		if( m_pPlayer->pev->flags & FL_DUCKING )
			Cone = 0.02;
		else
			Cone = 0.03;
	}

	m_pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector(Cone,Cone,Cone), 8192, BULLET_PLAYER_MP5, 0 );
	m_iClip--;

	if( m_iClip <= 10 )
		LowAmmoMsg( m_pPlayer );

//	m_pPlayer->AchievementStats[ACH_BULLETSFIRED]++;
	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, 0 );

	m_pPlayer->pev->punchangle.x += Cone * RANDOM_LONG(15,25);
	m_pPlayer->pev->punchangle.y += -Cone * RANDOM_LONG(-15,25);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + 0.1;
	if (m_flNextPrimaryAttack < gpGlobals->time)
		m_flNextPrimaryAttack = gpGlobals->time + 0.1;

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
 
}

void CWeaponHKMP5::Reload( void )
{
	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");

	if( !m_iClip )
		DefaultReload( 30, HKMP5_RELOAD, 2.5 );
	else
		DefaultReload( 30, HKMP5_RELOAD_NOSHOT, 2.0 );
}

void CWeaponHKMP5::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	SendWeaponAnim( HKMP5_IDLE );

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );// how long till we do this again.
}

class CHKMP5AmmoClip : public CBasePlayerAmmo
{
	DECLARE_CLASS( CHKMP5AmmoClip, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_9mmARclip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_9mmARclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_HKMP5_GIVE, "hkmp5ammo", _9MM_MAX_CARRY) != -1);

		if (bResult)
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );

		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_hkmp5, CHKMP5AmmoClip );
