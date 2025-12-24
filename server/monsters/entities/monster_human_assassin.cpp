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
// hassassin - Human assassin, fast and stealthy
//=========================================================

// diffusion - NOTE: I have removed the ability of assassins to reload, but I left the code
// it doesn't work the way I want it to and I don't have any more ideas
// main problems - 1) the clip can go lower than 0 despite that I set the conditions
// and 2) they don't reload like grunts when they have less than 0 ammo.
// they can stay in the cover and will reload only when player sees them and attacks them.

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"game/schedule.h"
#include	"squadmonster.h"
#include	"weapons/weapons.h"
#include	"entities/soundent.h"
#include	"game/game.h"
#include	"animation.h"

extern DLL_GLOBAL int  g_iSkillLevel;
float g_nextsmoketime = 0;
bool g_smokeallowed = true;

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_ASSASSIN_EXPOSED = LAST_COMMON_SCHEDULE + 1,// cover was blown.
	SCHED_ASSASSIN_JUMP,	// fly through the air
	SCHED_ASSASSIN_JUMP_ATTACK,	// fly through the air and shoot
	SCHED_ASSASSIN_JUMP_LAND, // hit and run away
	SCHED_ASSASSIN_COVER_AND_RELOAD, // diffusion - reload
};

//=========================================================
// monster-specific tasks
//=========================================================

enum
{
	TASK_ASSASSIN_FALL_TO_GROUND = LAST_COMMON_TASK + 1, // falling and waiting to hit ground
};


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		ASSASSIN_AE_SHOOT1	1
#define		ASSASSIN_AE_TOSS1	2
#define		ASSASSIN_AE_JUMP	3
#define		ASSASSIN_AE_RELOAD	4
#define		ASSASSIN_AE_KICK	5
#define		ASSASSIN_AE_GRUNT	6
#define		ASSASSIN_AE_DEATH	7

#define GUN_GROUP		2
#define GUN_PISTOL		0
#define GUN_NONE		1
#define GUN_TMP			2

#define HASS_WPN_PISTOL	1
#define HASS_WPN_TMP	2

#define bits_MEMORY_BADJUMP		(bits_MEMORY_CUSTOM1)

class CHAssassin : public CBaseMonster
{
	DECLARE_CLASS( CHAssassin, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed ( void );
	int  Classify ( void );
	int  ISoundMask ( void);
	void Shoot( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	Schedule_t* GetSchedule ( void );
	Schedule_t* GetScheduleOfType ( int Type );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );	// jump
	BOOL CheckMeleeAttack2 ( float flDot, float flDist );	// kick
	BOOL CheckRangeAttack1 ( float flDot, float flDist );	// shoot
	BOOL CheckRangeAttack2 ( float flDot, float flDist );	// throw grenade
	CBaseEntity *Kick( void );
	void StartTask ( Task_t *pTask );
	void RunAI( void );
	void RunTask ( Task_t *pTask );
	void DeathSound ( void );
	void IdleSound ( void );
	void SetActivity(Activity NewActivity);
	void ClearEffects(void);
	BOOL HasHumanGibs( void ) { return TRUE; };
	CUSTOM_SCHEDULES;

	DECLARE_DATADESC();

	float	m_flLastShot;
	float	m_flDiviation;

	float	m_flNextJump;
	Vector	m_vecJumpVelocity;

	float	m_flNextGrenadeCheck;
	Vector	m_vecTossVelocity;
	BOOL	m_fThrowGrenade;

	int	m_iTargetRanderamt;
	int	m_iFrustration;

//	int m_iClipSize;

//	float IdleTime; // diffusion - reload when nobody around

	// alien guy
	float NextAttackTime; // when range1attack not available, hide
	int ProjectileCount; // count the bullets, then disable the attack (not saved)

	// secass
	bool SmokeDropped; // drop a smoke only once, then use EMP grenades
};

LINK_ENTITY_TO_CLASS( monster_human_assassin, CHAssassin );

BEGIN_DATADESC( CHAssassin )
	DEFINE_FIELD( m_flLastShot, FIELD_TIME ),
	DEFINE_FIELD( m_flDiviation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextJump, FIELD_TIME ),
	DEFINE_FIELD( m_vecJumpVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_fThrowGrenade, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iTargetRanderamt, FIELD_INTEGER ),
	DEFINE_FIELD( m_iFrustration, FIELD_INTEGER ),
//	DEFINE_FIELD( m_iClipSize, FIELD_INTEGER ),
//	DEFINE_FIELD( IdleTime, FIELD_TIME ),
	DEFINE_FIELD( NextAttackTime, FIELD_TIME ),
//	DEFINE_FIELD( SmokeDropped, FIELD_BOOLEAN ), // don't save, allow throwing smokes again after saverestore
END_DATADESC()

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

//=========================================================
// Fail Schedule
//=========================================================
Task_t	tlAssassinFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY,		(float)2		},
	 { TASK_WAIT_PVS,			(float)0		},
	{ TASK_SET_SCHEDULE,		(float)SCHED_CHASE_ENEMY },
};

Schedule_t	slAssassinFail[] =
{
	{
		tlAssassinFail,
		SIZEOFARRAY ( tlAssassinFail ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_PROVOKED			|
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_NO_AMMO_LOADED	|
		bits_COND_ENEMY_LOST		|
		bits_COND_HEAR_SOUND		|
		bits_COND_ENEMY_OCCLUDED,
	
		bits_SOUND_DANGER |
		bits_SOUND_PLAYER,
		"AssassinFail"
	},
};


//=========================================================
// Enemy exposed Agrunt's cover
//=========================================================
Task_t	tlAssassinExposed[] =
{
	{ TASK_STOP_MOVING,			(float)0							},
	{ TASK_RANGE_ATTACK1,		(float)0							},
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_ASSASSIN_JUMP			},
	{ TASK_SET_SCHEDULE,		(float)SCHED_TAKE_COVER_FROM_ENEMY	},
};

Schedule_t slAssassinExposed[] =
{
	{
		tlAssassinExposed,
		SIZEOFARRAY ( tlAssassinExposed ),
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_ENEMY_LOST |
		bits_COND_ENEMY_OCCLUDED,
		0,
		"AssassinExposed",
	},
};


//=========================================================
// Take cover from enemy! Tries lateral cover before node 
// cover! 
//=========================================================
Task_t	tlAssassinTakeCoverFromEnemy[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_WAIT,					(float)0.1					},  // AIFIX
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RANGE_ATTACK1	},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
};

Schedule_t	slAssassinTakeCoverFromEnemy[] =
{
	{ 
		tlAssassinTakeCoverFromEnemy,
		SIZEOFARRAY ( tlAssassinTakeCoverFromEnemy ), 
		bits_COND_NEW_ENEMY |
		bits_COND_CAN_MELEE_ATTACK1		|
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_LOST |
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED,
		
		bits_SOUND_DANGER,
		"AssassinTakeCoverFromEnemy"
	},
};


//=========================================================
// Take cover from enemy! Tries lateral cover before node 
// cover! 
//=========================================================
Task_t	tlAssassinTakeCoverFromEnemy2[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_WAIT,					(float)0.1					},  // AIFIX
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_RANGE_ATTACK1,			(float)0					},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RANGE_ATTACK2	},
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY,	(float)384			},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
};

Schedule_t	slAssassinTakeCoverFromEnemy2[] =
{
	{ 
		tlAssassinTakeCoverFromEnemy2,
		SIZEOFARRAY ( tlAssassinTakeCoverFromEnemy2 ), 
		bits_COND_NEW_ENEMY |
		bits_COND_CAN_MELEE_ATTACK2		|
		bits_COND_HEAR_SOUND		|
		bits_COND_ENEMY_LOST |
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED,
		
		bits_SOUND_DANGER,
		"AssassinTakeCoverFromEnemy2"
	},
};


//=========================================================
// hide from the loudest sound source
//=========================================================
Task_t	tlAssassinTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_MELEE_ATTACK1	},
	{ TASK_STOP_MOVING,					(float)0					},
	{ TASK_FIND_COVER_FROM_BEST_SOUND,	(float)0					},
	{ TASK_RUN_PATH,					(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0					},
	{ TASK_REMEMBER,					(float)bits_MEMORY_INCOVER	},
	{ TASK_TURN_LEFT,					(float)179					},
};

Schedule_t	slAssassinTakeCoverFromBestSound[] =
{
	{ 
		tlAssassinTakeCoverFromBestSound,
		SIZEOFARRAY ( tlAssassinTakeCoverFromBestSound ), 
		bits_COND_NEW_ENEMY | bits_COND_ENEMY_LOST | bits_COND_NO_AMMO_LOADED,
		0,
		"AssassinTakeCoverFromBestSound"
	},
};





//=========================================================
// AlertIdle Schedules
//=========================================================
Task_t	tlAssassinHide[] =
{
	{ TASK_STOP_MOVING,			0						 },
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE			 },
	{ TASK_WAIT,				(float)2				 },
	{ TASK_SET_SCHEDULE,		(float)SCHED_CHASE_ENEMY },
};

Schedule_t	slAssassinHide[] =
{
	{ 
		tlAssassinHide,
		SIZEOFARRAY ( tlAssassinHide ), 
		bits_COND_NEW_ENEMY				|
		bits_COND_SEE_ENEMY				|
		bits_COND_SEE_FEAR				|
		bits_COND_LIGHT_DAMAGE			|
		bits_COND_HEAVY_DAMAGE			|
		bits_COND_PROVOKED		|
		bits_COND_HEAR_SOUND	|
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_LOST |
		bits_COND_ENEMY_OCCLUDED,
		
		bits_SOUND_DANGER,
		"AssassinHide"
	},
};



//=========================================================
// HUNT Schedules
//=========================================================
Task_t tlAssassinHunt[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_COMBAT_FACE	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0		},
	{ TASK_RUN_PATH,			(float)0		},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},
};

Schedule_t slAssassinHunt[] =
{
	{ 
		tlAssassinHunt,
		SIZEOFARRAY ( tlAssassinHunt ),
		bits_COND_NEW_ENEMY			|
		// bits_COND_SEE_ENEMY			|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_HEAR_SOUND	|
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_LOST |
		bits_COND_ENEMY_OCCLUDED,
		
		bits_SOUND_DANGER,
		"AssassinHunt"
	},
};

Task_t tlAssassinHunt2[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_MOVE_TO_ENEMY_LKPRUN	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0		},
	{ TASK_RUN_PATH,			(float)0		},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},
};

Schedule_t slAssassinHunt2[] =
{
	{
		tlAssassinHunt2,
		SIZEOFARRAY( tlAssassinHunt2 ),
		bits_COND_NEW_ENEMY |
		// bits_COND_SEE_ENEMY			|
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_HEAR_SOUND |
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_LOST |
		bits_COND_ENEMY_OCCLUDED,

		bits_SOUND_DANGER,
		"AssassinHunt2"
	},
};


//=========================================================
// Jumping Schedules
//=========================================================
Task_t	tlAssassinJump[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_HOP	},
	{ TASK_SET_SCHEDULE,		(float)SCHED_ASSASSIN_JUMP_ATTACK },
};

Schedule_t	slAssassinJump[] =
{
	{ 
		tlAssassinJump,
		SIZEOFARRAY ( tlAssassinJump ), 
		0, 
		0, 
		"AssassinJump"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlAssassinJumpAttack[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_ASSASSIN_JUMP_LAND	},
	// { TASK_SET_ACTIVITY,		(float)ACT_FLY	},
	{ TASK_ASSASSIN_FALL_TO_GROUND, (float)0		},
};


Schedule_t	slAssassinJumpAttack[] =
{
	{ 
		tlAssassinJumpAttack,
		SIZEOFARRAY ( tlAssassinJumpAttack ), 
		0, 
		0,
		"AssassinJumpAttack"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlAssassinJumpLand[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_ASSASSIN_EXPOSED	},
	// { TASK_SET_FAIL_SCHEDULE,		(float)SCHED_MELEE_ATTACK1	},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_REMEMBER,				(float)bits_MEMORY_BADJUMP	},
	{ TASK_FIND_NODE_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_FORGET,					(float)bits_MEMORY_BADJUMP	},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RANGE_ATTACK1	},
};

Schedule_t	slAssassinJumpLand[] =
{
	{ 
		tlAssassinJumpLand,
		SIZEOFARRAY ( tlAssassinJumpLand ), 
		0, 
		0,
		"AssassinJumpLand"
	},
};
/*
// diffusion - cover and reload
Task_t	tlAssassinHideReload[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RELOAD			},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_WAIT_FACE_ENEMY,			(float)2					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_RELOAD			},
};

Schedule_t slAssassinHideReload[] = 
{
	{
		tlAssassinHideReload,
		SIZEOFARRAY ( tlAssassinHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_LOST |
		bits_COND_ENEMY_OCCLUDED,

		bits_SOUND_DANGER,
		"AssassinHideReload"
	}
};
*/
Task_t tlAssassinRangeAttack1[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, (float)0 },
//	{ TASK_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
};

Schedule_t slAssassinRangeAttack1[] =
{
	{
		tlAssassinRangeAttack1,
		SIZEOFARRAY( tlAssassinRangeAttack1 ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_ENEMY_LOST |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_ENEMY_LOST	|
		bits_COND_NO_AMMO_LOADED |
//		bits_COND_NOFIRE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Assassin Range Attack1"
	},
};

DEFINE_CUSTOM_SCHEDULES( CHAssassin )
{
	slAssassinFail,
	slAssassinExposed,
	slAssassinTakeCoverFromEnemy,
	slAssassinTakeCoverFromEnemy2,
	slAssassinTakeCoverFromBestSound,
	slAssassinHide,
	slAssassinHunt,
	slAssassinJump,
	slAssassinJumpAttack,
	slAssassinJumpLand,
//	slAssassinHideReload, // diffusion
	slAssassinRangeAttack1,
};

IMPLEMENT_CUSTOM_SCHEDULES( CHAssassin, CBaseMonster );

//=========================================================
// DieSound
//=========================================================
void CHAssassin :: DeathSound ( void )
{
}

//=========================================================
// IdleSound
//=========================================================
void CHAssassin :: IdleSound ( void )
{
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CHAssassin :: ISoundMask ( void) 
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CHAssassin :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_HUMAN_MILITARY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CHAssassin :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 200;
		break;
	default:			
		ys = 200;
		break;
	}

	pev->yaw_speed = ys;
}


//=========================================================
// Shoot
//=========================================================
void CHAssassin :: Shoot ( void )
{
	if (m_hEnemy == NULL)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	if (m_flLastShot + 2 < gpGlobals->time)
	{
		m_flDiviation = 0.10;
	}
	else
	{
		m_flDiviation -= 0.01;
		if (m_flDiviation < 0.02)
			m_flDiviation = 0.02;
	}
	m_flLastShot = gpGlobals->time;

	if( HasWeapon(HASS_WPN_PISTOL) )
	{
		UTIL_MakeVectors ( GetAbsAngles() );
		Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
		EjectBrass( GetAbsOrigin() + gpGlobals->v_up * 32 + gpGlobals->v_forward * 12, vecShellVelocity, GetAbsAngles().y, SHELL_9MM, TE_BOUNCE_SHELL);
		FireBullets(1, vecShootOrigin, vecShootDir, Vector( m_flDiviation, m_flDiviation, m_flDiviation ), 2048, BULLET_MONSTER_9MM, 1, RANDOM_FLOAT(3, 5) ); // shoot +-8 degrees, ~4 dmg
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/assassin_pistol.wav", RANDOM_FLOAT(0.6, 0.8), ATTN_NORM);
	}
	else if( HasWeapon(HASS_WPN_TMP) )
	{
		UTIL_MakeVectors ( GetAbsAngles() );
		Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
		EjectBrass( GetAbsOrigin() + gpGlobals->v_up * 32 + gpGlobals->v_forward * 12, vecShellVelocity, GetAbsAngles().y, SHELL_9MM, TE_BOUNCE_SHELL);
		FireBullets(1, vecShootOrigin, vecShootDir, Vector( m_flDiviation, m_flDiviation, m_flDiviation ), 2048, BULLET_MONSTER_9MM, 1, RANDOM_FLOAT(2, 3) ); // shoot +-8 degrees, ~3 dmg

		switch(RANDOM_LONG(0,2))
		{
			case 0:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/assassin_tmp01.wav", RANDOM_FLOAT(0.6, 0.8), ATTN_NORM); break;
			case 1:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/assassin_tmp02.wav", RANDOM_FLOAT(0.6, 0.8), ATTN_NORM); break;
			case 2:	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/assassin_tmp03.wav", RANDOM_FLOAT(0.6, 0.8), ATTN_NORM); break;
		}
	}

	//pev->effects |= EF_MUZZLEFLASH;

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );

//	m_cAmmoLoaded--;
//	if ( m_cAmmoLoaded <= 0 )
//		SetConditions(bits_COND_NO_AMMO_LOADED);
//	ALERT(at_console, "%3d\n", m_cAmmoLoaded);
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CHAssassin :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case ASSASSIN_AE_SHOOT1:
		Shoot( );
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 192, 0.3, ENTINDEX(edict()) );
		break;
	case ASSASSIN_AE_GRUNT:
		if( !HasSpawnFlags( SF_MONSTER_GAG ) )
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "assassin/ass_kicking.wav", 1, ATTN_NORM ); break;
			case 1: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "assassin/ass_kicking2.wav", 1, ATTN_NORM ); break;
			case 2: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "assassin/ass_kicking3.wav", 1, ATTN_NORM ); break;
			}
		}
		break;
	case ASSASSIN_AE_DEATH:
		if( !HasSpawnFlags( SF_MONSTER_GAG ) )
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_death.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
			case 1: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_death2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
			case 2: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_death3.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
			}
		}
		break;
	case ASSASSIN_AE_TOSS1:
		{
			UTIL_MakeVectors( GetAbsAngles() );
			CBaseEntity *pGrenade = CGrenade::ShootTimed( pev, GetAbsOrigin() + gpGlobals->v_forward * 34 + Vector (0, 0, 32), m_vecTossVelocity, 2.0 );
			if( pGrenade )
			{
				SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
				pGrenade->pev->body = 1; // change to body without pin
			}

			m_flNextGrenadeCheck = gpGlobals->time + 6;// wait six seconds before even looking again to see if a grenade can be thrown.
			m_fThrowGrenade = FALSE;
			// !!!LATER - when in a group, only try to throw grenade if ordered.
		}
		break;
	case ASSASSIN_AE_JUMP:
		{
			// ALERT( at_console, "jumping");
			UTIL_MakeAimVectors( GetAbsAngles() );
			pev->movetype = MOVETYPE_TOSS;
			pev->flags &= ~FL_ONGROUND;
			SetAbsVelocity( m_vecJumpVelocity );
			m_flNextJump = gpGlobals->time + 3.0;
			if( !HasSpawnFlags( SF_MONSTER_GAG ) && m_MonsterState == MONSTERSTATE_COMBAT ) // be silent when idle
				EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_jump.wav", 1, ATTN_NORM, 0, RANDOM_LONG(95,105) );
		}
		return;
/*	case ASSASSIN_AE_RELOAD:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/assassin_reload.wav", 1, ATTN_NORM );
		m_cAmmoLoaded = m_iClipSize;
		ClearConditions( bits_COND_NO_AMMO_LOADED );
		UTIL_MakeAimVectors( GetAbsAngles() );
		pev->movetype = MOVETYPE_TOSS;
		pev->flags &= ~FL_ONGROUND;
		SetAbsVelocity( m_vecJumpVelocity * 0.6 );
		break;*/
	case ASSASSIN_AE_KICK:
	{
		CBaseEntity *pHurt = Kick();

		if( pHurt )
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/punch1.wav", 1, ATTN_NORM ); break;
			case 1: EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/punch2.wav", 1, ATTN_NORM ); break;
			case 2: EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/punch3.wav", 1, ATTN_NORM ); break;
			}
			UTIL_MakeVectors( GetAbsAngles() );
			pHurt->pev->punchangle.x = 15;
			pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50 );
			pHurt->TakeDamage( pev, pev, 5, DMG_CLUB );
			UTIL_ScreenShakeLocal( pHurt, pHurt->GetAbsOrigin(), 1.5, 100, 0.5, 0, true );
		}
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
void CHAssassin :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/hassassin.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	if (!pev->health) pev->health = 65;
	pev->max_health = pev->health;
	m_flFieldOfView		= VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_MELEE_ATTACK1 | bits_CAP_DOORS_GROUP;
	pev->friction		= 1;

	m_HackedGunPos		= Vector( 0, 24, 48 );

//	m_iTargetRanderamt	= 40;
//	pev->renderamt		= 40;
//	pev->rendermode		= kRenderTransTexture;

	AddWeapon(HASS_WPN_PISTOL);
	SetBodygroup( GUN_GROUP, GUN_PISTOL );

//	m_iClipSize = 15;
//	m_cAmmoLoaded = m_iClipSize;
//	IdleTime = 0;

	m_vecJumpVelocity = Vector( 0, 0, 500 );

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CHAssassin :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/hassassin.mdl");

	PRECACHE_SOUND("scripts/cloak.wav"); //new cloak sound for Diffusion

	PRECACHE_SOUND("weapons/assassin_pistol.wav");
	PRECACHE_SOUND("weapons/assassin_tmp01.wav");
	PRECACHE_SOUND("weapons/assassin_tmp02.wav");
	PRECACHE_SOUND("weapons/assassin_tmp03.wav");
//	PRECACHE_SOUND("weapons/assassin_reload.wav");
	PRECACHE_SOUND( "weapons/punch1.wav" );
	PRECACHE_SOUND( "weapons/punch2.wav" );
	PRECACHE_SOUND( "weapons/punch3.wav" );

	PRECACHE_SOUND( "assassin/ass_death.wav" );
	PRECACHE_SOUND( "assassin/ass_death2.wav" );
	PRECACHE_SOUND( "assassin/ass_death3.wav" );
	PRECACHE_SOUND( "assassin/ass_jump.wav" );
	PRECACHE_SOUND( "assassin/ass_kicking.wav" );
	PRECACHE_SOUND( "assassin/ass_kicking2.wav" );
	PRECACHE_SOUND( "assassin/ass_kicking3.wav" );
}	

void CHAssassin :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// grunt is either shooting standing or shooting crouched
		if( HasWeapon(HASS_WPN_PISTOL) )
			iSequence = LookupSequence( "shoot" );
		else if( HasWeapon(HASS_WPN_TMP) )
			iSequence = LookupSequence( "shoottmp" );
		break;
//	case ACT_RELOAD:
//		iSequence = LookupSequence( "jumpreload" );
//		break;
	default:
		iSequence = LookupActivity ( NewActivity );
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
// CheckMeleeAttack1 - jump like crazy if the enemy gets too close. 
//=========================================================
BOOL CHAssassin :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	if ( m_flNextJump < gpGlobals->time && (flDist <= 128 || HasMemory( bits_MEMORY_BADJUMP )) && m_hEnemy != NULL )
	{
		TraceResult	tr;

		Vector vecDest = GetAbsOrigin() + Vector( RANDOM_FLOAT( -64, 64), RANDOM_FLOAT( -64, 64 ), 160 );

		UTIL_TraceHull( GetAbsOrigin() + Vector( 0, 0, 36 ), vecDest + Vector( 0, 0, 36 ), dont_ignore_monsters, human_hull, ENT(pev), &tr);

		if ( tr.fStartSolid || tr.flFraction < 1.0)
			return FALSE;

		float flGravity = g_psv_gravity->value;

		float time = sqrt( 160 / (0.5 * flGravity));
		float speed = flGravity * time / 160;
		m_vecJumpVelocity = (vecDest - GetAbsOrigin()) * speed;

		return TRUE;
	}

	return FALSE;
}

CBaseEntity *CHAssassin::Kick( void )
{
	TraceResult tr;

	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecStart = GetAbsOrigin();
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * 70);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );

	if( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		return pEntity;
	}

	return NULL;
}

//=========================================================
// CheckMeleeAttack2 - diffusion: kick
//=========================================================
BOOL CHAssassin::CheckMeleeAttack2( float flDot, float flDist )
{
	CBaseMonster *pEnemy = NULL;

	if( m_hEnemy != NULL )
		pEnemy = m_hEnemy->MyMonsterPointer();

	if( !pEnemy )
		return FALSE;

	if( flDist <= 64 && flDot >= 0.7 && pEnemy->Classify() != CLASS_ALIEN_BIOWEAPON && pEnemy->Classify() != CLASS_PLAYER_BIOWEAPON )
	{
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CheckRangeAttack1  - drop a cap in their ass
//=========================================================
BOOL CHAssassin :: CheckRangeAttack1 ( float flDot, float flDist )
{
//	if ( HasConditions(bits_COND_NO_AMMO_LOADED) )
//		return FALSE;
	
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist > 64 && flDist <= 2048 /* && flDot >= 0.5 */ /* && NoFriendlyFire() */ )
	{
		TraceResult	tr;

		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
//		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget(vecSrc), dont_ignore_monsters, ENT(pev), &tr);
//		if ( tr.flFraction == 1 || tr.pHit == m_hEnemy->edict() )
//			return TRUE;
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, ENT( pev ), &tr );
		if( tr.flFraction == 1.0 )
			return TRUE;
		}

	return FALSE;
}

//=========================================================
// CheckRangeAttack2 - toss grenade if enemy gets in the way and is too close. 
//=========================================================
BOOL CHAssassin :: CheckRangeAttack2 ( float flDot, float flDist )
{
	m_fThrowGrenade = FALSE;
	if ( !FBitSet ( m_hEnemy->pev->flags, FL_ONGROUND ) )
		return FALSE; // don't throw grenades at anything that isn't on the ground!

	// don't get grenade happy unless the player starts to piss you off
	if ( m_iFrustration <= 2)
		return FALSE;

	if ( m_flNextGrenadeCheck < gpGlobals->time && !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 512 /* && flDot >= 0.5 */ /* && NoFriendlyFire() */ )
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition( ), m_hEnemy->Center(), flDist, 0.6f ); // use dist as speed to get there in 1 second

		if ( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;

			return TRUE;
		}
	}

	return FALSE;
}


//=========================================================
// RunAI
//=========================================================
void CHAssassin :: RunAI( void )
{
	CBaseMonster :: RunAI();

	if( HasWeapon(HASS_WPN_TMP) ) // smoke girl
	{
		if( gpGlobals->time > g_nextsmoketime )
			g_smokeallowed = true;
		
		if( g_iSkillLevel > SKILL_EASY )
		{
			// always visible if moving
			if( m_hEnemy == NULL || pev->deadflag != DEAD_NO || m_Activity == ACT_RUN || m_Activity == ACT_WALK || !(pev->flags & FL_ONGROUND) )
				m_iTargetRanderamt = 255;
			else
				m_iTargetRanderamt = 40;

			if( pev->renderamt > m_iTargetRanderamt )
			{
				if( pev->renderamt == 255 )
					EMIT_SOUND( ENT( pev ), CHAN_BODY, "scripts/cloak.wav", 0.2, ATTN_NORM );

				pev->renderamt = Q_max( pev->renderamt - 50, m_iTargetRanderamt );
				pev->rendermode = kRenderTransTexture;
			}
			else if( pev->renderamt < m_iTargetRanderamt )
			{
				pev->renderamt = Q_min( pev->renderamt + 50, m_iTargetRanderamt );
				if( pev->renderamt == 255 )
					pev->rendermode = kRenderNormal;
			}
		}
	}
}


//=========================================================
// StartTask
//=========================================================
void CHAssassin :: StartTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK2:
		if (!m_fThrowGrenade)
			TaskComplete( );
		else
			CBaseMonster :: StartTask ( pTask );
	break;
	case TASK_ASSASSIN_FALL_TO_GROUND:
	break;
//	case TASK_RELOAD:
//		m_IdealActivity = ACT_RELOAD;
//		break;
	default:
		CBaseMonster :: StartTask ( pTask );
	break;
	}
}


//=========================================================
// RunTask 
//=========================================================
void CHAssassin :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ASSASSIN_FALL_TO_GROUND:
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );

		if (m_fSequenceFinished)
		{
			if (GetAbsVelocity().z > 0)
				pev->sequence = LookupSequence( "fly_up" );
			else if (HasConditions ( bits_COND_SEE_ENEMY ))
			{
				pev->sequence = LookupSequence( "fly_attack" );
				pev->frame = 0;
			}
			else
			{
				pev->sequence = LookupSequence( "fly_down" );
				pev->frame = 0;
			}
			
			ResetSequenceInfo( );
			SetYawSpeed();
		}
		if (pev->flags & FL_ONGROUND)
		{
			// ALERT( at_console, "on ground\n");
			TaskComplete( );
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
Schedule_t *CHAssassin :: GetSchedule ( void )
{
//	if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
//		return GetScheduleOfType ( SCHED_ASSASSIN_COVER_AND_RELOAD );
	
	if( HasFlag(F_ENTITY_ONFIRE) )
		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
	
	switch	( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
//			if( (m_cAmmoLoaded < m_iClipSize) && (gpGlobals->time > IdleTime + 5) )
//				return GetScheduleOfType ( SCHED_RELOAD );
	// fallthrough
	case MONSTERSTATE_ALERT:
		{
			if ( HasConditions ( bits_COND_HEAR_SOUND ))
			{
				CSound *pSound;
				pSound = PBestSound();

				ASSERT( pSound != NULL );
				if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );

				if ( pSound && (pSound->m_iType & bits_SOUND_COMBAT) )
					return GetScheduleOfType( SCHED_INVESTIGATE_SOUND );
			}

//			if( (m_cAmmoLoaded < m_iClipSize) && (gpGlobals->time > IdleTime + 3) )
//				return GetScheduleOfType ( SCHED_RELOAD );
		}
		break;

	case MONSTERSTATE_COMBAT:
		{
				// dead enemy
				if ( HasConditions( bits_COND_ENEMY_DEAD|bits_COND_ENEMY_LOST ) )
				{
					// call base class, all code to handle dead enemies is centralized there.
//					IdleTime = gpGlobals->time;
					return CBaseMonster :: GetSchedule();
				}

				// flying?
				if ( pev->movetype == MOVETYPE_TOSS)
				{
					if (pev->flags & FL_ONGROUND)
					{
						// ALERT( at_console, "landed\n");
						// just landed
						pev->movetype = MOVETYPE_STEP;
						return GetScheduleOfType ( SCHED_ASSASSIN_JUMP_LAND );
					}
					else
					{
						// ALERT( at_console, "jump\n");
						// jump or jump/shoot
						if ( m_MonsterState == MONSTERSTATE_COMBAT )
							return GetScheduleOfType ( SCHED_ASSASSIN_JUMP );
						else
							return GetScheduleOfType ( SCHED_ASSASSIN_JUMP_ATTACK );
					}
				}

				if ( HasConditions ( bits_COND_HEAR_SOUND ))
				{
					CSound *pSound;
					pSound = PBestSound();

					ASSERT( pSound != NULL );
					if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) && (pSound->m_entindex != ENTINDEX(edict())) )
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
				}

				if ( HasConditions ( bits_COND_LIGHT_DAMAGE ) || HasConditions ( bits_COND_HEAVY_DAMAGE ) )
					m_iFrustration++;

				// jump player!
				if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
					// ALERT( at_console, "melee attack 1\n");
					return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

				// kick
				if( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				{
				//	ALERT( at_console, "melee attack 2 - kick\n" );
					return GetScheduleOfType( SCHED_MELEE_ATTACK2 );
				}

				// diffusion - likely I can see the enemy through the func_wall glass
				if( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				{
					int choose = RANDOM_LONG( 0, 10 );
					if( choose == 3 )
						return GetScheduleOfType( SCHED_CHASE_ENEMY );
					else
						return GetScheduleOfType( SCHED_COMBAT_FACE );
				}

				// throw grenade
				if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) )
					// ALERT( at_console, "range attack 2\n");
					return GetScheduleOfType ( SCHED_RANGE_ATTACK2 );

				// spotted
				if ( HasConditions ( bits_COND_SEE_ENEMY ) && HasConditions ( bits_COND_ENEMY_FACING_ME ) )
				{
					// ALERT( at_console, "exposed\n");
					if( RANDOM_LONG(0,3) == 0)
					{
						m_iFrustration++;
						return GetScheduleOfType ( SCHED_ASSASSIN_EXPOSED );
					}
					else
						return GetScheduleOfType ( SCHED_RANGE_ATTACK1 );
				}

				// can attack
				if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
				{
					m_iFrustration = 0;
					return GetScheduleOfType ( SCHED_RANGE_ATTACK1 );
				}

				if ( HasConditions ( bits_COND_SEE_ENEMY ) )
					// ALERT( at_console, "face\n");
					return GetScheduleOfType ( SCHED_COMBAT_FACE );

				// new enemy
				if( HasConditions( bits_COND_NEW_ENEMY ) )
				{
					if( HasConditions( bits_COND_ENEMY_FACING_ME ) )
						// ALERT( at_console, "take cover\n");
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
					else if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
						return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
					else
						return GetScheduleOfType( SCHED_CHASE_ENEMY );
				}

				// chase more often on harder difficulty
				if( g_iSkillLevel == SKILL_EASY )
				{
					if( RANDOM_LONG( 0, 7 ) == 3 )
						return GetScheduleOfType( SCHED_CHASE_ENEMY );
				}
				else if( g_iSkillLevel == SKILL_MEDIUM )
				{
					if( RANDOM_LONG( 0, 4 ) == 3 )
						return GetScheduleOfType( SCHED_CHASE_ENEMY );
				}
				else
				{
					if( RANDOM_LONG( 0, 2 ) == 2 )
						return GetScheduleOfType( SCHED_CHASE_ENEMY );
				}

				// ALERT( at_console, "stand\n");
				return GetScheduleOfType ( SCHED_ALERT_STAND );
		}
		break;
	}

	return CBaseMonster :: GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t* CHAssassin :: GetScheduleOfType ( int Type ) 
{
	// ALERT( at_console, "%d\n", m_iFrustration );
	switch	( Type )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		if (pev->health > 30)
			return slAssassinTakeCoverFromEnemy;
		else
			return slAssassinTakeCoverFromEnemy2;
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		return slAssassinTakeCoverFromBestSound;
	case SCHED_ASSASSIN_EXPOSED:
		return slAssassinExposed;
	case SCHED_FAIL:
		if (m_MonsterState == MONSTERSTATE_COMBAT)
			return slAssassinFail;
		break;
	case SCHED_ALERT_STAND:
		if (m_MonsterState == MONSTERSTATE_COMBAT)
			return slAssassinHide;
		break;
	case SCHED_CHASE_ENEMY:
		return slAssassinHunt;
	case SCHED_MELEE_ATTACK1:
		if (pev->flags & FL_ONGROUND)
		{
			if (m_flNextJump > gpGlobals->time)
			{
				// can't jump yet, go ahead and fail
				return slAssassinFail;
			}
			else
			{
				return slAssassinJump;
			}
		}
		else
		{
			return slAssassinJumpAttack;
		}
	case SCHED_ASSASSIN_JUMP:
	case SCHED_ASSASSIN_JUMP_ATTACK:
		return slAssassinJumpAttack;
	case SCHED_ASSASSIN_JUMP_LAND:
		return slAssassinJumpLand;
//	case SCHED_ASSASSIN_COVER_AND_RELOAD:
//		return &slAssassinHideReload[ 0 ];
//	case SCHED_RANGE_ATTACK1:
//		return slAssassinRangeAttack1;
	}

	return CBaseMonster :: GetScheduleOfType( Type );
}

void CHAssassin::ClearEffects(void)
{

}











//=================================== DIFFUSION - assassin security =================================================

class SecAss : public CHAssassin
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify(void);
	void ClearEffects(void);
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CheckRangeAttack2( float flDot, float flDist );
};

LINK_ENTITY_TO_CLASS( monster_security_assassin , SecAss);

void SecAss :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/hassassin_security.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	if (!pev->health) pev->health = 75;
	pev->max_health = pev->health;
	m_flFieldOfView		= VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_MELEE_ATTACK1 | bits_CAP_DOORS_GROUP;
	pev->friction		= 1;

	m_HackedGunPos		= Vector( 0, 24, 48 );

	AddWeapon(HASS_WPN_TMP);
	SetBodygroup( GUN_GROUP, GUN_TMP );
	// undone - make a possibility to choose weapons in the editor
	// (don't forget to edit cloak mode as it's using only TMP weapon)

	if( HasWeapon(HASS_WPN_TMP) && (g_iSkillLevel != SKILL_EASY) )
	{
		m_iTargetRanderamt	= 40;
		pev->renderamt		= 40;
		pev->rendermode		= kRenderTransTexture;
	}

//	m_iClipSize = 22;
//	m_cAmmoLoaded = m_iClipSize;
//	IdleTime = 0;

	SmokeDropped = false;

	m_vecJumpVelocity = Vector( 0, 0, 500 );

	MonsterInit();
}

void SecAss :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/hassassin_security.mdl");

	PRECACHE_SOUND("weapons/assassin_pistol.wav");
	PRECACHE_SOUND("weapons/assassin_tmp01.wav");
	PRECACHE_SOUND("weapons/assassin_tmp02.wav");
	PRECACHE_SOUND("weapons/assassin_tmp03.wav");
//	PRECACHE_SOUND("weapons/assassin_reload.wav");
	PRECACHE_SOUND( "weapons/punch1.wav" );
	PRECACHE_SOUND( "weapons/punch2.wav" );
	PRECACHE_SOUND( "weapons/punch3.wav" );

	PRECACHE_SOUND( "assassin/ass_death.wav" );
	PRECACHE_SOUND( "assassin/ass_death2.wav" );
	PRECACHE_SOUND( "assassin/ass_death3.wav" );
	PRECACHE_SOUND( "assassin/ass_jump.wav" );
	PRECACHE_SOUND( "assassin/ass_kicking.wav" );
	PRECACHE_SOUND( "assassin/ass_kicking2.wav" );
	PRECACHE_SOUND( "assassin/ass_kicking3.wav" );

	PRECACHE_SOUND("scripts/cloak.wav"); //new cloak sound for Diffusion
	PRECACHE_SOUND( "weapons/smoke_grenade.wav" );
	PRECACHE_SOUND( "weapons/emp_explode.wav" );

	g_nextsmoketime = 0;
	g_smokeallowed = true;
}

int	SecAss :: Classify ( void )
{
	return m_iClass ? m_iClass : 14; // Faction A
}

void SecAss::ClearEffects(void)
{

}

BOOL SecAss::CheckRangeAttack2( float flDot, float flDist )
{
	m_fThrowGrenade = FALSE;
	if( !FBitSet( m_hEnemy->pev->flags, FL_ONGROUND ) )
		return FALSE; // don't throw grenades at anything that isn't on the ground!

	if( g_smokeallowed && !SmokeDropped ) // time to throw smoke
	{
		if( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 512 )
		{
			m_fThrowGrenade = TRUE;
			return TRUE;
		}
	}

	// don't get grenade happy unless the player starts to piss you off
	if( m_iFrustration <= 2 )
		return FALSE;

	if( m_flNextGrenadeCheck < gpGlobals->time && !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 512 )
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition(), m_hEnemy->Center(), flDist, 0.6f ); // use dist as speed to get there in 1 second

		if( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;

			return TRUE;
		}
	}

	return FALSE;
}

void SecAss::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case ASSASSIN_AE_SHOOT1:
		Shoot();
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, GetAbsOrigin(), 192, 0.3, ENTINDEX( edict() ) );
		break;
	case ASSASSIN_AE_GRUNT:
		if( !HasSpawnFlags( SF_MONSTER_GAG ) )
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "assassin/ass_kicking.wav", 1, ATTN_NORM ); break;
			case 1: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "assassin/ass_kicking2.wav", 1, ATTN_NORM ); break;
			case 2: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "assassin/ass_kicking3.wav", 1, ATTN_NORM ); break;
			}
		}
		break;
	case ASSASSIN_AE_DEATH:
		if( !HasSpawnFlags( SF_MONSTER_GAG ) )
		{
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_death.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
			case 1: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_death2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
			case 2: EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_death3.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
			}
		}
		break;
	case ASSASSIN_AE_TOSS1:
	{
		UTIL_MakeVectors( GetAbsAngles() );
		// diffusion - throw a smoke!
		if( g_smokeallowed && !SmokeDropped )
		{
			Vector org = GetAbsOrigin() + gpGlobals->v_forward * 40 + Vector( 0, 0, 32 );
			MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, GetAbsOrigin() );
			WRITE_BYTE( TE_SMOKEGRENADE );
				WRITE_COORD( org.x );
				WRITE_COORD( org.y );
				WRITE_COORD( org.z );
				WRITE_BYTE( 75 ); // num of sprites
				WRITE_BYTE( 2 ); // speed of decay (*0.01)
				WRITE_BYTE( 55 ); // scale
				WRITE_BYTE( 75 ); // randomize position offset of each sprite (+/- 75 here)
			MESSAGE_END();
			SmokeDropped = true;
			g_smokeallowed = false;
			g_nextsmoketime = gpGlobals->time + 15.0f;
		}
		else
		{
			CBaseEntity *pGrenade = CGrenade::ShootTimed( pev, GetAbsOrigin() + gpGlobals->v_forward * 34 + Vector( 0, 0, 32 ), m_vecTossVelocity, 2.0, true ); // EMP
			if( pGrenade )
			{
				SET_MODEL( pGrenade->edict(), "models/weapons/w_grenade.mdl" );
				pGrenade->pev->body = 1; // change to body without pin
				pGrenade->pev->skin = 1;
			}
		}

		m_flNextGrenadeCheck = gpGlobals->time + 6;// wait six seconds before even looking again to see if a grenade can be thrown.
		m_fThrowGrenade = FALSE;
		// !!!LATER - when in a group, only try to throw grenade if ordered.
	}
	break;
	case ASSASSIN_AE_JUMP:
	{
		// ALERT( at_console, "jumping");
		UTIL_MakeAimVectors( GetAbsAngles() );
		pev->movetype = MOVETYPE_TOSS;
		pev->flags &= ~FL_ONGROUND;
		SetAbsVelocity( m_vecJumpVelocity );
		m_flNextJump = gpGlobals->time + 3.0;
		if( !HasSpawnFlags( SF_MONSTER_GAG ) && m_MonsterState == MONSTERSTATE_COMBAT ) // be silent when idle
			EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "assassin/ass_jump.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) );
	}
	return;
	/*	case ASSASSIN_AE_RELOAD:
			EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/assassin_reload.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_iClipSize;
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			UTIL_MakeAimVectors( GetAbsAngles() );
			pev->movetype = MOVETYPE_TOSS;
			pev->flags &= ~FL_ONGROUND;
			SetAbsVelocity( m_vecJumpVelocity * 0.6 );
			break;*/
	case ASSASSIN_AE_KICK:
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
			pHurt->TakeDamage( pev, pev, 5, DMG_CLUB );
			UTIL_ScreenShakeLocal( pHurt, pHurt->GetAbsOrigin(), 1.5, 100, 0.5, 0, true );
		}
	}
	break;
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}






// ============================== ALIEN ASSASSIN =============================================================

#define SECASS_ALIEN_EYE	"sprites/alien_eye.spr"

class SecAssAlien : public CHAssassin
{
	DECLARE_CLASS(SecAssAlien, CHAssassin);
public:
	void Spawn( void );
	void Precache( void );
	int Classify(void);
	void ClearEffects(void);
	void Shoot(void);
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunAI(void);
	void GibMonster( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );

	void TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	void Killed( entvars_t *pevAttacker, int iGib );

	CSprite *SecAssAlienEye;
	int m_iTrail;
	float LastSwooshTime;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( monster_alien_assassin , SecAssAlien);

BEGIN_DATADESC( SecAssAlien )
	DEFINE_FIELD( m_flLastShot, FIELD_TIME ),
	DEFINE_FIELD( m_flDiviation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextJump, FIELD_TIME ),
	DEFINE_FIELD( m_vecJumpVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_fThrowGrenade, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iTargetRanderamt, FIELD_INTEGER ),
	DEFINE_FIELD( m_iFrustration, FIELD_INTEGER ),
	DEFINE_FIELD( SecAssAlienEye, FIELD_CLASSPTR ),
	DEFINE_FIELD( LastSwooshTime, FIELD_TIME ),
//	DEFINE_FIELD( m_iClipSize, FIELD_INTEGER ),
//	DEFINE_FIELD( IdleTime, FIELD_TIME ),
END_DATADESC()

void SecAssAlien :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/hassassin_alien.mdl");

	PRECACHE_SOUND("scripts/cloak.wav"); //new cloak sound for Diffusion

	PRECACHE_MODEL( SECASS_ALIEN_EYE );

	PRECACHE_SOUND("drone/drone_hit1.wav");
	PRECACHE_SOUND("drone/drone_hit2.wav");
	PRECACHE_SOUND("drone/drone_hit3.wav");
	PRECACHE_SOUND("drone/drone_hit4.wav");
	PRECACHE_SOUND("drone/drone_hit5.wav");

	PRECACHE_SOUND("weapons/alien_ass_shot.wav");
	PRECACHE_SOUND("weapons/alien_ass_swoosh.wav");

	UTIL_PrecacheOther( "shock_beam" );
	UTIL_PrecacheOther( "alien_super_ball" );

	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
}

void SecAssAlien :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/hassassin_alien.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= DONT_BLEED;
	pev->effects		= 0;
	if (!pev->health) pev->health = RANDOM_LONG(180,210);
	pev->max_health = pev->health;
	m_flFieldOfView		= VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_MELEE_ATTACK1 | bits_CAP_DOORS_GROUP;
	pev->friction		= 1;

	m_HackedGunPos		= Vector( 0, 24, 48 );

	if( HasWeapon(HASS_WPN_TMP) && (g_iSkillLevel != SKILL_EASY) )
	{
		m_iTargetRanderamt	= 40;
		pev->renderamt		= 40;
		pev->rendermode		= kRenderTransTexture;
	}

	AddWeapon(HASS_WPN_TMP);
//	SetBodygroup( GUN_GROUP, GUN_TMP );
	// undone - make a possibility to choose weapons in the editor
	// (don't forget to edit cloak mode as it's using only TMP weapon)

//	m_iClipSize = 22;
//	m_cAmmoLoaded = m_iClipSize;
//	IdleTime = 0;

	SecAssAlienEye = CSprite::SpriteCreate( SECASS_ALIEN_EYE, GetAbsOrigin(), TRUE );
	if( SecAssAlienEye )
	{
		SecAssAlienEye->SetAttachment( edict(), 4 );
		SecAssAlienEye->SetScale( 0.1 );
		SecAssAlienEye->SetTransparency( kRenderGlow, 0, 255, 25, 25, kRenderFxNoDissipation );
		SecAssAlienEye->SetFadeDistance( pev->iuser4 );
	}

	LastSwooshTime = gpGlobals->time;

	SetFlag(F_METAL_MONSTER);

	MonsterInit();
}

int	SecAssAlien :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_ALIEN_MILITARY;
}

void SecAssAlien :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case ASSASSIN_AE_SHOOT1:
		Shoot( );
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 512, 0.3, ENTINDEX(edict()) );
		break;
	case ASSASSIN_AE_TOSS1:
		{
			Vector vecStart = GetAbsOrigin() + gpGlobals->v_forward * 34 + Vector( 0, 0, 32 );
			CBaseMonster *pBall = (CBaseMonster *)Create( "alien_super_ball", vecStart, g_vecZero, edict() );
			if( pBall )
			{
				pBall->SetAbsVelocity( ShootAtEnemy( vecStart ) * 100 );
				pBall->m_hEnemy = m_hEnemy;
			}

			m_flNextGrenadeCheck = gpGlobals->time + 10;// wait six seconds before even looking again to see if a grenade can be thrown.
			m_fThrowGrenade = FALSE;
			// !!!LATER - when in a group, only try to throw grenade if ordered.
		}
		break;
	case ASSASSIN_AE_JUMP:
		{
			// ALERT( at_console, "jumping");
			UTIL_MakeAimVectors( GetAbsAngles() );
			pev->movetype = MOVETYPE_TOSS;
			pev->flags &= ~FL_ONGROUND;
			SetAbsVelocity( m_vecJumpVelocity );
			m_flNextJump = gpGlobals->time + 3.0;
		}
		return;
//	case ASSASSIN_AE_RELOAD:
//		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/assassin_reload.wav", 1, ATTN_NORM );
//		m_cAmmoLoaded = m_iClipSize;
//		ClearConditions( bits_COND_NO_AMMO_LOADED );
//		UTIL_MakeAimVectors( GetAbsAngles() );
//		pev->movetype = MOVETYPE_TOSS;
//		pev->flags &= ~FL_ONGROUND;
//		SetAbsVelocity( m_vecJumpVelocity * 0.6 );
//		break;
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

void SecAssAlien :: Shoot ( void )
{
	if (m_hEnemy == NULL)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	if (m_flLastShot + 2 < gpGlobals->time)
	{
		m_flDiviation = 0.10;
	}
	else
	{
		m_flDiviation -= 0.01;
		if (m_flDiviation < 0.02)
			m_flDiviation = 0.02;
	}
	m_flLastShot = gpGlobals->time;

	UTIL_MakeVectors ( GetAbsAngles() );
	CBaseEntity *pShock = CBaseEntity::Create("shock_beam", vecShootOrigin, vecShootDir, edict());
	if( pShock )
	{
		pShock->pev->dmg = g_iSkillLevel * 0.5f; // :)
		pShock->pev->scale = 0.5;
		pShock->pev->velocity = ShootAtEnemy( vecShootOrigin ) * 3000;
		pShock->pev->nextthink = gpGlobals->time;
		ProjectileCount++;
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/alien_ass_shot.wav", 1, ATTN_NORM, 0, RANDOM_LONG(90,110));

	//pev->effects |= EF_MUZZLEFLASH;

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, -angDir.x );

//	m_cAmmoLoaded--;
//	if ( m_cAmmoLoaded <= 0 )
//		SetConditions(bits_COND_NO_AMMO_LOADED);
//	ALERT(at_console, "%3d\n", m_cAmmoLoaded);
}

void SecAssAlien::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
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
	
	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int SecAssAlien :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( HasSpawnFlags( SF_MONSTER_NODAMAGE ) )
		return 0;

	if( HasSpawnFlags( SF_MONSTER_NOPLAYERDAMAGE ) && (pevAttacker->flags & FL_CLIENT) )
		return 0;

	if( pevInflictor->owner == edict() )
		return 0; // diffusion - this should be done better somehow

	return CBaseMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );

}

void SecAssAlien :: Killed( entvars_t *pevAttacker, int iGib )
{
	ClearEffects();

	CBaseMonster::Killed( pevAttacker, iGib );
}

void SecAssAlien::GibMonster( void )
{
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

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

void SecAssAlien :: ClearEffects(void)
{
	if( SecAssAlienEye )
	{
		UTIL_Remove( SecAssAlienEye );
		SecAssAlienEye = NULL;
	}
}

void SecAssAlien::RunAI(void)
{
	if( m_Activity == ACT_RUN )
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 3 ); // life
		WRITE_BYTE( 7 );  // width
		WRITE_BYTE( 252 );   // r
		WRITE_BYTE( 255 );   // g
		WRITE_BYTE( 215 );   // b
		WRITE_BYTE( 50 );	// brightness
		MESSAGE_END();

		if( gpGlobals->time > LastSwooshTime + 3 )
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/alien_ass_swoosh.wav", 0.5, ATTN_STATIC, 0, RANDOM_LONG(80,120));
			LastSwooshTime = gpGlobals->time;
		}
	}

	if( ProjectileCount > 15 )
	{
		ProjectileCount = 0;
		NextAttackTime = gpGlobals->time + 5;
	}

	CBaseMonster :: RunAI();
}

BOOL SecAssAlien :: CheckRangeAttack2 ( float flDot, float flDist )
{
	m_fThrowGrenade = FALSE;

	if( m_flNextGrenadeCheck < gpGlobals->time && !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist >= 300 && flDot >= 0.5 )
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition(), m_hEnemy->Center(), flDist, 0.6f ); // use dist as speed to get there in 1 second

		if( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;

			return TRUE;
		}
	}

	return FALSE;
}

BOOL SecAssAlien::CheckMeleeAttack1( float flDot, float flDist )
{
	// no jumps
	return FALSE;
}

Schedule_t *SecAssAlien::GetSchedule( void )
{
	//	if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
	//		return GetScheduleOfType ( SCHED_ASSASSIN_COVER_AND_RELOAD );
	if( gpGlobals->time < NextAttackTime )
		ClearConditions( bits_COND_CAN_RANGE_ATTACK1 );

	if( HasFlag( F_ENTITY_ONFIRE ) )
		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
	
	switch( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
//			if( (m_cAmmoLoaded < m_iClipSize) && (gpGlobals->time > IdleTime + 5) )
//				return GetScheduleOfType ( SCHED_RELOAD );
	// fallthrough
	case MONSTERSTATE_ALERT:
	{
		if( HasConditions( bits_COND_HEAR_SOUND ) )
		{
			CSound *pSound;
			pSound = PBestSound();

			ASSERT( pSound != NULL );
			if( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );

			if( pSound && (pSound->m_iType & bits_SOUND_COMBAT) )
				return GetScheduleOfType( SCHED_INVESTIGATE_SOUND );
		}

		//			if( (m_cAmmoLoaded < m_iClipSize) && (gpGlobals->time > IdleTime + 3) )
		//				return GetScheduleOfType ( SCHED_RELOAD );
	}
	break;

	case MONSTERSTATE_COMBAT:
	{
		// dead enemy
		if( HasConditions( bits_COND_ENEMY_DEAD | bits_COND_ENEMY_LOST ) )
		{
			// call base class, all code to handle dead enemies is centralized there.
//					IdleTime = gpGlobals->time;
			return CBaseMonster::GetSchedule();
		}

		// flying?
		if( pev->movetype == MOVETYPE_TOSS )
		{
			if( pev->flags & FL_ONGROUND )
			{
				// ALERT( at_console, "landed\n");
				// just landed
				pev->movetype = MOVETYPE_STEP;
				return GetScheduleOfType( SCHED_ASSASSIN_JUMP_LAND );
			}
			else
			{
				// ALERT( at_console, "jump\n");
				// jump or jump/shoot
				if( m_MonsterState == MONSTERSTATE_COMBAT )
					return GetScheduleOfType( SCHED_ASSASSIN_JUMP );
				else
					return GetScheduleOfType( SCHED_ASSASSIN_JUMP_ATTACK );
			}
		}

		// hide more often from player on lower difficulty setting
		int choosescale = 10;
		if( g_iSkillLevel == SKILL_EASY )
			choosescale = 3;
		else if( g_iSkillLevel == SKILL_MEDIUM )
			choosescale = 6;

		int choosex = RANDOM_LONG( 0, choosescale );

		if( (gpGlobals->time < NextAttackTime) || (choosex == 3) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );

		if( HasConditions( bits_COND_HEAR_SOUND ) )
		{
			CSound *pSound;
			pSound = PBestSound();

			ASSERT( pSound != NULL );
			if( pSound && (pSound->m_iType & bits_SOUND_DANGER) && (pSound->m_entindex != ENTINDEX( edict() )) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
		}

		if( HasConditions( bits_COND_LIGHT_DAMAGE ) || HasConditions( bits_COND_HEAVY_DAMAGE ) )
			m_iFrustration++;

		// jump player!
	//	if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			// ALERT( at_console, "melee attack 1\n");
	//		return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		// kick
		if( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
		{
			//	ALERT( at_console, "melee attack 2 - kick\n" );
			return GetScheduleOfType( SCHED_MELEE_ATTACK2 );
		}

		// diffusion - likely I can see the enemy through the func_wall glass
		if( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			int choose = RANDOM_LONG( 0, 10 );
			if( choose == 3 )
				return GetScheduleOfType( SCHED_CHASE_ENEMY );
			else
				return GetScheduleOfType( SCHED_COMBAT_FACE );
		}

		// throw grenade
		if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
			// ALERT( at_console, "range attack 2\n");
			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

		// spotted
		if( HasConditions( bits_COND_SEE_ENEMY ) && HasConditions( bits_COND_ENEMY_FACING_ME ) )
		{
			// ALERT( at_console, "exposed\n");
		//	if( RANDOM_LONG( 0, 3 ) == 0 )
		//	{
		//		m_iFrustration++;
		//		return GetScheduleOfType( SCHED_ASSASSIN_EXPOSED );
		//	}
		//	else
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}

		// can attack
		if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			m_iFrustration = 0;
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}

		if( HasConditions( bits_COND_SEE_ENEMY ) )
			// ALERT( at_console, "face\n");
			return GetScheduleOfType( SCHED_COMBAT_FACE );

		// new enemy
	//	if( HasConditions( bits_COND_NEW_ENEMY ) )
			// ALERT( at_console, "take cover\n");
	//		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		if( !HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions(bits_COND_CAN_RANGE_ATTACK1) )
		{
			return GetScheduleOfType( SCHED_CHASE_ENEMY );
		}
		
		int choose2 = RANDOM_LONG( 0, 10 );
		if( choose2 > 6 )
			return GetScheduleOfType( SCHED_CHASE_ENEMY );
		else
			return GetScheduleOfType( SCHED_COMBAT_FACE );
	}
	break;
	}

	return CBaseMonster::GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t *SecAssAlien::GetScheduleOfType( int Type )
{
	// ALERT( at_console, "%d\n", m_iFrustration );
	switch( Type )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		if( pev->health > 30 )
			return slAssassinTakeCoverFromEnemy;
		else
			return slAssassinTakeCoverFromEnemy2;
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		return slAssassinTakeCoverFromBestSound;
	case SCHED_ASSASSIN_EXPOSED:
		return slAssassinExposed;
	case SCHED_FAIL:
		if( m_MonsterState == MONSTERSTATE_COMBAT )
			return slAssassinFail;
		break;
//	case SCHED_ALERT_STAND:
//		if( m_MonsterState == MONSTERSTATE_COMBAT )
//			return slAssassinHide;
//		break;
	case SCHED_CHASE_ENEMY:
		return slAssassinHunt2;
	case SCHED_MELEE_ATTACK1:
		if( pev->flags & FL_ONGROUND )
		{
			if( m_flNextJump > gpGlobals->time )
			{
				// can't jump yet, go ahead and fail
				return slAssassinFail;
			}
			else
			{
				return slAssassinJump;
			}
		}
		else
		{
			return slAssassinJumpAttack;
		}
	case SCHED_ASSASSIN_JUMP:
	case SCHED_ASSASSIN_JUMP_ATTACK:
		return slAssassinJumpAttack;
	case SCHED_ASSASSIN_JUMP_LAND:
		return slAssassinJumpLand;
//	case SCHED_ASSASSIN_COVER_AND_RELOAD:
//		return &slAssassinHideReload[ 0 ];
	case SCHED_RANGE_ATTACK1:
		return slAssassinRangeAttack1;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}