/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// monsterstate.cpp - base class monster functions for 
// controlling core AI.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "monsters.h"
#include "animation.h"
#include "game/saverestore.h"
#include "entities/soundent.h"
#include "weapons/weapons.h"
#include "player.h"

//=========================================================
// SetState
//=========================================================
void CBaseMonster :: SetState ( MONSTERSTATE State )
{

//	if ( State != m_MonsterState )
//	{
//		ALERT ( at_aiconsole, "State Changed to %d\n", State );
//	}

	
	switch( State )
	{
	
	// Drop enemy pointers when going to idle
	case MONSTERSTATE_IDLE:

		if ( m_hEnemy != NULL )
		{
			m_hEnemy = NULL;// not allowed to have an enemy anymore.
			ALERT ( at_aiconsole, "SetState: m_hEnemy stripped\n" );
		}
		break;
	}

	m_MonsterState = State;
	m_IdealMonsterState = State;
}

//=========================================================
// RunAI
//=========================================================
void CBaseMonster :: RunAI ( void )
{
	// to test model's eye height
//	UTIL_ParticleEffect ( EyePosition(), g_vecZero, 255, 10 );
	
	if( gpGlobals->time > NextUpdateTime )
	{
		if( !CheckingLocalMove )
		{
			Vector Org = GetAbsOrigin();
			UTIL_SetOrigin( this, Org );
		}
		
		CheckFire();

		// IDLE sound permitted in ALERT state is because monsters were silent in ALERT state. Only play IDLE sound in IDLE state
		// once we have sounds for that state.
		if( (m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT) && RANDOM_LONG( 0, 99 ) == 0 && !(pev->spawnflags & SF_MONSTER_GAG) )
			IdleSound();

		if( m_MonsterState != MONSTERSTATE_NONE &&
			m_MonsterState != MONSTERSTATE_PRONE &&
			m_MonsterState != MONSTERSTATE_DEAD
			)// don't bother with this crap if monster is prone. 
		{
			// collect some sensory Condition information.
			// don't let monsters outside of the player's PVS act up, or most of the interesting
			// things will happen before the player gets there!
			// UPDATE: We now let COMBAT state monsters think and act fully outside of player PVS. This allows the player to leave 
			// an area where monsters are fighting, and the fight will continue.
			if( !FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) || (m_MonsterState == MONSTERSTATE_COMBAT) )
			{
				Look( m_flDistLook );
				Listen();// check for audible sounds. 

				// check for low health
				if( pev->max_health > 0 ) // make sure this is valid
				{
					if( !HasConditions( bits_COND_LOW_HEALTH ) && (pev->health <= (pev->max_health * 0.25f)) )
						SetConditions( bits_COND_LOW_HEALTH );
					else if( HasConditions( bits_COND_LOW_HEALTH ) && (pev->health > (pev->max_health * 0.25f)) )
						ClearConditions( bits_COND_LOW_HEALTH ); // regenerated? clear condition
				}

				// now filter conditions.
				ClearConditions( IgnoreConditions() );

				GetEnemy();
			}

			// do these calculations if monster has an enemy.
			if( m_hEnemy != NULL )
				CheckEnemy( m_hEnemy );

			CheckAmmo();
		}

		NextUpdateTime = gpGlobals->time + 0.1;
	}

	FCheckAITrigger();
	PrescheduleThink();
	MaintainSchedule();

	// if the monster didn't use these conditions during the above call to MaintainSchedule() or CheckAITrigger()
	// we throw them out cause we don't want them sitting around through the lifespan of a schedule
	// that doesn't use them. 
	m_afConditions &= ~(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE);
}

// ==========================================================================
// diffusion - check if the monster is on fire, set or remove needed effects
// ==========================================================================
void CBaseMonster::CheckFire(void)
{
	if( HasFlag( F_FIRE_IMMUNE ) )
	{
		// we are immune. so if we get the flag, get rid of it, just in case
		if( HasFlag( F_ENTITY_ONFIRE ) )
			RemoveFlag( F_ENTITY_ONFIRE );

		return;
	}
	
	// monster got the flag, set him on fire
	if( HasFlag(F_ENTITY_ONFIRE) && !IsOnFire )
	{
		m_flCaughtFireTime = gpGlobals->time;
		IsOnFire = true;
		pev->effects |= EF_DIMLIGHT;
		pev->iuser3 = -667; // we need to distinguish this light on client to disable shadows!
	}

	// do stuff while burning, disable when the time runs out
	if( IsOnFire )
	{
		if( (gpGlobals->time > m_flCaughtFireTime + 7) || (pev->waterlevel > 0) )
		{
			pev->effects &= ~EF_DIMLIGHT;
			IsOnFire = false;
			RemoveFlag(F_ENTITY_ONFIRE);
			pev->iuser3 = 0;
		}
		else
		{
			Vector vecOrg = GetAbsOrigin();
			vecOrg.z += RANDOM_LONG(35,50);
			MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrg );
				WRITE_BYTE( TE_FIRE );
				WRITE_COORD( vecOrg.x + RANDOM_LONG(-10,10) );
				WRITE_COORD( vecOrg.y + RANDOM_LONG(-10,10));
				WRITE_COORD( vecOrg.z );
				WRITE_SHORT( g_sModelIndexFire );
				WRITE_BYTE( RANDOM_LONG(2,7) ); // scale x1.0
				WRITE_BYTE( RANDOM_LONG(10,20) ); // framerate
			MESSAGE_END();

			TakeDamage(VARS(eoNullEntity),VARS(eoNullEntity),0.5,DMG_BURN);
		}
	}
}

//=========================================================
// GetIdealState - surveys the Conditions information available
// and finds the best new state for a monster.
//=========================================================
MONSTERSTATE CBaseMonster :: GetIdealState ( void )
{
	int	iConditions;
	
	iConditions = IScheduleFlags();
	
	// If no schedule conditions, the new ideal state is probably the reason we're in here.
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
		
		/*
		IDLE goes to ALERT upon hearing a sound
		-IDLE goes to ALERT upon being injured
		IDLE goes to ALERT upon seeing food
		-IDLE goes to COMBAT upon sighting an enemy
		IDLE goes to HUNT upon smelling food
		*/
		{
			if ( iConditions & bits_COND_NEW_ENEMY )			
			{
				// new enemy! This means an idle monster has seen someone it dislikes, or 
				// that a monster in combat has found a more suitable target to attack
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
			else if ( iConditions & bits_COND_LIGHT_DAMAGE )
			{
				MakeIdealYaw ( m_vecEnemyLKP );
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}
			else if ( iConditions & bits_COND_HEAVY_DAMAGE )
			{
				MakeIdealYaw ( m_vecEnemyLKP );
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}
			else if ( iConditions & bits_COND_HEAR_SOUND )
			{
				CSound *pSound;
				
				pSound = PBestSound();
				ASSERT( pSound != NULL );
				if ( pSound && !HasSpawnFlags(SF_MONSTER_PRISONER) && !(HasFlag( F_PLAYER_CONTROL )) ) // diffusion - do NOT turn when I set this, FFS!
				{
					MakeIdealYaw ( pSound->m_vecOrigin );
					if ( pSound->m_iType & (bits_SOUND_COMBAT|bits_SOUND_DANGER) )
						m_IdealMonsterState = MONSTERSTATE_ALERT;
				}
			}
			else if ( iConditions & (bits_COND_SMELL | bits_COND_SMELL_FOOD) )
			{
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}

			break;
		}
	case MONSTERSTATE_ALERT:
		/*
		ALERT goes to IDLE upon becoming bored
		-ALERT goes to COMBAT upon sighting an enemy
		ALERT goes to HUNT upon hearing a noise
		*/
		{
			if ( iConditions & (bits_COND_NEW_ENEMY|bits_COND_SEE_ENEMY) )			
			{
				// see an enemy we MUST attack
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
			else if ( iConditions & bits_COND_HEAR_SOUND )
			{
				m_IdealMonsterState = MONSTERSTATE_ALERT;
				CSound *pSound = PBestSound();
				ASSERT( pSound != NULL );
				if ( pSound && !HasSpawnFlags( SF_MONSTER_PRISONER ) && !(HasFlag( F_PLAYER_CONTROL )) )
					MakeIdealYaw ( pSound->m_vecOrigin );
			}
			break;
		}
	case MONSTERSTATE_COMBAT:
		/*
		COMBAT goes to HUNT upon losing sight of enemy
		COMBAT goes to ALERT upon death of enemy
		*/
		{
			if ( m_hEnemy == NULL )
			{
				m_IdealMonsterState = MONSTERSTATE_ALERT;
				// pev->effects = EF_BRIGHTFIELD;
				ALERT ( at_aiconsole, "***Combat state with no enemy!\n" );
			}
			break;
		}
	case MONSTERSTATE_HUNT:
		/*
		HUNT goes to ALERT upon seeing food
		HUNT goes to ALERT upon being injured
		HUNT goes to IDLE if goal touched
		HUNT goes to COMBAT upon seeing enemy
		*/
		{
			break;
		}
	case MONSTERSTATE_SCRIPT:
		if ( iConditions & (bits_COND_TASK_FAILED|bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE) )
		{
			ExitScriptedSequence();	// This will set the ideal state
		}
		break;

	case MONSTERSTATE_DEAD:
		m_IdealMonsterState = MONSTERSTATE_DEAD;
		break;
	}

	return m_IdealMonsterState;
}

