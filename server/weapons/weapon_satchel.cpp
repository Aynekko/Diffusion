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
#include "game/game.h"
#include "entities/soundent.h"

class CSatchelCharge : public CGrenade
{
	DECLARE_CLASS( CSatchelCharge, CGrenade );

	Vector m_lastBounceOrigin; // BugFixedHL - Used to fix a bug in engine: when object isn't moving, but its speed isn't 0 and on ground isn't set

	void Spawn( void );
	void Precache( void );
	void BounceSound( void );

	void SatchelSlide( CBaseEntity *pOther );
	void SatchelThink( void );
public:
	DECLARE_DATADESC();

	void Deactivate( void );
	float LastBounceSoundTime; // diffusion - don't play sound too often
};

LINK_ENTITY_TO_CLASS( monster_satchel, CSatchelCharge );

BEGIN_DATADESC( CSatchelCharge )
	DEFINE_FUNCTION( SatchelThink ),
	DEFINE_FUNCTION( SatchelSlide ),
	DEFINE_FIELD( LastBounceSoundTime, FIELD_TIME ),
	DEFINE_FIELD(m_lastBounceOrigin, FIELD_VECTOR),
END_DATADESC()

//=========================================================
// Deactivate - do whatever it is we do to an orphaned 
// satchel when we don't want it in the world anymore.
//=========================================================
void CSatchelCharge::Deactivate( void )
{
	pev->solid = SOLID_NOT;
	UTIL_Remove( this );
}

void CSatchelCharge :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/weapons/w_satchel.mdl");
	UTIL_SetSize(pev, Vector( -4, -4, -4), Vector(4, 4, 4));	// Uses point-sized, and can be stepped over

	SetTouch( &CSatchelCharge::SatchelSlide );
	SetUse( &CGrenade::DetonateUse );
	SetThink(&CSatchelCharge::SatchelThink );
	pev->nextthink = gpGlobals->time + 0.1;

	pev->gravity = 0.5;
	pev->friction = 0.8;

	pev->dmg = 150;
	// ResetSequenceInfo( );
	pev->sequence = 1;
}

void CSatchelCharge::SatchelSlide( CBaseEntity *pOther )
{
	entvars_t	*pevOther = pOther->pev;

	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

	// SetLocalAvelocity( Vector( 300, 300, 300 ));
	pev->gravity = 1;// normal gravity now

	// HACKHACK - On ground isn't always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,10), ignore_monsters, edict(), &tr );

	if ( tr.flFraction < 1.0 )
	{
		Vector vecVelocity = GetAbsVelocity();
		Vector vecAvelocity = GetLocalAvelocity();
		// add a bit of static friction
		SetAbsVelocity( vecVelocity * 0.95 );
		SetAbsAvelocity( vecAvelocity * 0.9 );
		// play sliding sound, volume based on velocity
	}
	if (!(pev->flags & FL_ONGROUND) && GetAbsVelocity().Length2D() > 10)
	{
		// Fix for a bug in engine: when object isn't moving, but its speed isn't 0 and on ground isn't set
		if (GetAbsOrigin() != m_lastBounceOrigin)
			BounceSound();
	}
	m_lastBounceOrigin = GetAbsOrigin();
	// There is no model animation so commented this out to prevent net traffic
	//StudioFrameAdvance( );
}

void CSatchelCharge :: SatchelThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if( !DoWaterCheck )
	{
		// spawned underwater, no need to splash
		if( pev->waterlevel > 0 )
			SendWaterSplash = true;

		DoWaterCheck = true;
	}

	if (pev->waterlevel == 3)
	{
		pev->movetype = MOVETYPE_FLY;
		Vector vecVelocity = GetAbsVelocity() * 0.8f;
		Vector vecAvelocity = GetLocalAvelocity() * 0.9f;
		vecVelocity.z += 8.0f;
		SetAbsVelocity( vecVelocity );
		SetAbsAvelocity( vecAvelocity );
		if( !SendWaterSplash )
		{
			SendWaterSplash = true; // not setting it to false after that, to prevent bounce spam on water
			Vector org = GetAbsOrigin();
			MakeWaterSplash( org + Vector( 0, 0, 512 ), org, 1 );
		}
	}
	else if (pev->waterlevel == 0)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity.z -= 8.0f;
		SetAbsVelocity( vecVelocity );
	}

	SetNextThink( 0.1 );
}

void CSatchelCharge :: Precache( void )
{
	PRECACHE_MODEL("models/weapons/w_satchel.mdl");
	PRECACHE_SOUND("weapons/g_bounce1.wav");
	PRECACHE_SOUND("weapons/g_bounce2.wav");
	PRECACHE_SOUND("weapons/g_bounce3.wav");
}

void CSatchelCharge :: BounceSound( void )
{
	if( gpGlobals->time > LastBounceSoundTime + 0.5 )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce1.wav", 1, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce2.wav", 1, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce3.wav", 1, ATTN_NORM);	break;
		}

		// make npcs aware
		CSoundEnt::InsertSound( bits_SOUND_DANGER, GetAbsOrigin(), 250, 0.1f );
		LastBounceSoundTime = gpGlobals->time;
	}
}

class CSatchel : public CBasePlayerWeapon
{
	DECLARE_CLASS( CSatchel, CBasePlayerWeapon );
public:
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_SATCHEL + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int AddDuplicate( CBasePlayerItem *pOriginal );
	BOOL CanDeploy( void );
	BOOL Deploy( void );
	BOOL IsUseable( void );
	
	void Holster( void );
	void WeaponIdle( void );
	void Throw( void );
	int m_chargeReady;	
};

LINK_ENTITY_TO_CLASS( weapon_satchel, CSatchel );

BEGIN_DATADESC( CSatchel )
	DEFINE_FIELD( m_chargeReady, FIELD_INTEGER ),
END_DATADESC()

//=========================================================
// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
//=========================================================
int CSatchel::AddDuplicate( CBasePlayerItem *pOriginal )
{
	CSatchel *pSatchel;

	if ( g_pGameRules->IsMultiplayer() )
	{
		pSatchel = (CSatchel *)pOriginal;

		if ( pSatchel->m_chargeReady != 0 )
		{
			// player has some satchels deployed. Refuse to add more.
			return FALSE;
		}
	}

	return CBasePlayerWeapon::AddDuplicate ( pOriginal );
}

//=========================================================
//=========================================================
int CSatchel::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );

	pPlayer->AddWeapon( m_iId );
	m_chargeReady = 0;// this satchel charge weapon now forgets that any satchels are deployed by it.

	if ( bResult )
		return AddWeapon( );

	return FALSE;
}

void CSatchel::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_SATCHEL;
	SET_MODEL(ENT(pev), "models/weapons/w_satchel.mdl");

	m_iDefaultAmmo = SATCHEL_DEFAULT_GIVE;
		
	FallInit();// get ready to fall down.
}


void CSatchel::Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_satchel.mdl");
	PRECACHE_MODEL("models/weapons/v_satchel_radio.mdl");
	PRECACHE_MODEL("models/weapons/w_satchel.mdl");
	PRECACHE_MODEL("models/weapons/p_satchel.mdl");
	PRECACHE_MODEL("models/weapons/p_satchel_radio.mdl");

	UTIL_PrecacheOther( "monster_satchel" );
}


int CSatchel::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Satchel Charge";
	p->iMaxAmmo1 = SATCHEL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = WPN_SLOT_SATCHEL;
	p->iPosition = WPN_POS_SATCHEL;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	p->iId = m_iId = WEAPON_SATCHEL;
	p->iWeight = SATCHEL_WEIGHT;

	return 1;
}

//=========================================================
//=========================================================
BOOL CSatchel::IsUseable( void )
{	
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		// player is carrying some satchels
		return TRUE;
	}

	if ( m_chargeReady != 0 )
	{
		// player isn't carrying any satchels, but has some out
		return TRUE;
	}

	// diffusion - finally fixed :)
	if( m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] <= 0 && m_chargeReady == 2 )
		RetireWeapon();

	return FALSE;
}

BOOL CSatchel::CanDeploy( void )
{
	if( m_pPlayer->m_iHideHUD & (HIDEHUD_WPNS | HIDEHUD_ALL|HIDEHUD_WPNS_HOLDABLEITEM| HIDEHUD_WPNS_CUSTOM) )
		return FALSE;
	
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		// player is carrying some satchels
		return TRUE;
	}

	if ( m_chargeReady != 0 )
	{
		// player isn't carrying any satchels, but has some out
		return TRUE;
	}

	return FALSE;
}

BOOL CSatchel::Deploy( void )
{
	if( m_chargeReady == 2 ) // diffusion - https://github.com/FreeSlave/hlsdk-xash3d/commit/fd207c9dff08a63b26fb9b408682e7d9e10aa680
		m_chargeReady = 0;
	
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
	m_flTimeWeaponIdle = gpGlobals->time + 4.0;

	if ( m_chargeReady )
		return DefaultDeploy( "models/weapons/v_satchel_radio.mdl", "models/weapons/p_satchel_radio.mdl", SATCHEL_RADIO_DRAW, "hive" );

	return DefaultDeploy( "models/weapons/v_satchel.mdl", "models/weapons/p_satchel.mdl", SATCHEL_DRAW, "trip" );
}

void CSatchel::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	
	if (m_chargeReady)
		SendWeaponAnim( SATCHEL_RADIO_HOLSTER );
	else
		SendWeaponAnim( SATCHEL_DROP );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 0, ATTN_NORM);

	if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && !m_chargeReady )
	{
		m_pPlayer->RemoveWeapon( WEAPON_SATCHEL );
		SetThink( &CBasePlayerItem::DestroyItem );
		SetNextThink( 0.1 );
	}

	BaseClass::Holster();
}

void CSatchel::PrimaryAttack( void )
{
	switch (m_chargeReady)
	{
	case 0:
	{
		Throw();
		m_flNextPrimaryAttack = gpGlobals->time + 0.3;
		m_flNextSecondaryAttack = gpGlobals->time + 1.0;
	}
	break;
	case 1:
	{
		SendWeaponAnim(SATCHEL_RADIO_FIRE);

		edict_t* edPlayer = m_pPlayer->edict();
		CBaseEntity* pSatchel = NULL;
		bool fire_restriction = false;

		while( (pSatchel = UTIL_FindEntityByClassname( pSatchel, "monster_satchel" )) != NULL )
		{
			if( pSatchel->pev->owner == edPlayer )
			{
				fire_restriction = true;
				CSatchelCharge *MySatchel = (CSatchelCharge *)pSatchel;
				
				// if player is too far from this charge, don't leave it in the world - delete it
				float dist = (m_pPlayer->GetAbsOrigin() - pSatchel->GetAbsOrigin()).Length();
				if( dist > 6000 )
				{
					MySatchel->Deactivate();
					continue;
				}

				// the value passed into Use() is the explosion delay
				// it kind of simulates the signal delay due to long distance :) (1000 units = 0.1 sec delay)
				pSatchel->Use( m_pPlayer, m_pPlayer, USE_ON, dist * 0.0001f );
			}
		}

		if( fire_restriction )
		{
			if( m_pPlayer->LoudWeaponsRestricted )
				m_pPlayer->FireLoudWeaponRestrictionEntity();
		}

		m_chargeReady = 2;
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flNextSecondaryAttack = gpGlobals->time + 0.5;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
		break;
	}
	case 2:
	{
		// we're reloading, don't allow fire
	}
	break;
	}
}

void CSatchel::SecondaryAttack( void )
{
	if (m_chargeReady != 2)
	{
		Throw();
		m_flNextPrimaryAttack = gpGlobals->time + 0.3;
		m_flNextSecondaryAttack = gpGlobals->time + 1.0;
	}
}


void CSatchel::Throw( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		Vector vecSrc = m_pPlayer->GetAbsOrigin();

		Vector vecThrow = gpGlobals->v_forward * 274 + m_pPlayer->GetAbsVelocity();
		if( m_pPlayer->GetGroundEntity( ))
			vecThrow += m_pPlayer->GetGroundEntity( )->GetAbsVelocity();

		CBaseEntity *pSatchel = Create( "monster_satchel", vecSrc, g_vecZero, m_pPlayer->edict() );
		pSatchel->SetAbsVelocity( vecThrow );
		Vector vecAvelocity = g_vecZero;
		vecAvelocity.y = 400;
		pSatchel->SetLocalAvelocity( vecAvelocity );

		m_pPlayer->pev->viewmodel = MAKE_STRING("models/weapons/v_satchel_radio.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/weapons/p_satchel_radio.mdl");
		SendWeaponAnim( SATCHEL_RADIO_DRAW );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_chargeReady = 1;
		
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	}
}

void CSatchel::WeaponIdle( void )
{
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	switch( m_chargeReady )
	{
	case 0:
		SendWeaponAnim( SATCHEL_FIDGET1 );
		// use tripmine animations
		strcpy( m_pPlayer->m_szAnimExtention, "trip" );
		break;
	case 1:
		SendWeaponAnim( SATCHEL_RADIO_FIDGET1 );
		// use hivehand animations
		strcpy( m_pPlayer->m_szAnimExtention, "hive" );
		break;
	case 2:
		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		{
		//	RetireWeapon(); // diffusion - now in IsUseable
			m_chargeReady = 0;
			return;
		}

		m_pPlayer->pev->viewmodel = MAKE_STRING("models/weapons/v_satchel.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/weapons/p_satchel.mdl");
		SendWeaponAnim( SATCHEL_DRAW );

		// use tripmine animations
		strcpy( m_pPlayer->m_szAnimExtention, "trip" );

		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flNextSecondaryAttack = gpGlobals->time + 0.5;
		m_chargeReady = 0;
		break;
	}
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );// how long till we do this again.
}

//=========================================================
// DeactivateSatchels - removes all satchels owned by
// the provided player. Should only be used upon death.
//
// Made this global on purpose.
//=========================================================
void DeactivateSatchels( CBasePlayer *pOwner )
{
	bool Explode = (mp_explodesatchels.value > 0) && (g_pGameRules->IsMultiplayer());
	
	edict_t *pFind; 

	pFind = FIND_ENTITY_BY_CLASSNAME( NULL, "monster_satchel" );

	while ( !FNullEnt( pFind ) )
	{
		CBaseEntity *pEnt = CBaseEntity::Instance( pFind );
		CSatchelCharge *pSatchel = (CSatchelCharge *)pEnt;

		if ( pSatchel )
		{
			if( pSatchel->pev->owner == pOwner->edict() )
			{
				if( Explode )
					pSatchel->Use( pOwner, pOwner, USE_ON, 0 );
				else
					pSatchel->Deactivate();
			}
		}

		pFind = FIND_ENTITY_BY_CLASSNAME( pFind, "monster_satchel" );
	}
}