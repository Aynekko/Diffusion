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

class CSmokeGrenade : public CBasePlayerWeapon
{
	DECLARE_CLASS( CSmokeGrenade, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_SMOKEGRENADE + 1; }
	int GetItemInfo( ItemInfo *p );

	void PrimaryAttack( void );
	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( void );
	void WeaponIdle( void );

	float m_flStartThrow;
	float m_flReleaseThrow;
};

LINK_ENTITY_TO_CLASS( weapon_smokegrenade, CSmokeGrenade );

void CSmokeGrenade::Spawn( void )
{
	Precache();
	m_iId = WEAPON_SMOKEGRENADE;
	SET_MODEL( ENT( pev ), "models/w_smoke.mdl" );

	m_iDefaultAmmo = HANDGRENADE_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CSmokeGrenade::Precache( void )
{
	PRECACHE_MODEL( "models/w_smoke.mdl" );
	PRECACHE_MODEL( "models/v_smoke.mdl" );
	PRECACHE_MODEL( "models/p_smoke.mdl" );
	PRECACHE_SOUND( "weapons/gren_throw.wav" );
	PRECACHE_SOUND( "weapons/gren_pull.wav" );
	PRECACHE_SOUND( "weapons/smoke_grenade.wav" );
}

int CSmokeGrenade::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "Smoke Grenade";
	p->iMaxAmmo1 = SMOKEGRENADE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = WPN_SLOT_SMOKEGRENADE;
	p->iPosition = WPN_POS_SMOKEGRENADE;
	p->iId = m_iId = WEAPON_SMOKEGRENADE;
	p->iWeight = HANDGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CSmokeGrenade::Deploy()
{
	m_flReleaseThrow = -1;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = gpGlobals->time + DEFAULT_DEPLOY_TIME;
	return DefaultDeploy( "models/v_smoke.mdl", "models/p_smoke.mdl", HANDGRENADE_DRAW, "crowbar" );
}

BOOL CSmokeGrenade::CanHolster( void )
{
	// can only holster hand grenades when not primed!
	return (m_flStartThrow == 0);
}

void CSmokeGrenade::Holster( void )
{
	DontThink();

	m_flStartThrow = 0; // diffusion - https://github.com/ValveSoftware/halflife/issues/3251

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		SendWeaponAnim( HANDGRENADE_HOLSTER );
	else
	{
		// no more grenades!
		m_pPlayer->RemoveWeapon( WEAPON_SMOKEGRENADE );
		SetThink( &CBasePlayerItem::DestroyItem );
		SetNextThink( 0.1 );
	}

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "common/null.wav", 0, ATTN_NORM );

	BaseClass::Holster();
}

void CSmokeGrenade::PrimaryAttack( void )
{
	if( !m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 )
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim( HANDGRENADE_PINPULL );
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/gren_pull.wav", 1.0, ATTN_NORM );
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
	}
}

void CSmokeGrenade::WeaponIdle( void )
{
	if( m_flReleaseThrow == 0 )
		m_flReleaseThrow = gpGlobals->time;

	if( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if( m_flStartThrow )
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if( angThrow.x < 0 )
			angThrow.x = -10 + angThrow.x * ((90 - 10) / 90.0);
		else
			angThrow.x = -10 + angThrow.x * ((90 + 10) / 90.0);

		float flVelScale = gpGlobals->time - m_flStartThrow + 1.0f; // diffusion - by XaeroX, slightly edited for my needs
		float flVel = fabs( (90.0f - angThrow.x) * flVelScale * 2 );

		if( flVel > 500.0f )
			flVel = 500.0f;

		UTIL_MakeVectors( angThrow );

		Vector vecSrc = m_pPlayer->EyePosition() + gpGlobals->v_forward * 16;

		Vector PlayerVelocity = m_pPlayer->GetAbsVelocity();
		if( m_pPlayer->Dashed )
			PlayerVelocity *= 0.2; // don't apply huge velocity if thrown during dash
		Vector vecThrow = gpGlobals->v_forward * flVel + PlayerVelocity;

		// always explode 3 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 3.0;
		if( time < 0 )
			time = 0;

		if( PlayerVelocity.Length() < 200 )
			SendWeaponAnim( HANDGRENADE_THROW1 );
		else if( PlayerVelocity.Length() < 400 )
			SendWeaponAnim( HANDGRENADE_THROW2 );
		else
			SendWeaponAnim( HANDGRENADE_THROW3 );

		CGrenade::ShootSmoke( m_pPlayer->pev, vecSrc, vecThrow );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/gren_throw.wav", 1.0, ATTN_NORM ); //diffusion
		m_pPlayer->pev->punchangle.x = 4 * (gpGlobals->time - m_flStartThrow);
		if( m_pPlayer->pev->punchangle.x > 10 )
			m_pPlayer->pev->punchangle.x = 10;

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextPrimaryAttack = gpGlobals->time + 0.5;// ensure that the animation can finish playing
		}

		return;
	}
	else if( m_flReleaseThrow > 0 )
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			SendWeaponAnim( HANDGRENADE_DRAW );
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
		m_flReleaseThrow = -1;
		return;
	}

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		int iAnim;
		float flRand = RANDOM_FLOAT( 0, 1 );
		if( flRand <= 0.75 )
		{
			iAnim = HANDGRENADE_IDLE;
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );// how long till we do this again.
		}
		else
		{
			iAnim = HANDGRENADE_FIDGET;
			m_flTimeWeaponIdle = gpGlobals->time + 75.0 / 30.0;
		}

		SendWeaponAnim( iAnim );
	}
}