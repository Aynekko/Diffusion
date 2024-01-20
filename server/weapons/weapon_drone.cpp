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

class CWpnDrone : public CBasePlayerWeapon
{
	DECLARE_CLASS( CWpnDrone, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_DRONE + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
//	int SecondaryAmmoIndex( void );
	BOOL IsUseable(void);
	BOOL CanDeploy(void)
	{
		if( m_pPlayer->m_iHideHUD & (HIDEHUD_WPNS | HIDEHUD_ALL|HIDEHUD_WPNS_HOLDABLEITEM|HIDEHUD_WPNS_CUSTOM) )
			return FALSE;
		return TRUE;
	}
	BOOL Deploy( void );
	void Holster( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;
	bool CheckForDrone(void);
	int AddDuplicate( CBasePlayerItem *pOriginal );
	bool SpawnTest( void );

//	int ConfirmExplosion;
//	float ConfirmationTime;
	bool GotNewDrone;
	bool DroneDeployed;

	EHANDLE m_hTestModel;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( weapon_drone, CWpnDrone );

BEGIN_DATADESC( CWpnDrone )
//	DEFINE_FIELD( ConfirmExplosion, FIELD_INTEGER ),
//	DEFINE_FIELD( ConfirmationTime, FIELD_TIME ),
	DEFINE_FIELD( GotNewDrone, FIELD_BOOLEAN ),
	DEFINE_FIELD( DroneDeployed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hTestModel, FIELD_EHANDLE ),
END_DATADESC()

//=========================================================
//=========================================================
/*
int CWpnDrone::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}
*/

bool CWpnDrone::CheckForDrone(void)
{
	CBaseEntity *pDrone = NULL;

	if ((pDrone = UTIL_FindEntityByClassname( pDrone, "_playerdrone" )) != NULL)
	{
		m_pPlayer->DroneDeployed = true;
		return true;
	}
	else
	{
		m_pPlayer->DroneDeployed = false;
		return false;
	}
}

BOOL CWpnDrone::IsUseable(void)
{
	// HACKHACK if we are holding a radio stick right now and got a drone, switch to it
	if( !GotNewDrone && (m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0) )
	{
		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_drone.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_drone.mdl");
		SendWeaponAnim( DRONE_DEPLOY );
		GotNewDrone = true;
	}

	// sync
	DroneDeployed = m_pPlayer->DroneDeployed;

	// show test model
	if( m_pPlayer->CanShoot && m_pPlayer->CanUseDrone && !m_pPlayer->DroneDeployed && (m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] > 0) )
	{
		if( !m_hTestModel )
		{
			m_hTestModel = CBaseEntity::Create( "info_target", m_pPlayer->GetAbsOrigin(), g_vecZero, 0 );
			if( m_hTestModel )
			{
				SET_MODEL( m_hTestModel->edict(), "models/npc/drone_security.mdl" );
				m_hTestModel->pev->rendermode = kRenderTransColor;
				m_hTestModel->pev->renderamt = 175;
				m_hTestModel->pev->renderfx = kRenderFxFullbrightNoShadows;
				m_hTestModel->pev->skin = 1; // green eye
				m_hTestModel->pev->effects |= EF_NODRAW;
			}
		}
		else
		{
			if( (m_hTestModel->pev->effects & EF_NODRAW) && (gpGlobals->time > m_flNextPrimaryAttack) )
				m_hTestModel->pev->effects &= ~EF_NODRAW;

			Vector anglesAim = m_pPlayer->pev->v_angle;
			anglesAim.x *= 0.8;
			UTIL_MakeVectors( anglesAim );
			Vector SpawnPos = m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 50 + gpGlobals->v_up * 20;
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
		if( m_hTestModel )
			m_hTestModel->pev->effects |= EF_NODRAW;
	}

	return TRUE;
}

void CWpnDrone::Spawn( void )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/w_drone.mdl");
	m_iId = WEAPON_DRONE;

	m_iDefaultAmmo = 1;

	m_flTimeWeaponIdle = -1;

	FallInit();// get ready to fall down.
}

void CWpnDrone::Precache( void )
{
	PRECACHE_MODEL("models/v_drone.mdl");
	PRECACHE_MODEL("models/w_drone.mdl");
	PRECACHE_MODEL("models/p_drone.mdl");
	PRECACHE_MODEL("models/v_drone_radio.mdl");
	PRECACHE_MODEL("models/p_drone_radio.mdl");
	PRECACHE_SOUND("weapons/gauss_overcharge.wav");
	PRECACHE_SOUND("weapons/drone_call.wav");
	PRECACHE_SOUND("buttons_hl2/button17.wav");

	UTIL_PrecacheOther("_playerdrone");
}

int CWpnDrone::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "drone";
	p->iMaxAmmo1 = 1;// 3;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = WPN_SLOT_DRONE;
	p->iPosition = WPN_POS_DRONE;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DRONE;
	p->iWeight = MP5_WEIGHT;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY;

	return 1;
}

int CWpnDrone::AddToPlayer( CBasePlayer *pPlayer )
{
	if( pPlayer->DroneDeployed )
		return FALSE;
	
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
//=========================================================
int CWpnDrone::AddDuplicate( CBasePlayerItem *pOriginal )
{
	CWpnDrone *pWpnDrone;
	pWpnDrone = (CWpnDrone *)pOriginal;

	if( pWpnDrone->DroneDeployed )
	{
		// player has a drone deployed. Refuse to add more.
		return FALSE;
	}

	return CBasePlayerWeapon::AddDuplicate( pOriginal );
}

BOOL CWpnDrone::Deploy( void )
{
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + DRONE_DEPLOY_TIME;

//	ConfirmExplosion = 0;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 )
		GotNewDrone = true;

	// perform a check for an active drone to determine what we should deploy
	if( CheckForDrone() )
		return DefaultDeploy( "models/v_drone_radio.mdl", "models/p_drone_radio.mdl", DRONERADIO_DEPLOY, "default" );
	else
	{
		// do we have a drone at all? if not, just draw the radio 
		if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] <= 0 )
			return DefaultDeploy( "models/v_drone_radio.mdl", "models/p_drone_radio.mdl", DRONERADIO_DEPLOY, "default" );
		else
		{
			m_flTimeWeaponIdle = -1;
			return DefaultDeploy( "models/v_drone.mdl", "models/p_drone.mdl", DRONE_DEPLOY, "default");
		}
	}
}

void CWpnDrone::Holster(void)
{
	if( m_hTestModel != NULL )
		m_hTestModel->pev->effects |= EF_NODRAW;

	BaseClass::Holster();
}

void CWpnDrone::PrimaryAttack( void )
{	
	if( m_pPlayer->pev->waterlevel == 3 || !m_pPlayer->CanUseDrone )
	{
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/gauss_overcharge.wav", 1.0, ATTN_NORM, 0, 100 );
		m_flNextPrimaryAttack = gpGlobals->time + 1;
		if( !m_pPlayer->CanUseDrone )
			UTIL_ShowMessage( "UTIL_CANTSPAWN", m_pPlayer );
		return;
	}
	
	// we don't have an active drone on the map
	if( !m_pPlayer->DroneDeployed )
	{
		if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		{
			// no drones in the backpack, just make a false call
			SendWeaponAnim( DRONERADIO_CALL );
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/gauss_overcharge.wav", 1.0, ATTN_NORM, 0, 100);
			m_flNextPrimaryAttack = gpGlobals->time + 1;
			return;
		}
		
		// we have a drone in the backpack, try to spawn it
		if( !SpawnTest() ) // not enough space
		{
			UTIL_ShowMessage( "UTIL_CANTSPAWN", m_pPlayer );
			m_flNextPrimaryAttack = gpGlobals->time + 0.2;
			return;
		}

		Vector anglesAim = m_pPlayer->pev->v_angle;
		anglesAim.x *= 0.8;
		UTIL_MakeVectors( anglesAim );
		Vector SpawnPos = m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 50 + gpGlobals->v_up * 20;
		CBaseMonster *pDrone = (CBaseMonster*)Create( "_playerdrone", SpawnPos, g_vecZero, m_pPlayer->edict() );
		if(pDrone)
		{
			pDrone->m_iClass = CLASS_PLAYER_ALLY;
		//	pDrone->pev->targetname = MAKE_STRING( "testdrone" );

			// this is important, so the monsters could see the drone if the player is outside their PVS
			// see CBaseMonster :: RunAI for details
			pDrone->pev->effects |= EF_MERGE_VISIBILITY;

			// player gets this value when he picks up his flying drone.
			// when drone dies, or player loses all weapons, it's reset to 0
			float DroneHealth = m_pPlayer->DroneHealth;
			int DroneAmmo = (int)m_pPlayer->DroneAmmo;
			if( DroneAmmo == 0 )
				DroneAmmo = 500;

			if( DroneHealth == 0 )
				DroneHealth = 500;
			else if( DroneHealth < 25 )
				DroneHealth = 25; // let it live for a bit

			pDrone->pev->health = DroneHealth;
			pDrone->pev->max_health = 500;
			pDrone->m_iCounter = DroneAmmo;
			pDrone->pev->iuser3 = -662; // a flag so we can render drone's camera in viewmodel screen
			pDrone->pev->rendercolor = m_pPlayer->DroneColor; // apply custom color

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
			if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
				GotNewDrone = false;

			// drone spawned, switch to radio
			m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_drone_radio.mdl");
			m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_drone_radio.mdl");
			SendWeaponAnim( DRONERADIO_DEPLOY );
			m_pPlayer->DroneDeployed = true;
			m_flTimeWeaponIdle = gpGlobals->time;
			m_flNextSecondaryAttack = gpGlobals->time + 1;
		}
	}
	else // we have an active drone, try to call it to our location
	{
		if( !m_pPlayer->DroneControl )
		{
			CBaseEntity *pTarget = NULL;

			if( m_pPlayer->m_hDrone != NULL )
			{
				CBaseMonster *pDrone = m_pPlayer->m_hDrone->MyMonsterPointer();
				if( pDrone )
				{
					// drone to the rescue !!!
					EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/drone_call.wav", 0.5, ATTN_NORM, 0, 100 );
					SendWeaponAnim( DRONERADIO_CALL );
					pDrone->m_hTargetEnt = m_pPlayer; // <-- important, otherwise crash :)
					pDrone->ChangeSchedule( pDrone->GetScheduleOfType( SCHED_CHASE_OWNER ) );
				}
			}
			else
			{
				//			ALERT(at_console, "Can't find the drone!\n");
				// no one responded to our call, drone absent, switch state and viewmodel
				m_pPlayer->DroneDeployed = false;
				if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 )
				{
					m_pPlayer->pev->viewmodel = MAKE_STRING( "models/v_drone.mdl" );
					m_pPlayer->pev->weaponmodel = MAKE_STRING( "models/p_drone.mdl" );
					SendWeaponAnim( DRONE_DEPLOY );
				}
				else
				{
					SendWeaponAnim( DRONERADIO_CALL );
					EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/gauss_overcharge.wav", 1.0, ATTN_NORM, 0, 100 );
					m_flNextPrimaryAttack = gpGlobals->time + 1;
					return;
				}
			}
		}
	}

	// don't hold the button if not shooting
	if( !m_pPlayer->DroneControl )
		CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );

	m_flNextPrimaryAttack = gpGlobals->time + 1;
}

void CWpnDrone::SecondaryAttack( void )
{
	// don't hold the button
	CLIENT_COMMAND(m_pPlayer->edict(), "-attack2\n");
	
	if( !m_pPlayer->DroneDeployed )
		return;

	m_flNextSecondaryAttack = gpGlobals->time + 2;

	if( !m_pPlayer->DroneControl )
		m_pPlayer->DroneControl = true;
	else
		m_pPlayer->DroneControl = false;
/*
	// if the drone is on the map, on the first click we enter "waiting mode"
	// player has 3 seconds to click again to detonate the drone
	// if he didn't click, or switched the weapon, "waiting mode" is reset

	// ConfirmExplosion: 0 - not in waiting mode, 1 - in waiting mode

	if( ConfirmExplosion == 0 )
	{
		ConfirmationTime = gpGlobals->time + 3;
		SendWeaponAnim( DRONERADIO_CALL );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "buttons_hl2/button17.wav", 1.0, ATTN_NORM, 0, 80);
		ConfirmExplosion = 1;
	}

	if( (ConfirmExplosion == 2) && (gpGlobals->time < ConfirmationTime) )
	{
		CBaseEntity *pTarget = NULL;
		
		if ((pTarget = UTIL_FindEntityByClassname( pTarget, "_playerdrone" )) != NULL)
		{
			CBaseMonster *pDrone = pTarget->MyMonsterPointer();
			if( pDrone )
			{
				UTIL_ShowMessage( "UTIL_DRONEEXPLYES", m_pPlayer );
				// kill our guy :(
				pDrone->TakeDamage( VARS(eoNullEntity), VARS(eoNullEntity), pDrone->pev->health * 2, DMG_SHOCK );
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "buttons_hl2/button17.wav", 1.0, ATTN_NORM, 0, 100);
				SendWeaponAnim( DRONERADIO_CALL );
				ConfirmExplosion = 0;
				ConfirmationTime = 0;
				m_flNextSecondaryAttack = gpGlobals->time + 1;
			}
		}
	}
*/	
}

bool CWpnDrone::SpawnTest( void )
{
	TraceResult tracer;
	Vector anglesAim = m_pPlayer->pev->v_angle;
	anglesAim.x *= 0.75;
	UTIL_MakeVectors( anglesAim );
	//	Vector vecSrc = m_pPlayer->GetAbsOrigin();
	//	Vector vecEnd = vecSrc + gpGlobals->v_forward * 50;
	Vector vecSrc = m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 50 + gpGlobals->v_up * 20;

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

void CWpnDrone::WeaponIdle( void )
{
	/*
	if( ConfirmExplosion >= 1 )
	{	
		if( gpGlobals->time < ConfirmationTime - 2 )
		{
			ConfirmExplosion = 2;
			UTIL_ShowMessage( "UTIL_GRENSAFE1", m_pPlayer );
		}
		else if( gpGlobals->time < ConfirmationTime - 1 )
			UTIL_ShowMessage( "UTIL_GRENSAFE2", m_pPlayer );
		else if( gpGlobals->time < ConfirmationTime )
			UTIL_ShowMessage( "UTIL_GRENSAFE3", m_pPlayer );
		else if( gpGlobals->time > ConfirmationTime )
		{
			UTIL_ShowMessage( "UTIL_DRONEEXPLNO", m_pPlayer );
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/gauss_overcharge.wav", 1.0, ATTN_NORM, 0, 100);
			ConfirmExplosion = 0;
		}
	}*/
	
	if( m_flTimeWeaponIdle == -1 )
		return;
	
	// HACKHACK if our drone died and we have some in the backpack, switch to it
	if((m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0) && (m_pPlayer->DroneDeployed == false) )
	{
		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_drone.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_drone.mdl");
		SendWeaponAnim( DRONE_DEPLOY );
		m_flTimeWeaponIdle = -1;
	}
}
