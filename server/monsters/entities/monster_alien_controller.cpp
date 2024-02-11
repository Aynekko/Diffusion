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
// CONTROLLER
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"effects.h"
#include	"game/schedule.h"
#include	"weapons/weapons.h"
#include	"squadmonster.h"

// CDrone
#include	"entities/soundent.h"
#include	"customentity.h"
#include	"explode.h"
#include	"entities/func_break.h"
#include	"player.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define CONTROLLER_AE_HEAD_OPEN	1
#define CONTROLLER_AE_BALL_SHOOT	2
#define CONTROLLER_AE_SMALL_SHOOT	3
#define CONTROLLER_AE_POWERUP_FULL	4
#define CONTROLLER_AE_POWERUP_HALF	5

#define CONTROLLER_FLINCH_DELAY	2		// at most one flinch every n secs

const float DroneDmg[] =
{
	0.0f,
	0.8f, // easy
	1.0f, // medium
	1.2f // hard
};

const int ProjectileVelocity[] =
{
	0,
	2000,
	2500,
	3000
};

const int RocketDroneHealth[] =
{
	0,
	250,
	325,
	400
};

class CController : public CSquadMonster
{
	DECLARE_CLASS( CController, CSquadMonster );
public:
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	void PrescheduleThink( void );
	void RunAI( void );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );	// balls
	BOOL CheckRangeAttack2 ( float flDot, float flDist );	// head
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );	// block, throw
	Schedule_t* GetSchedule ( void );
	Schedule_t* GetScheduleOfType ( int Type );
	void StartTask ( Task_t *pTask );
	void RunTask ( Task_t *pTask );
	CUSTOM_SCHEDULES;

	void Stop( void );
	void Move ( float flInterval );
	int  CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist );
	void MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval );
	void SetActivity ( Activity NewActivity );
	BOOL ShouldAdvanceRoute( float flWaypointDist );
	int LookupFloat( );

	float m_flNextFlinch;

	float m_flShootTime;
	float m_flShootEnd;

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void DeathSound( void );

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );
	void ClearEffects(void);

	CSprite *m_pBall[2];	// hand balls
	int m_iBall[2];			// how bright it should be
	float m_iBallTime[2];	// when it should be that color
	int m_iBallCurrent[2];	// current brightness

	Vector m_vecEstVelocity;

	Vector m_velocity;
	int m_fInCombat;

	int m_iSoundState;

	int CanInvestigate;
	CSprite *AlienEye;
};

LINK_ENTITY_TO_CLASS( monster_alien_controller, CController );

BEGIN_DATADESC( CController )
	DEFINE_AUTO_ARRAY( m_pBall, FIELD_CLASSPTR ),
	DEFINE_AUTO_ARRAY( m_iBall, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iBallTime, FIELD_TIME ),
	DEFINE_AUTO_ARRAY( m_iBallCurrent, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecEstVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CanInvestigate, FIELD_INTEGER ),
	DEFINE_FIELD( AlienEye, FIELD_CLASSPTR ),
END_DATADESC()

const char *CController::pAttackSounds[] = 
{
	"controller/con_attack1.wav",
	"controller/con_attack2.wav",
	"controller/con_attack3.wav",
};

const char *CController::pIdleSounds[] = 
{
	"controller/con_idle1.wav",
	"controller/con_idle2.wav",
	"controller/con_idle3.wav",
	"controller/con_idle4.wav",
	"controller/con_idle5.wav",
};

const char *CController::pAlertSounds[] = 
{
	"controller/con_alert1.wav",
	"controller/con_alert2.wav",
	"controller/con_alert3.wav",
};

const char *CController::pPainSounds[] = 
{
	"controller/con_pain1.wav",
	"controller/con_pain2.wav",
	"controller/con_pain3.wav",
};

const char *CController::pDeathSounds[] = 
{
	"controller/con_die1.wav",
	"controller/con_die2.wav",
};


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CController :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_ALIEN_MILITARY;
}

void CController::PrescheduleThink( void )
{
	if( InSquad() && m_hEnemy != NULL )
	{
		if( HasConditions( bits_COND_SEE_ENEMY ) )
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time; // update the squad's last enemy sighting time.
		else
		{
			if( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
				MySquadLeader()->m_fEnemyEluded = TRUE; // been a while since we've seen the enemy
		}
	}
}


//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CController :: SetYawSpeed ( void )
{
	int ys;

	ys = 120;

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys;
}

int CController :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();

	if( m_hEnemy == NULL )
	{
		MakeIdealYaw( pevAttacker->origin );
	//	ChangeYaw( pev->yaw_speed );
		SetTurnActivity();
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}


void CController::Killed( entvars_t *pevAttacker, int iGib )
{
	// shut off balls
	/*
	m_iBall[0] = 0;
	m_iBallTime[0] = gpGlobals->time + 4.0;
	m_iBall[1] = 0;
	m_iBallTime[1] = gpGlobals->time + 4.0;
	*/
	CSquadMonster::Killed( pevAttacker, iGib );
}

void CController::ClearEffects(void)
{
	// fade balls
	if (m_pBall[0])
	{
		m_pBall[0]->SUB_StartFadeOut();
		m_pBall[0] = NULL;
	}
	if (m_pBall[1])
	{
		m_pBall[1]->SUB_StartFadeOut();
		m_pBall[1] = NULL;
	}
}


void CController::GibMonster( void )
{
	// delete balls
	if (m_pBall[0])
	{
		UTIL_Remove( m_pBall[0] );
		m_pBall[0] = NULL;
	}
	if (m_pBall[1])
	{
		UTIL_Remove( m_pBall[1] );
		m_pBall[1] = NULL;
	}
	CSquadMonster::GibMonster( );
}




void CController :: PainSound( void )
{
	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pPainSounds ); 
}	

void CController :: AlertSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pAlertSounds ); 
}

void CController :: IdleSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pIdleSounds ); 
}

void CController :: AttackSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pAttackSounds ); 
}

void CController :: DeathSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pDeathSounds ); 
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CController :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CONTROLLER_AE_HEAD_OPEN:
		{
			Vector vecStart, angleGun;
			
			GetAttachment( 0, vecStart, angleGun );
			
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_ELIGHT );
				WRITE_SHORT( entindex( ) + 0x1000 );		// entity, attachment
				WRITE_COORD( vecStart.x );		// origin
				WRITE_COORD( vecStart.y );
				WRITE_COORD( vecStart.z );
				WRITE_COORD( 1 );	// radius
				WRITE_BYTE( 255 );	// R
				WRITE_BYTE( 192 );	// G
				WRITE_BYTE( 64 );	// B
				WRITE_BYTE( 20 );	// life * 10
				WRITE_COORD( -32 ); // decay
			MESSAGE_END();

			m_iBall[0] = 192;
			m_iBallTime[0] = gpGlobals->time + Q_atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 255;
			m_iBallTime[1] = gpGlobals->time + Q_atoi( pEvent->options ) / 15.0;

		}
		break;

		case CONTROLLER_AE_BALL_SHOOT:
		{
			Vector vecStart, angleGun;
			
			GetAttachment( 0, vecStart, angleGun );

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_ELIGHT );
				WRITE_SHORT( entindex( ) + 0x1000 );		// entity, attachment
				WRITE_COORD( 0 );		// origin
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );
				WRITE_COORD( 32 );	// radius
				WRITE_BYTE( 255 );	// R
				WRITE_BYTE( 192 );	// G
				WRITE_BYTE( 64 );	// B
				WRITE_BYTE( 10 );	// life * 10
				WRITE_COORD( 32 ); // decay
			MESSAGE_END();

			CBaseMonster *pBall = (CBaseMonster*)Create( "controller_head_ball", vecStart, GetAbsAngles(), edict() );

			pBall->SetAbsVelocity( Vector( 0, 0, 32 ));
			pBall->m_hEnemy = m_hEnemy;

			m_iBall[0] = 0;
			m_iBall[1] = 0;
		}
		break;

		case CONTROLLER_AE_SMALL_SHOOT:
		{
			AttackSound( );
			m_flShootTime = gpGlobals->time;
			m_flShootEnd = m_flShootTime + Q_atoi( pEvent->options ) / 15.0;
		}
		break;
		case CONTROLLER_AE_POWERUP_FULL:
		{
			m_iBall[0] = 255;
			m_iBallTime[0] = gpGlobals->time + Q_atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 255;
			m_iBallTime[1] = gpGlobals->time + Q_atoi( pEvent->options ) / 15.0;
		}
		break;
		case CONTROLLER_AE_POWERUP_HALF:
		{
			m_iBall[0] = 192;
			m_iBallTime[0] = gpGlobals->time + Q_atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 192;
			m_iBallTime[1] = gpGlobals->time + Q_atoi( pEvent->options ) / 15.0;
		}
		break;
		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CController :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/controller.mdl");
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 64 ));

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	pev->flags			|= FL_FLY;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	if (!pev->health) pev->health	= gSkillData.controllerHealth;
	pev->view_ofs		= Vector( 0, 0, -2 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_FULL;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	if( RANDOM_LONG(0,2) == 0)
		CanInvestigate = 1;
	else
		CanInvestigate = 0;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CController :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/controller.mdl");

	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDeathSounds );

	PRECACHE_MODEL( "sprites/xspark4.spr");

	UTIL_PrecacheOther( "controller_energy_ball" );
	UTIL_PrecacheOther( "controller_head_ball" );
}	

//=========================================================
// AI Schedules Specific to this monster
//=========================================================


// Chase enemy schedule
Task_t tlControllerChaseEnemy[] = 
{
	{ TASK_GET_PATH_TO_ENEMY,	(float)128		},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},

};

Schedule_t slControllerChaseEnemy[] =
{
	{ 
		tlControllerChaseEnemy,
		SIZEOFARRAY ( tlControllerChaseEnemy ),
		bits_COND_NEW_ENEMY			|
		bits_COND_TASK_FAILED,
		0,
		"ControllerChaseEnemy"
	},
};


// CDrone follow owner
Task_t tlDroneChaseOwner[] = 
{
	{ TASK_MOVE_TO_TARGET_RANGE,	(float)100	},
};

Schedule_t slDroneChaseOwner[] =
{
	{ 
		tlDroneChaseOwner,
		SIZEOFARRAY ( tlDroneChaseOwner ),
		bits_COND_NEW_ENEMY,
		bits_SOUND_DANGER,
		"DroneChaseOwner"
	},
};



Task_t	tlControllerStrafe[] =
{
	{ TASK_WAIT,					(float)0.2					},
	{ TASK_GET_PATH_TO_ENEMY,		(float)128					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_WAIT,					(float)1					},
};

Schedule_t	slControllerStrafe[] =
{
	{ 
		tlControllerStrafe,
		SIZEOFARRAY ( tlControllerStrafe ), 
		bits_COND_NEW_ENEMY,
		0,
		"ControllerStrafe"
	},
};


Task_t	tlControllerTakeCover[] =
{
	{ TASK_WAIT,					(float)0.2					},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_WAIT,					(float)1					},
};

Schedule_t	slControllerTakeCover[] =
{
	{ 
		tlControllerTakeCover,
		SIZEOFARRAY ( tlControllerTakeCover ), 
		bits_COND_NEW_ENEMY,
		0,
		"ControllerTakeCover"
	},
};


Task_t	tlControllerFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
//	{ TASK_WAIT,				(float)2		},
	{ TASK_FACE_IDEAL,			(float)0		}, // diffusion - FIXME this helped, when the flying monster gets hit from nowhere he turns to face the attacker
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slControllerFail[] =
{
	{
		tlControllerFail,
		SIZEOFARRAY ( tlControllerFail ),
		0,
		0,
		"ControllerFail"
	},
};

DEFINE_CUSTOM_SCHEDULES( CController )
{
	slControllerChaseEnemy,
	slControllerStrafe,
	slControllerTakeCover,
	slControllerFail,
	slDroneChaseOwner,
};

IMPLEMENT_CUSTOM_SCHEDULES( CController, CSquadMonster );


Vector Intersect( Vector vecSrc, Vector vecDst, Vector vecMove, float flSpeed )
{
	Vector vecTo = vecDst - vecSrc;

	float a = DotProduct( vecMove, vecMove ) - flSpeed * flSpeed;
	float b = 0 * DotProduct(vecTo, vecMove); // why does this work?
	float c = DotProduct( vecTo, vecTo );

	float t;
	if (a == 0)
	{
		t = c / (flSpeed * flSpeed);
	}
	else
	{
		t = b * b - 4 * a * c;
		t = sqrt( t ) / (2.0 * a);
		float t1 = -b +t;
		float t2 = -b -t;

		if (t1 < 0 || t2 < t1)
			t = t2;
		else
			t = t1;
	}

	// ALERT( at_console, "Intersect %f\n", t );

	if (t < 0.1)
		t = 0.1;
	if (t > 10.0)
		t = 10.0;

	Vector vecHit = vecTo + vecMove * t;
	return vecHit.Normalize( ) * flSpeed;
}


int CController::LookupFloat( )
{
	if (m_velocity.Length( ) < 32.0)
		return LookupSequence( "up" );

	UTIL_MakeAimVectors( GetAbsAngles() );
	float x = DotProduct( gpGlobals->v_forward, m_velocity );
	float y = DotProduct( gpGlobals->v_right, m_velocity );
	float z = DotProduct( gpGlobals->v_up, m_velocity );

	if (fabs(x) > fabs(y) && fabs(x) > fabs(z))
	{
		if (x > 0)
			return LookupSequence( "forward");
		else
			return LookupSequence( "backward");
	}
	else if (fabs(y) > fabs(z))
	{
		if (y > 0)
			return LookupSequence( "right");
		else
			return LookupSequence( "left");
	}
	else
	{
		if (z > 0)
			return LookupSequence( "up");
		else
			return LookupSequence( "down");
	}
}

//=========================================================
// StartTask
//=========================================================
void CController::StartTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		CSquadMonster::StartTask( pTask );
		break;
	case TASK_GET_PATH_TO_ENEMY_LKP:
	{
		if( BuildNearestRoute( m_vecEnemyLKP, pev->view_ofs, pTask->flData, (m_vecEnemyLKP - GetAbsOrigin()).Length() + 1024 ) )
		{
			TaskComplete();
		}
		else
		{
			// no way to get there =(
			ALERT( at_aiconsole, "GetPathToEnemyLKP failed!!\n" );
			TaskFail();
		}
		break;
	}
	case TASK_GET_PATH_TO_ENEMY:
	{
		CBaseEntity *pEnemy = m_hEnemy;

		if( pEnemy == NULL )
		{
			TaskFail();
			return;
		}

		if( BuildNearestRoute( pEnemy->GetAbsOrigin(), pEnemy->pev->view_ofs, pTask->flData, (pEnemy->GetAbsOrigin() - GetAbsOrigin()).Length() + 1024 ) )
		{
			TaskComplete();
		}
		else
		{
			// no way to get there =(
			ALERT( at_aiconsole, "GetPathToEnemy failed!!\n" );
			TaskFail();
		}
		break;
	}
	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask 
//=========================================================
void CController :: RunTask ( Task_t *pTask )
{
	if (m_flShootEnd > gpGlobals->time)
	{
		Vector vecHand, vecAngle;
		
		GetAttachment( 2, vecHand, vecAngle );
	
		while (m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->time)
		{
			Vector vecSrc = vecHand + GetAbsVelocity() * (m_flShootTime - gpGlobals->time);
			Vector vecDir;
			
			if (m_hEnemy != NULL)
			{
				if (HasConditions( bits_COND_SEE_ENEMY ))
					m_vecEstVelocity = m_vecEstVelocity * 0.5 + m_hEnemy->GetAbsVelocity() * 0.5;
				else
					m_vecEstVelocity = m_vecEstVelocity * 0.8;

				vecDir = Intersect( vecSrc, m_hEnemy->BodyTarget( GetAbsOrigin() ), m_vecEstVelocity, gSkillData.controllerSpeedBall );
				float delta = 0.03490; // +-2 degree
				vecDir = vecDir + Vector( RANDOM_FLOAT( -delta, delta ), RANDOM_FLOAT( -delta, delta ), RANDOM_FLOAT( -delta, delta ) ) * gSkillData.controllerSpeedBall;

				vecSrc = vecSrc + vecDir * (gpGlobals->time - m_flShootTime);
				CBaseMonster *pBall = (CBaseMonster*)Create( "controller_energy_ball", vecSrc, GetAbsAngles(), edict() );
				pBall->SetAbsVelocity( vecDir );
			}
			m_flShootTime += 0.2;
		}

		if (m_flShootTime > m_flShootEnd)
		{
			m_iBall[0] = 64;
			m_iBallTime[0] = m_flShootEnd;
			m_iBall[1] = 64;
			m_iBallTime[1] = m_flShootEnd;
			m_fInCombat = FALSE;
		}
	}

	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT:
	case TASK_WAIT:
	case TASK_WAIT_FACE_ENEMY:
	case TASK_WAIT_PVS:
		if( m_hEnemy != NULL )
		{
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );
		}

		if( m_hTargetEnt != NULL ) // diffusion - look to my scripted_sequence
		{
			MakeIdealYaw( m_hTargetEnt->GetAbsOrigin() );
			ChangeYaw( 20 );// pev->yaw_speed // turn slowly. !!!this must be a editable value.
		}

		if (m_fSequenceFinished)
			m_fInCombat = FALSE;

		CSquadMonster :: RunTask ( pTask );

		if (!m_fInCombat)
		{
			if (HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK1 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else if (HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK2 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else
			{
				int iFloat = LookupFloat( );
				if (m_fSequenceFinished || iFloat != pev->sequence)
				{
					pev->sequence = iFloat;
					pev->frame = 0;
					ResetSequenceInfo( );
				}
			}
		}
		break;
	default: 
		CBaseMonster :: RunTask ( pTask );
		break;
	}
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CController :: GetSchedule ( void )
{
	switch	( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
	{
		//	Vector vecTmp = Intersect( Vector( 0, 0, 0 ), Vector( 100, 4, 7 ), Vector( 2, 10, -3 ), 20.0 );
		if( HasConditions( bits_COND_NEW_ENEMY ) )
		{
			if( InSquad() )
				MySquadLeader()->m_fEnemyEluded = FALSE;
		}

		// dead enemy
		if ( HasConditions ( bits_COND_LIGHT_DAMAGE ) )
		{
			// m_iFrustration++;
		}
		if ( HasConditions ( bits_COND_HEAVY_DAMAGE ) )
		{
			// m_iFrustration++;
		}

		if ( HasConditions( bits_COND_ENEMY_DEAD|bits_COND_ENEMY_LOST ) )
		// call base class, all code to handle dead enemies is centralized there.
				return CSquadMonster:: GetSchedule();
	}
	break;
	}

	return CSquadMonster :: GetSchedule();
}



//=========================================================
//=========================================================
Schedule_t* CController :: GetScheduleOfType ( int Type ) 
{
	// ALERT( at_console, "%d\n", m_iFrustration );
	switch	( Type )
	{
	case SCHED_CHASE_ENEMY:
		return slControllerChaseEnemy;
	case SCHED_RANGE_ATTACK1:
		return slControllerStrafe;
	case SCHED_RANGE_ATTACK2:
	case SCHED_MELEE_ATTACK1:
	case SCHED_MELEE_ATTACK2:
	case SCHED_TAKE_COVER_FROM_ENEMY:
		return slControllerTakeCover;
	case SCHED_FAIL:
		return slControllerFail;
	}

	return CSquadMonster:: GetScheduleOfType( Type );
}



//=========================================================
// CheckRangeAttack1  - shoot a bigass energy ball out of their head
//=========================================================
BOOL CController :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if( flDot > 0.5 && flDist > 256 && flDist <= 2048 )
	{
		TraceResult	tr;
		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, ENT( pev ), &tr );

		if( tr.flFraction == 1.0 )
			return TRUE;
	}

	return FALSE;
}


BOOL CController :: CheckRangeAttack2 ( float flDot, float flDist )
{
	if( flDot > 0.5 && flDist > 64 && flDist <= 2048 )
	{
		TraceResult	tr;
		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, ENT( pev ), &tr );

		if( tr.flFraction == 1.0 )
			return TRUE;
	}

	return FALSE;
}


BOOL CController :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	return FALSE;
}


void CController :: SetActivity ( Activity NewActivity )
{
	CBaseMonster::SetActivity( NewActivity );

	switch ( m_Activity)
	{
	case ACT_WALK:
		m_flGroundSpeed = 100;
		break;
	default:
		m_flGroundSpeed = 100;
		break;
	}
}



//=========================================================
// RunAI
//=========================================================
void CController :: RunAI( void )
{
	CBaseMonster::RunAI();
	
	// diffusion - could be pushed by trigger but never slows down after, fix?
	if( pev->velocity.Length() > 0.1 )
		pev->velocity *= 0.8;
	else
		pev->velocity = g_vecZero;
	
	Vector vecStart, angleGun;

	if ( HasMemory( bits_MEMORY_KILLED ) )
		return;

	for (int i = 0; i < 2; i++)
	{
		if (m_pBall[i] == NULL)
		{
			m_pBall[i] = CSprite::SpriteCreate( "sprites/xspark4.spr", GetAbsOrigin(), TRUE );
			m_pBall[i]->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, 0 );
			m_pBall[i]->SetAttachment( edict(), (i + 3) );
			m_pBall[i]->SetScale( 1.0 );
		}

		float t = m_iBallTime[i] - gpGlobals->time;
		if (t > 0.1)
			t = 0.1 / t;
		else
			t = 1.0;

		m_iBallCurrent[i] += (m_iBall[i] - m_iBallCurrent[i]) * t;

		m_pBall[i]->SetBrightness( m_iBallCurrent[i] );

		GetAttachment( i + 2, vecStart, angleGun );
		UTIL_SetOrigin( m_pBall[i], vecStart );
		
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_ELIGHT );
			WRITE_SHORT( entindex( ) + 0x1000 * (i + 3) );		// entity, attachment
			WRITE_COORD( vecStart.x );		// origin
			WRITE_COORD( vecStart.y );
			WRITE_COORD( vecStart.z );
			WRITE_COORD( m_iBallCurrent[i] / 8 );	// radius
			WRITE_BYTE( 255 );	// R
			WRITE_BYTE( 192 );	// G
			WRITE_BYTE( 64 );	// B
			WRITE_BYTE( 5 );	// life * 10
			WRITE_COORD( 0 ); // decay
		MESSAGE_END();
	}
}


extern void DrawRoute( entvars_t *pev, WayPoint_t *m_Route, int m_iRouteIndex, int r, int g, int b );

void CController::Stop( void ) 
{ 
	m_IdealActivity = GetStoppedActivity();
	m_velocity = g_vecZero; // diffusion - reset velocity value if stopped
}


#define DIST_TO_CHECK	200
void CController :: Move ( float flInterval ) 
{
	float		flWaypointDist;
	float		flCheckDist;
	float		flDist;// how far the lookahead check got before hitting an object.
	float		flMoveDist;
	Vector		vecDir;
	Vector		vecApex;
	CBaseEntity	*pTargetEnt;

	if( HasFlag( F_PLAYER_CONTROL ) )
	{
		TaskFail();
		return;
	}

	// Don't move if no valid route
	if ( FRouteClear() )
	{
		ALERT( at_aiconsole, "Tried to move with no route!\n" );
		TaskFail();
		return;
	}
	
	if ( m_flMoveWaitFinished > gpGlobals->time )
		return;

// Debug, test movement code
#if 0
//	if ( CVAR_GET_FLOAT("stopmove" ) != 0 )
	{
		if ( m_movementGoal == MOVEGOAL_ENEMY )
			RouteSimplify( m_hEnemy );
		else
			RouteSimplify( m_hTargetEnt );
		FRefreshRoute();
		return;
	}
#else
// Debug, draw the route
	DrawRoute( pev, m_Route, m_iRouteIndex, 0, 0, 255 );
#endif

	// if the monster is moving directly towards an entity (enemy for instance), we'll set this pointer
	// to that entity for the CheckLocalMove and Triangulate functions.
	pTargetEnt = NULL;

	if (m_flGroundSpeed == 0)
	{
		m_flGroundSpeed = 100;
		// TaskFail( );
		// return;
	}

	flMoveDist = m_flGroundSpeed * flInterval;

	do 
	{
		// local move to waypoint.
		vecDir = ( m_Route[ m_iRouteIndex ].vecLocation - GetAbsOrigin() ).Normalize();
		flWaypointDist = ( m_Route[ m_iRouteIndex ].vecLocation - GetAbsOrigin() ).Length();
		
		// MakeIdealYaw ( m_Route[ m_iRouteIndex ].vecLocation );
		// ChangeYaw ( pev->yaw_speed );

		// if the waypoint is closer than CheckDist, CheckDist is the dist to waypoint
		if ( flWaypointDist < DIST_TO_CHECK )
		{
			flCheckDist = flWaypointDist;
		}
		else
		{
			flCheckDist = DIST_TO_CHECK;
		}
		
		if ( (m_Route[ m_iRouteIndex ].iType & (~bits_MF_NOT_TO_MASK)) == bits_MF_TO_ENEMY )
		{
			// only on a PURE move to enemy ( i.e., ONLY MF_TO_ENEMY set, not MF_TO_ENEMY and DETOUR )
			pTargetEnt = m_hEnemy;
		}
		else if ( (m_Route[ m_iRouteIndex ].iType & ~bits_MF_NOT_TO_MASK) == bits_MF_TO_TARGETENT )
		{
			pTargetEnt = m_hTargetEnt;
		}

		// !!!BUGBUG - CheckDist should be derived from ground speed.
		// If this fails, it should be because of some dynamic entity blocking this guy.
		// We've already checked this path, so we should wait and time out if the entity doesn't move
		flDist = 0;
		if ( CheckLocalMove ( GetAbsOrigin(), GetAbsOrigin() + vecDir * flCheckDist, pTargetEnt, &flDist ) != LOCALMOVE_VALID )
		{
			CBaseEntity *pBlocker;

			// Can't move, stop
			Stop();
			// Blocking entity is in global trace_ent
			pBlocker = CBaseEntity::Instance( gpGlobals->trace_ent );
			if (pBlocker)
			{
				DispatchBlocked( edict(), pBlocker->edict() );
			}
			if ( pBlocker && m_moveWaitTime > 0 && pBlocker->IsMoving() && !pBlocker->IsPlayer() && (gpGlobals->time-m_flMoveWaitFinished) > 3.0 )
			{
				// Can we still move toward our target?
				if ( flDist < m_flGroundSpeed )
				{
					// Wait for a second
					m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime;
	//				ALERT( at_aiconsole, "Move %s!!!\n", STRING( pBlocker->pev->classname ) );
					return;
				}
			}
			else 
			{
				// try to triangulate around whatever is in the way.
				if ( FTriangulate( GetAbsOrigin(), m_Route[ m_iRouteIndex ].vecLocation, flDist, pTargetEnt, &vecApex ) )
				{
					InsertWaypoint( vecApex, bits_MF_TO_DETOUR );
					RouteSimplify( pTargetEnt );
				}
				else
				{
	 			    ALERT ( at_aiconsole, "Couldn't Triangulate\n" );
					Stop();
					if ( m_moveWaitTime > 0 )
					{
						FRefreshRoute();
						m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime * 0.5;
					}
					else
					{
						TaskFail();
						ALERT( at_aiconsole, "Failed to move!\n" );
						//ALERT( at_aiconsole, "%f, %f, %f\n", GetAbsOrigin().z, (GetAbsOrigin() + (vecDir * flCheckDist)).z, m_Route[m_iRouteIndex].vecLocation.z );
					}
					return;
				}
			}
		}

		// UNDONE: this is a hack to quit moving farther than it has looked ahead.
		if (flCheckDist < flMoveDist)
		{
			MoveExecute( pTargetEnt, vecDir, flCheckDist / m_flGroundSpeed );

			// ALERT( at_console, "%.02f\n", flInterval );
			AdvanceRoute( flWaypointDist );
			flMoveDist -= flCheckDist;
		}
		else
		{
			MoveExecute( pTargetEnt, vecDir, flMoveDist / m_flGroundSpeed );

			if ( ShouldAdvanceRoute( flWaypointDist - flMoveDist ) )
			{
				AdvanceRoute( flWaypointDist );
			}
			flMoveDist = 0;
		}

		if ( MovementIsComplete() )
		{
			Stop();
			RouteClear();
		}
	} while (flMoveDist > 0 && flCheckDist > 0);

	// cut corner?
	if (flWaypointDist < 128)
	{
		if ( m_movementGoal == MOVEGOAL_ENEMY )
			RouteSimplify( m_hEnemy );
		else
			RouteSimplify( m_hTargetEnt );
		FRefreshRoute();

		if (m_flGroundSpeed > 100)
			m_flGroundSpeed -= 40;
	}
	else
	{
		if (m_flGroundSpeed < 400)
			m_flGroundSpeed += 10;
	}
}



BOOL CController:: ShouldAdvanceRoute( float flWaypointDist )
{
	if ( flWaypointDist <= 32  )
		return TRUE;

	return FALSE;
}


int CController :: CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist )
{
	if( HasFlag( F_PLAYER_CONTROL ) )
		return LOCALMOVE_INVALID;
	
	TraceResult tr;

	UTIL_TraceHull( vecStart + Vector( 0, 0, 32), vecEnd + Vector( 0, 0, 32), dont_ignore_monsters, large_hull, edict(), &tr );

	// ALERT( at_console, "%.0f %.0f %.0f : ", vecStart.x, vecStart.y, vecStart.z );
	// ALERT( at_console, "%.0f %.0f %.0f\n", vecEnd.x, vecEnd.y, vecEnd.z );

	if (pflDist)
	{
		*pflDist = ( (tr.vecEndPos - Vector( 0, 0, 32 )) - vecStart ).Length();// get the distance.
	}

	// ALERT( at_console, "check %d %d %f\n", tr.fStartSolid, tr.fAllSolid, tr.flFraction );
	if (tr.fStartSolid || tr.flFraction < 1.0)
	{
		if ( pTarget && pTarget->edict() == gpGlobals->trace_ent )
			return LOCALMOVE_VALID;
		return LOCALMOVE_INVALID;
	}

	return LOCALMOVE_VALID;
}


void CController::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval )
{
	if( HasFlag( F_PLAYER_CONTROL ) )
		return;
	
	if ( m_IdealActivity != m_movementActivity )
		m_IdealActivity = m_movementActivity;

	// ALERT( at_console, "move %.4f %.4f %.4f : %f\n", vecDir.x, vecDir.y, vecDir.z, flInterval );

	// float flTotal = m_flGroundSpeed * pev->framerate * flInterval;
	// UTIL_MoveToOrigin ( ENT(pev), m_Route[ m_iRouteIndex ].vecLocation, flTotal, MOVE_STRAFE );

	m_velocity = m_velocity * 0.8 + m_flGroundSpeed * vecDir * 0.2;

	UTIL_MoveToOrigin ( ENT(pev), GetAbsOrigin() + m_velocity, m_velocity.Length() * flInterval, MOVE_STRAFE );

	// g-cont. see code of engine function SV_MoveStep for details
	SetAbsOrigin( pev->origin );
}




//=========================================================
// Controller bouncy ball attack
//=========================================================
class CControllerHeadBall : public CBaseMonster
{
	DECLARE_CLASS( CControllerHeadBall, CBaseMonster );

	void Spawn( void );
	void Precache( void );
	void HuntThink( void );
	void DieThink( void );
	void BounceTouch( CBaseEntity *pOther );
	void MovetoTarget( Vector vecTarget );
	void Crawl( void );
	int m_iTrail;
	int m_flNextAttack;
	Vector m_vecIdeal;

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS( controller_head_ball, CControllerHeadBall );

BEGIN_DATADESC( CControllerHeadBall )
	DEFINE_FUNCTION( HuntThink ),
	DEFINE_FUNCTION( DieThink ),
	DEFINE_FUNCTION( BounceTouch ),
END_DATADESC()

void CControllerHeadBall :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/xspark4.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->scale = 2.0;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SetThink( &CControllerHeadBall::HuntThink );
	SetTouch( &CControllerHeadBall::BounceTouch );

	m_vecIdeal = Vector( 0, 0, 0 );

	pev->nextthink = gpGlobals->time + 0.1;

	m_hOwner = Instance( pev->owner );
	pev->dmgtime = gpGlobals->time;
}


void CControllerHeadBall :: Precache( void )
{
	PRECACHE_MODEL("sprites/xspark1.spr");
	PRECACHE_SOUND("debris/zap4.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
}


void CControllerHeadBall :: HuntThink( void  )
{
	SetNextThink( 0.1 );
	pev->renderamt -= 5;
	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( entindex( ) );		// entity, attachment
		WRITE_COORD( vecOrigin.x );		// origin
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( pev->renderamt / 16 );	// radius
		WRITE_BYTE( 255 );	// R
		WRITE_BYTE( 255 );	// G
		WRITE_BYTE( 255 );	// B
		WRITE_BYTE( 2 );	// life * 10
		WRITE_COORD( 0 ); // decay
	MESSAGE_END();

	// check world boundaries
	if( gpGlobals->time - pev->dmgtime > 5 || pev->renderamt < 64 || m_hEnemy == NULL || m_hOwner == NULL || !IsInWorld( FALSE ))
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}

	MovetoTarget( m_hEnemy->Center( ) );

	if ((m_hEnemy->Center() - GetAbsOrigin()).Length() < 64)
	{
		TraceResult tr;

		UTIL_TraceLine( GetAbsOrigin(), m_hEnemy->Center(), dont_ignore_monsters, ENT(pev), &tr );

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity != NULL && pEntity->pev->takedamage)
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( m_hOwner->pev, gSkillData.controllerDmgZap, GetAbsVelocity(), &tr, DMG_SHOCK );
			ApplyMultiDamage( pev, m_hOwner->pev );
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMENTPOINT );
			WRITE_SHORT( entindex() );
			WRITE_COORD( tr.vecEndPos.x );
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_SHORT( g_sModelIndexLaser );
			WRITE_BYTE( 0 ); // frame start
			WRITE_BYTE( 10 ); // framerate
			WRITE_BYTE( 3 ); // life
			WRITE_BYTE( 20 );  // width
			WRITE_BYTE( 0 );   // noise
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
			WRITE_BYTE( 10 );		// speed
		MESSAGE_END();

		UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );

		m_flNextAttack = gpGlobals->time + 3.0;

		SetThink( &CControllerHeadBall::DieThink );
		pev->nextthink = gpGlobals->time + 0.3;
	}

	// Crawl( );
}


void CControllerHeadBall :: DieThink( void  )
{
	UTIL_Remove( this );
}


void CControllerHeadBall :: MovetoTarget( Vector vecTarget )
{
	// accelerate
	float flSpeed = m_vecIdeal.Length();
	if (flSpeed == 0)
	{
		m_vecIdeal = GetAbsVelocity();
		flSpeed = m_vecIdeal.Length();
	}

	if (flSpeed > 400)
	{
		m_vecIdeal = m_vecIdeal.Normalize( ) * 400;
	}
	m_vecIdeal = m_vecIdeal + (vecTarget - GetAbsOrigin()).Normalize() * 100;
	SetAbsVelocity( m_vecIdeal );
}



void CControllerHeadBall :: Crawl( void  )
{

	Vector vecAim = Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize( );
	Vector vecPnt = GetAbsOrigin() + GetAbsVelocity() * 0.3 + vecAim * 64;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMENTPOINT );
		WRITE_SHORT( entindex() );
		WRITE_COORD( vecPnt.x);
		WRITE_COORD( vecPnt.y);
		WRITE_COORD( vecPnt.z);
		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 ); // frame start
		WRITE_BYTE( 10 ); // framerate
		WRITE_BYTE( 3 ); // life
		WRITE_BYTE( 20 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 10 );		// speed
	MESSAGE_END();
}


void CControllerHeadBall::BounceTouch( CBaseEntity *pOther )
{
	Vector vecDir = m_vecIdeal.Normalize( );

	TraceResult tr = UTIL_GetGlobalTrace( );

	float n = -DotProduct(tr.vecPlaneNormal, vecDir);

	vecDir = 2.0 * tr.vecPlaneNormal * n + vecDir;

	m_vecIdeal = vecDir * m_vecIdeal.Length();
}


class CControllerZapBall : public CBaseMonster
{
	DECLARE_CLASS( CControllerZapBall, CBaseMonster );

	void Spawn( void );
	void Precache( void );
	void AnimateThink( void );
	void ExplodeTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( controller_energy_ball, CControllerZapBall );

BEGIN_DATADESC( CControllerZapBall )
	DEFINE_FUNCTION( AnimateThink ),
	DEFINE_FUNCTION( ExplodeTouch ),
END_DATADESC()

void CControllerZapBall :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/xspark4.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 200;
	pev->scale = 0.5;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SetThink( &CControllerZapBall::AnimateThink );
	SetTouch( &CControllerZapBall::ExplodeTouch );

	m_hOwner = Instance( pev->owner );
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
	pev->nextthink = gpGlobals->time + 0.1;
}


void CControllerZapBall :: Precache( void )
{
	PRECACHE_MODEL("sprites/xspark4.spr");
	// PRECACHE_SOUND("debris/zap4.wav");
	// PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/alien_zap1.wav");
	PRECACHE_SOUND("weapons/alien_zap2.wav");
	PRECACHE_SOUND("weapons/alien_zap3.wav");
	PRECACHE_SOUND("weapons/alien_zap4.wav");
}


void CControllerZapBall :: AnimateThink( void  )
{
	SetNextThink( 0.1 );
	
	pev->frame = ((int)pev->frame + 1) % 11;

	if (gpGlobals->time - pev->dmgtime > 5 || GetAbsVelocity().Length() < 10)
	{
		SetTouch( NULL );
		UTIL_Remove( this );
	}
}

void CControllerZapBall::ExplodeTouch( CBaseEntity *pOther )
{
	if (pOther->pev->takedamage)
	{
		TraceResult tr = UTIL_GetGlobalTrace( );

		entvars_t	*pevOwner;
		if (m_hOwner != NULL)
		{
			pevOwner = m_hOwner->pev;
		}
		else
		{
			pevOwner = pev;
		}

		ClearMultiDamage( );
		pOther->TraceAttack(pevOwner, gSkillData.controllerDmgBall, GetAbsVelocity().Normalize(), &tr, DMG_ENERGYBEAM ); 
		ApplyMultiDamage( pevOwner, pevOwner );

		switch(RANDOM_LONG( 0, 3 ))
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap1.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap2.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		case 2:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap3.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		case 3:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap4.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		}

	}

	UTIL_Remove( this );
}


// diffusion - for monster_alien_soldier


class CAlienZapBall : public CBaseMonster
{
	DECLARE_CLASS( CAlienZapBall, CBaseMonster );

	void Spawn( void );
	void Precache( void );
	void AnimateThink( void );
	void ExplodeTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( alien_energy_ball, CAlienZapBall );

BEGIN_DATADESC( CAlienZapBall )
	DEFINE_FUNCTION( AnimateThink ),
	DEFINE_FUNCTION( ExplodeTouch ),
END_DATADESC()

void CAlienZapBall :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/xspark4.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 200;
	pev->scale = 0.25;
	pev->dmg = 5;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SetThink( &CAlienZapBall::AnimateThink );
	SetTouch( &CAlienZapBall::ExplodeTouch );

	m_hOwner = Instance( pev->owner );
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
	pev->nextthink = gpGlobals->time + 0.1;
}


void CAlienZapBall :: Precache( void )
{
	PRECACHE_MODEL("sprites/xspark4.spr");
	// PRECACHE_SOUND("debris/zap4.wav");
	// PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/alien_zap1.wav");
	PRECACHE_SOUND("weapons/alien_zap2.wav");
	PRECACHE_SOUND("weapons/alien_zap3.wav");
	PRECACHE_SOUND("weapons/alien_zap4.wav");
}


void CAlienZapBall :: AnimateThink( void  )
{
	SetNextThink( 0.1 );
	
	pev->frame = ((int)pev->frame + 1) % 11;

	if (gpGlobals->time - pev->dmgtime > 5 || GetAbsVelocity().Length() < 10)
	{
		SetTouch( NULL );
		UTIL_Remove( this );
	}
}

void CAlienZapBall::ExplodeTouch( CBaseEntity *pOther )
{
	TraceResult tr = UTIL_GetGlobalTrace();
	
	if (pOther->pev->takedamage)
	{
		entvars_t	*pevOwner;
		if (m_hOwner != NULL)
			pevOwner = m_hOwner->pev;
		else
			pevOwner = pev;

		ClearMultiDamage( );
		pOther->TraceAttack(pevOwner, pev->dmg, GetAbsVelocity().Normalize(), &tr, DMG_ENERGYBEAM ); // 5 dmg
		ApplyMultiDamage( pevOwner, pevOwner );
	}

	switch( RANDOM_LONG( 0, 3 ) )
	{
	case 0:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap1.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	case 1:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap2.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	case 2:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap3.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	case 3:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap4.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	}

	UTIL_Remove( this );
}

//=======================================================================================================================
// shotgun alien_soldier ================================================================================================
//=======================================================================================================================

class CAlienZapShotgun : public CBaseMonster
{
	DECLARE_CLASS( CAlienZapShotgun, CBaseMonster );

	void Spawn( void );
	void Precache( void );
	void AnimateThink( void );
	void ExplodeTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( alien_shotgun_ball, CAlienZapShotgun );

BEGIN_DATADESC( CAlienZapShotgun )
	DEFINE_FUNCTION( AnimateThink ),
	DEFINE_FUNCTION( ExplodeTouch ),
END_DATADESC()

void CAlienZapShotgun :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/animglow01.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 200;
	pev->rendercolor.y = 0;
	pev->rendercolor.z = 0;
	pev->renderamt = 255;
	pev->scale = 1;

	UTIL_SetSize( pev, Vector(-4,-4,0), Vector(4,4,4) );

	SetThink( &CAlienZapShotgun::AnimateThink );
	SetTouch( &CAlienZapShotgun::ExplodeTouch );

	m_hOwner = Instance( pev->owner );
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
	pev->nextthink = gpGlobals->time + 0.1;
}


void CAlienZapShotgun :: Precache( void )
{
	PRECACHE_MODEL("sprites/animglow01.spr");
	// PRECACHE_SOUND("debris/zap4.wav");
	// PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/alien_zap1.wav");
	PRECACHE_SOUND("weapons/alien_zap2.wav");
	PRECACHE_SOUND("weapons/alien_zap3.wav");
	PRECACHE_SOUND("weapons/alien_zap4.wav");
}


void CAlienZapShotgun :: AnimateThink( void  )
{
	SetNextThink( 0.1 );
	
	pev->frame = ((int)pev->frame + 1) % 11;

	if (gpGlobals->time - pev->dmgtime > 5 || GetAbsVelocity().Length() < 10)
	{
		SetTouch( NULL );
		UTIL_Remove( this );
	}
}

void CAlienZapShotgun::ExplodeTouch( CBaseEntity *pOther )
{
	TraceResult tr = UTIL_GetGlobalTrace();
	
	if (pOther->pev->takedamage)
	{
		entvars_t	*pevOwner;
		if (m_hOwner != NULL)
		{
			pevOwner = m_hOwner->pev;
		}
		else
		{
			pevOwner = pev;
		}

		ClearMultiDamage( );
		pOther->TraceAttack(pevOwner, 6.66f, GetAbsVelocity().Normalize(), &tr, DMG_ENERGYBEAM ); // 6.66 dmg
		ApplyMultiDamage( pevOwner, pevOwner );
	}

	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, pev->origin );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( pev->origin.x );	// X
		WRITE_COORD( pev->origin.y );	// Y
		WRITE_COORD( pev->origin.z );	// Z
		WRITE_BYTE( 16 );		// radius * 0.1
		WRITE_BYTE( 255 );		// r
		WRITE_BYTE( 255 );		// g
		WRITE_BYTE( 255 );		// b
		WRITE_BYTE( 10 );		// time * 10
		WRITE_BYTE( 10 );		// decay * 0.1
		WRITE_BYTE( 100 ); // brightness
		WRITE_BYTE( 0 ); // shadows
	MESSAGE_END();

	switch( RANDOM_LONG( 0, 3 ) )
	{
	case 0:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap1.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	case 1:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap2.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	case 2:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap3.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	case 3:
		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/alien_zap4.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
		break;
	}

	UTIL_DoSparkNoSound( pev, tr.vecEndPos );

	UTIL_Remove( this );
}

//==========================================================================================================================
// grenade launcher for alien_soldier ======================================================================================
//==========================================================================================================================


class CAlienSuperBall : public CBaseMonster
{
	DECLARE_CLASS( CAlienSuperBall, CBaseMonster );

	void Spawn( void );
	void Precache( void );
	void HuntThink( void );
	void DieThink( void );
	void BounceTouch( CBaseEntity *pOther );
	void MovetoTarget( Vector vecTarget );
	void Crawl( void );
	int m_iTrail;
	int m_flNextAttack;
	int m_iSoundState;
	Vector m_vecIdeal;

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS( alien_super_ball, CAlienSuperBall );

BEGIN_DATADESC( CAlienSuperBall )
	DEFINE_FUNCTION( HuntThink ),
	DEFINE_FUNCTION( DieThink ),
	DEFINE_FUNCTION( BounceTouch ),
END_DATADESC()

void CAlienSuperBall :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

//	SET_MODEL(ENT(pev), "sprites/xspark4.spr");
	SET_MODEL(ENT(pev), "sprites/animglow01.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 0;
	pev->rendercolor.y = 100;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->scale = 1.0;

//	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	UTIL_SetSize( pev, Vector(-18,-18,18), Vector(18,18,18) );

	SetThink( &CAlienSuperBall::HuntThink );
	SetTouch( &CAlienSuperBall::BounceTouch );

	m_vecIdeal = Vector( 0, 0, 0 );

	pev->nextthink = gpGlobals->time + 0.1;

	m_hOwner = Instance( pev->owner );
	pev->dmgtime = gpGlobals->time;
}


void CAlienSuperBall :: Precache( void )
{
//	PRECACHE_MODEL("sprites/xspark1.spr");
	PRECACHE_MODEL("sprites/animglow01.spr");
//	PRECACHE_SOUND("debris/zap4.wav");
//	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/alien_zap1.wav");
	PRECACHE_SOUND("weapons/alien_zap2.wav");
	PRECACHE_SOUND("weapons/alien_zap3.wav");
	PRECACHE_SOUND("weapons/alien_zap4.wav");
	PRECACHE_SOUND("weapons/alien_superball.wav");
}


void CAlienSuperBall :: HuntThink( void  )
{
//---------------------------------------------------------------------------
	if( pev->deadflag == DEAD_NO )
	{
		if (m_iSoundState == 0)
			m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions		
		else
		{
			CBaseEntity *pPlayer = NULL;

			pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
			// UNDONE: this needs to send different sounds to every player for multiplayer.	
			if (pPlayer)
			{

				float pitch = DotProduct( GetAbsVelocity() - pPlayer->GetAbsVelocity(), (pPlayer->GetAbsOrigin() - GetAbsOrigin()).Normalize() );

				pitch = (int)(100 + pitch / 50.0);

				if (pitch > 250) 
					pitch = 250;
				if (pitch < 50)
					pitch = 50;
				if (pitch == 100)
					pitch = 101;

				float flVol = 1.0;

				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/alien_superball.wav", 0.5, 1.5, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch); //0.5 volume, 1.5 radius
			}
		}
	}
//------------------------------------------------------------------------------------------------------------------------
	
	SetNextThink( 0.1 );
	pev->renderamt -= 5;
	Vector vecOrigin = GetAbsOrigin();

	// check world boundaries
	if( gpGlobals->time - pev->dmgtime > 5 || pev->renderamt < 64 || m_hEnemy == NULL || m_hOwner == NULL || !IsInWorld( FALSE ))
	{
		SetTouch( NULL );
		STOP_SOUND( ENT(pev), CHAN_STATIC, "weapons/alien_superball.wav" );
		UTIL_Remove( this );
		return;
	}

	MovetoTarget( m_hEnemy->Center( ) );

	if ((m_hEnemy->Center() - GetAbsOrigin()).Length() < 80)
	{
		TraceResult tr;

		UTIL_TraceLine( GetAbsOrigin(), m_hEnemy->Center(), dont_ignore_monsters, ENT(pev), &tr );

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity != NULL && pEntity->pev->takedamage)
		{
			ClearMultiDamage( );
											  // 15 instead of gSkillData.controllerDmgZap
			pEntity->TraceAttack( m_hOwner->pev, 15, GetAbsVelocity(), &tr, DMG_SHOCK );
			ApplyMultiDamage( pev, m_hOwner->pev );
		}
		// fire a lightning
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMENTPOINT );
			WRITE_SHORT( entindex() );
			WRITE_COORD( tr.vecEndPos.x );
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_SHORT( g_sModelIndexLaser );
			WRITE_BYTE( 0 ); // frame start
			WRITE_BYTE( 10 ); // framerate
			WRITE_BYTE( 3 ); // life
			WRITE_BYTE( 10 );  // width
			WRITE_BYTE( 150 );   // noise
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 150 );	// brightness
			WRITE_BYTE( 10 );		// speed
		MESSAGE_END();

		// emit a light on impact
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, pev->origin );
			WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(pev->origin.x);	// X
			WRITE_COORD(pev->origin.y);	// Y
			WRITE_COORD(pev->origin.z);	// Z
			WRITE_BYTE( 16 );		// radius * 0.1
			WRITE_BYTE( 255 );		// r
			WRITE_BYTE( 255 );		// g
			WRITE_BYTE( 255 );		// b
			WRITE_BYTE( 10 );		// time * 10
			WRITE_BYTE( 15 );		// decay * 0.1
			WRITE_BYTE( 100 ); // brightness
			WRITE_BYTE( 0 ); // shadows
		MESSAGE_END( );

//		UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );

		switch(RANDOM_LONG( 0, 3 ))
		{
		case 0:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap1.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		case 1:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap2.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		case 2:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap3.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		case 3:
			UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/alien_zap4.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
			break;
		}

		UTIL_ScreenShake( GetAbsOrigin(), 15.0, 150.0, 1.0, 500, true );

//		m_flNextAttack = gpGlobals->time + 3.0;

		SetThink( &CAlienSuperBall::DieThink );
		pev->nextthink = gpGlobals->time + 0.3;
	}

	// Crawl( );
}


void CAlienSuperBall :: DieThink( void  )
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, "weapons/alien_superball.wav" );
	UTIL_Remove( this );
}


void CAlienSuperBall :: MovetoTarget( Vector vecTarget )
{
	// accelerate
	float flSpeed = m_vecIdeal.Length();
	if (flSpeed == 0)
	{
		m_vecIdeal = GetAbsVelocity();
		flSpeed = m_vecIdeal.Length();
	}

	if (flSpeed > 400)
	{
		m_vecIdeal = m_vecIdeal.Normalize( ) * 400;
	}
	m_vecIdeal = m_vecIdeal + (vecTarget - GetAbsOrigin()).Normalize() * 100;
	SetAbsVelocity( m_vecIdeal );
}



void CAlienSuperBall :: Crawl( void  )
{

	Vector vecAim = Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize( );
	Vector vecPnt = GetAbsOrigin() + GetAbsVelocity() * 0.3 + vecAim * 64;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMENTPOINT );
		WRITE_SHORT( entindex() );
		WRITE_COORD( vecPnt.x);
		WRITE_COORD( vecPnt.y);
		WRITE_COORD( vecPnt.z);
		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 ); // frame start
		WRITE_BYTE( 10 ); // framerate
		WRITE_BYTE( 3 ); // life
		WRITE_BYTE( 20 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 10 );		// speed
	MESSAGE_END();
}


void CAlienSuperBall::BounceTouch( CBaseEntity *pOther )
{
	Vector vecDir = m_vecIdeal.Normalize( );

	TraceResult tr = UTIL_GetGlobalTrace( );

	float n = -DotProduct(tr.vecPlaneNormal, vecDir);

	vecDir = 2.0 * tr.vecPlaneNormal * n + vecDir;

	m_vecIdeal = vecDir * m_vecIdeal.Length();
}




//==========================================================================================================================
//================================Drone for Diffusion=======================================================================
//==========================================================================================================================

int iDroneMuzzleFlash;
#define ALIEN_EYE		"sprites/alien_eye.spr"

void DroneExplosion( Vector center, float randomRange, float time, int magnitude, float scale );

class CDrone : public CController
{
	DECLARE_CLASS(CDrone, CController);
public:
	void Spawn( void );
	void Precache( void );
//	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void StartTask(Task_t *pTask );
	void RunTask ( Task_t *pTask );
	void RunAI(void);
	Schedule_t* GetSchedule ( void );
	Schedule_t* GetScheduleOfType ( int Type );
	int ObjectCaps(void);
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	float LastSparkTime;
	bool IsPlayerDrone;

	void PainSound(void);
	void AlertSound(void);
	void IdleSound( void );

	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void ClearEffects(void);
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int SpriteExplosion;
	CSprite *AlienEye;
	bool CriticalDamage;

	bool IsRocketDrone; // monster_security_heavydrone
	float NextRocketAttack; // monster_security_heavydrone

	float SpawnTime; // player's drone only

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( monster_security_drone, CDrone);
LINK_ENTITY_TO_CLASS( _playerdrone, CDrone);
LINK_ENTITY_TO_CLASS( monster_security_heavydrone, CDrone );

BEGIN_DATADESC( CDrone )
	DEFINE_FIELD( AlienEye, FIELD_CLASSPTR ),
	DEFINE_FIELD( CriticalDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecEstVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CanInvestigate, FIELD_INTEGER ),
	DEFINE_FIELD( LastSparkTime, FIELD_TIME ),
	DEFINE_FIELD( IsRocketDrone, FIELD_BOOLEAN ),
	DEFINE_FIELD( SpawnTime, FIELD_TIME ),
	DEFINE_FIELD( NextRocketAttack, FIELD_TIME ),
	DEFINE_FIELD( IsPlayerDrone, FIELD_BOOLEAN ),
END_DATADESC()

int	CDrone :: Classify ( void )
{
	return m_iClass ? m_iClass : 14; // Faction A
}

int CDrone::ObjectCaps(void)
{
	int flags = 0;

	if( FClassnameIs( pev, "_playerdrone" ) )
		flags = FCAP_IMPULSE_USE;
	
	return (BaseClass :: ObjectCaps()) | flags;
}

void CDrone :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/drone_security.mdl");
	UTIL_SetSize( pev, Vector( -16, -16, -8 ), Vector( 16, 16, 8 ) );

	if( FClassnameIs( pev, "monster_security_heavydrone" ) )
		IsRocketDrone = true;

	if( FClassnameIs( pev, "_playerdrone" ) )
		IsPlayerDrone = true;

	if( IsRocketDrone )
	{
		pev->body = 1;
		NextRocketAttack = 0;
		m_iCounter = 3; // loaded rockets
		if( !pev->rendercolor || ( pev->rendercolor == g_vecZero ) )
			pev->rendercolor = Vector( 25, 25, 25 );
	}

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	pev->flags			|= FL_FLY;
	m_bloodColor		= DONT_BLEED;
	if( !pev->health )
	{
		pev->health = 75;
		if( IsRocketDrone )
			pev->health = RocketDroneHealth[g_iSkillLevel];
	}
	pev->view_ofs		= Vector( 0, 0, -2 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	AlienEye = CSprite::SpriteCreate( ALIEN_EYE, GetAbsOrigin(), TRUE );
	if( AlienEye )
	{
		AlienEye->SetAttachment( edict(), 7 );  // attachment sets in order starting from 0 in mdl file
		AlienEye->SetScale( 0.1 );
		if( FClassnameIs( pev, "_playerdrone" ) )
		{
			AlienEye->SetTransparency( kRenderTransAdd, 0, 255, 0, 100, 0 );
			pev->skin = 1;
		}
		else
			AlienEye->SetTransparency( kRenderTransAdd, 255, 0, 0, 150, 0 );
		AlienEye->SetFadeDistance( 1000 );
	}

	EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_spawn.wav", 0.5, ATTN_NORM );

	CriticalDamage = false; // didn't say "critical damage" yet

	if( IsPlayerDrone )
	{
		// UNDONE only player drone for now. it follows the grunts too but it's very buggy
		// ...is it really needed though?
		m_hOwner = Instance( pev->owner );
		m_hTargetEnt = m_hOwner;

		// tell player that I'm here
		CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE( pev->owner );
		if( pPlayer )
			pPlayer->DroneDeployed = true;

		SpawnTime = gpGlobals->time + 5;
		SetFlag( F_ENTITY_BUSY ); // drone can't be grabbed back right now.
		ObjectCaps(); // refresh just in case
	}

	SetFlag( F_METAL_MONSTER);

	m_afCapability |= bits_CAP_SQUAD;

	MonsterInit();

	m_afCapability &= ~bits_CAP_CANSEEFLASHLIGHT;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CDrone :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/drone_security.mdl");

	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
//	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDeathSounds );

//	PRECACHE_MODEL( "sprites/xspark4.spr");

//	iDroneMuzzleFlash = PRECACHE_MODEL("sprites/muzzle_shock.spr");
//	PRECACHE_SOUND("shocktrooper/shock_trooper_attack.wav");
//	PRECACHE_SOUND("weapons/shock_fire.wav");
//	PRECACHE_SOUND("weapons/shock_impact.wav");

	PRECACHE_SOUND("buttons/spark1.wav");
	PRECACHE_SOUND("buttons/spark2.wav");
	PRECACHE_SOUND("buttons/spark3.wav");
	PRECACHE_SOUND("buttons/spark4.wav");
	PRECACHE_SOUND("buttons/spark5.wav");
	PRECACHE_SOUND("buttons/spark6.wav");

	PRECACHE_SOUND("drone/drone_running.wav");

	PRECACHE_SOUND("drone/drone_hit1.wav");
	PRECACHE_SOUND("drone/drone_hit2.wav");
	PRECACHE_SOUND("drone/drone_hit3.wav");
	PRECACHE_SOUND("drone/drone_hit4.wav");
	PRECACHE_SOUND("drone/drone_hit5.wav");

	PRECACHE_SOUND("drone/drone_alert1.wav");
	PRECACHE_SOUND("drone/drone_alert2.wav");
	PRECACHE_SOUND("drone/drone_alert3.wav");

	PRECACHE_SOUND("drone/drone_damage.wav");
	PRECACHE_SOUND("drone/drone_spawn.wav");

	PRECACHE_SOUND("drone/drone_shoot1.wav");
	PRECACHE_SOUND("drone/drone_shoot2.wav");
	PRECACHE_SOUND("drone/drone_shoot3.wav");
	PRECACHE_SOUND( "drone/drone_shoot1_d.wav" );
	PRECACHE_SOUND( "drone/drone_shoot2_d.wav" );
	PRECACHE_SOUND( "drone/drone_shoot3_d.wav" );
	PRECACHE_SOUND( "drone/drone_emptyclip.wav" );

	PRECACHE_MODEL( ALIEN_EYE );

	SpriteExplosion = PRECACHE_MODEL( "sprites/white.spr" );

//	UTIL_PrecacheOther("shock_beam");
}	

void CDrone :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CONTROLLER_AE_HEAD_OPEN:
		case CONTROLLER_AE_BALL_SHOOT:
		case CONTROLLER_AE_SMALL_SHOOT:
			m_flShootTime = gpGlobals->time;
			m_flShootEnd = m_flShootTime + RANDOM_FLOAT(1.5,2.5);//atoi( pEvent->options ) / 15.0;
		case CONTROLLER_AE_POWERUP_FULL:
		case CONTROLLER_AE_POWERUP_HALF:
		break;
		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

Schedule_t *CDrone :: GetSchedule ( void )
{
	if( HasFlag( F_PLAYER_CONTROL ) )
	{
		if( m_MonsterState != MONSTERSTATE_IDLE )
			m_MonsterState = MONSTERSTATE_IDLE;
		return 0;
	}
	
	switch	( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
			Vector vecTmp = Intersect( Vector( 0, 0, 0 ), Vector( 100, 4, 7 ), Vector( 2, 10, -3 ), 20.0 );

			// dead enemy
			if ( HasConditions ( bits_COND_LIGHT_DAMAGE ) )
			{
				// m_iFrustration++;
			}
			if ( HasConditions ( bits_COND_HEAVY_DAMAGE ) )
			{
				// m_iFrustration++;
			}

			if ( HasConditions( bits_COND_ENEMY_DEAD|bits_COND_ENEMY_LOST ) )
			{
			// call base class, all code to handle dead enemies is centralized there.
				return CSquadMonster:: GetSchedule();
			}
		}
		break;
	}

	return CSquadMonster:: GetSchedule();
}



//=========================================================
//=========================================================
Schedule_t* CDrone :: GetScheduleOfType ( int Type ) 
{
	if( HasFlag( F_PLAYER_CONTROL ) )
		return 0;
	
	// ALERT( at_console, "%d\n", m_iFrustration );
	switch	( Type )
	{
	case SCHED_CHASE_ENEMY:
		return slControllerChaseEnemy;
	case SCHED_CHASE_OWNER:
		return slDroneChaseOwner;
	case SCHED_RANGE_ATTACK1:
		return slControllerStrafe;
	case SCHED_RANGE_ATTACK2:
	case SCHED_MELEE_ATTACK1:
	case SCHED_MELEE_ATTACK2:
	case SCHED_TAKE_COVER_FROM_ENEMY:
		return slControllerTakeCover;
	case SCHED_FAIL:
		return slControllerFail;
	}

	return CSquadMonster:: GetScheduleOfType( Type );
}

void CDrone :: RunAI( void )
{
	static bool sndrestart;

	if( AlienEye ) // diffusion - !!! the origin is not being updated, have to do this, otherwise distance culling issues
		AlienEye->pev->origin = pev->origin;
	
	if( IsPlayerDrone )
	{
		if( gpGlobals->time > SpawnTime )
			RemoveFlag( F_ENTITY_BUSY );

		/*
		if( m_iCounter <= 0 )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( pev->owner );
			CBasePlayer *pPlayer = (CBasePlayer *)pEntity;

			if( pPlayer )
			{
				// "retrieve" the drone (give the new one, technically)
				pPlayer->DroneHealth = pev->health;
				pPlayer->DroneAmmo = 0;
				pPlayer->DroneDeployed = false;
				pPlayer->GiveAmmo( 1, "drone", 1 );
				UTIL_ShowMessage( "UTIL_DRONEAMMOOUT", pPlayer );
			}
			UTIL_Remove( this );
			return;
		}*/
	}
	
	if( IsAlive() && (pev->health <= 30) )
	{
		if( gpGlobals->time > LastSparkTime )
		{
			Vector vecSparkOrigin = GetAbsOrigin();
			// setting location of the spark roughly in the center of model
			vecSparkOrigin.y -= 16;
			vecSparkOrigin.x -= 16; 

			UTIL_DoSpark(pev, vecSparkOrigin);
			LastSparkTime = gpGlobals->time + RANDOM_FLOAT(0.5,2.5);
		}
	}

	// diffusion - don't update the sound if killed or being removed.
	if( (pev->deadflag == DEAD_NO) && !(pev->flags & FL_KILLME) )
	{
		if( m_iSoundState == 0 )
			m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
		else
		{
			static float pitch = 100;
			const int lim = 115;

			if( HasFlag( F_PLAYER_CONTROL ) )
				pitch = 90 + GetAbsVelocity().Length() * 0.07;
			else
			{
				if( m_velocity.Length() > 50 || GetAbsVelocity().Length() > 50 )
					pitch += 2;
				else
					pitch -= 3;
			}

			pitch = bound( 90, pitch, lim );

			EMIT_SOUND_DYN( edict(), CHAN_STATIC, "drone/drone_running.wav", 0.5, 1.5, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch ); //0.5 volume, 1.5 radius
		}
	}

	if( HasFlag( F_PLAYER_CONTROL ) )
	{
		sndrestart = true;
		return;
	}

	if( sndrestart )
	{
		// diffusion - PVS issues
		// when we let go of controls and the drone was outside of player's PVS,
		// the sound of the drone was being played near the player,
		// until the drone would appear in the PVS again.
		STOP_SOUND( edict(), CHAN_STATIC, "drone/drone_running.wav" );
		sndrestart = false;
	}

	// diffusion - could be pushed by trigger but never slows down after, fix?
	if( pev->velocity.Length() > 0.1 )
		pev->velocity *= 0.8;
	else
		pev->velocity = g_vecZero;
	
	CBaseMonster :: RunAI();
}

void CDrone :: StartTask ( Task_t *pTask )
{
	if( HasFlag( F_PLAYER_CONTROL ) )
		return;
	
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		CSquadMonster :: StartTask ( pTask );
		break;
	case TASK_GET_PATH_TO_ENEMY_LKP:
		{
			if (BuildNearestRoute( m_vecEnemyLKP, pev->view_ofs, pTask->flData, (m_vecEnemyLKP - GetAbsOrigin()).Length() + 1024 ))
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToEnemyLKP failed!!\n" );
				TaskFail();
			}
			break;
		}
	case TASK_GET_PATH_TO_ENEMY:
		{
			CBaseEntity *pEnemy = m_hEnemy;

			if ( pEnemy == NULL )
			{
				TaskFail();
				return;
			}

			if (BuildNearestRoute( pEnemy->GetAbsOrigin(), pEnemy->pev->view_ofs, pTask->flData, (pEnemy->GetAbsOrigin() - GetAbsOrigin()).Length() + 1024 ))
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToEnemy failed!!\n" );
				TaskFail();
			}
			break;
		}
	default:
		CSquadMonster :: StartTask ( pTask );
		break;
	}
}

void CDrone :: RunTask ( Task_t *pTask )
{	
	if( HasFlag( F_PLAYER_CONTROL ) )
	{
		if( m_fInCombat )
			m_fInCombat = FALSE;

		return;
	}

	if (m_flShootEnd > gpGlobals->time)
	{
		Vector vecHand, vecAngle;
		
		GetAttachment( 2, vecHand, vecAngle );
	
		while (m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->time)
		{
			Vector vecSrc = vecHand + GetAbsVelocity() * (m_flShootTime - gpGlobals->time);
			
			if( m_hEnemy != NULL )
			{
				Vector vecShootOrigin = GetGunPosition();
				Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

				if( IsRocketDrone && gpGlobals->time > NextRocketAttack )
				{
					Vector rocket_ang = UTIL_VecToAngles( vecShootDir );
					CBaseMonster *pRocket = (CBaseMonster *)Create( "drone_rocket", vecShootOrigin, rocket_ang, edict() );

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

					m_iCounter--; // at max 3 rockets
					if( m_iCounter <= 0 )
					{
						m_iCounter = 3;
						NextRocketAttack = gpGlobals->time + 8;
						m_flShootTime = m_flShootEnd + 999; // make it stop and go into reload mode (use bullets)
					}
					m_flShootTime += 0.75;
				}
				else if( IsPlayerDrone )
				{
					// can I shoot?
					bool DroneCanShoot = true;
					CBasePlayer *pPlayerOwner = (CBasePlayer *)CBaseEntity::Instance( pev->owner );
					if( pPlayerOwner && pPlayerOwner->LoudWeaponsRestricted )
						DroneCanShoot = false;
					
					if( DroneCanShoot )
					{
						if( m_iCounter > 0 )
						{
							FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_3DEGREES, 4096, BULLET_MONSTER_MP5, 1, 1.5f ); // 1.5 fixed damage

							m_iCounter--; // player's drone keeps track of bullets

							PlayClientSound( this, 253 );

							pev->effects |= EF_MUZZLEFLASH;

							Vector	vecShellVelocity = pev->velocity
								+ gpGlobals->v_right * RANDOM_FLOAT( 50, 70 )
								+ gpGlobals->v_up * RANDOM_FLOAT( 100, 150 )
								+ gpGlobals->v_forward * 25;

							Vector ShellPos = GetAbsOrigin();
							ShellPos.z += 30;
							EjectBrass( ShellPos
								+ gpGlobals->v_up * -(RANDOM_LONG( 10, 15 ))
								+ gpGlobals->v_forward * RANDOM_LONG( 15, 25 )
								+ gpGlobals->v_right * RANDOM_LONG( 2, 6 ), vecShellVelocity,
								pev->angles.y, SHELL_9MM, TE_BOUNCE_SHELL );

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
						}
						else
							EMIT_SOUND( edict(), CHAN_WEAPON, "drone/drone_emptyclip.wav", 1, ATTN_NORM );
					}

					m_flShootTime += 0.1;
				}
				else
				{
					FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_3DEGREES, 4096, BULLET_MONSTER_MP5, 1, DroneDmg[g_iSkillLevel] );

					PlayClientSound( this, 253 );

					pev->effects |= EF_MUZZLEFLASH;

					Vector	vecShellVelocity = pev->velocity
						+ gpGlobals->v_right * RANDOM_FLOAT( 50, 70 )
						+ gpGlobals->v_up * RANDOM_FLOAT( 100, 150 )
						+ gpGlobals->v_forward * 25;

					Vector ShellPos = GetAbsOrigin();
					ShellPos.z += 30;
					EjectBrass( ShellPos
						+ gpGlobals->v_up * -(RANDOM_LONG( 10, 15 ))
						+ gpGlobals->v_forward * RANDOM_LONG( 15, 25 )
						+ gpGlobals->v_right * RANDOM_LONG( 2, 6 ), vecShellVelocity,
						pev->angles.y, SHELL_9MM, TE_BOUNCE_SHELL );

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

					m_flShootTime += 0.1;
				}
			}
			else
				m_flShootTime += 0.5;
		}

		if (m_flShootTime > m_flShootEnd)
			m_fInCombat = FALSE;
	}
	
	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT:
	case TASK_WAIT:
	case TASK_WAIT_FACE_ENEMY:
	case TASK_WAIT_PVS:
		if( m_hEnemy != NULL )
		{
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );
		}
		else if( m_hTargetEnt != NULL ) // diffusion - look to my scripted_sequence
		{
			MakeIdealYaw( m_hTargetEnt->GetAbsOrigin() );
			ChangeYaw( 20 );// pev->yaw_speed // turn slowly. !!!this must be a editable value.
		}

		if (m_fSequenceFinished)
			m_fInCombat = FALSE;

		CSquadMonster :: RunTask ( pTask );

		if (!m_fInCombat)
		{
			if (HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK1 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else if (HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK2 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else
			{
				int iFloat = LookupFloat( );
				if (m_fSequenceFinished || iFloat != pev->sequence)
				{
					pev->sequence = iFloat;
					pev->frame = 0;
					ResetSequenceInfo( );
				}
			}
		}
		break;
	case TASK_MOVE_TO_TARGET_RANGE: // diffusion - only for player's drone
	{
		if( !IsPlayerDrone )
		{
			CSquadMonster::RunTask( pTask );
			break;
		}
		
		float distance;

		if( m_hTargetEnt == NULL )
			TaskFail();
		else
		{
			distance = (m_vecMoveGoal - GetLocalOrigin()).Length2D();
			// Re-evaluate when you think you're finished, or the target has moved too far
			if( (distance < pTask->flData) || (m_vecMoveGoal - m_hTargetEnt->GetAbsOrigin()).Length() > pTask->flData * 0.5 )
			{
				m_vecMoveGoal = m_hTargetEnt->GetAbsOrigin();
				distance = (m_vecMoveGoal - GetLocalOrigin()).Length2D();
				FRefreshRoute();
			}

			// look at player
			if( (m_MonsterState == MONSTERSTATE_IDLE) || (m_MonsterState == MONSTERSTATE_ALERT) )
			{
				MakeIdealYaw( m_hTargetEnt->GetAbsOrigin() );
				ChangeYaw( 40 );// pev->yaw_speed // turn slowly. !!!this must be a editable value.
			}
			else
			{
				if( m_hEnemy != NULL )
				{
					MakeIdealYaw( m_hEnemy->GetAbsOrigin() );
					ChangeYaw( 50 );
				}
			}

			// Set the appropriate activity based on an overlapping range
			// overlap the range to prevent oscillation
			if( distance < pTask->flData )
			{
				TaskComplete();
				Stop(); // diffusion
				RouteClear();		// Stop moving
				m_hTargetEnt = NULL; // clear target
			}
			else if( distance < 190 && m_movementActivity != ACT_WALK )
				m_movementActivity = ACT_WALK;
			else if( distance >= 270 && m_movementActivity != ACT_RUN )
				m_movementActivity = ACT_RUN;
		}

		break;
	}
	default: 
		CSquadMonster :: RunTask ( pTask );
		break;
	}
}

BOOL CDrone :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if( HasFlag( F_PLAYER_CONTROL ) )
		return FALSE;
	
	if ( flDot > 0.5 && flDist > 64 && flDist <= 2048 )
	{
		TraceResult	tr;
		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, ENT( pev ), &tr );

		if( tr.flFraction == 1.0 )
			return TRUE;
	}

	return FALSE;
}

BOOL CDrone :: CheckRangeAttack2 ( float flDot, float flDist )
{
	return FALSE; // no secondary attack, maybe create one?
}


void CDrone :: PainSound( void )
{
	switch(RANDOM_LONG(0,4))
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}
}

void CDrone :: AlertSound(void)
{
	switch(RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert3.wav", 1, ATTN_NORM ); break;
	}
}

void CDrone :: IdleSound(void)
{
	// no sounds...yet?
}

void CDrone::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !IsPlayerDrone )
		return;
	
	if( pActivator && pActivator->IsPlayer() )
	{
		if( pev->owner != pActivator->edict() )
			return; // you can't steal someone else's drone! (lol)
	}
	
	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pev->owner );

	if( pPlayer )
	{
		pPlayer->GiveAmmo( 1, "drone", 1 );
		pPlayer->DroneHealth = pev->health;
		pPlayer->DroneAmmo = m_iCounter;
		pPlayer->DroneDeployed = false;

		// fire target if set
		if( !FStringNull( pPlayer->DroneTarget_OnReturn ) )
			UTIL_FireTargets( pPlayer->DroneTarget_OnReturn, pPlayer, pPlayer, USE_TOGGLE, 0 );
		CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayer->m_pActiveItem->GetWeaponPtr();
		gun->m_flNextPrimaryAttack = gun->m_flNextSecondaryAttack = gpGlobals->time + DRONE_DEPLOY_TIME;
	}

	// very important to do this!!!
	if( pev->effects & EF_MERGE_VISIBILITY )
		pev->effects &= ~EF_MERGE_VISIBILITY;

	UTIL_Remove(this);
}

void CDrone::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if( pev->owner )
	{
		if( (pevAttacker->flags & FL_CLIENT) && (VARS(pev->owner) == pevAttacker) )
			return;
	}
	
	if (RANDOM_LONG(0,3) == 0)
		UTIL_Ricochet( ptr->vecEndPos, 1.0 );

	UTIL_Sparks ( ptr->vecEndPos );
	
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CDrone :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pev->owner )
	{
		if( (pevAttacker->flags & FL_CLIENT) && (VARS( pev->owner ) == pevAttacker) )
			return 0;
	}
	
	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();
	
//------------------------------------------------	
//	Vector vecSparkOrigin = GetAbsOrigin();
//	// setting location of the spark in the center of model
//	vecSparkOrigin.z -= 8;
//	vecSparkOrigin.y -= 32;
//	vecSparkOrigin.x -= 32; 
//	UTIL_DoSparkNoSound( pev, vecSparkOrigin);
//------------------------------------------------
	if (pev->health < 40)
	{
		Vector vecOrigin = GetAbsOrigin();
		
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrigin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 10 ); // scale x1.0
			WRITE_BYTE( 10 ); // framerate
			WRITE_BYTE( 3 ); // pos randomize
		MESSAGE_END();

		if( !CriticalDamage )
		{
			EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_damage.wav", 0.5, ATTN_NORM );
			CriticalDamage = true;
		}
	}
//------------------------------------------------
	if( (m_hEnemy == NULL) && !HasFlag(F_PLAYER_CONTROL) )
	{
		MakeIdealYaw( pevAttacker->origin );
		//	ChangeYaw( pev->yaw_speed );
		SetTurnActivity();
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CDrone::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();
	
	// inform the player that I died
	if( IsPlayerDrone )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pev->owner );

		if( pPlayer )
		{
			// "retrieve" the drone (give the new one, technically)
			pPlayer->DroneHealth = 0;
			pPlayer->DroneAmmo = 0;
			pPlayer->DroneDeployed = false;
			pPlayer->GiveAmmo( 1, "drone", 1 );
		}
	}
	
	pev->renderfx = kRenderFxExplode;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 0;
	pev->rendercolor.z = 0;
	StopAnimation();
	SetThink( &CBaseEntity::SUB_Remove );
	int i;

	int parts = MODEL_FRAMES( g_sModelIndexMetalGibs );
	for ( i = 0; i < 10; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );
				
		int bodyPart = 0;
		if ( parts > 1 )
			bodyPart = RANDOM_LONG( 0, pev->body-1 );

		pGib->pev->body = bodyPart;
		pGib->m_bloodColor = DONT_BLEED;
		pGib->m_material = matMetal;
		pGib->SetAbsOrigin( GetAbsOrigin() );
//		pGib->SetAbsVelocity( UTIL_RandomBloodVector() * RANDOM_FLOAT( 1, 2 ));
		pGib->SetNextThink( 1.25 );
		pGib->SetThink( &CBaseEntity::SUB_FadeOut );
	}
	
//------------------------------------------------------------
	Vector position = GetAbsOrigin();
	position.z += 32;
	DroneExplosion( position, 75, 0.1, 60, 1 ); //60 + (i*20)	
//------------------------------------------------------------	
		Vector vecOrigin = GetAbsOrigin();
			
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_BREAKMODEL);

			// position
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );

			// size
			WRITE_COORD( 0.01 );
			WRITE_COORD( 0.01 );
			WRITE_COORD( 0.01 );

			// velocity
			WRITE_COORD( 0 ); 
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );

			// randomization of the velocity
			WRITE_BYTE( 25 ); 

			// Model
			WRITE_SHORT( g_sModelIndexMetalGibs );	//model id#

			// # of shards
			WRITE_BYTE( RANDOM_LONG(15,25) );

			// duration
			WRITE_BYTE( 20 );// 3.0 seconds

			// flags

			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();

		// blast effect
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z);
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z + RANDOM_LONG(200,400) ); // reach damage radius over .2 seconds
		WRITE_SHORT( SpriteExplosion );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 4 ); // life
		WRITE_BYTE( 12 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 120 ); // brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	CSquadMonster::Killed( pevAttacker, GIB_NEVER );
}

void CDrone::ClearEffects(void)
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, "drone/drone_running.wav" );
	STOP_SOUND( ENT(pev), CHAN_STATIC, "drone/drone_damage.wav" );
	STOP_SOUND( ENT(pev), CHAN_STATIC, "drone/drone_spawn.wav" );
	UTIL_Remove( AlienEye );
	AlienEye = NULL;

	if( pev->effects & EF_MERGE_VISIBILITY )
		pev->effects &= ~EF_MERGE_VISIBILITY;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

void DroneExplosion( Vector center, float randomRange, float time, int magnitude, float scale )
{
	KeyValueData	kvd;
	char			buf[128];

	center.x += RANDOM_FLOAT( -randomRange, randomRange );
	center.y += RANDOM_FLOAT( -randomRange, randomRange );

	CBaseEntity *pExplosion = CBaseEntity::Create( "env_explosion", center, g_vecZero, NULL );
	sprintf( buf, "%3d", magnitude );
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue( &kvd );
//	pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;
	pExplosion->pev->scale = scale;
	pExplosion->Spawn();
	pExplosion->SetThink( &CBaseEntity::SUB_CallUseToggle );
	pExplosion->pev->nextthink = gpGlobals->time + time;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////




// A L I E N    D R O N E

class CDroneAlien : public CController
{
	DECLARE_CLASS(CDroneAlien, CController);
public:
	void Spawn( void );
	void Precache( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunTask ( Task_t *pTask );
	void MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval );
	void RunAI( void );

	void PainSound(void);
	void AlertSound(void);
	void IdleSound( void );

	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void ClearEffects(void);
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int SpriteExplosion;
	CSprite *AlienEye;
	bool CriticalDamage;
	float SecondaryAttackTime;

	DECLARE_DATADESC();

	int ShootAttachment;
};

LINK_ENTITY_TO_CLASS( monster_alien_drone, CDroneAlien);

BEGIN_DATADESC( CDroneAlien )
	DEFINE_FIELD( AlienEye, FIELD_CLASSPTR ),
	DEFINE_FIELD( CriticalDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecEstVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CanInvestigate, FIELD_INTEGER ),
	DEFINE_FIELD( SecondaryAttackTime, FIELD_TIME ),
END_DATADESC()

int	CDroneAlien :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_ALIEN_MILITARY;
}

void CDroneAlien :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/drone_alien.mdl");
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 32 ));

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	pev->flags			|= FL_FLY;
	m_bloodColor		= DONT_BLEED;
	if (!pev->health) pev->health	= 75;
	pev->view_ofs		= Vector( 0, 0, -2 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	AlienEye = CSprite::SpriteCreate( ALIEN_EYE, GetAbsOrigin(), TRUE );
	if( AlienEye )
	{
		AlienEye->SetAttachment( edict(), 1 );
		AlienEye->SetScale( 0.2 );
		AlienEye->SetTransparency( kRenderTransAdd, 255, 0, 0, 150, 0 );
		AlienEye->SetFadeDistance( 1000 );
	}

	SecondaryAttackTime = gpGlobals->time;

	SetFlag(F_METAL_MONSTER);

	m_afCapability |= bits_CAP_SQUAD;
	m_fEnemyEluded = FALSE;

	MonsterInit();

	m_afCapability &= ~bits_CAP_CANSEEFLASHLIGHT; // robots can't recognize player's flashlight
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CDroneAlien :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/drone_alien.mdl");

	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDeathSounds );

//	PRECACHE_MODEL( "sprites/xspark4.spr");

/*	iDroneMuzzleFlash = */PRECACHE_MODEL("sprites/muzzle_shock.spr");
	PRECACHE_SOUND("shocktrooper/shock_trooper_attack.wav");
	PRECACHE_SOUND("drone/aliendrone_shoot1.wav");
	PRECACHE_SOUND("drone/aliendrone_shoot2.wav");
	PRECACHE_SOUND("drone/aliendrone_shoot3.wav");
	PRECACHE_SOUND("drone/aliendrone_shoot4.wav");

	PRECACHE_SOUND("buttons/spark1.wav");
	PRECACHE_SOUND("buttons/spark2.wav");
	PRECACHE_SOUND("buttons/spark3.wav");
	PRECACHE_SOUND("buttons/spark4.wav");
	PRECACHE_SOUND("buttons/spark5.wav");
	PRECACHE_SOUND("buttons/spark6.wav");

	PRECACHE_SOUND("drone/aliendrone_loop.wav");

	PRECACHE_SOUND("drone/drone_hit1.wav");
	PRECACHE_SOUND("drone/drone_hit2.wav");
	PRECACHE_SOUND("drone/drone_hit3.wav");
	PRECACHE_SOUND("drone/drone_hit4.wav");
	PRECACHE_SOUND("drone/drone_hit5.wav");

	PRECACHE_MODEL( ALIEN_EYE );

	UTIL_PrecacheOther("shock_beam");
	UTIL_PrecacheOther( "shootball" ); // combine ball
	PRECACHE_SOUND( "comball/spawn.wav" );
}

void CDroneAlien::ClearEffects(void)
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, "drone/aliendrone_loop.wav" );
	STOP_SOUND( ENT(pev), CHAN_STATIC, "drone/drone_damage.wav" );
	UTIL_Remove( AlienEye );
	AlienEye = NULL;
}

void CDroneAlien::RunAI(void)
{
	if( AlienEye ) // diffusion - !!! the origin is not being updated, have to do this, otherwise distance culling issues
		AlienEye->pev->origin = pev->origin;

	CBaseMonster::RunAI();
}

void CDroneAlien :: RunTask ( Task_t *pTask )
{
	// diffusion - don't update the sound if killed or being removed.
	if( (pev->deadflag == DEAD_NO) && !(pev->flags & FL_KILLME) )
	{
		if( m_iSoundState == 0 )
			m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
		else
		{
			static float pitch = 100;
			const int lim = 115;

			if( m_velocity.Length() > 50 )
				pitch++;
			else
				pitch--;

			pitch = bound( 90, pitch, lim );

			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "drone/aliendrone_loop.wav", 0.5, 1.5, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch); //0.5 volume, 1.5 radius
		}
	}
//----------------------------------------------------------------------------------------

	if ( HasConditions(bits_COND_CAN_RANGE_ATTACK1) && (m_flShootEnd > gpGlobals->time))
	{
		Vector vecHand, vecAngle;
		GetAttachment( 2, vecHand, vecAngle );
	
		while (m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->time)
		{
			Vector vecSrc = vecHand + GetAbsVelocity() * (m_flShootTime - gpGlobals->time);
			Vector vecDir;
			
			if (m_hEnemy != NULL)
			{
				Vector	vecGunPos;
				Vector	vecGunAngles;

				CSprite *pMuzzleFlash = CSprite::SpriteCreate( "sprites/muzzle_shock.spr", GetAbsOrigin(), TRUE );
				if( pMuzzleFlash )
				{
					if( ShootAttachment == 0 )
						pMuzzleFlash->SetAttachment( edict(), 2 );
					else if( ShootAttachment == 1 )
						pMuzzleFlash->SetAttachment( edict(), 3 );
					pMuzzleFlash->pev->scale = 0.5;
					pMuzzleFlash->pev->rendermode = kRenderTransAdd;
					pMuzzleFlash->pev->renderamt = 255;
					pMuzzleFlash->AnimateAndDie( 25 );
				}

				if( ShootAttachment == 0 )
				{
					GetAttachment(1, vecGunPos, vecGunAngles);
					ShootAttachment = 1;
				}
				else if( ShootAttachment == 1 )
				{
					GetAttachment(2, vecGunPos, vecGunAngles);
					ShootAttachment = 0;
				}

				UTIL_MakeVectors(pev->angles);
				Vector vecShootOrigin = vecGunPos + gpGlobals->v_forward * 32 + gpGlobals->v_up * -4;
				Vector vecShootDir = ShootAtEnemy( vecShootOrigin );
				vecGunAngles = UTIL_VecToAngles(vecShootDir);

				/*
				if( gpGlobals->time > SecondaryAttackTime + RANDOM_FLOAT(7.0, 15.0))
				{
					CBaseMonster *pBall = (CBaseMonster*)Create( "shootball", vecShootOrigin, vecGunAngles, edict() );
					pBall->pev->spawnflags |= BIT(12); // ENVBALL_ALIENSHIP: explode on first bounce
					pBall->SetAbsVelocity( vecShootDir * 700 );
					m_flShootTime += 10;
					SecondaryAttackTime = gpGlobals->time;
				}
				else
				{*/
					switch( RANDOM_LONG(0,3) )
					{
					case 0: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "drone/aliendrone_shoot1.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110)); break;
					case 1: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "drone/aliendrone_shoot2.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110)); break;
					case 2: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "drone/aliendrone_shoot3.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110)); break;
					case 3: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "drone/aliendrone_shoot4.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110)); break;
					}
					CBaseEntity *pShock = CBaseEntity::Create("shock_beam", vecShootOrigin, vecGunAngles, edict());
					if( pShock )
					{
						pShock->pev->scale = 0.5;
						pShock->pev->velocity = vecShootDir * ProjectileVelocity[g_iSkillLevel];

						Vector AddVelocity = g_vecZero;
						if( g_iSkillLevel == SKILL_HARD )
							AddVelocity = m_hEnemy->pev->velocity + m_hEnemy->pev->basevelocity;
						else if( g_iSkillLevel == SKILL_MEDIUM )
							AddVelocity = m_hEnemy->pev->velocity.Normalize() * 150 + m_hEnemy->pev->basevelocity;
						pShock->pev->velocity += AddVelocity;
						pShock->pev->nextthink = gpGlobals->time;
						pShock->pev->dmg = 1.25;
					}
			//	}

				SetBlending( 0, vecGunAngles.x );
				// Play fire sound.
				CSoundEnt::InsertSound(bits_SOUND_COMBAT, GetAbsOrigin(), 384, 0.3, ENTINDEX(edict()) );
			}

			m_flShootTime += 0.2; // shooting rate
		}

		if (m_flShootTime > m_flShootEnd)
			m_fInCombat = FALSE;
	}

	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT:
	case TASK_WAIT:
	case TASK_WAIT_FACE_ENEMY:
	case TASK_WAIT_PVS:
		if( m_hEnemy != NULL )
		{
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );
		}

		if( m_hTargetEnt != NULL ) // diffusion - look to my scripted_sequence
		{
			MakeIdealYaw( m_hTargetEnt->GetAbsOrigin() );
			ChangeYaw( 20 );// pev->yaw_speed // turn slowly. !!!this must be a editable value.
		}

		if (m_fSequenceFinished)
			m_fInCombat = FALSE;

		CSquadMonster :: RunTask ( pTask );

		if (!m_fInCombat)
		{
			if (HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK1 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else if (HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK2 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else
			{
				int iFloat = LookupFloat( );
				if (m_fSequenceFinished || iFloat != pev->sequence)
				{
					pev->sequence = iFloat;
					pev->frame = 0;
					ResetSequenceInfo( );
				}
			}
		}
		break;
	default: 
		CSquadMonster :: RunTask ( pTask );
		break;
	}
}

void CDroneAlien::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval )
{
	if ( m_IdealActivity != m_movementActivity )
		m_IdealActivity = m_movementActivity;

	// ALERT( at_console, "move %.4f %.4f %.4f : %f\n", vecDir.x, vecDir.y, vecDir.z, flInterval );

	// float flTotal = m_flGroundSpeed * pev->framerate * flInterval;
	// UTIL_MoveToOrigin ( ENT(pev), m_Route[ m_iRouteIndex ].vecLocation, flTotal, MOVE_STRAFE );

	float SkillMultiplier = 1.0f;
	if( g_iSkillLevel == SKILL_MEDIUM )
		SkillMultiplier = 1.05;
	else if( g_iSkillLevel == SKILL_HARD )
		SkillMultiplier = 1.1;

	m_velocity = m_velocity * 0.8 + m_flGroundSpeed * vecDir * 0.2;

	if( m_MonsterState == MONSTERSTATE_COMBAT )
		m_velocity *= SkillMultiplier;

	UTIL_MoveToOrigin ( ENT(pev), GetAbsOrigin() + m_velocity, m_velocity.Length() * flInterval, MOVE_STRAFE );

	// g-cont. see code of engine function SV_MoveStep for details
	SetAbsOrigin( pev->origin );	
}

void CDroneAlien :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CONTROLLER_AE_SMALL_SHOOT:
		{
//			AttackSound( );
			m_flShootTime = gpGlobals->time;
			m_flShootEnd = m_flShootTime + Q_atoi( pEvent->options ) / 15.0;
		}
		break;
		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

BOOL CDroneAlien :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if( flDot > 0.5 && flDist > 64 && flDist <= 4096 )
	{
		TraceResult	tr;
		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, ENT( pev ), &tr );

		if( tr.flFraction == 1.0 )
			return TRUE;
	}

	return FALSE;
}

BOOL CDroneAlien :: CheckRangeAttack2 ( float flDot, float flDist )
{
	return FALSE; // no secondary attack, maybe create one?
}


void CDroneAlien :: PainSound( void )
{
	switch(RANDOM_LONG(0,4))
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}
}

void CDroneAlien :: AlertSound(void)
{
//	switch(RANDOM_LONG(0,2))
//	{
//	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert1.wav", 1, ATTN_NORM ); break;
//	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert2.wav", 1, ATTN_NORM ); break;
//	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert3.wav", 1, ATTN_NORM ); break;
//	}
}

void CDroneAlien :: IdleSound(void)
{
	// no sounds...yet?
}

void CDroneAlien::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if (RANDOM_LONG(0,3) == 0)
		UTIL_Ricochet( ptr->vecEndPos, 1.0 );

	UTIL_Sparks ( ptr->vecEndPos );
	
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CDroneAlien :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();
	
//------------------------------------------------	
//	Vector vecSparkOrigin = GetAbsOrigin();
//	// setting location of the spark in the center of model
//	vecSparkOrigin.z -= 8;
//	vecSparkOrigin.y -= 32;
//	vecSparkOrigin.x -= 32; 
//	UTIL_DoSparkNoSound( pev, vecSparkOrigin);
//------------------------------------------------
	if (pev->health < 40)
	{
		Vector vecOrigin = GetAbsOrigin();
		
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrigin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 10 ); // scale x1.0
			WRITE_BYTE( 10 ); // framerate
			WRITE_BYTE( 3 ); // pos randomize
		MESSAGE_END();

		if( !CriticalDamage )
		{
		//	EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_damage.wav", 0.5, ATTN_NORM ); // UNDONE: SOUND!
			CriticalDamage = true;
		}
	}
//------------------------------------------------

	if( m_hEnemy == NULL )
	{
		MakeIdealYaw( pevAttacker->origin );
		//	ChangeYaw( pev->yaw_speed );
		SetTurnActivity();
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CDroneAlien::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();
	
	pev->renderfx = kRenderFxExplode;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 0;
	pev->rendercolor.z = 0;
	StopAnimation();
	int i;

	int parts = MODEL_FRAMES( g_sModelIndexMetalGibs );
	for ( i = 0; i < 10; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );
				
		int bodyPart = 0;
		if ( parts > 1 )
			bodyPart = RANDOM_LONG( 0, pev->body-1 );

		pGib->pev->body = bodyPart;
		pGib->m_bloodColor = DONT_BLEED;
		pGib->m_material = matMetal;
		pGib->SetAbsOrigin( GetAbsOrigin() );
//		pGib->SetAbsVelocity( UTIL_RandomBloodVector() * RANDOM_FLOAT( 1, 2 ));
		pGib->SetNextThink( 1.25 );
		pGib->SetThink( &CBaseEntity::SUB_FadeOut );
	}
	
//------------------------------------------------------------
	Vector position = GetAbsOrigin();
	position.z += 32;
	DroneExplosion( position, 75, 0.1, 60, 1 ); //60 + (i*20)	
//------------------------------------------------------------	
		Vector vecOrigin = GetAbsOrigin();
			
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_BREAKMODEL);

			// position
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );

			// size
			WRITE_COORD( 0.01 );
			WRITE_COORD( 0.01 );
			WRITE_COORD( 0.01 );

			// velocity
			WRITE_COORD( 0 ); 
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );

			// randomization of the velocity
			WRITE_BYTE( 25 ); 

			// Model
			WRITE_SHORT( g_sModelIndexMetalGibs );	//model id#

			// # of shards
			WRITE_BYTE( RANDOM_LONG(15,25) );

			// duration
			WRITE_BYTE( 20 );// 3.0 seconds

			// flags

			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();

		// blast effect
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z);
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z + RANDOM_LONG(200,400) ); // reach damage radius over .2 seconds
		WRITE_SHORT( SpriteExplosion );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 4 ); // life
		WRITE_BYTE( 12 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 120 ); // brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	SetThink( &CBaseEntity::SUB_Remove );

	CSquadMonster::Killed( pevAttacker, GIB_NEVER );
}





//============================================================================================
//           A L I E N    S H I P
//============================================================================================




class CAlienShip : public CController
{
	DECLARE_CLASS(CAlienShip, CController);
public:
		void Spawn( void );
	void Precache( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunTask ( Task_t *pTask );
	void MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval );
	void SetYawSpeed(void);

	void PainSound(void);
	void AlertSound(void);
	void IdleSound( void );

	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void ClearEffects(void);
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int SpriteExplosion;
	CSprite *AlienEye;
	bool CriticalDamage;
	float SecondaryAttackTime;
	float DroneSpawnTime; // last time when the ship spawned a drone
	int MaxDrones; // maximum number of drones the ship can spawn
	int SpawnedDrones; // number of drones the ship already spawned
	float CriticalHitTime; // for critical hit sound (damage from balls). don't play it too often
	float NextAttackTime; // time when the ship is able to attack
	bool ResetAttack; // yep it's getting out of hand by now

	DECLARE_DATADESC();

	int ShootAttachment;
};

LINK_ENTITY_TO_CLASS( monster_alien_ship, CAlienShip);

BEGIN_DATADESC( CAlienShip )
	DEFINE_FIELD( AlienEye, FIELD_CLASSPTR ),
	DEFINE_FIELD( CriticalDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecEstVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CanInvestigate, FIELD_INTEGER ),
	DEFINE_FIELD( SecondaryAttackTime, FIELD_TIME ),
	DEFINE_FIELD( DroneSpawnTime, FIELD_TIME ),
	DEFINE_FIELD( MaxDrones, FIELD_INTEGER ),
	DEFINE_FIELD( SpawnedDrones, FIELD_INTEGER ),
	DEFINE_FIELD( CriticalHitTime, FIELD_TIME ),
	DEFINE_FIELD( NextAttackTime, FIELD_TIME ),
	DEFINE_FIELD( ResetAttack, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fInCombat, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flShootTime, FIELD_TIME ),
	DEFINE_FIELD( m_flShootEnd, FIELD_TIME ),
END_DATADESC()

int	CAlienShip :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_ALIEN_MILITARY;
}

void CAlienShip :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/alien_ship.mdl");
	UTIL_SetSize( pev, Vector( -400, -400, 0 ), Vector( 400, 400, 400 ));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;

	// this monster is using custom node_airs specifically for big monsters.
	// smaller drones will use regular node_airs and those two paths don't conflict with each other.
	// I feel it's a better solution than using path_tracks like apache does...
	SetFlag( F_MONSTER_BIGNODE );

	pev->flags			|= FL_FLY;
	m_bloodColor		= DONT_BLEED;
	if (!pev->health) pev->health	= 3000;
	pev->view_ofs		= Vector( 0, 0, -2 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_FULL;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	AlienEye = CSprite::SpriteCreate( ALIEN_EYE, GetAbsOrigin(), TRUE );
	if( AlienEye )
	{
		AlienEye->SetAttachment( edict(), 1 );
		AlienEye->SetScale( 4.0 );
//		AlienEye->SetTransparency( kRenderGlow, 255, 0, 0, 100, kRenderFxNoDissipation );
		AlienEye->SetTransparency( kRenderTransAdd, 255, 0, 0, 100, 0 );
		AlienEye->SetFadeDistance( 5000 );
	}

	SecondaryAttackTime = gpGlobals->time;

	DroneSpawnTime = gpGlobals->time + 5;
	MaxDrones = 5;
	SpawnedDrones = 0;

	CriticalHitTime = gpGlobals->time;

	NextAttackTime = gpGlobals->time;

	ResetAttack = true;

	SetFlag(F_METAL_MONSTER);

	MonsterInit();

	// override here after monsterinit
	m_flDistTooFar = 4096; 
	m_flDistLook = 6000;
	m_afCapability &= ~bits_CAP_CANSEEFLASHLIGHT;
	HealthBarType = 2;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CAlienShip :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/alien_ship.mdl");

	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDeathSounds );

//	PRECACHE_MODEL( "sprites/xspark4.spr");

	/*iDroneMuzzleFlash = */PRECACHE_MODEL("sprites/muzzle_shock_red.spr");
	PRECACHE_SOUND("drone/alienship_shoot1.wav");
	PRECACHE_SOUND("drone/alienship_shoot2.wav");
	PRECACHE_SOUND("drone/alienship_shoot3.wav");
	PRECACHE_SOUND("drone/alienship_shoot4.wav");

	PRECACHE_SOUND("buttons/spark1.wav");
	PRECACHE_SOUND("buttons/spark2.wav");
	PRECACHE_SOUND("buttons/spark3.wav");
	PRECACHE_SOUND("buttons/spark4.wav");
	PRECACHE_SOUND("buttons/spark5.wav");
	PRECACHE_SOUND("buttons/spark6.wav");

	PRECACHE_SOUND("drone/alienship_loop.wav");

	PRECACHE_SOUND("drone/drone_hit1.wav");
	PRECACHE_SOUND("drone/drone_hit2.wav");
	PRECACHE_SOUND("drone/drone_hit3.wav");
	PRECACHE_SOUND("drone/drone_hit4.wav");
	PRECACHE_SOUND("drone/drone_hit5.wav");

	PRECACHE_SOUND("drone/alienship_lowhp.wav");
	PRECACHE_SOUND("drone/alienship_hit.wav");
	PRECACHE_SOUND("drone/alienship_dronespawn.wav");

	PRECACHE_MODEL( ALIEN_EYE );

	UTIL_PrecacheOther("shock_beam");
	UTIL_PrecacheOther( "shootball" ); // combine ball
	PRECACHE_SOUND( "comball/spawn.wav" );
}

void CAlienShip::ClearEffects(void)
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, "drone/alienship_loop.wav" );
	if( AlienEye )
	{
		UTIL_Remove( AlienEye );
		AlienEye = NULL;
	}
}

void CAlienShip :: RunTask ( Task_t *pTask )
{
	// diffusion - don't update the sound if killed or being removed.
	if( (pev->deadflag == DEAD_NO) && !(pev->flags & FL_KILLME) )
	{
		if( m_iSoundState == 0 )
			m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
		else
		{
			static float pitch = 100;
			const int lim = 115;

			if( m_velocity.Length() > 50 )
				pitch++;
			else
				pitch--;

			pitch = bound( 90, pitch, lim );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "drone/alienship_loop.wav", 1, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch ); //1 volume, 0.2 radius
		}
	}
//----------------------------------------------------------------------------------------
	if (m_fInCombat)
	{
		Vector vecHand, vecAngle;
		GetAttachment( 2, vecHand, vecAngle );
	
		if (m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->time)
		{
			Vector vecSrc = vecHand + GetAbsVelocity() * (m_flShootTime - gpGlobals->time);
			Vector vecDir;
			
			if (m_hEnemy != NULL)
			{
				if( FInViewCone( m_hEnemy, 0.5 ) ) // ship has full field of view, so we must do another check for shooting
				{
					ALERT( at_aiconsole, "AlienShip: ATTACK!\n" );
					Vector	vecGunPos;
					Vector	vecGunAngles;

					CSprite *pMuzzleFlash = CSprite::SpriteCreate( "sprites/muzzle_shock_red.spr", GetAbsOrigin(), TRUE );
					if( pMuzzleFlash )
					{
						if( ShootAttachment == 0 )
							pMuzzleFlash->SetAttachment( edict(), 2 );
						else if( ShootAttachment == 1 )
							pMuzzleFlash->SetAttachment( edict(), 3 );
						pMuzzleFlash->pev->scale = 1.5;
						pMuzzleFlash->pev->rendermode = kRenderTransAdd;
						pMuzzleFlash->pev->renderamt = 255;
						pMuzzleFlash->AnimateAndDie( 25 );
					}

					if( ShootAttachment == 0 )
					{
						GetAttachment( 1, vecGunPos, vecGunAngles );
						ShootAttachment = 1;
					}
					else if( ShootAttachment == 1 )
					{
						GetAttachment( 2, vecGunPos, vecGunAngles );
						ShootAttachment = 0;
					}

					UTIL_MakeVectors( pev->angles );
					Vector vecShootOrigin = vecGunPos + gpGlobals->v_forward * 32;

					Vector vecShootDir = ShootAtEnemy( vecShootOrigin );
					vecGunAngles = UTIL_VecToAngles( vecShootDir );

					// the time has come for secondary attack
					if( gpGlobals->time > SecondaryAttackTime )
					{
						CBaseMonster *pBall = (CBaseMonster *)Create( "shootball", vecShootOrigin, vecGunAngles, edict() );
						pBall->pev->scale = 0.3;
						pBall->SetAbsVelocity( vecShootDir * 2000 );
						pBall->pev->spawnflags |= BIT( 12 ); // ENVBALL_ALIENSHIP: explode on first bounce
						pBall->pev->fuser2 = 75; // damage
						EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "drone/alienship_shoot1.wav", 1, 0.3, 0, RANDOM_LONG( 30, 50 ) );
						m_flShootTime += 2;
						SecondaryAttackTime = gpGlobals->time + RANDOM_FLOAT( 7.0, 15.0 );
					}
					else // regular attack
					{
						switch( RANDOM_LONG( 0, 3 ) )
						{
						case 0: EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "drone/alienship_shoot1.wav", 1, 0.3, 0, RANDOM_LONG( 80, 90 ) ); break;
						case 1: EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "drone/alienship_shoot2.wav", 1, 0.3, 0, RANDOM_LONG( 80, 90 ) ); break;
						case 2: EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "drone/alienship_shoot3.wav", 1, 0.3, 0, RANDOM_LONG( 80, 90 ) ); break;
						case 3: EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "drone/alienship_shoot4.wav", 1, 0.3, 0, RANDOM_LONG( 80, 90 ) ); break;
						}

						CBaseEntity *pShock = CBaseEntity::Create( "shock_beam", vecShootOrigin, vecGunAngles, edict() );
						if( pShock )
						{
							pShock->pev->spawnflags |= BIT( 0 ); // SHOCK_ALIENSHIP
							pShock->pev->dmg = 7.5;
							pShock->pev->velocity = vecShootDir * 4000;
							Vector AddVelocity = g_vecZero;
							if( g_iSkillLevel == SKILL_HARD )
								AddVelocity = m_hEnemy->pev->velocity + m_hEnemy->pev->basevelocity;
							else if( g_iSkillLevel == SKILL_MEDIUM )
								AddVelocity = m_hEnemy->pev->velocity.Normalize() * 150 + m_hEnemy->pev->basevelocity;
							pShock->pev->velocity += AddVelocity;
							pShock->pev->nextthink = gpGlobals->time;
						}
					}

					SetBlending( 0, vecGunAngles.x );
					// Play combat sound.
					CSoundEnt::InsertSound( bits_SOUND_COMBAT, GetAbsOrigin(), 1024, 0.3, ENTINDEX(edict()) );

					// spawn a drone when the time comes to it (and when we are able to attack)
					if( (gpGlobals->time > DroneSpawnTime) && (SpawnedDrones < MaxDrones) && HasConditions(bits_COND_CAN_RANGE_ATTACK1) )
					{
						Vector vecStart = GetAbsOrigin();
						vecStart.x -= 40 * SpawnedDrones;
						vecStart.z -= 100;  // spawn the drone at the bottom of the ship
						CBaseMonster *pDrone = (CBaseMonster *)Create( "monster_alien_drone", vecStart, GetAbsAngles(), edict() );
						if( pDrone )
						{
							pDrone->m_iClass = m_iClass; // do not shoot at the owner!!!
							// make the drone aware of the enemy which alien ship was angry at
							// even if the drone didn't see him at the moment of spawn.
							if( m_hEnemy != NULL )
							{
								pDrone->SetEnemy( m_hEnemy );
								pDrone->SetConditions( bits_COND_NEW_ENEMY );
							}
							pDrone->m_flDistTooFar = 4096; // those drones can shoot from afar
							DroneSpawnTime = gpGlobals->time + RANDOM_LONG( 20, 30 );
							SpawnedDrones++;
							EMIT_SOUND_DYN( edict(), CHAN_STATIC, "drone/alienship_dronespawn.wav", 1, 0.2, 0, RANDOM_LONG( 90, 110 ) );
						}
					}
				}
			}

			m_flShootTime += 0.2;
		}

		if ((m_flShootTime > m_flShootEnd) && (ResetAttack == false))
		{
			m_fInCombat = FALSE;
			m_flShootTime = m_flShootEnd = 0;
			NextAttackTime = gpGlobals->time + RANDOM_FLOAT(2.5,4.0);
			ResetAttack = true;
		//	ALERT(at_aiconsole, "AlienShip: ATTACK FINISHED!\n");
		}
	}

	if( m_hEnemy != NULL )
	{
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );
	}

	if( !m_fInCombat && (gpGlobals->time > NextAttackTime) && HasConditions(bits_COND_CAN_RANGE_ATTACK1) )
	{
		if( (ResetAttack == true) && (m_hEnemy != NULL) )
		{
			m_flShootTime = gpGlobals->time;
			m_flShootEnd = m_flShootTime + 4;
			pev->sequence = LookupActivity( ACT_RANGE_ATTACK1 );
			pev->frame = 0;
			ResetSequenceInfo();
			m_fInCombat = TRUE;
			ResetAttack = false;
		//	ALERT(at_aiconsole, "AlienShip: ATTACK RESET!\n");
		}
	}

	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT:
	case TASK_WAIT:
	case TASK_WAIT_FACE_ENEMY:
	case TASK_WAIT_PVS:
	{
		/*
		if( m_hEnemy != NULL )
		{
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );
		}

		if( !m_fInCombat && (gpGlobals->time > NextAttackTime) )
		{
			if( (ResetAttack == true) && (m_hEnemy != NULL) )
			{
				m_flShootTime = gpGlobals->time;
				m_flShootEnd = m_flShootTime + 4;
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK1 );
				pev->frame = 0;
				ResetSequenceInfo();
				m_fInCombat = TRUE;
				ResetAttack = false;
				//			ALERT(at_console, "RESET!\n");
			}
		}
		*/

		if( m_hTargetEnt != NULL ) // diffusion - look to my scripted_sequence
		{
			MakeIdealYaw( m_hTargetEnt->GetAbsOrigin() );
			ChangeYaw( 20 );// pev->yaw_speed // turn slowly. !!!this must be a editable value.
		}

		CSquadMonster::RunTask( pTask );
	}
	break;
	default: 
		CSquadMonster :: RunTask ( pTask );
	break;
	}
}

void CAlienShip::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval )
{
	if ( m_IdealActivity != m_movementActivity )
		m_IdealActivity = m_movementActivity;

	// ALERT( at_console, "move %.4f %.4f %.4f : %f\n", vecDir.x, vecDir.y, vecDir.z, flInterval );

	// float flTotal = m_flGroundSpeed * pev->framerate * flInterval;
	// UTIL_MoveToOrigin ( ENT(pev), m_Route[ m_iRouteIndex ].vecLocation, flTotal, MOVE_STRAFE );

//	ALERT( at_console, "m_movact %i\n", m_movementActivity );

	m_velocity = (m_velocity * 0.8 + m_flGroundSpeed * vecDir * 0.2) * 1.05;  // () - original code

	UTIL_MoveToOrigin ( ENT(pev), GetAbsOrigin() + m_velocity, m_velocity.Length() * flInterval, MOVE_STRAFE );

	// g-cont. see code of engine function SV_MoveStep for details
	SetAbsOrigin( pev->origin );	
}

void CAlienShip :: SetYawSpeed ( void )
{
	pev->yaw_speed = 15;
}

void CAlienShip :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	CBaseMonster::HandleAnimEvent( pEvent );
}

BOOL CAlienShip :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if( flDot > 0 && flDist > 128 && flDist <= 8192 )
	{
		TraceResult	tr;
		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, ENT( pev ), &tr );

		if( tr.flFraction == 1.0 )
			return TRUE;
	}

	return FALSE;
}

BOOL CAlienShip :: CheckRangeAttack2 ( float flDot, float flDist )
{
	return FALSE; // no secondary attack, maybe create one?
}


void CAlienShip :: PainSound( void )
{
	switch(RANDOM_LONG(0,4))
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}
}

void CAlienShip :: AlertSound(void)
{
//	switch(RANDOM_LONG(0,2))
//	{
//	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert1.wav", 1, ATTN_NORM ); break;
//	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert2.wav", 1, ATTN_NORM ); break;
//	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_alert3.wav", 1, ATTN_NORM ); break;
//	}
}

void CAlienShip :: IdleSound(void)
{
	// no sounds...yet?
}

void CAlienShip::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if (RANDOM_LONG(0,3) == 0)
		UTIL_Ricochet( ptr->vecEndPos, 1.0 );

	UTIL_Sparks ( ptr->vecEndPos );
	
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CAlienShip::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();
	
	if( FClassnameIs(pevInflictor, "shootball") || FClassnameIs(pevInflictor, "playerball") )
	{
		flDamage *= 3;
		Vector vecOrigin = GetAbsOrigin();
		
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrigin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + 600);
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 100 ); // scale x1.0
			WRITE_BYTE( 10 ); // framerate
			WRITE_BYTE( 30 ); // pos randomize
		MESSAGE_END();

		if( (gpGlobals->time > CriticalHitTime) && (pev->health > 500) )
		{
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, "drone/alienship_hit.wav", 1, 0.2, 0, RANDOM_LONG(90,110) );
			CriticalHitTime = gpGlobals->time + 5;
		}

		if( m_fInCombat )
			m_flShootTime += RANDOM_LONG(5,10);
	}

	if( FClassnameIs(pevInflictor, "crossbow_bolt") )
		flDamage *= 0.01;

	if( bitsDamageType & DMG_BULLET)
		flDamage *= 0.2;

	if( bitsDamageType & DMG_SHOCK )
		flDamage *= 2;

	if (pev->health < 500)
	{
		if( !CriticalDamage )
		{
			if( FClassnameIs(pevInflictor, "shootball") || FClassnameIs(pevInflictor, "playerball") )
			{
				// do nothing - because the sound can be played while ship is already dead
			}
			else
				EMIT_SOUND_DYN( edict(), CHAN_STATIC, "drone/alienship_lowhp.wav", 1, 0.2, 0, RANDOM_LONG(90,110) );

			CriticalDamage = true;
		}
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CAlienShip::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();
	
	pev->renderfx = kRenderFxExplode;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 0;
	pev->rendercolor.z = 0;
	StopAnimation();
	SetThink( &CBaseEntity::SUB_Remove );
	int i;

	int parts = MODEL_FRAMES( g_sModelIndexMetalGibs );
	for ( i = 0; i < 10; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );
				
		int bodyPart = 0;
		if ( parts > 1 )
			bodyPart = RANDOM_LONG( 0, pev->body-1 );

		pGib->pev->body = bodyPart;
		pGib->m_bloodColor = DONT_BLEED;
		pGib->m_material = matMetal;
		pGib->SetAbsOrigin( GetAbsOrigin() );
//		pGib->SetAbsVelocity( UTIL_RandomBloodVector() * RANDOM_FLOAT( 1, 2 ));
		pGib->SetNextThink( 1.25 );
		pGib->SetThink( &CBaseEntity::SUB_FadeOut );
	}
	
//------------------------------------------------------------
	Vector position = GetAbsOrigin();
	position.z += 32;
	DroneExplosion( position, 75, 0.1, 250, 4 ); //60 + (i*20)	
//------------------------------------------------------------	
		Vector vecOrigin = GetAbsOrigin();
			
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_BREAKMODEL);

			// position
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + 32 );

			// size     (this doesn't seem to change anything at all)
			WRITE_COORD( 16 );
			WRITE_COORD( 16 );
			WRITE_COORD( 16 );

			// velocity
			WRITE_COORD( 0 ); 
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );

			// randomization of the velocity
			WRITE_BYTE( 25 ); 

			// Model
			WRITE_SHORT( g_sModelIndexMetalGibs );	//model id#

			// # of shards
			WRITE_BYTE( RANDOM_LONG(40,50) );

			// duration
			WRITE_BYTE( 20 );// 3.0 seconds

			// flags

			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();

		// blast effect
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z);
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z + RANDOM_LONG(400,800) ); // reach damage radius over .2 seconds
		WRITE_SHORT( SpriteExplosion );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 4 ); // life
		WRITE_BYTE( 12 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 120 ); // brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	CSquadMonster::Killed( pevAttacker, GIB_NEVER );
}