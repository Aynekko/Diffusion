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

#define HANDGRENADE_PRIMARY_VOLUME	450

class CHandGrenade : public CBasePlayerWeapon
{
	DECLARE_CLASS( CHandGrenade, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_HANDGRENADE + 1; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack(void); // diffusion - non-timed throw
	bool SecondaryAttackPressed;
	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( void );
	void WeaponIdle( void );

	void HandgrenadeThink(void);

	float m_flStartThrow;
	float m_flReleaseThrow;

	bool WarningSound;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( weapon_handgrenade, CHandGrenade );

BEGIN_DATADESC( CHandGrenade )
	DEFINE_FIELD( WarningSound, FIELD_BOOLEAN ),
	DEFINE_FIELD( SecondaryAttackPressed, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( HandgrenadeThink ),
END_DATADESC();

void CHandGrenade::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_HANDGRENADE;
	SET_MODEL(ENT(pev), "models/weapons/w_grenade.mdl");

	pev->dmg = 100;
	m_iDefaultAmmo = HANDGRENADE_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CHandGrenade::Precache( void )
{
	PRECACHE_MODEL("models/weapons/w_grenade.mdl");
	PRECACHE_MODEL("models/weapons/v_grenade.mdl");
	PRECACHE_MODEL("models/weapons/p_grenade.mdl");
	PRECACHE_SOUND("weapons/gren_throw.wav");  // diffusion - new sounds
	PRECACHE_SOUND("weapons/gren_pull.wav");
	PRECACHE_SOUND("weapons/gauss_overcharge.wav");
}

int CHandGrenade::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hand Grenade";
	p->iMaxAmmo1 = HANDGRENADE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = WPN_SLOT_HANDGRENADE;
	p->iPosition = WPN_POS_HANDGRENADE;
	p->iId = m_iId = WEAPON_HANDGRENADE;
	p->iWeight = HANDGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CHandGrenade::Deploy( )
{
	m_flReleaseThrow = -1;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = gpGlobals->time + DEFAULT_DEPLOY_TIME;
	return DefaultDeploy( "models/weapons/v_grenade.mdl", "models/weapons/p_grenade.mdl", HANDGRENADE_DRAW, "crowbar" );
}

BOOL CHandGrenade::CanHolster( void )
{
	// can only holster hand grenades when not primed!
	return ( m_flStartThrow == 0 );
}

void CHandGrenade::Holster( void )
{
	DontThink();

	m_flStartThrow = 0; // diffusion - https://github.com/ValveSoftware/halflife/issues/3251
	
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		SendWeaponAnim( HANDGRENADE_HOLSTER );
	else
	{
		// no more grenades!
		m_pPlayer->RemoveWeapon( WEAPON_HANDGRENADE );
		SetThink( &CBasePlayerItem::DestroyItem );
		SetNextThink( 0.1 );
	}

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 0, ATTN_NORM);

	BaseClass::Holster();
}

void CHandGrenade::PrimaryAttack( void )
{
	if (!m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		SecondaryAttackPressed = false;
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim( HANDGRENADE_PINPULL );
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gren_pull.wav", 1.0, ATTN_NORM); //diffusion
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;

		SetThink(&CHandGrenade::HandgrenadeThink);
		SetNextThink( 0 );
	}
}

void CHandGrenade::SecondaryAttack(void)
{
	if (!m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		SecondaryAttackPressed = true;
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim( HANDGRENADE_PINPULL );
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gren_pull.wav", 1.0, ATTN_NORM); //diffusion
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
	}

	UTIL_ShowMessage( "UTIL_GRENSAFE1", m_pPlayer );
}

void CHandGrenade::HandgrenadeThink(void) // it's so stupid that it's almost genius
{
	if( m_flReleaseThrow > 0 )
	{
		WarningSound = false;
		SetThink( NULL );
		return;
	}
	
	if( gpGlobals->time > m_flStartThrow + 1.9 )
	{
		if( WarningSound == false )
		{
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/gauss_overcharge.wav", 1.0, ATTN_NORM, 0, 100 );
			WarningSound = true;
		}
		UTIL_ShowMessage( "UTIL_GRENNOTSAFE3", m_pPlayer );
	}
	else if( gpGlobals->time > m_flStartThrow + 1.6 )
		UTIL_ShowMessage( "UTIL_GRENNOTSAFE2", m_pPlayer );
	else if( gpGlobals->time > m_flStartThrow + 1.3 )
		UTIL_ShowMessage( "UTIL_GRENNOTSAFE1", m_pPlayer );
	else if( gpGlobals->time > m_flStartThrow + 0.9 )
		UTIL_ShowMessage( "UTIL_GRENSAFE3", m_pPlayer );
	else if( gpGlobals->time > m_flStartThrow + 0.6 )
		UTIL_ShowMessage( "UTIL_GRENSAFE2", m_pPlayer );
	else if( gpGlobals->time > m_flStartThrow + 0.3 )
		UTIL_ShowMessage( "UTIL_GRENSAFE1", m_pPlayer );

	if( gpGlobals->time > m_flStartThrow + 2.7)
	{
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		WarningSound = false;
		// achievement
		m_pPlayer->SendAchievementStatToClient( ACH_OVERCOOK, 1, ACHVAL_EQUAL );
		SetThink( NULL );
	}
	else
	{
		SetThink( &CHandGrenade::HandgrenadeThink );
		SetNextThink( 0.1 );
	}
}

void CHandGrenade::WeaponIdle( void )
{
	if (m_flReleaseThrow == 0)
		m_flReleaseThrow = gpGlobals->time;

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_flStartThrow)
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if( angThrow.x < 0 ) // throwing upwards
			angThrow.x = angThrow.x * 0.866f - 15; // ((90 - 12) / 90.0)
		else
			angThrow.x = angThrow.x * 1.111f - 10; // ((90 + 10) / 90.0)

		float flVelScale = gpGlobals->time - m_flStartThrow + 1.0f; // diffusion - by XaeroX, slightly edited for my needs
		float flVel = fabs(( 90.0f - angThrow.x ) * flVelScale * 2);

		if ( flVel > 500.0f )
			flVel = 500.0f;

		UTIL_MakeVectors( angThrow );

		Vector vecSrc = m_pPlayer->EyePosition() + gpGlobals->v_forward * 16;

		Vector PlayerVelocity = m_pPlayer->GetAbsVelocity();
		if( m_pPlayer->Dashed )
			PlayerVelocity *= 0.2; // don't apply huge velocity if thrown during dash
		Vector vecThrow = gpGlobals->v_forward * flVel + PlayerVelocity;

		// always explode 3 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 3.0;
		if (time < 0)
			time = 0;

		if (PlayerVelocity.Length() < 200)
			SendWeaponAnim( HANDGRENADE_THROW1 );
		else if (PlayerVelocity.Length() < 400)
			SendWeaponAnim( HANDGRENADE_THROW2 );
		else
			SendWeaponAnim( HANDGRENADE_THROW3 );

		CBaseEntity *pGrenade = NULL;
		
		pGrenade = CGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecThrow, SecondaryAttackPressed ? 3.0f : time );

		if( pGrenade )
		{
			SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
			pGrenade->pev->body = 1; // change to body without pin
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gren_throw.wav", 1.0, ATTN_NORM); //diffusion
		m_pPlayer->pev->punchangle.x = 4 * (gpGlobals->time - m_flStartThrow);
		if (m_pPlayer->pev->punchangle.x > 10)
			m_pPlayer->pev->punchangle.x = 10;

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if( m_pPlayer->LoudWeaponsRestricted )
			m_pPlayer->FireLoudWeaponRestrictionEntity();

		if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5;// ensure that the animation can finish playing
		}

		return;
	}
	else if (m_flReleaseThrow > 0)
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if( SecondaryAttackPressed )
			SecondaryAttackPressed = false;

		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
			SendWeaponAnim( HANDGRENADE_DRAW );
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
		m_flReleaseThrow = -1;
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.75)
		{
			iAnim = HANDGRENADE_IDLE;
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );// how long till we do this again.
		}
		else 
		{
			iAnim = HANDGRENADE_FIDGET;
			m_flTimeWeaponIdle = gpGlobals->time + 75.0 / 30.0;
		}

		SendWeaponAnim( iAnim );
	}
}