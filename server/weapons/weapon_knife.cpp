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

class CKnife : public CBasePlayerWeapon
{
	DECLARE_CLASS( CKnife, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_KNIFE + 1; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
};

LINK_ENTITY_TO_CLASS( weapon_knife, CKnife );

void CKnife::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_KNIFE;
	SET_MODEL(ENT(pev), "models/weapons/w_knife.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

void CKnife::Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_knife.mdl");
	PRECACHE_MODEL("models/weapons/w_knife.mdl");
	PRECACHE_MODEL("models/weapons/p_knife.mdl");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");
	PRECACHE_SOUND( "weapons/cbar_uw.wav" ); // "miss" underwater
}

int CKnife::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = WPN_SLOT_KNIFE;
	p->iPosition = WPN_POS_KNIFE;
	p->iId = WEAPON_KNIFE;
	p->iWeight = CROWBAR_WEIGHT;
	return 1;
}

BOOL CKnife::Deploy( )
{
	m_flNextPrimaryAttack = gpGlobals->time + KNIFE_DEPLOY_TIME;
	m_flNextSecondaryAttack = gpGlobals->time + KNIFE_DEPLOY_TIME;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	return DefaultDeploy( "models/weapons/v_knife.mdl", "models/weapons/p_knife.mdl", KNIFE_DRAW, "crowbar" );
}

void CKnife::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	SendWeaponAnim( KNIFE_HOLSTER );
	BaseClass::Holster();
}

// diffusion - I did the attack using FireBullets at short distance instead of tracehull.
// I didn't like that sometimes you can hit something on the side while aiming forward.
// with this method I can finally hit only what I aim for.

void CKnife::PrimaryAttack( void )
{
	m_pPlayer->pev->punchangle.x = RANDOM_FLOAT( 1, 2 );
	m_pPlayer->pev->punchangle.y = RANDOM_FLOAT( -2, -1 );

	// NOTENOTE we are checking the length of 75, but "firing bullets" at 90
	// because there are situations where the traceline makes a hit (flFraction > 0.992) but bullets still can't reach the surface
	// and we have a silent swoosh, with visible punchangle and no hit!
	// update: increased "bullet fire" more to make an easier hit.
	TraceResult trCenter;
	Vector tracevec = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 75;
	UTIL_TraceLine( m_pPlayer->GetGunPosition( ), tracevec, dont_ignore_monsters, ENT( m_pPlayer->pev ), &trCenter );
	
	if( trCenter.flFraction >= 1 )
	{
		if( m_pPlayer->pev->waterlevel == 3 )
		{
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/cbar_uw.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xF ) );
			UTIL_Bubbles( tracevec - Vector( 20, 20, 20 ), tracevec + Vector( 20, 20, 20 ), RANDOM_LONG( 8, 14 ) );
		}
		else
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xF ) );
	}
	else
	{
		UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
		m_pPlayer->FireBullets( 1, m_pPlayer->GetGunPosition( ), gpGlobals->v_forward, g_vecZero, 130, BULLET_PLAYER_CROWBAR, 0, DMG_WPN_KNIFE );
	}

	SendWeaponAnim( KNIFE_ATTACK1HIT );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + KNIFE_NEXT_PA_TIME;
}

void CKnife::SecondaryAttack( void )
{
	m_pPlayer->pev->punchangle.x = RANDOM_FLOAT( 2, 3 );
	m_pPlayer->pev->punchangle.y = RANDOM_FLOAT( -3, -2 );

	TraceResult tr;
	Vector tracevec = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 75;
	UTIL_TraceLine( m_pPlayer->GetGunPosition( ), tracevec, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );
	if( tr.flFraction >= 1 )
	{
		if( m_pPlayer->pev->waterlevel == 3 )
		{
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/cbar_uw.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xF ) );
			UTIL_Bubbles( tracevec - Vector( 20, 20, 20 ), tracevec + Vector( 20, 20, 20 ), RANDOM_LONG( 8, 14 ) );
		}
		else
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xF ) );
	}
	else
	{
		UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
		m_pPlayer->FireBullets( 1, m_pPlayer->GetGunPosition( ), gpGlobals->v_forward, g_vecZero, 130, BULLET_PLAYER_CROWBAR, 0, DMG_WPN_KNIFE * 2 );
	}

	SendWeaponAnim( KNIFE_ATTACK2HIT );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + KNIFE_NEXT_SA_TIME;
}
