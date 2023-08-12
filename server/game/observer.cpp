//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
// observer.cpp
//
#include	"extdll.h"
#include	"game/game.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons/weapons.h"
#include	"pm_defs.h"

#define NEXT_OBSERVER_INPUT_DELAY	0.5

extern int gmsgCurWeapon;
extern int gmsgSetFOV;
extern int gmsgTeamInfo;
extern int gmsgSpectator;

extern int g_teamplay;

//=========================================================
// StartObserver: player has become a spectator. Set it up.
//=========================================================
void CBasePlayer::StartObserver(void)
{
	// clear any clientside entities attached to this player
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_KILLPLAYERATTACHMENTS);
		WRITE_BYTE(ENTINDEX(edict()));	// index number of primary entity
	MESSAGE_END();

	// Holster weapon immediately, to allow it to cleanup
	if (m_pActiveItem)
		m_pActiveItem->Holster();

	// Let go of tanks
	if (m_pTank != NULL)
		m_pTank->Use(this, this, USE_OFF, 0);

	// Remove all the player's stuff
	RemoveAllItems(FALSE);

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
		WRITE_BYTE(0);
		WRITE_BYTE(0);
		WRITE_BYTE(0);
	MESSAGE_END();

	// reset FOV
	pev->fov = m_flFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
		WRITE_BYTE((int)m_flFOV);
	MESSAGE_END();

	// Store view offset to use it later
	Vector view_ofs = pev->view_ofs;

	// Setup flags
	m_iHideHUD = HIDEHUD_WPNS | HIDEHUD_HEALTH | HIDEHUD_FLASHLIGHT;
	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->effects = EF_NODRAW;
	pev->view_ofs = g_vecZero;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NOCLIP;
	ClearBits(m_afPhysicsFlags, PFLAG_DUCKING);
	ClearBits(pev->flags, FL_DUCKING);
	pev->deadflag = DEAD_RESPAWNABLE;
	pev->health = 1;
	pev->flags |= FL_SPECTATOR;
	ShieldOn = false;

	// Clear out the status bar
	m_fInitHUD = TRUE;

	// Clear welcome cam status
	m_bInWelcomeCam = FALSE;

	// Move player to same view position he had on entering spectator
	UTIL_SetOrigin(this, pev->origin + view_ofs);

	// Delay between observer inputs
	m_flNextObserverInput = 0.5;

	// Setup spectator mode
	Observer_SetMode(m_iObserverMode);

	// Update Team Status
	//pev->team = 0;
	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
		WRITE_BYTE(ENTINDEX(edict()));	// index number of primary entity
		WRITE_STRING("");
	MESSAGE_END();

	// Send Spectator message (it is not used in client dll)
	MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
		WRITE_BYTE(ENTINDEX(edict()));	// index number of primary entity
		WRITE_BYTE(1);
	MESSAGE_END();
}

//=========================================================
// StopObserver: leave observer mode
//=========================================================
void CBasePlayer::StopObserver(void)
{
	// Turn off spectator
	pev->iuser1 = pev->iuser2 = 0;
	m_iHideHUD = 0;

	GetClassPtr((CBasePlayer*)pev)->Spawn();
	pev->nextthink = -1;

	// Send Spectator message (it is not used in client dll)
	MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
	WRITE_BYTE(ENTINDEX(edict()));	// index number of primary entity
		WRITE_BYTE(0);
	MESSAGE_END();

	// Update Team Status
	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
	WRITE_BYTE(ENTINDEX(edict()));	// index number of primary entity
	if (g_teamplay)
		WRITE_STRING(TeamID());
	else
		WRITE_STRING("Players");
	MESSAGE_END();
}

//=========================================================
// Observer_SetMode: attempt to change the observer mode
//=========================================================
void CBasePlayer::Observer_SetMode(int iMode)
{
	// Just abort if we're changing to the mode we're already in
	if (iMode == pev->iuser1)
		return;

	// is valid mode ?
	if (iMode < OBS_CHASE_LOCKED || iMode > OBS_MAP_CHASE)
		iMode = OBS_ROAMING; // now it is

	if (iMode == OBS_ROAMING && m_hObserverTarget)
	{
		// Set view point at same place where we may be looked in First Person mode
		pev->angles = m_hObserverTarget->pev->v_angle;
		pev->fixangle = TRUE;
		// Compensate view offset
		UTIL_SetOrigin(this, m_hObserverTarget->pev->origin + m_hObserverTarget->pev->view_ofs);
	}

	// set spectator mode
	m_iObserverMode = iMode;
	pev->iuser1 = iMode;
	pev->iuser3 = 0;

	Observer_CheckTarget();

	// print spectator mode on client screen
	string_t ModeTXT;

	switch (pev->iuser1)
	{
		default:
		case 0: ModeTXT = MAKE_STRING("NONE"); break;
		case 1: ModeTXT = MAKE_STRING("Locked chase camera"); break;
		case 2: ModeTXT = MAKE_STRING("Free chase camera"); break;
		case 3: ModeTXT = MAKE_STRING("Roaming"); break;
		case 4: ModeTXT = MAKE_STRING("First person"); break;
		case 5: ModeTXT = MAKE_STRING("Map free"); break;
		case 6: ModeTXT = MAKE_STRING("Map chase"); break;
	}

	ClientPrint(pev, HUD_PRINTCENTER, UTIL_VarArgs("Spectator mode: %s\n", STRING(ModeTXT)));
}

//=======================================================================================
// Observer_FindNextPlayer: find the next client in the game for this player to spectate.
//=======================================================================================
void CBasePlayer::Observer_FindNextPlayer( bool bReverse )
{
	// MOD AUTHORS: Modify the logic of this function if you want to restrict the observer to watching
	//				only a subset of the players. e.g. Make it check the target's team.

	int		iStart;
	if ( m_hObserverTarget )
		iStart = ENTINDEX( m_hObserverTarget->edict() );
	else
		iStart = ENTINDEX( edict() );
	int iCurrent = iStart;
	m_hObserverTarget = NULL;
	int iDir = bReverse ? -1 : 1; 

	do
	{
		iCurrent += iDir;

		// Loop through the clients
		if (iCurrent > gpGlobals->maxClients)
			iCurrent = 1;
		if (iCurrent < 1)
			iCurrent = gpGlobals->maxClients;

		CBaseEntity *pEnt = UTIL_PlayerByIndex( iCurrent );
		if ( !pEnt )
			continue;
		if ( pEnt == this )
			continue;
		// Don't spec observers or players who haven't picked a class yet
		if ( ((CBasePlayer*)pEnt)->IsObserver() || (pEnt->pev->effects & EF_NODRAW) )
			continue;

		// MOD AUTHORS: Add checks on target here.

		m_hObserverTarget = pEnt;
		break;

	} while ( iCurrent != iStart );

	// Did we find a target?
	if ( m_hObserverTarget )
	{
		// Move to the target
		UTIL_SetOrigin( this, m_hObserverTarget->pev->origin );

		// ALERT( at_console, "Now Tracking %s\n", STRING( m_hObserverTarget->pev->netname ) );

		// Store the target in pev so the physics DLL can get to it
		if (pev->iuser1 != OBS_ROAMING)
			pev->iuser2 = ENTINDEX( m_hObserverTarget->edict() );
	}
}

//=================================================================
// Observer_FindNextSpot: find the next spot and move spectator to it
//=================================================================
void CBasePlayer::Observer_FindNextSpot(bool bReverse)
{
	const int classesCount = 4;
	char* classes[] = { "info_intermission", "info_player_coop", "info_player_start", "info_player_deathmatch" };
	vec_t offsets[] = { 0, VEC_VIEW.z, VEC_VIEW.z, VEC_VIEW.z };	// View offset for spots (will looks like we spawn)

	int iStartClass = 0;
	if (m_hObserverTarget)
	{
		// Get current target's class
		const char* name = STRING(m_hObserverTarget->edict()->v.classname);
		// Pick starting class index
		for (int i = 0; i < classesCount; i++)
		{
			if (!strcmp(classes[i], name))
			{
				iStartClass = i;
				break;
			}
		}
	}

	CBaseEntity* pSpot = m_hObserverTarget, * pResultSpot = NULL;
	vec_t iResultSpotOffset;

	for (int i = 0; i < classesCount; i++)
	{
		int current = iStartClass + (bReverse ? -i : i);
		if (current >= classesCount)
			current -= classesCount;
		if (current < 0)
			current += classesCount;

		pSpot = UTIL_FindEntityByClassname(pSpot, classes[current], pSpot == NULL, bReverse);
		if (!pSpot)
			continue;

		// Spot found
		pResultSpot = pSpot;
		iResultSpotOffset = offsets[current];
		break;
	}

	if (!pResultSpot)
	{
		// Overlook a player (this will never happen actually)
		Observer_FindNextPlayer(bReverse);
		return;
	}
	m_hObserverTarget = pResultSpot;

	// Move player there
	UTIL_SetOrigin(this, m_hObserverTarget->pev->origin + Vector(0, 0, iResultSpotOffset));
	// Find target for intermission
	edict_t* pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(m_hObserverTarget->pev->target));
	if (pTarget && !FNullEnt(pTarget))
	{
		// Calculate angles to look at camera target
		pev->angles = UTIL_VecToAngles(pTarget->v.origin - m_hObserverTarget->pev->origin);
		pev->angles.x = -pev->angles.x;
	}
	else
	{
		pev->angles = m_hObserverTarget->pev->angles;
	}
	pev->fixangle = TRUE;
}

//===========================================================
// Observer_HandleButtons: handle buttons in observer mode
//===========================================================
void CBasePlayer::Observer_HandleButtons()
{
	// Slow down mouse clicks
	if (m_flNextObserverInput > gpGlobals->time)
		return;

	// Jump changes from modes: Chase to Roaming
	if (m_afButtonPressed & IN_JUMP)
	{
		int iMode;
		
		// UNDONE: OBS_MAP_FREE and OBS_MAP_CHASE don't work, obviously
		switch (pev->iuser1)
		{
		case OBS_CHASE_LOCKED: iMode = OBS_CHASE_FREE; break;
		case OBS_CHASE_FREE: iMode = OBS_IN_EYE; break;
		case OBS_IN_EYE: iMode = OBS_ROAMING; break;
		case OBS_ROAMING: iMode = OBS_CHASE_LOCKED; break;
		default: iMode = OBS_ROAMING; break;
		}

		Observer_SetMode(iMode);

		m_flNextObserverInput = gpGlobals->time + 0.2;
	}

	// Attack moves to the next player
	if (m_afButtonPressed & IN_ATTACK)//&& pev->iuser1 != OBS_ROAMING )
	{
		Observer_FindNextPlayer(false);

		m_flNextObserverInput = gpGlobals->time + 0.2;
	}

	// Attack2 moves to the prev player
	if (m_afButtonPressed & IN_ATTACK2)// && pev->iuser1 != OBS_ROAMING )
	{
		Observer_FindNextPlayer(true);

		m_flNextObserverInput = gpGlobals->time + 0.2;
	}
}

//===========================================================
// Observer_CheckTarget: check up on the state of the player
// that we are currently spectating
//===========================================================
void CBasePlayer::Observer_CheckTarget()
{
	if (pev->iuser1 == OBS_ROAMING)
		return;

	// try to find a traget if we have no current one
	if (m_hObserverTarget == NULL)
	{
		Observer_FindNextPlayer(false);

		if (m_hObserverTarget == NULL)
		{
			// no target found at all 
			int lastMode = pev->iuser1;
			Observer_SetMode(OBS_ROAMING);
			m_iObserverLastMode = lastMode;	// don't overwrite users lastmode
			return;	// we still have no target return
		}
	}

	CBasePlayer* target = (CBasePlayer*)(UTIL_PlayerByIndex(ENTINDEX(m_hObserverTarget->edict())));

	if (!target)
	{
		Observer_FindNextPlayer(false);
		return;
	}

	// check target
	if (target->pev->deadflag == DEAD_DEAD)
	{
		if ((target->m_fDeadTime + 2.0f) < gpGlobals->time)
		{
			// 3 secs after death change target
			Observer_FindNextPlayer(false);
			return;
		}
	}
}

void CBasePlayer::Observer_CheckProperties()
{
	// try to find a traget if we have no current one
	if (pev->iuser1 == OBS_IN_EYE && m_hObserverTarget != NULL)
	{
		CBasePlayer* target = (CBasePlayer*)(UTIL_PlayerByIndex(ENTINDEX(m_hObserverTarget->edict())));

		if (!target)
			return;

		int weapon = (target->m_pActiveItem != NULL) ? target->m_pActiveItem->m_iId : 0;
		// use fov of tracked client
		if (m_flFOV != target->m_flFOV || m_iObserverWeapon != weapon)
		{
			m_flFOV = target->m_flFOV;
			m_iClientFOV = (int)m_flFOV;
			// write fov before weapon data, so zoomed crosshair is set correctly
			MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
				WRITE_BYTE((int)m_flFOV);
			MESSAGE_END();


			m_iObserverWeapon = weapon;
			//send weapon update
			MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
			WRITE_BYTE(1);	// 1 = current weapon, not on target
			WRITE_BYTE(m_iObserverWeapon);
			WRITE_BYTE(0);	// clip
			MESSAGE_END();
		}
	}
	else
	{
		m_flFOV = 90;

		if (m_iObserverWeapon != 0)
		{
			m_iObserverWeapon = 0;

			MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
				WRITE_BYTE(1);	// 1 = current weapon
				WRITE_BYTE(m_iObserverWeapon);
				WRITE_BYTE(0);	// clip
			MESSAGE_END();
		}
	}
}