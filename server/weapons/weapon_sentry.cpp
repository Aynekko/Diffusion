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

class CWpnSentry : public CBasePlayerWeapon
{
	DECLARE_CLASS( CWpnSentry, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_SENTRY + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
//	int SecondaryAmmoIndex( void );
	BOOL Deploy( void );
	void Holster( void );
	void WeaponIdle( void );

	bool SpawnTest( void );

	EHANDLE m_hTestModel;
	float RotateAngle;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( weapon_sentry, CWpnSentry );

BEGIN_DATADESC( CWpnSentry )
	DEFINE_FIELD( m_hTestModel, FIELD_EHANDLE ),
	DEFINE_FIELD( RotateAngle, FIELD_FLOAT ),
END_DATADESC()

//=========================================================
//=========================================================
/*
int CWpnSentry::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}
*/

void CWpnSentry::Spawn( void )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/weapons/w_sentry.mdl");
	m_iId = WEAPON_SENTRY;

	m_iDefaultAmmo = 1;

	FallInit();// get ready to fall down.
}


void CWpnSentry::Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_sentry.mdl");
	PRECACHE_MODEL("models/weapons/w_sentry.mdl");
	PRECACHE_MODEL("models/weapons/p_sentry.mdl");

//	PRECACHE_SOUND("items/clipinsert1.wav");
//	PRECACHE_SOUND("items/cliprelease1.wav");

//	PRECACHE_SOUND("weapons/ar2_secondary.wav");
//	PRECACHE_SOUND("weapons/ar2_reload.wav");
//	PRECACHE_SOUND("weapons/ar2_shoot.wav");

	UTIL_PrecacheOther("_playersentry");
}

int CWpnSentry::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "sentry";
	p->iMaxAmmo1 = 1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = WPN_SLOT_SENTRY;
	p->iPosition = WPN_POS_SENTRY;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SENTRY;
	p->iWeight = MP5_WEIGHT;

	return 1;
}

int CWpnSentry::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CWpnSentry::Deploy( )
{
	m_flNextPrimaryAttack = gpGlobals->time + SENTRY_DEPLOY_TIME;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	return DefaultDeploy( "models/weapons/v_sentry.mdl", "models/weapons/p_sentry.mdl", SENTRY_DEPLOY, "trip" );
}

void CWpnSentry::Holster(void)
{
	if( m_hTestModel != NULL )
		m_hTestModel->pev->effects |= EF_NODRAW;
	
	BaseClass::Holster();
}

bool CWpnSentry::SpawnTest( void )
{
	TraceResult tracer;
	Vector anglesAim = m_pPlayer->pev->v_angle;

	if( anglesAim.x > 15.0f ) // small hack for more intuitive turret placing at the floor
		anglesAim.x = 15.0f;

	anglesAim.x *= 0.75;
	UTIL_MakeVectors( anglesAim );
//	Vector vecSrc = m_pPlayer->GetAbsOrigin();
//	Vector vecEnd = vecSrc + gpGlobals->v_forward * 50;
	Vector vecSrc = m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 50 + gpGlobals->v_up * 10;

	UTIL_TraceHull( vecSrc, vecSrc, dont_ignore_monsters, human_hull, m_pPlayer->edict(), &tracer );
	if( (tracer.flFraction < 1.0f) || tracer.fAllSolid )
		return false;

	// don't spawn behind the glass!!!
	UTIL_TraceLine( m_pPlayer->GetAbsOrigin(), vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &tracer );
	if( (tracer.flFraction < 1.0f) || tracer.fAllSolid )
		return false;

	// last thing...
	if( UTIL_PointContents( vecSrc ) == CONTENTS_WATER )
		return false;

	return true;
}

void CWpnSentry::PrimaryAttack()
{	
	CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
	
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		RetireWeapon();
		return;
	}
	
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		return;
	}

	// perform spawn check
	if( !SpawnTest() )
	{
		UTIL_ShowMessage( "UTIL_CANTSPAWN", m_pPlayer );
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		return;
	}

	if( m_hTestModel != NULL )
		m_hTestModel->pev->effects |= EF_NODRAW;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/ar2_shoot.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110));

	Vector anglesAim = m_pPlayer->pev->v_angle;
	if( anglesAim.x > 15.0f ) // small hack for more intuitive turret placing at the floor
		anglesAim.x = 15.0f;

	anglesAim.x *= 0.75;
	UTIL_MakeVectors( anglesAim );
	Vector SpawnPos = m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 50 - gpGlobals->v_up * 10;
	CBaseMonster *pSentry = (CBaseMonster*)Create( "_playersentry", SpawnPos, Vector(0,RotateAngle,0), m_pPlayer->edict() );
	if( pSentry )
	{
		pSentry->m_iClass = CLASS_PLAYER_ALLY;
		pSentry->pev->targetname = MAKE_STRING( "_playersentry" ); // in case we want to delete them from the map or else
		if( g_pGameRules->IsMultiplayer() )
		{
			pSentry->pev->health = 300;
			pSentry->pev->max_health = 300;
		}
		else
		{
			pSentry->pev->health = 200;
			pSentry->pev->max_health = 200;
		}
	}

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		// last sentry was spawned, switch to another weapon right away
		RetireWeapon();
		return;
	}
	else
		SendWeaponAnim( SENTRY_DEPLOYSLOW );

	m_flNextPrimaryAttack = gpGlobals->time + 3;
	if (m_flNextPrimaryAttack < gpGlobals->time)
		m_flNextPrimaryAttack = gpGlobals->time + 3;

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
}

// rotate the turret before spawn, and also update the position
void CWpnSentry::SecondaryAttack(void)
{
	if( m_hTestModel == NULL )
		return;
	
	Vector anglesAim = m_pPlayer->pev->v_angle;
	if( anglesAim.x > 15.0f ) // small hack for more intuitive turret placing at the floor
		anglesAim.x = 15.0f;

	anglesAim.x *= 0.75;
	UTIL_MakeVectors( anglesAim );
	Vector vecSrc = m_pPlayer->GetAbsOrigin();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 50 - gpGlobals->v_up * 10;

	m_hTestModel->pev->rendercolor = Vector( 255, 255, 50 );

	Vector SentryAng = m_hTestModel->GetLocalAngles();
	RotateAngle += 100 * gpGlobals->frametime;
	if( RotateAngle > 360 ) RotateAngle = 0;
	SentryAng.y = RotateAngle;
	
	if( m_hTestModel->pev->effects & EF_NODRAW )
		m_hTestModel->pev->effects &= ~EF_NODRAW;
	m_hTestModel->SetAbsOrigin( vecEnd );
	m_hTestModel->SetLocalAngles( SentryAng );
}

void CWpnSentry::WeaponIdle( void )
{
	if( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		return;
	
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	// show test model
	if( m_pPlayer->CanShoot && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if( m_hTestModel == NULL )
		{
			m_hTestModel = CBaseEntity::Create( "info_target", m_pPlayer->GetAbsOrigin(), g_vecZero, 0 );
			if( m_hTestModel != NULL )
			{
				SET_MODEL( m_hTestModel->edict(), "models/sentry.mdl" );
				m_hTestModel->pev->rendermode = kRenderTransColor;
				m_hTestModel->pev->renderamt = 175;
				m_hTestModel->pev->renderfx = kRenderFxFullbrightNoShadows;
				m_hTestModel->pev->effects |= EF_SKIPPVS;
			}
		}
		else
		{
			if( m_hTestModel->pev->effects & EF_NODRAW )
				m_hTestModel->pev->effects &= ~EF_NODRAW;

			Vector anglesAim = m_pPlayer->pev->v_angle;
			if( anglesAim.x > 15.0f ) // small hack for more intuitive turret placing at the floor
				anglesAim.x = 15.0f;

			anglesAim.x *= 0.75;
			UTIL_MakeVectors( anglesAim );
			Vector SpawnPos = m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 50 - gpGlobals->v_up * 10;
			m_hTestModel->SetAbsOrigin( SpawnPos );
			m_hTestModel->RelinkEntity( FALSE );

			if( !SpawnTest() || (m_pPlayer->pev->waterlevel == 3) )
				m_hTestModel->pev->rendercolor = Vector( 255, 50, 50 );
			else
				m_hTestModel->pev->rendercolor = Vector( 50, 255, 50 );
		}
	}
	else
	{
		if( m_hTestModel != NULL )
			m_hTestModel->pev->effects |= EF_NODRAW;
	}

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	SendWeaponAnim( SENTRY_IDLE );

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );// how long till we do this again.
}
