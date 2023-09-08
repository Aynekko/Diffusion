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

enum glock_e
{
	GLOCK_IDLE1 = 0,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_ADD_SILENCER,
	GLOCK_SHOOT_SILENCER,
	GLOCK_SHOOT_EMPTY_SILENCER,
};

class CGlock : public CBasePlayerWeapon
{
	DECLARE_CLASS( CGlock, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_GLOCK + 1; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void GlockFire( float flSpread, float flCycleTime, BOOL fUseAutoAim );
	BOOL Deploy( void );
	void Reload( void );
	void WeaponIdle( void );
private:
	int m_iShell;

	bool ResetSilencer; // saverestore issues
};

LINK_ENTITY_TO_CLASS( weapon_glock, CGlock );
LINK_ENTITY_TO_CLASS( weapon_9mmhandgun, CGlock );

void CGlock::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_9mmhandgun" ); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_GLOCK;
	SET_MODEL( edict(), "models/w_9mmhandgun.mdl" );

	m_iDefaultAmmo = GLOCK_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CGlock::Precache( void )
{
	PRECACHE_MODEL("models/v_9mmhandgun.mdl");
	PRECACHE_MODEL("models/w_9mmhandgun.mdl");
	PRECACHE_MODEL("models/p_9mmhandgun.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

//	PRECACHE_SOUND ("weapons/pl_gun1.wav");//silenced handgun
//	PRECACHE_SOUND ("weapons/pl_gun2.wav");//silenced handgun
//	PRECACHE_SOUND ("weapons/pl_gun3.wav");//handgun
	PRECACHE_SOUND("weapons/pistol1.wav");
	PRECACHE_SOUND("weapons/pistol2.wav");
	PRECACHE_SOUND("weapons/pistol3.wav");
	PRECACHE_SOUND( "weapons/pistol1_d.wav" );
	PRECACHE_SOUND( "weapons/pistol2_d.wav" );
	PRECACHE_SOUND( "weapons/pistol3_d.wav" );
}

int CGlock::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLOCK_MAX_CLIP;
	p->iSlot = WPN_SLOT_GLOCK;
	p->iPosition = WPN_POS_GLOCK;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK;
	p->iWeight = GLOCK_WEIGHT;

	return 1;
}

BOOL CGlock::Deploy( )
{
	if( m_pPlayer->LoudWeaponsRestricted )
		pev->body = 1;
	else
		pev->body = 0;

	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1;
	return DefaultDeploy( "models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", GLOCK_DRAW, "onehanded" );
}

void CGlock::SecondaryAttack( void )
{
	if( m_iClip <= 0 )
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack2\n");

	GlockFire( 0.1, 0.2, FALSE );
}

void CGlock::PrimaryAttack( void )
{
	if( m_iClip <= 0 )
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
	
	GlockFire( 0.01, 0.3, TRUE );
}

void CGlock::GlockFire( float flSpread , float flCycleTime, BOOL fUseAutoAim )
{
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		}
		
		return;
	}

	if( m_iClip <= 6)
		LowAmmoMsg(m_pPlayer);

	m_iClip--;

	if (m_iClip != 0)
		SendWeaponAnim( GLOCK_SHOOT );
	else
		SendWeaponAnim( GLOCK_SHOOT_EMPTY );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
		
	Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity() 
							 + gpGlobals->v_right * RANDOM_FLOAT(50,70) 
							 + gpGlobals->v_up * RANDOM_FLOAT(100,150) 
							 + gpGlobals->v_forward * 25;
	EjectBrass ( m_pPlayer->EyePosition() + gpGlobals->v_up * -12 + gpGlobals->v_forward * 32 + gpGlobals->v_right * 10,
	vecShellVelocity, m_pPlayer->GetAbsAngles().y, m_iShell, TE_BOUNCE_SHELL ); 

	// silenced
	if (pev->body == 1)
	{
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

		switch(RANDOM_LONG(0,1))
		{
		case 0:
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/pl_gun1.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/pl_gun2.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
			break;
		}
	}
	else
	{
		// non-silenced
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	//	switch( RANDOM_LONG(0,2) )
	//	{
	//	case 0: EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/pistol1.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 90 + RANDOM_LONG(0,12)); break;
	//	case 1: EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/pistol2.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 90 + RANDOM_LONG(0,12)); break;
	//	case 2: EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/pistol3.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 90 + RANDOM_LONG(0,12)); break;
	//	}
		PlayClientSound( m_pPlayer, WEAPON_GLOCK );

		if( m_pPlayer->LoudWeaponsRestricted )
			m_pPlayer->FireLoudWeaponRestrictionEntity();
	}

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming;
	
	if ( fUseAutoAim )
		vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	else
		vecAiming = gpGlobals->v_forward;

	float Cone = m_pPlayer->pev->velocity.Length() * 0.00019;
	Cone = bound( 0.01, Cone, 0.05 );
	if( Cone < 0.02 )
	{
		if( m_pPlayer->pev->flags & FL_DUCKING )
			Cone = 0.01;
		else
			Cone = 0.0175;
	}

	m_pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector( flSpread + Cone, flSpread + Cone, flSpread + Cone ), 8192, BULLET_PLAYER_9MM, 0, DMG_WPN_PISTOL );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + flCycleTime;

//	m_pPlayer->AchievementStats[ACH_BULLETSFIRED]++;
	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );

	m_pPlayer->pev->punchangle.x += -Cone * 35;
	m_pPlayer->pev->punchangle.y += -Cone * RANDOM_LONG( -10, 20 );
}

void CGlock::Reload( void )
{
	int iResult;

	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");

	if (m_iClip == 0)
		iResult = DefaultReload( GLOCK_MAX_CLIP, GLOCK_RELOAD, 1.5 );
	else
		iResult = DefaultReload( GLOCK_MAX_CLIP, GLOCK_RELOAD_NOT_EMPTY, 1.5 );

	if (iResult)
	{
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
	}
}

void CGlock::WeaponIdle( void )
{
	if( !ResetSilencer )
	{
	//	SendWeaponAnim( GLOCK_IDLE1 );
		ResetSilencer = true;
	}
	
	if( m_pPlayer->LoudWeaponsRestricted && pev->body == 0 )
	{
		pev->body = 1;
		SendWeaponAnim( GLOCK_IDLE1 );
	}
	else if( !m_pPlayer->LoudWeaponsRestricted && pev->body == 1 )
	{
		pev->body = 0;
		SendWeaponAnim( GLOCK_IDLE1 );
	}
	
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.3 + 0 * 0.75)
		{
			iAnim = GLOCK_IDLE3;
			m_flTimeWeaponIdle = gpGlobals->time + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = GLOCK_IDLE1;
			m_flTimeWeaponIdle = gpGlobals->time + 60.0 / 16.0;
		}
		else
		{
			iAnim = GLOCK_IDLE2;
			m_flTimeWeaponIdle = gpGlobals->time + 40.0 / 16.0;
		}
		SendWeaponAnim( iAnim );
	}
}

class CGlockAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CGlockAmmo, CBasePlayerAmmo );

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
		if (pOther->GiveAmmo( AMMO_GLOCKCLIP_GIVE, "9mm", _9MM_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_glockclip, CGlockAmmo );
LINK_ENTITY_TO_CLASS( ammo_9mmclip, CGlockAmmo );