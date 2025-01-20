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
//=========================================================
// GameRules.cpp
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons/weapons.h"
#include "game/gamerules.h"
#include "game/teamplay_gamerules.h"
#include "game/skill.h"
#include "game/game.h"

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules* g_pGameRules = NULL;
extern DLL_GLOBAL BOOL g_fGameOver;

int g_teamplay = 0;

//=========================================================
//=========================================================
BOOL CGameRules::CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry )
{
	int iAmmoIndex;

	if ( pszAmmoName )
	{
		iAmmoIndex = pPlayer->GetAmmoIndex( pszAmmoName );

		if ( iAmmoIndex > -1 )
		{
			if ( pPlayer->AmmoInventory( iAmmoIndex ) < iMaxCarry )
			{
				// player has room for more of this type of ammo
				return TRUE;
			}
		}
	}

	return FALSE;
}

/*
//=========================================================
// AllowMuzzleAmbientLight: allow dlights from gunshots
//=========================================================
BOOL CGameRules::AllowMuzzleAmbientLight(void)
{
	if (g_pGameRules->IsMultiplayer()) // performance
		return false;

	if (sv_muzzlelight.value > 0)
		return true;

	return false;
}
*/

//=========================================================
//=========================================================
edict_t *CGameRules :: GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );
	CBaseEntity *pSpawnSpot = CBaseEntity::Instance( pentSpawnSpot );

	pPlayer->SetAbsOrigin( pSpawnSpot->GetAbsOrigin() + Vector(0,0,1) );
	pPlayer->pev->v_angle = g_vecZero;
	pPlayer->SetAbsVelocity( g_vecZero );
	pPlayer->SetAbsAngles( pSpawnSpot->GetAbsAngles() );
	pPlayer->pev->punchangle = g_vecZero;

	if( FBitSet( pSpawnSpot->pev->spawnflags, 1 ) ) // the START WITH SUIT flag
	{
		pPlayer->AddWeapon( WEAPON_SUIT );
		pPlayer->CanTakeEMPDamage = true;
	}

	if( FBitSet( pSpawnSpot->pev->spawnflags, 2 ))
		pPlayer->SetFlag(F_PLAYER_MENU);

	if( !FBitSet( pSpawnSpot->pev->spawnflags, 4 ) )
	{
		pPlayer->pev->fixangle = TRUE; // FIXME - only do fixangle if NOT an upside-down spawn...
	}
	
	return pentSpawnSpot;
}

//=========================================================
//=========================================================
BOOL CGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	// only living players can have items
	if ( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	if ( pWeapon->pszAmmo1() )
	{
		if ( !CanHaveAmmo( pPlayer, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1() ) )
		{
			// we can't carry anymore ammo for this gun. We can only 
			// have the gun if we aren't already carrying one of this type
			if ( pPlayer->HasPlayerItem( pWeapon ) )
				return FALSE;
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if ( pPlayer->HasPlayerItem( pWeapon ) )
			return FALSE;
	}

	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return TRUE;
}

//=========================================================
// RefreshSkillData: load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData ( void )
{
	int	iSkill = (int)CVAR_GET_FLOAT( "skill" );

	if( iSkill < 1 )
		iSkill = 2; // diffusion - start on medium by default
	else if( iSkill > 3 ) // TODO - more skills?
		iSkill = 3;

	g_iSkillLevel = iSkill;

	ALERT ( at_console, "\n^2*** GAME SKILL LEVEL: %d ***^7\n", g_iSkillLevel );
}

//=========================================================
// InstallGameRules: instantiate the proper game rules object
//=========================================================
CGameRules *InstallGameRules( void )
{
	SERVER_COMMAND( "exec game.cfg\n" );
	COMMAND_EXECUTE( );

	if ( !gpGlobals->deathmatch )
	{
		// generic half-life
		g_teamplay = 0;
		return new CHalfLifeRules;
	}
	else
	{
		if ( teamplay.value > 0 )
		{
			// teamplay

			g_teamplay = 1;
			return new CHalfLifeTeamplay;
		}
		if ((int)gpGlobals->deathmatch == 1)
		{
			// vanilla deathmatch
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
		else
		{
			// vanilla deathmatch??
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
	}
}



