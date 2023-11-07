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
#include "game/gamerules.h"

class CWpnFiveSeven : public CBasePlayerWeapon
{
	DECLARE_CLASS( CWpnFiveSeven, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_57 + 1; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void FiveSevenFire( float flSpread, float flCycleTime, BOOL fUseAutoAim );
	BOOL Deploy( void );
	void Reload( void );
	void WeaponIdle( void );
private:
	int m_iShell;
};

LINK_ENTITY_TO_CLASS( weapon_fiveseven, CWpnFiveSeven );

void CWpnFiveSeven::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_fiveseven" );
	Precache( );
	m_iId = WEAPON_FIVESEVEN;
	SET_MODEL( edict(), "models/w_fiveseven.mdl" );

	m_iDefaultAmmo = FIVESEVEN_MAX_CLIP * 2; // 2 rounds

	FallInit();// get ready to fall down.
}

void CWpnFiveSeven::Precache( void )
{
	PRECACHE_MODEL("models/v_fiveseven.mdl");
	PRECACHE_MODEL("models/w_fiveseven.mdl");
	PRECACHE_MODEL("models/p_fiveseven.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

	PRECACHE_SOUND("weapons/fiveseven_shoot.wav");
	PRECACHE_SOUND( "weapons/fiveseven_shoot_d.wav" );
}

int CWpnFiveSeven::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "ammo57";
	p->iMaxAmmo1 = 100;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = FIVESEVEN_MAX_CLIP;
	p->iSlot = WPN_SLOT_57;
	p->iPosition = WPN_POS_57;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_FIVESEVEN;
	p->iWeight = GLOCK_WEIGHT;

	return 1;
}

BOOL CWpnFiveSeven::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1;
	return DefaultDeploy( "models/v_fiveseven.mdl", "models/p_fiveseven.mdl", WPN57_DEPLOY, "onehanded" );
}

void CWpnFiveSeven::PrimaryAttack( void )
{
//	if( m_iClip <= 0 )
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
	
	FiveSevenFire( 0.008, 0.2, TRUE );
}

void CWpnFiveSeven::FiveSevenFire( float flSpread , float flCycleTime, BOOL fUseAutoAim )
{
	// don't fire underwater
	if( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		return;
	}
	
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		}
		
		return;
	}

	if( m_iClip <= 7)
		LowAmmoMsg(m_pPlayer);

	m_iClip--;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

	if (m_iClip != 0)
		SendWeaponAnim( WPN57_SHOOT );
	else
		SendWeaponAnim( WPN57_SHOOT_EMPTY );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
		
	Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity() 
							 + gpGlobals->v_right * RANDOM_FLOAT(50,70) 
							 + gpGlobals->v_up * RANDOM_FLOAT(100,150) 
							 + gpGlobals->v_forward * 25;
	EjectBrass ( m_pPlayer->EyePosition() + gpGlobals->v_up * -12 + gpGlobals->v_forward * 32 + gpGlobals->v_right * 10,
	vecShellVelocity, m_pPlayer->GetAbsAngles().y, m_iShell, TE_BOUNCE_SHELL ); 

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
//	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/fiveseven_shoot.wav", 1.0, ATTN_NORM, 0, 90 + RANDOM_LONG(0,12));
	PlayClientSound( m_pPlayer, WEAPON_FIVESEVEN );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming;
	
	if ( fUseAutoAim )
		vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	else
		vecAiming = gpGlobals->v_forward;

	float Cone = m_pPlayer->pev->velocity.Length() * 0.00015;
	Cone = bound( 0.005, Cone, 0.05 );
	if( Cone < 0.01 )
	{
		if( m_pPlayer->pev->flags & FL_DUCKING )
			Cone = 0.005;
		else
			Cone = 0.01;
	}

	m_pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector( flSpread + Cone, flSpread + Cone, flSpread + Cone ), 8192, BULLET_PLAYER_9MM, 0, DMG_WPN_FIVESEVEN ); // 18 dmg
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + flCycleTime;

//	m_pPlayer->AchievementStats[ACH_BULLETSFIRED]++;
	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );

	m_pPlayer->pev->punchangle.x += -Cone * 20;
	m_pPlayer->pev->punchangle.y += -Cone * RANDOM_LONG( -10, 20 );
}

void CWpnFiveSeven::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == 20 )
		return;

	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");

	DefaultReload( 20, WPN57_RELOAD, 3.0 );

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
}

void CWpnFiveSeven::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
		SendWeaponAnim( WPN57_IDLE );
}

class CWpnFiveSevenAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CWpnFiveSevenAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_9mmclip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_9mmclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_FIVESEVEN_GIVE, "ammo57", _57_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_fiveseven, CWpnFiveSevenAmmo );
