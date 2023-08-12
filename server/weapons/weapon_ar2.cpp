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

enum mp555_e
{
	MP555_IDLE,
	MP555_LAUNCH,
	MP555_DEPLOY,
	MP555_FIRE,
	MP555_HOLSTER,
};

class C_AR2 : public CBasePlayerWeapon
{
	DECLARE_CLASS( C_AR2, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int SecondaryAmmoIndex( void );
	BOOL Deploy( void );
//	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;

	float m_iShotsFired;
	bool m_bDelayFire;

	int m_iShell;
};

LINK_ENTITY_TO_CLASS( weapon_ar2, C_AR2 );

//=========================================================
//=========================================================
int C_AR2::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}

void C_AR2::Spawn( void )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/w_ar2.mdl");
	m_iId = WEAPON_AR2;

	m_iDefaultAmmo = AR2_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void C_AR2::Precache( void )
{
	PRECACHE_MODEL("models/v_ar2.mdl");
	PRECACHE_MODEL("models/w_ar2.mdl");
	PRECACHE_MODEL("models/p_ar2.mdl");

	PRECACHE_SOUND("items/clipinsert1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	PRECACHE_SOUND("weapons/ar2_secondary.wav");
	PRECACHE_SOUND( "weapons/ar2_secondary_d.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot1.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot2.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot3.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot4.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot1_d.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot2_d.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot3_d.wav" );
	PRECACHE_SOUND( "drone/aliendrone_shoot4_d.wav" );
	PRECACHE_SOUND( "weapons/crystal_empty.wav" );

	UTIL_PrecacheOther("shock_beam");
	UTIL_PrecacheOther("playerball");
}

int C_AR2::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "ar2ammo";
	p->iMaxAmmo1 = AR2_MAX_CARRY;
	p->pszAmmo2 = "ar2balls";
	p->iMaxAmmo2 = M203_GRENADE_MAX_CARRY;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_AR2;
	p->iWeight = MP5_WEIGHT;

	return 1;
}

int C_AR2::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL C_AR2::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.5;
	return DefaultDeploy( "models/v_ar2.mdl", "models/p_ar2.mdl", MP555_DEPLOY, "mp5" );
}

void C_AR2::PrimaryAttack()
{	
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM );
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM );
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

	SendWeaponAnim( MP555_FIRE );
	m_flNextAnimTime = gpGlobals->time + 0.2;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	switch( RANDOM_LONG(0,3) )
//	{
//	case 0: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "drone/aliendrone_shoot1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ) ); break;
//	case 1: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "drone/aliendrone_shoot2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ) ); break;
//	case 2: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "drone/aliendrone_shoot3.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ) ); break;
//	case 3: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "drone/aliendrone_shoot4.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ) ); break;
//	}
	PlayClientSound( m_pPlayer, WEAPON_AR2 );

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );
	
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2 + gpGlobals->v_right * 6 + gpGlobals->v_forward * 8;
	Vector vecDir = gpGlobals->v_forward;

	MakeWeaponShake( m_pPlayer, WEAPON_AR2, 0 );
	
	float Cone = m_pPlayer->pev->velocity.Length() * 0.0001;
	
	if( Cone < 0.01 )
	{
		if( m_pPlayer->pev->flags & FL_DUCKING )
			Cone = 0.005;
		else
			Cone = 0.01;
	}

	if( Cone > 0.07 )
		Cone = 0.07;

	m_pPlayer->pev->punchangle.x = Cone * RANDOM_LONG(30,40);
	m_pPlayer->pev->punchangle.y = -Cone * RANDOM_LONG(30,40);

	CBaseEntity *pShock = CBaseEntity::Create( "shock_beam", vecSrc, anglesAim, m_pPlayer->edict() );
	pShock->SetLocalVelocity( vecDir * 5000 );
	pShock->pev->nextthink = gpGlobals->time;
	pShock->pev->owner = m_pPlayer->edict();
	pShock->pev->dmg = 15;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 15 )
		LowAmmoMsg( m_pPlayer );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + 0.1;
	if (m_flNextPrimaryAttack < gpGlobals->time)
		m_flNextPrimaryAttack = gpGlobals->time + 0.1;

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );

//	m_pPlayer->AchievementStats[ACH_BULLETSFIRED]++;
	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, 0 );
}

void C_AR2::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM );
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if( m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] <= 0 )
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM );
		CLIENT_COMMAND( m_pPlayer->edict(), "-attack2\n" );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;
			
	m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType]--;

	SendWeaponAnim( MP555_LAUNCH );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/ar2_secondary.wav", 1, ATTN_NORM);
	PlayClientSound( m_pPlayer, WEAPON_AR2, 1 );

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );
	
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2 + gpGlobals->v_right * 6 + gpGlobals->v_forward * 8;
	Vector vecDir = gpGlobals->v_forward;

	CBaseEntity *pBall = CBaseEntity::Create("playerball", vecSrc, anglesAim, m_pPlayer->edict());
	if( pBall )
	{
		pBall->SetLocalVelocity( vecDir * 1000 );
		pBall->pev->nextthink = gpGlobals->time;
	}

	MakeWeaponShake( m_pPlayer, WEAPON_AR2, 1 );
	
	m_flNextPrimaryAttack = gpGlobals->time + 1;
	m_flNextSecondaryAttack = gpGlobals->time + 1;
	m_flTimeWeaponIdle = gpGlobals->time + 5;// idle pretty soon after shooting.

	if (!m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType])
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_pPlayer->pev->punchangle.x -= 12;
}

void C_AR2::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	SendWeaponAnim( MP555_IDLE );

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );// how long till we do this again.
}


//===========================================================================================================

//======================================================
// ammo
//======================================================
class CAR2Ammo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CAR2Ammo, CBasePlayerAmmo );

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
		int bResult = (pOther->GiveAmmo( AR2_DEFAULT_GIVE, "ar2ammo", AR2_MAX_CARRY ) != -1);
		if( bResult )
		{
		//	EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
			PlayPickupSound( pOther );
		}
		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_ar2, CAR2Ammo );

//======================================================
// energy balls
//======================================================
class CAR2Ball : public CBasePlayerAmmo
{
	DECLARE_CLASS( CAR2Ball, CBasePlayerAmmo );

	void Spawn( void )
	{
		Precache();
		SET_MODEL( ENT( pev ), "models/w_ARgrenade.mdl" );
		CBasePlayerAmmo::Spawn();
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/w_ARgrenade.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		int bResult = (pOther->GiveAmmo( AMMO_M203BOX_GIVE, "ar2balls", M203_GRENADE_MAX_CARRY ) != -1);

		if( bResult )
		//	EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
			PlayPickupSound( pOther );

		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_ar2ball, CAR2Ball );