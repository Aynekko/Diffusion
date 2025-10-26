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

class CDeagle : public CBasePlayerWeapon
{
	DECLARE_CLASS( CDeagle, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_DEAGLE + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
	void Reload( void );
	void WeaponIdle( void );
};

LINK_ENTITY_TO_CLASS( weapon_python, CDeagle );
LINK_ENTITY_TO_CLASS( weapon_357, CDeagle );

int CDeagle::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = g_WpnAmmo[WEAPON_DEAGLE];
	p->iMaxAmmo1 = DEAGLE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = DEAGLE_MAX_CLIP;
	p->iFlags = 0;
	p->iSlot = WPN_SLOT_DEAGLE;
	p->iPosition = WPN_POS_DEAGLE;
	p->iId = m_iId = WEAPON_DEAGLE;
	p->iWeight = DEAGLE_WEIGHT;

	return 1;
}

int CDeagle::AddToPlayer( CBasePlayer *pPlayer )
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

void CDeagle::Spawn( void )
{
	pev->classname = MAKE_STRING("weapon_357"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_DEAGLE;
	SET_MODEL(ENT(pev), "models/weapons/w_deagle.mdl");

	m_iDefaultAmmo = DEAGLE_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CDeagle::Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_deagle.mdl");
	PRECACHE_MODEL("models/weapons/w_deagle.mdl");
	PRECACHE_MODEL("models/weapons/p_deagle.mdl");

//	PRECACHE_MODEL("models/weapons/w_357ammobox.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND ("weapons/357_cock1.wav");
	PRECACHE_SOUND ("weapons/deagle_shot1.wav");
	PRECACHE_SOUND ("weapons/deagle_shot1_d.wav");

	PRECACHE_SOUND( "weapons/de_clipout.wav" );
	PRECACHE_SOUND( "weapons/de_clipin.wav" );
	PRECACHE_SOUND( "weapons/de_slide.wav" );
	PRECACHE_SOUND( "weapons/de_draw.wav" );
	PRECACHE_SOUND( "weapons/de_back.wav" );
}

BOOL CDeagle::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + DEAGLE_DEPLOY_TIME;
	return DefaultDeploy( "models/weapons/v_deagle.mdl", "models/weapons/p_deagle.mdl", DEAGLE_DRAW, "python" );
}


void CDeagle::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
	m_flTimeWeaponIdle = gpGlobals->time + 10 + RANDOM_FLOAT ( 0, 5 );
	SendWeaponAnim( DEAGLE_HOLSTER );
	BaseClass::Holster();
}

void CDeagle::PrimaryAttack()
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

	if( m_iClip == 1 )
		SendWeaponAnim( DEAGLE_FIRE_EMPTY );
	else
		SendWeaponAnim( DEAGLE_FIRE1 );

	m_iClip--;

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
	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, ACHVAL_ADD );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + DEAGLE_NEXT_PA_TIME;
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
	EjectBrass ( m_pPlayer->EyePosition() + gpGlobals->v_up * -12 + gpGlobals->v_forward * 24 + gpGlobals->v_right * 4,
	vecShellVelocity, m_pPlayer->GetAbsAngles().y, SHELL_50AE, TE_BOUNCE_SHELL ); 
}


void CDeagle::Reload( void )
{
	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");

	if( !m_iClip )
		DefaultReload( 7, DEAGLE_RELOAD_EMPTY, DEAGLE_RELOADEMPTY_TIME );
	else
		DefaultReload( 7, DEAGLE_RELOAD, DEAGLE_RELOAD_TIME );
}


void CDeagle::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
}

class CDeagleAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CDeagleAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_deagle_clip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_deagle_clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_DEAGLEBOX_GIVE, g_WpnAmmo[WEAPON_DEAGLE], DEAGLE_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_357, CDeagleAmmo );
LINK_ENTITY_TO_CLASS( ammo_deagle, CDeagleAmmo );