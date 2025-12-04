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
/*

===== player.cpp ========================================================

  functions dealing with the player

  DIFFUSION NOTES:
  pev->vuser1.x = used for water-on-screen effect!!!
  pev->vuser1.y = motion blur in the car
  pev->vuser1.z = monochrome effect (1.0 or less)
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "entities/trains.h"
#include "nodes.h"
#include "weapons/weapons.h"
#include "entities/soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "game/gamerules.h"
#include "game/game.h"
#include "hltv.h"
#include "entities/CRope.h"
#include "pm_defs.h"

#include "bot/bot.h"

#include "studio.h"

extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL	BOOL	g_fDrawLines;
int gEvilImpulse101;

float     m_fRegenRemander; // DiffusionRegen
float	  RegenWaitTime;
float     RegenRate;

extern DLL_GLOBAL int		g_iSkillLevel, gDisplayTitle;

BOOL gInitHUD = TRUE;

extern void CopyToBodyQue( CBaseEntity *pCorpse );
extern void respawn( CBaseEntity *pClient, BOOL fCopyCorpse );
extern Vector VecBModelOrigin(entvars_t *pevBModel );
extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

// the world node graph
extern CGraph	WorldGraph;

extern Vector vWaypoint[MAX_WAYPOINTS];
extern int ConnectedPoints[MAX_WAYPOINTS][MAX_WAYPOINT_CONNECTIONS];
extern int ConnectionsForPoint[MAX_WAYPOINTS];
extern int WaypointsLoaded;
extern int PointFlag[MAX_WAYPOINTS];

#define TRAIN_ACTIVE	0x80 
#define TRAIN_NEW		0xc0
#define TRAIN_OFF		0x00
#define TRAIN_NEUTRAL	0x01
#define TRAIN_SLOW		0x02
#define TRAIN_MEDIUM	0x03
#define TRAIN_FAST		0x04 
#define TRAIN_BACK_SLOW	0x05
#define TRAIN_BACK_MEDIUM	0x06
#define TRAIN_BACK_FAST	0x07
#define TRAIN_LOCKED	0x08

#define	FLASH_DRAIN_TIME	 1.0f //100 units/1.5 minutes
#define	FLASH_CHARGE_TIME	 0.25f // 100 units/20 seconds  (seconds per unit)

// Global Savedata for player
BEGIN_DATADESC( CBasePlayer )
	DEFINE_FIELD( m_flFlashLightTime, FIELD_TIME ),
	DEFINE_FIELD( m_iFlashBattery, FIELD_INTEGER ),

	DEFINE_FIELD( m_afButtonLast, FIELD_INTEGER ),
	DEFINE_FIELD( m_afButtonPressed, FIELD_INTEGER ),
	DEFINE_FIELD( m_afButtonReleased, FIELD_INTEGER ),

	DEFINE_ARRAY( m_rgItems, FIELD_INTEGER, MAX_ITEMS ),
	DEFINE_FIELD( m_afPhysicsFlags, FIELD_INTEGER ),

	DEFINE_FIELD( m_flTimeStepSound, FIELD_TIME ),
	DEFINE_FIELD( m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( m_flSwimTime, FIELD_TIME ),
	DEFINE_FIELD( m_flDuckTime, FIELD_TIME ),

	DEFINE_FIELD( m_flSuitUpdate, FIELD_TIME ),
	DEFINE_ARRAY( m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST ),
	DEFINE_FIELD( m_iSuitPlayNext, FIELD_INTEGER ),
	DEFINE_ARRAY( m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT ),
	DEFINE_ARRAY( m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT ),
	DEFINE_FIELD( m_lastDamageAmount, FIELD_INTEGER ),

	DEFINE_ARRAY( m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( m_pActiveItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pLastItem, FIELD_CLASSPTR ),
	
	DEFINE_ARRAY( m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_FIELD( m_idrowndmg, FIELD_INTEGER ),
	DEFINE_FIELD( m_idrownrestored, FIELD_INTEGER ),
	DEFINE_FIELD( m_tSneaking, FIELD_TIME ),

	DEFINE_FIELD( m_iTrain, FIELD_INTEGER ),
	DEFINE_FIELD( m_bitsHUDDamage, FIELD_INTEGER ),
	DEFINE_FIELD( m_flFallVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( m_iTargetVolume, FIELD_INTEGER ),
	DEFINE_FIELD( m_iWeaponVolume, FIELD_INTEGER ),
	DEFINE_FIELD( m_iExtraSoundTypes, FIELD_INTEGER ),
	DEFINE_FIELD( m_iWeaponFlash, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLongJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fInitHUD, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_tbdPrev, FIELD_TIME ),

	DEFINE_FIELD( m_pTank, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pMonitor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pHoldableItem, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iHideHUD, FIELD_INTEGER ),
	DEFINE_FIELD( m_flFOV, FIELD_FLOAT ),

	DEFINE_FIELD( m_iRainDripsPerSecond, FIELD_INTEGER ),
	DEFINE_FIELD( m_flRainWindX, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRainWindY, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRainRandX, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRainRandY, FIELD_FLOAT ),

	DEFINE_FIELD( m_iRainIdealDripsPerSecond, FIELD_INTEGER ),
	DEFINE_FIELD( m_flRainIdealWindX, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRainIdealWindY, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRainIdealRandX, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRainIdealRandY, FIELD_FLOAT ),

	DEFINE_FIELD( m_flRainEndFade, FIELD_TIME ),
	DEFINE_FIELD( m_flRainNextFadeUpdate, FIELD_TIME ),

	DEFINE_ARRAY( m_hKeyCatchers, FIELD_EHANDLE, MAX_KEYCATCHERS ),
	DEFINE_FIELD( m_iNumKeyCatchers, FIELD_INTEGER ),
 
	// holdable item stuff
	DEFINE_FIELD( m_vecHoldableItemPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_flHoldableItemDistance, FIELD_FLOAT ),
	
	DEFINE_FIELD( m_nCustomSprayFrames, FIELD_INTEGER ),
	DEFINE_ARRAY( m_szAnimExtention, FIELD_CHARACTER, 32 ),
	DEFINE_FUNCTION( PlayerDeathThink ),
	DEFINE_FIELD( m_iSndRoomtype, FIELD_INTEGER ),

	DEFINE_FIELD( BrokenSuit, FIELD_BOOLEAN ),
	DEFINE_FIELD( NextBrokenDmgTime, FIELD_TIME ),

	DEFINE_FIELD( m_flStaminaValue, FIELD_FLOAT ), // DiffusionSprint
	DEFINE_FIELD( m_flStaminaWait, FIELD_TIME ),
	DEFINE_FIELD( m_fTimeLastHurt, FIELD_TIME ), // DiffusionRegen
	DEFINE_FIELD( CanRegenerateHealth, FIELD_BOOLEAN ),
	DEFINE_FIELD( EnterWaterTime, FIELD_TIME ),
	DEFINE_FIELD( LastDashTime, FIELD_TIME ),
	DEFINE_FIELD( Dashed, FIELD_BOOLEAN ),
	DEFINE_FIELD( CanDash, FIELD_BOOLEAN ),
	DEFINE_FIELD( CanUse, FIELD_BOOLEAN ),
	DEFINE_FIELD( CanJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( CanSprint, FIELD_BOOLEAN ),
	DEFINE_FIELD( CanMove, FIELD_BOOLEAN ),
	DEFINE_FIELD( WeaponLowered, FIELD_BOOLEAN ),
	DEFINE_FIELD( DrunkLevel, FIELD_INTEGER ),
	DEFINE_FIELD( CanShoot, FIELD_BOOLEAN ),
	DEFINE_FIELD( CanSelectEmptyWeapon, FIELD_BOOLEAN ),
	DEFINE_FIELD( CanElectroBlast, FIELD_BOOLEAN ),
	DEFINE_FIELD( HUDSuitOffline, FIELD_BOOLEAN ),
	DEFINE_FIELD( AchievementCheckTime, FIELD_TIME ),
	DEFINE_FIELD( Objective, FIELD_STRING ),
	DEFINE_FIELD( Objective2, FIELD_STRING ),
	DEFINE_ARRAY( CountSlot, FIELD_INTEGER, 5 ),
	DEFINE_FIELD( m_flLastTimeTutorWasShown, FIELD_TIME ),
	DEFINE_FIELD( LastSwimSound, FIELD_TIME ),
	DEFINE_FIELD( LastSwimUpSound, FIELD_TIME ),
	DEFINE_FIELD( Submerged, FIELD_BOOLEAN ),
	DEFINE_FIELD( LastUseCheckTime, FIELD_TIME ),
	DEFINE_FIELD( IsUpsideDown, FIELD_BOOLEAN ),
	DEFINE_FIELD( ShieldOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( ShieldAvailableLVL, FIELD_INTEGER ),
	// drone
	DEFINE_FIELD( DroneDeployed, FIELD_BOOLEAN ),
	DEFINE_FIELD( DroneHealth, FIELD_FLOAT ),
	DEFINE_FIELD( DroneAmmo, FIELD_FLOAT ),
	DEFINE_FIELD( m_hDrone, FIELD_EHANDLE ),
	DEFINE_FIELD( DroneControl, FIELD_BOOLEAN ),
	DEFINE_FIELD( DroneColor, FIELD_VECTOR ),
	DEFINE_FIELD( CanUseDrone, FIELD_BOOLEAN ),
	DEFINE_FIELD( DroneTarget_OnDeploy, FIELD_STRING ),
	DEFINE_FIELD( DroneTarget_OnReturn, FIELD_STRING ),
	DEFINE_FIELD( DroneTarget_OnEnteringFirstPerson, FIELD_STRING ),
	DEFINE_FIELD( DroneTarget_OnLeavingFirstPerson, FIELD_STRING ),
	DEFINE_FIELD( DroneDistance, FIELD_INTEGER ),

	DEFINE_ARRAY( m_DroneTextParms, FIELD_CHARACTER, sizeof(hudtextparms_t) ),
	DEFINE_ARRAY( m_AliceTextParms, FIELD_CHARACTER, sizeof(hudtextparms_t) ),
	DEFINE_ARRAY( CameraEntity, FIELD_CHARACTER, 128 ),
	DEFINE_FIELD( CameraOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CameraAngles, FIELD_VECTOR ),
	DEFINE_FIELD( ZoomState, FIELD_INTEGER ),
	DEFINE_FIELD( ButtonFreezeTime, FIELD_TIME ),
	DEFINE_FIELD( HUDtextTime, FIELD_TIME ),
//	DEFINE_FIELD( m_pFlashlightMonster, FIELD_CLASSPTR ), // this is fucked up: I can't save this thing because it crashes the game or corrupts whole player physics. What the hell?

	// remember the weapons we had throughout the game
	DEFINE_ARRAY( HadWeapon, FIELD_BOOLEAN, MAX_WEAPONS ),

	// achievements
	DEFINE_ARRAY( AchievementStats, FIELD_FLOAT, TOTAL_ACHIEVEMENTS ),

	// blast
	DEFINE_FIELD( BlastAbilityLVL, FIELD_INTEGER),
	DEFINE_FIELD( BlastChargesReady, FIELD_INTEGER),
	DEFINE_FIELD( LastBlastTime, FIELD_TIME ),
	DEFINE_FIELD( BlastDMGOverride, FIELD_BOOLEAN ),

	// car
	DEFINE_FIELD( pCar, FIELD_CLASSPTR ),
	DEFINE_FIELD( InCar, FIELD_BOOLEAN ),

	DEFINE_FIELD( BreathingEffect, FIELD_BOOLEAN ),
	DEFINE_FIELD( WigglingEffect, FIELD_BOOLEAN),

	DEFINE_FIELD( m_flLastWeaponSwitchTime, FIELD_TIME ),
	DEFINE_FIELD( LoudWeaponsRestricted, FIELD_BOOLEAN),
	DEFINE_FIELD( bCheatsWereUsed, FIELD_BOOLEAN ),
END_DATADESC()

extern cvar_t sv_regeneration;

LINK_ENTITY_TO_CLASS( player, CBasePlayer );

void CBasePlayer :: Pain( void )
{
	float	flRndSound;//sound randomizer

	flRndSound = RANDOM_FLOAT ( 0 , 1 ); 
	
	if ( flRndSound <= 0.33 )
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM);
	else if ( flRndSound <= 0.66 )	
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM);
	else
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM);
}

Vector VecVelocityForDamage(float flDamage)
{
	Vector vec(RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));

	if (flDamage > -50)
		vec = vec * 0.7;
	else if (flDamage > -200)
		vec = vec * 2;
	else
		vec = vec * 10;
	
	return vec;
}

//-----------------------------------------------------------------------------
// Sets the view angles
//-----------------------------------------------------------------------------
void CBasePlayer :: SnapEyeAngles( const Vector &viewAngles )
{
	pev->v_angle = viewAngles;
	pev->fixangle = TRUE;
}

int TrainSpeed(int iSpeed, int iMax)
{
	float fSpeed, fMax;
	int iBack, iRet = 0;

	fMax = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed/fMax;
	iBack = (fSpeed < 0.0f) ? 1 : 0;
	fSpeed = fabs( fSpeed );

	if (iSpeed == 0)
		iRet = TRAIN_NEUTRAL;
	else if (fSpeed < 0.33)
		iRet = (iBack) ? TRAIN_BACK_SLOW : TRAIN_SLOW;
	else if (fSpeed < 0.66)
		iRet = (iBack) ? TRAIN_BACK_MEDIUM : TRAIN_MEDIUM;
	else
		iRet = (iBack) ? TRAIN_BACK_FAST : TRAIN_FAST;

	return iRet;
}

void CBasePlayer :: DeathSound( void )
{
	// temporarily using pain sounds for death sounds
	switch (RANDOM_LONG(1,3)) 
	{
	case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM); break;
	case 2: EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM); break;
	case 3: EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM); break;
	}

	// play one of the suit death alarms
	// LRC- if no suit, then no flatline sound. (unless it's a deathmatch.)
	if ( !HasWeapon( WEAPON_SUIT ) && !g_pGameRules->IsDeathmatch() )
		return;

	// play one of the suit death alarms
	EMIT_GROUPNAME_SUIT(ENT(pev), "HEV_DEAD");
}

// override takehealth
// bitsDamageType indicates type of damage healed. 

int CBasePlayer :: TakeHealth( float flHealth, int bitsDamageType )
{
	return CBaseMonster :: TakeHealth (flHealth, bitsDamageType);
}

Vector CBasePlayer :: GetGunPosition( )
{
	return EyePosition();
}

void CBasePlayer::StartWelcomeCam( void )
{
	if( m_bInWelcomeCam )
		return;

	m_bInWelcomeCam = TRUE;

	// Remove crosshair
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	m_iHideHUD = HIDEHUD_WPNS | HIDEHUD_HEALTH;
	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->view_ofs = g_vecZero;
	pev->fixangle = TRUE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NOCLIP;	// HACK HACK: Player fall down with MOVETYPE_NONE
	pev->health = 1;					// Let player stay vertically, not lie on a side
	pev->deadflag = DEAD_RESPAWNABLE;
	pev->effects = EF_NODRAW;			// Hide model. This is used instead of pev->modelindex = 0
	
	CBaseEntity *pSpot, *pNewSpot;
	pSpot = UTIL_FindEntityByClassname( NULL, "info_intermission" );
	if( !FNullEnt( pSpot ) )
	{
		// at least one intermission spot in the world.
		int iRand = RANDOM_LONG( 0, 3 );

		while( iRand > 0 )
		{
			pNewSpot = UTIL_FindEntityByClassname( pSpot, "info_intermission" );

			if( pNewSpot )
				pSpot = pNewSpot;

			iRand--;
		}

		UTIL_SetOrigin( this, pSpot->GetAbsOrigin() );
		// Find target for intermission
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, pSpot->GetTarget() );
		if( pTarget )
		{
			// Calculate angles to look at camera target
			pev->angles = UTIL_VecToAngles( pTarget->GetAbsOrigin() - pSpot->GetAbsOrigin() );
		}
		else
		{
			pev->angles = pSpot->pev->angles;
		}
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down
		TraceResult tr;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, 128 ), ignore_monsters, edict(), &tr );
		UTIL_SetOrigin( this, tr.vecEndPos - Vector( 0, 0, 10 ) );
		pev->angles.x = pev->v_angle.x = 30;
	}
}

void CBasePlayer::StopWelcomeCam( void )
{
	m_bInWelcomeCam = FALSE;

	m_iHideHUD = 0;

	Spawn();
	pev->nextthink = -1;
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if( IsSpawnProtected )
		return;
	
	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;
		/* unused, they were all 1.0
		switch ( ptr->iHitgroup )
		{
		default:
		case HITGROUP_GENERIC:
		case HITGROUP_HEAD:
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			break;
		}
		*/
		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage);// a little surface blood.
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}

	if( HasFlag( F_METAL_MONSTER ) )
	{
		UTIL_Sparks( ptr->vecEndPos );

		switch( RANDOM_LONG( 0, 4 ) )
		{
		case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
		case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
		case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
		case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
		}
	}
}

/*
	Take some damage.  
	NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
	etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define ARMOR_RATIO	 0.2	// Armor Takes 80% of the damage
#define ARMOR_BONUS  0.5	// Each Point of Armor is work 1/x points of health

int CBasePlayer :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( g_fGameOver ) // diffusion
		return 0;
	
	if (pev->flags & FL_GODMODE)
		return 0;

	if( IsSpawnProtected )
		return 0;

	// HACKHACK for our own electroblast...
	if( (pevInflictor == pev) && (bitsDamageType == DMG_SHOCK) && (BlastDMGOverride == true) )
	{
		return 0;
	}

	// Already dead
	if( !IsAlive() )
		return 0;

	// do not accept any damage when in car. The car will pass the damage
	if( pCar && !PlayerCarDmgOverride )
		return 0;

	PlayerCarDmgOverride = false;

	if( Dashed )
	{
		// the player is in dashing state. modify the damage to make game easier
		switch( g_iSkillLevel )
		{
			case SKILL_EASY: // disable bullet damage and minimize the rest
				if( bitsDamageType & DMG_BULLET )
					return 0;
				flDamage *= 0.5f;
			break;
			case SKILL_MEDIUM: // minimize the damage
				if( bitsDamageType & DMG_BULLET )
					flDamage *= 0.2f;
				else
					flDamage *= 0.75f;
			break;
			case SKILL_HARD: // minimize the damage from bullets only
				if( bitsDamageType & DMG_BULLET )
					flDamage *= 0.5f;
			break;
		}
	}

	//--------------------------------------------------------------------

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	if( !g_pGameRules->FPlayerCanTakeDamage( this, pAttacker ) )
		// Refuse the damage
		return 0;

	/*
	if( pAttacker )
	{
		CBaseMonster *MyAttacker = (CBaseMonster *)GET_PRIVATE( pAttacker->edict() );
		if( (pAttacker->Classify() == CLASS_PLAYER_ALLY) && !(MyAttacker->m_afMemory & bits_MEMORY_PROVOKED) )
			return 0;  // diffusion - no friendly fire!!! (except when they are pissed at us)
		// probably must check relationship (R_HT) but I don't know how. IRelationship(pAttacker) returns zero...
	}*/
	
	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = bitsDamageType;
	int ffound = TRUE;
	int fmajor;
	int fcritical;
	int fTookDamage;
	int ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = pev->health;

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	// diffusion - skill-based damage multiplier (for the time being...)
	if (!(g_pGameRules->IsMultiplayer())) // not sure if this condition is needed, didn't test
	{
		if (g_iSkillLevel == SKILL_EASY)
		{
			if (pev->health < 25)
				flDamage *= 0.5;
		}
		else if (g_iSkillLevel == SKILL_MEDIUM)
		{
			if (pev->health < 20)
				flDamage *= 0.75;
			else if (pev->health > 60)
				flDamage *= 1.1;
		}
		else if (g_iSkillLevel == SKILL_HARD)
		{
			if (pev->health > 60)
				flDamage *= 1.15;
		}
	}

	// diffusion - skip small damage
	if( flDamage < 0.01 )
		return 0;

	if (g_pGameRules->IsMultiplayer() && (mp_alwaysgib.value == 1))
		bitsDamageType |= DMG_ALWAYSGIB;
		
	if( FClassnameIs( pevInflictor, "playerball" ) && (pevInflictor->owner == edict()) )
		flDamage *= 0.25;

	if ( bitsDamageType & DMG_BLAST ) // diffusion - screen shakes from explosions, "true" - in air also
		UTIL_ScreenShakeLocal( this, GetAbsOrigin(), 10.0, 150.0, 1.0, 1200, true );

	// shield doesn't protect from EMP, so this goes first
	if( bitsDamageType & DMG_EMP )
	{
		float OriginalDamage = flDamage;
		// 20% of this damages the HP, 80% goes to deplete the stamina
		flDamage = OriginalDamage * 0.2f;
		m_flStaminaValue -= OriginalDamage * 0.8f;
		if( m_flStaminaValue < 0.0f )
			m_flStaminaValue = 0.0f;
		m_flStaminaWait = gpGlobals->time + 3;
		// add glitch effect for the emp grenade
		if( FClassnameIs( pevInflictor, "grenade_emp" ) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
				WRITE_BYTE( TE_PLAYER_GLITCH );
				WRITE_BYTE( (int)flDamage ); // x0.1
				WRITE_BYTE( 2 ); // hold for 2 seconds
			MESSAGE_END();
		}
		goto skip_shield;
	}

	// diffusion - shield absorbs half of the damage if we have enough stamina.
	if( ShieldOn )
	{
		if( bitsDamageType & DMG_FALL )
			goto skip_shield;

		float shield_dmg_absorbed;
		float shield_energy_spent;
		switch( ShieldAvailableLVL )
		{
			default:
			case 1:
				shield_dmg_absorbed = flDamage * 0.35f;
				shield_energy_spent = flDamage * 0.5f;
				break;
			case 2:
				shield_dmg_absorbed = flDamage * 0.5f;
				shield_energy_spent = flDamage * 0.4f;
				break;
			case 3:
				shield_dmg_absorbed = flDamage * 0.65f;
				shield_energy_spent = flDamage * 0.35f;
				break;
		}

		if( m_flStaminaValue > shield_energy_spent )
		{
			m_flStaminaValue -= shield_energy_spent;
			flDamage -= shield_dmg_absorbed;
		}
		else // we took more damage than we have stamina available
		{
			flDamage -= m_flStaminaValue;
			m_flStaminaValue = 0;
		}
	}
skip_shield:

	if( flDamage > 15 ) // diffusion
	{
		if( !(bitsDamageType & DMG_EMP) )
			UTIL_ScreenFade( this, Vector( 128, 0, 0 ), 2, 0, 150, FFADE_IN );
		// also monochrome effect is applied on client
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// Armor. // diffusion - no batteries in game
/*	if (pev->armorvalue && !(bitsDamageType & (DMG_FALL | DMG_DROWN)) )// armor doesn't protect against fall or drown damage!
	{
		float flNew = flDamage * flRatio;

		float flArmor;

		flArmor = (flDamage - flNew) * flBonus;

		// Does this use more armor than we have?
		if (flArmor > pev->armorvalue)
		{
			flArmor = pev->armorvalue;
			flArmor *= (1/flBonus);
			flNew = flDamage - flArmor;
			pev->armorvalue = 0;
		}
		else
			pev->armorvalue -= flArmor;
		
		flDamage = flNew;
	} 
*/
	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)

	// diffusion - no more INT. Instead, if hp < 1, hp becomes 0 (combat.cpp, CBaseMonster::TakeDamage)
	fTookDamage = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, /*(int)*/flDamage, bitsDamageType);

	// reset damage time countdown for each type of time based damage player just sustained
	{
		for (int i = 0; i < CDMG_TIMEBASED; i++)
			if (bitsDamageType & (DMG_PARALYZE << i))
				m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// take damage event
		WRITE_SHORT( ENTINDEX(this->edict()) );	// index number of primary entity
		WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		WRITE_LONG( 5 );   // eventflags (priority and flags)
	MESSAGE_END();


	// how bad is it, doc?

	ftrivial = (pev->health > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (pev->health < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

		// DMG_BURN	
		// DMG_FREEZE
		// DMG_BLAST
		// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;  // make sure the damage bits get resent

	while (fTookDamage && (!ftrivial || (bitsDamage & DMG_TIMEBASED)) && ffound && bitsDamage)
	{
		ffound = FALSE;

		if (bitsDamage & DMG_CLUB)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC);	// minor fracture
			bitsDamage &= ~DMG_CLUB;
			ffound = TRUE;
		}
		if (bitsDamage & (DMG_FALL | DMG_CRUSH))
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG5", FALSE, SUIT_NEXT_IN_30SEC);	// major fracture
			else
				SetSuitUpdate("!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC);	// minor fracture
	
			bitsDamage &= ~(DMG_FALL | DMG_CRUSH);
			ffound = TRUE;
		}
		
		if (bitsDamage & DMG_BULLET)
		{
			if (m_lastDamageAmount > 5)
				SetSuitUpdate("!HEV_DMG6", FALSE, SUIT_NEXT_IN_30SEC);	// blood loss detected
			//else
			//	SetSuitUpdate("!HEV_DMG0", FALSE, SUIT_NEXT_IN_30SEC);	// minor laceration
			
			bitsDamage &= ~DMG_BULLET;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_SLASH)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG1", FALSE, SUIT_NEXT_IN_30SEC);	// major laceration
			else
				SetSuitUpdate("!HEV_DMG0", FALSE, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_SLASH;
			ffound = TRUE;
		}
		
		if (bitsDamage & DMG_SONIC)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG2", FALSE, SUIT_NEXT_IN_1MIN);	// internal bleeding
			bitsDamage &= ~DMG_SONIC;
			ffound = TRUE;
		}

		if (bitsDamage & (DMG_POISON | DMG_PARALYZE))
		{
			SetSuitUpdate("!HEV_DMG3", FALSE, SUIT_NEXT_IN_1MIN);	// blood toxins detected
			bitsDamage &= ~(DMG_POISON | DMG_PARALYZE);
			ffound = TRUE;
		}

		if (bitsDamage & DMG_ACID)
		{
			SetSuitUpdate("!HEV_DET1", FALSE, SUIT_NEXT_IN_1MIN);	// hazardous chemicals detected
			bitsDamage &= ~DMG_ACID;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_NERVEGAS)
		{
			SetSuitUpdate("!HEV_DET0", FALSE, SUIT_NEXT_IN_1MIN);	// biohazard detected
			bitsDamage &= ~DMG_NERVEGAS;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_RADIATION)
		{
			SetSuitUpdate("!HEV_DET2", FALSE, SUIT_NEXT_IN_1MIN);	// radiation detected
			bitsDamage &= ~DMG_RADIATION;
			ffound = TRUE;
		}
		if (bitsDamage & DMG_SHOCK)
		{
			bitsDamage &= ~DMG_SHOCK;
			ffound = TRUE;
		}
	}
	
	// diffusion
	if( bitsDamage & DMG_FALL )
	{
		// use punchangle in PM_CheckFalling
		// BUGBUG player receives DMG_GENERIC, if gets falldamage while holding crouch!
	}
	else if( bitsDamage & DMG_DROWN )
	{
		// instead of punchangle, player will get a blue screenfade
		// and disable gibbing from drowning
		bitsDamage &= ~DMG_ALWAYSGIB;
		bitsDamage |= DMG_NEVERGIB;
	}
	else
	{
		float AddPunch = flDamage * 0.2;
		if( AddPunch > 2 )
			AddPunch = 2;

		if( bitsDamage & DMG_BLAST )
			AddPunch *= 2;

		Vector AddPunchVector = g_vecZero;
		
		switch( RANDOM_LONG( 0, 3 ))
		{
		case 0: AddPunchVector = Vector(0.5, 0, -2); break;
		case 1: AddPunchVector = Vector(1, 0, 2); break;
		case 2: AddPunchVector = Vector(1, 0, -2); break;
		case 3: AddPunchVector = Vector(0.5, 0, 1); break;
		}

		pev->punchangle += AddPunchVector * AddPunch;
	}
	
	if (fTookDamage && !ftrivial && fmajor && flHealthPrev >= 75) 
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate("!HEV_MED1", FALSE, SUIT_NEXT_IN_30MIN);	// automedic on

		// give morphine shot if not given recently
		SetSuitUpdate("!HEV_HEAL7", FALSE, SUIT_NEXT_IN_30MIN);	// morphine shot
	}
	
	if (fTookDamage && !ftrivial && fcritical && flHealthPrev < 75)
	{

		// already took major damage, now it's critical...
		if (pev->health < 6)
			SetSuitUpdate("!HEV_HLTH3", FALSE, SUIT_NEXT_IN_10MIN);	// near death
		else if (pev->health < 20)
			SetSuitUpdate("!HEV_HLTH2", FALSE, SUIT_NEXT_IN_10MIN);	// health critical
	
		// give critical health warnings
		if (!RANDOM_LONG(0,3) && flHealthPrev < 50)
			SetSuitUpdate("!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN); //seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if (fTookDamage && (bitsDamageType & DMG_TIMEBASED) && flHealthPrev < 75)
	{
		if (flHealthPrev < 50)
		{
			if (!RANDOM_LONG(0,3))
				SetSuitUpdate("!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN); //seek medical attention
		}
		else
			SetSuitUpdate("!HEV_HLTH1", FALSE, SUIT_NEXT_IN_10MIN);	// health dropping
	}

	// achievement
	if( !(bitsDamageType & DMG_TRIGGER) && pevInflictor->owner != edict() && pevAttacker != pev ) // do not count the damage received from yourself or from trigger
		SendAchievementStatToClient( ACH_RECEIVEDAMAGE, (int)flDamage, ACHVAL_ADD );

	if (pev->health < pev->max_health)                // DiffusionRegen
		m_fTimeLastHurt = gpGlobals->time;

	// we need to send the "1" to the HUD to indicate that we need to flash the health bar
	MESSAGE_BEGIN( MSG_ONE, gmsgHealthVisual, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_BYTE( pev->health );
		WRITE_BYTE( pev->max_health );
	MESSAGE_END();

	return fTookDamage;
}

//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
void CBasePlayer::PackDeadPlayerItems( void )
{
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon *rgpPackWeapons[ 20 ];// 20 hardcoded for now. How to determine exactly how many weapons we have?
	int iPackAmmo[ MAX_AMMO_SLOTS + 1];
	int iPW = 0;// index into packweapons array
	int iPA = 0;// index into packammo array

	memset(rgpPackWeapons, NULL, sizeof(rgpPackWeapons) );
	memset(iPackAmmo, -1, sizeof(iPackAmmo) );

	// get the game rules 
	iWeaponRules = g_pGameRules->DeadPlayerWeapons( this );
	iAmmoRules = g_pGameRules->DeadPlayerAmmo( this );

	if ( iWeaponRules == GR_PLR_DROP_GUN_NO && iAmmoRules == GR_PLR_DROP_AMMO_NO )
	{
		// nothing to pack. Remove the weapons and return. Don't call create on the box!
		RemoveAllItems( FALSE );
		return;
	}

// go through all of the weapons and make a list of the ones to pack
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				switch( iWeaponRules )
				{
				case GR_PLR_DROP_GUN_ACTIVE:
					if ( m_pActiveItem && pPlayerItem == m_pActiveItem )
					{
						// this is the active item. Pack it.
						rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					}
					break;

				case GR_PLR_DROP_GUN_ALL:
					rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					break;

				default:
					break;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

// now go through ammo and make a list of which types to pack.
	if ( iAmmoRules != GR_PLR_DROP_AMMO_NO )
	{
		for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
		{
			if ( m_rgAmmo[ i ] > 0 )
			{
				// player has some ammo of this type.
				switch ( iAmmoRules )
				{
				case GR_PLR_DROP_AMMO_ALL:
					iPackAmmo[ iPA++ ] = i;
					break;
					
				case GR_PLR_DROP_AMMO_ACTIVE:
					if ( m_pActiveItem && i == m_pActiveItem->PrimaryAmmoIndex() ) 
					{
						// this is the primary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					else if ( m_pActiveItem && i == m_pActiveItem->SecondaryAmmoIndex() ) 
					{
						// this is the secondary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					break;

				default:
					break;
				}
			}
		}
	}

	// create a box to pack the stuff into.
	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create( "weaponbox", GetAbsOrigin(), GetAbsAngles(), edict() );

	Vector vecAngles = pWeaponBox->GetAbsAngles();

	vecAngles.x = 0;// don't let weaponbox tilt.
	vecAngles.z = 0;

	pWeaponBox->SetAbsAngles( vecAngles );
	pWeaponBox->SetThink( &CWeaponBox::Kill );
	pWeaponBox->SetNextThink( 120 );

	// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

	// pack the ammo
	while ( iPackAmmo[iPA] != -1 )
	{
		pWeaponBox->PackAmmo( MAKE_STRING( CBasePlayerItem::AmmoInfoArray[iPackAmmo[iPA]].pszName ), m_rgAmmo[iPackAmmo[iPA]] );
		iPA++;
	}

	// now pack all of the items in the lists
	while ( rgpPackWeapons[iPW] )
	{
		// weapon unhooked from the player. Pack it into der box.
		pWeaponBox->PackWeapon( rgpPackWeapons[iPW] );

		iPW++;
	}

	pWeaponBox->SetAbsVelocity( GetAbsVelocity() * 1.2 ); // weaponbox has player's velocity, then some.

	RemoveAllItems( FALSE );// now strip off everything that wasn't handled by the code above.
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit, BOOL removeCycler )
{
	if (m_pActiveItem)
	{
		ResetAutoaim( );
		m_pActiveItem->Holster( );
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	int i;
	CBasePlayerItem *pPendingItem;
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while (m_pActiveItem)
		{
			pPendingItem = m_pActiveItem->m_pNext; 
			m_pActiveItem->Drop( );
			m_pActiveItem = pPendingItem;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;

	pev->viewmodel	= 0;
	pev->weaponmodel	= 0;

	// BugFixedHL - Remove deployed satchels
	DeactivateSatchels(this);
	
	if ( removeSuit && removeCycler )
		RemoveAllWeapons();	// clear all the weapons
	else if( removeCycler )
	{
		if( HasWeapon( WEAPON_SUIT ))
		{
			RemoveAllWeapons();		// clear all the weapons
			AddWeapon( WEAPON_SUIT );	// leave only suit
		}
		else
			RemoveAllWeapons();		// clear all the weapons
	}
	else if( removeSuit )
	{
		if( HasWeapon( WEAPON_CYCLER ))
		{
			RemoveAllWeapons();		// clear all the weapons
			AddWeapon( WEAPON_CYCLER );	// leave only cycler
		}
		else
			RemoveAllWeapons();		// clear all the weapons
	}
	else
	{
		if( HasWeapon( WEAPON_SUIT ) && HasWeapon( WEAPON_CYCLER ))
		{
			// leave both cycler and suit
			RemoveAllWeapons();		// clear all the weapons
			AddWeapon( WEAPON_CYCLER );
			AddWeapon( WEAPON_SUIT );
		}
		else if( HasWeapon( WEAPON_SUIT ))
		{
			RemoveAllWeapons();		// clear all the weapons
			AddWeapon( WEAPON_SUIT );	// leave only suit
		}
		else if( HasWeapon( WEAPON_CYCLER ))
		{
			RemoveAllWeapons();		// clear all the weapons
			AddWeapon( WEAPON_CYCLER );	// leave only cycler
		}
		else
			RemoveAllWeapons();		// clear all the weapons
	}

	// diffusion - clear all drone info and delete it
	DroneHealth = 0;
	DroneAmmo = 0;
	if( m_hDrone != NULL )
	{
		UTIL_Remove( m_hDrone );
		m_hDrone = NULL;
	}

	for ( i = 0; i < MAX_AMMO_SLOTS;i++)
		m_rgAmmo[i] = 0;

	UpdateClientData();
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0);
		WRITE_BYTE(0);
	MESSAGE_END();
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
entvars_t *g_pevLastInflictor;  // Set in combat.cpp.  Used to pass the damage inflictor for death messages.
								// Better solution:  Add as parameter to all Killed() functions.


//======================================================================================
// Killed: perform certain actions when player dies
//======================================================================================
void CBasePlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	CSound *pSound;

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster( );

	g_pGameRules->PlayerKilled( this, pevAttacker, g_pevLastInflictor );

	BlastChargesReady = 0;
	ShieldOn = false;

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	if (m_pMonitor != NULL )
	{
		m_pMonitor->Use( this, this, USE_RESET, 0 );
		m_pMonitor = NULL;
	}

	if( FlashlightIsOn() )
		FlashlightTurnOff();

	if( m_pHoldableItem != NULL )
		DropHoldableItem( 0 );

	if( pCar != NULL )
		pCar->Use( this, this, USE_OFF, 0 );

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	{
		if ( pSound )
			pSound->Reset();
	}

	SetAnimation( PLAYER_DIE );

	// reset torso controllers
	pev->controller[0] = 0x7F;
	pev->controller[1] = 0x7F;
	pev->controller[2] = 0x7F;
	pev->controller[3] = 0x7F;

	m_flDeathAnimationStartTime = gpGlobals->time;
	
	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes
	
	pev->deadflag		= DEAD_DYING;
	pev->movetype		= MOVETYPE_TOSS;
	ClearBits( pev->flags, FL_ONGROUND );
	if (GetAbsVelocity().z < 10)
		ApplyAbsVelocityImpulse( Vector( 0, 0, RANDOM_FLOAT(0,300)));

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);
	
	// send "health" update message to zero
	m_iClientHealth = pev->health - 1;
	m_iClientStamina = 0;

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0XFF);
		WRITE_BYTE(0xFF);
	MESSAGE_END();

	// disable crosshair
	MESSAGE_BEGIN( MSG_ONE, gmsgCrosshairStatic, NULL, pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	// disable use icon
	MESSAGE_BEGIN( MSG_ONE, gmsgUseIcon, NULL, pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	// reset FOV
	pev->fov = m_flFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE(0);
	MESSAGE_END();

	if( !(g_pGameRules->IsMultiplayer()) ) // fade to black in singleplayer
		UTIL_ScreenFade( this, Vector(0,0,0), 4.0f, 0, 255, FFADE_OUT | FFADE_STAYOUT ); 

	pev->solid = SOLID_NOT;

	pev->punchangle.x -= 15;

	if ( ( pev->health < -40 && iGib != GIB_NEVER ) || iGib == GIB_ALWAYS )
	{
		GibMonster();	// This clears pev->model
		pev->effects |= EF_NODRAW;
		return;
	}

	DeathSound();

	Vector vecAngles = GetAbsAngles();	
	vecAngles.x = 0;
	vecAngles.z = 0;
	SetAbsAngles( vecAngles );

	SetThink(&CBasePlayer::PlayerDeathThink);
	SetNextThink( 0.1 );
}

void CBasePlayer::GibMonster( void )
{
	if( !HasFlag( F_METAL_MONSTER ) )
	{
		CBaseMonster::GibMonster();
		return;
	}

	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_BREAKMODEL );

		// position
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z + 10 );
		// size
		WRITE_COORD( 0.01 );
		WRITE_COORD( 0.01 );
		WRITE_COORD( 0.01 );
		// velocity
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		// randomization of the velocity
		WRITE_BYTE( 30 );
		// Model
		WRITE_SHORT( g_sModelIndexMetalGibs );	//model id#
			// # of shards
		WRITE_BYTE( RANDOM_LONG( 30, 40 ) );
		// duration
		WRITE_BYTE( 20 );// 3.0 seconds
			// flags
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();
}

// ==================================================================
// SetAnimation: set the activity based on an event or current state
// ==================================================================

void CBasePlayer::SetAnimation(PLAYER_ANIM playerAnim)
{
	int animDesired;
	float speed;
	char szAnim[64];

	// diffusion hacks
	bool ForceReset = false;
	studiohdr_t *pstudiohdr;
	mstudioseqdesc_t *pseqdesc = NULL;
	if( pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( edict() ) )
	{
		pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + pev->sequence;
		if( FBitSet( pev->flags, FL_DUCKING ) && (!strncmp( pseqdesc->label, "ref_aim_", 8 ) || !strncmp( pseqdesc->label, "ref_shoot_", 10 )) )
			ForceReset = true;

		if( !FBitSet( pev->flags, FL_DUCKING ) && (!strncmp( pseqdesc->label, "crouch_aim_", 11 ) || !strncmp( pseqdesc->label, "crouch_shoot_", 13 )) )
			ForceReset = true;
	}
	if( m_pActiveItem && m_pActiveItem->m_iId != cached_weapon_id )
	{
		ForceReset = true;
		cached_weapon_id = m_pActiveItem->m_iId;
	}
	if( pev->waterlevel != cached_waterlevel )
	{
		ForceReset = true;
		cached_waterlevel = pev->waterlevel;
	}

	speed = GetAbsVelocity().Length2D();

	if (pev->flags & FL_FROZEN || DroneControl)
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
		ForceReset = true;
	}

	switch (playerAnim) 
	{
	case PLAYER_JUMP:
		m_IdealActivity = ACT_HOP;
		pev->fuser1 = 0.0f;
		break;
	
	case PLAYER_SUPERJUMP:
		m_IdealActivity = ACT_LEAP;
		pev->fuser1 = 0.0f;
		break;
	
	case PLAYER_DIE:
		m_IdealActivity = ACT_DIESIMPLE;
		m_IdealActivity = GetDeathActivity( );
		break;

	case PLAYER_ATTACK1:	
		switch( m_Activity )
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_RANGE_ATTACK1;
			break;
		}
		break;
	case PLAYER_RELOAD:
		switch( m_Activity )
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_RELOAD;
			break;
		}
		break;
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if ( !FBitSet( pev->flags, FL_ONGROUND ) && (m_Activity == ACT_HOP || m_Activity == ACT_LEAP) )	// Still jumping
			m_IdealActivity = m_Activity;
		else if ( pev->waterlevel > 1 )
		{
			if ( speed == 0 )
				m_IdealActivity = ACT_HOVER;
			else
				m_IdealActivity = ACT_SWIM;
		}
		else
			m_IdealActivity = ACT_WALK;
		break;
	}

	switch (m_IdealActivity)
	{
	case ACT_HOVER:
	case ACT_LEAP:
	case ACT_SWIM:
	case ACT_HOP:
	case ACT_DIESIMPLE:
	default:
		if ( m_Activity == m_IdealActivity)
			return;
		m_Activity = m_IdealActivity;

		animDesired = LookupActivity( m_Activity );
		// Already using the desired animation?
		if (pev->sequence == animDesired)
			return;

		pev->gaitsequence = 0;
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo( );
		return;

	case ACT_RANGE_ATTACK1:
		if ( FBitSet( pev->flags, FL_DUCKING ) )	// crouching
			strcpy( szAnim, "crouch_shoot_" );
		else
			strcpy( szAnim, "ref_shoot_" );
		strcat( szAnim, m_szAnimExtention );
		animDesired = LookupSequence( szAnim );
		if (animDesired == -1)
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		if (!m_fSequenceLoops)
			pev->effects |= EF_NOINTERP;

		m_Activity = m_IdealActivity;

		pev->sequence = animDesired;
		ResetSequenceInfo( );
		break;

	case ACT_RELOAD:

	//	if( FBitSet( pev->flags, FL_DUCKING ) )    // crouching
	//		strcpy( szAnim, "crouch_reload_" );
	//	else
	//		strcpy( szAnim, "ref_reload_" );
	//	strcat( szAnim, m_szAnimExtention );
	//	animDesired = LookupSequence( szAnim );
		animDesired = LookupSequence( "ref_reload_onehanded" ); // TEMP
		if( animDesired == -1 )
			animDesired = 0;

		if( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		if( !m_fSequenceLoops )
			pev->effects |= EF_NOINTERP;

		m_Activity = m_IdealActivity;
		pev->sequence = animDesired;
		ResetSequenceInfo();
		break;

	case ACT_WALK:
		/*
		if (m_Activity != ACT_RANGE_ATTACK1 || m_fSequenceFinished)
		{
			if ( FBitSet( pev->flags, FL_DUCKING ) )	// crouching
				strcpy( szAnim, "crouch_aim_" );
			else
				strcpy( szAnim, "ref_aim_" );
			strcat( szAnim, m_szAnimExtention );
			animDesired = LookupSequence( szAnim );
			if (animDesired == -1)
				animDesired = 0;

			m_Activity = ACT_WALK;
		}
		else
			animDesired = pev->sequence;*/
			// diffusionplayermodelhack
		if( (m_Activity != ACT_RANGE_ATTACK1 && m_Activity != ACT_RELOAD) || m_fSequenceFinished )
		{
			if( FBitSet( pev->flags, FL_DUCKING ) )    // crouching
				strcpy( szAnim, "crouch_aim_" );
			else
				strcpy( szAnim, "ref_aim_" );
			strcat( szAnim, m_szAnimExtention );

			if( fabs(speed) > 110 )
			{
				animDesired = pev->sequence;

				if( m_Activity == ACT_HOP )
				{
					animDesired = LookupSequence( szAnim );
					if( animDesired == -1 )
						animDesired = 0;
					m_Activity = ACT_WALK;
				}
			}
			else if( (FBitSet( pev->flags, FL_DUCKING )) && (speed < 100) )
			{
				animDesired = pev->sequence;

				if( m_Activity == ACT_HOP || m_Activity == ACT_RELOAD )
				{
					animDesired = LookupSequence( szAnim );
					if( animDesired == -1 )
						animDesired = 0;
					m_Activity = ACT_WALK;
				}
			}
			else
				animDesired = LookupSequence( szAnim );

			if( animDesired == -1 )
				animDesired = 0;

			m_Activity = ACT_WALK;
		}
		else
		{
			animDesired = pev->sequence;
		}
		break;
	}

	if( FBitSet( pev->flags, FL_DUCKING ) )
	{
		if( speed == 0 )
		{
			pev->gaitsequence = LookupActivity( ACT_CROUCHIDLE );
			// pev->gaitsequence    = LookupActivity( ACT_CROUCH );
		}
		else
		{
			if( (pev->button & IN_BACK) && FBitSet( pev->flags, FL_DUCKING ) )
				pev->gaitsequence = LookupSequence( "crawl_back" );
			else
				pev->gaitsequence = LookupActivity( ACT_CROUCH );
		}
	}
	else if( speed > 200 )
	{
		if( pev->button & IN_BACK )
			pev->gaitsequence = LookupSequence( "run_back" );
		else
			pev->gaitsequence = LookupActivity( ACT_RUN );
	}
	else if( speed > 0 )
	{
		if( pev->button & IN_BACK )
			pev->gaitsequence = LookupSequence( "walk_back" );
		else
			pev->gaitsequence = LookupActivity( ACT_WALK );
	}
	else
	{
		// pev->gaitsequence	= LookupActivity( ACT_WALK );
		pev->gaitsequence	= LookupSequence( "deep_idle" );
	}

	if( ForceReset )
	{
		m_Activity = ACT_RESET;

		if( FBitSet( pev->flags, FL_DUCKING ) )    // crouching
			strcpy( szAnim, "crouch_aim_" );
		else
			strcpy( szAnim, "ref_aim_" );
		strcat( szAnim, m_szAnimExtention );

		animDesired = LookupSequence( szAnim );
		if( animDesired == -1 )
			animDesired = 0;

		if( m_Activity == ACT_HOP )
		{
			animDesired = LookupSequence( szAnim );
			if( animDesired == -1 )
				animDesired = 0;
			m_Activity = ACT_WALK;
		}

		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo();
	}


	// Already using the desired animation?
	if ( pev->sequence == animDesired)
		return;

	//ALERT( at_console, "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	pev->sequence		= animDesired;
	pev->frame			= 0;
	ResetSequenceInfo( );
}

//============================================================================
// WaterMove: handle any effects or take actions while player is in water
//============================================================================
#define AIRTIME	12		// lung full of air lasts this many seconds

void CBasePlayer::WaterMove()
{
	if( (pev->vuser1.x > 0.0f) && (pev->waterlevel < 3) )
	{
		pev->vuser1.x -= 10 * gpGlobals->frametime;
		if( pev->vuser1.x < 0.0f )
			pev->vuser1.x = 0.0f;
	}

	if( pev->waterlevel == 0 ) // cache the velocity
		NotInWaterVelocity = GetAbsVelocity();

	if( pev->movetype == MOVETYPE_NOCLIP )
	{
		pev->air_finished = gpGlobals->time + AIRTIME;
		return;
	}

	if (pev->health < 0)
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if (pev->waterlevel != 3) 
	{
		// not underwater
		
		// play 'up for air' sound
		if (pev->air_finished < gpGlobals->time)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "footsteps/wade1.wav", 1, ATTN_NORM);
		else if (pev->air_finished < gpGlobals->time + 9)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "footsteps/wade2.wav", 1, ATTN_NORM);

		pev->air_finished = gpGlobals->time + AIRTIME;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if (m_idrowndmg > m_idrownrestored)
		{
			// set drowning damage bit.  hack - dmg_drownrecover actually
			// makes the time based damage code 'give back' health over time.
			// make sure counter is cleared so we start count correctly.
			
			// NOTE: this actually causes the count to continue restarting
			// until all drowning damage is healed.

			m_bitsDamageType |= DMG_DROWNRECOVER;
			m_bitsDamageType &= ~DMG_DROWN;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		}

	}
	else
	{	// fully under water
		// stop restoring damage while underwater
		m_bitsDamageType &= ~DMG_DROWNRECOVER;
		m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		pev->vuser1.x = 0.5f; // on-screen effect

		if (pev->air_finished < gpGlobals->time)		// drown!
		{
			if (pev->pain_finished < gpGlobals->time)
			{
				// take drowning damage
				pev->dmg += 1;
				if (pev->dmg > 5)
					pev->dmg = 5;
				TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), pev->dmg, DMG_DROWN);
				// NOTENOTE we have another screenfade if received damage is above 15
				UTIL_ScreenFade( this, Vector(50,50,255), 1, 0, 30, FFADE_IN );
				pev->pain_finished = gpGlobals->time + 1;
				
				// track drowning damage, give it back when
				// player finally takes a breath

				m_idrowndmg += pev->dmg;
			} 
		}
		else
			m_bitsDamageType &= ~DMG_DROWN;
	}

//-------------------------------------------------------------------------------------------
	// diffusion - entering water sound implementation + damage from falling into water

	if (!pev->waterlevel)
	{
		if (FBitSet(pev->flags, FL_INWATER))
		{       
			ClearBits(pev->flags, FL_INWATER);
			EnterWaterTime = gpGlobals->time;
		}
		return;
	}

	// hit water with some velocity
	if( pev->waterlevel > 0 && (NotInWaterVelocity.Length() > 0.01f) )
	{
		// play appropriate splash sounds, based on velocity
		if( gpGlobals->time > EnterWaterTime + 0.6 ) // magic number to not spam the sounds (re-entering water again too fast)
		{
			Vector PlayerOrg = GetAbsOrigin();
			if( NotInWaterVelocity.Length() > 400 )
			{
				switch( RANDOM_LONG( 0, 1 ) ) // heavy splash
				{
				case 0:	EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_jump_hard1.wav", 1, ATTN_NORM ); break;
				case 1:	EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_jump_hard2.wav", 1, ATTN_NORM ); break;
				}

				// take damage
				const float vertical_absvelocity = fabs( NotInWaterVelocity.z );
				if( vertical_absvelocity >= PLAYER_MAX_SAFE_FALL_SPEED )
				{
					// do screenshake
					UTIL_ScreenShakeLocal( this, GetAbsOrigin(), 10.0, 150.0, 1.0, 400, true );

					if( vertical_absvelocity >= PLAYER_MAX_SAFE_FALL_SPEED * 1.5 )
					{
						float flFallDamage = (NotInWaterVelocity.Length() - PLAYER_MAX_SAFE_FALL_SPEED) * DAMAGE_FOR_FALL_SPEED * 0.2;
						TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), flFallDamage, DMG_FALL );
					}
				}

				// "shoot" into water to figure out splash particle location
				MakeWaterSplash( PlayerOrg + Vector( 0, 0, 200 ), PlayerOrg - Vector( 0, 0, ((pev->flags & FL_DUCKING) ? 17 : 35) ), 1 );

				// create a box of bubbles under the player (if there's not enough water they just won't spawn so it's okay)
				UTIL_Bubbles( GetAbsOrigin() - Vector( 64, 64, 196 ), GetAbsOrigin() + Vector( 64, 64, -32 ), 100 );
			}
			else
			{
				switch( RANDOM_LONG( 0, 2 ) )  // light splash
				{
				case 0:	EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_jump1.wav", 1, ATTN_NORM ); break;
				case 1:	EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_jump2.wav", 1, ATTN_NORM ); break;
				case 2:	EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_jump3.wav", 1, ATTN_NORM ); break;
				}

				MakeWaterSplash( PlayerOrg + Vector( 0, 0, 200 ), PlayerOrg - Vector( 0, 0, ((pev->flags & FL_DUCKING) ? 17 : 35) ) );
			}
		}

		NotInWaterVelocity = g_vecZero; // we are done, reset the velocity
	}

	// make a small splash sound when we go from underwater and our head shows up
	if( !Submerged && (pev->waterlevel > 2) )
	{
		LastSwimUpSound = gpGlobals->time;
		Submerged = true;
	}

	if( LastSwimUpSound > 0 && pev->waterlevel < 3 )
	{
		// in order for sound to play, the player must be at least for 1 full second underwater
		// no abuse!
		if( gpGlobals->time > LastSwimUpSound + 1.0f )
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_out1.wav", 1, ATTN_NORM ); break;
			case 1: EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_out2.wav", 1, ATTN_NORM ); break;
			case 2: EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/water_out3.wav", 1, ATTN_NORM ); break;
			}
		}

		LastSwimUpSound = -1.0f;
		Submerged = false;
		pev->vuser1.x = 15;
	}
	 
	//-------------------------------------------------------------------------------------------
	// make bubbles

	if (pev->waterlevel == 3) // diffusion - fix by Solokiller
	{
		int air = (int)(pev->air_finished - gpGlobals->time);
		if (!RANDOM_LONG(0,0x1f) && RANDOM_LONG(0,AIRTIME-1) >= air)
		{
			if( gpGlobals->time > LastSwimSound + 1 )
			{
				switch (RANDOM_LONG(0,3))
				{
				case 0:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim1.wav", 0.8, ATTN_NORM); break;
				case 1:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim2.wav", 0.8, ATTN_NORM); break;
				case 2:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim3.wav", 0.8, ATTN_NORM); break;
				case 3:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim4.wav", 0.8, ATTN_NORM); break;
				}
				UTIL_Bubbles( GetAbsOrigin() - Vector( 64, 64, 64 ), GetAbsOrigin() + Vector( 64, 64, 64 ), 15 );
				LastSwimSound = gpGlobals->time;
			}
		}
	}

	if (pev->watertype == CONTENT_LAVA)		// do damage
	{
		if (pev->dmgtime < gpGlobals->time)
			TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 10 * pev->waterlevel, DMG_BURN);
	}
	else if (pev->watertype == CONTENT_SLIME)		// do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 4 * pev->waterlevel, DMG_ACID);
	}
	
	if (!FBitSet(pev->flags, FL_INWATER))
	{
		SetBits(pev->flags, FL_INWATER);
		pev->dmgtime = 0;
	}
}

void CBasePlayer::OnTeleport( void )
{
	// reset torso controllers
	pev->controller[0] = 0x7F;
	pev->controller[1] = 0x7F;
	pev->controller[2] = 0x7F;
	pev->controller[3] = 0x7F;
	pev->gaitsequence = 0;
	pev->sequence = 0;
	pev->frame = 0;
}


// TRUE if the player is attached to a ladder
BOOL CBasePlayer::IsOnLadder( void )
{
	return ( pev->movetype == MOVETYPE_FLY );
}

void CBasePlayer::PlayerDeathThink(void)
{
	float flForward;

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		flForward = GetAbsVelocity().Length() - 20;
		if (flForward <= 0)
			SetAbsVelocity( g_vecZero );
		else    
			SetAbsVelocity( flForward * GetAbsVelocity().Normalize() );
	}

	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
	//	PackDeadPlayerItems();
		// diffusion - drop current weapon and remove the rest
		DropPlayerItem( "" );
		RemoveAllItems( FALSE );
	}

	// Clear inclination came from client view
	pev->angles.x = 0;

	if (pev->modelindex && (!m_fSequenceFinished) && (pev->deadflag == DEAD_DYING))
		StudioFrameAdvance( );

	// time given to animate corpse and don't allow to respawn till this time ends
	if (gpGlobals->time < m_flDeathAnimationStartTime + 1.5)
		return;

	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// this prevents a bug where the dead body would go to a player's head if he walked over it while the dead player was clicking their button to respawn
	if ( pev->movetype != MOVETYPE_NONE && FBitSet(pev->flags, FL_ONGROUND) )
		pev->movetype = MOVETYPE_NONE;

	if (pev->deadflag == DEAD_DYING)
		pev->deadflag = DEAD_DEAD;
	
	StopAnimation();

	pev->effects |= EF_NOINTERP;
//	pev->effects &= ~EF_DIMLIGHT;
//	pev->framerate = 0.0;

	BOOL fAnyButtonDown = (pev->button & ~IN_SCORE );
	m_afButtonLast = pev->button;
	
	// wait for all buttons released
	if (pev->deadflag == DEAD_DEAD)
	{
		if (fAnyButtonDown)
			return;

		if ( g_pGameRules->FPlayerCanRespawn( this ) )
		{
			m_fDeadTime = gpGlobals->time;
			pev->deadflag = DEAD_RESPAWNABLE;
		}
		
		return;
	}

// if the player has been dead for one second longer than allowed by forcerespawn, 
// forcerespawn isn't on. Send the player off to an intermission camera until they 
// choose to respawn.
	if( g_pGameRules->IsMultiplayer() )
	{
		if( mp_killercamera.value > 0 && m_hKiller != NULL )
		{
			if( !m_hKiller->IsAlive() )
			{
				m_hKiller = NULL;
				fLerpToKiller = 0.0f;
				StartDeathCam(); // go to dead camera.
			}
			else
			{
				if( !(m_afPhysicsFlags & PFLAG_OBSERVER) )
				{
					CopyToBodyQue( this );
					m_afPhysicsFlags |= PFLAG_OBSERVER;
					pev->solid = SOLID_NOT;
					pev->takedamage = DAMAGE_NO;
					pev->movetype = MOVETYPE_NOCLIP;
					pev->effects = EF_NODRAW;
				}

				if( m_afPhysicsFlags & PFLAG_OBSERVER ) // observe the killer position
				{
					Vector MyOrg = GetAbsOrigin();
					Vector KillerOrg = m_hKiller->GetAbsOrigin();
					// lerp into killer's position
					Vector dest;
					if( fLerpToKiller < 1.0f )
						fLerpToKiller += 2.0f * gpGlobals->frametime;
					VectorLerp( MyOrg, fLerpToKiller, KillerOrg, dest );
					SetAbsOrigin( dest );
				}
			}
		}
		else if( (gpGlobals->time > (m_fDeadTime + 5)) && !(m_afPhysicsFlags & PFLAG_OBSERVER) )
		{
			StartDeathCam(); // go to dead camera.
		}
	}

	// return if player is spectating
	if( pev->iuser1 )
		return;

	if( gpGlobals->time > HUDtextTime + HUD_TEXT_DELAY )
	{
		if( g_pGameRules->IsMultiplayer() )
			UTIL_ShowMessage( "UTIL_RESPAWN", this );
		else
			UTIL_ShowMessage( "UTIL_RELOAD", this );

		HUDtextTime = gpGlobals->time;
	}
	
	// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if (!fAnyButtonDown 
		&& !( g_pGameRules->IsMultiplayer() && forcerespawn.value > 0 && (gpGlobals->time > (m_fDeadTime + 5))) )
		return;

	pev->button = 0;

	m_flDeathAnimationStartTime = 0;

	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if (!(m_afPhysicsFlags & PFLAG_OBSERVER))	// don't copy a corpse if we're in deathcam.
			CopyToBodyQue(this); // make a copy of the dead body for appearances sake

		// respawn player
		Spawn();
	}
	else
		SERVER_COMMAND("reload\n"); // restart the entire server

	pev->nextthink = -1;
}

//=========================================================
// StartDeathCam: find an intermission spot and send the
// player off into observer mode
//=========================================================
void CBasePlayer::StartDeathCam(void)
{
	edict_t *pSpot, *pNewSpot;
	int iRand;

	if (m_afPhysicsFlags & PFLAG_OBSERVER)
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	CopyToBodyQue(this);

	pSpot = FIND_ENTITY_BY_CLASSNAME(NULL, "info_intermission");
	if (!FNullEnt(pSpot))
	{
		// at least one intermission spot in the world.
		iRand = RANDOM_LONG(0, 3);

		while (iRand > 0)
		{
			pNewSpot = FIND_ENTITY_BY_CLASSNAME(pSpot, "info_intermission");

			if (pNewSpot)
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

		UTIL_SetOrigin(this, pSpot->v.origin);
		// Find target for intermission
		edict_t* pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pSpot->v.target));
		if (pTarget && !FNullEnt(pTarget))
		{
			// Calculate angles to look at camera target
			pev->angles = UTIL_VecToAngles(pTarget->v.origin - pSpot->v.origin);
			pev->angles.x = -pev->angles.x;
		}
		else
			pev->angles = pSpot->v.v_angle;
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, 128), ignore_monsters, edict(), &tr);
		UTIL_SetOrigin(this, tr.vecEndPos - Vector(0, 0, 10));
		pev->angles.x = pev->v_angle.x = 90;
	}

	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->view_ofs = g_vecZero;
	pev->fixangle = TRUE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NOCLIP;	// HACK HACK: Player fall down with MOVETYPE_NONE
	pev->health = 1;					// Let player stay vertically, not lie on a side
	pev->effects = EF_NODRAW;			// Hide model. This is used instead of pev->modelindex = 0
	//pev->modelindex = 0;				// Commented to let view point be moved
}

//========================================================================= 
// PlayerUse: handle USE keypress
//========================================================================= 
#define	PLAYER_SEARCH_RADIUS	(float)64

void CBasePlayer::PlayerUse ( void )
{
	if( IsObserver() )
		return;

	if( !CanUse )
		return;
	
	// Was use pressed or released?
	if ( ! ((pev->button | m_afButtonPressed | m_afButtonReleased) & IN_USE) )
		return;

	m_afPhysicsFlags &= ~PFLAG_USING;

	if ( m_afButtonPressed & IN_USE )
	{
		// prevent use spam
		if( gpGlobals->time < m_flUseReleaseTime + 0.25 )
			return;

		m_flUseReleaseTime = gpGlobals->time;
		
		if ( m_pTank != NULL )
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
			return;
		}
		else if (m_pMonitor != NULL )
		{
			m_pMonitor->Use( this, this, USE_RESET, 0 );
			m_pMonitor = NULL;
			return;
		}
		else if (m_pHoldableItem != NULL )
		{
			DropHoldableItem ( 0 );
			return;
		}
		else if( pCar != NULL )
		{
			pCar->Use( this, this, USE_OFF, 0 );
			return;
		}
		else // Hit Use on a train?
		{
			CFuncTrackTrain *pTrain = CFuncTrackTrain::Instance( pev->groundentity );

			if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
			else
			{	
				// Start controlling the train!
				if ( pTrain && !(pev->button & IN_JUMP) && FBitSet(pev->flags, FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls( this ))
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed( pTrain->GetSpeed(), pTrain->GetMaxSpeed());

					if( pTrain->IsLockedByMaster( this ))
						m_iTrain = TRAIN_LOCKED;
					else
					{
						if( pTrain->pev->speed == 0 )
							EMIT_SOUND( ENT(pev), CHAN_ITEM, "plats/train_use1.wav", 0.8, ATTN_NORM);
					}
					m_iTrain |= TRAIN_NEW;
					return;
				}
			}
		}
	}

	CBaseEntity *pObject = NULL;
	CBaseEntity *pClosest = NULL;
	float flMaxDot = VIEW_FIELD_NARROW;
	TraceResult tr;
	Vector vecLOS;
	float flDot;

	// so we know which way we are facing
	if( m_hDrone && DroneControl )
		UTIL_MakeVectors( m_hDrone->pev->angles );
	else
		UTIL_MakeVectors( pev->v_angle );

	// LRC- try to get an exact entity to use.
	// (is this causing "use-buttons-through-walls" problems? Surely not!)
	Vector EyePos = (m_hDrone && DroneControl) ? m_hDrone->EyePosition() : EyePosition();
	UTIL_TraceLine( EyePos, EyePos + (gpGlobals->v_forward * PLAYER_SEARCH_RADIUS), dont_ignore_monsters, edict(), &tr );

	if( tr.pHit )
	{
		pObject = CBaseEntity::Instance( tr.pHit );

		if( m_hDrone && DroneControl )
		{
			if( !pObject || !(pObject->ObjectCaps() & FCAP_DRONE_USE) )
				pObject = NULL;
		}
		else
		{
			if( !pObject || !(pObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE | FCAP_HOLDABLE_ITEM)) )
				pObject = NULL;
		}
	}

	if( !pObject ) //LRC- couldn't find a direct solid object to use, try the normal method
	{	
		while(( pObject = UTIL_FindEntityInSphere( pObject, (m_hDrone && DroneControl) ? m_hDrone->GetAbsOrigin() : GetAbsOrigin(), PLAYER_SEARCH_RADIUS )) != NULL )
		{
			int caps = pObject->ObjectCaps();
			if( ((m_hDrone && DroneControl) ? (caps & FCAP_DRONE_USE) : 1) && (caps & (FCAP_IMPULSE_USE|FCAP_CONTINUOUS_USE|FCAP_ONOFF_USE|FCAP_HOLDABLE_ITEM)) && !( caps & FCAP_ONLYDIRECT_USE ))
			{
				// !!!PERFORMANCE- should this check be done on a per case basis AFTER we've determined that
				// this object is actually usable? This dot is being done for every object within PLAYER_SEARCH_RADIUS
				// when player hits the use key. How many objects can be in that area, anyway? (sjb)
				vecLOS = (VecBModelOrigin( pObject->pev ) - EyePos);
			
				// This essentially moves the origin of the target to the corner nearest the player to test to see 
				// if it's "hull" is in the view cone
				vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );
			
				flDot = DotProduct (vecLOS , gpGlobals->v_forward);
				if (flDot > flMaxDot )
				{
					// only if the item is in front of the user
					pClosest = pObject;
					flMaxDot = flDot;
//					ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
				}
//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
		}

		pObject = pClosest;
	}

	// Found an object
	if( pObject )
	{		
		if( pObject->HasFlag(F_ENTITY_UNUSEABLE| F_ENTITY_BUSY) )
		{
			// don't show text or use
			if ( m_afButtonPressed & IN_USE )
				EMIT_SOUND( ENT(pev), CHAN_ITEM, "common/use_no.wav", 0.4, ATTN_NORM);
		}
		else
		{
			//!!!UNDONE: traceline here to prevent USEing buttons through walls
			int caps = pObject->ObjectCaps();

			if( m_afButtonPressed & IN_USE )
			{
				// don't play sounds for NPCs, because NPCs will allow respond with speech.
				if( !pObject->MyMonsterPointer() && pObject != m_hDrone )
				{
					EMIT_SOUND( ENT( pev ), CHAN_ITEM, "common/use.wav", 0.4, ATTN_NORM );
					Vector ObjectOrg = VecBModelOrigin( pObject->pev );
					MESSAGE_BEGIN( MSG_ONE, gmsgUseIcon, NULL, pev );
						WRITE_BYTE( USEICON_INTERACTION );
						WRITE_COORD( ObjectOrg.x );
						WRITE_COORD( ObjectOrg.y );
						WRITE_COORD( ObjectOrg.z );
					MESSAGE_END();
				}
			}

			if ( ( (pev->button & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) ||
				 ( (m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
			{
				if ( caps & FCAP_CONTINUOUS_USE )
					m_afPhysicsFlags |= PFLAG_USING;

				pObject->Use( this, this, USE_SET, 1 );
			}
			// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
			else if ( (m_afButtonReleased & IN_USE) && (caps & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
				pObject->Use( this, this, USE_SET, 0 );
			else if ( (m_afButtonPressed & IN_USE) && (caps & FCAP_HOLDABLE_ITEM) )
				PickHoldableItem( pObject ); // picked up the item
		}
	}
	else
	{
		if ( m_afButtonPressed & IN_USE )
			EMIT_SOUND( ENT(pev), CHAN_ITEM, "common/use_no.wav", 0.4, ATTN_NORM);
	}
}

void CBasePlayer::Jump()
{	
	if (FBitSet(pev->flags, FL_WATERJUMP))
		return;
	
	if (pev->waterlevel >= 2)
		return;

	if( DroneControl )
		return;

	if( pCar )
		return;

	// jump velocity is sqrt( height * gravity * 2)

	// If this isn't the first frame pressing the jump button, break out.
	if( !BhopEnabled )
	{
		if( !FBitSet( m_afButtonPressed, IN_JUMP ) )
			return;         // don't pogo stick
	}

	if ( !(pev->flags & FL_ONGROUND) || !pev->groundentity )
		return;

// many features in this function use v_forward, so makevectors now.
	UTIL_MakeVectors (GetAbsAngles());

	// ClearBits(pev->flags, FL_ONGROUND);		// don't stairwalk
	
	SetAnimation( PLAYER_JUMP );

	if ( m_fLongJump &&
		(pev->button & IN_DUCK) &&
		( pev->flDuckTime > 0 ) &&
		GetAbsVelocity().Length() > 50 )
	{
		SetAnimation( PLAYER_SUPERJUMP );
	}

	// If you're standing on a conveyor, add it's velocity to yours (for momentum)
	entvars_t *pevGround = VARS(pev->groundentity);
	if ( pevGround && (pevGround->flags & FL_CONVEYOR) )
		ApplyAbsVelocityImpulse( GetBaseVelocity( ));

	SendAchievementStatToClient( ACH_JUMPS, 1, ACHVAL_ADD );
}

void CBasePlayer::Duck( void )
{
	if( DroneControl )
		return;
	
	if (pev->button & IN_DUCK) 
	{
		if ( m_IdealActivity != ACT_LEAP )
			SetAnimation( PLAYER_WALK );
	}
}

//
// ID's player as such.
//
int CBasePlayer::Classify ( void )
{
	return CLASS_PLAYER;
}


void CBasePlayer::AddPoints( int score, BOOL bAllowNegativeScore )
{
	// Positive score always adds
	if ( score < 0 )
	{
		if ( !bAllowNegativeScore )
		{
			if ( pev->frags < 0 )		// Can't go more negative
				return;
			
			if ( -score > pev->frags )	// Will this go negative?
				score = -pev->frags;		// Sum will be 0
		}
	}

	pev->frags += score;

	RefreshScore();
}

void CBasePlayer::RefreshScore( void )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX( edict() ) );
		WRITE_SHORT( pev->frags );
		WRITE_SHORT( m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( m_szTeamName ) + 1 );
		WRITE_BYTE( HasFlag( F_BOT ) ? 1 : 0 );
		WRITE_BYTE( IsObserver() ? 1 : 0 );
	MESSAGE_END();
}


void CBasePlayer::AddPointsToTeam( int score, BOOL bAllowNegativeScore )
{
	int index = entindex();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && i != index )
		{
			if ( g_pGameRules->PlayerRelationship( this, pPlayer ) == GR_TEAMMATE )
				pPlayer->AddPoints( score, bAllowNegativeScore );
		}
	}
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0; 

	m_flHealthbarsDisappearDelay = 0;
//	memset( healthbarscache, 0, sizeof( healthbarscache ) );
}

void CBasePlayer::UpdateStatusBar()
{
	int newSBarState[ SBAR_END ];
	char sbuf0[ SBAR_STRING_SIZE ];
	char sbuf1[ SBAR_STRING_SIZE ];
	char TmpText[32];

	memset( newSBarState, 0, sizeof(newSBarState) );
	Q_strncpy( sbuf0, m_SbarString0, SBAR_STRING_SIZE );
	Q_strncpy( sbuf1, m_SbarString1, SBAR_STRING_SIZE );

	// Find an ID Target
	TraceResult tr;

	// diffusion - account fov
	float currentfov = m_flFOV;
	if( currentfov == 0.0f )
		currentfov = 80;

	float fov_mult = 80.0f / currentfov;

	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector vecSrc = EyePosition();
	Vector vecEnd = vecSrc + (gpGlobals->v_forward * 8192 * fov_mult);

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	int healthbarsnewvalues[4];
	memset( healthbarsnewvalues, 0, sizeof( healthbarsnewvalues ) );

	if( sv_allowhealthbars.value > 0 )
	{
		if( tr.flFraction != 1.0 && HasWeapon( WEAPON_SUIT ) && !(pev->effects & EF_PLAYERUSINGCAMERA) )
		{
			if( !FNullEnt( tr.pHit ) )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
				if( pEntity && pEntity->EnableHealthBar )
				{
					m_flHealthbarsDisappearDelay = gpGlobals->time + 0.1;

					if( pEntity->IsMonster() )
					{
						CBaseMonster *pMonster = (CBaseMonster *)pEntity;

						if( pMonster->m_IdealMonsterState != MONSTERSTATE_DEAD )
						{
							healthbarsnewvalues[0] = 100 * (pMonster->pev->health / pMonster->pev->max_health);
							healthbarsnewvalues[1] = pMonster->HealthBarType;
							healthbarsnewvalues[2] = pMonster->entindex();
							healthbarsnewvalues[3] = pMonster->bForceHealthbar;
						}
						else
							memset( healthbarsnewvalues, 0, sizeof( healthbarsnewvalues ) );
					}
					else if( pEntity->IsPlayer() && g_pGameRules->IsMultiplayer() )
					{
						// allow healthbars in multiplayer (can be disabled clientside, but server will send them anyway if allowed)
						if( pEntity->pev->deadflag == DEAD_NO ) // this changes in killed, so should work
						{
							healthbarsnewvalues[0] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
							healthbarsnewvalues[1] = pEntity->HealthBarType;
							healthbarsnewvalues[2] = pEntity->entindex();
							healthbarsnewvalues[3] = pEntity->bForceHealthbar;
						}
						else
							memset( healthbarsnewvalues, 0, sizeof( healthbarsnewvalues ) );
					}
					else if( pEntity->EnableHealthBar )
					{
						if( pEntity->pev->health > 0 )
						{
							healthbarsnewvalues[0] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
							healthbarsnewvalues[1] = pEntity->HealthBarType;
							healthbarsnewvalues[2] = pEntity->entindex();
							healthbarsnewvalues[3] = pEntity->bForceHealthbar;
						}
						else
							memset( healthbarsnewvalues, 0, sizeof( healthbarsnewvalues ) );
					}
				}
			}
			else if( m_flHealthbarsDisappearDelay > gpGlobals->time )
			{
				// hold the values for a short amount of time after viewing the object
				healthbarsnewvalues[0] = healthbarscache[0];
				healthbarsnewvalues[1] = healthbarscache[1];
				healthbarsnewvalues[2] = healthbarscache[2];
				healthbarsnewvalues[3] = healthbarscache[3];
			}
		}
	}

	// Check values and send if they don't match
	bool ResendHealthbar = false;
	if( memcmp( healthbarsnewvalues, healthbarscache, 4 ) )
		ResendHealthbar = true;

	if( ResendHealthbar )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHealthbars, NULL, pev );
			WRITE_BYTE( bound( 0, healthbarsnewvalues[0], 200) ); // hp in percents
			WRITE_BYTE( healthbarsnewvalues[1] ); // bar size (0-2)
			WRITE_SHORT( healthbarsnewvalues[2] ); // entindex
			WRITE_BYTE( healthbarsnewvalues[3] ); // force healthbar?
		MESSAGE_END();

		healthbarscache[0] = healthbarsnewvalues[0];
		healthbarscache[1] = healthbarsnewvalues[1];
		healthbarscache[2] = healthbarsnewvalues[2];
		healthbarscache[3] = healthbarsnewvalues[3];
	}

	// original hl stuff
	if( (vecSrc - tr.vecEndPos).Length() <= MAX_ID_RANGE )
	{
		if( tr.flFraction != 1.0 )
		{
			if( !FNullEnt( tr.pHit ) )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

				if( pEntity )
				{
					if( pEntity->Classify() == CLASS_PLAYER )
					{
						newSBarState[SBAR_ID_TARGETNAME] = ENTINDEX( pEntity->edict() );
						Q_strncpy( sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%", SBAR_STRING_SIZE );

						// allies and medics get to see the targets health
						if( g_pGameRules->PlayerRelationship( this, pEntity ) == GR_TEAMMATE )
						{
							newSBarState[SBAR_ID_TARGETHEALTH] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
							newSBarState[SBAR_ID_TARGETARMOR] = pEntity->pev->armorvalue; // No need to get it % based since 100 it's the max.
						}

						m_flStatusBarDisappearDelay = gpGlobals->time + 1.0;
					}
					// diffusion monster-specific stuff
					else if( pEntity->IsMonster() && pEntity->IsAlive() && !FStringNull( pEntity->CustomNameString ) )
					{
						newSBarState[SBAR_ID_TARGETNAME] = ENTINDEX( pEntity->edict() );
						newSBarState[SBAR_ID_TARGETHEALTH] = 0; // not used here
						newSBarState[SBAR_ID_TARGETARMOR] = 0; // not used here

						sprintf( TmpText, "%s\n", STRING( pEntity->CustomNameString ) );

						strcpy( sbuf1, "1 " );
						strcat( sbuf1, (char *)(TmpText) );
						strcat( sbuf1, "2\n3" );

						m_flStatusBarDisappearDelay = gpGlobals->time + 0.2;
					}
				}
			}
			else if( m_flStatusBarDisappearDelay > gpGlobals->time )
			{
				// hold the values for a short amount of time after viewing the object
				newSBarState[SBAR_ID_TARGETNAME] = m_izSBarState[SBAR_ID_TARGETNAME];
				newSBarState[SBAR_ID_TARGETHEALTH] = m_izSBarState[SBAR_ID_TARGETHEALTH];
				newSBarState[SBAR_ID_TARGETARMOR] = m_izSBarState[SBAR_ID_TARGETARMOR];
			}
		}
	}
	
	BOOL bForceResend = FALSE;

	if ( strcmp( sbuf0, m_SbarString0 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( sbuf0 );
		MESSAGE_END();

		Q_strncpy( m_SbarString0, sbuf0, SBAR_STRING_SIZE );

		// make sure everything's resent
		bForceResend = TRUE;
	}

	if ( strcmp( sbuf1, m_SbarString1 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( sbuf1 );
		MESSAGE_END();

		Q_strncpy( m_SbarString1, sbuf1, SBAR_STRING_SIZE );

		// make sure everything's resent
		bForceResend = TRUE;
	}
	
	// Check values and send if they don't match
	for (int i = 1; i < SBAR_END; i++)
	{
		if ( newSBarState[i] != m_izSBarState[i] || bForceResend )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusValue, NULL, pev );
				WRITE_BYTE( i );
				WRITE_SHORT( newSBarState[i] );
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
}

void CBasePlayer :: TransferReset( void )
{
	// rebuild key-catcheres after changelevel
	// g-cont. may be put in Restore?
	UpdateKeyCatchers();

	CreateFlashlightMonster();
}

void CBasePlayer :: UpdateKeyCatchers( void )
{
	CBaseEntity *pTarget = UTIL_FindEntityByClassname( NULL, "player_keycatcher" );
	m_iNumKeyCatchers = 0;

	while( pTarget && ( m_iNumKeyCatchers < MAX_KEYCATCHERS ))
	{
		// make sure that key catcher is active
		if( pTarget->pev->button != 0 )
			m_hKeyCatchers[m_iNumKeyCatchers++] = pTarget;
		pTarget = UTIL_FindEntityByClassname( pTarget, "player_keycatcher" );
	}
}

void CBasePlayer::CreateFlashlightMonster( void )
{
	m_pFlashlightMonster = NULL;

	if( gpGlobals->maxClients != 1 )
		return;

	CBaseEntity *tmp = NULL;
	// delete all flashlight monsters (maybe there's a few...just in case O.O)
	while( (tmp = UTIL_FindEntityByClassname( tmp, "_flashlight" )) != NULL )
		UTIL_Remove( tmp );

	// create a new one
	m_pFlashlightMonster = CBaseEntity::Create( "_flashlight", GetAbsOrigin(), g_vecZero, edict() );
	if( !FlashlightIsOn() )
		m_pFlashlightMonster->pev->effects |= EF_NODRAW;
//	m_pFlashlightMonster->SetModel( "sprites/laserdot.spr" );
//	m_pFlashlightMonster->pev->rendermode = kRenderGlow;
//	m_pFlashlightMonster->pev->renderfx = kRenderFxNoDissipation;
//	m_pFlashlightMonster->pev->renderamt = 255;
}

//==========================================================================
// ManageRope: handle movement on a rope.
//==========================================================================
void CBasePlayer::ManageRope(void)
{
	// We're on a rope. - Solokiller
	if (m_afPhysicsFlags & PFLAG_ONROPE && m_pRope)
	{
		SetAbsVelocity(g_vecZero);

		const Vector vecAttachPos = m_pRope->GetAttachedObjectsPosition();

		SetAbsOrigin(vecAttachPos);

		Vector vecForce;

		/*
		//TODO: This causes sideways acceleration that doesn't occur in Op4. - Solokiller
		//TODO: should be IN_MOVERIGHT and IN_MOVELEFT - Solokiller
		if( pev->button & IN_DUCK )
		{
			vecForce.x = gpGlobals->v_right.x;
			vecForce.y = gpGlobals->v_right.y;
			vecForce.z = 0;

			m_pRope->ApplyForceFromPlayer( vecForce );
		}

		if( pev->button & IN_JUMP )
		{
			vecForce.x = -gpGlobals->v_right.x;
			vecForce.y = -gpGlobals->v_right.y;
			vecForce.z = 0;
			m_pRope->ApplyForceFromPlayer( vecForce );
		}
		*/

		//Determine if any force should be applied to the rope, or if we should move around. - Solokiller
		if (pev->button & (IN_BACK | IN_FORWARD))
		{
			if ((gpGlobals->v_forward.x * gpGlobals->v_forward.x +
				gpGlobals->v_forward.y * gpGlobals->v_forward.y -
				gpGlobals->v_forward.z * gpGlobals->v_forward.z) <= 0.0)
			{
				if (m_bIsClimbing)
				{
					const float flDelta = gpGlobals->time - m_flLastClimbTime;
					m_flLastClimbTime = gpGlobals->time;
					if (pev->button & IN_FORWARD)
					{
						if (gpGlobals->v_forward.z < 0.0)
						{
							if (!m_pRope->MoveDown(flDelta))
							{
								//Let go of the rope, detach. - Solokiller
								pev->movetype = MOVETYPE_WALK;
								pev->solid = SOLID_SLIDEBOX;

								m_afPhysicsFlags &= ~PFLAG_ONROPE;
								m_pRope->DetachObject();
								m_pRope = NULL;
								m_bIsClimbing = false;
							}
						}
						else
						{
							m_pRope->MoveUp(flDelta);
						}
					}

					if (pev->button & IN_BACK)
					{
						if (gpGlobals->v_forward.z < 0.0)
						{
							m_pRope->MoveUp(flDelta);
						}
						else if (!m_pRope->MoveDown(flDelta))
						{
							//Let go of the rope, detach. - Solokiller
							pev->movetype = MOVETYPE_WALK;
							pev->solid = SOLID_SLIDEBOX;
							m_afPhysicsFlags &= ~PFLAG_ONROPE;
							m_pRope->DetachObject();
							m_pRope = NULL;
							m_bIsClimbing = false;
						}
					}
				}
				else
				{
					m_bIsClimbing = true;
					m_flLastClimbTime = gpGlobals->time;
				}
			}
			else
			{
				vecForce.x = gpGlobals->v_forward.x;
				vecForce.y = gpGlobals->v_forward.y;
				vecForce.z = 0.0;
				if (pev->button & IN_BACK)
				{
					vecForce.x = -gpGlobals->v_forward.x;
					vecForce.y = -gpGlobals->v_forward.y;
					vecForce.z = 0;
				}
				m_pRope->ApplyForceFromPlayer(vecForce);
				m_bIsClimbing = false;
			}
		}
		else
		{
			m_bIsClimbing = false;
		}

		if (m_afButtonPressed & IN_JUMP)
		{
			//We've jumped off the rope, give us some momentum - Solokiller
			pev->movetype = MOVETYPE_WALK;
			pev->solid = SOLID_SLIDEBOX;
			this->m_afPhysicsFlags &= ~PFLAG_ONROPE;

			Vector vecDir = gpGlobals->v_up * 165.0 + gpGlobals->v_forward * 150.0;

			Vector vecVelocity = m_pRope->GetAttachedObjectsVelocity() * 2;

			vecVelocity = vecVelocity.Normalize();

			vecVelocity = vecVelocity * 200;

			SetAbsVelocity(vecVelocity + vecDir);

			m_pRope->DetachObject();
			m_pRope = NULL;
			m_bIsClimbing = false;
		}
		return;
	}
}

//==========================================================================
// ManageTrain: handle controls of a train.
//==========================================================================
void CBasePlayer::ManageTrain(void)
{
	// Train speed control
	if (m_afPhysicsFlags & PFLAG_ONTRAIN)
	{
		CFuncTrackTrain* pTrain = CFuncTrackTrain::Instance(pev->groundentity);
		float vel;

		if (!pTrain)
		{
			TraceResult trainTrace;
			// Maybe this is on the other side of a level transition
			UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -38), ignore_monsters, edict(), &trainTrace);

			// HACKHACK - Just look for the func_tracktrain classname
			if (trainTrace.flFraction != 1.0 && trainTrace.pHit)
				pTrain = CFuncTrackTrain::Instance(trainTrace.pHit);

			if (!pTrain || !(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) || !pTrain->OnControls(this))
			{
				//ALERT( at_error, "In train mode with no train!\n" );
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				return;
			}
		}
		else if (!FBitSet(pev->flags, FL_ONGROUND) || FBitSet(pTrain->pev->spawnflags, SF_TRACKTRAIN_NOCONTROL) /*|| (pev->button & (IN_MOVELEFT|IN_MOVERIGHT) )*/) // diffusion - that's not needed tbh
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW | TRAIN_OFF;
			return;
		}

		SetAbsVelocity(g_vecZero);
		vel = 0;
		if (m_afButtonPressed & IN_FORWARD)
		{
			vel = 1;
			pTrain->Use(this, this, USE_SET, (float)vel);
		}
		else if (m_afButtonPressed & IN_BACK)
		{
			vel = -1;
			pTrain->Use(this, this, USE_SET, (float)vel);
		}

		if (vel)
		{
			if (pTrain->IsLockedByMaster(this))
				m_iTrain = TRAIN_LOCKED;
			else
				m_iTrain = TrainSpeed(pTrain->GetSpeed(), pTrain->GetMaxSpeed());

			m_iTrain |= TRAIN_ACTIVE | TRAIN_NEW;
		}

	}
	else if( m_iTrain & TRAIN_ACTIVE )
		m_iTrain = TRAIN_NEW; // turn off train
}

#define CLIMB_SHAKE_FREQUENCY	22	// how many frames in between screen shakes when climbing
#define	MAX_CLIMB_SPEED			190	// fastest vertical climbing speed possible
#define	CLIMB_SPEED_DEC			15	// climbing deceleration rate
#define	CLIMB_PUNCH_X			-7  // how far to 'punch' client X axis when climbing
#define CLIMB_PUNCH_Z			7	// how far to 'punch' client Z axis when climbing

void CBasePlayer::PreThink( void )
{
	// clear motion blur if not in car
	if( !pCar && pev->vuser1.y > 0 )
		pev->vuser1.y = 0;

	// drunk? edit friction
	if( DrunkLevel > 0 )
		pev->friction = 1.0f - (DrunkLevel * 0.1f);

	if( Dashed && pev->flags & FL_ONGROUND )
		pev->friction = 0.0f;

	// do not interfere with player angles because they are used to catch mouse movement
	if( pCar )
		pev->punchangle = g_vecZero;

	int buttonsChanged = (m_afButtonLast ^ pev->button);	// These buttons have changed this frame

	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed = buttonsChanged & pev->button;		// The changed ones still down are "pressed"
	m_afButtonReleased = buttonsChanged & (~pev->button);	// The ones not down are "released"

	if( sv_enablebunnyhopping.value > 0 && !BhopEnabled )
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "bhop", "1" );
		BhopEnabled = true;
	}
	else if( sv_enablebunnyhopping.value <= 0 && BhopEnabled )
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "bhop", "0" );
		BhopEnabled = false;
	}

	g_pGameRules->PlayerThink( this );

	if( g_fGameOver )
		return;         // intermission or finale

	for( int i = 0; i < m_iNumKeyCatchers && (pev->button || buttonsChanged); i++ )
	{
		if( m_hKeyCatchers[i] != NULL )
		{
			CPlayerKeyCatcher *pCatch = (CPlayerKeyCatcher *)(CBaseEntity *)m_hKeyCatchers[i];
			if( pCatch ) pCatch->CatchButton( this, pev->button, m_afButtonPressed, m_afButtonReleased );
		}
	}

//	ALERT( at_console, "server fov %f\n", m_flFOV );

	// diffusion - clamped
	if( gpGlobals->maxClients > 1 ) // multiplayer
	{
		if( pev->health > 250 )
			pev->health = 250;

		if( pev->max_health > 250 )
			pev->max_health = 250;
	}
	else // singleplayer
	{
		if( pev->health > 200 )
			pev->health = 200;
		if( pev->max_health > 200 )
			pev->max_health = 200;
	}

	if( ShieldOn )
	{
		if( ShieldAvailableLVL == 0 )
		{
			ShieldOn = false;
		}
		else
		{
			if( m_flStaminaValue > 0 )
			{
				m_flStaminaValue -= (10.0f / (float)ShieldAvailableLVL) * gpGlobals->frametime;
			}

			if( m_flStaminaValue <= 0 )
				ShieldOn = false;
		}
	}

	UTIL_MakeVectors( pev->v_angle );             // is this still used?

	ItemPreFrame();

	WaterMove();

	if( g_pGameRules && g_pGameRules->FAllowFlashlight() )
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;

	// show bot waypoints
	if( bot_show_all_points.value )
	{
		if( WaypointsLoaded > 0 )
		{
			static float lastwpdebugtime = 0;
			for( int w = 0; w < WaypointsLoaded; w++ )
			{
				if( (vWaypoint[w] - pev->origin).Length() < 100 )
				{
					char test[100];
					snprintf( test, sizeof( test ), "WP %i [%i], conn to %i %i %i %i %i\n", w, PointFlag[w], ConnectedPoints[w][0], ConnectedPoints[w][1], ConnectedPoints[w][2], ConnectedPoints[w][3], ConnectedPoints[w][4] );
					UTIL_CenterPrintAll( test );

					for( int c = 0; c < ConnectionsForPoint[w]; c++ )
					{
						MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
						WRITE_BYTE( TE_BEAMPOINTS );
						WRITE_COORD( vWaypoint[w].x );
						WRITE_COORD( vWaypoint[w].y );
						WRITE_COORD( vWaypoint[w].z );
						WRITE_COORD( vWaypoint[ConnectedPoints[w][c]].x );
						WRITE_COORD( vWaypoint[ConnectedPoints[w][c]].y );
						WRITE_COORD( vWaypoint[ConnectedPoints[w][c]].z );
						WRITE_SHORT( g_sModelIndexLaser );
						WRITE_BYTE( 0 ); // startframe
						WRITE_BYTE( 0 ); // framerate
						WRITE_BYTE( 1 ); // life

						WRITE_BYTE( 10 );  // width

						WRITE_BYTE( 0 );   // noise

						// secondary shot is always white, and intensity based on charge
						WRITE_BYTE( 255 );   // r, g, b
						WRITE_BYTE( 255 );   // r, g, b
						WRITE_BYTE( 255 );   // r, g, b

						WRITE_BYTE( 255 );	// brightness

						WRITE_BYTE( 0 );		// speed
						MESSAGE_END();
					}

					break;
				}

				if( lastwpdebugtime > gpGlobals->time + 0.2 )
					lastwpdebugtime = gpGlobals->time + 0.2;

				if( gpGlobals->time > lastwpdebugtime )
				{
					for( int wp = 0; wp < WaypointsLoaded; wp++ )
					{
						if( (vWaypoint[wp] - pev->origin).Length() < 1000 )
							UTIL_Ricochet( vWaypoint[wp], 1.0 );
					}

					lastwpdebugtime = gpGlobals->time + 0.2;
				}
			}
		}
	}


	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();

	CheckTimeBasedDamage();

	CheckSuitUpdate();

	// Observer Button Handling
	if( IsObserver() )
	{
		Observer_HandleButtons();
		Observer_CheckTarget();
		Observer_CheckProperties();
		pev->impulse = 0;
		return;
	}

	// Welcome cam buttons handling
	if( m_bInWelcomeCam )
	{
		if( gpGlobals->time > HUDtextTime + HUD_TEXT_DELAY )
		{
			UTIL_ShowMessage( "UTIL_SPAWN", this );
			HUDtextTime = gpGlobals->time;
		}

		if( m_afButtonPressed & IN_ATTACK )
			StopWelcomeCam();

		return;
	}

	ManageDrone(); // must be before deaththink for reasons
	ManageCar();

	if( pev->deadflag >= DEAD_DYING )
	{
		PlayerDeathThink();
		return;
	}

	//	if( !(g_pGameRules->IsMultiplayer()) )
	//		PlayWallSlideSound( 1 );

	if( HasWeapon( WEAPON_SUIT ) && BrokenSuit && (gpGlobals->time > NextBrokenDmgTime) )
	{
		float dmg_amt = RANDOM_FLOAT( 3.0f, 15.0f );
		if( dmg_amt >= pev->health ) // don't die
			dmg_amt = pev->health * 0.5f;
		TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), dmg_amt, DMG_SHOCK );
		UTIL_DoSpark( pev, GetAbsOrigin() );
		// add glitch effect
		MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
			WRITE_BYTE( TE_PLAYER_GLITCH );
			WRITE_BYTE( 10 ); // x0.1
			WRITE_BYTE( 0 ); // don't hold
		MESSAGE_END();
		NextBrokenDmgTime = gpGlobals->time + RANDOM_LONG( 45, 75 );
	}

	// So the correct flags get sent to client asap.
	// diffusion - an exception when controlling drone
	if( !DroneControl )
	{
		if( m_afPhysicsFlags & PFLAG_ONTRAIN )
			pev->flags |= FL_ONTRAIN;
		else
			pev->flags &= ~FL_ONTRAIN;
	}

	ManageRope();

	ManageTrain();

	if (pev->button & IN_JUMP)
	{
		// If on a ladder, jump off the ladder
		// else Jump
		Jump();
	}

	// If trying to duck, already ducked, or in the process of ducking
	if ((pev->button & IN_DUCK) || FBitSet(pev->flags,FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING) )
		Duck();

	if( pev->effects & EF_UPSIDEDOWN )
	{
		if( pev->gravity > 0 )
			pev->gravity = -pev->gravity;
	}
	else
	{
		if( pev->gravity < 0 )
			pev->gravity = -pev->gravity;
	}

	if( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		if( pev->effects & EF_UPSIDEDOWN )
			m_flFallVelocity = GetAbsVelocity().z;
		else
			m_flFallVelocity = -GetAbsVelocity().z;
	}

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;

	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
		SetAbsVelocity( g_vecZero );

	if (m_pHoldableItem != NULL)
		UpdateHoldableItem ();
#if 0
	if ( !g_pGameRules->IsMultiplayer() && (m_flStaminaValue > 20) && !pCar ) //DiffusionSprint
	{
		if ( m_afButtonPressed & IN_JUMP && (pev->flags & FL_ONGROUND ) && !DroneControl )
			m_flStaminaValue -= 20.0f;
		else if ( m_afButtonReleased & IN_JUMP )
			return;
	}
#endif
	// diffusion - USE icon
	CBaseEntity *pUseObject = NULL;

	if( !CanUse || (pev->flags & FL_FROZEN) || pCar )
		m_iUseIcon = USEICON_NOICON;
	else
	{
		// find a useable object, figure out what icon to show
		if( gpGlobals->time > LastUseCheckTime + 0.2 )
		{
			TraceResult tracer;
			if( m_hDrone && DroneControl )
				UTIL_MakeVectors( m_hDrone->pev->angles );
			else
				UTIL_MakeVectors( pev->v_angle );
			Vector EyePos = (m_hDrone && DroneControl) ? m_hDrone->EyePosition() : EyePosition();
			UTIL_TraceLine( EyePos, EyePos + (gpGlobals->v_forward * PLAYER_SEARCH_RADIUS), dont_ignore_monsters, edict(), &tracer );

			if( tracer.pHit )
			{
				pUseObject = CBaseEntity::Instance( tracer.pHit );
				if( m_hDrone && DroneControl )
				{
					if( !pUseObject || !(pUseObject->ObjectCaps() & FCAP_DRONE_USE) )
						pUseObject = NULL;
					else
						m_hCachedUseObject = pUseObject;
				}
				else
				{
					if( !pUseObject || !(pUseObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE | FCAP_HOLDABLE_ITEM)) )
						pUseObject = NULL;
					else
						m_hCachedUseObject = pUseObject;
				}
			}
			
			Vector vecLOS;
			float flDot;
			CBaseEntity *pClosest = NULL;
			float flMaxDot = VIEW_FIELD_NARROW;

			if( !pUseObject )
			{
				while( (pUseObject = UTIL_FindEntityInSphere( pUseObject, ( m_hDrone && DroneControl ) ? m_hDrone->GetAbsOrigin() : GetAbsOrigin(), PLAYER_SEARCH_RADIUS )) != NULL )
				{
					int caps = pUseObject->ObjectCaps();

					if( ((m_hDrone && DroneControl) ? (caps & FCAP_DRONE_USE) : 1) && (caps & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE | FCAP_HOLDABLE_ITEM)) && !(caps & FCAP_ONLYDIRECT_USE) )
					{
						vecLOS = (VecBModelOrigin( pUseObject->pev ) - EyePos);
						vecLOS = UTIL_ClampVectorToBox( vecLOS, pUseObject->pev->size * 0.5 );
						flDot = DotProduct( vecLOS, gpGlobals->v_forward );
						if( flDot > flMaxDot )
						{
							pClosest = pUseObject;
							flMaxDot = flDot;
						}
					}
				}

				pUseObject = pClosest;
				m_hCachedUseObject = pUseObject;
			}

			LastUseCheckTime = gpGlobals->time;
		}
	}

	if( pUseObject == NULL ) // use cached object between checks, so the icon won't flicker
		pUseObject = m_hCachedUseObject;

	if( pUseObject && (!pUseObject->IsMonster() || pUseObject == m_hDrone) && CanUse && !m_pHoldableItem )
	{
		if( pUseObject->HasFlag( F_ENTITY_BUSY ) )
			m_iUseIcon = USEICON_BUSY;
		else if( pUseObject->HasFlag( F_ENTITY_UNUSEABLE ) || pUseObject->HasFlag( F_BUTTON_SECRET ) )
			m_iUseIcon = USEICON_NOICON;
		else
		{
			if( pev->button & IN_USE )
			{
				if( gpGlobals->time > m_flUseReleaseTime + 0.5 )
					m_iUseIcon = USEICON_CANUSE;
				else
				{
					if( pUseObject->HasFlag( F_BUTTON_LOCKED ) )
						m_iUseIcon = USEICON_LOCKED;
					else
						m_iUseIcon = USEICON_PRESSED;
				}
			}
			else
				m_iUseIcon = USEICON_CANUSE;
		}

		UseEntOrg = VecBModelOrigin( pUseObject->pev );
	}
	else // found nothing?
	{
		m_iUseIcon = USEICON_NOICON;
		UseEntOrg = g_vecZero;
	}

	if( m_iClientUseIcon != m_iUseIcon || ClientUseEntOrg != UseEntOrg )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgUseIcon, NULL, pev );
			WRITE_BYTE( m_iUseIcon );
			if( pUseObject )
			{
				WRITE_COORD( UseEntOrg.x );
				WRITE_COORD( UseEntOrg.y );
				WRITE_COORD( UseEntOrg.z );
			}
			else
			{
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );
			}
		MESSAGE_END();

		m_iClientUseIcon = m_iUseIcon;
		ClientUseEntOrg = UseEntOrg;
	}

	// diffusion - can drop items with force now :)
	if( m_pHoldableItem != NULL )
	{
		// start counting
		if (pev->button & IN_ATTACK)
		{
			// around 2-3 seconds to full velocity
			DropHoldItemVel += 300 * gpGlobals->frametime;
			DropHoldItemVel = bound( 50, DropHoldItemVel, 500 );
			// show some visual feedback...
			if( DropHoldItemVel > 350 )
				UTIL_ShowMessage( "UTIL_GRENSAFE3", this );
			else if( DropHoldItemVel > 225 )
				UTIL_ShowMessage( "UTIL_GRENSAFE2", this );
			else if( DropHoldItemVel > 100 )
				UTIL_ShowMessage( "UTIL_GRENSAFE1", this );
		}
		else if( m_afButtonReleased & IN_ATTACK )
		{
			pev->punchangle.x = DropHoldItemVel * 0.04;
			DropHoldableItem( DropHoldItemVel );
			DropHoldItemVel = 0;
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, "player/throw.wav", 0.5f, ATTN_NORM, 0, RANDOM_LONG( 85, 105 ) );
		}
	}

	ManageElectroBlast();
}

//==========================================================================
// ManageDrone: handle the active friendly drone :)
// multiplayer is not supported yet!
//==========================================================================
void CBasePlayer::ManageDrone( void )
{
	if( DroneDeployed )
	{
		if( m_hDrone == NULL )
		{
			// the drone was deployed, search and grab a pointer
			CBaseEntity *pDrone = NULL;
			while ( (pDrone = UTIL_FindEntityByClassname( pDrone, "_playerdrone" )) != NULL )
			{
				if( pDrone->pev->owner == edict() )
				{
					m_hDrone = pDrone;
					CBaseMonster *pDroneMonster = pDrone->MyMonsterPointer();
					pDroneMonster->m_hEnemy = NULL;
					// apply custom color
					m_hDrone->pev->rendercolor = DroneColor;
					// fire target if set
					if( !FStringNull(DroneTarget_OnDeploy) )
						UTIL_FireTargets( DroneTarget_OnDeploy, this, this, USE_TOGGLE, 0 );
					break;
				}
			}
			
			if( pDrone == NULL )
				DroneDeployed = false; // safety check
		}
		else // manage an active pointer
		{
			if( !CanUseDrone || (m_pActiveItem && (pev->button & IN_RELOAD) && (m_pActiveItem->m_iId == WEAPON_DRONE)) ) // retrieve the drone instantly
			{
				m_hDrone->Use( this, this, USE_TOGGLE, 0 );
				return;
			}
			
			DroneDistance = (int)fUnitsToMeters( (m_hDrone->GetAbsOrigin() - GetAbsOrigin()).Length() );
			DroneHealth = m_hDrone->pev->health;
			DroneAmmo = m_hDrone->m_iCounter;

			if( DroneControl ) // drone is in 1st person mode - this sets by weapon_drone
			{
				if( CameraEntity[0] == '\0' )
				{
					SET_VIEW( edict(), m_hDrone->edict() );
					Q_strcpy( CameraEntity, "_friendlydrone" );
					UTIL_ScreenFade( this, g_vecZero, 0.5, 0, 255, FFADE_IN );
					m_hDrone->SetFlag( F_PLAYER_CONTROL );
					pev->flags |= FL_ONTRAIN;
					pev->effects |= EF_PLAYERDRONECONTROL;
					DroneCrosshairUpdate = true;
					m_hDrone->pev->effects &= ~EF_MERGE_VISIBILITY;
					// disable player's flashlight
					if( FlashlightIsOn() )
						FlashlightTurnOff();
					// fire target if set
					if( !FStringNull( DroneTarget_OnEnteringFirstPerson ) )
						UTIL_FireTargets( DroneTarget_OnEnteringFirstPerson, this, this, USE_TOGGLE, 0 );
					// we are all set and controlling the drone.
				}

				if( !Q_strcmp( CameraEntity, "_friendlydrone" ) )
				{
					// copy the view parameters for correct distance-culling
					CameraOrigin = m_hDrone->GetAbsOrigin();
					CameraAngles = m_hDrone->GetAbsAngles();
				}
				else // player have changed the camera?
				{
					DroneControl = false;
					return;
				}

				if( m_pActiveItem != NULL ) // changed weapon?
				{
					if( m_pActiveItem->m_iId != WEAPON_DRONE )
					{
						DroneControl = false;
						return;
					}
				}

				// copy player's mouse movement so we can rotate the camera
				Vector DroneAngles = pev->v_angle;
				DroneAngles.x *= 0.4; // yes
				m_hDrone->SetAbsAngles( DroneAngles );
				UTIL_MakeVectors( DroneAngles );
				Vector vecShootDir = gpGlobals->v_forward; // copy to use later

				// movement
				// set up the direction
				if( pev->button & IN_FORWARD )
					drone_forwmove = 1;
				else if( pev->button & IN_BACK )
					drone_forwmove = -1;
				else
					drone_forwmove = 0;

				if( pev->button & IN_MOVERIGHT )
					drone_sidemove = 1;
				else if( pev->button & IN_MOVELEFT )
					drone_sidemove = -1;
				else
					drone_sidemove = 0;

				if( pev->button & IN_JUMP )
					drone_upmove = 1;
				else if( pev->button & IN_DUCK )
					drone_upmove = -1;
				else
					drone_upmove = 0;

				// accelerate/decelerate
				if( pev->button & (IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT | IN_JUMP | IN_DUCK) )
					drone_Speed += 200 * gpGlobals->frametime;
				else
					drone_Speed -= 300 * gpGlobals->frametime;

				drone_Speed = bound( 0, drone_Speed, 300 );

				// update the direction vector
				// only update this vector once in a while to prevent stucking in walls and the drone could bounce
				if( drone_UpdateTime > gpGlobals->time )
					drone_UpdateTime = 0; // map change / saverestore issues

				if( gpGlobals->time > drone_UpdateTime + 0.1 )
				{
					// record current movement direction
					drone_currentdir = m_hDrone->GetAbsVelocity().Normalize();
					// update direction
					m_hDrone->SetAbsVelocity( m_hDrone->GetAbsVelocity() + (gpGlobals->v_forward * drone_forwmove + gpGlobals->v_right * drone_sidemove + gpGlobals->v_up * drone_upmove) * drone_Speed );
					// calculate difference between directions, this will affect the velocity
					// I don't know math so it will have to do :(
					drone_DirChange = DotProduct( drone_currentdir, m_hDrone->GetAbsVelocity().Normalize() );
					if( drone_DirChange <= 0.4f )
						drone_DirChange = 0.4f;

					drone_Speed *= drone_DirChange;

					// shooting will be also updated here
					if( pev->button & IN_ATTACK )
					{
						if( m_hDrone->m_iCounter > 0 )
						{
							Vector vecShootOrigin = m_hDrone->GetAbsOrigin();
							vecShootOrigin.z += 8.0f; // 8 not 16, so it would be a bit below the center

							m_hDrone->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_3DEGREES, 4096, BULLET_MONSTER_MP5, 1, DMG_WPN_DRONE, VARS( m_hDrone->pev->owner ) );
							m_hDrone->m_iCounter--;
							CSoundEnt::InsertSound( bits_SOUND_COMBAT, m_hDrone->pev->origin, 384, 0.3 );

							switch( RANDOM_LONG( 0, 2 ) )
							{
							case 0: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "drone/drone_shoot1.wav", 1, ATTN_NORM ); break;
							case 1: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "drone/drone_shoot2.wav", 1, ATTN_NORM ); break;
							case 2: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "drone/drone_shoot3.wav", 1, ATTN_NORM ); break;
							}

							m_hDrone->pev->effects |= EF_MUZZLEFLASH;

							MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecShootOrigin );
								WRITE_BYTE( TE_DLIGHT );
								WRITE_COORD( vecShootOrigin.x );		// origin
								WRITE_COORD( vecShootOrigin.y );
								WRITE_COORD( vecShootOrigin.z );
								WRITE_BYTE( 15 );	// radius
								WRITE_BYTE( 255 );	// R
								WRITE_BYTE( 255 );	// G
								WRITE_BYTE( 180 );	// B
								WRITE_BYTE( 0 );	// life * 10
								WRITE_BYTE( 0 ); // decay
								WRITE_BYTE( 125 ); // brightness
								WRITE_BYTE( 0 ); // shadows
							MESSAGE_END();

							if( LoudWeaponsRestricted )
								FireLoudWeaponRestrictionEntity();
						}
						else
							EMIT_SOUND( m_hDrone->edict(), CHAN_WEAPON, "drone/drone_emptyclip.wav", 1, ATTN_NORM );

						// UNDONE brass shell?
					}

					drone_UpdateTime = gpGlobals->time;
				}

				// update velocity and direction
				m_hDrone->SetAbsVelocity( m_hDrone->GetAbsVelocity().Normalize() * drone_Speed * drone_DirChange );
			}
			else // player has left 1st person mode
			{
				// make sure that we were looking through the drone camera
				// (maybe some other camera took our view)
				if( !Q_strcmp( CameraEntity, "_friendlydrone" ) )
				{
					SET_VIEW( edict(), edict() );
					CameraEntity[0] = '\0';
					UTIL_ScreenFade( this, g_vecZero, 0.5, 0, 255, FFADE_IN );
				}

				if( pev->effects & EF_PLAYERDRONECONTROL )
				{
					// reset player's X angle, look forward
					Vector ang = GetAbsAngles();
					ang.x = 0;
					SetAbsAngles( ang );
					pev->fixangle = TRUE;

					pev->effects &= ~EF_PLAYERDRONECONTROL;
					pev->flags &= ~FL_ONTRAIN;
					m_hDrone->pev->angles.x = 0; // reset vertical turn
					m_hDrone->SetAbsAngles( m_hDrone->GetAbsAngles() );
					m_hDrone->RemoveFlag( F_PLAYER_CONTROL );
					m_hDrone->SetAbsVelocity( g_vecZero );
					m_hDrone->pev->effects |= EF_MERGE_VISIBILITY;
					drone_Speed = 0;
					drone_forwmove = drone_sidemove = drone_upmove = 0;

					DroneCrosshairUpdate = true;

					// disable drone's flashlight if it was on
					if( m_hDrone->pev->effects & EF_DIMLIGHT )
						m_hDrone->pev->effects &= ~EF_DIMLIGHT;

					// fire target if set
					if( !FStringNull( DroneTarget_OnLeavingFirstPerson ) )
						UTIL_FireTargets( DroneTarget_OnLeavingFirstPerson, this, this, USE_TOGGLE, 0 );
				}
			}
		}
	}
	else // drone not deployed? we get this state if drone dies, disappears from the world, or grabbed by +use
	{
		if( DroneControl && !Q_strcmp( CameraEntity, "_friendlydrone" ) ) // were we in 1st person at the moment?
		{
			SET_VIEW( edict(), edict() );
			CameraEntity[0] = '\0';
			UTIL_ScreenFade( this, g_vecZero, 0.5, 0, 255, FFADE_IN );
			pev->flags &= ~FL_ONTRAIN;
			pev->effects &= ~EF_PLAYERDRONECONTROL;
			DroneCrosshairUpdate = true;

			// reset player's X angle, look forward
			Vector ang = GetAbsAngles();
			ang.x = 0;
			SetAbsAngles( ang );
			pev->fixangle = TRUE;

			drone_forwmove = drone_sidemove = drone_upmove = 0;
			DroneControl = false;
		}

		m_hDrone = NULL;
	}

	// update HUD
	if( HasWeapon( WEAPON_DRONE ) )
	{
		if( gpGlobals->time > DroneInfoUpdateTime )
		{
			if( DroneColor != DroneColor_CL 
				|| (int)DroneHealth != DroneHealth_CL
				|| (int)DroneAmmo != DroneAmmo_CL
				|| DroneDeployed != DroneDeployed_CL
				|| DroneDistance != DroneDistance_CL 
				)
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
				WRITE_BYTE( TE_DRONEPARAMS );
				WRITE_BYTE( DroneColor.x );
				WRITE_BYTE( DroneColor.y );
				WRITE_BYTE( DroneColor.z );
				WRITE_SHORT( (short)DroneHealth );
				WRITE_SHORT( (short)DroneAmmo );
				WRITE_BYTE( (int)DroneDeployed );
				WRITE_SHORT( (short)DroneDistance );
				MESSAGE_END();

				DroneColor_CL = DroneColor;
				DroneHealth_CL = DroneHealth;
				DroneAmmo_CL = DroneAmmo;
				DroneDeployed_CL = DroneDeployed;
				DroneDistance_CL = DroneDistance;
			}

			DroneInfoUpdateTime = gpGlobals->time + 0.2;
		}
	}
}

void CBasePlayer::FireLoudWeaponRestrictionEntity( void )
{
	if( !LoudWeaponsRestricted )
		return;

	CBaseEntity *pEntity = NULL;
	pEntity = UTIL_FindEntityByClassname( NULL, "loudness_restriction_relay" );
	if( pEntity )
		pEntity->Use( this, this, USE_TOGGLE, 0 );
}

//=====================================================================================
// ManageCar: drive the car!
//=====================================================================================
void CBasePlayer::ManageCar( void )
{
	if( !pCar )
	{
		if( InCar )
		{
		//	ALERT( at_aiconsole, "ManageCar: Exit car!\n" );
			InCar = false;
			HideWeapons( FALSE );
			pev->effects &= ~EF_NODRAW;
			pev->solid = SOLID_SLIDEBOX;
		}

		return;
	}

	if( !InCar )
	{
		if( pCar )
		{
			InCar = true;
			HideWeapons( TRUE );
			pev->effects |= EF_NODRAW;
			m_hCachedUseObject = NULL;
			pev->solid = SOLID_NOT;
		}
	}
	else
	{
		// if I don't make myself solid, I can collide with the car collision (solid env_model)
		// and it will also make the car stuck when moving on an elevator.
		// so I made myself non-solid...
		// BUT - monsters now can't see me...so this is a workaround :|
		if( pev->solid == SOLID_NOT )
			UTIL_SetOrigin( this, pev->origin );
	}

	if( FlashlightIsOn() )
		FlashlightTurnOff();
}

//=====================================================================================
// ManageElectroBlast: everything related to electro-blast
//=====================================================================================
void CBasePlayer::ManageElectroBlast( void )
{
	// removing override must be delayed so it could transfer as a "killing weapon" to clients
	if( BlastDMGOverride && (gpGlobals->time > LastBlastTime + 1) )
		BlastDMGOverride = false;
	
	if( !HasWeapon( WEAPON_SUIT ) )
	{
		if( BlastAbilityLVL > 0 )
			BlastAbilityLVL = 0;

		goto skip_blast;
	}
	
	if( BlastAbilityLVL == 0 )
		goto skip_blast;

//	ALERT( at_notice, "LVL %i, ammo %i\n", BlastAbilityLVL, BlastChargesReady );

	if( BlastAbilityLVL == -1 ) // first time upgrading?
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIconTutor, NULL, pev );
			WRITE_STRING( "tutor_electroblast" );
			WRITE_STRING( "textures/!tutor/tutor_electroblast.dds" );
		MESSAGE_END();
		LastBlastTime = gpGlobals->time - 27;
		BlastChargesReady = 0;
		BlastAbilityLVL = 1;
	}

	// here we regenerate the charges
	if( BlastAbilityLVL == 1 )
	{
		if( (gpGlobals->time > LastBlastTime + 30) && (BlastChargesReady == 0) )
		{
			BlastChargesReady++;
			LastBlastTime = gpGlobals->time - 2;
		}
	}
	else if( BlastAbilityLVL == 2 )
	{
		if( (gpGlobals->time > LastBlastTime + 30) && (BlastChargesReady < 2) )
		{
			BlastChargesReady++;
			LastBlastTime = gpGlobals->time - 2;
		}
	}
	else if( BlastAbilityLVL == 3 )
	{
		if( (gpGlobals->time > LastBlastTime + 30) && (BlastChargesReady < 3) )
		{
			BlastChargesReady++;
			LastBlastTime = gpGlobals->time - 2;
		}
	}

	// can't blast in car
	if( pCar )
		goto skip_blast;

	if( ElectroblastButton )
	{
		if( (gpGlobals->time > LastBlastTime + 3) && (BlastChargesReady > 0) )
		{
			LastBlastTime = gpGlobals->time;

			if( !CanElectroBlast ) // disabled by script
			{
				UTIL_ShowMessage( "UTIL_NOBLAST", this );
				goto skip_blast;
			}

			// do blast
			UTIL_ScreenShakeLocal( this, GetAbsOrigin(), 10.0, 150.0, 1.0, 400, true );

			Vector vecOrigin = GetAbsOrigin();
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecOrigin );
				WRITE_BYTE( TE_BEAMCYLINDER );
				WRITE_COORD( vecOrigin.x );
				WRITE_COORD( vecOrigin.y );
				WRITE_COORD( vecOrigin.z + 4 );
				WRITE_COORD( vecOrigin.x );
				WRITE_COORD( vecOrigin.y );
				WRITE_COORD( vecOrigin.z + 666 ); // reach damage radius over .4 seconds
				WRITE_SHORT( g_sBlastTexture );
				WRITE_BYTE( 0 ); // startframe
				WRITE_BYTE( 0 ); // framerate
				WRITE_BYTE( 4 ); // life
				WRITE_BYTE( 64 );  // width
				WRITE_BYTE( 32 );   // noise
				WRITE_BYTE( 200 ); // R
				WRITE_BYTE( 0 ); // G
				WRITE_BYTE( 45 ); // B
				WRITE_BYTE( 125 ); //brightness
				WRITE_BYTE( 0 ); // speed
			MESSAGE_END();

			BlastDMGOverride = true; // don't hit yourself with your own blast

			float flDmg = 150.0f;
			if( m_flStaminaValue >= 50.0f )
			{
				m_flStaminaValue -= 50.0f;
			}
			else
			{
				flDmg *= m_flStaminaValue / 50.0f;
				m_flStaminaValue = 0.0f;
			}

			RadiusDamage( pev, pev, flDmg, 666.0f, CLASS_PLAYER_ALLY, DMG_SHOCK );

			EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "player/electroblast.wav", 1.0, 0.3, 0, RANDOM_LONG( 95, 105 ) );

			BlastChargesReady--;

			m_flStaminaWait = gpGlobals->time + 5.0f;

			MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, pev->origin );
				WRITE_BYTE( TE_DLIGHT );
				WRITE_COORD( pev->origin.x );	// X
				WRITE_COORD( pev->origin.y );	// Y
				WRITE_COORD( pev->origin.z );	// Z
				WRITE_BYTE( 35 );		// radius * 0.1
				WRITE_BYTE( 200 );		// r
				WRITE_BYTE( 0 );		// g
				WRITE_BYTE( 45 );		// b
				WRITE_BYTE( 15 );		// time * 10
				WRITE_BYTE( 25 );		// decay * 0.1
				WRITE_BYTE( 125 ); // brightness
				WRITE_BYTE( 0 ); // shadows
			MESSAGE_END();
		}
		else
			UTIL_ShowMessage( "UTIL_BLASTNOTREADY", this );
	}

	skip_blast:
	ElectroblastButton = false;
}

/* Time based Damage works as follows: 
	1) There are several types of timebased damage:

		#define DMG_PARALYZE		(1 << 14)	// slows affected creature down
		#define DMG_NERVEGAS		(1 << 15)	// nerve toxins, very bad
		#define DMG_POISON			(1 << 16)	// blood poisioning
		#define DMG_RADIATION		(1 << 17)	// radiation exposure
		#define DMG_DROWNRECOVER	(1 << 18)	// drown recovery
		#define DMG_ACID			(1 << 19)	// toxic chemicals or acid burns
		#define DMG_SLOWBURN		(1 << 20)	// in an oven
		#define DMG_SLOWFREEZE		(1 << 21)	// in a subzero freezer

	2) A new hit inflicting tbd restarts the tbd counter - each monster has an 8bit counter,
		per damage type. The counter is decremented every second, so the maximum time 
		an effect will last is 255/60 = 4.25 minutes.  Of course, staying within the radius
		of a damaging effect like fire, nervegas, radiation will continually reset the counter to max.

	3) Every second that a tbd counter is running, the player takes damage.  The damage
		is determined by the type of tdb.  
			Paralyze		- 1/2 movement rate, 30 second duration.
			Nervegas		- 5 points per second, 16 second duration = 80 points max dose.
			Poison			- 2 points per second, 25 second duration = 50 points max dose.
			Radiation		- 1 point per second, 50 second duration = 50 points max dose.
			Drown			- 5 points per second, 2 second duration.
			Acid/Chemical	- 5 points per second, 10 second duration = 50 points max.
			Burn			- 10 points per second, 2 second duration.
			Freeze			- 3 points per second, 10 second duration = 30 points max.

	4) Certain actions or countermeasures counteract the damaging effects of tbds:

		Armor/Heater/Cooler - Chemical(acid),burn, freeze all do damage to armor power, then to body
							- recharged by suit recharger
		Air In Lungs		- drowning damage is done to air in lungs first, then to body
							- recharged by poking head out of water
							- 10 seconds if swiming fast
		Air In SCUBA		- drowning damage is done to air in tanks first, then to body
							- 2 minutes in tanks. Need new tank once empty.
		Radiation Syringe	- Each syringe full provides protection vs one radiation dosage
		Antitoxin Syringe	- Each syringe full provides protection vs one poisoning (nervegas or poison).
		Health kit			- Immediate stop to acid/chemical, fire or freeze damage.
		Radiation Shower	- Immediate stop to radiation damage, acid/chemical or fire damage.
		
	
*/

// If player is taking time based damage, continue doing damage to player -
// this simulates the effect of being poisoned, gassed, dosed with radiation etc -
// anything that continues to do damage even after the initial contact stops.
// Update all time based damage counters, and shut off any that are done.

// The m_bitsDamageType bit MUST be set if any damage is to be taken.
// This routine will detect the initial on value of the m_bitsDamageType
// and init the appropriate counter.  Only processes damage every second.

//#define PARALYZE_DURATION	30		// number of 2 second intervals to take damage
//#define PARALYZE_DAMAGE		0.0		// damage to take each 2 second interval

//#define NERVEGAS_DURATION	16
//#define NERVEGAS_DAMAGE		5.0

//#define POISON_DURATION		25
//#define POISON_DAMAGE		2.0

//#define RADIATION_DURATION	50
//#define RADIATION_DAMAGE	1.0

//#define ACID_DURATION		10
//#define ACID_DAMAGE			5.0

//#define SLOWBURN_DURATION	2
//#define SLOWBURN_DAMAGE		1.0

//#define SLOWFREEZE_DURATION	1.0
//#define SLOWFREEZE_DAMAGE	3.0

/* */


void CBasePlayer::CheckTimeBasedDamage() 
{
	int i;
	BYTE bDuration = 0;

	// diffusion - moved to player's class
//	static float gtbdPrev = 0.0;

	if (!(m_bitsDamageType & DMG_TIMEBASED))
		return;

	// only check for time based damage approx. every 2 seconds
	if (fabs(gpGlobals->time - m_tbdPrev) < 2.0)
		return;
	
	m_tbdPrev = gpGlobals->time;

	for (i = 0; i < CDMG_TIMEBASED; i++)
	{
		// make sure bit is set for damage type
		if (m_bitsDamageType & (DMG_PARALYZE << i))
		{
			switch (i)
			{
			case itbd_Paralyze:
				// UNDONE - flag movement as half-speed
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
//				TakeDamage(pev, pev, NERVEGAS_DAMAGE, DMG_GENERIC);	
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_Poison:
				TakeDamage(pev, pev, POISON_DAMAGE, DMG_GENERIC);
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
//				TakeDamage(pev, pev, RADIATION_DAMAGE, DMG_GENERIC);
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// NOTE: this hack is actually used to RESTORE health
				// after the player has been drowning and finally takes a breath
				if (m_idrowndmg > m_idrownrestored)
				{
					int idif = Q_min(m_idrowndmg - m_idrownrestored, 10);

					TakeHealth(idif, DMG_GENERIC);
					m_idrownrestored += idif;
				}
				bDuration = 4;	// get up to 5*10 = 50 points back
				break;
			case itbd_Acid:
//				TakeDamage(pev, pev, ACID_DAMAGE, DMG_GENERIC);
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
//				TakeDamage(pev, pev, SLOWBURN_DAMAGE, DMG_GENERIC);
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
//				TakeDamage(pev, pev, SLOWFREEZE_DAMAGE, DMG_GENERIC);
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if (m_rgbTimeBasedDamage[i])
			{
				// use up an antitoxin on poison or nervegas after a few seconds of damage					
				if (((i == itbd_NerveGas) && (m_rgbTimeBasedDamage[i] < NERVEGAS_DURATION)) ||
					((i == itbd_Poison)   && (m_rgbTimeBasedDamage[i] < POISON_DURATION)))
				{
					if (m_rgItems[ITEM_ANTIDOTE])
					{
						m_rgbTimeBasedDamage[i] = 0;
						m_rgItems[ITEM_ANTIDOTE]--;
						SetSuitUpdate("!HEV_HEAL4", FALSE, SUIT_REPEAT_OK);
					}
				}


				// decrement damage duration, detect when done.
				if (!m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0)
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~(DMG_PARALYZE << i);	
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

/*
THE POWER SUIT

The Suit provides 3 main functions: Protection, Notification and Augmentation. 
Some functions are automatic, some require power. 
The player gets the suit shortly after getting off the train in C1A0 and it stays
with him for the entire game.

Protection

	Heat/Cold
		When the player enters a hot/cold area, the heating/cooling indicator on the suit 
		will come on and the battery will drain while the player stays in the area. 
		After the battery is dead, the player starts to take damage. 
		This feature is built into the suit and is automatically engaged.
	Radiation Syringe
		This will cause the player to be immune from the effects of radiation for N seconds. Single use item.
	Anti-Toxin Syringe
		This will cure the player from being poisoned. Single use item.
	Health
		Small (1st aid kits, food, etc.)
		Large (boxes on walls)
	Armor
		The armor works using energy to create a protective field that deflects a
		percentage of damage projectile and explosive attacks. After the armor has been deployed,
		it will attempt to recharge itself to full capacity with the energy reserves from the battery.
		It takes the armor N seconds to fully charge. 

Notification (via the HUD)

x	Health
x	Ammo  
x	Automatic Health Care
		Notifies the player when automatic healing has been engaged. 
x	Geiger counter
		Classic Geiger counter sound and status bar at top of HUD 
		alerts player to dangerous levels of radiation. This is not visible when radiation levels are normal.
x	Poison
	Armor
		Displays the current level of armor. 

Augmentation 

	Reanimation (w/adrenaline)
		Causes the player to come back to life after he has been dead for 3 seconds. 
		Will not work if player was gibbed. Single use.
	Long Jump
		Used by hitting the ??? key(s). Caused the player to further than normal.
	SCUBA	
		Used automatically after picked up and after player enters the water. 
		Works for N seconds. Single use.	
	
Things powered by the battery

	Armor		
		Uses N watts for every M units of damage.
	Heat/Cool	
		Uses N watts for every second in hot/cold area.
	Long Jump	
		Uses N watts for every jump.
	Alien Cloak	
		Uses N watts for each use. Each use lasts M seconds.
	Alien Shield	
		Augments armor. Reduces Armor drain by one half
 
*/

// if in range of radiation source, ping geiger counter

#define GEIGERDELAY 0.25

void CBasePlayer :: UpdateGeigerCounter( void )
{
	BYTE range;

	// delay per update ie: don't flood net with these msgs
	if (gpGlobals->time < m_flgeigerDelay)
		return;

	m_flgeigerDelay = gpGlobals->time + GEIGERDELAY;
		
	// send range to radition source to client

	range = (BYTE) (m_flgeigerRange / 4);

	if (range != m_igeigerRangePrev)
	{
		m_igeigerRangePrev = range;

		MESSAGE_BEGIN( MSG_ONE, gmsgGeigerRange, NULL, pev );
			WRITE_BYTE( range );
		MESSAGE_END();
	}

	// reset counter and semaphore
	if (!RANDOM_LONG(0,3))
		m_flgeigerRange = 1000;

}

/*
================
CheckSuitUpdate

Play suit update if it's time
================
*/

#define SUITUPDATETIME	3.5
#define SUITFIRSTUPDATETIME 0.1

void CBasePlayer::CheckSuitUpdate()
{
	int i;
	int isentence = 0;
	int isearch = m_iSuitPlayNext;
	
	// Ignore suit updates if no suit
	if ( !HasWeapon(WEAPON_SUIT) )
		return;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();

	if ( (g_pGameRules->IsMultiplayer()) || (CVAR_GET_FLOAT("suitvolume") == 0) )
	{
		// don't bother updating HEV voice in multiplayer.
		return;
	}

	if ( gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0)
	{
		// play a sentence off of the end of the queue
		for (i = 0; i < CSUITPLAYLIST; i++)
			{
			if (isentence = m_rgSuitPlayList[isearch])
				break;
			
			if (++isearch == CSUITPLAYLIST)
				isearch = 0;
			}

		if (isentence)
		{
			m_rgSuitPlayList[isearch] = 0;
			if (isentence > 0)
			{
				// play sentence number

				char sentence[CBSENTENCENAME_MAX+1];
				strcpy(sentence, "!");
				strcat(sentence, gszallsentencenames[isentence]);
				EMIT_SOUND_SUIT(ENT(pev), sentence);
			}
			else
			{
				// play sentence group
				EMIT_GROUPID_SUIT(ENT(pev), -isentence);
			}
		m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
		}
		else
			// queue is empty, don't check 
			m_flSuitUpdate = 0;
	}
}
 
// add sentence to suit playlist queue. if fgroup is true, then
// name is a sentence group (HEV_AA), otherwise name is a specific
// sentence name ie: !HEV_AA0.  If iNoRepeat is specified in
// seconds, then we won't repeat playback of this word or sentence
// for at least that number of seconds.

void CBasePlayer::SetSuitUpdate(const char *name, int fgroup, int iNoRepeatTime)
{
	int i;
	int isentence;
	int iempty = -1;
	
	// Ignore suit updates if no suit
	if ( !HasWeapon( WEAPON_SUIT ))
		return;

	if ( (g_pGameRules->IsMultiplayer()) || (CVAR_GET_FLOAT("suitvolume") == 0) )
	{
		// due to static channel design, etc. We don't play HEV sounds in multiplayer right now.
		return;
	}

	// if name == NULL, then clear out the queue

	if (!name)
	{
		for (i = 0; i < CSUITPLAYLIST; i++)
			m_rgSuitPlayList[i] = 0;
		return;
	}
	// get sentence or group number
	if (!fgroup)
	{
		isentence = SENTENCEG_Lookup(name, NULL);
		if (isentence < 0)
			return;
	}
	else
		// mark group number as negative
		isentence = -SENTENCEG_GetIndex(name);

	// check norepeat list - this list lets us cancel
	// the playback of words or sentences that have already
	// been played within a certain time.

	for (i = 0; i < CSUITNOREPEAT; i++)
	{
		if (isentence == m_rgiSuitNoRepeat[i])
			{
			// this sentence or group is already in 
			// the norepeat list

			if (m_rgflSuitNoRepeatTime[i] < gpGlobals->time)
				{
				// norepeat time has expired, clear it out
				m_rgiSuitNoRepeat[i] = 0;
				m_rgflSuitNoRepeatTime[i] = 0.0;
				iempty = i;
				break;
				}
			else
				{
				// don't play, still marked as norepeat
				return;
				}
			}
		// keep track of empty slot
		if (!m_rgiSuitNoRepeat[i])
			iempty = i;
	}

	// sentence is not in norepeat list, save if norepeat time was given

	if (iNoRepeatTime)
	{
		if (iempty < 0)
			iempty = RANDOM_LONG(0, CSUITNOREPEAT-1); // pick random slot to take over
		m_rgiSuitNoRepeat[iempty] = isentence;
		m_rgflSuitNoRepeatTime[iempty] = iNoRepeatTime + gpGlobals->time;
	}

	// find empty spot in queue, or overwrite last spot
	
	m_rgSuitPlayList[m_iSuitPlayNext++] = isentence;
	if (m_iSuitPlayNext == CSUITPLAYLIST)
		m_iSuitPlayNext = 0;

	if (m_flSuitUpdate <= gpGlobals->time)
	{
		if (m_flSuitUpdate == 0)
			// play queue is empty, don't delay too long before playback
			m_flSuitUpdate = gpGlobals->time + SUITFIRSTUPDATETIME;
		else 
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME; 
	}

}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
static void CheckPowerups(entvars_t *pev)
{
	if (pev->health <= 0)
		return;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes
}


//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
void CBasePlayer :: UpdatePlayerSound ( void )
{
	int iBodyVolume;
	int iVolume;
	CSound *pSound;

	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt :: ClientSoundIndex( edict() ) );

	if ( !pSound )
	{
		ALERT ( at_console, "Client lost reserved sound!\n" );
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.
	
	if ( FBitSet ( pev->flags, FL_ONGROUND ) )
	{	
		iBodyVolume = GetAbsVelocity().Length(); 

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast. 
		if ( iBodyVolume > 512 )
			iBodyVolume = 512;
	}
	else
		iBodyVolume = 0;

	if ( pev->button & IN_JUMP )
		iBodyVolume += 100;

	// convert player move speed and actions into sound audible by monsters.
	if ( m_iWeaponVolume > iBodyVolume )
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player. 
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if ( m_iWeaponVolume < 0 )
	{
		m_iWeaponVolume = 0;
	}


	// if target volume is greater than the player sound's current volume, we paste the new volume in 
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if ( m_iTargetVolume > iVolume )
	{
		iVolume = m_iTargetVolume;
	}
	else if ( iVolume > m_iTargetVolume )
	{
		iVolume -= 250 * gpGlobals->frametime;

		if ( iVolume < m_iTargetVolume )
		{
			iVolume = 0;
		}
	}

	if ( m_fNoPlayerSound )
	{
		// debugging flag, lets players move around and shoot without monsters hearing.
		iVolume = 0;
	}

	if ( gpGlobals->time > m_flStopExtraSoundTime )
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two 
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if ( pSound )
	{
		pSound->m_vecOrigin = GetAbsOrigin();
		pSound->m_iType |= ( bits_SOUND_PLAYER | m_iExtraSoundTypes );
		pSound->m_iVolume = iVolume;
	}

	// keep track of virtual muzzle flash
	m_iWeaponFlash -= 256 * gpGlobals->frametime;
	if (m_iWeaponFlash < 0)
		m_iWeaponFlash = 0;
}

//===============================================================================
// LookAtPlayers: look at other players just like in HL2DM (some code is from there)
//===============================================================================
void CBasePlayer::LookAtPlayers( void )
{
	if( !g_pGameRules->IsMultiplayer() )
		return;

	if( IsObserver() )
		return;
	
	bool bFoundViewTarget = false;

	// save the vector, to prevent too many computation calls
	Vector PlayerOrg = GetAbsOrigin();

	Vector vForward;
	g_engfuncs.pfnAngleVectors( GetAbsAngles(), vForward, NULL, NULL );

	for( int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( iClient );
		if( !pEnt || !pEnt->IsPlayer() )
			continue;

		if( pEnt->entindex() == entindex() )
			continue;

		if( !pEnt->IsAlive() )
			continue;

		Vector vTargetOrigin = pEnt->GetAbsOrigin();
		Vector vMyOrigin = PlayerOrg;

		Vector vDir = vTargetOrigin - vMyOrigin;

		if( vDir.Length() > 200 )
			continue;

		vDir = vDir.Normalize();

		if( DotProduct( vForward, vDir ) < 0.0f )
			continue;

		m_vLookAtTarget = pEnt->EyePosition();
		bFoundViewTarget = true;
		break;
	}

	if( !bFoundViewTarget )
		m_vLookAtTarget = PlayerOrg + vForward * 512;

	headyaw = UTIL_ApproachAngle( VecToYaw( m_vLookAtTarget - PlayerOrg ) - GetLocalAngles().y, headyaw, 130 * gpGlobals->frametime, true );

	// turn towards vector
	SetBoneController( 3, headyaw );
}

void CBasePlayer::PostThink()
{		
	/*
	static float tm = 0;
	if( pev->button & IN_USE && tm < gpGlobals->time )
	{
		CBaseEntity *pShock = CBaseEntity::Create( "shock_beam", g_vecZero, g_vecZero, 0 );
		if( pShock )
		{
			pShock->SetAbsVelocity( Vector(5,0,0) );
			pShock->SetNextThink( 0 );
			pShock->pev->dmg = 1.25f;
		}
		tm = gpGlobals->time + 3;
	}*/

	if( CameraEntity[0] == '\0' )
	{
		if( pev->effects & EF_PLAYERDRONECONTROL )
			Q_strcpy( CameraEntity, "_friendlydrone" );
		else
		{
			CameraOrigin = GetAbsOrigin();
			CameraAngles = pev->v_angle;
			if( pev->effects & EF_PLAYERUSINGCAMERA )
				pev->effects &= ~EF_PLAYERUSINGCAMERA;
		}
	}
	
	if( g_fGameOver )
	{
		// exit the car
		if( pCar )
			pCar->Use( this, this, USE_OFF, 0 );
		goto pt_end;         // intermission or finale
	}

	if( !IsAlive() || m_bInWelcomeCam )
		goto pt_end;

	// update player variables for parent system
	SetAbsAngles( pev->angles );
	SetAbsOrigin( pev->origin );
	SetAbsVelocity( pev->velocity );
	
	WorldPhysic->MoveCharacter( this );
	
	// Handle Tank controlling
	if ( m_pTank != NULL )
	{
		if ( m_pTank->OnControls( this ) && !pev->weaponmodel )
		{
		}
		else
		{	// they've moved off the platform
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
		}
	}

	if ( m_pMonitor != NULL )
	{
		if ( !m_pMonitor->OnControls( this ))
		{	
			// they've moved off the platform
			m_pMonitor->Use( this, this, USE_RESET, 0 );
			m_pMonitor = NULL;
		}
	}

	// ----------------flashlight "dot" which monsters can see-----------------------------
	if( gpGlobals->maxClients == 1 ) // UNDONE must be tested in multiplayer with monsters and other players, single-player for now
	{
		if( m_pFlashlightMonster )
		{
			if( !(m_pFlashlightMonster->pev->effects & EF_NODRAW) )
			{
				TraceResult TraceFlashlightMonster;
				UTIL_MakeVectors( pev->v_angle );
				int FlashlightRadius = 600; // same value in r_misc.cpp - Flashlight()
				UTIL_TraceLine( GetGunPosition(), GetGunPosition() + gpGlobals->v_forward * FlashlightRadius, ignore_monsters, ENT( pev ), &TraceFlashlightMonster );
				Vector newposition = TraceFlashlightMonster.vecEndPos;
				if( TraceFlashlightMonster.flFraction < 1 )
				{
					TraceResult GodDamnTrace = UTIL_GetGlobalTrace();
					newposition = TraceFlashlightMonster.vecEndPos + GodDamnTrace.vecPlaneNormal * 5; // pull out from the touched surface
				}

				UTIL_SetOrigin( m_pFlashlightMonster, newposition );
			}
		}
	}
	// ------------------------------------------------------------------------------------------------

	if( m_hDrone == NULL )
	{
		// regenerate drone hp and ammo when it's not active
		if( DroneHealth < DRONE_MAX_HEALTH )
			DroneHealth += 5 * gpGlobals->frametime;
		// ammo regenerating speed depends on health
		if( DroneAmmo < DRONE_MAX_AMMO )
			DroneAmmo += DroneHealth * 0.1f * gpGlobals->frametime;
		DroneHealth = bound( 0, DroneHealth, DRONE_MAX_HEALTH );
		DroneAmmo = bound( 1, DroneAmmo, DRONE_MAX_AMMO );
	//	ALERT( at_console, "hp %f ammo %f\n", DroneHealth, DroneAmmo );
	}

	LookAtPlayers();

	// do weapon stuff
	ItemPostFrame( );
	
	// diffusion - check for a completed vehicle achievement
	CheckVehicleAchievement();

//	PunchAngle(); // moved to playermove, but it has bugs... different clients have different decay speeds. and here it's laggy cause server :(

	// diffusion - a button has indicated to freeze player for 2 seconds after press.
	// TODO make something better, more customizeable
	if( HasFlag( F_PLAYER_BUTTONFREEZED ) )
	{
		if( CanMove )
		{
			ButtonFreezeTime = gpGlobals->time + 2;
			CanMove = false;
		}

		// hackhack? :) don't want to interfere with FL_FROZEN, could cause buggy situations.
		if( ButtonFreezeTime < gpGlobals->time )
		{
			RemoveFlag( F_PLAYER_BUTTONFREEZED );
			CanMove = true;
		}
	}

// check to see if player landed hard enough to make a sound
// falling farther than half of the maximum safe distance, but not as far a max safe distance will
// play a bootscrape sound, and no damage will be inflicted. Fallling a distance shorter than half
// of maximum safe distance will make no sound. Falling farther than max safe distance will play a 
// fallpain sound, and damage will be inflicted based on how far the player fell

	if ( (FBitSet(pev->flags, FL_ONGROUND)) && (pev->health > 0) && m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD )
	{
		// ALERT ( at_console, "%f\n", m_flFallVelocity );

		if (pev->watertype == CONTENT_WATER)
		{
			// Did he hit the world or a non-moving entity?
			// BUG - this happens all the time in water, especially when 
			// BUG - water has current force
			// if ( !pev->groundentity || VARS(pev->groundentity)->velocity.z == 0 )
				// EMIT_SOUND(ENT(pev), CHAN_BODY, "footsteps/wade1.wav", 1, ATTN_NORM);
		}
		else if ( m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED )
		{// after this point, we start doing damage
			
			float flFallDamage = g_pGameRules->FlPlayerFallDamage( this );

			if ( !IsAlive() && flFallDamage > pev->health )
			{//splat
				// note: play on item channel because we play footstep landing on body channel
				switch( RANDOM_LONG(0,2) )
				{
					case 0: EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat1.wav", 1, ATTN_NORM); break;
					case 1: EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat2.wav", 1, ATTN_NORM); break;
					case 2: EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat3.wav", 1, ATTN_NORM); break;
				}
			}

			if ( flFallDamage > 0 )
				TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), flFallDamage, DMG_FALL ); 
		}

		if ( IsAlive() )
			SetAnimation( PLAYER_WALK );
	}
	
	if (FBitSet(pev->flags, FL_ONGROUND))
	{		
		if (m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer())
		{
			CSoundEnt::InsertSound ( bits_SOUND_PLAYER, GetAbsOrigin(), m_flFallVelocity, 0.2 );
			// ALERT( at_console, "fall %f\n", m_flFallVelocity );
		}
		m_flFallVelocity = 0;
	}

	// select the proper animation for the player character	
	if ( IsAlive() )
	{
		if (!GetAbsVelocity().x && !GetAbsVelocity().y)
			SetAnimation( PLAYER_IDLE );
		else if ((GetAbsVelocity().x || GetAbsVelocity().y) && (FBitSet(pev->flags, FL_ONGROUND)))
			SetAnimation( PLAYER_WALK );
		else if (pev->waterlevel > 1)
			SetAnimation( PLAYER_WALK );
	}
	
	if( pev->effects & EF_UPSIDEDOWN )
	{
		if( !IsUpsideDown )
		{
			pev->flags |= FL_DUCKING;
			IsUpsideDown = true;
		}
	}
	else
	{
		if( IsUpsideDown )
		{
			pev->flags |= FL_DUCKING;
			IsUpsideDown = false;
		}
	}

	// calc gait animation
	StudioGaitFrameAdvance();

	// calc player animation
	StudioFrameAdvance();
	
	CheckPowerups(pev);
	
	UpdatePlayerSound();

	ManageStamina();

	Dash();
	
	RegenerateHealth();
	
	// something bad happened
	if( HasFlag( F_PLAYER_HASITEM ) && (m_pHoldableItem == NULL) )
	{
		m_iHideHUD &= ~HIDEHUD_WPNS_HOLDABLEITEM;
		HideWeapons( FALSE );
		RemoveFlag( F_PLAYER_HASITEM );
	}

	if( g_pGameRules->IsMultiplayer() && IsSpawnProtected )
	{
		if( gpGlobals->time > HUDtextTime + HUD_TEXT_DELAY )
		{
			UTIL_ShowMessage( "UTIL_SPAWNPROTECT", this );
			HUDtextTime = gpGlobals->time;
		}

		if( gpGlobals->time > SpawnProtectTime )
			IsSpawnProtected = false;
	}
	
pt_end:
	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = pev->button;

	// Don't allow dead model to rotate until DeathCam or Spawn happen (CopyToBodyQue)
	if( pev->deadflag == DEAD_NO || (m_afPhysicsFlags & PFLAG_OBSERVER) || pev->fixangle )
		m_vecLastViewAngles = pev->angles;
	else
		pev->angles = m_vecLastViewAngles;

	KillingSpreeSounds();
	ConsecutiveKillSounds();
}

void CBasePlayer::KillingSpreeSounds( void )
{
	/*
	multiplayer
	count the consecutive kills
	5 kills - killing spree: level 1
	10 kills - dominating: level 2
	15 kills - rampage: level 3
	20 kills - godlike: level 4
	25 kills - wicked sick: level 5
	*/

	if( !(g_pGameRules->IsMultiplayer()) )
		return;
	
	if( gpGlobals->time < SpreeCheckTime )
		return;

	SpreeCheckTime = gpGlobals->time + 0.5;
	
	// killing spree sounds
	int SpreeSoundNum = 0;
	if( ConsecutiveKills >= 5 )
	{
		if( KillingSpreeLevel < 1 )
		{
			SpreeSoundNum = SPREE_SND_KILLINGSPREE;
			KillingSpreeLevel = 1; // killing spree!
			ConsecutiveKills = 0;
		}
		else if( KillingSpreeLevel < 2 )
		{
			SpreeSoundNum = SPREE_SND_DOMINATING;
			KillingSpreeLevel = 2; // dominating!
			ConsecutiveKills = 0;
		}
		else if( KillingSpreeLevel < 3 )
		{
			SpreeSoundNum = SPREE_SND_RAMPAGE;
			KillingSpreeLevel = 3; // rampage!
			ConsecutiveKills = 0;
		}
		else if( KillingSpreeLevel < 4 )
		{
			SpreeSoundNum = SPREE_SND_GODLIKE;
			KillingSpreeLevel = 4; // godlike!
			ConsecutiveKills = 0;
		}
		else if( KillingSpreeLevel < 5 )
		{
			SpreeSoundNum = SPREE_SND_WICKEDSICK;
			KillingSpreeLevel = 5; // wicked sick!
			ConsecutiveKills = 0;
		}
	}

	if( SpreeSoundNum == 0 )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgTempEnt );
		WRITE_BYTE( TE_UNREALSOUND );
		WRITE_BYTE( SpreeSoundNum );
	MESSAGE_END();

	if( pev->netname )
	{
		switch( SpreeSoundNum )
		{
		case SPREE_SND_KILLINGSPREE: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* %s is on killing spree!\n", STRING(pev->netname)) ); break;
		case SPREE_SND_DOMINATING: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* %s is dominating!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_RAMPAGE: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* %s is on rampage!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_GODLIKE: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* %s is godlike!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_WICKEDSICK: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* %s is wicked sick!\n", STRING( pev->netname ) ) ); break;
		}
	}
}

void CBasePlayer::ConsecutiveKillSounds( void )
{
	/*
	SPREE_SND_DOUBLEKILL,	8
	SPREE_SND_TRIPLEKILL,	9
	SPREE_SND_MULTIKILL,	10
	SPREE_SND_MEGAKILL,		11
	SPREE_SND_ULTRAKILL,	12
	SPREE_SND_MONSTERKILL,	13
	*/

	if( !(g_pGameRules->IsMultiplayer()) )
		return;

	// consecutive killing spree expired
	if( gpGlobals->time > LastConsecutiveKillTime + 5 )
	{
		ConsecutiveKillLevel = 0;
		PrevConsecutiveKillLevel = 0;
	}
	
	int SoundNum = 0;

	if( ConsecutiveKillLevel != PrevConsecutiveKillLevel )
	{
		switch( ConsecutiveKillLevel )
		{
		case 1: SoundNum = SPREE_SND_DOUBLEKILL; break;
		case 2: SoundNum = SPREE_SND_TRIPLEKILL; break;
		case 3: SoundNum = SPREE_SND_MULTIKILL; break;
		case 4: SoundNum = SPREE_SND_MEGAKILL; break;
		case 5: SoundNum = SPREE_SND_ULTRAKILL; break;
		}

		if( ConsecutiveKillLevel > 5 )
			SoundNum = SPREE_SND_MONSTERKILL;
		
		PrevConsecutiveKillLevel = ConsecutiveKillLevel;
	}

	if( SoundNum == 0 )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgTempEnt );
		WRITE_BYTE( TE_UNREALSOUND );
		WRITE_BYTE( SoundNum );
	MESSAGE_END();

	if( pev->netname )
	{
		switch( SoundNum )
		{
		case SPREE_SND_DOUBLEKILL: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* Doublekill by %s!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_TRIPLEKILL: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* Triplekill by %s!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_MULTIKILL: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* Multikill by %s!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_MEGAKILL: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* Megakill by %s!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_ULTRAKILL: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* Ultrakill by %s!\n", STRING( pev->netname ) ) ); break;
		case SPREE_SND_MONSTERKILL: UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* Monsterkill by %s!\n", STRING( pev->netname ) ) ); break;
		}
	}
}

//=================================================================
// ManageStamina: this manages everything about player's stamina.
// Stamina code by Ku2zoff. My modifications marked with "+"
//=================================================================
void CBasePlayer::ManageStamina(void)
{
	// +: "on ground" - stamina not draining while in air, or while ducking and pressing "sprint"
	if( (pev->button & IN_RUN) && (pev->button & IN_FORWARD) && (m_flStaminaValue > 0) && (pev->flags & FL_ONGROUND) && !(pev->flags & FL_DUCKING) && !DroneControl )
	{
		m_flStaminaValue -= 17.5f * gpGlobals->frametime; // scale by frametime, so fps won't affect the speed
	}
	else if( !ShieldOn && m_flStaminaValue < 100 ) // RECHARGE!
	{
		float RechargeBonus = 1.0f;
		if( pev->health > 100 ) // special mode? faster recharge
			RechargeBonus = pev->health * 0.01f;

		if( gpGlobals->time > m_flStaminaWait )
		{
			if( pev->velocity.Length2D() < 75 ) // +: regen is faster when player moves very slow or standing still
				m_flStaminaValue += RechargeBonus * 30 * gpGlobals->frametime;
			else if( pev->velocity.Length2D() < 290 ) // +: regen is slow when walking
				m_flStaminaValue += RechargeBonus * 17 * gpGlobals->frametime;
		}
		else if( gpGlobals->time > m_flStaminaWait - 1.5 )
			m_flStaminaValue += RechargeBonus * 6 * gpGlobals->frametime;
	}
	
	// +: to disable stamina drain when stationary (vel. less than 15)
	if ((pev->velocity.Length2D() < 15) && (pev->button & IN_RUN) && (pev->movetype != MOVETYPE_NOCLIP))
		CLIENT_COMMAND(ENT(pev), "-sprint\n");

	// +: no sprinting/stamina drain when strafing/going backwards
	if( (pev->button & IN_RUN) && (pev->button & IN_BACK || pev->button & IN_DUCK))
	{
		if (pev->movetype != MOVETYPE_NOCLIP)
			CLIENT_COMMAND(ENT(pev), "-sprint\n");
	}

	// +: stamina drain when jumping :)
		// MOVED TO PRE-THINK

	if ((m_flStaminaValue < 1) && (pev->button & IN_RUN))
	{
		CLIENT_COMMAND(ENT(pev), "-sprint\n");
		//ALERT(at_console, "low stamina!\n");
	}

	// +: waiting time before recharge (this was tricky lol)
	if ((m_afButtonReleased & IN_RUN) /*|| (m_afButtonPressed & IN_JUMP)*/)
	{
		// mother of god...
		if (!(pev->button & IN_MOVELEFT) || !(pev->button & IN_MOVERIGHT) || !(pev->button & IN_BACK) && (pev->flags & FL_ONGROUND) && !(pev->flags & FL_DUCKING))
		{
			/*
			if we have 90 stamina: 50/90 = 0.56 sec wait time
			60 stamina: 50/60 = 0.83 sec
			30 stamina: 50/30 = 1.66 sec
			15 stamina: 50/15 = 3.33 sec (will be limited to 3 sec)
			*/

			m_flStaminaWait = gpGlobals->time + (50 / (1 + m_flStaminaValue));

			if (m_afButtonPressed & IN_JUMP) // faster recover from jumps
				m_flStaminaWait -= 0.5;

			if( pev->health > 100 ) // special mode? less wait
				m_flStaminaWait -= 0.5;

			m_flStaminaWait = bound( 0, m_flStaminaWait, gpGlobals->time + 3 );

			//ALERT(at_console, "%.2f\n", m_flStaminaWait - gpGlobals->time);
		}
	}

	m_flStaminaValue = bound( 0.0f, m_flStaminaValue, 100.0f );
}

//=================================================================
// RegenerateHealth: add health to the wounded player automatically
//=================================================================
void CBasePlayer::RegenerateHealth(void)
{
	if ( CanRegenerateHealth && HasWeapon(WEAPON_SUIT) && (sv_regeneration.value == 1) ) // player_customize can control CanRegenerateHealth value
	{
		// regeneration speeds depend on skill level
		RegenRate = 300 / pev->health;

		if (g_iSkillLevel == SKILL_EASY)
		{
			RegenWaitTime = 3;
			RegenRate = bound(5, RegenRate, 10);
		}
		else if (g_iSkillLevel == SKILL_MEDIUM)
		{
			RegenWaitTime = 4;
			RegenRate = bound(4, RegenRate, 8);
		}
		else if (g_iSkillLevel == SKILL_HARD)
		{
			RegenWaitTime = 5;
			RegenRate = bound(3, RegenRate, 6);
		}

		if( pev->health > 100 )
			RegenRate *= 0.5f; // bonus health regenerate slower

		if (IsAlive() && (pev->health < pev->max_health))
		{
			if (gpGlobals->time > m_fTimeLastHurt + RegenWaitTime)
			{
				// Regenerate based on rate, and scale it by the frametime
				m_fRegenRemander += RegenRate * gpGlobals->frametime;

				if (m_fRegenRemander >= 1)
				{
					TakeHealth(m_fRegenRemander, DMG_GENERIC);
					SendAchievementStatToClient( ACH_HPREGENERATE, (int)m_fRegenRemander, ACHVAL_ADD );
					m_fRegenRemander = 0;
				}
			}
			else
				return;
		}
	}
}

//=====================================================================================
// Dash: replaces "turn left" command which is not really needed anyway
// This should be done through playermove I guess, to disable lags in multiplayer...
//=====================================================================================
void CBasePlayer::Dash(void)
{
	if( Dashed )
	{
		// clamp velocity to not overshoot
		if( !(pev->effects & EF_UPSIDEDOWN) && pev->velocity.z > 500 )
		{
			pev->velocity.z *= 0.25f;
		}
		else if( pev->effects & EF_UPSIDEDOWN && pev->velocity.z < -500 )
		{
			pev->velocity.z *= 0.25f;
		}
		
		if( gpGlobals->time - LastDashTime > 0.15 )
		{
			SetAbsVelocity( DashRememberVelocity );
			pev->friction = 1.0f;
			DashRememberVelocity = g_vecZero;
			SendAchievementStatToClient( ACH_DASH, 1, ACHVAL_ADD );
			Dashed = false;
		}
		return;
	}

	if( !DashButton )
		return;
	
	// player has suit, but dash it not allowed - show the message
	if( !CanDash && ((pev->button & IN_MOVELEFT) || (pev->button & IN_MOVERIGHT) || (pev->button & IN_BACK) || (pev->button & IN_FORWARD)))
	{
		UTIL_ShowMessage( "UTIL_NODASH", this );
	}
	
	bool Dash_OnGround = false; // check if we are touching the ground
	bool Dash_IsDucking = false; // check if we are ducking

	if( pev->flags & FL_ONGROUND )
		Dash_OnGround = true;

	if( pev->flags & FL_DUCKING )
		Dash_IsDucking = true;

	// override the checks in multiplayer
	if (g_pGameRules->IsMultiplayer())
	{
		if (mp_dash_air.value > 0)
			Dash_OnGround = true; // allow in air

		Dash_IsDucking = false; // allow in ducking
	}
	else
		Dash_OnGround = true; // I decided to allow to dash in air in singleplayer now...
	
	if (
			(HasWeapon( WEAPON_SUIT ))			&&
			(CanDash)							&&
			Dash_OnGround						&&
			!Dash_IsDucking						&&
			(!Dashed)							&&
			(m_flStaminaValue >= 5.0f)			&&
			(pev->velocity.Length2D() > 100)	&&
			(gpGlobals->time > LastDashTime + 1)&&
			( (pev->button & IN_MOVELEFT) || (pev->button & IN_MOVERIGHT) || (pev->button & IN_BACK) || (pev->button & IN_FORWARD) )
		)
	{
		DashRememberVelocity = GetAbsVelocity();
		DashRememberVelocity.z *= 0.2; // don't throw player into the floor after dash, if he was falling (MP only)

		float dash_spd_mult = 10.0f;
		if( m_flStaminaValue > 30.0f )
		{
			m_flStaminaValue -= 30.0f;
		}
		else
		{
			dash_spd_mult *= m_flStaminaValue / 30.0f;
			m_flStaminaValue = 0.0f;
		}

		pev->velocity.x *= dash_spd_mult;
		pev->velocity.y *= dash_spd_mult;

		UTIL_ScreenShakeLocal( this, GetAbsOrigin(), 2.0f * dash_spd_mult * 0.1f, 1000.0, 0.5, 100, true );
		if( pev->waterlevel == 3 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_STATIC, "player/dash_underwater.wav", dash_spd_mult * 0.1f, 1.5f );
			UTIL_Bubbles( GetAbsOrigin() - Vector( 64, 64, 64 ), GetAbsOrigin() + Vector( 64, 64, 64 ), 50 );
		}
		else
			EMIT_SOUND(ENT(pev), CHAN_STATIC, "player/dash.wav", dash_spd_mult * 0.1f, 1.5f );
		LastDashTime = gpGlobals->time;
		Dashed = true;
		
		m_flStaminaWait = gpGlobals->time + (50.0f / (m_flStaminaValue * 0.75f));

		if( m_flStaminaWait > gpGlobals->time + 3 )
			m_flStaminaWait = gpGlobals->time + 3;

		//ALERT(at_console, "After-Dash Wait %.2f\n", m_flStaminaWait - gpGlobals->time);
	}

	DashButton = false;
}


// checks if the spot is clear of players
BOOL IsSpawnPointValid( CBaseEntity *pPlayer, CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	if( !pSpot->IsTriggered( pPlayer ))
		return FALSE;

	while(( ent = UTIL_FindEntityInSphere( ent, pSpot->GetAbsOrigin(), 128 )) != NULL )
	{
		// if ent is a client, don't spawn on 'em
		if( ent->IsPlayer() && ent != pPlayer )
			return FALSE;
	}

	return TRUE;
}


DLL_GLOBAL CBaseEntity	*g_pLastSpawn;

/*
============
EntSelectSpawnPoint

Returns the entity to spawn at

USES AND SETS GLOBAL g_pLastSpawn
============
*/
edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer )
{
	CBaseEntity *pSpot;
	edict_t		*player;

	player = pPlayer->edict();

// choose a info_player_deathmatch point
	if (g_pGameRules->IsCoOp())
	{
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_coop");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_start");
		if ( !FNullEnt(pSpot) ) 
			goto ReturnSpot;
	}
	else if ( g_pGameRules->IsDeathmatch() )
	{
		pSpot = g_pLastSpawn;
		// Randomize the start spot
		for ( int i = RANDOM_LONG(1,9); i > 0; i-- )
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		if ( FNullEnt( pSpot ) )  // skip over the null point
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );

		CBaseEntity *pFirstSpot = pSpot;

		do 
		{
			if ( pSpot )
			{
				// check if pSpot is valid
				if ( IsSpawnPointValid( pPlayer, pSpot ) )
				{
					if ( pSpot->GetAbsOrigin() == g_vecZero )
					{
						pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
						continue;
					}

					// if so, go to pSpot
					goto ReturnSpot;
				}
			}
			// increment pSpot
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

		// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
		if ( !FNullEnt( pSpot ) )
		{
			CBaseEntity *ent = NULL;
			while(( ent = UTIL_FindEntityInSphere( ent, pSpot->GetAbsOrigin(), 128 )) != NULL )
			{
				// if ent is a client, kill em (unless they are ourselves)
				if ( ent->IsPlayer() && !(ent->edict() == player) )
					ent->TakeDamage( VARS(INDEXENT(0)), VARS(INDEXENT(0)), 300, DMG_GENERIC );
			}
			goto ReturnSpot;
		}
	}

	// If startspot is set, (re)spawn there.
	if ( FStringNull( gpGlobals->startspot ) || ((STRING( gpGlobals->startspot ))[0] == '\0') )
	{
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}
	else
	{
		pSpot = UTIL_FindEntityByTargetname( NULL, STRING(gpGlobals->startspot) );
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT(at_error, "PutClientInServer: no info_player_start on level");
		return INDEXENT(0);
	}

	g_pLastSpawn = pSpot;
	return pSpot->edict();
}

void CBasePlayer::Spawn( void )
{
	m_bConnected = TRUE;
	
	pev->classname		= MAKE_STRING("player");
	pev->health		= 100;
	pev->armorvalue		= 0;
	pev->takedamage		= DAMAGE_AIM;
	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_WALK;
	pev->max_health		= pev->health;
	pev->flags		   &= FL_PROXY;	// keep proxy flag set by engine
	pev->flags		   |= FL_CLIENT; 
	pev->air_finished	= gpGlobals->time + 12;
	pev->dmg			= 2;				// initial water damage
	pev->effects		= 0;
	pev->deadflag		= DEAD_NO;
	pev->dmg_take		= 0;
	pev->dmg_save		= 0;
	pev->friction		= 1.0;
	pev->gravity		= 1.0;
	m_bitsHUDDamage		= -1;
	m_bitsDamageType		= 0;
	m_afPhysicsFlags		= 0;
	m_flStaminaValue = 100; // DiffusionSprint
	CanRegenerateHealth = true; // DiffusionRegen - health regeneration is enabled by default, player_customize controls this value
	m_fRegenRemander = 0; // DiffusionRegen
	m_fLongJump		= FALSE;// no longjump module. 

	pev->targetname		= MAKE_STRING( "*player" );

	m_iRainDripsPerSecond = 0;
	m_flRainWindX = 0;
	m_flRainWindY = 0;
	m_flRainRandX = 0;
	m_flRainRandY = 0;
	m_iRainIdealDripsPerSecond = 0;
	m_flRainIdealWindX = 0;
	m_flRainIdealWindY = 0;
	m_flRainIdealRandX = 0;
	m_flRainIdealRandY = 0;
	m_flRainEndFade = 0;
	m_flRainNextFadeUpdate = 0;

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	pev->fov = m_flFOV = 0;// init field of view.
	m_iClientFOV		= -1; // make sure fov reset is sent
	m_iClientWeaponID = -1;
	memset( m_iClientWeapons, -1, MAX_WEAPON_BYTES );

	m_flNextDecalTime	= 0;// let this player decal as soon as he spawns.

	m_flgeigerDelay = gpGlobals->time + 2.0;	// wait a few seconds until user-defined message registrations
												// are recieved by all clients
	
	m_flTimeStepSound	= 0;
	m_iStepLeft = 0;
	m_flFieldOfView		= 0.5;// some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor	= BLOOD_COLOR_RED;
	m_flNextAttack	= gpGlobals->time;
	StartSneaking();

	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message
	m_iClientSndRoomtype = -1;
	m_iClientUseIcon = -1;
	m_iClientHealth = -1;
	m_iClientStamina = -1;
	m_iClientBlastAbilityLVL = -1;
	m_iClientBlastChargesReady = -1;

	// don't let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam( this );
	edict_t *spawnpoint = g_pGameRules->GetPlayerSpawnSpot( this );
	CBaseEntity *pSpawnPoint = NULL;
	if( spawnpoint != NULL )
		pSpawnPoint = CBaseEntity::Instance( spawnpoint );

	// Move all player spectators to new target origin (bugfix for pmove/PAS issue)
	CBasePlayer* plr;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		plr = (CBasePlayer*)UTIL_PlayerByIndex(i);
		if (!plr ||
			plr->pev->iuser1 == 0 ||
			plr->pev->iuser1 == OBS_ROAMING ||
			plr->pev->iuser1 == OBS_MAP_FREE ||
			plr->pev->iuser2 == 0 ||
			plr->m_hObserverTarget != this)
			continue;

		UTIL_SetOrigin(plr, pev->origin);
	}

	SET_MODEL( ENT( pev ), "models/player.mdl");
	g_ulModelIndexPlayer = pev->modelindex;

	pev->sequence = LookupActivity( ACT_IDLE );

	if ( FBitSet(pev->flags, FL_DUCKING) ) 
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);

	pev->view_ofs = VEC_VIEW;

	Precache();

	m_HackedGunPos		= Vector( 0, 32, 0 );

	// BugFixedHL - Reset view entity
	SET_VIEW(edict(), edict());

	if ( m_iPlayerSound == SOUNDLIST_EMPTY )
		ALERT ( at_console, "Couldn't alloc player sound slot!\n" );

	m_fNoPlayerSound = FALSE;// normal sound behavior.

	m_pLastItem = NULL;
	m_fInitHUD = TRUE;
	m_iClientHideHUD = -1;  // force this to be recalculated
	m_fWeapon = FALSE;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;
	m_iClientSndRoomtype = -1;

	UpdateKeyCatchers ();

	// reset all ammo values to 0
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0;  // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	m_lastx = 0;
	m_lasty = 0;
	
	m_flNextChatTime = gpGlobals->time;
	m_flNextChatSoundTime = gpGlobals->time;
	
	g_pGameRules->PlayerSpawn( this );

	// create a cinematic collision box
	m_pUserData = WorldPhysic->CreateBoxFromEntity( this );

	LastDashTime = 0;
	Dashed = false;
	CanDash = true;
	CanUse = true;
	CanElectroBlast = true;
	CanJump = true;
	CanSprint = true;
	CanMove = true;
	CanShoot = true;
	WeaponLowered = false;
	DrunkLevel = 0;
	CanSelectEmptyWeapon = false;
	HUDSuitOffline = false;
	BreathingEffect = false;
	WigglingEffect = false;
	ShieldOn = false;
	LastUseCheckTime = gpGlobals->time;
	LastBlastTime = gpGlobals->time;
	BlastAbilityLVL = 0;
	if( g_pGameRules->IsMultiplayer() )
	{
		BlastAbilityLVL = 1; // UNDONE - make a server cvar?
		ShieldAvailableLVL = 3;
	}
	BlastChargesReady = 0;
	LastSwimUpSound = -1.0f;
	m_flLastWeaponSwitchTime = 0;
	EnableHealthBar = true;
	ConfirmedHit = 0;

	CanUseDrone = false; // drone can be used only in special zones defined by game
	DroneHealth = DRONE_MAX_HEALTH;
	DroneAmmo = DRONE_MAX_AMMO;
	DroneDistance = 0;
	CameraEntity[0] = '\0';

	KillingSpreeLevel = 0;
	ConsecutiveKills = 0;
	LastConsecutiveKillTime = gpGlobals->time - 10;

	headyaw = 0.0f;

	DashButton = false;
	ElectroblastButton = false;

	IsUpsideDown = false; // diffusion - reset ducking, upside-down issues
	if( pSpawnPoint != NULL )
	{
		if( FBitSet( pSpawnPoint->pev->spawnflags, 4 ) )
			pev->effects |= EF_UPSIDEDOWN;
	}

	BrokenSuit = false;
	NextBrokenDmgTime = gpGlobals->time;

	if( !(g_pGameRules->IsMultiplayer()) )
	{
		SetHUDTexts();
	}

	m_hCachedUseObject = NULL;

	SetFlag(F_NOBACKCULL);

	// diffusion - name your player model starting with "robo" and you'll get those metal effects
	char model_name[32];
	if( g_pGameRules->IsTeamplay() )
		Q_strncpy( model_name, g_pGameRules->GetTeamID( this ), sizeof( model_name ) );
	else
		Q_strncpy( model_name, g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( edict() ), "model" ), sizeof( model_name ) );

	if( !Q_strncmp( model_name, "robo", 4 ) )
	{
		SetFlag( F_METAL_MONSTER );
		m_bloodColor = DONT_BLEED;
	}
	else
		RemoveFlag( F_METAL_MONSTER );

	RefreshScore();

	DroneColor = Vector( 255, 255, 255 );
	DroneAmmo = DRONE_MAX_AMMO;
	DroneHealth = DRONE_MAX_HEALTH;

	CreateFlashlightMonster();

	m_hKiller = NULL;
	fLerpToKiller = 0.0f;
}

void CBasePlayer::SetHUDTexts(void)
{
	m_DroneTextParms.channel = 15;
	m_DroneTextParms.x = -1;
	m_DroneTextParms.y = 0.8;
	m_DroneTextParms.effect = 0;

	m_DroneTextParms.r1 = 255;
	m_DroneTextParms.g1 = 255;
	m_DroneTextParms.b1 = 255;
	m_DroneTextParms.a1 = 200;

	m_DroneTextParms.r2 = 255;
	m_DroneTextParms.g2 = 160;
	m_DroneTextParms.b2 = 0;
	m_DroneTextParms.a2 = 200;
	m_DroneTextParms.fadeinTime = 0;
	m_DroneTextParms.fadeoutTime = 0.5;
	m_DroneTextParms.holdTime = 1;
	m_DroneTextParms.fxTime = 0;
}

void CBasePlayer :: CheckCompatibility( void )
{
	hudtextparms_t	m_textParms;

	m_textParms.channel = 0;
	m_textParms.x = -1;
	m_textParms.y = -1;
	m_textParms.effect = 0;

	m_textParms.r1 = 255;
	m_textParms.g1 = 255;
	m_textParms.b1 = 255;
	m_textParms.a1 = 200;

	m_textParms.r2 = 255;
	m_textParms.g2 = 160;
	m_textParms.b2 = 0;
	m_textParms.a2 = 200;
	m_textParms.fadeinTime = 3;
	m_textParms.fadeoutTime = 3;
	m_textParms.holdTime = 10;
	m_textParms.fxTime = 0.25;
}

void CBasePlayer :: Precache( void )
{
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.
	
	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	// diffusion - moved to ServerActivate in client.cpp
/*	if ( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if ( !WorldGraph.FSetGraphPointers() )
		{
			ALERT ( at_console, "**Graph pointers were not set!\n");
		}
		else
		{
			ALERT ( at_console, "**Graph Pointers Set!\n" );
		} 
	}*/

	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	// init geiger counter vars during spawn and each time
	// we cross a level transition

	m_flgeigerRange = 1000;
	m_igeigerRangePrev = 1000;

	memset( m_iClientWeapons, -1, MAX_WEAPON_BYTES );

	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;

	m_iClientBattery = -1;
	m_iClientSndRoomtype = -1;
	m_flFlashLightTime = 1;
	m_iClientBlastChargesReady = -1;
	m_iClientBlastAbilityLVL = -1;

	m_bRainNeedsUpdate = true;

	m_iTrain |= TRAIN_NEW;

	m_iStartMessage = 1;// send player init messages
	m_iUpdateTime = 5;  // won't update for 1/2 a second

	if ( gInitHUD )
		m_fInitHUD = TRUE;

	BhopEnabled = false;
}

//
// Marks everything as new so the player will resend this to the hud.
//
void CBasePlayer::RenewItems(void)
{

}

void CBasePlayer::UpdateHoldableItem( void )
{
	if (m_pHoldableItem == NULL )
		return;

	// diffusion - without this, it is possible to "prop-fly" on a holdable item, standing on top of it and jumping
	// so, if the player physically touched the holdable thingy (and also looking down!), just drop it.
	// BUGBUG - there are still occasions where you can stand on it - when looking forward and jumping on it, when it's against the wall
	if( m_pHoldableItem->Intersects(this) && (pev->v_angle.x > 45))
	{
		DropHoldableItem( 0 );
		return;
	}

	// diffusion - we are too far from the holdable item!!! (stuck somehwere?)
	// (UNDONE should we account bounds of the item? how big can it be?)
	if( (m_pHoldableItem->GetAbsOrigin() - GetAbsOrigin()).Length() > 300 )
	{
		DropHoldableItem( 0 );
		return;
	}

	// not a valid item anymore (example: respawnable barrel held by player just blew up)
	if( m_pHoldableItem->pev->effects & EF_NODRAW )
	{
		DropHoldableItem( 0 );
		return;
	}

	UTIL_MakeVectors (pev->v_angle);

	Vector vecSrc = EyePosition ();
	Vector vecDst = vecSrc + gpGlobals->v_forward * m_flHoldableItemDistance;

	TraceResult tr;

	if( m_pHoldableItem->m_iActorType == ACTOR_DYNAMIC )
		UTIL_TraceHull( vecSrc, vecDst, dont_ignore_monsters, head_hull, m_pHoldableItem->edict(), &tr );
	else TRACE_MONSTER_HULL( m_pHoldableItem->edict(), vecSrc, vecDst, dont_ignore_monsters, m_pHoldableItem->edict(), &tr );

	Vector newOrigin = tr.vecEndPos;

	UTIL_SetOrigin( m_pHoldableItem, newOrigin );

	// diffusion - need to check if we can actually see the item, otherwise it will go through walls
	TraceResult trCheckSight;
	bool CheckSight = true;

	// diffusion - can't traceline object with the owner... -____-
	edict_t *TempOwner;
	Vector TraceTo = vecSrc + gpGlobals->v_forward * 320;
	if( m_pHoldableItem->pev->owner != NULL )
	{
		// save the owner, nullify it, do the traceline, then restore the owner
		TempOwner = m_pHoldableItem->pev->owner;
		m_pHoldableItem->pev->owner = NULL;
		UTIL_TraceLine( vecSrc, TraceTo, dont_ignore_monsters, dont_ignore_glass, edict(), &trCheckSight );
		m_pHoldableItem->pev->owner = TempOwner;
	}
	else
		UTIL_TraceLine( vecSrc, TraceTo, dont_ignore_monsters, dont_ignore_glass, edict(), &trCheckSight );

	if( trCheckSight.pHit != m_pHoldableItem->edict() )
		CheckSight = false;

	// make sure that new position is valid
	if( m_pHoldableItem->m_iActorType == ACTOR_DYNAMIC )
		UTIL_TraceHull( newOrigin, newOrigin, dont_ignore_monsters, head_hull, m_pHoldableItem->edict(), &tr );
	else
		TRACE_MONSTER_HULL( m_pHoldableItem->edict(), newOrigin, newOrigin, dont_ignore_monsters, m_pHoldableItem->edict(), &tr );

	if (tr.fStartSolid || tr.fAllSolid || !CheckSight)
	{
		// this is bad place. Restore old valid origin
		UTIL_SetOrigin( m_pHoldableItem, m_vecHoldableItemPosition ); 

		if( m_pHoldableItem->m_iActorType == ACTOR_DYNAMIC )
			WorldPhysic->UpdateActorPos( m_pHoldableItem );
#if 0
		// a pseudocode here:
		if (m_flLastBlockedTime < gpGlobals->time)
		{
			EMIT_SOUND ("common\item_stuck.wav");
			m_flLastBlockedTime = gpGlobals->time + 1.0;			
		}
#endif
		// diffusion - one more check
		// ...but it was proven to be messy. delete
	//	UTIL_TraceLine( vecSrc, vecDst, dont_ignore_monsters, dont_ignore_glass, edict(), &trCheckSight );
	//	if( trCheckSight.pHit != m_pHoldableItem->edict() )
	//		DropHoldableItem( 0 );

		return;
	}

	Vector vecAngles = m_pHoldableItem->GetLocalAngles ();

	// NOTE: we not afraid gimball lock here because only YAW rotation is used
	vecAngles.y = m_pHoldableItem->pev->fuser2 + pev->v_angle.y;
	m_pHoldableItem->SetLocalAngles ( vecAngles );

	// refresh the last valid position
	m_vecHoldableItemPosition = m_pHoldableItem->GetAbsOrigin ();

	if( m_pHoldableItem->m_iActorType == ACTOR_DYNAMIC )
		WorldPhysic->UpdateActorPos( m_pHoldableItem );
}

void CBasePlayer::PickHoldableItem( CBaseEntity *pObject )
{
	if (pObject->m_fPicked || pObject->m_hParent != NULL)
	{
		// item is already picked by another player, play use sound and return
		EMIT_SOUND( ENT(pev), CHAN_ITEM, "common/use.wav", 0.4, ATTN_NORM);
		return;
	}

	// diffusion - player is physically touching the object (could be standing on it)
	if( pObject->Intersects(this) && (pev->v_angle.x > 45) )
		return;

	// diffusion - need to check if the item is not behind a wall
	TraceResult trCheckSight;

	// hack! have to do this before traceline. can't pick up point entity after saverestore :(
	Vector org = pObject->GetLocalOrigin();
	UTIL_SetOrigin( pObject, org );

	Vector vecSrc = EyePosition();
	Vector vecDst = pObject->GetAbsOrigin();

	// diffusion - can't traceline object with the owner... -____-
	edict_t *TempOwner;
	if( pObject->pev->owner != NULL )
	{
		// save the owner, nullify it, do the traceline, then restore the owner
		TempOwner = pObject->pev->owner;
		pObject->pev->owner = NULL;
		UTIL_TraceLine( vecSrc, vecDst, dont_ignore_monsters, dont_ignore_glass, edict(), &trCheckSight );
		pObject->pev->owner = TempOwner;
	}
	else
		UTIL_TraceLine( vecSrc, vecDst, dont_ignore_monsters, dont_ignore_glass, edict(), &trCheckSight );

	if( trCheckSight.pHit != pObject->edict() ) // UNDONE likely need to check parented objects too
		return;

	// flare-specific stuff
	if( FClassnameIs(pObject, "item_flare") && pObject->HasFlag(F_ENTITY_ONCEUSED) )
	{
		pObject->RemoveFlag(F_ENTITY_ONCEUSED); // setthink won't be set again.
		if( pObject->HasFlag(F_CUSTOMFLAG1) )
			pObject->RemoveFlag(F_CUSTOMFLAG1); // now the flare is able to hurt monsters
		pObject->pev->owner = edict(); // disable collision for player (issues)
		pObject->SetTouch( &CFlare::BounceTouch );
		pObject->SetThink( &CFlare::Ignite );
		pObject->SetNextThink( 0.1 );
	}
	
	// remember distance and the last valid origin
	m_vecHoldableItemPosition = pObject->GetAbsOrigin();
	m_flHoldableItemDistance = (m_vecHoldableItemPosition - EyePosition()).Length();

	pObject->MakeNonMoving();
	pObject->ClearGroundEntity();
	m_pHoldableItem = pObject;
	pObject->m_fPicked = TRUE;

	HideWeapons( TRUE );
	m_iHideHUD |= HIDEHUD_WPNS_HOLDABLEITEM; // diffusion - hide the weapons (return on drop)

	if( m_pHoldableItem->m_iActorType == ACTOR_DYNAMIC )
		WorldPhysic->MakeKinematic( m_pHoldableItem, TRUE );
	pObject->pev->fuser2 = pObject->GetLocalAngles().y - pev->v_angle.y;

	SetFlag( F_PLAYER_HASITEM );

	pObject->OnPickupObject();
}

void CBasePlayer::DropHoldableItem( int Velocity )
{
	if( Velocity < 0 )
		Velocity = 0;
	
	CBaseEntity *pObject = m_pHoldableItem;
	m_pHoldableItem = NULL;

	pObject->RestoreMoveType();
	pObject->ClearGroundEntity ();
	pObject->m_fPicked = FALSE;

	m_iHideHUD &= ~HIDEHUD_WPNS_HOLDABLEITEM;
	HideWeapons( FALSE );

	if( Dashed == false ) // do not apply huge velocity to object during dash
	{
		Vector angThrow = pev->v_angle;
		UTIL_MakeVectors( angThrow );
		Vector vecThrow = gpGlobals->v_forward * Velocity + GetAbsVelocity();
		pObject->SetAbsVelocity( vecThrow );
	}

	pObject->SetBaseVelocity( GetBaseVelocity() );	// in case player is standing on the conveyor belt

	if( pObject->m_iActorType == ACTOR_DYNAMIC )
	{
		WorldPhysic->MakeKinematic( pObject, FALSE );
		WorldPhysic->AddForce( pObject, GetAbsVelocity() * 3000.0f );
	}

	RemoveFlag( F_PLAYER_HASITEM );

	pObject->OnDropObject();
}

int CBasePlayer::Restore( CRestore &restore )
{
	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;

	m_bConnected = TRUE;

	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	// landmark isn't present.
	if( !pSaveData->fUseLandmark )
	{
		ALERT( at_console, "No Landmark:%s\n", pSaveData->szLandmarkName );

		// default to normal spawn
		edict_t *pentSpawnSpot = EntSelectSpawnPoint( this );
		CBaseEntity *pSpawnSpot = CBaseEntity::Instance( pentSpawnSpot );
		SetAbsOrigin( pSpawnSpot->GetAbsOrigin() + Vector( 0, 0, 1 ));
		SetAbsAngles( pSpawnSpot->GetAbsAngles( ));
		if( FBitSet( pSpawnSpot->pev->spawnflags, 4 ) )
			pev->effects |= EF_UPSIDEDOWN;
	}

	pev->v_angle.z = 0;		// Clear out roll

	SetAbsAngles( pev->v_angle );
	pev->fixangle = TRUE;	// turn this way immediately

	// Copied from spawn() for now
	m_bloodColor = BLOOD_COLOR_RED;

	g_ulModelIndexPlayer = pev->modelindex;

	if ( FBitSet(pev->flags, FL_DUCKING) ) 
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	if ( m_fLongJump )
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "1" );
	else
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );

	RenewItems();

	CreateFlashlightMonster();

	return status;
}

void CBasePlayer::SelectNextItem( int iItem )
{
	CBasePlayerItem *pItem;

	pItem = m_rgpPlayerItems[ iItem ];
	
	if (!pItem)
		return;
	
	if (pItem == m_pActiveItem)
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext; 
		if (!pItem)
			return;

		CBasePlayerItem *pLast;
		pLast = pItem;
		while (pLast->m_pNext)
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext = m_pActiveItem;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[ iItem ] = pItem;
	}

	ResetAutoaim( );

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
		m_pActiveItem->Holster( );

	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->Deploy( );
		m_pActiveItem->UpdateItemInfo( );
	}
}

void CBasePlayer::SelectItem( const char *pstr )
{
	if (!pstr)
		return;

	CBasePlayerItem *pItem = NULL;

	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			pItem = m_rgpPlayerItems[i];
	
			while (pItem)
			{
				if (FClassnameIs(pItem->pev, pstr))
					break;
				pItem = pItem->m_pNext;
			}
		}

		if (pItem)
			break;
	}

	if (!pItem)
		return;

	
	if (pItem == m_pActiveItem)
		return;

	ResetAutoaim( );

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
		m_pActiveItem->Holster( );
	
	m_pLastItem = m_pActiveItem;
	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->Deploy( );
		m_pActiveItem->UpdateItemInfo( );
	}
}


void CBasePlayer::SelectLastItem( void )
{
	if( InCar )
	{
		if( pCar )
		{
			if( pCar->CamUnlocked )
			{
				pCar->fMouseTouchedOFF = CAR_CAMERA_MOUSE_WAIT_OFF;
				pCar->fMouseTouchedON = 0.0f;
				pCar->CamUnlocked = false;
			}
			else
				pCar->SecondaryCamera = !pCar->SecondaryCamera;
		}

		return;
	}
	
	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS ) )
		return;

	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS_HOLDABLEITEM ) )
		return;

	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS_CUSTOM ) )
		return;
	
	if( !m_pLastItem || m_pActiveItem == m_pLastItem )
	{
		// sometimes when I have only one weapon (or no last weapons), I still try to press Q by habit
		// to quickly get out of scope zoom. in that case it never worked.
		// so this is a quick hack to bypass this - still leaving zoom by pressing Q even if you have one weapon.
		if( m_pActiveItem && (m_pActiveItem->m_iId == WEAPON_SNIPER || m_pActiveItem->m_iId == WEAPON_CROSSBOW))
		{
			if( ZoomState > 0 )
			{
				m_pActiveItem->Holster();
				m_pActiveItem->Deploy();
			}
		}
		return;
	}

	if ( m_pActiveItem && !m_pActiveItem->CanHolster() )
		return;

	if( gpGlobals->time < m_flLastWeaponSwitchTime )
		return;

	m_flLastWeaponSwitchTime = gpGlobals->time + 0.5;

	if( !m_pLastItem->CanDeploy() ) // last item is out of ammo?
	{
		m_pLastItem = m_pActiveItem;
		if( g_pGameRules->GetNextBestWeapon( this, m_pActiveItem ) )
			return;
		else
			m_pLastItem = NULL;

		return;
	}

	ResetAutoaim( );

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
		m_pActiveItem->Holster( );
	
	CBasePlayerItem *pTemp = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = pTemp;
	m_pActiveItem->Deploy( );
	m_pActiveItem->UpdateItemInfo( );
	
	// diffusion - play weapon switch sound
	PlayClientSound( this, 249 );
	
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
BOOL CBasePlayer::HasWeapons( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
			return TRUE;
	}

	return FALSE;
}

void CBasePlayer::SelectPrevItem( int iItem )
{
}

const char *CBasePlayer::TeamID( void )
{
	if ( pev == NULL )		// Not fully connected yet
		return "";

	// return their team name
	return m_szTeamName;
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
	DECLARE_CLASS( CSprayCan, CBaseEntity );
public:
	void	Spawn( CBaseEntity *pOwner );
	void	Think( void );

	virtual int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn( CBaseEntity *pOwner )
{
	SetAbsOrigin( pOwner->GetAbsOrigin() + Vector ( 0, 0, 32 ));
	SetAbsAngles( pOwner->pev->v_angle );
	pev->owner = pOwner->edict();
	pev->frame = 0;

	SetNextThink( 0.1 );
	EMIT_SOUND( edict(), CHAN_VOICE, "player/sprayer.wav", 1, ATTN_NORM );
}

void CSprayCan::Think( void )
{
	TraceResult	tr;	
	int playernum;
	int nFrames;
	CBasePlayer *pPlayer;
	
	pPlayer = (CBasePlayer *)GET_PRIVATE(pev->owner);

	if (pPlayer)
		nFrames = pPlayer->GetCustomDecalFrames();
	else
		nFrames = -1;

	playernum = ENTINDEX(pev->owner);
	
	// ALERT(at_console, "Spray by player %i, %i of %i\n", playernum, (int)(pev->frame + 1), nFrames);

	UTIL_MakeVectors( GetAbsAngles( ));
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + gpGlobals->v_forward * 128, dont_ignore_monsters, pev->owner, &tr );

	// No customization present.
	if (nFrames == -1)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
			UTIL_StudioDecalTrace( &tr, DECAL_LAMBDA6 );
		else UTIL_DecalTrace( &tr, DECAL_LAMBDA6 );

		UTIL_Remove( this );
	}
	else
	{
		UTIL_PlayerDecalTrace( &tr, playernum, pev->frame, TRUE );
		// Just painted last custom frame.
		if( pev->frame++ >= (nFrames - 1))
			UTIL_Remove( this );
	}

	SetNextThink( 0.1 );
}

class CBloodSplat : public CBaseEntity
{
	DECLARE_CLASS( CBloodSplat, CBaseEntity );
public:
	void Spawn( CBaseEntity *pOwner );
	void Spray( void );
};

void CBloodSplat :: Spawn( CBaseEntity *pOwner )
{
	SetAbsOrigin( pOwner->GetAbsOrigin() + Vector ( 0, 0, 32 ));
	SetAbsAngles( pOwner->pev->v_angle );
	pev->owner = pOwner->edict();

	SetThink( &CBloodSplat::Spray );
	SetNextThink( 0.1 );
}

void CBloodSplat::Spray ( void )
{
	TraceResult tr;	
	
	UTIL_MakeVectors( GetAbsAngles() );
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + gpGlobals->v_forward * 128, dont_ignore_monsters, pev->owner, &tr );

	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

	if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
		UTIL_BloodStudioDecalTrace( &tr, BLOOD_COLOR_RED );
	else
		UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( 0.1 );
}

//==============================================

void CBasePlayer::GiveNamedItem( const char *pszName )
{
	CBaseEntity *pEnt = CreateEntityByName( pszName );

	if( !pEnt )
	{
		ALERT ( at_console, "NULL Ent in GiveNamedItem!\n" );
		return;
	}

	pEnt->SetAbsOrigin( GetAbsOrigin() );
	pEnt->pev->spawnflags |= SF_NORESPAWN;

	DispatchSpawn( pEnt->edict() );
	DispatchTouch( pEnt->edict(), ENT( pev ) );
}

CBaseEntity *FindEntityForward( CBaseEntity *pMe )
{
	TraceResult tr;

	UTIL_MakeVectors( pMe->pev->v_angle );
	UTIL_TraceLine( pMe->EyePosition(), pMe->EyePosition() + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr );

	if( tr.flFraction != 1.0 && !FNullEnt( tr.pHit ))
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		return pHit;
	}
	return NULL;
}


BOOL CBasePlayer :: FlashlightIsOn( void )
{
	return FBitSet(pev->effects, EF_DIMLIGHT);
}


void CBasePlayer :: FlashlightTurnOn( void )
{
	if ( !g_pGameRules->FAllowFlashlight() )
		return;

	if( !HasWeapon( WEAPON_SUIT ) )
		return;

	if( pCar != NULL )
		return;

	EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM );
	SetBits( pev->effects, EF_DIMLIGHT );

	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_BYTE( m_iFlashBattery );
	MESSAGE_END();

	m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;

	if( m_pFlashlightMonster )
		m_pFlashlightMonster->pev->effects &= ~EF_NODRAW;
}


void CBasePlayer :: FlashlightTurnOff( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_STATIC, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM );
	ClearBits(pev->effects, EF_DIMLIGHT);

	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(m_iFlashBattery);
	MESSAGE_END();

	m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;

	if( m_pFlashlightMonster )
		m_pFlashlightMonster->pev->effects |= EF_NODRAW;
}

/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
void CBasePlayer :: ForceClientDllUpdate( void )
{
	m_iClientHealth  = -1;
	m_iClientBattery = -1;
	m_iClientSndRoomtype = -1;
	m_iClientStamina = -1;
	memset( m_iClientWeapons, -1, MAX_WEAPON_BYTES );
	m_iStartMessage = 1; // send player init messages
	m_iTrain |= TRAIN_NEW;  // Force new train message.
	m_fWeapon = FALSE;          // Force weapon send
	m_fKnownItem = FALSE;    // Force weaponinit messages.
	m_bRainNeedsUpdate = true;
	m_fInitHUD = TRUE;		// Force HUD gmsgResetHUD message

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();
}

/*
============
ImpulseCommands
============
*/
extern float g_flWeaponCheat;

void CBasePlayer::ImpulseCommands( )
{
	TraceResult	tr;// UNDONE: kill me! This is temporary for PreAlpha CDs

	// Handle use events
	PlayerUse();
		
	int iImpulse = (int)pev->impulse;

	// custom handled buttons
	if( iImpulse >= 1 && iImpulse <= 50 )
	{
		UTIL_FireTargets( "game_firetarget", this, this, USE_TOGGLE, iImpulse );
		pev->impulse = 0;
		return;
	}

	switch (iImpulse)
	{
	case 99:
		{

		int iOn;

		if (!gmsgLogo)
		{
			iOn = 1;
			gmsgLogo = REG_USER_MSG("Logo", 1);
		} 
		else 
			iOn = 0;
		
		ASSERT( gmsgLogo > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgLogo, NULL, pev );
			WRITE_BYTE(iOn);
		MESSAGE_END();

		if(!iOn)
			gmsgLogo = 0;
		break;
		}
	case 100:
		if( m_hDrone && DroneControl )
		{
			if( m_hDrone->pev->effects & EF_DIMLIGHT )
				m_hDrone->pev->effects &= ~EF_DIMLIGHT;
			else
				m_hDrone->pev->effects |= EF_DIMLIGHT;

			EMIT_SOUND( m_hDrone->edict(), CHAN_STATIC, "drone/drone_flashlight.wav", VOL_NORM, ATTN_IDLE );
		}
		else
		{
			if( FlashlightIsOn() )
				FlashlightTurnOff();
			else
				FlashlightTurnOn();
		}
		break;
	case 201:// paint decal
		if ( gpGlobals->time < m_flNextDecalTime )
		{
			// too early!
			break;
		}

		UTIL_MakeVectors( pev->v_angle );
		UTIL_TraceLine( EyePosition(), EyePosition() + gpGlobals->v_forward * 128, dont_ignore_monsters, edict(), &tr );

		if ( tr.flFraction != 1.0 )
		{
			// line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + decalfrequency.value;
			CSprayCan *pCan = GetClassPtr((CSprayCan *)NULL);
			pCan->Spawn( this );
		}

		break;

	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands( iImpulse );
		break;
	}
	
	pev->impulse = 0;
}

const char *pHitboxNames[] =
{
"Generic",
"Head",
"Chest",
"Stomach",
"Left Arm",
"Right Arm",
"Left Leg",
"Right Leg",
"Unknown8",
"Unknown9",
"Unknown10",
"Unknown11",
"Unknown12",
"Unknown13",
"Unknown14",
"Unknown15",
"",
};

//=========================================================
//=========================================================
void CBasePlayer::CheatImpulseCommands( int iImpulse )
{
	if( g_flWeaponCheat == 0.0 )
		return;
	else
	{
		if( !bCheatsWereUsed )
		{
			if( !g_pGameRules->IsMultiplayer() && (int)CVAR_GET_FLOAT( "developer" ) < 2 )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, edict() );
				WRITE_BYTE( 0 );
				WRITE_STRING( "HINT_CHEATS" );
				MESSAGE_END();
			}
		}

		bCheatsWereUsed = true;
	}

	CBaseEntity *pEntity;
	TraceResult tr;

	switch( iImpulse )
	{
	case 76:
		UTIL_MakeVectors( Vector( 0, pev->v_angle.y, 0 ));
		Create( "monster_human_grunt", GetAbsOrigin() + gpGlobals->v_forward * 128, GetAbsAngles( ));
		break;
	case 101: // diffusion - removed unnecessary weapons I don't use
		gEvilImpulse101 = TRUE;
		m_flLastTimeTutorWasShown = gpGlobals->time; // don't show tutors when cheated weapons
		GiveNamedItem( "item_suit" );
		GiveNamedItem( "weapon_knife" );
		GiveNamedItem( "weapon_9mmhandgun" );
		GiveNamedItem( "weapon_shotgun" );
		GiveNamedItem( "weapon_shotgun_xm" );
		GiveNamedItem( "weapon_9mmAR" );
		GiveNamedItem( "ammo_ARgrenades" );
		GiveNamedItem( "weapon_handgrenade" );
		GiveNamedItem( "weapon_tripmine" );
		GiveNamedItem( "weapon_357" );
		GiveNamedItem( "weapon_crossbow" );
		GiveNamedItem( "weapon_gausniper" );
		GiveNamedItem( "weapon_rpg" );
		GiveNamedItem( "weapon_satchel" );
		GiveNamedItem( "weapon_ar2" );
		GiveNamedItem( "ammo_ar2ball" );
		GiveNamedItem( "weapon_drone" );
		GiveNamedItem( "weapon_sentry" );
		GiveNamedItem( "weapon_hkmp5" );
		GiveNamedItem( "weapon_fiveseven" );
		GiveNamedItem( "weapon_sniper" );
		GiveNamedItem( "weapon_g36c" );
		gEvilImpulse101 = FALSE;
		break;
	case 102:
		// Gibbage!!!
		CGib::SpawnRandomGibs( pev, 1, 1 );
		break;

	case 103:
		// What the hell are you doing?
		pEntity = FindEntityForward( this );
		if ( pEntity )
		{
			CBaseMonster *pMonster = pEntity->MyMonsterPointer();
			if ( pMonster )
				pMonster->ReportAIState();
		}
		break;

	case 104:
		// Dump all of the global state varaibles (and global entity names)
		gGlobalState.DumpGlobals();
		break;

	case 105:// player makes no sound for monsters to hear.
		{
			if ( m_fNoPlayerSound )
			{
				ALERT ( at_console, "Player is audible\n" );
				m_fNoPlayerSound = FALSE;
			}
			else
			{
				ALERT ( at_console, "Player is silent\n" );
				m_fNoPlayerSound = TRUE;
			}
			break;
		}

	case 106:
		// Give me the classname and targetname of this entity.
		pEntity = FindEntityForward( this );
		if ( pEntity )
			pEntity->ReportInfo();
		break;

	case 107:
		{
			TraceResult tr;

			edict_t *pWorld = INDEXENT( 0 );

			Vector start = EyePosition();
			Vector end = start + gpGlobals->v_forward * 1024;
			UTIL_TraceLine( start, end, dont_ignore_monsters, edict(), &tr );
			if ( tr.pHit )
				pWorld = tr.pHit;

			if( pWorld->v.solid == SOLID_BSP )
			{
				const char *pTextureName = TRACE_TEXTURE( pWorld, start, end );
				if ( pTextureName )
					ALERT( at_console, "Texture: %s\n", pTextureName );
			}
			else ALERT( at_console, "Hitgroup: %s\n", pHitboxNames[tr.iHitgroup] );
		}
		break;
	case 195:// show shortest paths for entire level to nearest node
		{
			Create( "node_viewer_fly", GetAbsOrigin(), GetAbsAngles( ));
		}
		break;
	case 196:// show shortest paths for entire level to nearest node
		{
			Create( "node_viewer_large", GetAbsOrigin(), GetAbsAngles( ));
		}
		break;
	case 197:// show shortest paths for entire level to nearest node
		{
			Create( "node_viewer_human", GetAbsOrigin(), GetAbsAngles( ));
		}
		break;
	case 199:// show nearest node and all connections
		{
			ALERT( at_console, "%d\n", WorldGraph.FindNearestNode( GetAbsOrigin(), bits_NODE_GROUP_REALM ));
			WorldGraph.ShowNodeConnections( WorldGraph.FindNearestNode ( GetAbsOrigin(), bits_NODE_GROUP_REALM ));
		}
		break;
	case 202:// Random blood splatter
		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine ( EyePosition(), EyePosition() + gpGlobals->v_forward * 128, dont_ignore_monsters, edict(), &tr );

		if ( tr.flFraction != 1.0 )
		{// line hit something, so paint a decal
			CBloodSplat *pBlood = GetClassPtr((CBloodSplat *)NULL);
			pBlood->Spawn( this );
		}
		break;
	case	203:// remove creature.
		pEntity = FindEntityForward( this );
		if ( pEntity )
		{
			if( pEntity != g_pWorld )
				UTIL_Remove( pEntity );
		}
		break;
	}
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
int CBasePlayer::AddPlayerItem( CBasePlayerItem *pItem )
{
	CBasePlayerItem *pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];

	while (pInsert)
	{
		if (FClassnameIs( pInsert->pev, STRING( pItem->pev->classname) ))
		{
			if( pItem->AddDuplicate( pInsert ))
			{
				g_pGameRules->PlayerGotWeapon ( this, pItem );
				pItem->CheckRespawn();

				// ugly hack to update clip w/o an update clip message
				pInsert->UpdateItemInfo( );
				if (m_pActiveItem)
					m_pActiveItem->UpdateItemInfo( );

				pItem->Kill( );
			}
			else if( gEvilImpulse101 )
			{
				pItem->Kill( );
			}
			return FALSE;
		}
		pInsert = pInsert->m_pNext;
	}


	if( pItem->AddToPlayer( this ))
	{
		g_pGameRules->PlayerGotWeapon ( this, pItem );
		pItem->CheckRespawn();

		pItem->m_pNext = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		// should we switch to this item?
		if ( g_pGameRules->FShouldSwitchWeapon( this, pItem ) )
		{
			SwitchWeapon( pItem );
		}

		return TRUE;
	}
	else if( gEvilImpulse101 )
	{
		pItem->Kill( );
	}
	return FALSE;
}



int CBasePlayer::RemovePlayerItem( CBasePlayerItem *pItem )
{
	if (m_pActiveItem == pItem)
	{
		ResetAutoaim( );
		pItem->Holster( );
		pItem->pev->nextthink = 0;// crowbar may be trying to swing again, etc.
		pItem->SetThink( NULL );
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}

	if ( m_pLastItem == pItem ) // diffusion - BugFixedHL by Lev
		m_pLastItem = NULL;

	CBasePlayerItem *pPrev = m_rgpPlayerItems[pItem->iItemSlot()];

	if (pPrev == pItem)
	{
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem->m_pNext;
		return TRUE;
	}
	else
	{
		while (pPrev && pPrev->m_pNext != pItem)
		{
			pPrev = pPrev->m_pNext;
		}
		if (pPrev)
		{
			pPrev->m_pNext = pItem->m_pNext;
			return TRUE;
		}
	}
	return FALSE;
}


//
// Returns the unique ID for the ammo, or -1 if error
//
int CBasePlayer :: GiveAmmo( int iCount, const char *szName, int iMax )
{
	if ( !szName )
	{
		// no ammo.
		return -1;
	}

	if ( !g_pGameRules->CanHaveAmmo( this, szName, iMax ) )
	{
		// game rules say I can't have any more of this ammo type.
		return -1;
	}

	int i = 0;

	i = GetAmmoIndex( szName );

	if ( i < 0 || i >= MAX_AMMO_SLOTS )
		return -1;

	int iAdd = Q_min( iCount, iMax - m_rgAmmo[i] );
	if ( iAdd < 1 )
		return i;

	m_rgAmmo[ i ] += iAdd;


	if ( gmsgAmmoPickup )  // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN( MSG_ONE, gmsgAmmoPickup, NULL, pev );
			WRITE_BYTE( GetAmmoIndex(szName) );		// ammo ID
			WRITE_BYTE( iAdd );		// amount
		MESSAGE_END();

		if (!this->m_fInitHUD)
		{
			// Send to all spectating players
			CBasePlayer* plr;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				plr = (CBasePlayer*)UTIL_PlayerByIndex(i);
				if (!plr || plr->pev->iuser1 != OBS_IN_EYE || plr->m_hObserverTarget != this)
					continue;

				MESSAGE_BEGIN(MSG_ONE, gmsgAmmoPickup, NULL, plr->pev);
				WRITE_BYTE(GetAmmoIndex(szName));		// ammo ID
				WRITE_BYTE(iAdd);		// amount
				MESSAGE_END();
			}
		}
	}

	return i;
}


/*
============
ItemPreFrame

Called every frame by the player PreThink
============
*/
void CBasePlayer::ItemPreFrame()
{
	if ( gpGlobals->time < m_flNextAttack )
	{
		return;
	}

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPreFrame( );
}


/*
============
ItemPostFrame

Called every frame by the player PostThink
============
*/
void CBasePlayer::ItemPostFrame()
{
//	static int fInSelect = FALSE;

	ImpulseCommands();

	// check if the player is using a tank
	if( m_pTank != NULL )
		return;

	// diffusion - https://github.com/ValveSoftware/halflife/issues/3345
	// check again if the player is using a tank if they started using it in PlayerUse
	if( m_pTank != NULL )
		return;

	if( !m_pActiveItem )
		return;

	if ( gpGlobals->time < m_flNextAttack )
		return;

	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS ))
		return;

	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS_HOLDABLEITEM ) )
		return;

	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS_CUSTOM ) )
		return;

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPostFrame( );
}

int CBasePlayer::AmmoInventory( int iAmmoIndex )
{
	if (iAmmoIndex == -1)
	{
		return -1;
	}

	return m_rgAmmo[ iAmmoIndex ];
}

int CBasePlayer::GetAmmoIndex(const char *psz)
{
	int i;

	if (!psz)
		return -1;

	for (i = 1; i < MAX_AMMO_SLOTS; i++)
	{
		if ( !CBasePlayerItem::AmmoInfoArray[i].pszName )
			continue;

		if (stricmp( psz, CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0)
			return i;
	}

	return -1;
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
// If this player is spectating someone target will be in pPlayer
void CBasePlayer::SendAmmoUpdate(CBasePlayer* pPlayer)
{
	for (int i = 0; i < MAX_AMMO_SLOTS;i++)
	{
		if (this->m_rgAmmoLast[i] != pPlayer->m_rgAmmo[i])
		{
			this->m_rgAmmoLast[i] = pPlayer->m_rgAmmo[i];

			ASSERT( pPlayer->m_rgAmmo[i] >= 0 );
			ASSERT( pPlayer->m_rgAmmo[i] < 255 );

			// send "Ammo" update message
			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
				WRITE_BYTE( i );
				WRITE_BYTE( bound(0, pPlayer->m_rgAmmo[i], 254 ) );  // clamp the value to one byte
			MESSAGE_END();
		}
	}
}

//=========================================================
// UpdateClientData: resends any changed player HUD info to the client.
// Called every frame by PlayerPreThink
// Also called at start of demo recording and playback by
// ForceClientDllUpdate to ensure the demo gets messages
// reflecting all of the HUD state info.
//=========================================================
void CBasePlayer :: UpdateClientData( void )
{
	if (m_fInitHUD)
	{
		m_fInitHUD = FALSE;
		gInitHUD = FALSE;

		MESSAGE_BEGIN( MSG_ONE, gmsgResetHUD, NULL, pev );
			WRITE_BYTE( 1 );
		MESSAGE_END();

		// reset use icon
		MESSAGE_BEGIN(MSG_ONE, gmsgUseIcon, NULL, pev);
			WRITE_BYTE(0);
		MESSAGE_END();

		if ( !m_fGameHUDInitialized )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, pev );
			MESSAGE_END();

			// Send spectator statuses
			CBasePlayer* plr;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				plr = (CBasePlayer*)UTIL_PlayerByIndex(i);
				if (!plr || !plr->IsObserver())
					continue;
			}

			g_pGameRules->InitHUD( this );
			m_fGameHUDInitialized = TRUE;
			m_iObserverMode = OBS_ROAMING;

			if ( g_pGameRules->IsMultiplayer() )
				UTIL_FireTargets( "game_playerjoin", this, this, USE_TOGGLE, 0 );

			CheckCompatibility();
			m_iStartMessage = 1; //send player init messages
		}

		// entity state of the world is not sent to client so have to do this :(
		edict_t *pWorld = INDEXENT( 0 );
		int sunlightscale = pWorld->v.fuser1;

		// this must be reset it seems (jump and sprint conditions)
		MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
			WRITE_BYTE( TE_PLAYERPARAMS );
			WRITE_BYTE( CanJump );
			WRITE_BYTE( CanSprint );
			WRITE_BYTE( CanMove );
			WRITE_BYTE( HUDSuitOffline );
			WRITE_BYTE( CanSelectEmptyWeapon );
			WRITE_BYTE( InCar );
			WRITE_BYTE( BreathingEffect );
			WRITE_BYTE( WigglingEffect );
			WRITE_BYTE( ShieldOn );
			WRITE_BYTE( PlayingDrums );
			WRITE_BYTE( WeaponLowered );
			WRITE_BYTE( CanShoot );
			WRITE_BYTE( DrunkLevel );
			WRITE_BYTE( CanUseDrone );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
			WRITE_BYTE( TE_SUNLIGHT_SCALE );
			WRITE_SHORT( sunlightscale );
		MESSAGE_END();
		CanJump_CL = CanJump;
		CanSprint_CL = CanSprint;
		CanMove_CL = CanMove;
		HUDSuitOffline_CL = HUDSuitOffline;
		CanSelectEmptyWeapon_CL = CanSelectEmptyWeapon;
		InCar_CL = InCar;
		BreathingEffect_CL = BreathingEffect;
		WigglingEffect_CL = WigglingEffect;
		ShieldOn_CL = ShieldOn;
		PlayingDrums_CL = PlayingDrums;
		WeaponLowered_CL = WeaponLowered;
		CanShoot_CL = CanShoot;
		DrunkLevel_CL = DrunkLevel;
		CanUseDrone_CL = CanUseDrone;
		SunLightScale_CL = sunlightscale;

		MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( STRING( Objective ) );
			WRITE_STRING( STRING( Objective2 ) );
		MESSAGE_END();
		Objective_CL = Objective;
		Objective2_CL = Objective2;
		
		// update zoom state
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, pev );
		WRITE_BYTE( ZoomState );
		if( m_pActiveItem != NULL )
			WRITE_BYTE( m_pActiveItem->m_iId );
		else
			WRITE_BYTE( 0 );
		MESSAGE_END();

		InitStatusBar();

		// disable Alice hud until it's updated
		MESSAGE_BEGIN( MSG_ONE, gmsgHealthVisualAlice, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	CBasePlayer* pPlayer = this;
	// We will take spectating target player status if we have it
	if (pev->iuser1 == OBS_IN_EYE && m_hObserverTarget)
	{
		// This will fix angles, so pain display will show correct direction
		pev->angles = pev->v_angle = m_hObserverTarget->pev->angles;
		pev->fixangle = TRUE;
		pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(ENTINDEX(m_hObserverTarget->edict()));
		if (!pPlayer)
			pPlayer = this;
	}

	if ( m_iHideHUD != m_iClientHideHUD )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHideWeapon, NULL, pev );
			WRITE_BYTE( m_iHideHUD );
		MESSAGE_END();

		m_iClientHideHUD = m_iHideHUD;
	}

	if ( memcmp( m_iWeapons, m_iClientWeapons, MAX_WEAPON_BYTES ))
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapons, NULL, pev );
			WRITE_BYTES( m_iWeapons, MAX_WEAPON_BYTES );
		MESSAGE_END();

		memcpy( m_iClientWeapons, m_iWeapons, MAX_WEAPON_BYTES );
	}

	// diffusion - set default fov
	// + small fix
	// BUGBUG - dedicated server gets spammed with "default_fov" not found!
	if ( !IS_DEDICATED_SERVER() )
	{
	/*	if (CVAR_GET_FLOAT("default_fov") <= 0)
		{
			ALERT(at_error, "default_fov should be above zero!\n");
			CVAR_SET_FLOAT("default_fov", 85);
		}
	*/
		if (m_flFOV == 0.0f)
			m_flFOV = CVAR_GET_FLOAT("default_fov");
	}
	
//	ALERT( at_console, "m_iClientFOV %i, m_flFOV %i\n", m_iClientFOV, (int)m_flFOV );
	if ( (int)m_flFOV != m_iClientFOV )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
			WRITE_BYTE( (int)m_flFOV );
		MESSAGE_END();
		// cache FOV change at end of function, so weapon updates can see that FOV has changed
	}

	if( Objective != Objective_CL || Objective2 != Objective2_CL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_STRING( STRING( Objective ) );
		WRITE_STRING( STRING( Objective2 ) );
		MESSAGE_END();
		Objective_CL = Objective;
		Objective2_CL = Objective2;
	}
	
	if( (CanJump != CanJump_CL) 
		|| (CanSprint != CanSprint_CL) 
		|| (CanMove != CanMove_CL) 
		|| (HUDSuitOffline != HUDSuitOffline_CL) 
		|| (CanSelectEmptyWeapon != CanSelectEmptyWeapon_CL) 
		|| (InCar != InCar_CL) 
		|| (BreathingEffect != BreathingEffect_CL) 
		|| (WigglingEffect != WigglingEffect_CL) 
		|| (ShieldOn != ShieldOn_CL) 
		|| (PlayingDrums != PlayingDrums_CL)
		|| (WeaponLowered != WeaponLowered_CL)
		|| (DrunkLevel != DrunkLevel_CL)
		|| (CanShoot != CanShoot_CL)
		|| (CanUseDrone != CanUseDrone_CL)
		)
	{
		// HACKHACK I send this through tempent :)
		MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
			WRITE_BYTE( TE_PLAYERPARAMS );
			WRITE_BYTE( CanJump );
			WRITE_BYTE( CanSprint );
			WRITE_BYTE( CanMove );
			WRITE_BYTE( HUDSuitOffline );
			WRITE_BYTE( CanSelectEmptyWeapon );
			WRITE_BYTE( InCar );
			WRITE_BYTE( BreathingEffect );
			WRITE_BYTE( WigglingEffect );
			WRITE_BYTE( ShieldOn );
			WRITE_BYTE( PlayingDrums );
			WRITE_BYTE( WeaponLowered );
			WRITE_BYTE( CanShoot );
			WRITE_BYTE( DrunkLevel );
			WRITE_BYTE( CanUseDrone );
		MESSAGE_END();

		CanJump_CL = CanJump;
		CanSprint_CL = CanSprint;
		CanMove_CL = CanMove;
		HUDSuitOffline_CL = HUDSuitOffline;
		CanSelectEmptyWeapon_CL = CanSelectEmptyWeapon;
		InCar_CL = InCar;
		BreathingEffect_CL = BreathingEffect;
		WigglingEffect_CL = WigglingEffect;
		ShieldOn_CL = ShieldOn;
		PlayingDrums_CL = PlayingDrums;
		WeaponLowered_CL = WeaponLowered;
		CanShoot_CL = CanShoot;
		DrunkLevel_CL = DrunkLevel;
		CanUseDrone_CL = CanUseDrone;
	}

	edict_t *pWorld = INDEXENT( 0 );
	int sunlightscale = pWorld->v.fuser1;
	if( sunlightscale != SunLightScale_CL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
			WRITE_BYTE( TE_SUNLIGHT_SCALE );
			WRITE_SHORT( sunlightscale );
		MESSAGE_END();
		SunLightScale_CL = sunlightscale;
	}

	// diffusion - setup c-c-c-crosshair!!!
	if( m_pActiveItem == NULL )
	{
		if( m_iClientWeaponID != -1 )
			m_iClientWeaponID = 0;
	}
	
	if (m_pActiveItem != NULL)
	{
		if ((m_pActiveItem->m_iId != m_iClientWeaponID) || (ConfirmedHit > 0) || DroneCrosshairUpdate )
		{
			// BYTE 0,1 (active, not active)
			// BYTE - weapon ID
			// BYTE - enable hitmarker
			MESSAGE_BEGIN(MSG_ONE, gmsgCrosshairStatic, NULL, pev);
				WRITE_BYTE(1);
				WRITE_BYTE(m_pActiveItem->m_iId);
				if (ConfirmedHit > 0) // place a hitmarker if we dealt damage
				{
					WRITE_BYTE(ConfirmedHit);
					ConfirmedHit = 0;
					WRITE_SHORT( DamageDealt );
					DamageDealt = 0;
				}
				else
				{
					WRITE_BYTE( 0 );
					WRITE_SHORT( 0 );
				}
				if( DroneControl )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
			MESSAGE_END();

			if( DroneCrosshairUpdate )
				DroneCrosshairUpdate = false;
		}

		m_iClientWeaponID = m_pActiveItem->m_iId;
	}
	else if ( m_iClientWeaponID == 0 )
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgCrosshairStatic, NULL, pev);
			WRITE_BYTE(0);
		MESSAGE_END();
		m_iClientWeaponID = -1;
	}

	if( m_iSndRoomtype != m_iClientSndRoomtype )
	{
		// update dsp sound
		MESSAGE_BEGIN( MSG_ONE, SVC_ROOMTYPE, NULL, pev );
			WRITE_SHORT( m_iSndRoomtype );
		MESSAGE_END();

		m_iClientSndRoomtype = m_iSndRoomtype;
	}
  
	// HACKHACK -- send the message to display the game title
	if (gDisplayTitle)
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgShowGameTitle, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		gDisplayTitle = 0;
	}

	if (m_iStartMessage != 0)
	{
		SendStartMessages();
		m_iStartMessage = 0;
	}

	if ((int)pev->health != m_iClientHealth)
	{
		int iHealth = Q_max( pev->health, 0 );  // make sure that no negative health values are sent

		// diffusion - FIXME!!! I still need to send this for pain sprites to draw, must be moved to new health HUD somehow
		MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
			WRITE_BYTE( iHealth );
		MESSAGE_END();
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgHealthVisual, NULL, pev );
			WRITE_BYTE( 0 ); // don't flash the health
			WRITE_BYTE( iHealth );  // send the value to client HUD
			WRITE_BYTE( pev->max_health );
		MESSAGE_END();

		m_iClientHealth = pev->health;
	}

	if( ((int)m_flStaminaValue != m_iClientStamina) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStamina, NULL, pev );
			WRITE_BYTE( (int)m_flStaminaValue );
		MESSAGE_END();

		m_iClientStamina = (int)m_flStaminaValue;
	}

	// update blast charge client values when changed
	if( m_iClientBlastChargesReady != BlastChargesReady )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBlastIcons, NULL, pev );
		WRITE_BYTE( BlastAbilityLVL );
		WRITE_BYTE( BlastChargesReady );
		WRITE_BYTE( CanElectroBlast );
		MESSAGE_END();
		m_iClientBlastChargesReady = BlastChargesReady;
	}

	if( m_iClientBlastAbilityLVL != BlastAbilityLVL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBlastIcons, NULL, pev );
		WRITE_BYTE( BlastAbilityLVL );
		WRITE_BYTE( BlastChargesReady );
		WRITE_BYTE( CanElectroBlast );
		MESSAGE_END();
		m_iClientBlastAbilityLVL = BlastAbilityLVL;
	}

/*  //diffusion - no battery in game
	if (pev->armorvalue != m_iClientBattery)
	{
		m_iClientBattery = pev->armorvalue;

		ASSERT( gmsgBattery > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgBattery, NULL, pev );
			WRITE_SHORT( (int)pev->armorvalue);
		MESSAGE_END();
	}
*/
	if (pev->dmg_take || pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = GetAbsOrigin();
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		edict_t *other = pev->dmg_inflictor;
		if ( other )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(other);
			if ( pEntity )
				damageOrigin = pEntity->Center();
		}
		
		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;

		// diffusion - I think we need bounds for this, cause write_byte?
		int dmgTake = bound( 1, pev->dmg_take, 255 );
		int dmgSave = bound( 1, pev->dmg_save, 255 );

		// Send this player's damage to all his specators
		CBasePlayer* plr;
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			plr = (CBasePlayer*)UTIL_PlayerByIndex(i);
			if (!plr || !plr->IsObserver() || plr->m_hObserverTarget != this)
				continue;

			MESSAGE_BEGIN(MSG_ONE, gmsgDamage, NULL, plr->pev);
				WRITE_BYTE( dmgSave );
				WRITE_BYTE( dmgTake );
				WRITE_LONG(visibleDamageBits);
				WRITE_COORD(damageOrigin.x);
				WRITE_COORD(damageOrigin.y);
				WRITE_COORD(damageOrigin.z);
			MESSAGE_END();
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgDamage, NULL, pev );
			WRITE_BYTE( dmgSave );
			WRITE_BYTE( dmgTake );
			WRITE_LONG( visibleDamageBits );
			WRITE_COORD( damageOrigin.x );
			WRITE_COORD( damageOrigin.y );
			WRITE_COORD( damageOrigin.z );
		MESSAGE_END();
	
		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;
		
		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}

	// Update Flashlight
	if ((m_flFlashLightTime) && (m_flFlashLightTime <= gpGlobals->time))
	{
		if (FlashlightIsOn())
		{
			if (m_iFlashBattery)
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				m_iFlashBattery--;
				
				if (!m_iFlashBattery)
					FlashlightTurnOff();
			}
		}
		else
		{
			if (m_iFlashBattery < 100)
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
				m_iFlashBattery++;
			}
			else
				m_flFlashLightTime = 0;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
			if( pev->effects & EF_DIMLIGHT )
				WRITE_BYTE(1);
			else
				WRITE_BYTE(0);
			WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();
	}

	// calculate and update rain fading
	if( m_flRainEndFade > 0.0f )
	{
		if( gpGlobals->time < m_flRainEndFade )
		{
			// we're in fading process
			if( m_flRainNextFadeUpdate <= gpGlobals->time )
			{
				int secondsLeft = m_flRainEndFade - gpGlobals->time + 1;

				m_iRainDripsPerSecond += (m_iRainIdealDripsPerSecond - m_iRainDripsPerSecond) / secondsLeft;
				m_flRainWindX += (m_flRainIdealWindX - m_flRainWindX) / (float)secondsLeft;
				m_flRainWindY += (m_flRainIdealWindY - m_flRainWindY) / (float)secondsLeft;
				m_flRainRandX += (m_flRainIdealRandX - m_flRainRandX) / (float)secondsLeft;
				m_flRainRandY += (m_flRainIdealRandY - m_flRainRandY) / (float)secondsLeft;

				m_flRainNextFadeUpdate = gpGlobals->time + 1; // update once per second
				m_bRainNeedsUpdate = true;

				ALERT( at_aiconsole, "Rain fading: curdrips: %i, idealdrips %i\n", m_iRainDripsPerSecond, m_iRainIdealDripsPerSecond );
			}
		}
		else
		{
			// finish fading process
			m_flRainNextFadeUpdate = 0;
			m_flRainEndFade = 0;

			m_iRainDripsPerSecond = m_iRainIdealDripsPerSecond;
			m_flRainWindX = m_flRainIdealWindX;
			m_flRainWindY = m_flRainIdealWindY;
			m_flRainRandX = m_flRainIdealRandX;
			m_flRainRandY = m_flRainIdealRandY;

			m_bRainNeedsUpdate = true;

			ALERT( at_aiconsole, "Rain fading finished at %i drips\n", m_iRainDripsPerSecond );
		}
	}

	// send rain message
	if( m_bRainNeedsUpdate )
	{
		// search for env_rain entity
		CBaseEntity *pFind; 
		pFind = UTIL_FindEntityByClassname( NULL, "env_rain" );
		if( !FNullEnt( pFind ))
		{
			// rain allowed on this map
			CBaseEntity *pEnt = CBaseEntity::Instance( pFind->edict() );

			float raindistance = pEnt->pev->frags;
			float rainheight = pEnt->pev->origin[2];
			int rainmode = pEnt->pev->impulse;

			// diffusion - search for current active rain and get info from there
			if( pEnt->pev->iuser1 > gpGlobals->time ) // rain not yet faded
			{
				m_flRainEndFade = pEnt->pev->iuser1;
				m_iRainIdealDripsPerSecond = pEnt->pev->iuser2;
			}
			else
			{
				m_iRainDripsPerSecond = pEnt->pev->iuser2;
				m_iRainIdealDripsPerSecond = pEnt->pev->iuser2;
			}

			m_flRainNextFadeUpdate = gpGlobals->time + 1; // update once per second
			
			/*
			// search for constant rain_modifies
			pFind = UTIL_FindEntityByClassname( NULL, "env_rainmodify" );
			while( !FNullEnt( pFind ))
			{
				if( FBitSet( pFind->pev->spawnflags, 1 ))
				{
					// copy settings to player's data and clear fading
					m_iRainDripsPerSecond = pFind->pev->impulse;
					m_flRainWindX = pFind->pev->fuser1;
					m_flRainWindY = pFind->pev->fuser2;
					m_flRainRandX = pFind->pev->fuser3;
					m_flRainRandY = pFind->pev->fuser4;

					m_flRainEndFade = 0;
					break;
				}
				pFind = UTIL_FindEntityByClassname( pFind, "env_rainmodify" );
			}*/

			MESSAGE_BEGIN( MSG_ONE, gmsgRainData, NULL, pev );
				WRITE_SHORT( m_iRainDripsPerSecond );
				WRITE_COORD( raindistance );
				WRITE_COORD( m_flRainWindX );
				WRITE_COORD( m_flRainWindY );
				WRITE_COORD( m_flRainRandX );
				WRITE_COORD( m_flRainRandY );
				WRITE_SHORT( rainmode );
				WRITE_COORD( rainheight );
				WRITE_BYTE( pEnt->pev->iuser3 ); // 1 = use smoke particles on ground
			MESSAGE_END();

			if( m_iRainDripsPerSecond )
				ALERT( at_aiconsole, "Sending enabling rain message\n" );
			else
				ALERT( at_aiconsole, "Sending disabling rain message\n" );
		}
		else
		{
			// no rain on this map
			m_iRainDripsPerSecond = 0;
			m_flRainWindX = 0.0f;
			m_flRainWindY = 0.0f;
			m_flRainRandX = 0.0f;
			m_flRainRandY = 0.0f;
			m_iRainIdealDripsPerSecond = 0;
			m_flRainIdealWindX = 0;
			m_flRainIdealWindY = 0;
			m_flRainIdealRandX = 0;
			m_flRainIdealRandY = 0;
			m_flRainEndFade = 0;
			m_flRainNextFadeUpdate = 0;

			ALERT( at_aiconsole, "Clearing rain data\n" );
		}

		m_bRainNeedsUpdate = false;
	}

	if (m_iTrain & TRAIN_NEW)
	{
		ASSERT( gmsgTrain > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgTrain, NULL, pev );
			WRITE_BYTE(m_iTrain & 0xF);
		MESSAGE_END();

		m_iTrain &= ~TRAIN_NEW;
	}

	// New Weapon?
	if (!m_fKnownItem)
	{
		m_fKnownItem = TRUE;

	// WeaponInit Message
	// byte  = # of weapons
	//
	// for each weapon:
	// byte		name str length (not including null)
	// bytes... name
	// byte		Ammo Type
	// byte		Ammo2 Type
	// byte		bucket
	// byte		bucket pos
	// byte		flags	
	// ????		Icons
		
		// Send ALL the weapon info now
		int i;

		for (i = 0; i < MAX_WEAPONS; i++)
		{
			ItemInfo& II = CBasePlayerItem::ItemInfoArray[i];

			if ( !II.iId )
				continue;

			const char *pszName;
			if (!II.pszName)
				pszName = "Empty";
			else
				pszName = II.pszName;

			MESSAGE_BEGIN( MSG_ONE, gmsgWeaponList, NULL, pev );  
				WRITE_STRING(pszName);			// string	weapon name
				WRITE_BYTE(GetAmmoIndex(II.pszAmmo1));	// byte		Ammo Type
				WRITE_BYTE(II.iMaxAmmo1);				// byte     Max Ammo 1
				WRITE_BYTE(GetAmmoIndex(II.pszAmmo2));	// byte		Ammo2 Type
				WRITE_BYTE(II.iMaxAmmo2);				// byte     Max Ammo 2
				WRITE_BYTE(II.iSlot);					// byte		bucket
				WRITE_BYTE(II.iPosition);				// byte		bucket pos
				WRITE_BYTE(II.iId);						// byte		id (bit index into m_iWeapons)
				WRITE_BYTE(II.iFlags);					// byte		Flags
			MESSAGE_END();
		}
	}

	SendAmmoUpdate(pPlayer);

	// Update all the items
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )  // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData( this );
	}

	if (m_pClientActiveItem != pPlayer->m_pActiveItem)
	{
		if (pPlayer->m_pActiveItem == NULL)
		{
			// If no weapon, we have to send update here
			CBasePlayer* plr;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				plr = (CBasePlayer*)UTIL_PlayerByIndex(i);
				if (!plr || !plr->IsObserver() || plr->m_hObserverTarget != pPlayer)
					continue;

				MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, plr->pev);
				WRITE_BYTE(0);
				WRITE_BYTE(0);
				WRITE_BYTE(0);
				MESSAGE_END();
			}

			MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pPlayer->pev);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			MESSAGE_END();
		}
		else if (this != pPlayer)
		{
			// Special case for spectator
			CBasePlayerWeapon* gun = (CBasePlayerWeapon*)pPlayer->m_pActiveItem->GetWeaponPtr();
			if (gun)
			{
				int state;
				if (pPlayer->m_fOnTarget)
					state = WEAPON_IS_ONTARGET;
				else
					state = 1;

				MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
				WRITE_BYTE(state);
				WRITE_BYTE(gun->m_iId);
				WRITE_BYTE(gun->m_iClip);
				MESSAGE_END();
			}
		}
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	m_iClientFOV = (int)m_flFOV;

	// Update Status Bar
	if ( m_flNextSBarUpdateTime < gpGlobals->time )
	{
		UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.2;
	}
}

void CBasePlayer :: SendStartMessages( void )
{
	for( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if( !pEdict || pEdict->free || pEdict->v.flags & FL_KILLME )
			continue;

		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if( !pEntity ) continue;

		pEntity->StartMessage( this );
	}
}

//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
BOOL CBasePlayer :: FBecomeProne ( void )
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer :: BarnacleVictimBitten ( entvars_t *pevBarnacle )
{
	TakeDamage ( pevBarnacle, pevBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB );
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns. 
//=========================================================
void CBasePlayer :: BarnacleVictimReleased ( void )
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}


//=========================================================
// Illumination 
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer :: Illumination( void )
{
	int iIllum = CBaseEntity::Illumination( );

	iIllum += m_iWeaponFlash;
	if (iIllum > 255)
		return 255;
	return iIllum;
}


void CBasePlayer :: EnableControl(BOOL fControl)
{
	if (!fControl)
	{
		pev->flags |= FL_FROZEN;
		SetAbsVelocity( g_vecZero );
	}
	else
		pev->flags &= ~FL_FROZEN;

}

void CBasePlayer :: HideWeapons( BOOL fHideWeapons )
{
	if( fHideWeapons && (m_iHideHUD & HIDEHUD_WPNS) )
		return; // already hidden

	if( !fHideWeapons && !(m_iHideHUD & HIDEHUD_WPNS) )
		return; // already deployed

	// weapons were hidden by holdable item or player_customize
	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS_HOLDABLEITEM ) )
		return;

	if( FBitSet( m_iHideHUD, HIDEHUD_WPNS_CUSTOM ) )
		return;
	
	if( fHideWeapons )
	{
		if( m_pActiveItem )
		{
			m_pActiveItem->Holster(); // UNDONE - the gun doesn't have time to play holster anim!
			pev->viewmodel = 0;
		}

		m_iHideHUD |= HIDEHUD_WPNS;
	}
	else
	{
		m_iHideHUD &= ~HIDEHUD_WPNS;
		
		// bring back player's weapons
		if( m_pActiveItem )
			m_pActiveItem->Deploy();
	}
}

#define DOT_1DEGREE   0.9998476951564
#define DOT_2DEGREE   0.9993908270191
#define DOT_3DEGREE   0.9986295347546
#define DOT_4DEGREE   0.9975640502598
#define DOT_5DEGREE   0.9961946980917
#define DOT_6DEGREE   0.9945218953683
#define DOT_7DEGREE   0.9925461516413
#define DOT_8DEGREE   0.9902680687416
#define DOT_9DEGREE   0.9876883405951
#define DOT_10DEGREE  0.9848077530122
#define DOT_15DEGREE  0.9659258262891
#define DOT_20DEGREE  0.9396926207859
#define DOT_25DEGREE  0.9063077870367

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector CBasePlayer :: GetAutoaimVector( float flDelta )
{
	if (g_iSkillLevel == SKILL_HARD)
	{
		UTIL_MakeVectors( pev->v_angle + pev->punchangle );
		return gpGlobals->v_forward;
	}

	Vector vecSrc = GetGunPosition( );
	float flDist = 8192;

	// always use non-sticky autoaim
	// UNDONE: use server variable to chose!
	if (1 || g_iSkillLevel == SKILL_MEDIUM)
	{
		m_vecAutoAim = Vector( 0, 0, 0 );
		// flDelta *= 0.5;
	}

	BOOL m_fOldTargeting = m_fOnTarget;
	Vector angles = AutoaimDeflection(vecSrc, flDist, flDelta );

	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
		m_fOnTarget = 0;
	else if (m_fOldTargeting != m_fOnTarget)
		m_pActiveItem->UpdateItemInfo( );

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;


	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (0 || g_iSkillLevel == SKILL_EASY)
		m_vecAutoAim = m_vecAutoAim * 0.67 + angles * 0.33;
	else
		m_vecAutoAim = angles * 0.9;

	// m_vecAutoAim = m_vecAutoAim * 0.99;

	// Don't send across network if sv_aim is 0
	if ( g_psv_aim->value != 0 )
	{
		if ( m_vecAutoAim.x != m_lastx || m_vecAutoAim.y != m_lasty )
		{
			SET_CROSSHAIRANGLE( edict(), -m_vecAutoAim.x, m_vecAutoAim.y );
			
			m_lastx = m_vecAutoAim.x;
			m_lasty = m_vecAutoAim.y;
		}
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors( pev->v_angle + pev->punchangle + m_vecAutoAim );
	return gpGlobals->v_forward;
}


Vector CBasePlayer :: AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  )
{
	edict_t		*pEdict = INDEXENT( 1 );
	CBaseEntity	*pEntity;
	float		bestdot;
	Vector		bestdir;
	edict_t		*bestent;
	TraceResult tr;

	if ( g_psv_aim->value == 0 )
	{
		ResetAutoaim ();
		return g_vecZero;
	}

	UTIL_MakeVectors( pev->v_angle + pev->punchangle + m_vecAutoAim );

	// try all possible entities
	bestdir = gpGlobals->v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	m_fOnTarget = FALSE;

	UTIL_TraceLine( vecSrc, vecSrc + bestdir * flDist, dont_ignore_monsters, edict(), &tr );


	if ( tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
	{
		// don't look through water
		if (!((pev->waterlevel != 3 && tr.pHit->v.waterlevel == 3) 
			|| (pev->waterlevel == 3 && tr.pHit->v.waterlevel == 0)))
		{
			if (tr.pHit->v.takedamage == DAMAGE_AIM)
				m_fOnTarget = TRUE;

			return m_vecAutoAim;
		}
	}

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		Vector center;
		Vector dir;
		float dot;

		if ( pEdict->free )	// Not in use
			continue;
		
		if (pEdict->v.takedamage != DAMAGE_AIM)
			continue;
		if (pEdict == edict())
			continue;
//		if (pev->team > 0 && pEdict->v.team == pev->team)
//			continue;	// don't aim at teammate
		if ( !g_pGameRules->ShouldAutoAim( this, pEdict ) )
			continue;

		pEntity = Instance( pEdict );
		if (pEntity == NULL)
			continue;

		if (!pEntity->IsAlive())
			continue;

		// don't look through water
		if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3) 
			|| (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
			continue;

		center = pEntity->BodyTarget( vecSrc );

		dir = (center - vecSrc).Normalize( );

		// make sure it's in front of the player
		if (DotProduct (dir, gpGlobals->v_forward ) < 0)
			continue;

		dot = fabs( DotProduct (dir, gpGlobals->v_right ) ) 
			+ fabs( DotProduct (dir, gpGlobals->v_up ) ) * 0.5;

		// tweek for distance
		dot *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);

		if (dot > bestdot)
			continue;	// to far to turn

		UTIL_TraceLine( vecSrc, center, dont_ignore_monsters, edict(), &tr );
		if (tr.flFraction != 1.0 && tr.pHit != pEdict)
		{
			// ALERT( at_console, "hit %s, can't see %s\n", STRING( tr.pHit->v.classname ), STRING( pEdict->v.classname ) );
			continue;
		}

		// don't shoot at friends
		if (IRelationship( pEntity ) < 0)
		{
			if ( !pEntity->IsPlayer() && !g_pGameRules->IsDeathmatch())
				// ALERT( at_console, "friend\n");
				continue;
		}

		// can shoot at this one
		bestdot = dot;
		bestent = pEdict;
		bestdir = dir;
	}

	if (bestent)
	{
		bestdir = UTIL_VecToAngles (bestdir);
		bestdir = bestdir - pev->v_angle - pev->punchangle;

		if (bestent->v.takedamage == DAMAGE_AIM)
			m_fOnTarget = TRUE;

		return bestdir;
	}

	return Vector( 0, 0, 0 );
}

void CBasePlayer :: ResetAutoaim( void )
{
	if( m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0 )
	{
		SET_CROSSHAIRANGLE( edict(), 0, 0 );
		m_vecAutoAim = g_vecZero;
		m_lastx = 0;
	}

	m_fOnTarget = FALSE;
}

/*
=============
SetCustomDecalFrames

  UNDONE:  Determine real frame limit, 8 is a placeholder.
  Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer :: SetCustomDecalFrames( int nFrames )
{
	if (nFrames > 0 &&
		nFrames < 8)
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer :: GetCustomDecalFrames( void )
{
	return m_nCustomSprayFrames;
}


//=========================================================
// DropPlayerItem - drop the named item, or if no name,
// the active item. // BugFixedHL by Lev
//=========================================================
void CBasePlayer::DropPlayerItem(const char* pszItemName)
{
	if (!g_pGameRules->IsMultiplayer() || (weaponstay.value > 0))
	{
		// no dropping in single player, only with cheats.
		if( !g_flWeaponCheat )
			return;
	}

	CBasePlayerItem* pWeaponItem;

	if( pszItemName[0] == '\0' )
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		pWeaponItem = m_pActiveItem;
	}
	else
	{
		// try to match by name.
		bool match = false;
		for (int i = 0; i < MAX_ITEM_TYPES && !match; i++)
		{
			pWeaponItem = m_rgpPlayerItems[i];
			while ( pWeaponItem )
			{
				if (!strcmp(pszItemName, STRING( pWeaponItem->pev->classname)))
				{
					match = true;
					break;
				}

				pWeaponItem = pWeaponItem->m_pNext;
			}
		}
		if (!match)
			return;
	}

	// Return if we didn't find a weapon to drop
	if (!pWeaponItem ) return;

	g_pGameRules->GetNextBestWeapon(this, pWeaponItem );

//	UTIL_MakeVectors(pev->angles);
	UTIL_MakeVectors( pev->v_angle );

/*	CWeaponBox* pWeaponBox = (CWeaponBox*)CBaseEntity::Create("weaponbox", pev->origin + gpGlobals->v_forward * 10, pev->angles, edict());
	pWeaponBox->pev->angles.x = 0;
	pWeaponBox->pev->angles.z = 0;
	pWeaponBox->PackWeapon(pWeapon);
	pWeaponBox->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;

	// drop half of the ammo for this weapon.
	int iAmmoIndex = GetAmmoIndex(pWeapon->pszAmmo1()); // ???

	if( iAmmoIndex != -1 )
	{
		// this weapon weapon uses ammo, so pack an appropriate amount.
		if( pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE )
		{
			// pack up all the ammo, this weapon is its own ammo type
			pWeaponBox->PackAmmo( MAKE_STRING( pWeapon->pszAmmo1() ), m_rgAmmo[iAmmoIndex] );
			m_rgAmmo[iAmmoIndex] = 0;
		}
		else
		{
			// pack half of the ammo
			int ammoDrop = m_rgAmmo[iAmmoIndex] / 2;
			pWeaponBox->PackAmmo( MAKE_STRING( pWeapon->pszAmmo1() ), ammoDrop );
			m_rgAmmo[iAmmoIndex] -= ammoDrop;
		}
	}
*/

	// diffusion - drop actual weapon
	CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)pWeaponItem;

	if( pWeapon && pWeapon->m_iId == m_iBonusWeaponID )
	{
		// don't drop the gun you got as a bonus after death
		m_iBonusWeaponID = 0;
		return;
	}

	// don't drop knife
	if( FClassnameIs(pWeaponItem, "weapon_knife" ) )
		return;
	else if( FClassnameIs( pWeaponItem, "weapon_satchel" ) )
	{
		// don't drop satchels if the player deployed all of them
		if( m_rgAmmo[GetAmmoIndex( pWeaponItem->pszAmmo1() )] <= 0 )
			return;
	}
	else if( FClassnameIs( pWeaponItem, "weapon_handgrenade" ) )
	{
		// same goes for grenades
		if( m_rgAmmo[GetAmmoIndex( pWeaponItem->pszAmmo1() )] <= 0 )
			return;
	}

	CBasePlayerWeapon *pDrop = (CBasePlayerWeapon*)CBaseEntity::Create((char*)STRING( pWeaponItem->pev->classname), pev->origin + gpGlobals->v_forward * 10, pev->angles, edict() );

	m_iBonusWeaponID = 0;

	if( pDrop )
	{
		if( FClassnameIs(pDrop, "weapon_satchel") )
			DeactivateSatchels( this );
		
		pDrop->SetAbsAngles( Vector( pev->angles.x, RANDOM_LONG( 0, 359 ), pev->angles.z ) );
		pDrop->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;
		pDrop->pev->renderfx = kRenderFxGlowShell;
		pDrop->pev->spawnflags |= SF_NORESPAWN;// never respawn
		pDrop->SetFadeDistance( 700 );
		pDrop->SetFlag( F_WEAPON_DESPAWN );
		pDrop->m_flDelay = gpGlobals->time + 30.0f; // if the weapon stuck in the air for this long, assume it landed - don't keep it forever

		int iAmmoIndex = GetAmmoIndex( pWeaponItem->pszAmmo1() );
		if( iAmmoIndex != -1 )
		{
			// pack up all ammo
			pDrop->m_iClip = pWeapon->m_iClip;
			pDrop->m_iDefaultAmmo = m_rgAmmo[iAmmoIndex];
			m_rgAmmo[iAmmoIndex] = 0;
		}
		iAmmoIndex = GetAmmoIndex( pWeaponItem->pszAmmo2() );
		if( iAmmoIndex != -1 )
		{
			// pack up all ammo
			pDrop->m_iDefaultAmmo2 = m_rgAmmo[iAmmoIndex];
			m_rgAmmo[iAmmoIndex] = 0;
		}
		RemoveWeapon( pWeaponItem->m_iId );
		pWeaponItem->SetThink( &CBasePlayerItem::DestroyItem );
		pWeaponItem->SetNextThink( 0.1 );
	}
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];
	
	while (pItem)
	{
		if( FClassnameIs( pItem->pev, STRING(pCheckItem->pev->classname) ))
			return TRUE;

		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasNamedPlayerItem( const char *pszItemName )
{
	CBasePlayerItem *pItem;
	int i;
 
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pItem = m_rgpPlayerItems[ i ];
		
		while (pItem)
		{
			if ( !strcmp( pszItemName, STRING( pItem->pev->classname ) ) )
				return TRUE;

			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}

//=========================================================
// 
//=========================================================
BOOL CBasePlayer :: SwitchWeapon( CBasePlayerItem *pWeapon ) 
{
	if ( !pWeapon->CanDeploy() )
		return FALSE;
	
	ResetAutoaim( );
	
	if( m_pActiveItem )
	{
		m_pLastItem = m_pActiveItem;
		m_pActiveItem->Holster();
	}

	m_pActiveItem = pWeapon;
	pWeapon->Deploy( );

	return TRUE;
}

//=============================================================================
// diffusion - show a tutorial sprite when player picks up a new weapon.
//=============================================================================
void CBasePlayer::CheckTutorMessage( int m_iId )
{
//	ALERT(at_console, "got %3d\n", m_iId );

	if( g_pGameRules->IsMultiplayer() )
		return;

	// player picks up new weapons very quickly (impulse 101?)
	// don't spam the tutors
	if( gpGlobals->time < m_flLastTimeTutorWasShown + 3 )
		return;

	// player already had this weapon before. no need to show tutor
	if( HadWeapon[m_iId] )
		return;

	char TempString[32] = "error"; // default
	char TempPic[128];
	TempPic[0] = '\0';

	switch( m_iId )
	{
		case WEAPON_KNIFE:		Q_snprintf( TempString, sizeof( TempString ), "tutor_knife" ); break;
		case WEAPON_BERETTA:	Q_snprintf( TempString, sizeof( TempString ), "tutor_pistol"); break;
		case WEAPON_DEAGLE:		Q_snprintf( TempString, sizeof( TempString ), "tutor_deagle"); break;
		case WEAPON_MRC:		Q_snprintf( TempString, sizeof( TempString ), "tutor_mrc"); break;
		case WEAPON_CROSSBOW:	Q_snprintf( TempString, sizeof( TempString ), "tutor_crossbow"); break;
		case WEAPON_SHOTGUN:	Q_snprintf( TempString, sizeof( TempString ), "tutor_shotgun"); break;
		case WEAPON_SHOTGUN_XM:	Q_snprintf( TempString, sizeof( TempString ), "tutor_shotgun_xm" ); break;
		case WEAPON_RPG:		Q_snprintf( TempString, sizeof( TempString ), "tutor_rpg"); break;
		case WEAPON_GAUSS:		Q_snprintf( TempString, sizeof( TempString ), "tutor_gausniper"); break;
		case WEAPON_EGON:		Q_snprintf( TempString, sizeof( TempString ), "tutor_egon"); break; // not used
		case WEAPON_HORNETGUN:	Q_snprintf( TempString, sizeof( TempString ), "tutor_hornet"); break; // not used
		case WEAPON_HANDGRENADE: Q_snprintf( TempString, sizeof( TempString ), "tutor_grenade"); break;
		case WEAPON_TRIPMINE:	Q_snprintf( TempString, sizeof( TempString ), "tutor_tripmine"); break;
		case WEAPON_SATCHEL:	Q_snprintf( TempString, sizeof( TempString ), "tutor_satchel"); break;
		case WEAPON_SNARK:		Q_snprintf( TempString, sizeof( TempString ), "tutor_snark"); break; // not used
		case WEAPON_AR2:		Q_snprintf( TempString, sizeof( TempString ), "tutor_ar2"); break;
		case WEAPON_DRONE:		Q_snprintf( TempString, sizeof( TempString ), "tutor_drone"); break;
		case WEAPON_SENTRY:		Q_snprintf( TempString, sizeof( TempString ), "tutor_sentry"); break;
		case WEAPON_HKMP5:		Q_snprintf( TempString, sizeof( TempString ), "tutor_hkmp5"); break;
		case WEAPON_FIVESEVEN:	Q_snprintf( TempString, sizeof( TempString ), "tutor_fiveseven"); break;
		case WEAPON_SNIPER:		Q_snprintf( TempString, sizeof( TempString ), "tutor_sniper" ); break;
		case WEAPON_G36C:		Q_snprintf( TempString, sizeof( TempString ), "tutor_g36c" ); break;
	}

	if( !FStrEq( TempString, "error" ) )
		Q_snprintf( TempPic, sizeof( TempPic ), "textures/!tutor/%s", TempString );

	MESSAGE_BEGIN( MSG_ONE, gmsgStatusIconTutor, NULL, pev );
		WRITE_STRING( TempString );
		WRITE_STRING( TempPic );
	MESSAGE_END();

	m_flLastTimeTutorWasShown = gpGlobals->time;
}

//=============================================================================
// diffusion - check for a completed achievement every few seconds
//=============================================================================
void CBasePlayer::CheckVehicleAchievement(void)
{
	if( gpGlobals->time < AchievementCheckTime )
		return;

	if( AchievementStats[ACH_CARDISTANCE] > 25 ) // send it only once in 25m
	{
		SendAchievementStatToClient( ACH_CARDISTANCE, (int)AchievementStats[ACH_CARDISTANCE], ACHVAL_ADD );
		AchievementStats[ACH_CARDISTANCE] = 0;
	}

	if( AchievementStats[ACH_WATERJETDISTANCE] > 25 ) // send it only once in 25m
	{
		SendAchievementStatToClient( ACH_WATERJETDISTANCE, (int)AchievementStats[ACH_WATERJETDISTANCE], ACHVAL_ADD );
		AchievementStats[ACH_WATERJETDISTANCE] = 0;
	}

	AchievementCheckTime = gpGlobals->time + ACHIEVEMENT_CHECK_TIME;
}

void CBasePlayer::SendAchievementStatToClient( int AchNum, int Value, int Mode )
{
	if( HasFlag( F_BOT ) )
		return;

	if( bCheatsWereUsed || g_flWeaponCheat != 0.0 )
	{
	//	ALERT( at_aiconsole, "Achievement: %i, value: %i. NOT SENT - cheats enabled.\n", AchNum, Value );
		return;
	}
	
	// 3 modes: 0: +Value, 1: -Value, 2: =Value 
	
	MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, pev );
		WRITE_BYTE( TE_ACHIEVEMENT );
		WRITE_BYTE( AchNum );
		WRITE_LONG( Value );
		WRITE_BYTE( Mode );
	MESSAGE_END();

//	ALERT( at_aiconsole, "Achievement sent: %i, value sent: %i\n", AchNum, Value );
}
