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
// hgrunt
//=========================================================

//=========================================================
// Hit groups!	
//=========================================================
/*

  1 - Head
  2 - Stomach
  3 - Gun

*/

#include	"extdll.h"
#include	"plane.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"game/schedule.h"
#include	"animation.h"
#include	"squadmonster.h"
#include	"weapons/weapons.h"
#include	"talkmonster.h"
#include	"entities/soundent.h"
#include	"effects.h"
#include	"customentity.h"
#include	"hgrunt.h"

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_GRUNT_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE,// move to a location to set up an attack against the enemy. (usually when a friendly is in the way).
	SCHED_GRUNT_COVER_AND_RELOAD,
	SCHED_GRUNT_SWEEP,
	SCHED_GRUNT_FOUND_ENEMY,
	SCHED_GRUNT_REPEL,
	SCHED_GRUNT_REPEL_ATTACK,
	SCHED_GRUNT_REPEL_LAND,
	SCHED_GRUNT_WAIT_FACE_ENEMY,
	SCHED_GRUNT_TAKECOVER_FAILED,// special schedule type that forces analysis of conditions and picks the best possible schedule to recover from this type of failure.
	SCHED_GRUNT_ELOF_FAIL,
	SCHED_GRUNT_RUN_AND_FIRE,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_GRUNT_FACE_TOSS_DIR = LAST_COMMON_TASK + 1,
	TASK_GRUNT_SPEAK_SENTENCE,
	TASK_GRUNT_CHECK_FIRE,
};

int g_fGruntQuestion;				// true if an idle grunt asked a question. Cleared when someone answers.
bool g_bGruntAlert;

extern DLL_GLOBAL int		g_iSkillLevel;

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define GRUNT_CLIP_SIZE					36 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
#define GRUNT_VOL						0.35		// volume of grunt sounds
#define GRUNT_ATTN						ATTN_NORM	// attenutation of grunt sentences
#define HGRUNT_LIMP_HEALTH				20
#define HGRUNT_DMG_HEADSHOT				( DMG_BULLET | DMG_CLUB )	// damage types that can kill a grunt with a single headshot.
#define HGRUNT_NUM_HEADS				2 // how many grunt heads are there? 
#define HGRUNT_MINIMUM_HEADSHOT_DAMAGE	15 // must do at least this much damage in one shot to head to score a headshot kill  //not used?
#define HGRUNT_SENTENCE_VOLUME			(float)0.35 // volume of grunt sentences

#define HGRUNT_9MMAR				BIT(0)
#define HGRUNT_HANDGRENADE			BIT(1)
#define HGRUNT_GRENADELAUNCHER		BIT(2)
#define HGRUNT_SHOTGUN				BIT(3)

#define HEAD_GROUP					1

#define HEAD_MASK					0
#define HEAD_COMMANDER				1
#define HEAD_SHOTGUN				2
#define HEAD_M203					3

#define GUN_GROUP					2

#define GUN_MP5						0
#define GUN_SHOTGUN					1
#define GUN_NONE					2

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		HGRUNT_AE_RELOAD		( 2 )
#define		HGRUNT_AE_KICK			( 3 )
#define		HGRUNT_AE_BURST1		( 4 )
#define		HGRUNT_AE_BURST2		( 5 ) 
#define		HGRUNT_AE_BURST3		( 6 ) 
#define		HGRUNT_AE_GREN_TOSS		( 7 )
#define		HGRUNT_AE_GREN_LAUNCH	( 8 )
#define		HGRUNT_AE_GREN_DROP		( 9 )
#define		HGRUNT_AE_CAUGHT_ENEMY	( 10) // grunt established sight with an enemy (player only) that had previously eluded the squad.
#define		HGRUNT_AE_DROP_GUN		( 11) // grunt (probably dead) is dropping his mp5.

const char* CHGrunt::pGruntSentences[] =
{
	"HG_GREN", // grenade scared grunt
	"HG_ALERT", // sees player
	"HG_MONSTER", // sees monster
	"HG_COVER", // running to cover
	"HG_THROW", // about to throw grenade
	"HG_CHARGE",  // running out to get the enemy
	"HG_TAUNT", // say rude things
	"HG_DRONE", // deploy drone
};

enum
{
	HGRUNT_SENT_NONE = -1,
	HGRUNT_SENT_GREN = 0,
	HGRUNT_SENT_ALERT,
	HGRUNT_SENT_MONSTER,
	HGRUNT_SENT_COVER,
	HGRUNT_SENT_THROW,
	HGRUNT_SENT_CHARGE,
	HGRUNT_SENT_TAUNT,
} HGRUNT_SENTENCE_TYPES;

//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_GRUNT_NOFIRE	( bits_COND_SPECIAL1 )

LINK_ENTITY_TO_CLASS(monster_human_grunt, CHGrunt);

BEGIN_DATADESC(CHGrunt)
	DEFINE_FIELD(m_flNextGrenadeCheck, FIELD_TIME),
	DEFINE_FIELD(m_flNextPainTime, FIELD_TIME),
	//	DEFINE_FIELD( m_flLastEnemySightTime, FIELD_TIME ), // don't save, go to zero
	DEFINE_FIELD(m_vecTossVelocity, FIELD_VECTOR),
	DEFINE_FIELD(m_fThrowGrenade, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fStanding, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fFirstEncounter, FIELD_BOOLEAN),
	DEFINE_FIELD(m_cClipSize, FIELD_INTEGER),
	DEFINE_FIELD(m_iSentence, FIELD_INTEGER),
	DEFINE_FIELD(CanSpawnDrone, FIELD_BOOLEAN),
	DEFINE_FIELD(DroneSpawned, FIELD_BOOLEAN),
	DEFINE_KEYFIELD(wpns, FIELD_STRING, "wpns"),
	DEFINE_KEYFIELD(m_iFlashlightCap, FIELD_INTEGER, "flashlight"),
	DEFINE_FIELD(CanInvestigate, FIELD_INTEGER),
	DEFINE_FIELD(AttackStartTime, FIELD_TIME),
	DEFINE_FIELD(CombatWaitTime, FIELD_TIME),
	DEFINE_FIELD(FlashlightSpr, FIELD_CLASSPTR),
	DEFINE_KEYFIELD(Silent, FIELD_BOOLEAN, "silent"),
	DEFINE_KEYFIELD( ForceEMPGrenade, FIELD_BOOLEAN, "forceemp"),

	// monster_alien_soldier:
	DEFINE_FIELD(AlternateShoot, FIELD_INTEGER),
	DEFINE_FIELD(AlienEye, FIELD_CLASSPTR),

	// monster_andrew_grunt:
	DEFINE_FIELD( AndrewLastHurt, FIELD_TIME ),
	DEFINE_FIELD( AndrewDash, FIELD_BOOLEAN ),
	DEFINE_FIELD( AndrewDashTime, FIELD_TIME ),
	DEFINE_ARRAY( AndrewRespawnPoint, FIELD_VECTOR, MAX_ANDREW_SPAWNS ),
	DEFINE_FIELD( AndrewSpecialMode, FIELD_BOOLEAN ),
	DEFINE_FIELD( AndrewEscapeTime, FIELD_TIME ),
	DEFINE_FIELD( RespawnPoints, FIELD_INTEGER ),
	DEFINE_FIELD( FireRocketTime, FIELD_TIME ),
	DEFINE_FIELD( AccumulatedDamage, FIELD_FLOAT ),
	DEFINE_FIELD( RocketCount, FIELD_INTEGER ),
	DEFINE_FIELD( SpecialModeHealth, FIELD_FLOAT ),
	DEFINE_FIELD( DifficultyScaler, FIELD_FLOAT ),
	DEFINE_FIELD( ScaleDifficultyTime, FIELD_TIME ),
END_DATADESC()

IMPLEMENT_SAVERESTORE(CHGrunt, CSquadMonster);

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

//=========================================================
// GruntFail
//=========================================================
Task_t	tlGruntFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)1		},  // AIFIX
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slGruntFail[] =
{
	{
		tlGruntFail,
		SIZEOFARRAY(tlGruntFail),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"Grunt Fail"
	},
};

//=========================================================
// Grunt Combat Fail
//=========================================================
Task_t	tlGruntCombatFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },

//	{ TASK_WAIT,				(float)0.5		},
	{ TASK_WAIT_FACE_ENEMY,		(float)0.5		}, // 2

	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slGruntCombatFail[] =
{
	{
		tlGruntCombatFail,
		SIZEOFARRAY(tlGruntCombatFail),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"GruntCombatFail"
	},
};

//=========================================================
// Victory dance!
//=========================================================
Task_t	tlGruntVictoryDance[] =
{
	{ TASK_STOP_MOVING,						(float)0					},
	{ TASK_FACE_ENEMY,						(float)0					},
	{ TASK_WAIT,							(float)1.0					},  // 1.5   AIFIX
	{ TASK_GET_PATH_TO_ENEMY_CORPSE,		(float)0					},
	{ TASK_WALK_PATH,						(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,				(float)0					},
	{ TASK_FACE_ENEMY,						(float)0					},
	{ TASK_PLAY_SEQUENCE,					(float)ACT_VICTORY_DANCE	},
};

Schedule_t	slGruntVictoryDance[] =
{
	{
		tlGruntVictoryDance,
		SIZEOFARRAY(tlGruntVictoryDance),
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE,
		0,
		"GruntVictoryDance"
	},
};

//=========================================================
// Establish line of fire - move to a position that allows
// the grunt to attack.
//=========================================================
Task_t tlGruntEstablishLineOfFire[] =
{
		{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_GRUNT_ELOF_FAIL	},

		{ TASK_GET_PATH_TO_ENEMY,	(float)0						},
		{ TASK_GRUNT_SPEAK_SENTENCE,(float)0						},
		{ TASK_RUN_PATH,			(float)0						},
		{ TASK_WAIT_FOR_MOVEMENT,	(float)0						},
};

Schedule_t slGruntEstablishLineOfFire[] =
{
	{
		tlGruntEstablishLineOfFire,
		SIZEOFARRAY(tlGruntEstablishLineOfFire),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_ENEMY_LOST |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"GruntEstablishLineOfFire"
	},
};

Task_t tlGruntRunAndFire[] =
{
			{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_GRUNT_ELOF_FAIL	},

			{ TASK_GET_PATH_TO_ENEMY,	(float)256						},
			{ TASK_GRUNT_SPEAK_SENTENCE,(float)0						},
			{ TASK_RUN_PATH,			(float)0						},
			{ TASK_WAIT_FOR_MOVEMENT,	(float)0						},
};

Schedule_t slGruntRunAndFire[] =
{
	{
		tlGruntRunAndFire,
		SIZEOFARRAY( tlGruntRunAndFire ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_ENEMY_LOST |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_LOW_HEALTH |
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"GruntRunAndFire"
	},
};

//=========================================================
// GruntFoundEnemy - grunt established sight with an enemy
// that was hiding from the squad.
//=========================================================
Task_t	tlGruntFoundEnemy[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,(float)ACT_SIGNAL1			},
};

Schedule_t	slGruntFoundEnemy[] =
{
	{
		tlGruntFoundEnemy,
		SIZEOFARRAY(tlGruntFoundEnemy),
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	},
};

//=========================================================
// GruntCombatFace Schedule
//=========================================================
Task_t	tlGruntCombatFace1[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_WAIT,					(float)0.4					},  // 1.5 AIFIX
//	{ TASK_SET_SCHEDULE,			(float)SCHED_GRUNT_SWEEP	},  // no more stupid turns
};

Schedule_t	slGruntCombatFace[] =
{
	{
		tlGruntCombatFace1,
		SIZEOFARRAY(tlGruntCombatFace1),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_ENEMY_LOST |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"GruntCombatFace"
	},
};

//=========================================================
// Suppressing fire - don't stop shooting until the clip is
// empty or grunt gets hurt.
//=========================================================
Task_t	tlGruntSignalSuppress[] =
{
	{ TASK_STOP_MOVING,					0						},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,	(float)ACT_SIGNAL2		},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
};

Schedule_t	slGruntSignalSuppress[] =
{
	{
		tlGruntSignalSuppress,
		SIZEOFARRAY(tlGruntSignalSuppress),
		bits_COND_ENEMY_DEAD |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_LOST |
		bits_COND_HEAR_SOUND |
		bits_COND_GRUNT_NOFIRE |
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"SignalSuppress"
	},
};

Task_t	tlGruntSuppress[] =
{
	{ TASK_STOP_MOVING,			0							},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
};

Schedule_t	slGruntSuppress[] =
{
	{
		tlGruntSuppress,
		SIZEOFARRAY(tlGruntSuppress),
		bits_COND_ENEMY_DEAD |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_LOST |
		bits_COND_HEAR_SOUND |
		bits_COND_GRUNT_NOFIRE |
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"Suppress"
	},
};


//=========================================================
// grunt wait in cover - we don't allow danger or the ability
// to attack to break a grunt's run to cover schedule, but
// when a grunt is in cover, we do want them to attack if they can.
//=========================================================
Task_t	tlGruntWaitInCover[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_WAIT_FACE_ENEMY,			(float)1					},
};

Schedule_t	slGruntWaitInCover[] =
{
	{
		tlGruntWaitInCover,
		SIZEOFARRAY(tlGruntWaitInCover),
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,

		bits_SOUND_DANGER,
		"GruntWaitInCover"
	},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t	tlGruntTakeCover1[] =
{
	{ TASK_STOP_MOVING,				(float)0							},  // AIFIX
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_GRUNT_TAKECOVER_FAILED	},
	{ TASK_WAIT,					(float)0.1							},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0							},
	{ TASK_GRUNT_SPEAK_SENTENCE,	(float)0							},
	{ TASK_RUN_PATH,				(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0							},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER			},
	{ TASK_SET_SCHEDULE,			(float)SCHED_GRUNT_WAIT_FACE_ENEMY	},
};

Schedule_t	slGruntTakeCover[] =
{
	{
		tlGruntTakeCover1,
		SIZEOFARRAY(tlGruntTakeCover1),
		0,
		0,
		"TakeCover"
	},
};

//=========================================================
// drop grenade then run to cover.
//=========================================================
Task_t	tlGruntGrenadeCover1[] =
{
	{ TASK_STOP_MOVING,						(float)0							},
	{ TASK_FIND_COVER_FROM_ENEMY,			(float)99							},
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY,	(float)384							},
	{ TASK_PLAY_SEQUENCE,					(float)ACT_SPECIAL_ATTACK1			},
	{ TASK_CLEAR_MOVE_WAIT,					(float)0							},
	{ TASK_RUN_PATH,						(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,				(float)0							},
	{ TASK_SET_SCHEDULE,					(float)SCHED_GRUNT_WAIT_FACE_ENEMY	},
};

Schedule_t	slGruntGrenadeCover[] =
{
	{
		tlGruntGrenadeCover1,
		SIZEOFARRAY(tlGruntGrenadeCover1),
		0,
		0,
		"GrenadeCover"
	},
};


//=========================================================
// drop grenade then run to cover.
//=========================================================
Task_t	tlGruntTossGrenadeCover1[] =
{
	{ TASK_FACE_ENEMY,						(float)0							},
	{ TASK_RANGE_ATTACK2, 					(float)0							},
	{ TASK_SET_SCHEDULE,					(float)SCHED_TAKE_COVER_FROM_ORIGIN	},//	{ TASK_SET_SCHEDULE,					(float)SCHED_TAKE_COVER_FROM_ENEMY	},
};

Schedule_t	slGruntTossGrenadeCover[] =
{
	{
		tlGruntTossGrenadeCover1,
		SIZEOFARRAY(tlGruntTossGrenadeCover1),
		0,
		0,
		"TossGrenadeCover"
	},
};

//=========================================================
// hide from the loudest sound source (to run from grenade)
//=========================================================
Task_t	tlGruntTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_COWER			},// duck and cover if cannot move from explosion
	{ TASK_STOP_MOVING,					(float)0					},
	{ TASK_FIND_COVER_FROM_BEST_SOUND,	(float)0					},
	{ TASK_RUN_PATH,					(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0					},
	{ TASK_REMEMBER,					(float)bits_MEMORY_INCOVER	},
	{ TASK_TURN_LEFT,					(float)179					},
};

Schedule_t	slGruntTakeCoverFromBestSound[] =
{
	{
		tlGruntTakeCoverFromBestSound,
		SIZEOFARRAY(tlGruntTakeCoverFromBestSound),
		0,
		0,
		"GruntTakeCoverFromBestSound"
	},
};

//=========================================================
// Grunt reload schedule
//=========================================================
Task_t	tlGruntHideReload[] =
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

Schedule_t slGruntHideReload[] =
{
	{
		tlGruntHideReload,
		SIZEOFARRAY(tlGruntHideReload),
	//	bits_COND_HEAVY_DAMAGE | // diffusion - do not interrupt reload with this.
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

//=========================================================
// Do a turning sweep of the area
//=========================================================
Task_t	tlGruntSweep[] =
{
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
};

Schedule_t	slGruntSweep[] =
{
	{
		tlGruntSweep,
		SIZEOFARRAY(tlGruntSweep),

		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_HEAR_SOUND,

		bits_SOUND_WORLD |// sound flags
		bits_SOUND_DANGER |
		bits_SOUND_PLAYER,

		"Grunt Sweep"
	},
};

//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlGruntRangeAttack1A[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_WAIT_FACE_ENEMY,		(float)0		},//{ TASK_PLAY_SEQUENCE_FACE_ENEMY,		(float)ACT_CROUCH },
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
};

Schedule_t	slGruntRangeAttack1A[] =
{
	{
		tlGruntRangeAttack1A,
		SIZEOFARRAY(tlGruntRangeAttack1A),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_LOST |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAR_SOUND |
		bits_COND_GRUNT_NOFIRE |
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"Range Attack1A"
	},
};


//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlGruntRangeAttack1B[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_WAIT_FACE_ENEMY,		(float)0		},//	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,(float)ACT_IDLE_ANGRY  },
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
};

Schedule_t	slGruntRangeAttack1B[] =
{
	{
		tlGruntRangeAttack1B,
		SIZEOFARRAY(tlGruntRangeAttack1B),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_LOST |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NO_AMMO_LOADED |
		bits_COND_GRUNT_NOFIRE |
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"Range Attack1B"
	},
};

//=========================================================
// secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlGruntRangeAttack2[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_GRUNT_FACE_TOSS_DIR,		(float)0					},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_RANGE_ATTACK2	},
	{ TASK_WAIT,					(float)0.5					},//	{ TASK_SET_SCHEDULE,			(float)SCHED_GRUNT_WAIT_FACE_ENEMY	},// don't run immediately after throwing grenade.
};

Schedule_t	slGruntRangeAttack2[] =
{
	{
		tlGruntRangeAttack2,
		SIZEOFARRAY(tlGruntRangeAttack2),
		0,
		0,
		"RangeAttack2"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlGruntRepel[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_GLIDE 	},
};

Schedule_t	slGruntRepel[] =
{
	{
		tlGruntRepel,
		SIZEOFARRAY(tlGruntRepel),
		bits_COND_SEE_ENEMY |
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER |
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER,
		"Repel"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlGruntRepelAttack[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_FLY 	},
};

Schedule_t	slGruntRepelAttack[] =
{
	{
		tlGruntRepelAttack,
		SIZEOFARRAY(tlGruntRepelAttack),
		bits_COND_ENEMY_OCCLUDED,
		0,
		"Repel Attack"
	},
};

//=========================================================
// repel land
//=========================================================
Task_t	tlGruntRepelLand[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_LAND	},
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_RUN_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t	slGruntRepelLand[] =
{
	{
		tlGruntRepelLand,
		SIZEOFARRAY(tlGruntRepelLand),
		bits_COND_SEE_ENEMY |
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER |
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER,
		"Repel Land"
	},
};


DEFINE_CUSTOM_SCHEDULES(CHGrunt)
{
	slGruntFail,
	slGruntCombatFail,
	slGruntVictoryDance,
	slGruntEstablishLineOfFire,
	slGruntFoundEnemy,
	slGruntCombatFace,
	slGruntSignalSuppress,
	slGruntSuppress,
	slGruntWaitInCover,
	slGruntTakeCover,
	slGruntGrenadeCover, // not used
	slGruntTossGrenadeCover,
	slGruntTakeCoverFromBestSound,
	slGruntHideReload,
	slGruntSweep,
	slGruntRangeAttack1A,
	slGruntRangeAttack1B,
	slGruntRangeAttack2,
	slGruntRepel,
	slGruntRepelAttack,
	slGruntRepelLand,
	slGruntRunAndFire,
};

IMPLEMENT_CUSTOM_SCHEDULES(CHGrunt, CSquadMonster);

void CHGrunt::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "wpns" ))
	{
		wpns = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "flashlight"))
	{
		m_iFlashlightCap = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "silent" ) )
	{
		Silent = (Q_atoi( pkvd->szValue ) > 0 );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "forceemp" ) )
	{
		ForceEMPGrenade = (Q_atoi( pkvd->szValue ) > 0);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue(pkvd);
}

//=========================================================
// Speak Sentence - say your cued up sentence.
//
// Some grunt sentences (take cover and charge) rely on actually
// being able to execute the intended action. It's really lame
// when a grunt says 'COVER ME' and then doesn't move. The problem
// is that the sentences were played when the decision to TRY
// to move to cover was made. Now the sentence is played after 
// we know for sure that there is a valid path. The schedule
// may still fail but in most cases, well after the grunt has 
// started moving.
//=========================================================
void CHGrunt :: SpeakSentence( void )
{
	if ( m_iSentence == HGRUNT_SENT_NONE )
	{
		// no sentence cued up.
		return; 
	}

	if (FOkToSpeak())
	{
		SENTENCEG_PlayRndSz( ENT(pev), pGruntSentences[ m_iSentence ], HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
		JustSpoke();
	}
}

//=========================================================
// IRelationship - overridden because Alien Grunts are 
// Human Grunt's nemesis.
//=========================================================
int CHGrunt::IRelationship ( CBaseEntity *pTarget )
{
	CBaseMonster *pMonster = (CBaseMonster *)pTarget;
	
	//LRC- only hate alien grunts if my behaviour hasn't been overridden
	if (FClassnameIs( pMonster->pev, "monster_alien_grunt" ) || ( FClassnameIs( pMonster->pev,  "monster_gargantua" ) ) )
	{
		if( pMonster->Classify() != Classify() )
			return R_NM;
	}

	return CSquadMonster::IRelationship( pTarget );
}

//=========================================================
// GibMonster - make gun fly through the air.
//=========================================================
void CHGrunt :: GibMonster ( void )    // diffusion - grunts never drop guns initially, and the flag now does the opposite thing.
{
	Vector	vecGunPos;
	Vector	vecGunAngles;

if ( pev->spawnflags & SF_MONSTER_NO_WPN_DROP )

	if ( GetBodygroup( 2 ) != 2)
	{// throw a gun if the grunt has one
		GetAttachment( 0, vecGunPos, vecGunAngles );
		
		CBaseEntity *pGun;
		if ( HasWeapon( HGRUNT_SHOTGUN ))
			pGun = DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
		else
			pGun = DropItem( "weapon_9mmAR", vecGunPos, vecGunAngles );

		if ( pGun )
		{
			pGun->SetLocalVelocity( Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 )));
			pGun->SetLocalAvelocity( Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 ));
		}
	
		if ( HasWeapon( HGRUNT_GRENADELAUNCHER ))
		{
			pGun = DropItem( "ammo_ARgrenades", vecGunPos, vecGunAngles );
			if ( pGun )
			{
				pGun->SetLocalVelocity( Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 )));
				pGun->SetLocalAvelocity( Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 ));
			}
		}
	}

	CBaseMonster :: GibMonster();
}

void CHGrunt::ClearEffects(void)
{
	if( FlashlightSpr )
	{
		UTIL_Remove( FlashlightSpr );
		FlashlightSpr = NULL;
	}
}

//=========================================================
// ISoundMask - Overidden for human grunts because they 
// hear the DANGER sound that is made by hand grenades and
// other dangerous items.
//=========================================================
int CHGrunt :: ISoundMask ( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_PLAYER	|
			bits_SOUND_DANGER;
}

//=========================================================
// someone else is talking - don't speak
//=========================================================
BOOL CHGrunt :: FOkToSpeak( void )
{
// if someone else is talking, don't speak
	if (gpGlobals->time <= CTalkMonster::g_talkWaitTime)
		return FALSE;

	if ( HasSpawnFlags(SF_MONSTER_GAG) )
		return FALSE;

	// if player is not in pvs, don't speak
//	if (FNullEnt(FIND_CLIENT_IN_PVS(edict())))
//		return FALSE;
	
	return TRUE;
}

//=========================================================
//=========================================================
void CHGrunt :: JustSpoke( void )
{
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(1.5, 2.0);
	m_iSentence = HGRUNT_SENT_NONE;
}

//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CHGrunt :: PrescheduleThink ( void )
{
	if ( InSquad() && m_hEnemy != NULL )
	{
		if ( HasConditions ( bits_COND_SEE_ENEMY ) )
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time; // update the squad's last enemy sighting time.
		else
		{
			if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
				MySquadLeader()->m_fEnemyEluded = TRUE; // been a while since we've seen the enemy
		}
	}
}

//=========================================================
// FCanCheckAttacks - this is overridden for human grunts
// because they can throw/shoot grenades when they can't see their
// target and the base class doesn't check attacks if the monster
// cannot see its enemy.
//
// !!!BUGBUG - this gets called before a 3-round burst is fired
// which means that a friendly can still be hit with up to 2 rounds. 
// ALSO, grenades will not be tossed if there is a friendly in front,
// this is a bad bug. Friendly machine gun fire avoidance
// will unecessarily prevent the throwing of a grenade as well.
//=========================================================
BOOL CHGrunt :: FCanCheckAttacks ( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return TRUE;
	else
		return FALSE;
}


//=========================================================
// CheckMeleeAttack1
//=========================================================
BOOL CHGrunt::CheckMeleeAttack1(float flDot, float flDist)
{
	CBaseMonster* pEnemy = NULL;

	if (m_hEnemy != NULL)
		pEnemy = m_hEnemy->MyMonsterPointer();

	if (!pEnemy)
		return FALSE;

	if (flDist <= 64 && flDot >= 0.7 &&
		pEnemy->Classify() != CLASS_ALIEN_BIOWEAPON &&
		pEnemy->Classify() != CLASS_PLAYER_BIOWEAPON)
	{
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CheckRangeAttack1 - overridden for HGrunt, cause 
// FCanCheckAttacks() doesn't disqualify all attacks based
// on whether or not the enemy is occluded because unlike
// the base class, the HGrunt can attack when the enemy is
// occluded (throw grenade over wall, etc). We must 
// disqualify the machine gun attack if the enemy is occluded.
//=========================================================
BOOL CHGrunt :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 4096 && flDot >= 0.5 && NoFriendlyFire() )  // diffusion flDist was 2048
	{
		if ( (flDist > 1500) && (HasWeapon(HGRUNT_SHOTGUN)) )
			return FALSE; // don't waste bullets
		
		TraceResult	tr;

		if ( !m_hEnemy->IsPlayer() && flDist <= 64 )
			// kick nonclients, but don't shoot at them.
			return FALSE;

		// diffusion - some changes on crouching/standing behaviour
		if (flDist <= 800)
			m_fStanding = 1;
		else
			m_fStanding = 0;

		if( FClassnameIs( this, "monster_andrew_grunt" ) )
			m_fStanding = 1; // Andrew never crouches

		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget(vecSrc), ignore_monsters, ignore_glass, ENT(pev), &tr);
		
		if ( tr.flFraction == 1.0 )
			return TRUE;
	}

	return FALSE;
}
/*
//=========================================================
// CheckRangeAttack2 - this checks the Grunt's grenade
// attack. 
//=========================================================
*/

BOOL CHGrunt::CheckRangeAttack2( float flDot, float flDist )
{
	return CheckRangeAttack2Impl( g_GruntGrenadeSpeed, flDot, flDist);
}

BOOL CHGrunt::CheckRangeAttack2Impl( float grenadeSpeed, float flDot, float flDist )
{
	if ( !HasWeapon( HGRUNT_HANDGRENADE ) && !HasWeapon( HGRUNT_GRENADELAUNCHER ))
	{
		return FALSE;
	}
	
	// if the grunt isn't moving, it's ok to check.
	if( m_flGroundSpeed != 0 )
	{
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	// assume things haven't changed too much since last time
	if( gpGlobals->time < m_flNextGrenadeCheck )
	{
		return m_fThrowGrenade;
	}

	if( (!FBitSet ( m_hEnemy->pev->flags, FL_ONGROUND )) && (m_hEnemy->pev->waterlevel == 0) && (m_vecEnemyLKP.z > pev->absmax.z) && (!HasWeapon(HGRUNT_GRENADELAUNCHER)) )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		// diffusion - allow grenade launcher attack
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	Vector vecTarget;

	if( HasWeapon( HGRUNT_HANDGRENADE ))
	{
		// find feet
		if( RANDOM_LONG( 0, 1 ) || (m_vecEnemyLKP == g_vecZero) )
		{
			// magically know where they are
			vecTarget = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );
		}
		else
		{
			// toss it to where you last saw them
			vecTarget = m_vecEnemyLKP;
		}
		// vecTarget = m_vecEnemyLKP + (m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin);
		// estimate position
		// vecTarget = vecTarget + m_hEnemy->pev->velocity * 2;
	}
	else
	{
		// find target
		// vecTarget = m_hEnemy->BodyTarget( pev->origin );
		vecTarget = m_vecEnemyLKP + ( m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin );
		// estimate position
		if( HasConditions( bits_COND_SEE_ENEMY ) )
			vecTarget = vecTarget + ( ( vecTarget - pev->origin).Length() / (float)g_GruntGrenadeSpeed ) * m_hEnemy->pev->velocity;
	}

	// are any of my squad members near the intended grenade impact area?
	if( InSquad() )
	{
		if( SquadMemberInRange( vecTarget, 256 ) )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
			m_fThrowGrenade = FALSE;
		}
	}

	if( ( vecTarget - pev->origin ).Length2D() <= 256 )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	if( HasWeapon( HGRUNT_HANDGRENADE ))
	{
		Vector vecToss;
		if( CanSpawnDrone && !DroneSpawned )
		{
			// for drone spawn just use a default forward vector, we don't perform a toss, just looking in the enemy direction
			UTIL_MakeVectors( GetAbsAngles() );
			vecToss = gpGlobals->v_forward;
		}
		else
			vecToss = VecCheckToss( pev, GetGunPosition(), vecTarget, 0.6f );

		if( vecToss != g_vecZero )
		{
			if( CanSpawnDrone && !DroneSpawned )
			{
				m_vecTossVelocity = vecToss;
				// throw a drone
				m_fThrowGrenade = TRUE;
				// don't check again for a while.
				m_flNextGrenadeCheck = gpGlobals->time; // 1/3 second.
			}
			else
			{
				if( flCheckThrowDistance( pev, GetGunPosition(), vecToss ) < 300.0f )
				{
					// don't throw
					m_fThrowGrenade = FALSE;
					// don't check again for a while.
					m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
				}
				else
				{
					m_vecTossVelocity = vecToss;
					// throw a hand grenade
					m_fThrowGrenade = TRUE;
					// don't check again for a while.
					m_flNextGrenadeCheck = gpGlobals->time; // 1/3 second.
				}
			}
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}
	else // grenade launcher
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition(), vecTarget, g_GruntGrenadeSpeed, 0.5 );

		if( vecToss != g_vecZero )
		{
			float check = flCheckThrowDistance( pev, GetGunPosition(), vecToss );
			if( check < 300.0f )
			{
				// don't throw
				m_fThrowGrenade = FALSE;
				// don't check again for a while.
				m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
			}
			else
			{
				m_vecTossVelocity = vecToss;

				// throw a hand grenade
				m_fThrowGrenade = TRUE;
				// don't check again for a while.
				m_flNextGrenadeCheck = gpGlobals->time + 0.3; // 1/3 second.
			}
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}

	return m_fThrowGrenade;
}


void CHGrunt :: RunAI( void )
{	
	CBaseMonster :: RunAI();

	if( AlienEye ) // diffusion - !!! the origin is not being updated, have to do this, otherwise distance culling issues
		AlienEye->pev->origin = pev->origin;

	if( FlashlightCap && FlashlightSpr )
	{
		if( pev->effects & EF_MONSTERFLASHLIGHT )
			FlashlightSpr->SetBrightness( 200 );
		else
			FlashlightSpr->SetBrightness( 0 );
	}

	if( m_hEnemy && ( pev->sequence == LookupSequence( "runshootmp5" ) || pev->sequence == LookupSequence( "runshootshotgun" ) ) && BodyTurn( m_hEnemy->GetAbsOrigin() ) )
		RunningShooting = true;
	else
	{
		RunningShooting = false;
		SetBoneController( 1, 0 );
	}
}

//=========================================================
// TraceAttack - make sure we're not taking it in the helmet
//=========================================================
void CHGrunt :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if( HasSpawnFlags( SF_MONSTER_NODAMAGE ) )
		return;
	
	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return;

	// check for helmet shot
	if (ptr->iHitgroup == 11)
	{
		// make sure we're wearing one
		if (GetBodygroup( 1 ) == HEAD_MASK && (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB)))
		{
			// absorb damage
			flDamage -= 20;
			if (flDamage <= 0)
			{
				UTIL_Ricochet( ptr->vecEndPos, 1.0 );
				flDamage = 0.01;
			}
		}
		// it's head shot anyways
		ptr->iHitgroup = HITGROUP_HEAD;
	}
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


//=========================================================
// TakeDamage - overridden for the grunt because the grunt
// needs to forget that he is in cover if he's hurt. (Obviously
// not in a safe place anymore).
//=========================================================
int CHGrunt :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( HasSpawnFlags(SF_MONSTER_NODAMAGE) )
		return 0;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return 0;
	
	Forget( bits_MEMORY_INCOVER );
//	ALERT( at_console, "got dmg %f\n", flDamage );
	return CSquadMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CHGrunt :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:	
		ys = 150;		
		break;
	case ACT_RUN:	
		ys = 170;	
		break;
	case ACT_WALK:	
		ys = 150;		
		break;
	case ACT_RANGE_ATTACK1:	
		ys = 120;	
		break;
	case ACT_RANGE_ATTACK2:	
		ys = 120;	
		break;
	case ACT_MELEE_ATTACK1:	
		ys = 120;	
		break;
	case ACT_MELEE_ATTACK2:	
		ys = 120;	
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:	
		ys = 180;
		break;
	case ACT_GLIDE:
	case ACT_FLY:
		ys = 30;
		break;
	default:
		ys = 90;
		break;
	}

	pev->yaw_speed = ys;
}

void CHGrunt :: IdleSound( void )
{
	if (FOkToSpeak() && (g_fGruntQuestion || RANDOM_LONG(0,1)))
	{
		if (!g_fGruntQuestion)
		{
			// ask question or make statement
			switch (RANDOM_LONG(0,2))
			{
			case 0: // check in
				SENTENCEG_PlayRndSz(ENT(pev), "HG_CHECK", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				g_fGruntQuestion = 1;
				break;
			case 1: // question
				SENTENCEG_PlayRndSz(ENT(pev), "HG_QUEST", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				g_fGruntQuestion = 2;
				break;
			case 2: // statement
				SENTENCEG_PlayRndSz(ENT(pev), "HG_IDLE", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			}
		}
		else
		{
			switch (g_fGruntQuestion)
			{
			case 1: // check in
				SENTENCEG_PlayRndSz(ENT(pev), "HG_CLEAR", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			case 2: // question 
				SENTENCEG_PlayRndSz(ENT(pev), "HG_ANSWER", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			}
			g_fGruntQuestion = 0;
		}
		JustSpoke();

		// clear alert message here also
		g_bGruntAlert = false;
	}
}

//=========================================================
// CheckAmmo - overridden for the grunt because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CHGrunt :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions(bits_COND_NO_AMMO_LOADED);
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CHGrunt :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_HUMAN_MILITARY;
}

//=========================================================
//=========================================================
CBaseEntity *CHGrunt :: Kick( void )
{
	TraceResult tr;

	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecStart = GetAbsOrigin();
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * 70);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		return pEntity;
	}

	return NULL;
}

//=========================================================
// GetGunPosition	return the end of the barrel
//=========================================================
Vector CHGrunt :: GetGunPosition( )
{
	if (m_fStanding )
		return GetAbsOrigin() + Vector( 0, 0, 60 );
	else
		return GetAbsOrigin() + Vector( 0, 0, 48 );
}

//=========================================================
// Shoot
//=========================================================
void CHGrunt :: Shoot ( void )
{
	if (m_hEnemy == NULL)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );
	
	UTIL_MakeVectors ( GetAbsAngles() );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass ( vecShootOrigin - vecShootDir * 10, vecShellVelocity, GetAbsAngles().y, SHELL_556, TE_BOUNCE_SHELL);

	if( !m_fStanding )
		FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_4DEGREES, 4096, BULLET_MONSTER_MP5 );
	else if( RunningShooting )
		FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_20DEGREES, 4096, BULLET_MONSTER_MP5 );
	else
		FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_6DEGREES, 4096, BULLET_MONSTER_MP5 );

	pev->effects |= EF_MUZZLEFLASH;
	
	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );
}

//=========================================================
// Shoot
//=========================================================
void CHGrunt :: Shotgun ( void )
{
	if (m_hEnemy == NULL)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( GetAbsAngles() );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass ( vecShootOrigin - vecShootDir * 10, vecShellVelocity, GetAbsAngles().y, SHELL_12GAUGE, TE_BOUNCE_SHOTSHELL); 
	FireBullets( 4, vecShootOrigin, vecShootDir, RunningShooting ? VECTOR_CONE_15DEGREES : VECTOR_CONE_7DEGREES, 4096, BULLET_PLAYER_BUCKSHOT, 0, g_GruntShotgunDamage[g_iSkillLevel] ); // shoot +-7.5 degrees // diffusion 4096, was 2048

	pev->effects |= EF_MUZZLEFLASH;
	
	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CHGrunt :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch( pEvent->event )
	{
		case HGRUNT_AE_DROP_GUN:          // diffusion - only when the flag is set, grunts drop guns
			{
				if( HasSpawnFlags(SF_MONSTER_NO_WPN_DROP) && GetBodygroup( GUN_GROUP ) != GUN_NONE ) // diffusion - https://github.com/SamVanheer/halflife-updated/commit/bc7d6e3c0782f94553dd692351e7659903c0fa05
				{
					Vector	vecGunPos;
					Vector	vecGunAngles;

					GetAttachment( 0, vecGunPos, vecGunAngles );

					// switch to body group with no gun.
					SetBodygroup( GUN_GROUP, GUN_NONE );

					// now spawn a gun.
					if (HasWeapon( HGRUNT_SHOTGUN ))
						 DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
					else
						 DropItem( "weapon_9mmAR", vecGunPos, vecGunAngles );

					if (HasWeapon( HGRUNT_GRENADELAUNCHER ))
						DropItem( "ammo_ARgrenades", BodyTarget( GetAbsOrigin() ), vecGunAngles );
				}
			}

			break;

		case HGRUNT_AE_RELOAD:
			if (HasWeapon( HGRUNT_SHOTGUN ))
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload_shotgun.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload_mp5.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions(bits_COND_NO_AMMO_LOADED);
			break;

		case HGRUNT_AE_GREN_TOSS:
		{
			if( CanSpawnDrone ) // I can spawn drones.
			{
				if( !DroneSpawned ) // I didn't spawn any of them yet, so check if I'm able to
				{
					TraceResult tracer;
					Vector vecStart, vecEnd;
					vecStart = vecEnd = GetAbsOrigin();
					// drone spawns at 120. check up to 300, and 180 is acceptable
					vecEnd.z += 500;
					UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, dont_ignore_glass, edict(), &tracer);
					if( (vecStart - tracer.vecEndPos).Length() < 180 ) // not enough space. throw a grenade instead
					{
						UTIL_MakeVectors( GetAbsAngles() );
						CBaseEntity *pGrenade = CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5 );
						if( pGrenade )
						{
							SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
							pGrenade->pev->body = 1; // change to body without pin
						}
						EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/soldier_throw.wav", 1, ATTN_NORM );
						m_fThrowGrenade = FALSE;
						m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(6,12);// wait few seconds for the next try
					}
					else // spawn a drone
					{
						Vector vecStart = GetAbsOrigin();
						vecStart.z += 120;  // drone spawns right above grunt's head, there's a possibility that it could stuck though...
						CBaseMonster *pDrone = (CBaseMonster*)Create( "monster_security_drone", vecStart, GetAbsAngles(), edict() );
						if( pDrone )
						{
							pDrone->m_iClass = m_iClass; // do not shoot at the owner!!!
							// make the drone aware of the enemy which grunt was angry at
							// even if the drone didn't see him at the moment of spawn.

							if( m_hEnemy != NULL )
							{
								pDrone->SetEnemy( m_hEnemy );
								pDrone->SetConditions( bits_COND_NEW_ENEMY );
								pDrone->m_IdealMonsterState = m_IdealMonsterState; // do not strip the enemy after spawn
							}

							// copy these parameters too to act similarly
							pDrone->m_flDistLook = m_flDistLook;
							pDrone->m_flDistTooClose = m_flDistTooClose;
							pDrone->m_flDistTooFar = m_flDistTooFar;

							// add targetname too
							if( GetTargetname()[0] != '\0' )
							{
								char newname[64];
								sprintf_s( newname, "%s_drone", GetTargetname() );
								pDrone->pev->targetname = MAKE_STRING( newname );
							}
						}

						DroneSpawned = true; // the drone is now spawned, grunt can't spawn any more drones and will use the grenade
						m_fThrowGrenade = FALSE;
						m_flNextGrenadeCheck = gpGlobals->time + 3;
					}
				}
				else
				{
					UTIL_MakeVectors( GetAbsAngles() );
					CBaseEntity *pGrenade = CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5, ForceEMPGrenade );
					if( pGrenade )
					{
						SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
						pGrenade->pev->body = 1; // change to body without pin
					}
					EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/soldier_throw.wav", 1, ATTN_NORM );
					m_fThrowGrenade = FALSE;
					m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(8,15);// wait few seconds
				}
			}
			else // I can't (army guy), so throw a grenade
			{
				UTIL_MakeVectors( GetAbsAngles() );
				CBaseEntity *pGrenade = CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5, ForceEMPGrenade );
				if( pGrenade )
				{
					SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
					pGrenade->pev->body = 1; // change to body without pin
				}
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/soldier_throw.wav", 1, ATTN_NORM );
				m_fThrowGrenade = FALSE;
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(8,15);// wait few seconds
			}
			
		}
		break;

		case HGRUNT_AE_GREN_LAUNCH:
		{
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM);
			CGrenade::ShootContact( pev, GetGunPosition(), m_vecTossVelocity );
			m_fThrowGrenade = FALSE;
			if (g_iSkillLevel == SKILL_HARD)
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 8,12 );// wait a random amount of time before shooting again
			else
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 10,15 );;// wait 3-5 seconds before even looking again to see if a grenade can be thrown.
		}
		break;

		case HGRUNT_AE_GREN_DROP:
		{
			UTIL_MakeVectors( GetAbsAngles() );
			CGrenade::ShootTimed( pev, GetAbsOrigin() + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3, ForceEMPGrenade );
		}
		break;

		case HGRUNT_AE_BURST1:
		{
			if( pev->sequence == LookupSequence( "runshootmp5" ) || pev->sequence == LookupSequence( "runshootshotgun" ) )
			{
				if( !RunningShooting )
					break;
				if( !HasConditions(bits_COND_CAN_RANGE_ATTACK1) )
					break;
				if( m_hEnemy == NULL )
					break;
				// force client event
				MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, pev->origin );
				WRITE_BYTE( TE_CLIENTEVENT );
				WRITE_BYTE( 1 );
				WRITE_SHORT( entindex() );
				MESSAGE_END();
			}

			Vector org = GetAbsOrigin();
			org.z += 40;

			if ( HasWeapon( HGRUNT_9MMAR ))
			{
				PlayClientSound( this, 252, 0, (m_cAmmoLoaded <= 15 ? m_cAmmoLoaded : 0), org );
				Shoot();
			}
			else
			{
				Shotgun( );
			//	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1, ATTN_NORM );
				PlayClientSound( this, 254, 0, 0, org );
			}
		
			CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 1024, 0.3, ENTINDEX(edict()) );
		}
		break;
		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
		{
			if( pev->sequence == LookupSequence( "runshootmp5" ) || pev->sequence == LookupSequence( "runshootshotgun" ) )
			{
				if( !RunningShooting )
					break;
				if( !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
					break;
				if( m_hEnemy == NULL )
					break;
				// force client event
				MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, pev->origin );
				WRITE_BYTE( TE_CLIENTEVENT );
				WRITE_BYTE( 1 );
				WRITE_SHORT( entindex() );
				MESSAGE_END();
			}
			Shoot();
		}
		break;
		case HGRUNT_AE_KICK:
		{
			CBaseEntity *pHurt = Kick();

			if ( pHurt )
			{
				switch( RANDOM_LONG( 0, 2 ) )
				{
				case 0: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch1.wav", 1, ATTN_NORM ); break;
				case 1: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch2.wav", 1, ATTN_NORM ); break;
				case 2: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch3.wav", 1, ATTN_NORM ); break;
				}
				UTIL_MakeVectors( GetAbsAngles() );
				pHurt->pev->punchangle.x = 15;
				pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50 );
				pHurt->TakeDamage( pev, pev, g_GruntKickDamage, DMG_CLUB );
			}
		}
		break;

		case HGRUNT_AE_CAUGHT_ENEMY:
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz(ENT(pev), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
				 JustSpoke();
			}

		}

		default:
			CSquadMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CHGrunt::Spawn(void)
{
	Precache();

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/hgrunt.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	if( !pev->health ) pev->health = g_ArmyGuyHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	m_flFieldOfView		= 0;//180      0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime	= gpGlobals->time;
	m_iSentence			= HGRUNT_SENT_NONE;

	m_afCapability		= bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded		= FALSE;
	m_fFirstEncounter	= TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector ( 0, 0, 55 );

	// diffusion - new weapon system since it was changed in XashXT
	//--------------------------------------------------------------
	if (FStrEq( STRING(wpns), "9mmar" ))
	{
		AddWeapon(HGRUNT_9MMAR);
	}
	else if (FStrEq( STRING(wpns), "9mmar_hg" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon(HGRUNT_HANDGRENADE);
	}
	else if (FStrEq( STRING(wpns), "9mmar_gl" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon(HGRUNT_GRENADELAUNCHER);
	}
	else if (FStrEq( STRING(wpns), "shotgun" ))
	{
		AddWeapon(HGRUNT_SHOTGUN);
	}
	else if (FStrEq( STRING(wpns), "shotgun_hg" ))
	{
		AddWeapon(HGRUNT_SHOTGUN);
		AddWeapon(HGRUNT_HANDGRENADE);
	}
	else
	{
		// shuffle properly because it's still not exactly random it seems
		int rnd = RANDOM_LONG( 0, 3 );
		rnd = RANDOM_LONG( 0, 3 );
		rnd = RANDOM_LONG( 0, 3 );
		switch( rnd )
		{
		case 0:
			AddWeapon(HGRUNT_9MMAR);
			AddWeapon(HGRUNT_HANDGRENADE);
			break;
		case 1:
			AddWeapon(HGRUNT_SHOTGUN);
			break;
		case 2:
			AddWeapon(HGRUNT_9MMAR);
			AddWeapon(HGRUNT_GRENADELAUNCHER);
			break;
		case 3:
			AddWeapon(HGRUNT_SHOTGUN);
			AddWeapon(HGRUNT_HANDGRENADE);
			break;
		}
	}
	//------------------------------------------------------------

	if (HasWeapon( HGRUNT_SHOTGUN ))
	{
		SetBodygroup( GUN_GROUP, GUN_SHOTGUN );
		m_cClipSize = 8;
	}
	else
		m_cClipSize = GRUNT_CLIP_SIZE;

	m_cAmmoLoaded = m_cClipSize;

	if (RANDOM_LONG( 0, 99 ) < 80)
		pev->skin = 0;	// light skin
	else
		pev->skin = 1;	// dark skin

	if ( HasWeapon( HGRUNT_SHOTGUN ))
	{
		SetBodygroup( HEAD_GROUP, HEAD_SHOTGUN);
	}
	else if ( HasWeapon( HGRUNT_GRENADELAUNCHER ))
	{
		SetBodygroup( HEAD_GROUP, HEAD_M203 );
		pev->skin = 1; // always dark skin
	}

	if( RANDOM_LONG(0,2) == 0)
		CanInvestigate = 1;
	else
		CanInvestigate = 0;

	CanSpawnDrone = false; // army guys don't have pocket drones

	CTalkMonster::g_talkWaitTime = 0;

	CombatWaitTime = g_HGruntCombatWaitTime[g_iSkillLevel];

	if( m_iFlashlightCap > 0 )
	{
		FlashlightCap = true;

		FlashlightSpr = CSprite::SpriteCreate( HGRUNT_FLASHLIGHT_SPR, GetAbsOrigin(), TRUE );
		if( FlashlightSpr )
		{
			FlashlightSpr->SetAttachment( edict(), 1 );
			FlashlightSpr->SetScale( 0.3 );
			FlashlightSpr->SetTransparency( kRenderTransAdd, 255, 255, 255, 200, 0 );
		}

		if( m_iFlashlightCap == 2 )
			pev->effects |= EF_MONSTERFLASHLIGHT; // start ON
	}

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CHGrunt :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/hgrunt.mdl");

	PRECACHE_SOUND( "hgrunt/gr_mgun1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_mgun2.wav" );
	
	PRECACHE_SOUND( "hgrunt/hg_die1.wav" );
	PRECACHE_SOUND( "hgrunt/hg_die2.wav" );
	PRECACHE_SOUND( "hgrunt/hg_die3.wav" );

	PRECACHE_SOUND( "hgrunt/gr_pain1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain2.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain3.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain4.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain5.wav" );

	PRECACHE_SOUND( "hgrunt/gr_reload_mp5.wav" );
	PRECACHE_SOUND( "hgrunt/gr_reload_shotgun.wav" );

	PRECACHE_SOUND( "weapons/glauncher.wav" );

	PRECACHE_SOUND( "weapons/shotgun_npc.wav" );
	PRECACHE_SOUND( "weapons/shotgun_npc_d.wav" );
	PRECACHE_SOUND( "weapons/soldier_throw.wav" ); // grenade toss sound

	PRECACHE_SOUND("zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event

	// get voice pitch
	if( !m_voicePitch ) m_voicePitch = 97 + RANDOM_LONG(0,6);

	PRECACHE_SOUND( "weapons/punch1.wav" );
	PRECACHE_SOUND( "weapons/punch2.wav" );
	PRECACHE_SOUND( "weapons/punch3.wav" );
}	

//=========================================================
// start task
//=========================================================
void CHGrunt :: StartTask ( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
/*	case TASK_GET_PATH_TO_ENEMY:
	{
		if( m_hEnemy == NULL )
		{
			TaskFail();
			return;
		}

		if( m_pSchedule && FStrEq( m_pSchedule->pName, "GruntRunAndFire" ) )
		{

			// it's the same, but in reverse. Try nodes first.
			if( BuildNearestRoute( m_hEnemy->GetAbsOrigin(), m_hEnemy->pev->view_ofs, 0, (m_hEnemy->GetAbsOrigin() - GetLocalOrigin()).Length() ) )
			{
				TaskComplete();
			}
			//	else if( BuildRoute( m_hEnemy->GetAbsOrigin(), bits_MF_TO_ENEMY, m_hEnemy ) )
			//	{
			//		TaskComplete();
			//	}
			else
			{
				// no way to get there =(
				ALERT( at_aiconsole, "HGRUNT: GetPathToEnemy failed!\n" );
				TaskFail();
			}
		}
		else
			CSquadMonster::StartTask( pTask );
		break;
	}*/

	case TASK_GRUNT_CHECK_FIRE:
		if ( !NoFriendlyFire() )
		{
			SetConditions( bits_COND_GRUNT_NOFIRE );
		}
		TaskComplete();
		break;

	case TASK_GRUNT_SPEAK_SENTENCE:
		SpeakSentence();
		TaskComplete();
		break;
	
	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// grunt no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		CSquadMonster ::StartTask( pTask );
		break;

	case TASK_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;

	case TASK_GRUNT_FACE_TOSS_DIR:
		break;

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		CSquadMonster :: StartTask( pTask );
		if (pev->movetype == MOVETYPE_FLY)
			m_IdealActivity = ACT_GLIDE;
		break;

	default: 
		CSquadMonster :: StartTask( pTask );
		break;
	}
}

bool CHGrunt::BodyTurn( const Vector &vecTarget )
{
	float yaw = VecToYaw( vecTarget - GetAbsOrigin() ) - GetAbsAngles().y;

	if( yaw > 180 ) yaw -= 360;
	if( yaw < -180 ) yaw += 360;

	if( yaw < -65 || yaw > 65 )
		return false;

	// turn towards vector
	SetBoneController( 1, yaw );

	return true;
}

//=========================================================
// RunTask
//=========================================================
void CHGrunt :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_GRUNT_FACE_TOSS_DIR:
	{
		// project a point along the toss vector and turn to face that point.
		MakeIdealYaw( GetAbsOrigin() + m_vecTossVelocity * 64 );
		ChangeYaw( pev->yaw_speed );

		if ( FacingIdeal() )
		{
			m_iTaskStatus = TASKSTATUS_COMPLETE;
		}
		break;
	}
	case TASK_WAIT_FACE_ENEMY:
	{
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );

		if( gpGlobals->time > AttackStartTime )
			TaskComplete();

		break;
	}
	default:
		{
			CSquadMonster :: RunTask( pTask );
			break;
		}
	}
}

//=========================================================
// PainSound
//=========================================================
void CHGrunt :: PainSound ( void )
{
	if ( gpGlobals->time > m_flNextPainTime )
	{
#if 0
		if ( RANDOM_LONG(0,99) < 5 )
		{
			// pain sentences are rare
			if (FOkToSpeak())
			{
				SENTENCEG_PlayRndSz(ENT(pev), "HG_PAIN", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, PITCH_NORM);
				JustSpoke();
				return;
			}
		}
#endif 
		switch ( RANDOM_LONG(0,6) )
		{
		case 0:	
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain3.wav", 1, ATTN_NORM );	
			break;
		case 1:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain4.wav", 1, ATTN_NORM );	
			break;
		case 2:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain5.wav", 1, ATTN_NORM );	
			break;
		case 3:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain1.wav", 1, ATTN_NORM );	
			break;
		case 4:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain2.wav", 1, ATTN_NORM );	
			break;
		}

		m_flNextPainTime = gpGlobals->time + 1;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CHGrunt :: DeathSound ( void )
{
	switch ( RANDOM_LONG(0,2) )
	{
	case 0:	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "hgrunt/hg_die1.wav", 1, ATTN_IDLE, 0, RANDOM_LONG( 95,100) );	break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "hgrunt/hg_die2.wav", 1, ATTN_IDLE, 0, RANDOM_LONG( 95, 100 ) );	break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "hgrunt/hg_die3.wav", 1, ATTN_IDLE, 0, RANDOM_LONG( 95, 100 ) );	break;
	}
}

//=========================================================
// SetActivity 
//=========================================================
void CHGrunt :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// grunt is either shooting standing or shooting crouched
		if (HasWeapon( HGRUNT_9MMAR ))//(FBitSet( pev->weapons, HGRUNT_9MMAR))
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_mp5" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_mp5" );
			}
		}
		else
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_shotgun" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_shotgun" );
			}
		}
		break;
	case ACT_RANGE_ATTACK2:
		// grunt is going to a secondary long range attack. This may be a thrown 
		// grenade or fired grenade, we must determine which and pick proper sequence
		if ( HasWeapon( HGRUNT_HANDGRENADE ))
			// get toss anim
			iSequence = LookupSequence( "throwgrenade" );
		else
			// get launch anim
			iSequence = LookupSequence( "launchgrenade" );
		break;
	case ACT_RUN:
		if ( pev->health <= HGRUNT_LIMP_HEALTH )
			// limp!
			iSequence = LookupActivity ( ACT_RUN_HURT );
		else if( m_pSchedule && FStrEq( m_pSchedule->pName, "GruntRunAndFire" ) )
		{
			if( HasWeapon(HGRUNT_9MMAR) )
				iSequence = LookupSequence( "runshootmp5" );
			else
				iSequence = LookupSequence( "runshootshotgun" );
		}
		else
			iSequence = LookupSequence( "run" );
		break;
	case ACT_WALK:
		if ( pev->health <= HGRUNT_LIMP_HEALTH )
			// limp!
			iSequence = LookupActivity ( ACT_WALK_HURT );
		else
			iSequence = LookupActivity ( NewActivity );
		break;
	case ACT_IDLE:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
			NewActivity = ACT_IDLE_ANGRY;

		iSequence = LookupActivity ( NewActivity );
		break;
	default:
		iSequence = LookupActivity ( NewActivity );
		break;
	}
	
	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
			pev->frame = 0;

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

//=========================================================
// Get Schedule!
//=========================================================
Schedule_t *CHGrunt :: GetSchedule( void )
{
	// clear old sentence
	m_iSentence = HGRUNT_SENT_NONE;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling. 
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if (pev->flags & FL_ONGROUND)
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType ( SCHED_GRUNT_REPEL_LAND );
		}
		else
		{
			// repel down a rope, 
			if ( m_MonsterState == MONSTERSTATE_COMBAT )
				return GetScheduleOfType ( SCHED_GRUNT_REPEL_ATTACK );
			else
				return GetScheduleOfType ( SCHED_GRUNT_REPEL );
		}
	}

	// grunts place HIGH priority on running away from danger sounds.
	if ( HasConditions(bits_COND_HEAR_SOUND) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound)
		{
			if (pSound->m_iType & bits_SOUND_DANGER)
			{
				// dangerous sound nearby!
				
				//!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
				// and the grunt should find cover from the blast
				// good place for "SHIT!" or some other colorful verbal indicator of dismay.
				// It's not safe to play a verbal order here "Scatter", etc cause 
				// this may only affect a single individual in a squad. 
				
				if (FOkToSpeak())
				{
					SENTENCEG_PlayRndSz( ENT(pev), "HG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
			
			if (!HasConditions( bits_COND_SEE_ENEMY ) && ( pSound->m_iType & (bits_SOUND_PLAYER | bits_SOUND_COMBAT) ))
				MakeIdealYaw( pSound->m_vecOrigin );
			
			if ( !HasConditions( bits_COND_SEE_ENEMY ) && pSound && (pSound->m_iType & bits_SOUND_COMBAT) && (pSound->m_entindex != ENTINDEX(edict()) ) && (CanInvestigate == 1) && (pev->health > HGRUNT_LIMP_HEALTH) )
			{
				// diffusion - grunts now can investigate sounds (taken from hassassin)
				if( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz(ENT(pev), "HG_INVEST", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_INVESTIGATE_SOUND );
			}
		}
	}

	if( HasConditions( bits_COND_ENEMY_LOST ) )
	{
		if( FOkToSpeak() )
		{
			SENTENCEG_PlayRndSz( ENT( pev ), "HG_LOST", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
			JustSpoke();
		}
		return CBaseMonster :: GetSchedule();
	}

	if( HasFlag(F_ENTITY_ONFIRE) )
		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );

	switch	( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
	{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD | bits_COND_ENEMY_LOST ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

// new enemy
			if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				if ( InSquad() )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;

					if( !g_bGruntAlert && (m_hEnemy != NULL) )
					{
						if( m_hEnemy->IsPlayer() ) // player
						{
							if( HasConditions(bits_COND_SEE_FLASHLIGHT) )
								SENTENCEG_PlayRndSz( ENT( pev ), "HG_FLASHLIGHT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
							else
								SENTENCEG_PlayRndSz( ENT( pev ), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
						}
						else if (
								(m_hEnemy->Classify() != CLASS_PLAYER_ALLY) && 
								(m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE) && 
								(m_hEnemy->Classify() != CLASS_MACHINE)
							)
							// monster
							SENTENCEG_PlayRndSz( ENT(pev), "HG_MONST", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);

						JustSpoke();
						g_bGruntAlert = true;
					}
				}

				if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				{
					if( m_hEnemy 
						&& (m_hEnemy->pev->movetype != MOVETYPE_FLY) 
						&& (
							(m_hEnemy->IsMonster() && !FClassnameIs(m_hEnemy, "monster_target")) 
							|| m_hEnemy->IsPlayer()
							) 
						&& (m_hEnemy->GetAbsOrigin() - GetAbsOrigin()).Length() > 666
						&& pev->health > HGRUNT_LIMP_HEALTH
						&& !HasConditions( bits_COND_LOW_HEALTH )
						)
						return GetScheduleOfType( SCHED_GRUNT_RUN_AND_FIRE );

					return GetScheduleOfType( SCHED_GRUNT_SUPPRESS );
				}
				else
					return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
			}
		// no ammo
			else if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
			{
				//!!!KELLY - this individual just realized he's out of bullet ammo. 
				// He's going to try to find cover to run to and reload, but rarely, if 
				// none is available, he'll drop and reload in the open here. 
				return GetScheduleOfType ( SCHED_GRUNT_COVER_AND_RELOAD );
			}
			
		// damaged just a little
			else if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
			{	
				switch(RANDOM_LONG(0,3))
				{
				case 0: return GetScheduleOfType( SCHED_SMALL_FLINCH ); break;
				case 3:
				{
					if( pev->health < pev->max_health * 0.2f )
					{
						if( FOkToSpeak() ) // && RANDOM_LONG(0,1))
						{
							//SENTENCEG_PlayRndSz( ENT(pev), "HG_COVER", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
							m_iSentence = HGRUNT_SENT_COVER;
							//JustSpoke();
						}
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
					}
					else
						return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				break;
				default: return GetScheduleOfType( SCHED_RANGE_ATTACK1 ); break;
				}
			}
// can kick
			else if ( HasConditions ( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );

// can grenade launch

			else if ( HasWeapon( HGRUNT_GRENADELAUNCHER ) && HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				// shoot a grenade if you can
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}
// can shoot
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( InSquad() )
				{
					// if the enemy has eluded the squad and a squad member has just located the enemy
					// and the enemy does not see the squad member, issue a call to the squad to waste a 
					// little time and give the player a chance to turn.
					if ( MySquadLeader()->m_fEnemyEluded && !HasConditions ( bits_COND_ENEMY_FACING_ME ) )
					{
						MySquadLeader()->m_fEnemyEluded = FALSE;
						return GetScheduleOfType ( SCHED_GRUNT_FOUND_ENEMY );
					}
				}

				if ( OccupySlot ( bits_SLOTS_HGRUNT_ENGAGE ) )
				{
					// try to take an available ENGAGE slot
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					// throw a grenade if can and no engage slots are available
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				else
				{
					// hide!
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
// can't see enemy
			else if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					//!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
					if (FOkToSpeak())
					{
						if( CanSpawnDrone && !DroneSpawned )
							SENTENCEG_PlayRndSz( ENT(pev), "HG_DRONE", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						else
							SENTENCEG_PlayRndSz( ENT(pev), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				else if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				{
					//!!!KELLY - grunt cannot see the enemy and has just decided to 
					// charge the enemy's position. 
					if (FOkToSpeak())
					{
						//SENTENCEG_PlayRndSz( ENT(pev), "HG_CHARGE", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						m_iSentence = HGRUNT_SENT_CHARGE;
						//JustSpoke();
					}

					if( m_hEnemy
						&& pev->health > HGRUNT_LIMP_HEALTH
						&& !HasConditions( bits_COND_LOW_HEALTH )
						&& (m_hEnemy->pev->movetype != MOVETYPE_FLY)
						&& (
							(m_hEnemy->IsMonster() && !FClassnameIs( m_hEnemy, "monster_target" ))
							|| m_hEnemy->IsPlayer()
							)
						)
						return GetScheduleOfType( SCHED_GRUNT_RUN_AND_FIRE );

					return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
				}
				else
				{
					//!!!KELLY - grunt is going to stay put for a couple seconds to see if
					// the enemy wanders back out into the open, or approaches the
					// grunt's covered position. Good place for a taunt, I guess?
					if (FOkToSpeak() && RANDOM_LONG(0,1))
					{
						SENTENCEG_PlayRndSz( ENT(pev), "HG_TAUNT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}
			
			if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType ( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
		} break;
	}
	
	// no special cases here, call the base class
	return CSquadMonster :: GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t* CHGrunt :: GetScheduleOfType ( int Type ) 
{
	switch	( Type )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			if ( InSquad() )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) ) // && g_iSkillLevel == SKILL_HARD
				{
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( ENT(pev), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return slGruntTossGrenadeCover;
				}
				else
				{
					return &slGruntTakeCover[ 0 ];
				}
			}
			else
			{
				if ( RANDOM_LONG(0,1) )
				{
					return &slGruntTakeCover[ 0 ];
				}
				else
				{
					return slGruntTossGrenadeCover;
//					return &slGruntGrenadeCover[ 0 ];
				}
			}
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slGruntTakeCoverFromBestSound[ 0 ];
		}
	case SCHED_GRUNT_TAKECOVER_FAILED:
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}

			return GetScheduleOfType ( SCHED_FAIL );
		}
		break;
	case SCHED_GRUNT_ELOF_FAIL:
		{
			// human grunt is unable to move to a position that allows him to attack the enemy.
		//	if( FVisible(m_hEnemy) )
		//	{
		//		if (pev->health < 50)
		//			return GetScheduleOfType ( SCHED_GRUNT_SUPPRESS ); // diffusion - low hp, nervous
		//		else
		//			return GetScheduleOfType ( SCHED_RANGE_ATTACK1 ); // or just try to attack the enemy from the current position
		//	}
		//	else
			if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			return GetScheduleOfType( SCHED_MOVE_TO_ENEMY_LKPRUN ); // can't see enemy. try to reposition
		}
		break;
	case SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE:
		{
			return &slGruntEstablishLineOfFire[ 0 ];
		}
		break;
	case SCHED_GRUNT_RUN_AND_FIRE:
		{
			return &slGruntRunAndFire[0];
		}
		break;
	case SCHED_RANGE_ATTACK1:
		{
			AttackStartTime = gpGlobals->time + CombatWaitTime;

			if (m_fStanding)
				return &slGruntRangeAttack1B[ 0 ];
			else
				return &slGruntRangeAttack1A[ 0 ];
		}
	case SCHED_RANGE_ATTACK2:
		{
			AttackStartTime = gpGlobals->time + CombatWaitTime;
			return &slGruntRangeAttack2[ 0 ];
		}
	case SCHED_COMBAT_FACE:
		{
			return &slGruntCombatFace[ 0 ];
		}
	case SCHED_GRUNT_WAIT_FACE_ENEMY:
		{
			return &slGruntWaitInCover[ 0 ];
		}
	case SCHED_GRUNT_SWEEP:
		{
			return &slGruntSweep[ 0 ];
		}
	case SCHED_GRUNT_COVER_AND_RELOAD:
		{
			return &slGruntHideReload[ 0 ];
		}
	case SCHED_GRUNT_FOUND_ENEMY:
		{
			return &slGruntFoundEnemy[ 0 ];
		}
	case SCHED_VICTORY_DANCE:
		{
			if ( InSquad() )
			{
				if ( !IsLeader() )
				{
					return &slGruntFail[ 0 ];
				}
			}

			return &slGruntVictoryDance[ 0 ];
		}
	case SCHED_GRUNT_SUPPRESS:
		{
			if ( m_hEnemy->IsPlayer() && m_fFirstEncounter )
			{
				m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
				return &slGruntSignalSuppress[ 0 ];
			}
			else
			{
				return &slGruntSuppress[ 0 ];
			}
		}
	case SCHED_FAIL:
		{
			if ( m_hEnemy != NULL )
			{
				// grunt has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slGruntCombatFail[ 0 ];
			}

			return &slGruntFail[ 0 ];
		}
	case SCHED_GRUNT_REPEL:
		{
			Vector vecVelocity = GetAbsVelocity();
			if (vecVelocity.z > -128)
			{
				vecVelocity.z -= 32;
				SetAbsVelocity( vecVelocity );
			}
			return &slGruntRepel[ 0 ];
		}
	case SCHED_GRUNT_REPEL_ATTACK:
		{
			Vector vecVelocity = GetAbsVelocity();
			if (vecVelocity.z > -128)
			{
				vecVelocity.z -= 32;
				SetAbsVelocity( vecVelocity );
			}
			return &slGruntRepelAttack[ 0 ];
		}
	case SCHED_GRUNT_REPEL_LAND:
		{
			return &slGruntRepelLand[ 0 ];
		}
	default:
		{
			return CSquadMonster :: GetScheduleOfType ( Type );
		}
	}
}


//=========================================================
// CHGruntRepel - when triggered, spawns a monster_human_grunt
// repelling down a line.
//=========================================================
class CHGruntRepel : public CBaseMonster
{
	DECLARE_CLASS( CHGruntRepel, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iSpriteTexture;	// Don't save, precache
};

LINK_ENTITY_TO_CLASS( monster_grunt_repel, CHGruntRepel );

void CHGruntRepel::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT;
}

void CHGruntRepel::Precache( void )
{
	UTIL_PrecacheOther( "monster_human_grunt" );
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/rope.spr" );
}

void CHGruntRepel::Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	TraceResult tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -4096.0), dont_ignore_monsters, ENT(pev), &tr);
	/*
	if ( tr.pHit && Instance( tr.pHit )->pev->solid != SOLID_BSP) 
		return NULL;
	*/

	CBaseEntity *pEntity = NULL;
	if( value == 1.0f )
		pEntity = Create( "monster_security_soldier", GetAbsOrigin(), GetAbsAngles() );
	else
		pEntity = Create( "monster_human_grunt", GetAbsOrigin(), GetAbsAngles() );

	if( !pEntity )
		return;
	CBaseMonster *pGrunt = pEntity->MyMonsterPointer( );
	pGrunt->pev->movetype = MOVETYPE_FLY;
	pGrunt->SetAbsVelocity( Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) ));
	pGrunt->SetActivity( ACT_GLIDE );
	// UNDONE: position?
	pGrunt->m_vecLastPosition = tr.vecEndPos;
	// diffusion - pass trigger conditions too
	pGrunt->m_iTriggerCondition = m_iTriggerCondition;
	pGrunt->m_iTriggerCondition2 = m_iTriggerCondition2;
	pGrunt->m_iTriggerCondition3 = m_iTriggerCondition3;
	pGrunt->m_iszTriggerTarget = m_iszTriggerTarget;
	pGrunt->m_iszTriggerTarget2 = m_iszTriggerTarget2;
	pGrunt->m_iszTriggerTarget3 = m_iszTriggerTarget3;

	CBeam *pBeam = CBeam::BeamCreate( "sprites/rope.spr", 10 );
	pBeam->PointEntInit( GetAbsOrigin() + Vector( 0, 0, 112 ), pGrunt->entindex() );
	pBeam->SetFlags( BEAM_FSOLID );
	pBeam->SetColor( 255, 255, 255 );
	pBeam->SetThink( &CBaseEntity::SUB_Remove );
	pBeam->pev->nextthink = gpGlobals->time + -4096.0 * tr.flFraction / pGrunt->GetAbsVelocity().z + 0.5;

	UTIL_Remove( this );
}

class CSecuritySoldierRepel : public CHGruntRepel
{
	DECLARE_CLASS( CSecuritySoldierRepel, CHGruntRepel );
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iSpriteTexture;	// Don't save, precache
};

LINK_ENTITY_TO_CLASS( monster_security_repel, CSecuritySoldierRepel );

void CSecuritySoldierRepel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
}

void CSecuritySoldierRepel::Precache( void )
{
	UTIL_PrecacheOther( "monster_security_soldier" );
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/rope.spr" );
}

void CSecuritySoldierRepel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BaseClass::Use( pActivator, pCaller, useType, 1.0f );
}

//=========================================================
// DEAD HGRUNT PROP
//=========================================================
class CDeadHGrunt : public CBaseMonster
{
	DECLARE_CLASS( CDeadHGrunt, CBaseMonster );
public:
	void Spawn( void );
	int Classify ( void ) { return CLASS_HUMAN_MILITARY; }

	void KeyValue( KeyValueData *pkvd );

	int m_iPose;// which sequence to display	-- temporary, don't need to save
	static char *m_szPoses[3];
};

char *CDeadHGrunt::m_szPoses[] = { "deadstomach", "deadside", "deadsitting" };

void CDeadHGrunt::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else 
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_hgrunt_dead, CDeadHGrunt );

//=========================================================
// ********** DeadHGrunt SPAWN **********
//=========================================================
void CDeadHGrunt :: Spawn( void )
{
	PRECACHE_MODEL("models/npc/hgrunt.mdl");
	SET_MODEL(ENT(pev), "models/npc/hgrunt.mdl");

	pev->effects		= 0;
	pev->yaw_speed		= 8;
	pev->sequence		= 0;
	m_bloodColor		= BLOOD_COLOR_RED;

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );

	if (pev->sequence == -1)
	{
		ALERT ( at_console, "Dead hgrunt with bad pose\n" );
	}

	// Corpses have less health
	pev->health			= 8;

	// map old bodies onto new bodies
	switch( pev->body )
	{
	case 0: // Grunt with Gun
		pev->body = 0;
		pev->skin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_MASK );
		SetBodygroup( GUN_GROUP, GUN_MP5 );
		break;
	case 1: // Commander with Gun
		pev->body = 0;
		pev->skin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_COMMANDER );
		SetBodygroup( GUN_GROUP, GUN_MP5 );
		break;
	case 2: // Grunt no Gun
		pev->body = 0;
		pev->skin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_MASK );
		SetBodygroup( GUN_GROUP, GUN_NONE );
		break;
	case 3: // Commander no Gun
		pev->body = 0;
		pev->skin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_COMMANDER );
		SetBodygroup( GUN_GROUP, GUN_NONE );
		break;
	}

	MonsterInitDead();
}

//

//

//

//

//

//

//
// ===================================================================================================================================================================================
// ============================== Diffusion Alien Grunt for Chapter #4 (Blue dimension) ==============================================================================================
// ===================================================================================================================================================================================

#define ALIEN_EYE		"sprites/alien_eye.spr"//"sprites/gargeye1.spr"
#define GUN_ALTERNATE	3

class CHGruntAlien : public CHGrunt
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify ( void );
	int IRelationship ( CBaseEntity *pTarget );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	void Shotgun( void );
	void Shoot( void );
	void GibMonster(void);
	void Killed( entvars_t *pevAttacker, int iGib );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void ClearEffects(void);
	void SpeakSentence( void );
	static const char *pAlienGruntSentences[];
	void IdleSound(void);
	void PainSound(void);
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType ( int Type );
	void DeathSound( void );
};

const char *CHGruntAlien::pAlienGruntSentences[] = 
{
	"AG_GREN", // grenade scared grunt
	"AG_ALERT", // sees enemy
	"AG_MONSTER", // sees monsters
	"AG_COVER", // running to cover
	"AG_THROW", // about to throw grenade
	"AG_CHARGE",  // running out to get the enemy
	"AG_TAUNT", // say rude things
};

LINK_ENTITY_TO_CLASS( monster_alien_soldier , CHGruntAlien);

void CHGruntAlien :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/hgrunt_alien.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= DONT_BLEED;
	pev->effects		= 0;
	if( !pev->health ) pev->health = g_AlienRoboHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	m_flFieldOfView		= VIEW_FIELD_WIDE;//180         0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime	= gpGlobals->time;
	m_iSentence			= HGRUNT_SENT_NONE;

	m_afCapability		= bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded		= FALSE;
	m_fFirstEncounter	= TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector ( 0, 0, 55 );

	if (FStrEq( STRING(wpns), "9mmar" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AlternateShoot = 0;
	}
	else if (FStrEq( STRING(wpns), "9mmar_hg" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon(HGRUNT_HANDGRENADE);
		AlternateShoot = 0;
	}
	else if (FStrEq( STRING(wpns), "9mmar_gl" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon(HGRUNT_GRENADELAUNCHER);
		AlternateShoot = 0;
	}
	else if (FStrEq( STRING(wpns), "shotgun" ))
	{
		AddWeapon(HGRUNT_SHOTGUN);
	}
	else if (FStrEq( STRING(wpns), "shotgun_hg" ))
	{
		AddWeapon(HGRUNT_SHOTGUN);
		AddWeapon(HGRUNT_HANDGRENADE);
	}
	else if (FStrEq( STRING(wpns), "9mmar_hg_alt" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon(HGRUNT_HANDGRENADE);
		AlternateShoot = 1;
	}
	else if (FStrEq( STRING(wpns), "9mmar_gl_alt" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon(HGRUNT_GRENADELAUNCHER);
		AlternateShoot = 1;
	}
	else if (FStrEq( STRING(wpns), "9mmar_alt" ))
	{
		AddWeapon(HGRUNT_9MMAR);
		AlternateShoot = 1;
	}
	else //if ( FStringNull(wpns) )
	{
		// shuffle properly because it's still not exactly random it seems
		int rnd = RANDOM_LONG( 0, 3 );
		rnd = RANDOM_LONG( 0, 3 );
		rnd = RANDOM_LONG( 0, 3 );

		switch( rnd )
		{
		case 0:
			AddWeapon(HGRUNT_9MMAR);
			AddWeapon(HGRUNT_HANDGRENADE);
			AlternateShoot = RANDOM_LONG(0,1);
			break;
		case 1:
			AddWeapon(HGRUNT_9MMAR);
			AddWeapon(HGRUNT_GRENADELAUNCHER);
			AlternateShoot = RANDOM_LONG(0,1);
			break;
		case 2:
			AddWeapon(HGRUNT_SHOTGUN);
			break;
		case 3:
			AddWeapon(HGRUNT_SHOTGUN);
			AddWeapon(HGRUNT_HANDGRENADE);
			break;
		}
	}

	AlienEye = CSprite::SpriteCreate( ALIEN_EYE, GetAbsOrigin(), TRUE );
	if( AlienEye )
	{
		AlienEye->SetAttachment( edict(), 4 );
		AlienEye->SetScale( 0.1 );
	}

	if (HasWeapon( HGRUNT_SHOTGUN ))
	{
		SetBodygroup( GUN_GROUP, GUN_SHOTGUN );
		m_cClipSize		= 8;
		pev->skin = 2; // red skin and eye
		if( AlienEye )
			AlienEye->SetTransparency( kRenderTransAdd, 255, 0, 0, 150, 0 );
	}
	else
	{
		m_cClipSize		= GRUNT_CLIP_SIZE;

		//AlternateShoot = RANDOM_LONG(0,1);
		if (AlternateShoot == 1)
		{
			SetBodygroup( GUN_GROUP, GUN_ALTERNATE );
			pev->skin = 0; // blue skin and eye
			if( AlienEye )
				AlienEye->SetTransparency( kRenderTransAdd, 0, 120, 250, 150, 0 );
		}
		else
		{
			SetBodygroup( GUN_GROUP, GUN_MP5 );
			pev->skin = 1; // black skin, orange eye
			if( AlienEye )
				AlienEye->SetTransparency( kRenderTransAdd, 255, 100, 0, 150, 0 );
		}
	}

	m_cAmmoLoaded		= m_cClipSize;

	CTalkMonster::g_talkWaitTime = 0;

	CanSpawnDrone = false; // alien guys don't have pocket drones, they are separate npcs

	if( RANDOM_LONG(0,2) == 0)
		CanInvestigate = 1;
	else
		CanInvestigate = 0;

	CombatWaitTime = g_AlienCombatWaitTime[g_iSkillLevel];

	SetFlag(F_METAL_MONSTER);

	if( AlienEye )
		AlienEye->SetFadeDistance( 1000 );

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CHGruntAlien :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/hgrunt_alien.mdl");

	PRECACHE_SOUND( "hgrunt/gr_mgun1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_mgun2.wav" );
	
	PRECACHE_SOUND( "hgrunt/ag_die1.wav" );
	PRECACHE_SOUND( "hgrunt/ag_die2.wav" );
	PRECACHE_SOUND( "hgrunt/ag_die3.wav" );

	PRECACHE_SOUND( "hgrunt/ag_pain1.wav" );
	PRECACHE_SOUND( "hgrunt/ag_pain2.wav" );
	PRECACHE_SOUND( "hgrunt/ag_pain3.wav" );
	PRECACHE_SOUND( "hgrunt/ag_pain4.wav" );
	PRECACHE_SOUND( "hgrunt/ag_pain5.wav" );

	PRECACHE_SOUND( "hgrunt/ag_nade_beep.wav" );

	PRECACHE_SOUND( "weapons/alien_launcher.wav" );
	PRECACHE_SOUND( "weapons/alien_shotgun1.wav" );
	PRECACHE_SOUND( "weapons/alien_shotgun2.wav" );
	PRECACHE_SOUND( "weapons/alien_shotgun3.wav" );
	PRECACHE_SOUND( "weapons/alien_shotgun4.wav" );
	PRECACHE_SOUND( "weapons/alien_hks1.wav" );
	PRECACHE_SOUND( "weapons/alien_hks2.wav" );
	PRECACHE_SOUND( "weapons/alien_reload1.wav" );

	PRECACHE_SOUND("zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event

	// get voice pitch
	if( !m_voicePitch ) m_voicePitch = 97 + RANDOM_LONG(0,6);

	PRECACHE_MODEL( "sprites/xspark4.spr");
	PRECACHE_MODEL( ALIEN_EYE );

	UTIL_PrecacheOther( "alien_energy_ball" );

	UTIL_PrecacheOther( "shootball" ); // combine ball
	PRECACHE_SOUND( "comball/spawn.wav" );

	UTIL_PrecacheOther( "alien_super_ball" ); // instead of grenade launcher
	PRECACHE_SOUND( "weapons/alien_charge.wav" ); // used in mdl file
	UTIL_PrecacheOther( "alien_shotgun_ball" );
	UTIL_PrecacheOther( "shock_beam" );
	PRECACHE_SOUND( "weapons/ar2_shoot.wav" );
	PRECACHE_MODEL("sprites/muzzle_shock.spr");

	PRECACHE_SOUND("drone/drone_hit1.wav");
	PRECACHE_SOUND("drone/drone_hit2.wav");
	PRECACHE_SOUND("drone/drone_hit3.wav");
	PRECACHE_SOUND("drone/drone_hit4.wav");
	PRECACHE_SOUND("drone/drone_hit5.wav");

	PRECACHE_MODEL( "models/grenade_alien.mdl" );

	PRECACHE_SOUND( "weapons/soldier_throw.wav" ); // grenade toss sound
}	

void CHGruntAlien::DeathSound( void )
{
	switch( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "hgrunt/ag_die1.wav", 1, ATTN_IDLE, 0, RANDOM_LONG( 95, 100 ) );	break;
	case 1: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "hgrunt/ag_die2.wav", 1, ATTN_IDLE, 0, RANDOM_LONG( 95, 100 ) );	break;
	case 2: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "hgrunt/ag_die3.wav", 1, ATTN_IDLE, 0, RANDOM_LONG( 95, 100 ) );	break;
	}
}

int	CHGruntAlien :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_ALIEN_MILITARY;
}

int CHGruntAlien::IRelationship ( CBaseEntity *pTarget )
{
	return CSquadMonster::IRelationship( pTarget );
}

BOOL CHGruntAlien :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 3072 && flDot >= 0.5 && NoFriendlyFire() )
	{	
		TraceResult	tr;

		if ( !m_hEnemy->IsPlayer() && flDist <= 64 )
			// kick nonclients, but don't shoot at them.
			return FALSE;

		// diffusion - some changes on crouching/standing behaviour
		if (flDist <= 800)
			m_fStanding = 1;
		else
			m_fStanding = 0;

		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget(vecSrc), ignore_monsters, ignore_glass, ENT(pev), &tr);

		if ( tr.flFraction == 1.0 )
			return TRUE;
	}

	return FALSE;
}

void CHGruntAlien::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
//	if (RANDOM_LONG(0,3) == 0)
//		UTIL_Ricochet( ptr->vecEndPos, 0.5 );

	if( HasSpawnFlags( SF_MONSTER_NODAMAGE ) )
		return;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return;

	UTIL_Sparks ( ptr->vecEndPos );

	switch(RANDOM_LONG(0,4))
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}
	
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CHGruntAlien :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{		
	if( HasSpawnFlags(SF_MONSTER_NODAMAGE) )
		return 0;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return 0;
	
	Forget( bits_MEMORY_INCOVER );

	if( pevInflictor->owner == edict() )
		return 0; // diffusion - this should be done better somehow

	return CSquadMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );

}

void CHGruntAlien::GibMonster(void)
{
	Vector vecOrigin = GetAbsOrigin();
			
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_BREAKMODEL);

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
		WRITE_BYTE( RANDOM_LONG(30,40) );
			// duration
		WRITE_BYTE( 20 );// 3.0 seconds
			// flags
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( 0 );
}

void CHGruntAlien :: Killed( entvars_t *pevAttacker, int iGib )
{
	// diffusion - set skin with not a fullbright eye
	pev->skin += 3;
	
	CSquadMonster::Killed( pevAttacker, iGib );
}

void CHGruntAlien :: ClearEffects(void)
{
	if( AlienEye )
	{
		UTIL_Remove( AlienEye );
		AlienEye = NULL;
	}
}

// ================================TALKING================================================================================

void CHGruntAlien :: SpeakSentence( void )
{
	if ( m_iSentence == HGRUNT_SENT_NONE )
	{
		// no sentence cued up.
		return; 
	}

	if (FOkToSpeak())
	{
		SENTENCEG_PlayRndSz( ENT(pev), pAlienGruntSentences[ m_iSentence ], HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
		JustSpoke();
	}
}

void CHGruntAlien :: IdleSound( void )
{
	if (FOkToSpeak() && (g_fGruntQuestion || RANDOM_LONG(0,1)))
	{
		if (!g_fGruntQuestion)
		{
			// ask question or make statement
			switch (RANDOM_LONG(0,1))
			{
			case 0: // question
				SENTENCEG_PlayRndSz(ENT(pev), "AG_QUEST", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				g_fGruntQuestion = 2;
				break;
			case 1: // statement
				SENTENCEG_PlayRndSz(ENT(pev), "AG_IDLE", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			}
		}
		else
		{
			switch (g_fGruntQuestion)
			{
			default:
			case 2: // question 
				SENTENCEG_PlayRndSz(ENT(pev), "AG_ANSWER", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			}
			g_fGruntQuestion = 0;
		}
		JustSpoke();
	}
}

void CHGruntAlien :: PainSound ( void )
{
	if ( gpGlobals->time > m_flNextPainTime )
	{
#if 0
		if ( RANDOM_LONG(0,99) < 5 )
		{
			// pain sentences are rare
			if (FOkToSpeak())
			{
				SENTENCEG_PlayRndSz(ENT(pev), "AG_PAIN", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, PITCH_NORM);
				JustSpoke();
				return;
			}
		}
#endif 
		switch ( RANDOM_LONG(0,6) )
		{
		case 0:	
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/ag_pain3.wav", 1, ATTN_NORM );	
			break;
		case 1:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/ag_pain4.wav", 1, ATTN_NORM );	
			break;
		case 2:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/ag_pain5.wav", 1, ATTN_NORM );	
			break;
		case 3:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/ag_pain1.wav", 1, ATTN_NORM );	
			break;
		case 4:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/ag_pain2.wav", 1, ATTN_NORM );	
			break;
		}

		m_flNextPainTime = gpGlobals->time + 1;
	}
}
//===============================================================================================================

//=========================================================
// Get Schedule!
//=========================================================
Schedule_t *CHGruntAlien :: GetSchedule( void )
{	
	// clear old sentence
	m_iSentence = HGRUNT_SENT_NONE;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling. 
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if (pev->flags & FL_ONGROUND)
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType ( SCHED_GRUNT_REPEL_LAND );
		}
		else
		{
			// repel down a rope, 
			if ( m_MonsterState == MONSTERSTATE_COMBAT )
				return GetScheduleOfType ( SCHED_GRUNT_REPEL_ATTACK );
			else
				return GetScheduleOfType ( SCHED_GRUNT_REPEL );
		}
	}

	// grunts place HIGH priority on running away from danger sounds.
	if ( HasConditions(bits_COND_HEAR_SOUND) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound && (pSound->m_entindex != ENTINDEX(edict())) )
		{
			if (pSound->m_iType & bits_SOUND_DANGER)
			{
				// dangerous sound nearby!
				
				//!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
				// and the grunt should find cover from the blast
				// good place for "SHIT!" or some other colorful verbal indicator of dismay.
				// It's not safe to play a verbal order here "Scatter", etc cause 
				// this may only affect a single individual in a squad. 
				
				if (FOkToSpeak())
				{
					SENTENCEG_PlayRndSz( ENT(pev), "AG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
			
			if (!HasConditions( bits_COND_SEE_ENEMY ) && ( pSound->m_iType & (bits_SOUND_PLAYER | bits_SOUND_COMBAT) ))
				MakeIdealYaw( pSound->m_vecOrigin );
			
			if ( !HasConditions( bits_COND_SEE_ENEMY ) && (pSound->m_iType & bits_SOUND_COMBAT) && (CanInvestigate == 1) && (pev->health > HGRUNT_LIMP_HEALTH) )
				// diffusion - grunts now can investigate sounds (taken from hassassin)
				return GetScheduleOfType( SCHED_INVESTIGATE_SOUND );
		}
	}

	if( HasFlag(F_ENTITY_ONFIRE) )
		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );

	switch	( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD | bits_COND_ENEMY_LOST ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

// new enemy
			if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				if ( InSquad() )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;

					if (FOkToSpeak())// && RANDOM_LONG(0,1))
					{
						if (m_hEnemy != NULL)
							SENTENCEG_PlayRndSz( ENT(pev), "AG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);

						JustSpoke();
					}
				}

				if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				{
					if( m_hEnemy
						&& (m_hEnemy->pev->movetype != MOVETYPE_FLY)
						&& (
							(m_hEnemy->IsMonster() && !FClassnameIs( m_hEnemy, "monster_target" ))
							|| m_hEnemy->IsPlayer()
							)
						&& (m_hEnemy->GetAbsOrigin() - GetAbsOrigin()).Length() > 666
						&& pev->health > HGRUNT_LIMP_HEALTH
						&& !HasConditions( bits_COND_LOW_HEALTH )
						)
						return GetScheduleOfType( SCHED_GRUNT_RUN_AND_FIRE );

					return GetScheduleOfType( SCHED_GRUNT_SUPPRESS );
				}
				else
					return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
			}
// no ammo
			else if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
			{
				//!!!KELLY - this individual just realized he's out of bullet ammo. 
				// He's going to try to find cover to run to and reload, but rarely, if 
				// none is available, he'll drop and reload in the open here. 
				return GetScheduleOfType ( SCHED_GRUNT_COVER_AND_RELOAD );
			}
			
// damaged just a little
			else if( HasConditions( bits_COND_LIGHT_DAMAGE ) )
			{
				switch( RANDOM_LONG( 0, 3 ) )
				{
				case 0: return GetScheduleOfType( SCHED_SMALL_FLINCH ); break;
				case 3:
				{
					if( pev->health < pev->max_health * 0.2f )
					{
						if( FOkToSpeak() ) // && RANDOM_LONG(0,1))
						{
							//SENTENCEG_PlayRndSz( ENT(pev), "HG_COVER", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
							m_iSentence = HGRUNT_SENT_COVER;
							//JustSpoke();
						}
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
					}
					else
						return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				break;
				default: return GetScheduleOfType( SCHED_RANGE_ATTACK1 ); break;
				}
			}
// can kick
			else if ( HasConditions ( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );
// can grenade launch

			else if ( HasWeapon( HGRUNT_GRENADELAUNCHER ) && HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				// shoot a grenade if you can
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}
// can shoot
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( InSquad() )
				{
					// if the enemy has eluded the squad and a squad member has just located the enemy
					// and the enemy does not see the squad member, issue a call to the squad to waste a 
					// little time and give the player a chance to turn.
					if ( MySquadLeader()->m_fEnemyEluded && !HasConditions ( bits_COND_ENEMY_FACING_ME ) )
					{
						MySquadLeader()->m_fEnemyEluded = FALSE;
						return GetScheduleOfType ( SCHED_GRUNT_FOUND_ENEMY );
					}
				}

				if ( OccupySlot ( bits_SLOTS_HGRUNT_ENGAGE ) )
				{
					// try to take an available ENGAGE slot
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					// throw a grenade if can and no engage slots are available
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				else
				{
					// hide!
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
// can't see enemy
			else if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					//!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( ENT(pev), "AG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				else if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				{
					//!!!KELLY - grunt cannot see the enemy and has just decided to 
					// charge the enemy's position. 
					if (FOkToSpeak())// && RANDOM_LONG(0,1))
					{
						//SENTENCEG_PlayRndSz( ENT(pev), "HG_CHARGE", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						m_iSentence = HGRUNT_SENT_CHARGE;
						//JustSpoke();
					}
					
					if( m_hEnemy
						&& pev->health > HGRUNT_LIMP_HEALTH
						&& !HasConditions( bits_COND_LOW_HEALTH )
						&& (m_hEnemy->pev->movetype != MOVETYPE_FLY)
						&& (
							(m_hEnemy->IsMonster() && !FClassnameIs( m_hEnemy, "monster_target" ))
							|| m_hEnemy->IsPlayer()
							)
						)
						return GetScheduleOfType( SCHED_GRUNT_RUN_AND_FIRE );

					return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
				}
				else
				{
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}
			
			if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
			}
		} break;
	}
	
	// no special cases here, call the base class
	return CSquadMonster :: GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t* CHGruntAlien :: GetScheduleOfType ( int Type ) 
{
	switch	( Type )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			if ( InSquad() )
			{
				return &slGruntTakeCover[0];
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) ) // && g_iSkillLevel == SKILL_HARD
				{
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( ENT(pev), "AG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return slGruntTossGrenadeCover;
				}
				else
				{
					return &slGruntTakeCover[ 0 ];
				}
			}
			else
			{
				if ( RANDOM_LONG(0,1) )
				{
					return &slGruntTakeCover[ 0 ];
				}
				else
				{
					return slGruntTossGrenadeCover;
//					return &slGruntGrenadeCover[ 0 ];
				}
			}
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slGruntTakeCoverFromBestSound[ 0 ];
		}
	case SCHED_GRUNT_TAKECOVER_FAILED:
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}

			return GetScheduleOfType ( SCHED_FAIL );
		}
		break;
	case SCHED_GRUNT_ELOF_FAIL:
		{
			// human grunt is unable to move to a position that allows him to attack the enemy.
	//		if( FVisible(m_hEnemy) )
	//		{
	//			if (pev->health < 50)
	//				return GetScheduleOfType ( SCHED_GRUNT_SUPPRESS ); // diffusion - low hp, nervous
	//			else
	//				return GetScheduleOfType ( SCHED_RANGE_ATTACK1 ); // or just try to attack the enemy from the current position
	//		}
	//		else
		if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		return GetScheduleOfType( SCHED_MOVE_TO_ENEMY_LKPRUN ); // can't see enemy. try to reposition
		}
		break;
	case SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE:
		{
			return &slGruntEstablishLineOfFire[ 0 ];
		}
		break;
	case SCHED_GRUNT_RUN_AND_FIRE:
		{
			return &slGruntRunAndFire[0];
		}
		break;
	case SCHED_RANGE_ATTACK1:
		{
			AttackStartTime = gpGlobals->time + CombatWaitTime;
			if (m_fStanding)
				return &slGruntRangeAttack1B[ 0 ];
			else
				return &slGruntRangeAttack1A[ 0 ];
		}
	case SCHED_RANGE_ATTACK2:
		{
			AttackStartTime = gpGlobals->time + CombatWaitTime;
			return &slGruntRangeAttack2[ 0 ];
		}
	case SCHED_COMBAT_FACE:
		{
			return &slGruntCombatFace[ 0 ];
		}
	case SCHED_GRUNT_WAIT_FACE_ENEMY:
		{
			return &slGruntWaitInCover[ 0 ];
		}
	case SCHED_GRUNT_SWEEP:
		{
			return &slGruntSweep[ 0 ];
		}
	case SCHED_GRUNT_COVER_AND_RELOAD:
		{
			return &slGruntHideReload[ 0 ];
		}
	case SCHED_GRUNT_FOUND_ENEMY:
		{
			return &slGruntFoundEnemy[ 0 ];
		}
	case SCHED_VICTORY_DANCE:
		{
			if ( InSquad() )
			{
				if ( !IsLeader() )
				{
					return &slGruntFail[ 0 ];
				}
			}

			return &slGruntVictoryDance[ 0 ];
		}
	case SCHED_GRUNT_SUPPRESS:
		{
			if ( m_hEnemy->IsPlayer() && m_fFirstEncounter )
			{
				m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
				return &slGruntSignalSuppress[ 0 ];
			}
			else
			{
				return &slGruntSuppress[ 0 ];
			}
		}
	case SCHED_FAIL:
		{
			if ( m_hEnemy != NULL )
			{
				// grunt has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slGruntCombatFail[ 0 ];
			}

			return &slGruntFail[ 0 ];
		}
	case SCHED_GRUNT_REPEL:
		{
			Vector vecVelocity = GetAbsVelocity();
			if (vecVelocity.z > -128)
			{
				vecVelocity.z -= 32;
				SetAbsVelocity( vecVelocity );
			}
			return &slGruntRepel[ 0 ];
		}
	case SCHED_GRUNT_REPEL_ATTACK:
		{
			Vector vecVelocity = GetAbsVelocity();
			if (vecVelocity.z > -128)
			{
				vecVelocity.z -= 32;
				SetAbsVelocity( vecVelocity );
			}
			return &slGruntRepelAttack[ 0 ];
		}
	case SCHED_GRUNT_REPEL_LAND:
		{
			return &slGruntRepelLand[ 0 ];
		}
	default:
		{
			return CSquadMonster :: GetScheduleOfType ( Type );
		}
	}
}

void CHGruntAlien :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch( pEvent->event )
	{
		case HGRUNT_AE_DROP_GUN:          // diffusion - only when the flag is set, grunts drop guns
		{
			if( HasSpawnFlags(SF_MONSTER_NO_WPN_DROP) && GetBodygroup( GUN_GROUP ) != GUN_NONE )
			{
				Vector	vecGunPos;
				Vector	vecGunAngles;

				GetAttachment( 0, vecGunPos, vecGunAngles );

				// switch to body group with no gun.
				SetBodygroup( GUN_GROUP, GUN_NONE );

				// now spawn a gun.
				if (HasWeapon( HGRUNT_SHOTGUN ))
					 DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
				else
					 DropItem( "weapon_9mmAR", vecGunPos, vecGunAngles );

				if (HasWeapon( HGRUNT_GRENADELAUNCHER ))
					DropItem( "ammo_ARgrenades", BodyTarget( GetAbsOrigin() ), vecGunAngles );
			}
		}
		break;

		case HGRUNT_AE_RELOAD:
			EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/alien_reload1.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions(bits_COND_NO_AMMO_LOADED);
			break;

		case HGRUNT_AE_GREN_TOSS:
		{
			UTIL_MakeVectors(pev->angles);
			// diffusion - regular grenade but it explodes upon contact
			CBaseEntity *Gren = CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5 );
			if( Gren )
			{
				SET_MODEL( Gren->edict(), "models/grenade_alien.mdl" );
				Gren->SetTouch( &CGrenade::ExplodeTouch );
				// Tumble in air
				Vector avelocity( RANDOM_FLOAT( 100, 500 ), 0, 0 );
				Gren->SetLocalAvelocity( avelocity );
				EMIT_SOUND( ENT( Gren->pev ), CHAN_WEAPON, "hgrunt/ag_nade_beep.wav", 1, ATTN_NORM );
			}

			EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/soldier_throw.wav", 1, ATTN_NORM );

			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(5,10);
		}
		break;

		case HGRUNT_AE_GREN_LAUNCH:
		{
			if (AlternateShoot == 1)
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, "comball/spawn.wav", 1, ATTN_NORM);
			else
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/alien_launcher.wav", 1, ATTN_NORM);

			Vector vecStart, angleGun;
			GetAttachment( 0, vecStart, angleGun );
			UTIL_MakeVectors ( GetAbsAngles() );

			Vector vecShootOrigin = GetGunPosition();// +gpGlobals->v_forward * 32;
			Vector vecShootDir = ShootAtEnemy( vecShootOrigin );
			angleGun = UTIL_VecToAngles(vecShootDir);

			if (AlternateShoot == 1)
			{
				CBaseMonster *pBall = (CBaseMonster*)Create( "shootball", vecStart, angleGun, edict() );
				if( pBall )
				{
					Vector AddVelocity = g_vecZero;
					if( m_hEnemy != NULL )
					{
						if( g_iSkillLevel == SKILL_HARD )
							AddVelocity = m_hEnemy->pev->velocity + m_hEnemy->pev->basevelocity;
						else if( g_iSkillLevel == SKILL_MEDIUM )
							AddVelocity = m_hEnemy->pev->velocity.Normalize() * 150 + m_hEnemy->pev->basevelocity;
					}
					pBall->SetAbsVelocity( (ShootAtEnemy( vecStart ) * 700) + AddVelocity );
					pBall->m_iClass = m_iClass;
				}
			}
			else
			{
				CBaseMonster *pBall = (CBaseMonster*)Create( "alien_super_ball", vecStart, angleGun, edict() );
				if( pBall )
				{
					pBall->SetAbsVelocity( ShootAtEnemy( vecStart ) * 100 );
					if( m_hEnemy != NULL )
						pBall->m_hEnemy = m_hEnemy;
				}
			}

			Vector angDir = UTIL_VecToAngles( vecShootDir );	
			SetBlending( 0, -angDir.x );

			m_fThrowGrenade = FALSE;
			if (g_iSkillLevel == SKILL_HARD)
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 8, 15 );// wait a random amount of time before shooting again
			else
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 12, 18 );
		}
		break;

		case HGRUNT_AE_GREN_DROP: // not used
		{
			
		}
		break;

		case HGRUNT_AE_BURST1:
		{
			if( pev->sequence == LookupSequence( "runshootmp5" ) || pev->sequence == LookupSequence( "runshootshotgun" ) )
			{
				if( !RunningShooting )
					break;
				if( !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
					break;
				if( m_hEnemy == NULL )
					break;
				// force client event
				MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, pev->origin );
				WRITE_BYTE( TE_CLIENTEVENT );
				WRITE_BYTE( 3 );
				WRITE_SHORT( entindex() );
				MESSAGE_END();
			}
			
			if ( HasWeapon( HGRUNT_9MMAR ))
				Shoot();
			else
				Shotgun( );
		
			CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 1024, 0.3, ENTINDEX(edict()) );
		}
		break;

		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
			if( pev->sequence == LookupSequence( "runshootmp5" ) || pev->sequence == LookupSequence( "runshootshotgun" ) )
			{
				if( !RunningShooting )
					break;
				if( !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
					break;
				if( m_hEnemy == NULL )
					break;
				// force client event
				MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, pev->origin );
				WRITE_BYTE( TE_CLIENTEVENT );
				WRITE_BYTE( 1 );
				WRITE_SHORT( entindex() );
				MESSAGE_END();
			}
			Shoot();
			break;

		case HGRUNT_AE_KICK:
		{
			CBaseEntity *pHurt = Kick();

			if ( pHurt )
			{
				switch( RANDOM_LONG( 0, 2 ) )
				{
					case 0: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch1.wav", 1, ATTN_NORM ); break;
					case 1: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch2.wav", 1, ATTN_NORM ); break;
					case 2: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch3.wav", 1, ATTN_NORM ); break;
				}
				UTIL_MakeVectors( GetAbsAngles() );
				pHurt->pev->punchangle.x = 15;
				pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50 );
				pHurt->TakeDamage( pev, pev, g_GruntKickDamage, DMG_CLUB );
			}
		}
		break;

		case HGRUNT_AE_CAUGHT_ENEMY:
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz(ENT(pev), "AG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
				 JustSpoke();
			}

		}

		default:
			CSquadMonster::HandleAnimEvent( pEvent );
			break;
	}
}

void CHGruntAlien :: Shoot ( void )
{
	if (m_hEnemy == NULL)
		return;

	Vector LightOrg = GetAbsOrigin() + Vector(0,0,40);

	if (AlternateShoot == 1) // shoot shock trooper attack
	{
		Vector vecStart, angleGun;			
		GetAttachment( 0, vecStart, angleGun );
		UTIL_MakeVectors ( GetAbsAngles() );

		CSprite *pMuzzleFlash = CSprite::SpriteCreate( "sprites/muzzle_shock.spr", GetAbsOrigin(), TRUE );
		if( pMuzzleFlash )
		{
			pMuzzleFlash->SetAttachment( edict(), 1 );
			pMuzzleFlash->pev->scale = 0.5;
			pMuzzleFlash->pev->rendermode = kRenderTransAdd;
			pMuzzleFlash->pev->renderamt = 255;
			pMuzzleFlash->AnimateAndDie( 25 );
		}

		Vector vecShootOrigin = GetGunPosition();// +gpGlobals->v_forward * 32;
		Vector vecShootDir = ShootAtEnemy( vecShootOrigin );
		angleGun = UTIL_VecToAngles(vecShootDir);

		CBaseEntity *pShock = CBaseEntity::Create("shock_beam", vecStart, angleGun, edict());
		if( pShock )
		{
			Vector AddVelocity = g_vecZero;
			if( m_hEnemy != NULL )
			{
				if( g_iSkillLevel == SKILL_HARD )
				{
					AddVelocity = m_hEnemy->pev->velocity;
					if( !RunningShooting )
						AddVelocity += m_hEnemy->pev->basevelocity;
				}
				else if( g_iSkillLevel == SKILL_MEDIUM )
				{
					AddVelocity = m_hEnemy->pev->velocity.Normalize() * 150;
					if( !RunningShooting )
						AddVelocity += m_hEnemy->pev->basevelocity;
				}
			}
			pShock->SetAbsVelocity( (ShootAtEnemy( vecStart ) * 2500) + AddVelocity );
			pShock->SetNextThink( 0 );
			pShock->pev->dmg = 1.25f;
		}

		m_cAmmoLoaded--;
		
		Vector angDir = UTIL_VecToAngles( vecShootDir );
		SetBlending( 0, -angDir.x );
	
		// Play fire sound.
		// DISTANT SOUNDS !!!!!
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/ar2_shoot.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110));
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, GetAbsOrigin(), 1024, 0.3, ENTINDEX(edict()) );

		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, LightOrg );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( LightOrg.x );		// origin
			WRITE_COORD( LightOrg.y );
			WRITE_COORD( LightOrg.z );
			WRITE_BYTE( 30 );	// radius
			WRITE_BYTE( 0 );	// R
			WRITE_BYTE( 255 );	// G
			WRITE_BYTE( 255 );	// B
			WRITE_BYTE( 0 );	// life * 10
			WRITE_BYTE( 0 ); // decay
			WRITE_BYTE( 100 ); // brightness
			WRITE_BYTE( 0 ); // shadows
		MESSAGE_END();
	}
	else // shoot controller balls
	{
		switch(RANDOM_LONG( 0, 1 ))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/alien_hks1.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110) );
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/alien_hks2.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110) );
			break;
		}
	
		Vector vecStart, angleGun;
		GetAttachment( 0, vecStart, angleGun );
		UTIL_MakeVectors ( GetAbsAngles() );

		Vector vecShootOrigin = GetGunPosition();// +gpGlobals->v_forward * 32;
		Vector vecShootDir = ShootAtEnemy( vecShootOrigin );
		angleGun = UTIL_VecToAngles(vecShootDir);

		CBaseMonster *pBall = (CBaseMonster*)Create( "alien_energy_ball", vecStart, angleGun, edict() );
		if( pBall )
		{
			pBall->pev->scale = 1;
			Vector AddVelocity = g_vecZero;
			if( m_hEnemy != NULL )
			{
				if( g_iSkillLevel == SKILL_HARD )
					AddVelocity = m_hEnemy->pev->velocity + m_hEnemy->pev->basevelocity;
				else if( g_iSkillLevel == SKILL_MEDIUM )
					AddVelocity = m_hEnemy->pev->velocity.Normalize() * 150 + m_hEnemy->pev->basevelocity;
			}
			pBall->SetAbsVelocity( (ShootAtEnemy( vecStart ) * 4000) + AddVelocity );
			pBall->pev->dmg = 1.0f;
		}
		
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, LightOrg );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( LightOrg.x );		// origin
			WRITE_COORD( LightOrg.y );
			WRITE_COORD( LightOrg.z );
			WRITE_BYTE( 30 );	// radius
			WRITE_BYTE( 255 );	// R
			WRITE_BYTE( 255 );	// G
			WRITE_BYTE( 255 );	// B
			WRITE_BYTE( 0 );	// life * 10
			WRITE_BYTE( 0 ); // decay
			WRITE_BYTE( 100 ); // brightness
			WRITE_BYTE( 0 ); // shadows
		MESSAGE_END();
	
		m_cAmmoLoaded--;// take away a bullet!

		Vector angDir = UTIL_VecToAngles( vecShootDir );
		SetBlending( 0, -angDir.x );
	}
}

void CHGruntAlien :: Shotgun ( void )
{
	if (m_hEnemy == NULL)
		return;

	switch(RANDOM_LONG( 0, 3 ))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/alien_shotgun1.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110) );
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/alien_shotgun2.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110) );
		break;
	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/alien_shotgun3.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110) );
		break;
	case 3:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/alien_shotgun4.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110) );
		break;
	}

	Vector vecStart, angleGun;
	GetAttachment( 0, vecStart, angleGun );
	UTIL_MakeVectors ( GetAbsAngles() );

	Vector LightOrg = GetAbsOrigin() + Vector( 0, 0, 40 );
	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, LightOrg );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( LightOrg.x );		// origin
		WRITE_COORD( LightOrg.y );
		WRITE_COORD( LightOrg.z );
		WRITE_BYTE( 30 );	// radius
		WRITE_BYTE( 225 );	// R
		WRITE_BYTE( 0 );	// G
		WRITE_BYTE( 60 );	// B
		WRITE_BYTE( 0 );	// life * 10
		WRITE_BYTE( 0 ); // decay
		WRITE_BYTE( 100 ); // brightness
		WRITE_BYTE( 0 ); // shadows
	MESSAGE_END();

	Vector vecShootOrigin = GetGunPosition();// +gpGlobals->v_forward * 32;
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );
	angleGun = UTIL_VecToAngles(vecShootDir);

	CBaseMonster *pBall = (CBaseMonster*)Create( "alien_shotgun_ball", vecStart, angleGun, edict() );
	if( pBall )
	{
		pBall->pev->rendercolor.x = 225;
		pBall->pev->rendercolor.y = 0;
		pBall->pev->rendercolor.z = 60;
		Vector AddVelocity = g_vecZero;
		if( m_hEnemy != NULL )
		{
			if( g_iSkillLevel == SKILL_HARD )
				AddVelocity = m_hEnemy->pev->velocity + m_hEnemy->pev->basevelocity;
			else if( g_iSkillLevel == SKILL_MEDIUM )
				AddVelocity = m_hEnemy->pev->velocity.Normalize() * 150 + m_hEnemy->pev->basevelocity;
		}
		pBall->SetAbsVelocity( (ShootAtEnemy( vecStart ) * 3000) + AddVelocity );
		pBall->m_hEnemy = m_hEnemy;
	}
	
	m_cAmmoLoaded--;// take away a bullet!
	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );
}


// =================================================================================================================
// ======================================= Diffusion Security Grunt General ========================================
// =================================================================================================================

class CHGruntSecurityGeneral : public CHGrunt
{
public:
	void Spawn(void);
	void Precache(void);
	int Classify(void);
	void Shoot(void);
	void SetYawSpeed(void);
	void SetActivity(Activity NewActivity);
	int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	int IRelationship(CBaseEntity* pTarget);
	void HandleAnimEvent(MonsterEvent_t* pEvent);
	Schedule_t* GetSchedule(void);
	Schedule_t* GetScheduleOfType(int Type);
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckRangeAttack2Impl( float grenadeSpeed, float flDot, float flDist );

	bool bUseGrenLauncher;
};

LINK_ENTITY_TO_CLASS(monster_security_general, CHGruntSecurityGeneral);

void CHGruntSecurityGeneral::SetYawSpeed(void)
{
	int ys;

	switch (m_Activity)
	{
	case ACT_IDLE:
		ys = 200;
		break;
	case ACT_RUN:
		ys = 180;
		break;
	case ACT_WALK:
		ys = 200;
		break;
	case ACT_RANGE_ATTACK1:
		ys = 120;
		break;
	case ACT_RANGE_ATTACK2:
		ys = 120;
		break;
	case ACT_MELEE_ATTACK1:
		ys = 120;
		break;
	case ACT_MELEE_ATTACK2:
		ys = 120;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 220;
		break;
	case ACT_GLIDE:
	case ACT_FLY:
		ys = 30;
		break;
	default:
		ys = 120;
		break;
	}

	pev->yaw_speed = ys;
}

void CHGruntSecurityGeneral::Spawn()
{
	Precache();

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/hgrunt_securityg.mdl");
	UTIL_SetSize(pev, Vector(-18.4, -18.4, 0), Vector(18.4, 18.4, 82.8));  // diffusion - scaled x1.15

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->effects = 0;
	if( !pev->health ) pev->health = g_SecurityGeneralHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	m_flFieldOfView = -1;   // 360
	m_MonsterState = MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime = gpGlobals->time;
	m_iSentence = HGRUNT_SENT_NONE;
	bUseGrenLauncher = false;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded = FALSE;
	m_fFirstEncounter = TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector(0, 0, 63.25);  //55

	// diffusion - the general is using only these weapons
	if (!m_bHaveWeapons)
	{
		AddWeapon( HGRUNT_9MMAR );
		AddWeapon( HGRUNT_HANDGRENADE );
		AddWeapon( HGRUNT_GRENADELAUNCHER );
	}

	m_cClipSize = GRUNT_CLIP_SIZE * 2;

	m_cAmmoLoaded = m_cClipSize;

	if (RANDOM_LONG(0, 99) < 80)
		pev->skin = 0;	// light skin
	else
		pev->skin = 1;	// dark skin

	SetBodygroup(HEAD_GROUP, HEAD_M203);

	CTalkMonster::g_talkWaitTime = 0;

	CanSpawnDrone = true;
	DroneSpawned = false; // when true, grunt can't spawn any more drones

	CombatWaitTime = g_SecurityGeneralCombatWaitTime[g_iSkillLevel];

	SetFlag(F_MONSTER_CANT_LOSE_ENEMY);

	MonsterInit();

}

void CHGruntSecurityGeneral::Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/hgrunt_securityg.mdl");

	PRECACHE_SOUND( "hgrunt/general_mgun.wav" );
	PRECACHE_SOUND( "hgrunt/general_mgun_d.wav" );

	PRECACHE_SOUND("hgrunt/hg_die1.wav");
	PRECACHE_SOUND("hgrunt/hg_die2.wav");
	PRECACHE_SOUND("hgrunt/hg_die3.wav");

	PRECACHE_SOUND("hgrunt/gr_pain1.wav");
	PRECACHE_SOUND("hgrunt/gr_pain2.wav");
	PRECACHE_SOUND("hgrunt/gr_pain3.wav");
	PRECACHE_SOUND("hgrunt/gr_pain4.wav");
	PRECACHE_SOUND("hgrunt/gr_pain5.wav");

	PRECACHE_SOUND("hgrunt/gr_reload_mp5.wav");

	PRECACHE_SOUND("weapons/glauncher.wav");

	PRECACHE_SOUND( "weapons/shotgun_npc.wav" );
	PRECACHE_SOUND( "weapons/shotgun_npc_d.wav" );

	PRECACHE_SOUND("zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event

	UTIL_PrecacheOther("monster_security_drone");

	// get voice pitch
	if( !m_voicePitch ) m_voicePitch = 93; // a bit lower
}

int	CHGruntSecurityGeneral::Classify(void)
{
	return m_iClass ? m_iClass : 14; // Faction A
}

BOOL CHGruntSecurityGeneral::CheckRangeAttack2( float flDot, float flDist )
{
	return CheckRangeAttack2Impl( g_GruntGrenadeSpeed, flDot, flDist );
}

BOOL CHGruntSecurityGeneral::CheckRangeAttack2Impl( float grenadeSpeed, float flDot, float flDist )
{
	if( !HasWeapon( HGRUNT_HANDGRENADE ) && !HasWeapon( HGRUNT_GRENADELAUNCHER ) )
	{
		return FALSE;
	}

	// if the grunt isn't moving, it's ok to check.
	if( m_flGroundSpeed != 0 )
	{
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	// assume things haven't changed too much since last time
	if( gpGlobals->time < m_flNextGrenadeCheck )
	{
		return m_fThrowGrenade;
	}

	if( (!FBitSet( m_hEnemy->pev->flags, FL_ONGROUND )) && (m_hEnemy->pev->waterlevel == 0) && (m_vecEnemyLKP.z > pev->absmax.z) && (!HasWeapon( HGRUNT_GRENADELAUNCHER )) )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		// diffusion - allow grenade launcher attack
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	Vector vecTarget;

	if( HasWeapon( HGRUNT_HANDGRENADE ) && !bUseGrenLauncher )
	{
		// find feet
		if( RANDOM_LONG( 0, 1 ) || (m_vecEnemyLKP == g_vecZero) )
		{
			// magically know where they are
			vecTarget = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );
		}
		else
		{
			// toss it to where you last saw them
			vecTarget = m_vecEnemyLKP;
		}
		// vecTarget = m_vecEnemyLKP + (m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin);
		// estimate position
		// vecTarget = vecTarget + m_hEnemy->pev->velocity * 2;
	}
	else if( HasWeapon( HGRUNT_GRENADELAUNCHER ) && bUseGrenLauncher )
	{
		// find target
		// vecTarget = m_hEnemy->BodyTarget( pev->origin );
		vecTarget = m_vecEnemyLKP + (m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin);
		// estimate position
		if( HasConditions( bits_COND_SEE_ENEMY ) )
			vecTarget = vecTarget + ((vecTarget - pev->origin).Length() / (float)g_GruntGrenadeSpeed ) * m_hEnemy->pev->velocity;
	}
	else
		return FALSE;

	// are any of my squad members near the intended grenade impact area?
	if( InSquad() )
	{
		if( SquadMemberInRange( vecTarget, 256 ) )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
			m_fThrowGrenade = FALSE;
		}
	}

	if( (vecTarget - pev->origin).Length2D() <= 256 )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	if( HasWeapon( HGRUNT_HANDGRENADE ) && !bUseGrenLauncher )
	{
		Vector vecToss;
		if( CanSpawnDrone && !DroneSpawned )
		{
			// for drone spawn just use a default forward vector, we don't perform a toss, just looking in the enemy direction
			UTIL_MakeVectors( GetAbsAngles() );
			vecToss = gpGlobals->v_forward;
		}
		else
			vecToss = VecCheckToss( pev, GetGunPosition(), vecTarget, 0.6f );

		if( vecToss != g_vecZero )
		{
			if( CanSpawnDrone && !DroneSpawned )
			{
				m_vecTossVelocity = vecToss;
				// throw a drone
				m_fThrowGrenade = TRUE;
				// don't check again for a while.
				m_flNextGrenadeCheck = gpGlobals->time; // 1/3 second.
			}
			else
			{
				if( flCheckThrowDistance( pev, GetGunPosition(), vecToss ) < 300.0f )
				{
					// don't throw
					m_fThrowGrenade = FALSE;
					// don't check again for a while.
					m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
				}
				else
				{
					m_vecTossVelocity = vecToss;
					// throw a hand grenade
					m_fThrowGrenade = TRUE;
					// don't check again for a while.
					m_flNextGrenadeCheck = gpGlobals->time; // 1/3 second.
				}
			}
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}
	else if( HasWeapon( HGRUNT_GRENADELAUNCHER ) && bUseGrenLauncher ) // grenade launcher
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition(), vecTarget, g_GruntGrenadeSpeed, 0.5 );

		if( vecToss != g_vecZero )
		{
			float check = flCheckThrowDistance( pev, GetGunPosition(), vecToss );
			if( check < 300.0f )
			{
				// don't throw
				m_fThrowGrenade = FALSE;
				// don't check again for a while.
				m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
			}
			else
			{
				m_vecTossVelocity = vecToss;

				// throw a hand grenade
				m_fThrowGrenade = TRUE;
				// don't check again for a while.
				m_flNextGrenadeCheck = gpGlobals->time + 0.3; // 1/3 second.
			}
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}
	else
		return FALSE;

	return m_fThrowGrenade;
}

void CHGruntSecurityGeneral::SetActivity(Activity NewActivity)
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void* pmodel = GET_MODEL_PTR(ENT(pev));

	switch (NewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// grunt is either shooting standing or shooting crouched
		if (HasWeapon(HGRUNT_9MMAR))
		{
			if (m_fStanding)
			{
				// get aimable sequence
				iSequence = LookupSequence("standing_mp5");
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence("crouching_mp5");
			}
		}
		else
		{
			if (m_fStanding)
			{
				// get aimable sequence
				iSequence = LookupSequence("standing_shotgun");
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence("crouching_shotgun");
			}
		}
		break;
	case ACT_RANGE_ATTACK2:
		if( bUseGrenLauncher )
			iSequence = LookupSequence( "launchgrenade" );
		else
			iSequence = LookupSequence( "throwgrenade" );
		break;
	case ACT_RUN:
		iSequence = LookupActivity(NewActivity);
		break;
	case ACT_WALK:
		iSequence = LookupActivity(NewActivity);
		break;
	case ACT_IDLE:
		if (m_MonsterState == MONSTERSTATE_COMBAT)
		{
			NewActivity = ACT_IDLE_ANGRY;
		}
		iSequence = LookupActivity(NewActivity);
		break;
	default:
		iSequence = LookupActivity(NewActivity);
		break;
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if (iSequence > ACTIVITY_NOT_AVAILABLE)
	{
		if (pev->sequence != iSequence || !m_fSequenceLoops)
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT(at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity);
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

void CHGruntSecurityGeneral::Shoot(void)
{
	if (m_hEnemy == NULL)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy(vecShootOrigin);

	UTIL_MakeVectors(GetAbsAngles());

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40, 90) + gpGlobals->v_up * RANDOM_FLOAT(75, 200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass(vecShootOrigin - vecShootDir * 10, vecShellVelocity, GetAbsAngles().y, SHELL_556, TE_BOUNCE_SHELL);
	FireBullets(1, vecShootOrigin, vecShootDir, RunningShooting ? VECTOR_CONE_10DEGREES : VECTOR_CONE_3DEGREES, 4096, BULLET_MONSTER_MP5, 1, 7); // 7 dmg per bullet

	pev->effects |= EF_MUZZLEFLASH;

	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles(vecShootDir);
	SetBlending( 0, -angDir.x );
}

int CHGruntSecurityGeneral::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if( HasSpawnFlags(SF_MONSTER_NODAMAGE) )
		return 0;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return 0;

	Forget(bits_MEMORY_INCOVER);
	return CSquadMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

int CHGruntSecurityGeneral::IRelationship(CBaseEntity* pTarget)
{
	return CSquadMonster::IRelationship(pTarget);
}

void CHGruntSecurityGeneral::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch (pEvent->event)
	{
	case HGRUNT_AE_DROP_GUN:          // diffusion - only when the flag is set, grunts drop guns
	{
		if( HasSpawnFlags(SF_MONSTER_NO_WPN_DROP) && GetBodygroup( GUN_GROUP ) != GUN_NONE )
		{
			Vector	vecGunPos;
			Vector	vecGunAngles;

			GetAttachment(0, vecGunPos, vecGunAngles);

			// switch to body group with no gun.
			SetBodygroup(GUN_GROUP, GUN_NONE);

			// now spawn a gun.
			if (HasWeapon(HGRUNT_SHOTGUN))
				DropItem("weapon_shotgun", vecGunPos, vecGunAngles);
			else
				DropItem("weapon_9mmAR", vecGunPos, vecGunAngles);

			if (HasWeapon(HGRUNT_GRENADELAUNCHER))
				DropItem("ammo_ARgrenades", BodyTarget(GetAbsOrigin()), vecGunAngles);
		}
	}
	break;

	case HGRUNT_AE_RELOAD:
		if (HasWeapon(HGRUNT_SHOTGUN))
		{
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload_shotgun.wav", 1, ATTN_NORM);
		}
		else
		{
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload_mp5.wav", 1, ATTN_NORM);
		}
		m_cAmmoLoaded = m_cClipSize;
		ClearConditions(bits_COND_NO_AMMO_LOADED);
		break;

	case HGRUNT_AE_GREN_TOSS:
	{
		if( !DroneSpawned ) // I didn't spawn any of them yet, so check if I'm able to
		{
			TraceResult tracer;
			Vector vecStart, vecEnd;
			vecStart = vecEnd = GetAbsOrigin();
			// drone spawns at 120. check up to 300, and 180 is acceptable
			vecEnd.z += 500;
			UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, dont_ignore_glass, edict(), &tracer);
			if( (vecStart - tracer.vecEndPos).Length() < 180 ) // not enough space. throw a grenade instead
			{
				UTIL_MakeVectors(GetAbsAngles());
				CBaseEntity *pGrenade = CGrenade::ShootTimed(pev, GetGunPosition(), m_vecTossVelocity, 3.5, ForceEMPGrenade );
				if( pGrenade )
				{
					SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
					pGrenade->pev->body = 1; // change to body without pin
				}
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/soldier_throw.wav", 1, ATTN_NORM);
				m_fThrowGrenade = FALSE;
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(6, 12);// wait few seconds for the next try
			}
			else // spawn a drone
			{
				Vector vecStart = GetAbsOrigin();
				vecStart.z += 120;  // drone spawns right above grunt's head, there's a possibility that it could stuck though...
				CBaseMonster* pDrone = (CBaseMonster*)Create("monster_security_drone", vecStart, GetAbsAngles(), edict());
				if( pDrone )
				{
					pDrone->m_iClass = m_iClass; // do not shoot at the owner!!!
					// make the drone aware of the enemy which grunt was angry at
					// even if the drone didn't see him at the moment of spawn.
					if( m_hEnemy != NULL )
					{
						pDrone->SetEnemy( m_hEnemy );
						pDrone->SetConditions( bits_COND_NEW_ENEMY );
						pDrone->m_IdealMonsterState = m_IdealMonsterState; // do not strip the enemy after spawn
					}

					// copy these parameters too to act similarly
					pDrone->m_flDistLook = m_flDistLook;
					pDrone->m_flDistTooClose = m_flDistTooClose;
					pDrone->m_flDistTooFar = m_flDistTooFar;
					pDrone->pev->health *= 2.0f; // the general's drone has twice more hp

					// add targetname too
					if( GetTargetname()[0] != '\0' )
					{
						char newname[64];
						sprintf_s( newname, "%s_drone", GetTargetname() );
						pDrone->pev->targetname = MAKE_STRING( newname );
					}
				}
				DroneSpawned = true; // the drone is now spawned, grunt can't spawn any more drones and will use the grenade
				m_fThrowGrenade = FALSE;
				m_flNextGrenadeCheck = gpGlobals->time + 3;
			}
		}
		else
		{
			UTIL_MakeVectors(GetAbsAngles());
			CBaseEntity *pGrenade = CGrenade::ShootTimed(pev, GetGunPosition(), m_vecTossVelocity, 3.5, ForceEMPGrenade );
			if( pGrenade )
			{
				SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
				pGrenade->pev->body = 1; // change to body without pin
			}
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/soldier_throw.wav", 1, ATTN_NORM);
			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(6, 12);// wait few seconds
		}
	}
	break;

	case HGRUNT_AE_GREN_LAUNCH:
	{
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM);
		CGrenade::ShootContact(pev, GetGunPosition(), m_vecTossVelocity);
		m_fThrowGrenade = FALSE;
		if (g_iSkillLevel == SKILL_HARD)
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(2, 4);// wait a random amount of time before shooting again
		else
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT(3, 5);;// wait 3-5 seconds before even looking again to see if a grenade can be thrown.
	}
	break;

	case HGRUNT_AE_GREN_DROP:
	{
		UTIL_MakeVectors(GetAbsAngles());
		CGrenade::ShootTimed(pev, GetAbsOrigin() + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3);
	}
	break;

	case HGRUNT_AE_BURST1:
	{
		Vector org = GetAbsOrigin();
		org.z += 40;
		if (HasWeapon(HGRUNT_9MMAR))
		{
			Shoot();
			PlayClientSound( this, 245, 0, (m_cAmmoLoaded <= 15 ? m_cAmmoLoaded : 0), org );
		}
		else
		{
			Shotgun();
		//	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1, ATTN_NORM);
			PlayClientSound( this, 254, 0, 0, org );
		}

		CSoundEnt::InsertSound(bits_SOUND_COMBAT, GetAbsOrigin(), 1024, 0.3, ENTINDEX(edict()) );
	}
	break;

	case HGRUNT_AE_BURST2:
	case HGRUNT_AE_BURST3:
		Shoot();
		break;

	case HGRUNT_AE_KICK:
	{
		CBaseEntity* pHurt = Kick();

		if (pHurt)
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch1.wav", 1, ATTN_NORM ); break;
			case 1: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch2.wav", 1, ATTN_NORM ); break;
			case 2: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch3.wav", 1, ATTN_NORM ); break;
			}
			UTIL_MakeVectors(GetAbsAngles());
			pHurt->pev->punchangle.x = 15;
			pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50);
			pHurt->TakeDamage(pev, pev, g_GruntKickDamage, DMG_CLUB);
		}
	}
	break;

	case HGRUNT_AE_CAUGHT_ENEMY:
	{
		if (FOkToSpeak())
		{
			SENTENCEG_PlayRndSz(ENT(pev), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
			JustSpoke();
		}

	}

	default:
		CSquadMonster::HandleAnimEvent(pEvent);
		break;
	}
}

Schedule_t* CHGruntSecurityGeneral::GetSchedule(void)
{
	if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE) && !HasConditions(bits_COND_SEE_ENEMY))
	{
		// something hurt me and I can't see anyone. Better relocate
		return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);
	}

	// clear old sentence
	m_iSentence = HGRUNT_SENT_NONE;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling. 
	if (pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE)
	{
		if (pev->flags & FL_ONGROUND)
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType(SCHED_GRUNT_REPEL_LAND);
		}
		else
		{
			// repel down a rope, 
			if (m_MonsterState == MONSTERSTATE_COMBAT)
				return GetScheduleOfType(SCHED_GRUNT_REPEL_ATTACK);
			else
				return GetScheduleOfType(SCHED_GRUNT_REPEL);
		}
	}

	// grunts place HIGH priority on running away from danger sounds.
	if (HasConditions(bits_COND_HEAR_SOUND))
	{
		CSound* pSound;
		pSound = PBestSound();

		ASSERT(pSound != NULL);
		if (pSound)
		{
			/*	if (pSound->m_iType & bits_SOUND_DANGER)
				{
					// dangerous sound nearby!

					//!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
					// and the grunt should find cover from the blast
					// good place for "SHIT!" or some other colorful verbal indicator of dismay.
					// It's not safe to play a verbal order here "Scatter", etc cause
					// this may only affect a single individual in a squad.

					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( ENT(pev), "HG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
				} */
			if (!HasConditions(bits_COND_SEE_ENEMY) && (pSound->m_iType & (bits_SOUND_PLAYER | bits_SOUND_COMBAT)))
			{
				MakeIdealYaw(pSound->m_vecOrigin);
			}
		}
	}
	switch (m_MonsterState)
	{
	case MONSTERSTATE_COMBAT:
	{
		// dead enemy
		if (HasConditions(bits_COND_ENEMY_DEAD | bits_COND_ENEMY_LOST))
		{
			// call base class, all code to handle dead enemies is centralized there.
			return CBaseMonster::GetSchedule();
		}

		// new enemy
		if( HasConditions( bits_COND_NEW_ENEMY ) )
		{
			if( !g_bGruntAlert && (m_hEnemy != NULL) )// && RANDOM_LONG(0,1))
			{
				if( m_hEnemy->IsPlayer() )
				{
					if( HasConditions( bits_COND_SEE_FLASHLIGHT ) )
						SENTENCEG_PlayRndSz( ENT( pev ), "HG_FLASHLIGHT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
					else
						SENTENCEG_PlayRndSz( ENT( pev ), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
				}
				else if(
					(m_hEnemy->Classify() != CLASS_PLAYER_ALLY) &&
					(m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE) &&
					(m_hEnemy->Classify() != CLASS_MACHINE)
					)
					// monster
					SENTENCEG_PlayRndSz( ENT( pev ), "HG_MONST", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );

				JustSpoke();
				g_bGruntAlert = true;
			}

			if( (OccupySlot( bits_SLOTS_HGRUNT_GRENADE )) && !DroneSpawned ) // HasConditions ( bits_COND_CAN_RANGE_ATTACK2 )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 ); // spawn a drone

			if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_GRUNT_SUPPRESS );
			else
				return CBaseMonster::GetSchedule(); //return GetScheduleOfType ( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
		}
		// no ammo
		else if( HasConditions( bits_COND_NO_AMMO_LOADED ) )
		{
			//!!!KELLY - this individual just realized he's out of bullet ammo. 
			// He's going to try to find cover to run to and reload, but rarely, if 
			// none is available, he'll drop and reload in the open here. 
			return GetScheduleOfType( SCHED_GRUNT_COVER_AND_RELOAD );
		}

		// damaged just a little
		else if( HasConditions( bits_COND_LIGHT_DAMAGE ) )
		{
			if( pev->health < pev->max_health * 0.1f ) // he's tired and almost dead. run away or try to fire back
			{
				switch( RANDOM_LONG( 0, 1 ) )
				{
				case 0: return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY ); break;
				case 1: return GetScheduleOfType( SCHED_RANGE_ATTACK1 ); break;
				}
			}
			else if( pev->health < pev->max_health * 0.25f ) // the general now has 1/4 of his HP. 30% chance of flinch
			{
				switch( RANDOM_LONG( 0, 2 ) )
				{
				case 0: return GetScheduleOfType( SCHED_SMALL_FLINCH ); break;
				case 1: return GetScheduleOfType( SCHED_RANGE_ATTACK1 ); break;
				case 2: return GetScheduleOfType( SCHED_GRUNT_SUPPRESS ); break;
				}
			}
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}
		// can kick
		else if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
		{
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
		}
		// can grenade launch

		else if( CanSpawnDrone && DroneSpawned && HasWeapon( HGRUNT_GRENADELAUNCHER ) && RANDOM_LONG(0,100) > 60 && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
		{
			// shoot a grenade if you can
			bUseGrenLauncher = true;
			return GetScheduleOfType(SCHED_RANGE_ATTACK2);
		}
		// can shoot
		else if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
		{
			if (InSquad())
			{
				// if the enemy has eluded the squad and a squad member has just located the enemy
				// and the enemy does not see the squad member, issue a call to the squad to waste a 
				// little time and give the player a chance to turn.
				if (MySquadLeader()->m_fEnemyEluded && !HasConditions(bits_COND_ENEMY_FACING_ME))
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;
					return GetScheduleOfType(SCHED_GRUNT_FOUND_ENEMY);
				}
			}

			if (OccupySlot(bits_SLOTS_HGRUNT_ENGAGE))
			{
				// try to take an available ENGAGE slot
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);
			}
			else
			{
				// instead of hiding, fire!
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);
			}
		}
		// can't see enemy
		else if (HasConditions(bits_COND_ENEMY_OCCLUDED))
		{
		if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && CanSpawnDrone && DroneSpawned && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) ) // diffusion - undone sentences
			{
				//!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
				if (FOkToSpeak())
				{
					SENTENCEG_PlayRndSz(ENT(pev), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					JustSpoke();
				}
				bUseGrenLauncher = false;
				return GetScheduleOfType(SCHED_RANGE_ATTACK2);
			}
			else if (OccupySlot(bits_SLOTS_HGRUNT_ENGAGE))
			{
				if (FOkToSpeak())// && RANDOM_LONG(0,1))
				{
					//SENTENCEG_PlayRndSz( ENT(pev), "HG_CHARGE", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					m_iSentence = HGRUNT_SENT_CHARGE;
					//JustSpoke();
				}
				return GetScheduleOfType(SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE);
			}
			else
			{
				//!!!KELLY - grunt is going to stay put for a couple seconds to see if
				// the enemy wanders back out into the open, or approaches the
				// grunt's covered position. Good place for a taunt, I guess?
				if (FOkToSpeak() && RANDOM_LONG(0, 1))
				{
					SENTENCEG_PlayRndSz(ENT(pev), "HG_TAUNT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					JustSpoke();
				}
				return GetScheduleOfType(SCHED_STANDOFF);
			}
		}

		if (HasConditions(bits_COND_SEE_ENEMY) && !HasConditions(bits_COND_CAN_RANGE_ATTACK1))
		{
			return GetScheduleOfType(SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE);
		}
	}
	}

	// no special cases here, call the base class
	return CSquadMonster::GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t* CHGruntSecurityGeneral::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
	{
		if (InSquad())
		{
			if (HasConditions(bits_COND_CAN_RANGE_ATTACK2) && OccupySlot(bits_SLOTS_HGRUNT_GRENADE)) // && g_iSkillLevel == SKILL_HARD
			{
				if (FOkToSpeak())
				{
					SENTENCEG_PlayRndSz(ENT(pev), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					JustSpoke();
				}
				return slGruntTossGrenadeCover;
			}
			else
			{
				return &slGruntTakeCover[0];
			}
		}
		else
		{
			//				if ( RANDOM_LONG(0,1) )
			//				{
			return &slGruntTakeCover[0];
			//				}
			//				else
			//				{
			//					return &slGruntGrenadeCover[ 0 ];
			//				}
		}
	}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
	{
		return &slGruntTakeCoverFromBestSound[0];
	}
	case SCHED_GRUNT_TAKECOVER_FAILED:
	{
		if (HasConditions(bits_COND_CAN_RANGE_ATTACK1) && OccupySlot(bits_SLOTS_HGRUNT_ENGAGE))
		{
			return GetScheduleOfType(SCHED_RANGE_ATTACK1);
		}

		return GetScheduleOfType(SCHED_FAIL);
	}
	break;
	case SCHED_GRUNT_ELOF_FAIL:
	{
		// human grunt is unable to move to a position that allows him to attack the enemy.
	//	if (FVisible(m_hEnemy))
	//	{
	//		if (pev->health < 150) // for general
	//			return GetScheduleOfType(SCHED_GRUNT_SUPPRESS); // diffusion - low hp, nervous
	//		else
	//			return GetScheduleOfType(SCHED_RANGE_ATTACK1); // or just try to attack the enemy from the current position
	//	}
	//	else
		return GetScheduleOfType( SCHED_MOVE_TO_ENEMY_LKPRUN ); // can't see enemy. try to reposition
	}
	break;
	case SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE:
	{
		return &slGruntEstablishLineOfFire[0];
	}
	break;
	case SCHED_RANGE_ATTACK1:
	{
		AttackStartTime = gpGlobals->time + CombatWaitTime;
		if (m_fStanding)
			return &slGruntRangeAttack1B[0];
		else
			return &slGruntRangeAttack1A[0];
	}
	case SCHED_RANGE_ATTACK2:
	{
		AttackStartTime = gpGlobals->time + CombatWaitTime;
		return &slGruntRangeAttack2[0];
	}
	case SCHED_COMBAT_FACE:
	{
		return &slGruntCombatFace[0];
	}
	case SCHED_GRUNT_WAIT_FACE_ENEMY:
	{
		return &slGruntWaitInCover[0];
	}
	case SCHED_GRUNT_SWEEP:
	{
		return &slGruntSweep[0];
	}
	case SCHED_GRUNT_COVER_AND_RELOAD:
	{
		return &slGruntHideReload[0];
	}
	case SCHED_GRUNT_FOUND_ENEMY:
	{
		return &slGruntFoundEnemy[0];
	}
	case SCHED_VICTORY_DANCE:
	{
		if (InSquad())
		{
			if (!IsLeader())
			{
				return &slGruntFail[0];
			}
		}

		return &slGruntVictoryDance[0];
	}
	case SCHED_GRUNT_SUPPRESS:
	{
		if (m_hEnemy->IsPlayer() && m_fFirstEncounter)
		{
			m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
			return &slGruntSignalSuppress[0];
		}
		else
		{
			return &slGruntSuppress[0];
		}
	}
	case SCHED_FAIL:
	{
		if (m_hEnemy != NULL)
		{
			// grunt has an enemy, so pick a different default fail schedule most likely to help recover.
			return &slGruntCombatFail[0];
		}

		return &slGruntFail[0];
	}
	case SCHED_GRUNT_REPEL:
	{
		Vector vecVelocity = GetAbsVelocity();
		if (vecVelocity.z > -128)
		{
			vecVelocity.z -= 32;
			SetAbsVelocity(vecVelocity);
		}
		return &slGruntRepel[0];
	}
	case SCHED_GRUNT_REPEL_ATTACK:
	{
		Vector vecVelocity = GetAbsVelocity();
		if (vecVelocity.z > -128)
		{
			vecVelocity.z -= 32;
			SetAbsVelocity(vecVelocity);
		}
		return &slGruntRepelAttack[0];
	}
	case SCHED_GRUNT_REPEL_LAND:
	{
		return &slGruntRepelLand[0];
	}
	default:
	{
		return CSquadMonster::GetScheduleOfType(Type);
	}
	}
}





// ======================================= Diffusion Security Grunt ================================================

class CHGruntSecurity : public CHGrunt
{
public:
	void Spawn(void);
	void Precache(void);
	int Classify(void);
	int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	int IRelationship(CBaseEntity* pTarget);
};

LINK_ENTITY_TO_CLASS(monster_security_soldier, CHGruntSecurity);

void CHGruntSecurity::Spawn()
{
	Precache();

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/hgrunt_security.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->effects = 0;
	if( !pev->health ) pev->health = g_SecurityGuyHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	m_flFieldOfView = 0; //180 deg // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime = gpGlobals->time;
	m_iSentence = HGRUNT_SENT_NONE;

	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded = FALSE;
	m_fFirstEncounter = TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector(0, 0, 55);

	CanSpawnDrone = false;

	if (FStrEq(STRING(wpns), "9mmar"))
	{
		AddWeapon(HGRUNT_9MMAR);
	}
	else if (FStrEq(STRING(wpns), "9mmar_hg"))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon( HGRUNT_HANDGRENADE );
	}
	else if (FStrEq(STRING(wpns), "9mmar_hg_drone"))
	{
		AddWeapon(HGRUNT_9MMAR);
		AddWeapon( HGRUNT_HANDGRENADE );
		if( !Silent )
			CanSpawnDrone = true;
	}
	else if (FStrEq(STRING(wpns), "9mmar_gl"))
	{
		AddWeapon(HGRUNT_9MMAR);
		if( !Silent )
			AddWeapon(HGRUNT_GRENADELAUNCHER);
	}
	else if (FStrEq(STRING(wpns), "shotgun"))
	{
		AddWeapon(HGRUNT_SHOTGUN);
	}
	else if (FStrEq(STRING(wpns), "shotgun_hg"))
	{
		AddWeapon(HGRUNT_SHOTGUN);
		AddWeapon( HGRUNT_HANDGRENADE );
	}
	else if (FStrEq(STRING(wpns), "shotgun_hg_drone"))
	{
		AddWeapon(HGRUNT_SHOTGUN);
		AddWeapon( HGRUNT_HANDGRENADE );
		if( !Silent )
			CanSpawnDrone = true;
	}
	else
	{
		// shuffle properly because it's still not exactly random it seems
		int rnd = RANDOM_LONG( 0, 3 );
		rnd = RANDOM_LONG( 0, 3 );
		rnd = RANDOM_LONG( 0, 3 );

		switch( rnd )
		{
		case 0:
			AddWeapon(HGRUNT_9MMAR);
			AddWeapon( HGRUNT_HANDGRENADE );
			if( !Silent )
			{
				switch( RANDOM_LONG( 0, 2 ) ) // diffusion - 60% chance that a grunt with handgrenade also has a drone
				{
				case 0:	CanSpawnDrone = true; break;
				case 1:	CanSpawnDrone = true; break;
				case 2:	CanSpawnDrone = false; break;
				}
			}
			break;
		case 1:
			AddWeapon(HGRUNT_SHOTGUN);
			break;
		case 2:
			AddWeapon(HGRUNT_9MMAR);
			if( !Silent )
				AddWeapon(HGRUNT_GRENADELAUNCHER);
			break;
		case 3:
			AddWeapon(HGRUNT_SHOTGUN);
			AddWeapon( HGRUNT_HANDGRENADE );
			if( !Silent )
			{
				switch( RANDOM_LONG( 0, 2 ) ) // diffusion - 60% chance that a grunt with handgrenade also has a drone
				{
				case 0:	CanSpawnDrone = true; break;
				case 1:	CanSpawnDrone = true; break;
				case 2:	CanSpawnDrone = false; break;
				}
			}
			break;
		}
	}

	if (HasWeapon(HGRUNT_SHOTGUN))
	{
		SetBodygroup(GUN_GROUP, GUN_SHOTGUN);
		m_cClipSize = 8;
	}
	else
	{
		m_cClipSize = GRUNT_CLIP_SIZE;
	}
	m_cAmmoLoaded = m_cClipSize;

	if (RANDOM_LONG(0, 99) < 80)
		pev->skin = 0;	// light skin
	else
		pev->skin = 1;	// dark skin

	if (HasWeapon(HGRUNT_SHOTGUN))
		SetBodygroup(HEAD_GROUP, HEAD_SHOTGUN);
	else if (HasWeapon(HGRUNT_GRENADELAUNCHER))
	{
		SetBodygroup(HEAD_GROUP, HEAD_M203);
		pev->skin = 1; // always dark skin
	}

	DroneSpawned = false;

	if (RANDOM_LONG(0, 2) == 0)
		CanInvestigate = 1;
	else
		CanInvestigate = 0;

	CTalkMonster::g_talkWaitTime = 0;

	CombatWaitTime = g_SecurityGruntCombatWaitTime[g_iSkillLevel];

	if( m_iFlashlightCap > 0 )
	{
		FlashlightCap = true;

		FlashlightSpr = CSprite::SpriteCreate( HGRUNT_FLASHLIGHT_SPR, GetAbsOrigin(), TRUE );
		if( FlashlightSpr )
		{
			FlashlightSpr->SetAttachment( edict(), 1 );
			FlashlightSpr->SetScale( 0.3 );
			FlashlightSpr->SetTransparency( kRenderTransAdd, 255, 255, 255, 200, 0 );
		}

		if( m_iFlashlightCap == 2 )
			pev->effects |= EF_MONSTERFLASHLIGHT; // start ON
	}

	if( Silent )
		ForceEMPGrenade = true;

	MonsterInit();
}

void CHGruntSecurity::Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/hgrunt_security.mdl");

	PRECACHE_SOUND("hgrunt/gr_mgun1.wav");
	PRECACHE_SOUND("hgrunt/gr_mgun2.wav");

	PRECACHE_SOUND("hgrunt/hg_die1.wav");
	PRECACHE_SOUND("hgrunt/hg_die2.wav");
	PRECACHE_SOUND("hgrunt/hg_die3.wav");

	PRECACHE_SOUND("hgrunt/gr_pain1.wav");
	PRECACHE_SOUND("hgrunt/gr_pain2.wav");
	PRECACHE_SOUND("hgrunt/gr_pain3.wav");
	PRECACHE_SOUND("hgrunt/gr_pain4.wav");
	PRECACHE_SOUND("hgrunt/gr_pain5.wav");

	PRECACHE_SOUND("hgrunt/gr_reload_mp5.wav");
	PRECACHE_SOUND("hgrunt/gr_reload_shotgun.wav");

	PRECACHE_SOUND("weapons/glauncher.wav");

	PRECACHE_SOUND( "weapons/shotgun_npc.wav" );
	PRECACHE_SOUND( "weapons/shotgun_npc_d.wav" );
	PRECACHE_SOUND("weapons/soldier_throw.wav"); // grenade toss sound

	PRECACHE_SOUND("zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event

	// get voice pitch
	if( !m_voicePitch ) m_voicePitch = 97 + RANDOM_LONG(0, 6);

	UTIL_PrecacheOther("monster_security_drone");
}

int	CHGruntSecurity::Classify(void)
{
	return m_iClass ? m_iClass : 14; // Faction A
}

int CHGruntSecurity::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if( HasSpawnFlags( SF_MONSTER_NODAMAGE ) )
		return 0;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return 0;

	Forget(bits_MEMORY_INCOVER);
	return CSquadMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

int CHGruntSecurity::IRelationship(CBaseEntity* pTarget)
{
	return CSquadMonster::IRelationship(pTarget);
}


















// =================================================================================================================
// ======================================= ANDREW BOSS FIGHT =======================================================
// =================================================================================================================

class CAndrewGrunt : public CHGrunt
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void Shoot( void );
	void SetYawSpeed( void );
	void SetActivity( Activity NewActivity );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	int IRelationship( CBaseEntity *pTarget );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void RunAI( void );
	void CollectPoints( void );
	void Andrew_Respawn( void );
	void Andrew_Escape( void );
	void WarpEffect( void );

	float AndrewLastHurt;
};

LINK_ENTITY_TO_CLASS( monster_andrew_grunt, CAndrewGrunt );

void CAndrewGrunt::SetYawSpeed( void )
{
	int ys;

	switch( m_Activity )
	{
	case ACT_IDLE:
		ys = 200;
		break;
	case ACT_RUN:
		ys = 180;
		break;
	case ACT_WALK:
		ys = 200;
		break;
	case ACT_RANGE_ATTACK1:
		ys = 120;
		break;
	case ACT_RANGE_ATTACK2:
		ys = 120;
		break;
	case ACT_MELEE_ATTACK1:
		ys = 120;
		break;
	case ACT_MELEE_ATTACK2:
		ys = 120;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 220;
		break;
	case ACT_GLIDE:
	case ACT_FLY:
		ys = 30;
		break;
	default:
		ys = 120;
		break;
	}

	pev->yaw_speed = ys;
}

void CAndrewGrunt::Spawn()
{
	Precache();

	SET_MODEL( ENT( pev ), ANDREW_GRUNT_MODEL );

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->effects = 0;

	if( !pev->health )
		pev->health = g_AndrewHealth[g_iSkillLevel];

	pev->max_health = pev->health;

	m_flFieldOfView = -1;   // 360
	m_MonsterState = MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime = gpGlobals->time;
	m_iSentence = HGRUNT_SENT_NONE;

	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded = FALSE;
	m_fFirstEncounter = TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector( 0, 0, 63.25 );  //55

	// using only these weapons
	if( !m_bHaveWeapons )
	{
		AddWeapon( HGRUNT_9MMAR );
		AddWeapon( HGRUNT_GRENADELAUNCHER );
	}

	m_cClipSize = GRUNT_CLIP_SIZE;

	m_cAmmoLoaded = m_cClipSize;

	SetBodygroup( HEAD_GROUP, HEAD_M203 );

	CTalkMonster::g_talkWaitTime = 0;

	CanSpawnDrone = false; // no drones
	AndrewEscapeTime = AndrewDashTime = gpGlobals->time;

	CombatWaitTime = g_AndrewCombatWaitTime[g_iSkillLevel];

	SetFlag( F_MONSTER_CANT_LOSE_ENEMY );

	pev->renderamt = 255;

	DifficultyScaler = 1.0f;
	ScaleDifficultyTime = 0;

	MonsterInit();
}

void CAndrewGrunt::Precache()
{
	PRECACHE_MODEL( ANDREW_GRUNT_MODEL );

	PRECACHE_SOUND( "hgrunt/gr_mgun1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_mgun2.wav" );

	PRECACHE_SOUND( "hgrunt/hg_die1.wav" );
	PRECACHE_SOUND( "hgrunt/hg_die2.wav" );
	PRECACHE_SOUND( "hgrunt/hg_die3.wav" );

	PRECACHE_SOUND( "hgrunt/gr_pain1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain2.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain3.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain4.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain5.wav" );

	PRECACHE_SOUND( "hgrunt/gr_reload_mp5.wav" );

	PRECACHE_SOUND( "weapons/glauncher.wav" );

	PRECACHE_SOUND( "weapons/shotgun_npc.wav" );
	PRECACHE_SOUND( "weapons/shotgun_npc_d.wav" );

	PRECACHE_SOUND( "zombie/claw_miss2.wav" );// because we use the basemonster SWIPE animation event

	PRECACHE_SOUND( "hgrunt/rich_warp.wav" );
	PRECACHE_SOUND( "hgrunt/rich_rocket.wav" );

	m_iTrail = PRECACHE_MODEL( "sprites/smoke.spr" );

	if( !m_voicePitch ) m_voicePitch = 100;
}

int	CAndrewGrunt::Classify( void )
{
	return m_iClass ? m_iClass : 14; // Faction A (security soldiers)
}

//==============================================================================
// CollectPoints: find and remember the escape point and all of the spawn points.
//==============================================================================
void CAndrewGrunt::CollectPoints(void)
{
	// find all respawn points.
	CBaseEntity *pAndrewRespawnPoint = NULL;
	RespawnPoints = 0;

	while( (pAndrewRespawnPoint = UTIL_FindEntityByClassname( pAndrewRespawnPoint, "info_andrew_spawn_point")) != NULL )
	{
		if( RespawnPoints > MAX_ANDREW_SPAWNS - 1 ) // fixed limit for now
		{
			ALERT( at_aiconsole, "^2Andrew Grunt:^7 found too many spawn points! Max. is %i.\n", MAX_ANDREW_SPAWNS );
			break;
		}

		// collect the coordinates and delete the dummy entity
		AndrewRespawnPoint[RespawnPoints] = pAndrewRespawnPoint->GetAbsOrigin();
		UTIL_Remove( pAndrewRespawnPoint );
		RespawnPoints++;
	}

	ALERT( at_aiconsole, "^2Andrew Grunt^7: found %i spawn points.\n", RespawnPoints );
}

void CAndrewGrunt::SetActivity( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT( pev ) );

	// reset renderfx with new activity
	pev->renderfx = 0;

	switch( NewActivity )
	{
	case ACT_RANGE_ATTACK1:
			if( m_fStanding ) // get aimable sequence
				iSequence = LookupSequence( "standing_mp5" );
			else // get crouching shoot
				iSequence = LookupSequence( "crouching_mp5" );
		break;
	case ACT_RANGE_ATTACK2:
		iSequence = LookupSequence( "launchgrenade" );
		break;
	case ACT_RUN:
	{
		if( gpGlobals->time > AndrewDashTime + 7 )
		{
			iSequence = LookupSequence( "runfast" );
			AndrewDashTime = gpGlobals->time;
			pev->renderfx = kRenderFxGlowShell;
		}
		else
			iSequence = LookupActivity( NewActivity );
	}
	break;
	case ACT_WALK:
		iSequence = LookupActivity( NewActivity );
		break;
	case ACT_IDLE:
		if( m_MonsterState == MONSTERSTATE_COMBAT )
			NewActivity = ACT_IDLE_ANGRY;

		iSequence = LookupActivity( NewActivity );
		break;
	default:
		iSequence = LookupActivity( NewActivity );
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
		ALERT( at_console, "%s has no sequence for act:%d\n", STRING( pev->classname ), NewActivity );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

void CAndrewGrunt::Shoot( void )
{
	if( m_hEnemy == NULL )
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors( GetAbsAngles() );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( vecShootOrigin - vecShootDir * 10, vecShellVelocity, GetAbsAngles().y, SHELL_556, TE_BOUNCE_SHELL );
	FireBullets( 1, vecShootOrigin, vecShootDir, RunningShooting ? VECTOR_CONE_10DEGREES : VECTOR_CONE_3DEGREES, 4096, BULLET_MONSTER_MP5, 1, 4 ); // 4 dmg per bullet

	pev->effects |= EF_MUZZLEFLASH;

	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );
}

//===================================================================
// env_warpball copypaste with changes
//===================================================================
void CAndrewGrunt::WarpEffect( void )
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam *pBeam;
	Vector vecOrigin = Center();

	EMIT_SOUND( edict(), CHAN_BODY, "hgrunt/rich_warp.wav", 1, 0.1 );
	UTIL_ScreenShake( vecOrigin, 6, 160, 1.0, 666 );
	CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", vecOrigin, TRUE );
	pSpr->AnimateAndDie( 18 );
	pSpr->SetTransparency( kRenderGlow, 77, 210, 130, 255, kRenderFxNoDissipation );
	pSpr->SetScale( 2.0f );
	int iBeams = RANDOM_LONG( 20, 40 );
	while( iDrawn < iBeams && iTimes < (iBeams * 3) )
	{
		vecDest = 300 * (Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize());
		UTIL_TraceLine( vecOrigin, vecOrigin + vecDest, ignore_monsters, NULL, &tr );
		if( tr.flFraction != 1.0 )
		{
			// we hit something.
			iDrawn++;
			pBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 200 );
			pBeam->PointsInit( vecOrigin, tr.vecEndPos );
			if( RANDOM_LONG(0,100) > 80 ) // chance for red beam
				pBeam->SetColor( 255, 25, 25 );
			else // shades of blue-ish
			{
				pBeam->pev->rendercolor.x = 90;
				pBeam->pev->rendercolor.y = RANDOM_LONG( 110, 140 );
				pBeam->pev->rendercolor.z = 240;
			}
			pBeam->SetNoise( 65 );
			pBeam->SetBrightness( 220 );
			pBeam->SetWidth( 15 );
			pBeam->SetScrollRate( 35 );
			pBeam->SetThink( &CBeam::SUB_Remove );
			pBeam->pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.5, 1.6 );
		}
		iTimes++;
	}

	Vector LightOrg = Center();
	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, LightOrg );
	WRITE_BYTE( TE_DLIGHT );
	WRITE_COORD( LightOrg.x );		// origin
	WRITE_COORD( LightOrg.y );
	WRITE_COORD( LightOrg.z );
	WRITE_BYTE( 35 );	// radius
	WRITE_BYTE( 150 );	// R
	WRITE_BYTE( 220 );	// G
	WRITE_BYTE( 230 );	// B
	WRITE_BYTE( 10 );	// life * 10
	WRITE_BYTE( 25 ); // decay
	WRITE_BYTE( 125 ); // brightness
	WRITE_BYTE( 0 ); // shadows
	MESSAGE_END();
}

void CAndrewGrunt::RunAI( void )
{
	if( !HasMemory( bits_MEMORY_KILLED ) ) // fully alive, not dying
	{
		float RegenRate; // added health per second

		if( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			// enable difficulty scaler only once in combat
			if( ScaleDifficultyTime == 0 )
				ScaleDifficultyTime = gpGlobals->time + 30;

			if( !AndrewHealthbar )
			{
				cached_hp = (int)((pev->health / pev->max_health) * 100);
				MESSAGE_BEGIN( MSG_ALL, gmsgHealthbarCenter, NULL );
				WRITE_BYTE( cached_hp ); // hp in percents
				WRITE_STRING( "Andrew Rich" );
				MESSAGE_END();

				AndrewHealthbar = true;
			}
		}

		if( AndrewHealthbar )
		{
			int new_hp = (int)((pev->health / pev->max_health) * 100);
			if( new_hp != cached_hp )
			{
				MESSAGE_BEGIN( MSG_ALL, gmsgHealthbarCenter, NULL );
				WRITE_BYTE( new_hp ); // hp in percents
				WRITE_STRING( "Andrew Rich" );
				MESSAGE_END();
				cached_hp = new_hp;
				ALERT( at_aiconsole, "hp %i percent, current scaler %.2f\n", new_hp, DifficultyScaler );
			}
		}

		// every half a minute, decrease the regeneration rate, to prevent endless fights
		if( ScaleDifficultyTime > 0 && gpGlobals->time > ScaleDifficultyTime )
		{
			if( DifficultyScaler > 0.1f )
				DifficultyScaler -= 0.1f;

			ScaleDifficultyTime = gpGlobals->time + 30;
		}

		if( AndrewSpecialMode )
		{
			switch( g_iSkillLevel )
			{
			default:
			case SKILL_EASY: RegenRate = 40 * DifficultyScaler; break;
			case SKILL_MEDIUM: RegenRate = 50 * DifficultyScaler; break;
			case SKILL_HARD: RegenRate = 60 * DifficultyScaler; break;
			}
		}
		else
		{
			switch( g_iSkillLevel )
			{
			default:
			case SKILL_EASY: RegenRate = 20 * DifficultyScaler; break;
			case SKILL_MEDIUM: RegenRate = 25 * DifficultyScaler; break;
			case SKILL_HARD: RegenRate = 30 * DifficultyScaler; break;
			}
		}

		if( pev->health < pev->max_health && (pev->deadflag == DEAD_NO) )
		{
			if( AndrewSpecialMode || gpGlobals->time > AndrewLastHurt + 3 )
			{
				TakeHealth( RegenRate * gpGlobals->frametime, DMG_GENERIC );
			}
		}

		if( AndrewSpecialMode ) // do not run monster AI when in this mode
		{
			pev->sequence = 16; // loop crouching_idle
			pev->framerate = 0.5;
			pev->renderfx = kRenderFxGlowShell;

			// fire the rockets
			if( FireRocketTime > 0 && gpGlobals->time > FireRocketTime )
			{
				Vector vecTarget = m_hEnemy->GetAbsOrigin() + m_hEnemy->pev->velocity + m_hEnemy->pev->basevelocity;
				// make sure player is not too close
				float dist = (m_hEnemy->GetAbsOrigin() - GetAbsOrigin()).Length();
				if( dist > 500 )
				{
					Vector vecToss = VecCheckThrow( pev, GetGunPosition(), vecTarget, g_GruntGrenadeSpeed, 0.5 );
					if( vecToss != g_vecZero )
					{
						EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "hgrunt/rich_rocket.wav", 1.0, 0.1 );
						UTIL_MakeVectors( GetAbsAngles() );
						CBaseEntity *pRocket = CGrenade::ShootContact( pev, Center() - gpGlobals->v_forward * 4, vecToss );
						if( pRocket )
						{
							// rocket trail
							MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

							WRITE_BYTE( TE_BEAMFOLLOW );
							WRITE_SHORT( pRocket->entindex() );	// entity
							WRITE_SHORT( m_iTrail );	// model
							WRITE_BYTE( 15 ); // life
							WRITE_BYTE( 5 );  // width
							WRITE_BYTE( 224 );   // r, g, b
							WRITE_BYTE( 224 );   // r, g, b
							WRITE_BYTE( 255 );   // r, g, b
							WRITE_BYTE( 255 );	// brightness

							MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
						}
						RocketCount++;
						FireRocketTime = gpGlobals->time + 0.5f;

						if( RocketCount >= 5 )
							FireRocketTime = 0; // max 5 rockets
					}
				}
			}

			// restored to full health, back to fight! (wth is player doing?)
			// (or the shield is broken)
			if( SpecialModeHealth <= 0 || pev->health >= pev->max_health )
				Andrew_Respawn();

			return;
		}

		if( AccumulatedDamage > ACCUMULATED_DMG_THRESHOLD )
		{
			if( gpGlobals->time > AndrewEscapeTime )
				Andrew_Escape();

			// we reset this anyway, he could have teleported because of a grenade
			AccumulatedDamage = 0;
		}
	}

	CBaseMonster::RunAI();
}

void CAndrewGrunt::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( HasSpawnFlags( SF_MONSTER_NODAMAGE ) )
		return;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return;

	if( AndrewSpecialMode )
	{
		switch( RANDOM_LONG( 0, 4 ) )
		{
		case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
		case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
		case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
		case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
		}

		Vector LightOrg = Center();
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, LightOrg );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( LightOrg.x );		// origin
		WRITE_COORD( LightOrg.y );
		WRITE_COORD( LightOrg.z );
		WRITE_BYTE( 25 );	// radius
		WRITE_BYTE( 255 );	// R
		WRITE_BYTE( 25 );	// G
		WRITE_BYTE( 25 );	// B
		WRITE_BYTE( 10 );	// life * 10
		WRITE_BYTE( 50 ); // decay
		WRITE_BYTE( 150 ); // brightness
		WRITE_BYTE( 0 ); // shadows
		MESSAGE_END();
	}

	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CAndrewGrunt::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( HasSpawnFlags(SF_MONSTER_NODAMAGE) )
		return 0;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return 0;

	bitsDamageType |= DMG_NEVERGIB;

	if( AndrewSpecialMode )
	{
		SpecialModeHealth -= flDamage; // decrease "virtual health" of the shield - when it breaks, he goes back to fight
		return 0;
	}
	else
		AccumulatedDamage += flDamage;

	if( pev->health < pev->max_health )
		AndrewLastHurt = gpGlobals->time;

	Forget( bits_MEMORY_INCOVER );

	return CSquadMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

int CAndrewGrunt::IRelationship( CBaseEntity *pTarget )
{
	CBaseMonster *pMonster = (CBaseMonster *)pTarget;

	// pay attention to player at most times, ignore his drone
	if( pMonster->IsPlayer() )
	{
		if( pMonster->Classify() != Classify() )
			return R_NM;
	}

	return CSquadMonster::IRelationship( pTarget );
}

void CAndrewGrunt::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch( pEvent->event )
	{
	case HGRUNT_AE_DROP_GUN:
		break;

	case HGRUNT_AE_RELOAD:
	{
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "hgrunt/gr_reload_mp5.wav", 1, 0.5 );
		m_cAmmoLoaded = m_cClipSize;
		ClearConditions( bits_COND_NO_AMMO_LOADED );
	}
	break;

	case HGRUNT_AE_GREN_LAUNCH:
	{
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, 0.5 );
		CGrenade::ShootContact( pev, GetGunPosition(), m_vecTossVelocity );
		m_fThrowGrenade = FALSE;
		if( g_iSkillLevel == SKILL_HARD )
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 4,7 );
		else
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 6,9 );;
	}
	break;

	case HGRUNT_AE_BURST1:
	{
		Shoot();

		PlayClientSound( this, 252, 0, (m_cAmmoLoaded <= 15 ? m_cAmmoLoaded : 0), Center() );

		CSoundEnt::InsertSound( bits_SOUND_COMBAT, GetAbsOrigin(), 1024, 0.3, ENTINDEX(edict()) );
	}
	break;

	case HGRUNT_AE_BURST2:
	case HGRUNT_AE_BURST3:
		Shoot();
		break;

	case HGRUNT_AE_KICK:
	{
		CBaseEntity *pHurt = Kick();

		if( pHurt )
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch1.wav", 1, ATTN_NORM ); break;
			case 1: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch2.wav", 1, ATTN_NORM ); break;
			case 2: EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/punch3.wav", 1, ATTN_NORM ); break;
			}
			UTIL_MakeVectors( GetAbsAngles() );
			pHurt->pev->punchangle.x = 15;
			pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50 );
			pHurt->TakeDamage( pev, pev, g_GruntKickDamage, DMG_CLUB );
		}
	}
	break;

	case HGRUNT_AE_CAUGHT_ENEMY:
	{
		if( FOkToSpeak() )
		{
			SENTENCEG_PlayRndSz( ENT( pev ), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
			JustSpoke();
		}
	}

	default:
		CSquadMonster::HandleAnimEvent( pEvent );
		break;
	}
}

Schedule_t *CAndrewGrunt::GetSchedule( void )
{
	if( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) && !HasConditions( bits_COND_SEE_ENEMY ) )
	{
		// something hurt me and I can't see anyone. Better relocate
		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
	}

	// clear old sentence
	m_iSentence = HGRUNT_SENT_NONE;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling. 
	if( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if( pev->flags & FL_ONGROUND )
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_GRUNT_REPEL_LAND );
		}
		else
		{
			// repel down a rope, 
			if( m_MonsterState == MONSTERSTATE_COMBAT )
				return GetScheduleOfType( SCHED_GRUNT_REPEL_ATTACK );
			else
				return GetScheduleOfType( SCHED_GRUNT_REPEL );
		}
	}

	// grunts place HIGH priority on running away from danger sounds.
	if( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if( pSound )
		{
			if( pSound->m_iType & bits_SOUND_DANGER )
			{
				// dangerous sound nearby!

				if( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( ENT( pev ), "HG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
					JustSpoke();
				}

				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
			
			if( !HasConditions( bits_COND_SEE_ENEMY ) && (pSound->m_iType & (bits_SOUND_PLAYER | bits_SOUND_COMBAT)) )
				MakeIdealYaw( pSound->m_vecOrigin );
		}
	}

	switch( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
	{
		// dead enemy
		if( HasConditions( bits_COND_ENEMY_DEAD | bits_COND_ENEMY_LOST ) )
		{
			// call base class, all code to handle dead enemies is centralized there.
			return CBaseMonster::GetSchedule();
		}

		// new enemy
		if( HasConditions( bits_COND_NEW_ENEMY ) )
		{
			if( FOkToSpeak() )
			{
				if( (m_hEnemy != NULL) && m_hEnemy->IsPlayer() )
					// player
					SENTENCEG_PlayRndSz( ENT( pev ), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
				else if( (m_hEnemy != NULL) &&
					(m_hEnemy->Classify() != CLASS_PLAYER_ALLY) &&
					(m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE) &&
					(m_hEnemy->Classify() != CLASS_MACHINE) )
					// monster
					SENTENCEG_PlayRndSz( ENT( pev ), "HG_MONST", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );

				JustSpoke();
			}

			if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_GRUNT_SUPPRESS );
			else
				return CBaseMonster::GetSchedule(); //return GetScheduleOfType ( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
		}
		// no ammo
		else if( HasConditions( bits_COND_NO_AMMO_LOADED ) )
		{
			//!!!KELLY - this individual just realized he's out of bullet ammo. 
			// He's going to try to find cover to run to and reload, but rarely, if 
			// none is available, he'll drop and reload in the open here. 
			return GetScheduleOfType( SCHED_GRUNT_COVER_AND_RELOAD );
		}

		// damaged just a little
		else if( HasConditions( bits_COND_LIGHT_DAMAGE ) )
		{
			if( pev->health < pev->max_health * 0.1f ) // he's tired and almost dead. run away or try to fire back
			{
				switch( RANDOM_LONG( 0, 1 ) )
				{
				case 0: return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY ); break;
				case 1: return GetScheduleOfType( SCHED_RANGE_ATTACK1 ); break;
				}
			}
			else if( pev->health < pev->max_health * 0.25f ) // Andrew now has 1/4 of his HP. 30% chance of flinch
			{
				switch( RANDOM_LONG( 0, 2 ) )
				{
				case 0: return GetScheduleOfType( SCHED_SMALL_FLINCH ); break;
				case 1: return GetScheduleOfType( SCHED_RANGE_ATTACK1 ); break;
				case 2: return GetScheduleOfType( SCHED_GRUNT_SUPPRESS ); break;
				}
			}
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}
		// can kick
		else if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
		{
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
		}
		// can grenade launch

		else if( HasWeapon( HGRUNT_GRENADELAUNCHER ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
		{
			// shoot a grenade if you can
			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
		}
		// can shoot
		else if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			if( InSquad() )
			{
				// if the enemy has eluded the squad and a squad member has just located the enemy
				// and the enemy does not see the squad member, issue a call to the squad to waste a 
				// little time and give the player a chance to turn.
				if( MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;
					return GetScheduleOfType( SCHED_GRUNT_FOUND_ENEMY );
				}
			}

			if( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				// try to take an available ENGAGE slot
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}
			else
			{
				// instead of hiding, fire!
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}
		}
		// can't see enemy
		else if( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
		{
			if( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				if( FOkToSpeak() )
				{
					//SENTENCEG_PlayRndSz( ENT(pev), "HG_CHARGE", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					m_iSentence = HGRUNT_SENT_CHARGE;
					//JustSpoke();
				}

				return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
			}
			else
			{
				//!!!KELLY - grunt is going to stay put for a couple seconds to see if
				// the enemy wanders back out into the open, or approaches the
				// grunt's covered position. Good place for a taunt, I guess?
				if( FOkToSpeak() && RANDOM_LONG( 0, 1 ) )
				{
					SENTENCEG_PlayRndSz( ENT( pev ), "HG_TAUNT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_STANDOFF );
			}
		}

		if( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
		}
	}
	}

	// no special cases here, call the base class
	return CSquadMonster::GetSchedule();
}

Schedule_t *CAndrewGrunt::GetScheduleOfType( int Type )
{
	switch( Type )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
	{
		if( InSquad() )
		{
			if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				if( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( ENT( pev ), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
					JustSpoke();
				}
				return slGruntTossGrenadeCover;
			}
			else
			{
				return &slGruntTakeCover[0];
			}
		}
		else
		{
			//				if ( RANDOM_LONG(0,1) )
			//				{
			return &slGruntTakeCover[0];
			//				}
			//				else
			//				{
			//					return &slGruntGrenadeCover[ 0 ];
			//				}
		}
	}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
	{
		// DASH OR DISAPPEAR
		return &slGruntTakeCoverFromBestSound[0];
	}
	case SCHED_GRUNT_TAKECOVER_FAILED:
	{
		if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
		{
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}

		return GetScheduleOfType( SCHED_FAIL );
	}
	break;
	case SCHED_GRUNT_ELOF_FAIL:
	{
		// human grunt is unable to move to a position that allows him to attack the enemy.
	//	if( FVisible( m_hEnemy ) )
	//	{
	//		if( pev->health < 150 ) // for general
	//			return GetScheduleOfType( SCHED_GRUNT_SUPPRESS ); // diffusion - low hp, nervous
	//		else
	//			return GetScheduleOfType( SCHED_RANGE_ATTACK1 ); // or just try to attack the enemy from the current position
	//	}
	//	else
			return GetScheduleOfType( SCHED_MOVE_TO_ENEMY_LKPRUN ); // can't see enemy. try to reposition
	}
	break;
	case SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE:
	{
		return &slGruntEstablishLineOfFire[0];
	}
	break;
	case SCHED_RANGE_ATTACK1:
	{
		AttackStartTime = gpGlobals->time + CombatWaitTime;
		if( m_fStanding )
			return &slGruntRangeAttack1B[0];
		else
			return &slGruntRangeAttack1A[0];
	}
	case SCHED_RANGE_ATTACK2:
	{
		AttackStartTime = gpGlobals->time + CombatWaitTime;
		return &slGruntRangeAttack2[0];
	}
	case SCHED_COMBAT_FACE:
	{
		return &slGruntCombatFace[0];
	}
	case SCHED_GRUNT_WAIT_FACE_ENEMY:
	{
		return &slGruntWaitInCover[0];
	}
	case SCHED_GRUNT_SWEEP:
	{
		return &slGruntSweep[0];
	}
	case SCHED_GRUNT_COVER_AND_RELOAD:
	{
		return &slGruntHideReload[0];
	}
	case SCHED_GRUNT_FOUND_ENEMY:
	{
		return &slGruntFoundEnemy[0];
	}
	case SCHED_VICTORY_DANCE:
	{
		if( InSquad() )
		{
			if( !IsLeader() )
			{
				return &slGruntFail[0];
			}
		}

		return &slGruntVictoryDance[0];
	}
	case SCHED_GRUNT_SUPPRESS:
	{
		if( m_hEnemy->IsPlayer() && m_fFirstEncounter )
		{
			m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
			return &slGruntSignalSuppress[0];
		}
		else
		{
			return &slGruntSuppress[0];
		}
	}
	case SCHED_FAIL:
	{
		if( m_hEnemy != NULL )
		{
			// grunt has an enemy, so pick a different default fail schedule most likely to help recover.
			return &slGruntCombatFail[0];
		}

		return &slGruntFail[0];
	}
	case SCHED_GRUNT_REPEL:
	{
		Vector vecVelocity = GetAbsVelocity();
		if( vecVelocity.z > -128 )
		{
			vecVelocity.z -= 32;
			SetAbsVelocity( vecVelocity );
		}
		return &slGruntRepel[0];
	}
	case SCHED_GRUNT_REPEL_ATTACK:
	{
		Vector vecVelocity = GetAbsVelocity();
		if( vecVelocity.z > -128 )
		{
			vecVelocity.z -= 32;
			SetAbsVelocity( vecVelocity );
		}
		return &slGruntRepelAttack[0];
	}
	case SCHED_GRUNT_REPEL_LAND:
	{
		return &slGruntRepelLand[0];
	}
	default:
	{
		return CSquadMonster::GetScheduleOfType( Type );
	}
	}
}

//=================================================================================
// Andrew_Escape: teleport away in an attempt to escape from player's grenade, or after taking too much damage.
// Fire the rockets, start recharging health, and make glowshell
//=================================================================================
void CAndrewGrunt::Andrew_Escape(void)
{
	if( !IsAlive() )
		return;
	
	if( RespawnPoints == 0 )
		CollectPoints();
	
	int Tries = 0; // to prevent a possible loop, when all points are close to player (wtf?)

	// check if the player is too close to this point. if so, choose another
	edict_t *pPlayer = INDEXENT( 1 );
	Vector PlayerOrigin = pPlayer->v.origin;

ChooseAnEscapePoint:
	int RandomPointNum = RANDOM_LONG( 0, RespawnPoints - 1 );

	if( RespawnPoints == 0 )
	{
		ALERT( at_aiconsole, "^2Andrew Grunt:^7 ERROR: no spawn points, choosing origin!\n" );
		AndrewRespawnPoint[RandomPointNum] = GetAbsOrigin();
	}
	else if( Tries > 10 ) // too many tries - just choose any point
	{
		RandomPointNum = RANDOM_LONG( 0, RespawnPoints - 1 );
	}
	else if( (PlayerOrigin - AndrewRespawnPoint[RandomPointNum]).Length() < 1250 ) // need far distance
	{
		ALERT( at_aiconsole, "^2Andrew Grunt:^7 spawn point %i is too close to the player, choosing another...\n", RandomPointNum );
		Tries++;
		goto ChooseAnEscapePoint;
	}

	// create a warp beam between starting and ending locations
	CBeam *pBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 200 );
	pBeam->PointsInit( Center(), AndrewRespawnPoint[RandomPointNum] );
	pBeam->pev->rendercolor.x = 90;
	pBeam->pev->rendercolor.y = RANDOM_LONG( 110, 140 );
	pBeam->pev->rendercolor.z = 240;
	pBeam->SetNoise( 20 );
	pBeam->SetBrightness( 255 );
	pBeam->SetWidth( 50 );
	pBeam->SetScrollRate( 35 );
	pBeam->SetThink( &CBeam::SUB_Remove );
	pBeam->pev->nextthink = gpGlobals->time + 2.0;

	// now teleport
	WarpEffect();
	pev->velocity = g_vecZero;
	ClearSchedule();
	Teleport( &AndrewRespawnPoint[RandomPointNum], &GetAbsAngles(), &g_vecZero );
	AndrewSpecialMode = true;
	SpecialModeHealth = 150;
	FireRocketTime = gpGlobals->time + 2;
	RocketCount = 0;
	AccumulatedDamage = 0;
	pev->renderfx = kRenderFxGlowShell;
}

//=================================================================================
// Andrew_Respawn: choose a valid spawn point and go back to action.
//=================================================================================
void CAndrewGrunt::Andrew_Respawn( void )
{
	if( !IsAlive() )
		return;

	int Tries = 0; // to prevent a possible loop, when all points are close to player (wtf?)

ChooseAPoint:	
	int RandomPointNum = RANDOM_LONG( 0, RespawnPoints - 1 );

	// check if the player is too close to this point. if so, choose another
	edict_t *pPlayer = INDEXENT( 1 );
	Vector PlayerOrigin = pPlayer->v.origin;

	// too many tries or spawn points were never collected
	if( RespawnPoints == 0 )
	{
		ALERT( at_aiconsole, "^2Andrew Grunt:^7 ERROR: no spawn points, choosing origin!\n" );
		AndrewRespawnPoint[RandomPointNum] = GetAbsOrigin();
	}
	else if( Tries > 10 )  // too many tries - just choose any point
	{
		RandomPointNum = RANDOM_LONG( 0, RespawnPoints - 1 );
	}
	else if( (PlayerOrigin - AndrewRespawnPoint[RandomPointNum]).Length() < 750 )
	{
		ALERT( at_aiconsole, "^2Andrew Grunt:^7 spawn point %i is too close to the player, choosing another...\n", RandomPointNum );
		Tries++;
		goto ChooseAPoint;
	}

	// create a warp beam between starting and ending locations
	CBeam *pBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 200 );
	pBeam->PointsInit( Center(), AndrewRespawnPoint[RandomPointNum] );
	pBeam->pev->rendercolor.x = 90;
	pBeam->pev->rendercolor.y = RANDOM_LONG( 110, 140 );
	pBeam->pev->rendercolor.z = 240;
	pBeam->SetNoise( 20 );
	pBeam->SetBrightness( 255 );
	pBeam->SetWidth( 50 );
	pBeam->SetScrollRate( 35 );
	pBeam->SetThink( &CBeam::SUB_Remove );
	pBeam->pev->nextthink = gpGlobals->time + 2.0;
	
	// now teleport
	WarpEffect();
	Teleport( &AndrewRespawnPoint[RandomPointNum], &GetAbsAngles(), &g_vecZero );
	AndrewSpecialMode = false;
	AndrewEscapeTime = gpGlobals->time + 20; // when the next escape attempt happens
	RocketCount = 0;
	FireRocketTime = 0;
	SpecialModeHealth = 0;
	AccumulatedDamage = 0;

	pev->rendermode = kRenderNormal;
	pev->renderfx = 0;
}

void CAndrewGrunt::Killed( entvars_t *pevAttacker, int iGib )
{
	AndrewSpecialMode = false;
	AccumulatedDamage = 0;
	RocketCount = 0;
	FireRocketTime = 0;
	SpecialModeHealth = 0;
	pev->rendermode = kRenderNormal;
	pev->renderfx = 0;

	MESSAGE_BEGIN( MSG_ALL, gmsgHealthbarCenter, NULL );
	WRITE_BYTE( 255 ); // 255 disables the healthbar
	MESSAGE_END();

	CSquadMonster::Killed( pevAttacker, iGib );
}






//=================================================================================
// CAndrewSpawnPoint: dummy entity where Andrew will spawn again (a few different points in the battlefield)
//=================================================================================
class CAndrewSpawnPoint : public CPointEntity
{
	DECLARE_CLASS( CAndrewSpawnPoint, CPointEntity );
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( info_andrew_spawn_point, CAndrewSpawnPoint );

void CAndrewSpawnPoint::Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );
}