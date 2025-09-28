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
// monster template
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "talkmonster.h"
#include "game/schedule.h"
#include "game/defaultai.h"
#include "game/scripted.h"
#include "weapons/weapons.h"
#include "entities/soundent.h"
#include "plane.h"
#include "animation.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
// first flag is barney dying for scripted sequences?
#define	BARNEY_AE_DRAW		( 2 )
#define	BARNEY_AE_SHOOT		( 3 )
#define	BARNEY_AE_HOLSTER		( 4 )
#define	BARNEY_AE_RELOAD	( 5 )

#define	BARNEY_BODY_GUNHOLSTERED	0
#define	BARNEY_BODY_GUNDRAWN	1
#define	BARNEY_BODY_GUNGONE		2

typedef enum
{
  SCHED_BARNEY_HOLSTER = LAST_TALKMONSTER_SCHEDULE + 1,
  SCHED_BARNEY_COVER_AND_RELOAD, // diffusion - grunt code
	//Alice only!
  SCHED_ALICE_WAIT_FACE_ENEMY,
  SCHED_ALICE_TAKECOVER_FAILED,
  SCHED_ALICE_GETBACKTOPLAYER
};
 
typedef enum
{
  TASK_BARNEY_HOLSTER = LAST_TALKMONSTER_TASK + 1,
};


class CBarney : public CTalkMonster
{
	DECLARE_CLASS( CBarney, CTalkMonster );
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void BarneyFirePistol( void );
	void AlertSound( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	void RunTask( Task_t *pTask );
	void StartTask( Task_t *pTask );
	int ObjectCaps( void );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	
	void DeclineFollowing( void );

	// Override these to set behavior
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	void DeathSound( void );
	void PainSound( void );
	
	void TalkInit( void );

	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	void Killed( entvars_t *pevAttacker, int iGib );

	DECLARE_DATADESC();

	int	m_iBaseBody; //LRC - for barneys with different bodies
	BOOL	m_fGunDrawn;
	float	m_painTime;
	float	m_checkAttackTime;
	BOOL	m_lastAttackCheck;

	// UNDONE: What is this for?  It isn't used?
	float	m_flPlayerDamage;// how much pain has the player inflicted on me?

	int m_iClipSize;
	float m_flNextHolsterTime;
	float m_flIdleReloadTime;

	void RunAI(void);

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_barney, CBarney );

BEGIN_DATADESC( CBarney )
	DEFINE_FIELD( m_iBaseBody, FIELD_INTEGER ), //LRC
	DEFINE_FIELD( m_fGunDrawn, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_painTime, FIELD_TIME ),
	DEFINE_FIELD( m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flPlayerDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_iClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextHolsterTime, FIELD_TIME ),
END_DATADESC()

int CBarney::ObjectCaps(void)
{
	int flags = FCAP_IMPULSE_USE;

	if( pev->spawnflags & SF_MONSTER_NOFOLLOW )
		flags = 0;

	return BaseClass :: ObjectCaps() | flags;
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlBaFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slBaFollow[] =
{
	{
		tlBaFollow,
		SIZEOFARRAY ( tlBaFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Follow"
	},
};

//=========================================================
// BarneyDraw- much better looking draw schedule for when
// barney knows who he's gonna attack.
//=========================================================
Task_t	tlBarneyEnemyDraw[] =
{
	{ TASK_STOP_MOVING,					0				},
	{ TASK_FACE_ENEMY,					0				},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,	(float) ACT_ARM },
};

Schedule_t slBarneyEnemyDraw[] = 
{
	{
		tlBarneyEnemyDraw,
		SIZEOFARRAY ( tlBarneyEnemyDraw ),
		0,
		0,
		"Barney Enemy Draw"
	}
};

Task_t	tlBaFaceTarget[] =
{
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_ALICE_FACE_FOLLOW,			(float)0		},
//	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_CHASE },
};

Schedule_t	slBaFaceTarget[] =
{
	{
		tlBaFaceTarget,
		SIZEOFARRAY ( tlBaFaceTarget ),
		bits_COND_CLIENT_PUSH	|
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"FaceTarget"
	},
};


Task_t	tlIdleBaStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_TLK_HEADRESET,		(float)0		}, // reset head position
};

Task_t	tlBarneyHolster[] =
{
  { TASK_STOP_MOVING,			0					},
  { TASK_PLAY_SEQUENCE,		(float) ACT_DISARM	},
};

Schedule_t slBarneyHolster[] =
{
  {
    tlBarneyHolster,
    SIZEOFARRAY ( tlBarneyHolster ),
    bits_COND_HEAVY_DAMAGE,
 
    0,
    "Barney Holster"
  }
};

Schedule_t	slIdleBaStand[] =
{
	{ 
		tlIdleBaStand,
		SIZEOFARRAY ( tlIdleBaStand ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND	|
		bits_COND_SMELL			|
		bits_COND_PROVOKED,

		bits_SOUND_COMBAT		|// sound flags - change these, and you'll break the talking code.
		//bits_SOUND_PLAYER		|
		//bits_SOUND_WORLD		|
		
		bits_SOUND_DANGER		|
		bits_SOUND_MEAT			|// scents
		bits_SOUND_CARCASS		|
		bits_SOUND_GARBAGE,
		"IdleStand"
	},
};

Task_t tlBaRangeAttack1[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, (float)0 },
//	{ TASK_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
};

Schedule_t slBaRangeAttack1[] =
{
	{
		tlBaRangeAttack1,
		SIZEOFARRAY( tlBaRangeAttack1 ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_ENEMY_LOST |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NO_AMMO_LOADED |
//		bits_COND_NOFIRE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Range Attack1"
	},
};

// diffusion - cover and reload
Task_t	tlBaHideReload[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RELOAD			},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_RELOAD			},
};

Schedule_t slBaHideReload[] = 
{
	{
		tlBaHideReload,
		SIZEOFARRAY ( tlBaHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"BaHideReload"
	}
};

// diffusion - grunt code for alice
Task_t	tlAliceWaitInCover[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_WAIT_FACE_ENEMY,			(float)3				},
};

Schedule_t	slAliceWaitInCover[] =
{
	{ 
		tlAliceWaitInCover,
		SIZEOFARRAY ( tlAliceWaitInCover ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_HEAR_SOUND		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2,

		bits_SOUND_DANGER,
		"AliceWaitInCover"
	},
};

Task_t	tlAliceGetBackToPlayer[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128 },	// Move within 128 of target ent (client)
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_TAKE_COVER_FROM_ENEMY },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slAliceGetBackToPlayer[] =
{
	{
		tlAliceGetBackToPlayer,
		SIZEOFARRAY( tlAliceGetBackToPlayer ),
		0,

		0,
		"AliceGetBackToPlayer"
	},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t	tlAliceTakeCover1[] =
{
	{ TASK_STOP_MOVING,				(float)0							},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_ALICE_TAKECOVER_FAILED	},
	{ TASK_WAIT,					(float)0.1							},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0							},
	{ TASK_RUN_PATH,				(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0							},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER			},
	{ TASK_SET_SCHEDULE,			(float)SCHED_ALICE_WAIT_FACE_ENEMY	},
};

Schedule_t	slAliceTakeCover[] =
{
	{ 
		tlAliceTakeCover1,
		SIZEOFARRAY ( tlAliceTakeCover1 ), 
		0,
		0,
		"TakeCover"
	},
};

DEFINE_CUSTOM_SCHEDULES( CBarney )
{
	slBaFollow,
	slBarneyEnemyDraw,
	slBaFaceTarget,
	slIdleBaStand,
	slBarneyHolster,
	slBaRangeAttack1, // freeslave
	slBaHideReload, // diffusion
	slAliceWaitInCover,
	slAliceTakeCover,
	slAliceGetBackToPlayer,
};


IMPLEMENT_CUSTOM_SCHEDULES( CBarney, CTalkMonster );

void CBarney :: StartTask( Task_t *pTask )
{
	CTalkMonster::StartTask( pTask );	
}

void CBarney :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		if (m_hEnemy != NULL && (m_hEnemy->IsPlayer()))
		{
			pev->framerate = 1.5;
		}
		CTalkMonster::RunTask( pTask );
		break;
	default:
		CTalkMonster::RunTask( pTask );
		break;
	}
}




//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CBarney :: ISoundMask ( void) 
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_GARBAGE	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CBarney :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_PLAYER_ALLY;
}

//=========================================================
// ALertSound - barney says "Freeze!"
//=========================================================
void CBarney :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( FOkToSpeak() )
			PlaySentence( "BA_ATTACK", RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE );
	}

}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CBarney :: SetYawSpeed ( void )
{
	int ys = 140;

	switch ( m_Activity )
	{
	case ACT_IDLE:		
		ys = 160;
		break;
	case ACT_WALK:
		ys = 170;
		break;
	case ACT_RUN:
		ys = 180;
		break;
	}

	pev->yaw_speed = ys;
}


//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CBarney :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDist <= 1024 && flDot >= 0.5 )
	{
		if ( gpGlobals->time > m_checkAttackTime )
		{
			TraceResult tr;
			
			Vector shootOrigin = GetAbsOrigin() + Vector( 0, 0, 55 );
			CBaseEntity *pEnemy = m_hEnemy;
			Vector shootTarget = ( (pEnemy->BodyTarget( shootOrigin ) - pEnemy->GetAbsOrigin()) + m_vecEnemyLKP );
			UTIL_TraceLine( shootOrigin, shootTarget, dont_ignore_monsters, ENT(pev), &tr );
			m_checkAttackTime = gpGlobals->time + 1;
			if ( tr.flFraction == 1.0 || (tr.pHit != NULL && CBaseEntity::Instance(tr.pHit) == pEnemy) )
				m_lastAttackCheck = TRUE;
			else
				m_lastAttackCheck = FALSE;
			m_checkAttackTime = gpGlobals->time + 1.5;
		}
		return m_lastAttackCheck;
	}
	return FALSE;
}


//=========================================================
// BarneyFirePistol - shoots one round from the pistol at
// the enemy barney is facing.
//=========================================================
void CBarney :: BarneyFirePistol ( void )
{
	Vector vecShootOrigin;

	UTIL_MakeVectors(GetAbsAngles());
	vecShootOrigin = GetAbsOrigin() + Vector( 0, 0, 55 );
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );
	pev->effects |= EF_MUZZLEFLASH;

	if (pev->frags)
	{
		FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_2DEGREES, 1024, BULLET_PLAYER_357);
		if (RANDOM_LONG(0, 1))
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/357_shot1.wav", 1, ATTN_NORM, 0, 100 );
		else
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/357_shot2.wav", 1, ATTN_NORM, 0, 100 );
	}
	else
	{
		FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_2DEGREES, 1024, BULLET_MONSTER_9MM );

		int pitchShift = RANDOM_LONG( 0, 20 );
	
		// Only shift about half the time
		if ( pitchShift > 10 )
			pitchShift = 0;
		else
			pitchShift -= 5;

		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "barney/ba_attack2.wav", 1, ATTN_NORM, 0, 100 + pitchShift );
	}

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 384, 0.3, ENTINDEX(edict()) );

	m_cAmmoLoaded--;// take away a bullet!

	if ( m_cAmmoLoaded <= 0 )
		SetConditions(bits_COND_NO_AMMO_LOADED);
}
		
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CBarney :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case BARNEY_AE_SHOOT:
		BarneyFirePistol();
		break;

	case BARNEY_AE_DRAW:
		// barney's bodygroup switches here so he can pull gun from holster
		pev->body = m_iBaseBody + BARNEY_BODY_GUNDRAWN;
		m_fGunDrawn = TRUE;
		break;

	case BARNEY_AE_HOLSTER:
		// change bodygroup to replace gun in holster
		pev->body = m_iBaseBody + BARNEY_BODY_GUNHOLSTERED;
		m_fGunDrawn = FALSE;
		break;

	case BARNEY_AE_RELOAD:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barney/ba_reload1.wav", 1, ATTN_NORM );
		m_cAmmoLoaded = m_iClipSize;
		ClearConditions( bits_COND_NO_AMMO_LOADED );
		break;


	default:
		CTalkMonster::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Spawn
//=========================================================
void CBarney :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/barney.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	if( !pev->health ) pev->health = 75;
	pev->max_health = pev->health;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	m_iBaseBody = pev->body; //LRC
	pev->body			= m_iBaseBody + BARNEY_BODY_GUNHOLSTERED; // gun in holster
	m_fGunDrawn			= FALSE;

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_iClipSize = 15;
	m_cAmmoLoaded = m_iClipSize;
	m_flNextHolsterTime = gpGlobals->time;

	MonsterInit();
	SetUse( &CTalkMonster::FollowerUse );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBarney :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/barney.mdl");

	PRECACHE_SOUND("barney/ba_attack1.wav" );
	PRECACHE_SOUND("barney/ba_attack2.wav" );

	PRECACHE_SOUND("barney/ba_pain1.wav");
	PRECACHE_SOUND("barney/ba_pain2.wav");
	PRECACHE_SOUND("barney/ba_pain3.wav");

	PRECACHE_SOUND("barney/ba_die1.wav");
	PRECACHE_SOUND("barney/ba_die2.wav");
	PRECACHE_SOUND("barney/ba_die3.wav");

	PRECACHE_SOUND("barney/ba_reload1.wav");
	
	// every new barney must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to 0)
	TalkInit();
	CTalkMonster::Precache();
}	

// Init talk data
void CBarney :: TalkInit()
{
	
	CTalkMonster::TalkInit();

	// barney speach group names (group names are in sentences.txt)

	if (!m_iszSpeakAs)
	{
		m_szGrp[TLK_ANSWER]		=	"BA_ANSWER";
		m_szGrp[TLK_QUESTION]	=	"BA_QUESTION";
		m_szGrp[TLK_IDLE]		=	"BA_IDLE";
		m_szGrp[TLK_STARE]		=	"BA_STARE";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER) //LRC
			m_szGrp[TLK_USE]	=	"BA_PFOLLOW";
		else
			m_szGrp[TLK_USE] =	"BA_OK";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER)
			m_szGrp[TLK_UNUSE] = "BA_PWAIT";
		else
			m_szGrp[TLK_UNUSE] = "BA_WAIT";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER)
			m_szGrp[TLK_DECLINE] =	"BA_POK";
		else
			m_szGrp[TLK_DECLINE] =	"BA_NOTOK";
		m_szGrp[TLK_STOP] =		"BA_STOP";

		m_szGrp[TLK_NOSHOOT] =	"BA_SCARED";
		m_szGrp[TLK_HELLO] =	"BA_HELLO";

		m_szGrp[TLK_PLHURT1] =	"!BA_CUREA";
		m_szGrp[TLK_PLHURT2] =	"!BA_CUREB"; 
		m_szGrp[TLK_PLHURT3] =	"!BA_CUREC";

		m_szGrp[TLK_PHELLO] =	NULL;	//"BA_PHELLO";		// UNDONE
		m_szGrp[TLK_PIDLE] =	NULL;	//"BA_PIDLE";			// UNDONE
		m_szGrp[TLK_PQUESTION] = "BA_PQUEST";		// UNDONE

		m_szGrp[TLK_SMELL] =	"BA_SMELL";
	
		m_szGrp[TLK_WOUND] =	"BA_WOUND";
		m_szGrp[TLK_MORTAL] =	"BA_MORTAL";
	}

	// get voice for head - just one barney voice for now
	m_voicePitch = 100;
}

int CBarney :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if( HasSpawnFlags(SF_MONSTER_NODAMAGE))
		return 0;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return 0;
	
	// make sure friends talk about it if player hurts talkmonsters...
	int ret = CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return ret;

	// LRC - if my reaction to the player has been overridden, don't do this stuff
	if (m_iPlayerReact) return ret;

	if ( m_MonsterState != MONSTERSTATE_PRONE && (pevAttacker->flags & FL_CLIENT) )
	{
		m_flPlayerDamage += flDamage;

		// This is a heurstic to determine if the player intended to harm me
		// If I have an enemy, we can't establish intent (may just be crossfire)
		if ( m_hEnemy == NULL )
		{
			// If the player was facing directly at me, or I'm already suspicious, get mad
			if( (m_afMemory & bits_MEMORY_SUSPICIOUS) || UTIL_IsFacing( pevAttacker, GetAbsOrigin() ))
			{
				// Alright, now I'm pissed!
				if( !(pev->spawnflags & SF_MONSTER_GAG) )
				{
					if (m_iszSpeakAs)
					{
						char szBuf[32];
						strcpy(szBuf,STRING(m_iszSpeakAs));
						strcat(szBuf,"_MAD");
						PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
					}
					else
						PlaySentence( "BA_MAD", 4, VOL_NORM, ATTN_NORM );
				}

				Remember( bits_MEMORY_PROVOKED );
				StopFollowing( TRUE );
			}
			else
			{
				// Hey, be careful with that
				if( !(pev->spawnflags & SF_MONSTER_GAG) )
				{
					if (m_iszSpeakAs)
					{
						char szBuf[32];
						strcpy(szBuf,STRING(m_iszSpeakAs));
						strcat(szBuf,"_SHOT");
						PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
					}
					else
						PlaySentence( "BA_SHOT", 4, VOL_NORM, ATTN_NORM );
				}
				Remember( bits_MEMORY_SUSPICIOUS );
			}
		}
		else if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
		{
			if( !(pev->spawnflags & SF_MONSTER_GAG) )
			{
				if (m_iszSpeakAs)
				{
					char szBuf[32];
					strcpy(szBuf,STRING(m_iszSpeakAs));
					strcat(szBuf,"_SHOT");
					PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
				}
				else
					PlaySentence( "BA_SHOT", 4, VOL_NORM, ATTN_NORM );
			}
		}
	}

	return ret;
}

	
//=========================================================
// PainSound
//=========================================================
void CBarney :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(1, 1.5); // 0.5-0.75? too often!

	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CBarney :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}


void CBarney::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	switch( ptr->iHitgroup)
	{
	case HITGROUP_CHEST:
	case HITGROUP_STOMACH:
		if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST))
		{
			flDamage = flDamage / 2;
		}
		break;
	case 10:
		if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB))
		{
			flDamage -= 20;
			if (flDamage <= 0)
			{
				UTIL_Ricochet( ptr->vecEndPos, 1.0 );
				flDamage = 0.01;
			}
		}
		// always a head shot
		ptr->iHitgroup = HITGROUP_HEAD;
		break;
	}

	CTalkMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


void CBarney::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( pev->body < m_iBaseBody + BARNEY_BODY_GUNGONE && !(pev->spawnflags & SF_MONSTER_NO_WPN_DROP))
	{// drop the gun!
		Vector vecGunPos;
		Vector vecGunAngles;

		pev->body = m_iBaseBody + BARNEY_BODY_GUNGONE;

		GetAttachment( 0, vecGunPos, vecGunAngles );
		
		CBaseEntity *pGun;
		if (pev->frags)
			pGun = DropItem( "weapon_357", vecGunPos, vecGunAngles );
		else
			pGun = DropItem( "weapon_9mmhandgun", vecGunPos, vecGunAngles );
	}

	SetUse( NULL );	
	CTalkMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CBarney :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	case SCHED_ARM_WEAPON:
		if ( m_hEnemy != NULL )
		{
			// face enemy, then draw.
			return slBarneyEnemyDraw;
		}
		break;

	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		// call base class default so that barney will talk
		// when 'used' 
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
			return slBaFaceTarget;	// override this for different target face behavior
		else
			return psched;

	case SCHED_TARGET_CHASE:
		return slBaFollow;

	case SCHED_IDLE_STAND:
		// call base class default so that scientist will talk
		// when standing during idle
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
		{
			// just look straight ahead.
			return slIdleBaStand;
		}
		else
			return psched;	

	case SCHED_BARNEY_HOLSTER:
		if ( m_hEnemy == NULL )
			return slBarneyHolster;
		break;

	case SCHED_RANGE_ATTACK1:
		return slBaRangeAttack1;

	case SCHED_BARNEY_COVER_AND_RELOAD:
		return &slBaHideReload[ 0 ];
	}

	return CTalkMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CBarney :: GetSchedule ( void )
{
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
		PlaySentence( "BA_KILL", 4, VOL_NORM, ATTN_NORM );

	switch( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD | bits_COND_ENEMY_LOST ) )
			{
				m_flNextHolsterTime = gpGlobals->time + RANDOM_FLOAT( 6.0, 10.0 );
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

			// always act surprized with a new enemy
			if ( HasConditions( bits_COND_NEW_ENEMY ) && HasConditions( bits_COND_LIGHT_DAMAGE) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );
				
			// wait for one schedule to draw gun
			if (!m_fGunDrawn )
				return GetScheduleOfType( SCHED_ARM_WEAPON );

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );

			if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
				return GetScheduleOfType ( SCHED_RELOAD );
		}
		break;

	case MONSTERSTATE_ALERT:	
	case MONSTERSTATE_IDLE:
		if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		{
			// flinch if hurt
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
		}

		if ( m_fGunDrawn && m_hEnemy == NULL && m_flNextHolsterTime < gpGlobals->time )
				return GetScheduleOfType( SCHED_BARNEY_HOLSTER);
	
		if ( m_hEnemy == NULL && IsFollowing() )
		{
			if ( !m_hTargetEnt->IsAlive() )
			{
				// UNDONE: Comment about the recently dead player here?
				StopFollowing( FALSE );
				break;
			}
			else
			{
				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
					return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );

				return GetScheduleOfType( SCHED_TARGET_FACE );
			}
		}

		if ( HasConditions( bits_COND_CLIENT_PUSH ) )
		{
			return GetScheduleOfType( SCHED_MOVE_AWAY );
		}

		// try to say something about smells
		TrySmellTalk();
		break;
	}
	
	return CTalkMonster::GetSchedule();
}

MONSTERSTATE CBarney :: GetIdealState ( void )
{
	return CTalkMonster::GetIdealState();
}

void CBarney::DeclineFollowing( void )
{
	if( !(pev->spawnflags & SF_MONSTER_GAG) )
		PlaySentence( m_szGrp[TLK_DECLINE], 2, VOL_NORM, ATTN_NORM ); //LRC
}

void CBarney::RunAI(void)
{
	CBaseMonster :: RunAI();
}	

//=========================================================
// DEAD BARNEY PROP
//
// Designer selects a pose in worldcraft, 0 through num_poses-1
// this value is added to what is selected as the 'first dead pose'
// among the monster's normal animations. All dead poses must
// appear sequentially in the model file. Be sure and set
// the m_iFirstPose properly!
//
//=========================================================
class CDeadBarney : public CBaseMonster
{
	DECLARE_CLASS( CDeadBarney, CBaseMonster );
public:
	void Spawn( void );
	int Classify ( void ) { return CLASS_PLAYER_ALLY; }

	void KeyValue( KeyValueData *pkvd );

	int m_iPose;// which sequence to display	-- temporary, don't need to save
	static char *m_szPoses[3];
};

char *CDeadBarney::m_szPoses[] = { "lying_on_back", "lying_on_side", "lying_on_stomach" };

void CDeadBarney::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else 
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_barney_dead, CDeadBarney );

//=========================================================
// ********** DeadBarney SPAWN **********
//=========================================================
void CDeadBarney :: Spawn( )
{
	PRECACHE_MODEL("models/barney.mdl");
	SET_MODEL(ENT(pev), "models/barney.mdl");

	pev->yaw_speed		= 8;
	pev->sequence		= 0;
	m_bloodColor		= BLOOD_COLOR_RED;

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );
	if (pev->sequence == -1)
	{
		ALERT ( at_console, "Dead barney with bad pose\n" );
	}
	// Corpses have less health
	pev->health			= 8;//gSkillData.barneyHealth;

	MonsterInitDead();
}






















//=============================================================================================================================================
//============================== Alice ========================================================================================================
//=============================================================================================================================================


class CAlice : public CBarney
{
	DECLARE_CLASS(CAlice, CBarney);
public:
	void Spawn( void );
	void Precache( void );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	void BarneyFirePistol( void );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	Schedule_t *GetSchedule ( void );
	Schedule_t *GetScheduleOfType ( int Type );
	int DamageDecal( int bitsDamageType );
	void RunAI(void);
	void SetActivity( Activity NewActivity );

	void TalkInit(void);
	void AlertSound(void);
	void PainSound(void);
	void DeathSound(void);
	void Killed( entvars_t *pevAttacker, int iGib );

	// Alice only!
	void AliceUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL CineCleanup( void );
	void AliceSetFollowing( void );
	void AliceSendHUDData( bool enable );
	float AliceLastHurt;
	float CoverFailTime;
	float NewEnemySentenceTime;
	bool bAliceFollowing;
	float next_hud_update; // not saved
	int health_cached; // not saved

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( monster_alice , CAlice);

BEGIN_DATADESC( CAlice )
	DEFINE_FUNCTION( AliceUse ),
	DEFINE_FIELD( AliceLastHurt, FIELD_TIME ),
	DEFINE_FIELD( CoverFailTime, FIELD_TIME ),
	DEFINE_FIELD( m_flIdleReloadTime, FIELD_TIME ),
	DEFINE_FIELD( NewEnemySentenceTime, FIELD_TIME ),
	DEFINE_FIELD( bAliceFollowing, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iBaseBody, FIELD_INTEGER ), //LRC
	DEFINE_FIELD( m_fGunDrawn, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_painTime, FIELD_TIME ),
	DEFINE_FIELD( m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flPlayerDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_iClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextHolsterTime, FIELD_TIME ),
END_DATADESC()

void CAlice::Precache( void )
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/alice.mdl");

	PRECACHE_SOUND("barney/ba_attack1.wav" );
	PRECACHE_SOUND("barney/ba_attack2.wav" );

	PRECACHE_SOUND("alice/pain1.wav");
	PRECACHE_SOUND("alice/pain2.wav");
	PRECACHE_SOUND("alice/pain3.wav");

	PRECACHE_SOUND("alice/die1.wav");
	PRECACHE_SOUND("alice/die2.wav");
	PRECACHE_SOUND("alice/die3.wav");

	PRECACHE_SOUND("barney/ba_reload1.wav");
	
	// every new barney must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to 0)
	TalkInit();
	CTalkMonster::Precache();

	if( !next_hud_update ) // need to delay this after save-restore. It sends out too early!
		next_hud_update = gpGlobals->time + 0.1;
}

void CAlice :: Spawn( void )
{
	Precache();

	pev->spawnflags |= SF_MONSTER_NOFOLLOW;
	ObjectCaps(); // refresh after setting this flag

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/alice.mdl");

	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= DONT_BLEED;
	if( !pev->health ) pev->health = 500;
	pev->max_health = pev->health;
	pev->view_ofs		= Vector ( 0, 0, 48 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE;
	m_MonsterState		= MONSTERSTATE_NONE;

	m_iBaseBody = pev->body; //LRC
	pev->body			= m_iBaseBody + BARNEY_BODY_GUNHOLSTERED; // gun in holster
	m_fGunDrawn			= FALSE;

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_iClipSize = 15;
	m_cAmmoLoaded = m_iClipSize;
	m_flNextHolsterTime = gpGlobals->time;
	m_flIdleReloadTime = gpGlobals->time;
	CoverFailTime = gpGlobals->time;

	MonsterInit();
	SetUse( &CAlice::AliceUse );
	SetFlag( F_FIRE_IMMUNE );

	m_flDistTooFar = 4096;
}

void CAlice::AliceSendHUDData( bool enable )
{
	if( health_cached == (int)pev->health )
		return;

	CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
	MESSAGE_BEGIN( MSG_ONE, gmsgHealthVisualAlice, NULL, pPlayer->pev );
		WRITE_BYTE( (byte)enable );
		WRITE_BYTE( (byte)RemapVal( pev->health, 0, pev->max_health, 0, 100 ) );
	MESSAGE_END();
	
	health_cached = (int)pev->health;
}

void CAlice::AliceSetFollowing( void )
{
	// do not change following state during a sequence
	// it will be restored later post-sequence
	if( m_pCine )
		return;
	
	if( bAliceFollowing )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
		if( !pPlayer )
		{
			bAliceFollowing = false;
			return;
		}

		if( m_hEnemy != NULL )
			m_IdealMonsterState = MONSTERSTATE_ALERT;

		m_hTargetEnt = pPlayer;
		ClearConditions( bits_COND_CLIENT_PUSH );
		ClearSchedule();
		ALERT( at_aiconsole, "Alice is now following the player.\n" );
	}
	else // stop following
	{
		if( m_movementGoal == MOVEGOAL_TARGETENT )
			RouteClear(); // Stop him from walking toward the player
		m_hTargetEnt = NULL;
		// ClearSchedule();
		if( m_hEnemy != NULL )
			m_IdealMonsterState = MONSTERSTATE_COMBAT;
	}

	health_cached = pev->health + 1;
	AliceSendHUDData( bAliceFollowing );
}

BOOL CAlice::CineCleanup( void )
{
	CBaseMonster::CineCleanup();

	// start following the player immediately
	AliceSetFollowing();

	return TRUE;
}

void CAlice::AliceUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_ON )
		bAliceFollowing = true;
	else if( useType == USE_OFF )
		bAliceFollowing = false;
	else // toggle
		bAliceFollowing = !bAliceFollowing;

	AliceSetFollowing();
}

BOOL CAlice :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDist <= 2048 && flDot >= 0.5 )
	{
		if ( gpGlobals->time > m_checkAttackTime )
		{
			TraceResult tr;
		
			Vector shootOrigin = GetAbsOrigin() + Vector( 0, 0, 55 );
			CBaseEntity *pEnemy = m_hEnemy;

			Vector shootTarget = ( (pEnemy->BodyTarget( shootOrigin ) - pEnemy->GetAbsOrigin()) + m_vecEnemyLKP );
			UTIL_TraceLine( shootOrigin, shootTarget, dont_ignore_monsters, ENT(pev), &tr );
			m_checkAttackTime = gpGlobals->time + 0.2;
			if ( tr.flFraction == 1.0 || (tr.pHit != NULL && CBaseEntity::Instance(tr.pHit) == pEnemy) )
				m_lastAttackCheck = TRUE;
			else
				m_lastAttackCheck = FALSE;
			m_checkAttackTime = gpGlobals->time + 0.2;
		}
		return m_lastAttackCheck;
	}
	return FALSE;
}

void CAlice :: BarneyFirePistol ( void )
{
	Vector vecShootOrigin;

	UTIL_MakeVectors(GetAbsAngles());
	vecShootOrigin = GetAbsOrigin() + Vector( 0, 0, 55 );
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	pev->effects |= EF_MUZZLEFLASH;

	if (pev->frags)
	{
		FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_2DEGREES, 2048, BULLET_PLAYER_357);
		if (RANDOM_LONG(0, 1))
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/357_shot1.wav", 1, ATTN_NORM, 0, 100 );
		else
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/357_shot2.wav", 1, ATTN_NORM, 0, 100 );
	}
	else
	{
		FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_1DEGREES, 2048, BULLET_MONSTER_9MM, 1, 15); // 15 dmg

		int pitchShift = RANDOM_LONG( 0, 20 );
	
		// Only shift about half the time
		if ( pitchShift > 10 )
			pitchShift = 0;
		else
			pitchShift -= 5;

		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "barney/ba_attack2.wav", 1, ATTN_NORM, 0, 100 + pitchShift );
	}

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 384, 0.3, ENTINDEX(edict()) );

	m_cAmmoLoaded--;// take away a bullet!

	if ( m_cAmmoLoaded <= 0 )
		SetConditions(bits_COND_NO_AMMO_LOADED);

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );
}

int CAlice :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{	
	if( HasSpawnFlags( SF_MONSTER_NODAMAGE ) )
		return 0;
	
	if( pev->health < pev->max_health )
		AliceLastHurt = gpGlobals->time;
	
	if (pevAttacker->flags & FL_CLIENT) // player can't hurt Alice!
		return 0;

	if( bitsDamageType & DMG_BLAST )
	{
		if( g_iSkillLevel == SKILL_EASY ) // immunity (almost) from explosion damage in easy mode
			flDamage *= 0.01;
		else if( g_iSkillLevel == SKILL_MEDIUM ) // half-dmg from explosions in medium mode
			flDamage *= 0.5;
	}

	if( g_iSkillLevel == SKILL_EASY )  // some chance that incoming damage will be almost nullified
	{
		switch( RANDOM_LONG( 0, 3 ) )
		{
		case 0: flDamage *= 0.01; break;
		}
	}
	else if( g_iSkillLevel == SKILL_MEDIUM )  // some chance that incoming damage will be halved
	{
		switch( RANDOM_LONG( 0, 3 ) )
		{
		case 0: flDamage *= 0.5; break;
		}
	}

	int ret = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);

	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return ret;

	return ret;
}

void CAlice :: RunAI(void) // health regeneration
{
	if( pev->health < pev->max_health && IsAlive() )
	{
		if( gpGlobals->time > AliceLastHurt + 3 )
		{
			TakeHealth( 5.0f * gpGlobals->frametime, DMG_GENERIC );
		}
	}

	if( bAliceFollowing && next_hud_update < gpGlobals->time )
	{
		AliceSendHUDData( true );
		next_hud_update = gpGlobals->time + 0.25;
	}

	CBaseMonster::RunAI();
}

Schedule_t *CAlice :: GetSchedule ( void )
{
	if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE) && !HasConditions( bits_COND_SEE_ENEMY ))
			// something hurt me and I can't see anyone. Better relocate
			// this fixes the issue when the enemy is far away shooting at her back, and she does nothing
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
	
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );

		if (!HasConditions( bits_COND_SEE_ENEMY ) && pSound && ( pSound->m_iType & bits_SOUND_COMBAT) && !HasSpawnFlags( SF_MONSTER_PRISONER ) )
			MakeIdealYaw( pSound->m_vecOrigin );
	}


	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
	{
		// nice kill
		if (m_iszSpeakAs)
		{
			char szBuf[32];
			strcpy(szBuf,STRING(m_iszSpeakAs));
			strcat(szBuf,"_KILL");
			PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
		}
		else
		{
			PlaySentence( "ALICE_KILL", 4, VOL_NORM, ATTN_NORM );
		}
	}

	if( HasConditions( bits_COND_NEW_ENEMY ))
	{
		if (pev->health < 150)
		{
			if (gpGlobals->time > CoverFailTime)
			{
				CoverFailTime = gpGlobals->time + 5;
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}
			else
				return GetScheduleOfType( SCHED_ALICE_TAKECOVER_FAILED );
		}
			
	}

	switch( m_MonsterState )
	{
		case MONSTERSTATE_COMBAT:
		{
			float dist_to_player = IsFollowing() ? (m_hTargetEnt->GetAbsOrigin() - GetAbsOrigin()).Length() : 0.0f;

			// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD | bits_COND_ENEMY_LOST ) )
			{
				m_flNextHolsterTime = gpGlobals->time + RANDOM_FLOAT( 6.0, 10.0 );
				m_flIdleReloadTime = m_flNextHolsterTime - 3;
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

			if( IsFollowing() && dist_to_player > 1000 )
			{
				if( gpGlobals->time > CoverFailTime )
				{
					ALERT( at_aiconsole, "Alice: player is too far, going back to him!\n" );
					CoverFailTime = gpGlobals->time + 3;
					return GetScheduleOfType( SCHED_ALICE_GETBACKTOPLAYER );
				}
			}
			else if( pev->health < 150 )
			{
				if( (HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE )) ) // low hp - try to hide
				{
					if( gpGlobals->time > CoverFailTime )
					{
						CoverFailTime = gpGlobals->time + 10;
						PlaySentence( "ALICE_COVER", 4, VOL_NORM, ATTN_NORM );
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );  // hide if low hp
					}
					else
						return GetScheduleOfType( SCHED_ALICE_TAKECOVER_FAILED );//return CBaseMonster :: GetSchedule();
				}
			}

			// when getting back to player, check for enemy visibility and forget about them if no line of sight
			if( IsFollowing() && dist_to_player < 300 && !HasConditions( bits_COND_SEE_ENEMY ) )
			{
				m_hEnemy = NULL;
				SetState( MONSTERSTATE_ALERT );
				return CBaseMonster::GetSchedule();
			}
				
			// wait for one schedule to draw gun
			if (!m_fGunDrawn )
				return GetScheduleOfType( SCHED_ARM_WEAPON );

			// no ammo
			if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
			{
				if( !IsTalking() && RANDOM_LONG(0,1) == 0)
					PlaySentence( "ALICE_RELOAD", 4, VOL_NORM, ATTN_NORM );
				
				if (pev->health < 350)
					return GetScheduleOfType ( SCHED_BARNEY_COVER_AND_RELOAD );
				else
					return GetScheduleOfType ( SCHED_RELOAD );
			}

			if ( HasConditions(bits_COND_ENEMY_TOOCLOSE) )
			{
				if (gpGlobals->time > CoverFailTime)
				{
					CoverFailTime = gpGlobals->time + 5;
					if( !(pev->spawnflags & SF_MONSTER_GAG) && !IsTalking() )
					{
						if(RANDOM_LONG(0,1)==0)
							PlaySentence( "ALICE_COVER", 4, VOL_NORM, ATTN_NORM );
					}
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
				else
					return GetScheduleOfType( SCHED_ALICE_TAKECOVER_FAILED );
			}
		}
		break;

		case MONSTERSTATE_ALERT:	
		case MONSTERSTATE_IDLE:
			if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
			{
			// flinch if hurt
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
			}

			if( pev->health < 150 )
			{
				if ( (HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE)) ) 
				{	
					if (gpGlobals->time > CoverFailTime)
					{
						CoverFailTime = gpGlobals->time + 10;
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );  // hide if low hp
					}
					else
						return GetScheduleOfType( SCHED_ALICE_TAKECOVER_FAILED );
				}
			}

			if ( m_fGunDrawn && (m_hEnemy == NULL) )
			{
				if (m_flIdleReloadTime < gpGlobals->time)
				{
					if (m_cAmmoLoaded < m_iClipSize)
						return GetScheduleOfType( SCHED_RELOAD );
					else if (m_flNextHolsterTime < gpGlobals->time)
						return GetScheduleOfType( SCHED_BARNEY_HOLSTER);
				}
			}

			if ( m_hEnemy == NULL && IsFollowing() )
			{
				if ( !m_hTargetEnt->IsAlive() )
				{
					// UNDONE: Comment about the recently dead player here?
					StopFollowing( FALSE );
					break;
				}
				else
				{
					if ( m_fGunDrawn && (m_cAmmoLoaded < m_iClipSize) && (m_flIdleReloadTime < gpGlobals->time) )
					{
						if( !IsTalking() )
							PlaySentence( "ALICE_RELOAD", 2.5, VOL_NORM, ATTN_IDLE );
						return GetScheduleOfType( SCHED_RELOAD );
					}

					if( HasConditions( bits_COND_CLIENT_PUSH ) )
						return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );

					return GetScheduleOfType( SCHED_TARGET_FACE );
				}
			}

			if( HasConditions( bits_COND_CLIENT_PUSH ) )
				return GetScheduleOfType( SCHED_MOVE_AWAY );

			// try to say something about smells
			TrySmellTalk();
		break;
	}
	
	return CTalkMonster::GetSchedule();
}

Schedule_t* CAlice :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	case SCHED_ARM_WEAPON:
		if ( m_hEnemy != NULL )
			// face enemy, then draw.
			return slBarneyEnemyDraw;
		break;

	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		// call base class default so that barney will talk
		// when 'used' 
		psched = CTalkMonster::GetScheduleOfType( Type );

		if( psched == slIdleStand )
			return slBaFaceTarget;	// override this for different target face behavior
		else
			return psched;

	case SCHED_TARGET_CHASE:
		return slBaFollow;

	case SCHED_ALICE_GETBACKTOPLAYER:
		return slAliceGetBackToPlayer;

//	case SCHED_IDLE_STAND:
//		return slIdleStand;

	case SCHED_BARNEY_HOLSTER:
		if ( m_hEnemy == NULL )
			return slBarneyHolster;
		break;

	case SCHED_RANGE_ATTACK1:
		return slBaRangeAttack1;

	case SCHED_BARNEY_COVER_AND_RELOAD:
		return &slBaHideReload[ 0 ];

	case SCHED_ALICE_TAKECOVER_FAILED:
		return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

	case SCHED_ALICE_WAIT_FACE_ENEMY:
		return &slAliceWaitInCover[ 0 ];

	case SCHED_TAKE_COVER_FROM_ENEMY:
			return &slAliceTakeCover[ 0 ];
	}

	return CTalkMonster::GetScheduleOfType( Type );
}


// Init talk data
void CAlice :: TalkInit()
{
	
	CTalkMonster::TalkInit();

	// barney speach group names (group names are in sentences.txt)

	if (!m_iszSpeakAs)
	{
		m_szGrp[TLK_ANSWER]		=	"ALICE_ANSWER";
		m_szGrp[TLK_QUESTION]	=	"ALICE_QUESTION";
		m_szGrp[TLK_IDLE]		=	"ALICE_IDLE";
		m_szGrp[TLK_STARE]		=	"ALICE_STARE";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER) //LRC
			m_szGrp[TLK_USE]	=	"ALICE_PFOLLOW";
		else
			m_szGrp[TLK_USE] =	"ALICE_OK";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER)
			m_szGrp[TLK_UNUSE] = "ALICE_PWAIT";
		else
			m_szGrp[TLK_UNUSE] = "ALICE_WAIT";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER)
			m_szGrp[TLK_DECLINE] =	"ALICE_POK";
		else
			m_szGrp[TLK_DECLINE] =	"ALICE_NOTOK";
		m_szGrp[TLK_STOP] =		"ALICE_STOP";

		m_szGrp[TLK_NOSHOOT] =	"ALICE_SCARED";
		m_szGrp[TLK_HELLO] =	"ALICE_HELLO";

		m_szGrp[TLK_PLHURT1] =	"!ALICE_CUREA";
		m_szGrp[TLK_PLHURT2] =	"!ALICE_CUREB"; 
		m_szGrp[TLK_PLHURT3] =	"!ALICE_CUREC";

		m_szGrp[TLK_PHELLO] =	NULL;	//"BA_PHELLO";		// UNDONE
		m_szGrp[TLK_PIDLE] =	NULL;	//"BA_PIDLE";			// UNDONE
		m_szGrp[TLK_PQUESTION] = "ALICE_PQUEST";		// UNDONE

		m_szGrp[TLK_SMELL] =	"ALICE_SMELL";
	
		m_szGrp[TLK_WOUND] =	"ALICE_WOUND";
		m_szGrp[TLK_MORTAL] =	"ALICE_MORTAL";
	}

	// get voice for head - just one barney voice for now
	m_voicePitch = 100;
}

//=========================================================
// ALertSound
//=========================================================
void CAlice :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( NewEnemySentenceTime + 5 < gpGlobals->time )
		{
			PlaySentence( "ALICE_ATTACK", 3, VOL_NORM, ATTN_IDLE );
			NewEnemySentenceTime = gpGlobals->time;
		}
	}

}

void CAlice :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(3,5); // 0.5-0.75? too often!
	PlaySentence( "ALICE_HURT", RANDOM_FLOAT(2,4), VOL_NORM, ATTN_IDLE );
}

//=========================================================
// DeathSound 
//=========================================================
void CAlice :: DeathSound ( void )
{
	// no sounds
}

int CAlice :: DamageDecal( int bitsDamageType )  // I'm not sure if it's working
{
	return -1;
}

void CAlice::Killed( entvars_t *pevAttacker, int iGib )
{
	bAliceFollowing = false;
	health_cached = pev->health + 1;
	AliceSendHUDData( false );

	CBarney::Killed( pevAttacker, iGib );
}

void CAlice::SetActivity( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;

	switch( NewActivity )
	{
	case ACT_WALK:
		if( m_fGunDrawn )
			iSequence = LookupSequence( "walk_pistol" );
		break;
	case ACT_RUN:
		if( m_fGunDrawn )
			iSequence = LookupSequence( "run_pistol" );
		break;
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if( pev->sequence != iSequence || !m_fSequenceLoops )
			pev->frame = 0;

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		CBaseMonster::SetActivity( NewActivity );
	}
}