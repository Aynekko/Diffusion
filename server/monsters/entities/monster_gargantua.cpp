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
// Gargantua
//=========================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"nodes.h"
#include	"monsters.h"
#include	"game/schedule.h"
#include	"customentity.h"
#include	"weapons/weapons.h"
#include	"effects.h"
#include	"entities/soundent.h"
#include	"decals.h"
#include	"explode.h"
#include	"entities/func_break.h"
#include	"player.h"

//=========================================================
// Gargantua Monster
//=========================================================
const float GARG_ATTACKDIST = 120.0;

// Garg animation events
#define GARG_AE_SLASH_LEFT			1
//#define GARG_AE_BEAM_ATTACK_RIGHT		2	// No longer used
#define GARG_AE_LEFT_FOOT			3
#define GARG_AE_RIGHT_FOOT			4
#define GARG_AE_STOMP			5
#define GARG_AE_BREATHE			6
#define GARG_AE_BULLETS			7
#define STOMP_FRAMETIME			0.015	// gpGlobals->frametime

// Gargantua is immune to any damage but this
//#define GARG_DAMAGE			(DMG_ENERGYBEAM|DMG_CRUSH|DMG_MORTAR|DMG_BLAST)
//#define GARG_EYE_SPRITE_NAME		"sprites/alien_eye.spr"//"sprites/gargeye1.spr"
#define GARG_BEAM_SPRITE_NAME		"sprites/xbeam3.spr"
#define GARG_BEAM_SPRITE2		"sprites/xbeam3.spr"
#define GARG_STOMP_SPRITE_NAME	"sprites/gargeye1.spr"
#define GARG_STOMP_BUZZ_SOUND		"weapons/mine_charge.wav"
#define GARG_FLAME_LENGTH		330
#define GARG_GIB_MODEL		"models/metalplategibs.mdl"

#define ATTN_GARG			(ATTN_NORM)

#define STOMP_SPRITE_COUNT		10

int gStompSprite = 0, gGargGibModel = 0;
void SpawnExplosion( Vector center, float randomRange, float time, int magnitude );

class CSmoker;

// Spiral Effect (moved to customentity.h)
/*
class CSpiral : public CBaseEntity
{
	DECLARE_CLASS( CSpiral, CBaseEntity );
public:
	void Spawn( void );
	void Think( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
	static CSpiral *Create( const Vector &origin, float height, float radius, float duration );
};
*/
LINK_ENTITY_TO_CLASS( streak_spiral, CSpiral );


class CStomp : public CBaseEntity
{
	DECLARE_CLASS( CStomp, CBaseEntity );
public:
	void Spawn( void );
	void Think( void );
	static CStomp *StompCreate( const Vector &origin, const Vector &end, float speed );

private:
// UNDONE: re-use this sprite list instead of creating new ones all the time
//	CSprite		*m_pSprites[ STOMP_SPRITE_COUNT ];
};

LINK_ENTITY_TO_CLASS( garg_stomp, CStomp );
CStomp *CStomp::StompCreate( const Vector &origin, const Vector &end, float speed )
{
	CStomp *pStomp = GetClassPtr( (CStomp *)NULL );
	
	pStomp->SetAbsOrigin( origin );
	Vector dir = (end - origin);
	pStomp->pev->scale = dir.Length();
	pStomp->pev->movedir = dir.Normalize();
	pStomp->pev->speed = speed;
	pStomp->Spawn();
	
	return pStomp;
}

void CStomp::Spawn( void )
{
	pev->nextthink = gpGlobals->time;
	pev->classname = MAKE_STRING("garg_stomp");
	pev->dmgtime = gpGlobals->time;

	pev->framerate = 30;
	pev->model = MAKE_STRING(GARG_STOMP_SPRITE_NAME);
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN( edict(), CHAN_BODY, GARG_STOMP_BUZZ_SOUND, 1, ATTN_NORM, 0, PITCH_NORM * 0.55);
}


#define	STOMP_INTERVAL		0.025

void CStomp::Think( void )
{
	TraceResult tr;

	pev->nextthink = gpGlobals->time + 0.1;

	// Do damage for this frame
	Vector vecStart = GetAbsOrigin();
	vecStart.z += 30;
	Vector vecEnd = vecStart + (pev->movedir * pev->speed * STOMP_FRAMETIME);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit && tr.pHit != pev->owner )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		entvars_t *pevOwner = pev;
		if ( pev->owner )
			pevOwner = VARS(pev->owner);

		if ( pEntity )
			pEntity->TakeDamage( pev, pevOwner, gSkillData.gargantuaDmgStomp, DMG_SONIC );
	}
	
	// Accelerate the effect
	pev->speed = pev->speed + (STOMP_FRAMETIME) * pev->framerate;
//	pev->framerate = pev->framerate + (STOMP_FRAMETIME) * 1500;
	pev->speed = pev->speed * 1.3; // diffusion - thanks BlackShadow
	
	// Move and spawn trails
	while ( gpGlobals->time - pev->dmgtime > STOMP_INTERVAL )
	{
		SetAbsOrigin( GetAbsOrigin() + pev->movedir * pev->speed * STOMP_INTERVAL );

		for ( int i = 0; i < 2; i++ )
		{
			CSprite *pSprite = CSprite::SpriteCreate( GARG_STOMP_SPRITE_NAME, GetAbsOrigin(), TRUE );
			if ( pSprite )
			{
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 500 ), ignore_monsters, edict(), &tr );
				pSprite->SetAbsOrigin( tr.vecEndPos );
				pSprite->SetAbsVelocity( Vector(RANDOM_FLOAT(-200,200),RANDOM_FLOAT(-200,200),175));
				// pSprite->AnimateAndDie( RANDOM_FLOAT( 8.0, 12.0 ) );
				pSprite->pev->nextthink = gpGlobals->time + 0.3;
				pSprite->SetThink( &CBaseEntity::SUB_Remove );
				pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxFadeFast );
			}
		}
		pev->dmgtime += STOMP_INTERVAL;
		// Scale has the "life" of this effect
		pev->scale -= STOMP_INTERVAL * pev->speed;
		if ( pev->scale <= 0 )
		{
			// Life has run out
			UTIL_Remove(this);
			STOP_SOUND( edict(), CHAN_BODY, GARG_STOMP_BUZZ_SOUND );
		}

	}
}


void StreakSplash( const Vector &origin, const Vector &direction, int color, int count, int speed, int velocityRange )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_STREAK_SPLASH );
		WRITE_COORD( origin.x );		// origin
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( direction.x );	// direction
		WRITE_COORD( direction.y );
		WRITE_COORD( direction.z );
		WRITE_BYTE( color );	// Streak color 6
		WRITE_SHORT( count );	// count
		WRITE_SHORT( speed );
		WRITE_SHORT( velocityRange );	// Random velocity modifier
	MESSAGE_END();
}


class CGargantua : public CBaseMonster
{
	DECLARE_CLASS( CGargantua, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void ClearEffects(void);

	// somewhere around his guns, otherwise he will shoot walls
	// BUGBUG setting pev->view_ofs in Spawn does NOT work!
	Vector EyePosition( ) { return GetAbsOrigin() + Vector(0,0,100); }; 

	BOOL CheckMeleeAttack1( float flDot, float flDist );		// Swipe
	BOOL CheckRangeAttack1( float flDot, float flDist );		// Rockets
	BOOL CheckRangeAttack2( float flDot, float flDist );		// bullets
	void SetObjectCollisionBox( void )
	{
		pev->absmin = GetAbsOrigin() + Vector( -80, -80, 0 );
		pev->absmax = GetAbsOrigin() + Vector( 80, 80, 214 );
	}

	Schedule_t *GetScheduleOfType( int Type );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );

	void Killed( entvars_t *pevAttacker, int iGib );
	void DeathEffect( void );

	void RocketAttack(void);
	void RunAI(void);
	void FlameControls( float angleX, float angleY );

	int ShootAttachment;
	float ShootTime; // the time when the bullet attack can start
	bool IsShooting; // shooting bullets right now
	float ShootStartTime; // when robo starts shooting
	void ShootBullets(void);

	int m_iSoundState;
	float LastSparkTime;
	bool LastWarningSound; // don't save

	DECLARE_DATADESC();
	CUSTOM_SCHEDULES;

private:
	static const char *pAttackHitSounds[];
	static const char *pBeamAttackSounds[];
	static const char *pAttackMissSounds[];
	static const char *pRicSounds[];
	static const char *pFootSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pStompSounds[];
	static const char *pBreatheSounds[];

	CBaseEntity* GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);

	float		m_seeTime;		// Time to attack (when I see the enemy, I set this)
	float		m_flameTime;		// Time of next flame attack
	float		m_painSoundTime;		// Time of next pain sound
	float		m_streakTime;		// streak timer (don't send too many)
	float		m_flameX;			// Flame thrower aim
	float		m_flameY;			
};

LINK_ENTITY_TO_CLASS( monster_gargantua, CGargantua );

BEGIN_DATADESC( CGargantua )
//	DEFINE_FIELD( m_pEyeGlow, FIELD_CLASSPTR ),
//	DEFINE_FIELD( m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( m_seeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flameTime, FIELD_TIME ),
	DEFINE_FIELD( m_streakTime, FIELD_TIME ),
	DEFINE_FIELD( m_painSoundTime, FIELD_TIME ),
	DEFINE_FIELD( ShootTime, FIELD_TIME ),
	DEFINE_FIELD( ShootStartTime, FIELD_TIME ),
	DEFINE_FIELD( IsShooting, FIELD_BOOLEAN ),
	DEFINE_FIELD( LastSparkTime, FIELD_TIME ),
//	DEFINE_AUTO_ARRAY( m_pFlame, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_flameX, FIELD_FLOAT ),
	DEFINE_FIELD( m_flameY, FIELD_FLOAT ),
//	DEFINE_FUNCTION( ShootBullets ),
//	DEFINE_FUNCTION( RocketAttack ),
END_DATADESC()

const char *CGargantua::pAttackHitSounds[] = 
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CGargantua::pBeamAttackSounds[] = 
{
	"garg/gar_flameoff1.wav",
	"garg/gar_flameon1.wav",
	"garg/gar_flamerun1.wav",
};


const char *CGargantua::pAttackMissSounds[] = 
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CGargantua::pRicSounds[] = 
{
#if 0
//	"weapons/ric1.wav",
//	"weapons/ric2.wav",
//	"weapons/ric3.wav",
//	"weapons/ric4.wav",
//	"weapons/ric5.wav",
	"buttons/spark1.wav", // diffusion - new hit sounds since he's now taking damage from bullets
	"buttons/spark2.wav",
	"buttons/spark3.wav",
	
#else
	"buttons/spark1.wav",
	"buttons/spark2.wav",
	"buttons/spark3.wav",
/*	"debris/metal4.wav",
	"debris/metal6.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",    */
#endif
};

const char *CGargantua::pFootSounds[] = 
{
	"robo/step1.wav",
	"robo/step2.wav",
	"robo/step3.wav",
	"robo/step4.wav",
};


const char *CGargantua::pIdleSounds[] = 
{
	"garg/gar_idle1.wav",
	"garg/gar_idle2.wav",
	"garg/gar_idle3.wav",
	"garg/gar_idle4.wav",
	"garg/gar_idle5.wav",
};


const char *CGargantua::pAttackSounds[] = 
{
	"garg/gar_attack1.wav",
	"garg/gar_attack2.wav",
	"garg/gar_attack3.wav",
};

const char *CGargantua::pAlertSounds[] = 
{
	"garg/gar_alert1.wav",
	"garg/gar_alert2.wav",
	"garg/gar_alert3.wav",
};

const char *CGargantua::pPainSounds[] = 
{
	"garg/gar_pain1.wav",
	"garg/gar_pain2.wav",
	"garg/gar_pain3.wav",
};

const char *CGargantua::pStompSounds[] = 
{
	"garg/gar_stomp1.wav",
};

const char *CGargantua::pBreatheSounds[] = 
{
	"garg/gar_breathe1.wav",
	"garg/gar_breathe2.wav",
	"garg/gar_breathe3.wav",
};
//=========================================================
// AI Schedules Specific to this monster
//=========================================================
#if 0
enum
{
	SCHED_ = LAST_COMMON_SCHEDULE + 1,
};
#endif

enum
{
	TASK_SOUND_ATTACK = LAST_COMMON_TASK + 1,
	TASK_FLAME_SWEEP,
};
/*
Task_t	tlGargFlame[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SOUND_ATTACK,		(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_SIGNAL1	},
	{ TASK_SET_ACTIVITY,		(float)ACT_MELEE_ATTACK2 },
	{ TASK_FLAME_SWEEP,			(float)4.5		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
	{ TASK_WAIT,				(float)0.5		},
};

Schedule_t	slGargFlame[] =
{
	{ 
		tlGargFlame,
		SIZEOFARRAY ( tlGargFlame ), 
		0,
		0,
		"GargFlame"
	},
};
*/

// primary melee attack
Task_t	tlGargSwipe[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
};

Schedule_t	slGargSwipe[] =
{
	{ 
		tlGargSwipe,
		SIZEOFARRAY ( tlGargSwipe ), 
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"GargSwipe"
	},
};

const int BigRoboHealth[] =
{
	0,
	2000,
	2500,
	3000
};


DEFINE_CUSTOM_SCHEDULES( CGargantua )
{
//	slGargFlame,
	slGargSwipe,
}; IMPLEMENT_CUSTOM_SCHEDULES( CGargantua, CBaseMonster );

//=========================================================
// Spawn
//=========================================================
void CGargantua :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/npc/cobra.mdl");

	UTIL_SetSize( pev, Vector( -20, -20, 0 ), Vector( 20, 20, 160 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= DONT_BLEED;
	if( !pev->health )
		pev->health = BigRoboHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	//pev->view_ofs		= Vector ( 0, 0, 96 );// taken from mdl file
	m_flFieldOfView = VIEW_FIELD_WIDE;// width of forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();

	HealthBarType = 2; // the biggest bar

	m_seeTime = gpGlobals->time + 5;
	m_flameTime = gpGlobals->time + 2;

	IsShooting = false;
	ShootTime = gpGlobals->time + 2;

	SetFlag( F_MONSTER_CANT_LOSE_ENEMY | F_METAL_MONSTER | F_MONSTER_BIGNODE );

	m_afCapability &= ~bits_CAP_CANSEEFLASHLIGHT;

	m_flDistTooFar = 2200;
}


//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGargantua :: Precache()
{
	int i;

	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/npc/cobra.mdl");

	gStompSprite = PRECACHE_MODEL( GARG_STOMP_SPRITE_NAME );
	gGargGibModel = PRECACHE_MODEL( GARG_GIB_MODEL );
	PRECACHE_SOUND( GARG_STOMP_BUZZ_SOUND );

	for ( i = 0; i < SIZEOFARRAY( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

//	for ( i = 0; i < SIZEOFARRAY( pBeamAttackSounds ); i++ )
//		PRECACHE_SOUND((char *)pBeamAttackSounds[i]);

	for ( i = 0; i < SIZEOFARRAY( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < SIZEOFARRAY( pRicSounds ); i++ )
		PRECACHE_SOUND((char *)pRicSounds[i]);

	for ( i = 0; i < SIZEOFARRAY( pFootSounds ); i++ )
		PRECACHE_SOUND((char *)pFootSounds[i]);

//	for ( i = 0; i < SIZEOFARRAY( pIdleSounds ); i++ )
//		PRECACHE_SOUND((char *)pIdleSounds[i]);

	for ( i = 0; i < SIZEOFARRAY( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

//	for ( i = 0; i < SIZEOFARRAY( pPainSounds ); i++ )
//		PRECACHE_SOUND((char *)pPainSounds[i]);

	for ( i = 0; i < SIZEOFARRAY( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < SIZEOFARRAY( pStompSounds ); i++ )
		PRECACHE_SOUND((char *)pStompSounds[i]);

//	for ( i = 0; i < SIZEOFARRAY( pBreatheSounds ); i++ )
//		PRECACHE_SOUND((char *)pBreatheSounds[i]);

	PRECACHE_SOUND("drone/drone_hit1.wav");
	PRECACHE_SOUND("drone/drone_hit2.wav");
	PRECACHE_SOUND("drone/drone_hit3.wav");
	PRECACHE_SOUND("drone/drone_hit4.wav");
	PRECACHE_SOUND("drone/drone_hit5.wav");

	PRECACHE_SOUND("robo/shoot1.wav");
	PRECACHE_SOUND( "robo/shoot2.wav" );
	PRECACHE_SOUND( "robo/shoot3.wav" );
	PRECACHE_SOUND("robo/idle.wav");
	PRECACHE_SOUND("robo/warning1.wav");
	PRECACHE_SOUND("robo/warning2.wav");

	UTIL_PrecacheOther( "robo_rocket2" );
}

void CGargantua :: RunAI( void )
{	
	if( IsShooting && (gpGlobals->time > ShootStartTime + 5) )
	{
		IsShooting = false;
		ShootTime = gpGlobals->time + 1;
		ShootStartTime = 0;
	}

	if( IsAlive() && (pev->health <= pev->max_health * 0.2f) )
	{
		if( gpGlobals->time > LastSparkTime )
		{
			UTIL_DoSpark(pev, EyePosition());
			LastSparkTime = gpGlobals->time + RANDOM_FLOAT(0.5,2.5);
		}

		if( !LastWarningSound )
		{
			EMIT_SOUND( edict(), CHAN_STATIC, "robo/warning2.wav", 1, ATTN_NORM );
			LastWarningSound = true;
		}
	}
	
	CBaseMonster :: RunAI();
}

void CGargantua :: ShootBullets ( void )
{
	if (m_hEnemy == NULL)
		return;

	if( !IsShooting )
	{
		ShootStartTime = gpGlobals->time;
		IsShooting = true;
	}

	MakeIdealYaw( m_hEnemy->GetAbsOrigin() );
	ChangeYaw ( pev->yaw_speed );

	Vector vecEnemyOrigin = m_hEnemy->GetAbsOrigin();
	
	Vector	vecGunPos = GetGunPosition();
	Vector	vecGunAngles = vecEnemyOrigin - GetAbsOrigin();

	// switch between hands each shot
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
	
	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecStart = vecGunPos;// + gpGlobals->v_forward * 35;
	Vector vecAim = ShootAtEnemy( vecStart );
	vecGunAngles = UTIL_VecToAngles(vecAim);

	// MAKE 3 SOUNDS ON CLIENT !!!!!
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "robo/shoot1.wav", 1, 0.4, 0, RANDOM_LONG(90,110));

	int BulletDmg = 3;

	if( g_iSkillLevel == SKILL_MEDIUM )
		BulletDmg = 4;
	else if( g_iSkillLevel == SKILL_HARD )
		BulletDmg = 5;

	FireBullets(1, vecStart, vecAim, VECTOR_CONE_3DEGREES, 4096, BULLET_MONSTER_12MM, 1, BulletDmg );
	
	pev->effects |= EF_MUZZLEFLASH;

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 768, 0.3, ENTINDEX(edict()) );

	Vector angles = g_vecZero;

	Vector org = GetAbsOrigin();
	org.z += 64;
	Vector dir = m_hEnemy->BodyTarget(org) - org;
	angles = UTIL_VecToAngles( dir );
	angles.y -= GetAbsAngles().y;

	FlameControls( angles.x, angles.y );
}

void CGargantua::RocketAttack(void)
{
	if (m_hEnemy == NULL)
		return;

	MakeIdealYaw( m_hEnemy->GetAbsOrigin() );
	ChangeYaw ( pev->yaw_speed );
	
	Vector	vecGunPos;
	Vector	vecGunAngles;

	// switch between hands each shot
	if( ShootAttachment == 0 )
	{
		GetAttachment(0, vecGunPos, vecGunAngles);
		ShootAttachment = 1;
	}
	else if( ShootAttachment == 1 )
	{
		GetAttachment(3, vecGunPos, vecGunAngles);
		ShootAttachment = 0;
	}
	
	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecStart = vecGunPos + gpGlobals->v_forward * 35;
	Vector vecAim = ShootAtEnemy( vecStart );
	vecGunAngles = UTIL_VecToAngles(vecAim);

	CBaseMonster *pRocket = (CBaseMonster*)Create( "robo_rocket2", vecStart, vecGunAngles, edict() );
	
	if (pRocket)
	{
		if( m_hEnemy != NULL )
			pRocket->m_hEnemy = m_hEnemy;
		pRocket->SetAbsVelocity( GetAbsVelocity() + gpGlobals->v_forward * 100 );
	}

	Vector angles = g_vecZero;
	if ( m_hEnemy != NULL )
	{
		Vector org = GetAbsOrigin();
		org.z += 64;
		Vector dir = m_hEnemy->BodyTarget(org) - org;
		angles = UTIL_VecToAngles( dir );
		angles.y -= GetAbsAngles().y;
	}

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 1024, 0.3, ENTINDEX(edict()) );

	FlameControls( angles.x, angles.y );
}

void CGargantua :: FlameControls( float angleX, float angleY )
{
	if ( angleY < -180 )
		angleY += 360;
	else if ( angleY > 180 )
		angleY -= 360;

	if ( angleY < -45 )
		angleY = -45;
	else if ( angleY > 45 )
		angleY = 45;

	m_flameX = UTIL_ApproachAngle( angleX, m_flameX, 4 );
	m_flameY = UTIL_ApproachAngle( angleY, m_flameY, 8 );
	SetBoneController( 0, m_flameY );
	SetBoneController( 1, m_flameX );
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CGargantua :: Classify ( void )
{
	return m_iClass ? m_iClass : 14; // Faction A
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGargantua :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 60;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 45;
		break;
	case ACT_WALK:
	case ACT_RUN:
		ys = 60;
		break;

	default:
		ys = 60;
		break;
	}

	pev->yaw_speed = ys;
}


void CGargantua::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
//	ALERT( at_aiconsole, "CGargantua::TraceAttack\n");

	if ( !IsAlive() )
	{
		CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
		return;
	}

	// UNDONE: Hit group specific damage?
	if ( bitsDamageType & DMG_BLAST )  // diffusion - was bitsDamageType & (GARG_DAMAGE|DMG_BLAST)
	{
		if ( m_painSoundTime < gpGlobals->time )
		{
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "robo/warning1.wav", 1.0, 0.4, 0, PITCH_NORM );
			//UTIL_Sparks ( ptr->vecEndPos );
			m_painSoundTime = gpGlobals->time + RANDOM_FLOAT( 5,10 );
		}	
	}

	if ( pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0,100) < 20) )
	{
		UTIL_Sparks ( ptr->vecEndPos );
		pev->dmgtime = gpGlobals->time;
		if ( RANDOM_LONG(0,100) < 25 )
			EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, pRicSounds[ RANDOM_LONG(0,SIZEOFARRAY(pRicSounds)-1) ], 1.0, ATTN_NORM, 0, PITCH_NORM );
	}

	switch(RANDOM_LONG(0,4))
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}

/*
	bitsDamageType &= GARG_DAMAGE;

	if ( bitsDamageType == 0)
	{
		if ( pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0,100) < 20) )
		{
//			UTIL_Ricochet ( ptr->vecEndPos );
			pev->dmgtime = gpGlobals->time;
 		if ( RANDOM_LONG(0,100) < 25 )
				EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, pRicSounds[ RANDOM_LONG(0,SIZEOFARRAY(pRicSounds)-1) ], 1.0, ATTN_NORM, 0, PITCH_NORM );
		}
		flDamage = 0;
	}
*/
	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CGargantua::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
//	ALERT( at_aiconsole, "CGargantua::TakeDamage\n");

	if (pevInflictor->owner == edict())
		return 0;

	if( bitsDamageType & DMG_SLASH ) // knife xD
		return 0;

	if ( IsAlive() )
	{
//		if ( !(bitsDamageType & GARG_DAMAGE) )
//			flDamage *= 0.01;
		// diffusion - big robot can be damaged with everything
		if( bitsDamageType & DMG_BLAST )
		{
			flDamage *= 3;
			SetConditions( bits_COND_LIGHT_DAMAGE );
		}

	//	if ( bitsDamageType & DMG_BULLET )
	//		flDamage *= RANDOM_FLOAT( 0.5, 0.75 );

		if( FClassnameIs( pevInflictor, "crossbow_bolt") )
			flDamage *= 0.1;
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}


void CGargantua::DeathEffect( void )
{
	int i;
	UTIL_MakeVectors(GetAbsAngles());
	Vector deathPos = GetAbsOrigin() + gpGlobals->v_forward * 100;

	// Create a spiral of streaks
	CSpiral::Create( deathPos, (pev->absmax.z - pev->absmin.z) * 0.6, 125, 1.5 );

	Vector position = GetAbsOrigin();
	position.z += 32;
	for ( i = 0; i < 7; i+=2 )
	{
		SpawnExplosion( position, 70, (i * 0.3), 60 + (i*20) );
		position.z += 15;
	}

	CBaseEntity *pSmoker = CBaseEntity::Create( "env_smoker", GetAbsOrigin(), g_vecZero, NULL );
	pSmoker->pev->health = 1;	// 1 smoke balls
	pSmoker->pev->scale = 46;	// 4.6X normal size
	pSmoker->pev->dmg = 0;		// 0 radial distribution
	pSmoker->pev->nextthink = gpGlobals->time + 2.5;	// Start in 2.5 seconds
}


void CGargantua::Killed( entvars_t *pevAttacker, int iGib )
{
	CBaseMonster::Killed( pevAttacker, GIB_NEVER );
}

void CGargantua::ClearEffects(void)
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, "robo/idle.wav" );
	STOP_SOUND( ENT(pev), CHAN_VOICE, "robo/warning1.wav" );
	STOP_SOUND( ENT(pev), CHAN_STATIC, "robo/warning2.wav" );
}

//=========================================================
// CheckMeleeAttack1
// Garg swipe attack
//=========================================================
BOOL CGargantua::CheckMeleeAttack1( float flDot, float flDist )
{
//	ALERT(at_aiconsole, "CheckMelee(%f, %f)\n", flDot, flDist);

	if (flDot >= 0.7)
	{
		if (flDist <= GARG_ATTACKDIST)
			return TRUE;
	}
	return FALSE;
}

//=========================================================
// bullets
//=========================================================
BOOL CGargantua::CheckRangeAttack2( float flDot, float flDist )
{
	if( gpGlobals->time > ShootTime )
	{
		if (flDot >= 0.8 && flDist < 4096)
		{
			if( flDist > GARG_ATTACKDIST )
				return TRUE;
			else
				return FALSE;
		}
	}

	return FALSE;
}


//=========================================================
// rockets
//=========================================================
BOOL CGargantua::CheckRangeAttack1( float flDot, float flDist )
{
	if ( gpGlobals->time > m_seeTime )
	{
		if (flDot >= 0.7 && flDist > GARG_ATTACKDIST + 400)
			return TRUE;
	}

	return FALSE;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGargantua::HandleAnimEvent(MonsterEvent_t *pEvent)
{
	switch( pEvent->event )
	{
	case GARG_AE_SLASH_LEFT:
		{
			// HACKHACK!!!
			CBaseEntity *pHurt = GargantuaCheckTraceHullAttack( GARG_ATTACKDIST + 10.0, gSkillData.gargantuaDmgSlash, DMG_SLASH );
			if (pHurt)
			{
				if ( pHurt->pev->flags & FL_MONSTER )
				{
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - gpGlobals->v_right * 100 );
				}
				else if ( pHurt->pev->flags & FL_CLIENT )
				{
					pHurt->pev->punchangle.x = -30; // pitch
					pHurt->pev->punchangle.y = -30;	// yaw
					pHurt->pev->punchangle.z = 30;	// roll
					if( pHurt->pev->flags & FL_ONGROUND )
						pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_up * 500 );
					else
						pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - gpGlobals->v_right * 100 );
				}
				EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,SIZEOFARRAY(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0,15) );
			}
			else // Play a random attack miss sound
				EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,SIZEOFARRAY(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0,15) );

			Vector forward;
			UTIL_MakeVectorsPrivate( GetAbsAngles(), forward, NULL, NULL );
		}
	break;

	case GARG_AE_RIGHT_FOOT:
	case GARG_AE_LEFT_FOOT:
		UTIL_ScreenShake( GetAbsOrigin(), 4.0, 3.0, 1.0, 750 );
		EMIT_SOUND_DYN ( edict(), CHAN_BODY, pFootSounds[ RANDOM_LONG(0,SIZEOFARRAY(pFootSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10,10) );
	break;

	case GARG_AE_STOMP:
	{
		if( m_hEnemy != NULL )
		{
			RocketAttack();
			m_seeTime = gpGlobals->time + 10;
		}
		else
			m_seeTime = gpGlobals->time + 1;
	}
	break;

	case GARG_AE_BULLETS:
	{
		ShootBullets();
	}
	break;

	case GARG_AE_BREATHE:
	//	EMIT_SOUND_DYN ( edict(), CHAN_VOICE, pBreatheSounds[ RANDOM_LONG(0,SIZEOFARRAY(pBreatheSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10,10) );
	break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
	break;
	}
}


//=========================================================
// CheckTraceHullAttack - expects a length to trace, amount 
// of damage to do, and damage type. Returns a pointer to
// the damaged entity in case the monster wishes to do
// other stuff to the victim (punchangle, etc)
// Used for many contact-range melee attacks. Bites, claws, etc.

// Overridden for Gargantua because his swing starts lower as
// a percentage of his height (otherwise he swings over the
// players head)
//=========================================================
CBaseEntity* CGargantua::GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType)
{
	TraceResult tr;

	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecStart = GetAbsOrigin();
	vecStart.z += 64;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * flDist) - (gpGlobals->v_up * flDist * 0.3);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( iDamage > 0 )
			pEntity->TakeDamage( pev, pev, iDamage, iDmgType );

		return pEntity;
	}

	return NULL;
}


Schedule_t *CGargantua::GetScheduleOfType( int Type )
{
	// HACKHACK - turn off the flames if they are on and garg goes scripted / dead
//	if ( FlameIsOn() )
//		FlameDestroy();

	switch( Type )
	{
//		case SCHED_MELEE_ATTACK2:
//			return slGargFlame;
		case SCHED_MELEE_ATTACK1:
			return slGargSwipe;
		break;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}


void CGargantua::StartTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
//	case TASK_FLAME_SWEEP:
	//	if( m_hEnemy != NULL )
	//		ShootBullets();
		/*
		FlameCreate();
		m_flWaitFinished = gpGlobals->time + pTask->flData;
		m_flameTime = gpGlobals->time + 6;
		m_flameX = 0;
		m_flameY = 0;
		*/
//		break;
	case TASK_RUN_PATH:
	{
		// diffusion - switch activity according to the distance
		if( m_hEnemy != NULL )
		{
			if( (m_hEnemy->GetAbsOrigin() - GetAbsOrigin()).Length() > 1000 )
				m_movementActivity = ACT_RUN;
			else
				m_movementActivity = ACT_WALK;
		}
		else
			m_movementActivity = ACT_RUN;

		TaskComplete();
	}
	break;

	case TASK_SOUND_ATTACK:
		if ( RANDOM_LONG(0,100) < 30 )
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,SIZEOFARRAY(pAttackSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM );
		TaskComplete();
		break;
	
	case TASK_DIE:
		m_flWaitFinished = gpGlobals->time + 1.6;
		DeathEffect();
		// FALL THROUGH
	default: 
		CBaseMonster::StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CGargantua::RunTask( Task_t *pTask )
{
	// diffusion - don't update the sound if killed or being removed. this still can get called after ClearEffects, so the sound still plays.
	// frags is a hack to disable the ambient sound of the robo. see map ch2map3
	if( (pev->deadflag == DEAD_NO) && (pev->frags != -1000) && !(pev->flags & FL_KILLME) )
	{
		if (m_iSoundState == 0)
			m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
		else
		{
			static float pitch = 100;
			int lim = 115;
			
			if( m_movementActivity == ACT_WALK )
			{
				pitch++;
				lim = 105;
			}
			else if( m_movementActivity == ACT_RUN )
				pitch += 1.5;
			else
				pitch -= 1.5;

			pitch = bound( 90, pitch, lim );
		//	ALERT( at_console, "pitch %.f\n", pitch );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "robo/idle.wav", 0.3, 0.6, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch ); //0.3 volume, 0.6 radius
		}
	}
	
	switch ( pTask->iTask )
	{
	case TASK_DIE:
		if ( gpGlobals->time > m_flWaitFinished )
		{
			pev->renderfx = kRenderFxExplode;
			pev->rendercolor.x = 255;
			pev->rendercolor.y = 0;
			pev->rendercolor.z = 0;
			StopAnimation();
			pev->nextthink = gpGlobals->time + 0.15;
			SetThink(&CBaseEntity::SUB_Remove );
			int i;
			int parts = MODEL_FRAMES( gGargGibModel );
			for ( i = 0; i < 10; i++ )
			{
				CGib *pGib = GetClassPtr( (CGib *)NULL );

//				pGib->Spawn( GARG_GIB_MODEL );
				
				int bodyPart = 0;
				if ( parts > 1 )
					bodyPart = RANDOM_LONG( 0, pev->body-1 );

				pGib->pev->body = bodyPart;
				pGib->m_bloodColor = DONT_BLEED;
				pGib->m_material = matMetal;
				pGib->SetAbsOrigin( GetAbsOrigin() );
				pGib->SetAbsVelocity( UTIL_RandomBloodVector() * RANDOM_FLOAT( 300, 500 ));
				pGib->SetNextThink( 1.25 );
				pGib->SetThink(&CBaseEntity::SUB_FadeOut );
			}

			Vector vecOrigin = GetAbsOrigin();

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
				WRITE_BYTE( TE_BREAKMODEL);

				// position
				WRITE_COORD( vecOrigin.x );
				WRITE_COORD( vecOrigin.y );
				WRITE_COORD( vecOrigin.z );

				// size
				WRITE_COORD( 200 );
				WRITE_COORD( 200 );
				WRITE_COORD( 128 );

				// velocity
				WRITE_COORD( 0 ); 
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );

				// randomization
				WRITE_BYTE( 100 ); 

				// Model
				WRITE_SHORT( gGargGibModel );	//model id#

				// # of shards
				WRITE_BYTE( RANDOM_LONG(30,40) );

				// duration
				WRITE_BYTE( 20 );// 3.0 seconds

				// flags

				WRITE_BYTE( BREAK_METAL );  // diffusion - not flesh!
			MESSAGE_END();

			return;
		}
		else
			CBaseMonster::RunTask(pTask);
	break;

	case TASK_MELEE_ATTACK2:
		// this code is not used anymore
/*		if( BulletCount > 30 )
		{
			BulletCount = 0;
			TaskComplete();
			ShootTime = gpGlobals->time + 3; // next bullet attack in 3 seconds
			FlameControls( 0, 0 );
			SetBoneController( 0, 0 );
			SetBoneController( 1, 0 );
		}
		else if( BulletCount == -1 )
		{
			BulletCount = 0;
			TaskComplete();
			ShootTime = gpGlobals->time + 1;
			FlameControls( 0, 0 );
			SetBoneController( 0, 0 );
			SetBoneController( 1, 0 );
		}
		else
		{
			if( m_hEnemy != NULL )
			{
				MakeIdealYaw( m_hEnemy->GetAbsOrigin() );
				ChangeYaw ( pev->yaw_speed );
				Vector angles = g_vecZero;
				CBaseEntity *pEnemy = m_hEnemy;
				if ( pEnemy )
				{
					Vector org = GetAbsOrigin();
					org.z += 64;
					Vector dir = pEnemy->BodyTarget(org) - org;
					angles = UTIL_VecToAngles( dir );
					angles.y -= GetAbsAngles().y;
				}

				FlameControls( angles.x, angles.y );

				if( FVisible(m_hEnemy) )
					ShootBullets();
				else
					BulletCount = -1; // enemy isn't visible - stop shooting immediately and go find the enemy
			}
			else
			{
				BulletCount = 0;
				ShootTime = gpGlobals->time + 2;
				TaskComplete();
				FlameControls( 0, 0 );
				SetBoneController( 0, 0 );
				SetBoneController( 1, 0 );
			}	
		}*/
	break;
	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}









class CSmoker : public CBaseEntity
{
	DECLARE_CLASS( CSmoker, CBaseEntity );
public:
	void Spawn( void );
	void Think( void );
};

LINK_ENTITY_TO_CLASS( env_smoker, CSmoker );

void CSmoker::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero );
	pev->effects |= EF_NODRAW;
	SetAbsAngles( g_vecZero );
}


void CSmoker::Think( void )
{
	Vector vecOrigin = GetAbsOrigin();

	// lots of smoke
	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrigin );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecOrigin.x + RANDOM_FLOAT( -pev->dmg, pev->dmg ));
		WRITE_COORD( vecOrigin.y + RANDOM_FLOAT( -pev->dmg, pev->dmg ));
		WRITE_COORD( vecOrigin.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( RANDOM_LONG( pev->scale, pev->scale * 1.1 ));
		WRITE_BYTE( RANDOM_LONG( 8, 14 ) ); // framerate
		WRITE_BYTE( 2 ); // pos randomize
	MESSAGE_END();

	pev->health--;
	if ( pev->health > 0 )
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1, 0.2);
	else
		UTIL_Remove( this );
}


void CSpiral::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero );
	pev->effects |= EF_NODRAW;
	SetAbsAngles( g_vecZero );
}


CSpiral *CSpiral::Create( const Vector &origin, float height, float radius, float duration )
{
	if ( duration <= 0 )
		return NULL;

	CSpiral *pSpiral = GetClassPtr( (CSpiral *)NULL );
	pSpiral->Spawn();
	pSpiral->pev->dmgtime = pSpiral->pev->nextthink;
	pSpiral->SetAbsOrigin( origin );
	pSpiral->pev->scale = radius;
	pSpiral->pev->dmg = height;
	pSpiral->pev->speed = duration;
	pSpiral->pev->health = 0;
	pSpiral->SetAbsAngles( g_vecZero );

	return pSpiral;
}

#define SPIRAL_INTERVAL		0.1 //025

void CSpiral::Think( void )
{
	float time = gpGlobals->time - pev->dmgtime;

	while ( time > SPIRAL_INTERVAL )
	{
		Vector position = GetAbsOrigin();
		Vector direction = Vector( 0, 0, 1 );
		
		float fraction = 1.0 / pev->speed;

		float radius = (pev->scale * pev->health) * fraction;

		position.z += (pev->health * pev->dmg) * fraction;
		Vector vecAngles = GetAbsAngles();
		vecAngles.y = (pev->health * 360 * 8) * fraction;
		SetAbsAngles( vecAngles );
		UTIL_MakeVectors( GetAbsAngles() );
		position = position + gpGlobals->v_forward * radius;
		direction = (direction + gpGlobals->v_forward).Normalize();

		StreakSplash( position, Vector(0,0,1), RANDOM_LONG(8,11), 20, RANDOM_LONG(50,150), 400 );

		// Jeez, how many counters should this take ? :)
		pev->dmgtime += SPIRAL_INTERVAL;
		pev->health += SPIRAL_INTERVAL;
		time -= SPIRAL_INTERVAL;
	}

	pev->nextthink = gpGlobals->time;

	if ( pev->health >= pev->speed )
		UTIL_Remove( this );
}

// HACKHACK Cut and pasted from explode.cpp
void SpawnExplosion( Vector center, float randomRange, float time, int magnitude )
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
	pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

	pExplosion->Spawn();
	pExplosion->SetThink( &CBaseEntity::SUB_CallUseToggle );
	pExplosion->pev->nextthink = gpGlobals->time + time;
}







//=========================================================ROCKETS==========================================================
// this is first rocket "robo_rocket", the other one (which is used) "robo_rocket2" is located in weapon_rpg.cpp
// NOTENOTE used by func_tankrocket now


class CRoboRocket : public CGrenade
{
	DECLARE_CLASS( CRoboRocket, CGrenade );
	void Spawn( void );
	void Precache( void );
	void IgniteThink( void );
	void AccelerateThink( void );

	DECLARE_DATADESC();

	int m_iTrail;
	Vector m_vecForward;
};
LINK_ENTITY_TO_CLASS( robo_rocket, CRoboRocket );

BEGIN_DATADESC( CRoboRocket )
	DEFINE_FIELD( m_vecForward, FIELD_VECTOR ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( AccelerateThink ),
END_DATADESC()

void CRoboRocket :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/HVR.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	RelinkEntity( TRUE );

	SetThink( &CRoboRocket::IgniteThink );
	SetTouch( &CGrenade::ExplodeTouch );

	UTIL_MakeVectors( GetAbsAngles() );
	m_vecForward = gpGlobals->v_forward;
	pev->gravity = 0.5;
	pev->dmg = 50;
	pev->scale = 0.25;

	pev->nextthink = gpGlobals->time + 0.1;
}


void CRoboRocket :: Precache( void )
{
	PRECACHE_MODEL("models/HVR.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND ("weapons/rocket1.wav");
}


void CRoboRocket :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	// pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 4 ); // life
		WRITE_BYTE( 4 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 200 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	// set to accelerate
	SetThink( &CRoboRocket::AccelerateThink );
	pev->nextthink = gpGlobals->time + 0.1;
}


void CRoboRocket :: AccelerateThink( void  )
{
	// check world boundaries
	if ( !IsInWorld( FALSE ))
	{
		UTIL_Remove( this );
		return;
	}

	// accelerate
	float flSpeed = GetAbsVelocity().Length();
	if (flSpeed < 1800)
	{
		SetAbsVelocity( GetAbsVelocity() + m_vecForward * 200 );
	}

	// re-aim
	SetAbsAngles( UTIL_VecToAngles( GetAbsVelocity() ));

	SetNextThink( 0.1 );
}
//===========================================================================================================================


// diffusion - UNUSED CODE

/*
void CGargantua::EyeOn( int level )
{
	m_eyeBrightness = level;	
}

void CGargantua::EyeOff( void )
{
	m_eyeBrightness = 0;
}

void CGargantua::EyeUpdate( void )
{
	if ( m_pEyeGlow )
	{
		m_pEyeGlow->pev->renderamt = UTIL_Approach( m_eyeBrightness, m_pEyeGlow->pev->renderamt, 26 );
		if ( m_pEyeGlow->pev->renderamt == 0 )
			m_pEyeGlow->pev->effects |= EF_NODRAW;
		else
			m_pEyeGlow->pev->effects &= ~EF_NODRAW;
		UTIL_SetOrigin( m_pEyeGlow, GetAbsOrigin() );
	}
}
*/
/*
void CGargantua::StompAttack( void )
{
	TraceResult trace;

	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecStart = GetAbsOrigin() + Vector( 0, 0, 60 ) + 35 * gpGlobals->v_forward;
	Vector vecAim = ShootAtEnemy( vecStart );
	Vector vecEnd = (vecAim * 1024) + vecStart;

	UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, edict(), &trace );
	CStomp::StompCreate( vecStart, trace.vecEndPos, 0 );
	UTIL_ScreenShake( GetAbsOrigin(), 12.0, 100.0, 2.0, 1000 );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pStompSounds[ RANDOM_LONG(0,SIZEOFARRAY(pStompSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10,10) );

	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 20 ), ignore_monsters, edict(), &trace );

	if( trace.flFraction < 1.0 )
		UTIL_DecalTrace( &trace, DECAL_GARGSTOMP1 );
}
*/

/*
void CGargantua :: FlameCreate( void )
{
	int			i;
	Vector		posGun, angleGun;
	TraceResult trace;

	UTIL_MakeVectors( GetAbsAngles() );
	
	for ( i = 0; i < 4; i++ )
	{
		if ( i < 2 )
			m_pFlame[i] = CBeam::BeamCreate( GARG_BEAM_SPRITE_NAME, 240 );
		else
			m_pFlame[i] = CBeam::BeamCreate( GARG_BEAM_SPRITE2, 140 );
		if ( m_pFlame[i] )
		{
			int attach = i%2;
			// attachment is 0 based in GetAttachment
			GetAttachment( attach+1, posGun, angleGun );

			Vector vecEnd = (gpGlobals->v_forward * GARG_FLAME_LENGTH) + posGun;
			UTIL_TraceLine( posGun, vecEnd, dont_ignore_monsters, edict(), &trace );

			m_pFlame[i]->PointEntInit( trace.vecEndPos, entindex() );
			if ( i < 2 )
				m_pFlame[i]->SetColor( 255, 130, 90 );
			else
				m_pFlame[i]->SetColor( 0, 120, 255 );
			m_pFlame[i]->SetBrightness( 190 );
			m_pFlame[i]->SetFlags( BEAM_FSHADEIN );
			m_pFlame[i]->SetScrollRate( 20 );
			// attachment is 1 based in SetEndAttachment
			m_pFlame[i]->SetEndAttachment( attach + 2 );
			CSoundEnt::InsertSound( bits_SOUND_COMBAT, posGun, 384, 0.3 );
		}
	}
	EMIT_SOUND_DYN ( edict(), CHAN_BODY, pBeamAttackSounds[ 1 ], 1.0, ATTN_NORM, 0, PITCH_NORM );
	EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pBeamAttackSounds[ 2 ], 1.0, ATTN_NORM, 0, PITCH_NORM );
}
*/

/*
void CGargantua :: FlameUpdate( void )
{
	int				i;
	static float	offset[2] = { 60, -60 };
	TraceResult		trace;
	Vector			vecStart, angleGun;
	BOOL			streaks = FALSE;

	for ( i = 0; i < 2; i++ )
	{
		if ( m_pFlame[i] )
		{
			Vector vecAim = GetAbsAngles();
			vecAim.x += m_flameX;
			vecAim.y += m_flameY;

			UTIL_MakeVectors( vecAim );

			GetAttachment( i+1, vecStart, angleGun );
			Vector vecEnd = vecStart + (gpGlobals->v_forward * GARG_FLAME_LENGTH); //  - offset[i] * gpGlobals->v_right;

			UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, edict(), &trace );

			m_pFlame[i]->SetStartPos( trace.vecEndPos );
			m_pFlame[i+2]->SetStartPos( (vecStart * 0.6) + (trace.vecEndPos * 0.4) );

			if ( trace.flFraction != 1.0 && gpGlobals->time > m_streakTime )
			{
				StreakSplash( trace.vecEndPos, trace.vecPlaneNormal, 6, 20, 50, 400 );
				streaks = TRUE;
				UTIL_DecalTrace( &trace, DECAL_SMALLSCORCH1 + RANDOM_LONG(0,2) );
			}
			// RadiusDamage( trace.vecEndPos, pev, pev, gSkillData.gargantuaDmgFire, CLASS_ALIEN_MONSTER, DMG_BURN );
			FlameDamage( vecStart, trace.vecEndPos, pev, pev, gSkillData.gargantuaDmgFire, CLASS_ALIEN_MONSTER, DMG_BURN );

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_ELIGHT );
				WRITE_SHORT( entindex( ) + 0x1000 * (i + 2) );		// entity, attachment
				WRITE_COORD( vecStart.x );		// origin
				WRITE_COORD( vecStart.y );
				WRITE_COORD( vecStart.z );
				WRITE_COORD( RANDOM_FLOAT( 32, 48 ) );	// radius
				WRITE_BYTE( 255 );	// R
				WRITE_BYTE( 255 );	// G
				WRITE_BYTE( 255 );	// B
				WRITE_BYTE( 2 );	// life * 10
				WRITE_COORD( 0 ); // decay
			MESSAGE_END();
		}
	}
	if ( streaks )
		m_streakTime = gpGlobals->time;
}
*/

/*
void CGargantua :: FlameDamage( Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	CBaseEntity *pEntity = NULL;
	TraceResult	tr;
	float		flAdjustedDamage;
	Vector		vecSpot;

	Vector vecMid = (vecStart + vecEnd) * 0.5;

	float searchRadius = (vecStart - vecMid).Length();

	Vector vecAim = (vecEnd - vecStart).Normalize( );

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, vecMid, searchRadius )) != NULL)
	{
		if ( pEntity->pev->takedamage != DAMAGE_NO )
		{
			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{// houndeyes don't hurt other houndeyes with their attack
				continue;
			}
			
			vecSpot = pEntity->BodyTarget( vecMid );
		
			float dist = DotProduct( vecAim, vecSpot - vecMid );
			if (dist > searchRadius)
				dist = searchRadius;
			else if (dist < -searchRadius)
				dist = searchRadius;
			
			Vector vecSrc = vecMid + dist * vecAim;

			UTIL_TraceLine ( vecSrc, vecSpot, dont_ignore_monsters, ENT(pev), &tr );

			if ( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() )
			{// the explosion can 'see' this entity, so hurt them!
				// decrease damage for an ent that's farther from the flame.
				dist = ( vecSrc - tr.vecEndPos ).Length();

				if (dist > 64)
				{
					flAdjustedDamage = flDamage - (dist - 64) * 0.4;
					if (flAdjustedDamage <= 0)
						continue;
				}
				else
				{
					flAdjustedDamage = flDamage;
				}

				// ALERT( at_console, "hit %s\n", STRING( pEntity->pev->classname ) );
				if (tr.flFraction != 1.0)
				{
					ClearMultiDamage( );
					pEntity->TraceAttack( pevInflictor, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize( ), &tr, bitsDamageType );
					ApplyMultiDamage( pevInflictor, pevAttacker );
				}
				else
				{
					pEntity->TakeDamage ( pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType );
				}
			}
		}
	}
}
*/

/*
void CGargantua :: FlameDestroy( void )
{
	int i;

	EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pBeamAttackSounds[ 0 ], 1.0, ATTN_NORM, 0, PITCH_NORM );
	for ( i = 0; i < 4; i++ )
	{
		if ( m_pFlame[i] )
		{
			UTIL_Remove( m_pFlame[i] );
			m_pFlame[i] = NULL;
		}
	}
}
*/

/* // diffusion - eye always on now
void CGargantua :: PrescheduleThink( void )
{
	if ( !HasConditions( bits_COND_SEE_ENEMY ) )
	{
		m_seeTime = gpGlobals->time + 5;
		EyeOff();
	}
	else
		EyeOn( 200 );
	
	EyeUpdate();
}
*/