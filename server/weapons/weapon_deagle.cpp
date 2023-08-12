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
#include "weapons/weapons.h"
#include "monsters.h"
#include "player.h"
#include "game/gamerules.h"

enum python_e
{
	PYTHON_IDLE1 = 0,
	PYTHON_FIDGET,
	PYTHON_FIRE1,
	PYTHON_RELOAD,
	PYTHON_RELOAD_NOSHOT,
	PYTHON_HOLSTER,
	PYTHON_DRAW,
	PYTHON_IDLE2,
	PYTHON_IDLE3
};

class CPython : public CBasePlayerWeapon
{
	DECLARE_CLASS( CPython, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
	void Reload( void );
	void WeaponIdle( void );

	int m_iShell;
};

LINK_ENTITY_TO_CLASS( weapon_python, CPython );
LINK_ENTITY_TO_CLASS( weapon_357, CPython );

int CPython::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = DEAGLE_MAX_CLIP;
	p->iFlags = 0;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_DEAGLE;
	p->iWeight = PYTHON_WEIGHT;

	return 1;
}

int CPython::AddToPlayer( CBasePlayer *pPlayer )
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

void CPython::Spawn( void )
{
	pev->classname = MAKE_STRING("weapon_357"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_DEAGLE;
	SET_MODEL(ENT(pev), "models/w_357.mdl");

	m_iDefaultAmmo = PYTHON_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CPython::Precache( void )
{
	PRECACHE_MODEL("models/v_357.mdl");
	PRECACHE_MODEL("models/w_357.mdl");
	PRECACHE_MODEL("models/p_357.mdl");

	PRECACHE_MODEL("models/w_357ammobox.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND ("weapons/357_reload1.wav");
	PRECACHE_SOUND ("weapons/357_cock1.wav");
	PRECACHE_SOUND ("weapons/deagle_shot1.wav");
	PRECACHE_SOUND ("weapons/deagle_shot1_d.wav");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell
}

BOOL CPython::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1;
	return DefaultDeploy( "models/v_357.mdl", "models/p_357.mdl", PYTHON_DRAW, "python" );
}


void CPython::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
	m_flTimeWeaponIdle = gpGlobals->time + 10 + RANDOM_FLOAT ( 0, 5 );
	SendWeaponAnim( PYTHON_HOLSTER );
	BaseClass::Holster();
}

void CPython::PrimaryAttack()
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
	//	if (!m_fFireOnEmpty)
	//		Reload( );
	//	else
	//		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
	//		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		PlayEmptySound();

		return;
	}

	if (m_iClip <= 3)
		LowAmmoMsg(m_pPlayer);

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_iClip--;

	SendWeaponAnim( PYTHON_FIRE1 );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

//	switch(RANDOM_LONG(0,1))
//	{
//	case 0:
//		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_shot1.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM);
//		break;
//	case 1:
//		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_shot2.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM);
//		break;
//	}
	PlayClientSound( m_pPlayer, WEAPON_DEAGLE );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	m_pPlayer->FireBullets( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_357, 0 );
		
//	m_pPlayer->AchievementStats[ACH_BULLETSFIRED]++;
	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + 0.75;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );

	m_pPlayer->pev->punchangle.x -= 10;
	m_pPlayer->pev->punchangle.y += RANDOM_FLOAT(-2,2);
	m_pPlayer->pev->punchangle.z += 2;

	MakeWeaponShake( m_pPlayer, WEAPON_DEAGLE, 0 );

	// diffusion - added shell for deagle
	Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity() 
							 + gpGlobals->v_right * RANDOM_FLOAT(70,90) 
							 + gpGlobals->v_up * RANDOM_FLOAT(125,175) 
							 + gpGlobals->v_forward * 25;
	EjectBrass ( m_pPlayer->EyePosition() + gpGlobals->v_up * -12 + gpGlobals->v_forward * 32 + gpGlobals->v_right * 6,
	vecShellVelocity, m_pPlayer->GetAbsAngles().y, m_iShell, TE_BOUNCE_SHELL ); 
}


void CPython::Reload( void )
{
	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");
//	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/357_reload1.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );

	if( !m_iClip )
		DefaultReload( 7, PYTHON_RELOAD, 2.0 );
	else
		DefaultReload( 7, PYTHON_RELOAD_NOSHOT, 1.8 );
}


void CPython::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.5)
	{
		iAnim = PYTHON_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + (70.0/30.0);
	}
	else if (flRand <= 0.7)
	{
		iAnim = PYTHON_IDLE2;
		m_flTimeWeaponIdle = gpGlobals->time + (60.0/30.0);
	}
	else if (flRand <= 0.9)
	{
		iAnim = PYTHON_IDLE3;
		m_flTimeWeaponIdle = gpGlobals->time + (88.0/30.0);
	}
	else
	{
		iAnim = PYTHON_FIDGET;
		m_flTimeWeaponIdle = gpGlobals->time + (170.0/30.0);
	}
	SendWeaponAnim( iAnim );
}

class CPythonAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CPythonAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_357ammobox.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_357ammobox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_357BOX_GIVE, "357", _357_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_357, CPythonAmmo );