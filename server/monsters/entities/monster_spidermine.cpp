#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons/weapons.h"
#include "monsters.h"

//============================================================================
// Spider mine for Diffusion
// Not active at all until hits the floor first (by spawn or throw)
//============================================================================

#define MDL_SPIDERMINE "models/npc/spidermine.mdl"
#define SND_SPIDERMINE_OFF "spidermine/spider_off.wav"
#define SND_SPIDERMINE_ON "spidermine/spider_on.wav"
#define SND_SPIDERMINE_MOVE "spidermine/spider_move.wav"
#define SND_SPIDERMINE_JUMP "spidermine/spider_jump.wav"

#define ANIM_IDLE 0
#define ANIM_OFF 1
#define ANIM_ON 2
#define ANIM_MOVE 3
#define ANIM_JUMP 4

#define SPIDER_MAX_SPEED 300.0f

#define SF_SPIDERMINE_STARTOFF BIT(0)

static const float fAcceleration[] =
{
	0,
	30.0f,
	36.0f,
	42.0f
};

class CSpiderMine : public CGrenade
{
	DECLARE_CLASS( CSpiderMine, CGrenade );

	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void SpiderMineUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void StandbyThink( void );
	void HuntThink( void );
	void ThrowThink( void );
	void TurnOnThink( void );
	void TurnOffThink( void );
	void SetEnemy( CBaseEntity *enemy );
	CBaseEntity *AcquireTarget( void );
	void ExplodeThink( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void ClearEffects( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );

	DECLARE_DATADESC();

	float fLanded; // should be 1 second on ground to activate - not saved
	float fEnemyNotSighted; // not saved
	bool bProvoked; // becomes provoked when takes damage outside of look radius. Look radius doubles in this case so the mine has less chance to stay put
};

LINK_ENTITY_TO_CLASS( monster_spidermine, CSpiderMine );

BEGIN_DATADESC( CSpiderMine )
	DEFINE_FIELD( bProvoked, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( StandbyThink ),
	DEFINE_FUNCTION( HuntThink ),
	DEFINE_FUNCTION( ThrowThink ),
	DEFINE_FUNCTION( TurnOnThink ),
	DEFINE_FUNCTION( TurnOffThink ),
	DEFINE_FUNCTION( ExplodeThink ),
	DEFINE_FUNCTION( SpiderMineUse ),
END_DATADESC()

void CSpiderMine::Precache( void )
{
	PRECACHE_MODEL( MDL_SPIDERMINE );
	PRECACHE_SOUND( SND_SPIDERMINE_OFF );
	PRECACHE_SOUND( SND_SPIDERMINE_ON );
	PRECACHE_SOUND( SND_SPIDERMINE_MOVE );
	PRECACHE_SOUND( SND_SPIDERMINE_JUMP );
}

void CSpiderMine::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), MDL_SPIDERMINE );
	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 8 ) );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	pev->view_ofs.z = 24;
	if( !pev->health )
		pev->health = g_spidermineHealth[g_iSkillLevel];
	pev->max_health = pev->health;

	pev->dmg = 72.0f;

	if( !m_flDistLook )
		m_flDistLook = 768.0f;

	bProvoked = false;
	m_flFieldOfView = VIEW_FIELD_FULL;
	fLanded = 0.0f;
	fEnemyNotSighted = 0.0f;
	pev->speed = 0.0f;
	m_iState = STATE_OFF;
	pev->sequence = ANIM_IDLE;
	ResetSequenceInfo();
	pev->frame = 0;

	SetThink( &CSpiderMine::ThrowThink );
	SetNextThink( RANDOM_FLOAT( 0.1f, 0.25f ) );
}

int CSpiderMine::Classify( void )
{
	if( m_iClass )
		return m_iClass;

	return CLASS_MACHINE;
}

//=============================================================
// TurnOnThink: this is just for wake up animation to play
//=============================================================
void CSpiderMine::TurnOnThink( void )
{
	SetNextThink( 0.1f );
	StudioFrameAdvance( 0.1f );

	if( m_fSequenceFinished )
	{
		pev->sequence = ANIM_MOVE;
		ResetSequenceInfo();
		pev->frame = 0;
		SetThink( &CSpiderMine::HuntThink );
		SetNextThink( 0.0f );
	}
}

void CSpiderMine::TurnOffThink( void )
{
	SetNextThink( 0.1f );
	StudioFrameAdvance( 0.1f );

	if( m_fSequenceFinished )
	{
		bProvoked = false;
		pev->takedamage = DAMAGE_AIM;
		SetThink( &CSpiderMine::StandbyThink );
		SetNextThink( 0.0f );
	}
}

//=============================================================
// ThrowThink: called from spawn or use.
// First time hits the floor? Then set it as ready.
//=============================================================
void CSpiderMine::ThrowThink( void )
{
	if( pev->flags & FL_ONGROUND )
	{
		fLanded += 0.1f;

		if( fLanded >= 1.0f ) // ready
		{
		//	ALERT( at_console, "SpiderMine: ready.\n" );
			if( !HasSpawnFlags( SF_SPIDERMINE_STARTOFF ) )
			{
				pev->sequence = ANIM_OFF;
				ResetSequenceInfo();
				pev->frame = 0;
				if( pev->owner )
					EMIT_SOUND( edict(), CHAN_BODY, SND_SPIDERMINE_OFF, VOL_NORM, ATTN_NORM );
				SetThink( &CSpiderMine::TurnOffThink );
				SetNextThink( 0.0f );
			}
			else // mine with a name can only be activated by use
			{
				SetThink( NULL );
				DontThink();
			}

			return;
		}
	}
	else
		fLanded = 0.0f;

	// continue falling
	SetNextThink( 0.1f );
}

//=============================================================
// StandbyThink: look for enemies
//=============================================================
void CSpiderMine::StandbyThink( void )
{
	SetNextThink( 0.1f );

	fEnemyNotSighted = 0.0f;
	pev->movetype = MOVETYPE_TOSS;
	m_iState = STATE_ON;

	if( m_hEnemy != NULL ) // might be a leftover from previous think state - lose it
		SetEnemy( NULL );

	SetEnemy( AcquireTarget() );

	if( m_hEnemy != NULL )
	{
		// check enemy's height. If he's too much above, there's no reason to chase
		if( m_hEnemy->GetAbsOrigin().z > GetAbsOrigin().z + 300.0f )
		{
			// BUG: a closer and higher enemy will always be prioritized, which means it will
			// continuosly distract the spider from chasing anyone else. Not sure yet how to fix this.
			// Probably need to add height check into Look() function?
			// Due to nature of the game design, such situations should be extremely rare
			// (mostly lone player against the mines) so I'll leave it for now.
			SetEnemy( NULL );
		}
		else
		{
			pev->sequence = ANIM_ON;
			ResetSequenceInfo();
			pev->frame = 0;
			EMIT_SOUND( edict(), CHAN_BODY, SND_SPIDERMINE_ON, VOL_NORM, ATTN_NORM );
			SetThink( &CSpiderMine::TurnOnThink );
			SetNextThink( 0.0f );
		}
	}

//	ALERT( at_console, "SpiderMine: stand by.\n" );
}

//=============================================================
// HuntThink: run after enemy, check visibility.
// Can't see them for some time? Back to standby mode.
//=============================================================
void CSpiderMine::HuntThink( void )
{
	SetNextThink( 0.1f );
//	ALERT( at_console, "SpiderMine: hunt!\n" );

	if( pev->waterlevel > 0 )
	{
		SetThink( &CSpiderMine::ExplodeThink );
		SetNextThink( 0.0f );
		return;
	}

	pev->movetype = MOVETYPE_PUSHSTEP;
	m_iState = STATE_ON;
	pev->framerate = pev->speed / SPIDER_MAX_SPEED;

	StudioFrameAdvance( 0.1f );
	
	const int iPitch = 50 + (int)(pev->framerate * 50.0f);
	EMIT_SOUND_DYN( edict(), CHAN_BODY, SND_SPIDERMINE_MOVE, bound( 0.1f, pev->framerate, 1.0f ), ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, iPitch );

	if( fEnemyNotSighted > 5.0f )
		SetEnemy( NULL );

	if( !m_hEnemy )
	{
		pev->speed = 0.0f;
		SetAbsVelocity( g_vecZero );
		pev->sequence = ANIM_OFF;
		ResetSequenceInfo();
		pev->frame = 0;
		EMIT_SOUND( edict(), CHAN_BODY, SND_SPIDERMINE_OFF, VOL_NORM, ATTN_NORM );
		SetThink( &CSpiderMine::TurnOffThink );
		SetNextThink( 0.0f );
		return;
	}

	// enemy should be positive from here

	const float dist_to_enemy = (GetAbsOrigin() - m_hEnemy->GetAbsOrigin()).Length2D();
	const bool bEnemyDucking = (m_hEnemy->pev->flags & FL_DUCKING);

	if( !FVisible( m_hEnemy ) )
	{
		fEnemyNotSighted += 0.1f;
		pev->speed = UTIL_Approach( 0.0f, pev->speed, 10.0f );
		SetAbsVelocity( GetAbsVelocity().Normalize() * pev->speed );
		if( fEnemyNotSighted > 3.0f && pev->speed == 0.0f )
		{
			SetAbsVelocity( g_vecZero );
			pev->sequence = ANIM_OFF;
			ResetSequenceInfo();
			pev->frame = 0;
			EMIT_SOUND( edict(), CHAN_BODY, SND_SPIDERMINE_OFF, VOL_NORM, ATTN_NORM );
			SetThink( &CSpiderMine::TurnOffThink );
			SetNextThink( 0.0f );
			return;
		}
	}
	else // seeing the enemy, chase them
	{
		if( dist_to_enemy < (bEnemyDucking ? 100.0f : 150.0f) )
		{
			// too close to the enemy - jump and blow up!
			// don't jump if falling or the enemy is ducking (vent?)
			if( pev->flags & FL_ONGROUND && !bEnemyDucking )
			{
				pev->sequence = ANIM_JUMP;
				ResetSequenceInfo();
				pev->frame = 0;
				SetAbsVelocity( Vector( 0, 0, 300 ) );
				EMIT_SOUND( edict(), CHAN_BODY, SND_SPIDERMINE_JUMP, VOL_NORM, ATTN_NORM );
			}
			SetThink( &CSpiderMine::ExplodeThink );
			SetNextThink( 0.0f );
			return;
		}
		else if( dist_to_enemy > m_flDistLook + 512.0f + (bProvoked ? 1024.0f : 0.0f) )
		{
			// enemy is too far - stop chasing and eventually back to standby
			fEnemyNotSighted += 1.0f;
			pev->speed = UTIL_Approach( 0.0f, pev->speed, 20.0f );
			if( pev->speed == 0.0f )
			{
				SetAbsVelocity( g_vecZero );
				pev->sequence = ANIM_OFF;
				ResetSequenceInfo();
				pev->frame = 0;
				EMIT_SOUND( edict(), CHAN_BODY, SND_SPIDERMINE_OFF, VOL_NORM, ATTN_NORM );
				SetThink( &CSpiderMine::TurnOffThink );
				SetNextThink( 0.0f );
				return;
			}
		}
		else
		{
			// chase!
			fEnemyNotSighted = 0.0f;
			pev->speed = UTIL_Approach( SPIDER_MAX_SPEED, pev->speed, fAcceleration[g_iSkillLevel] );

			Vector vel = GetAbsVelocity();
			Vector vecDir = (m_hEnemy->EyePosition() - GetAbsOrigin()).Normalize();
			Vector NewVelocity = vecDir * pev->speed;
			NewVelocity.z = vel.z;
			SetAbsVelocity( NewVelocity );
		}
	}
}

void CSpiderMine::SetEnemy( CBaseEntity *enemy )
{
	m_hEnemy = enemy;
	if( m_hEnemy )
	{
		SetConditions( bits_COND_SEE_ENEMY );
		FCheckAITrigger();
		ClearConditions( bits_COND_SEE_ENEMY );
	}
}

CBaseEntity *CSpiderMine::AcquireTarget( void )
{
	Look( m_flDistLook * (bProvoked ? 2.0f : 1.0f) );
	return BestVisibleEnemy();
}

void CSpiderMine::ExplodeThink( void )
{
	SetNextThink( 0.1f );
	StudioFrameAdvance( 0.1f );

	if( m_fSequenceFinished )
	{
		DontThink();
		Detonate();
	}
}

void CSpiderMine::SpiderMineUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_iState == STATE_OFF )
	{
		// make sure it falls on ground, so don't call standby mode right away
		pev->flags &= ~FL_ONGROUND;
		fLanded = 0.0f;
		SetThink( &CSpiderMine::ThrowThink );
		SetNextThink( 0.0f );
	}
	else // STATE_ON, disable mine
	{
		SetEnemy( NULL );
		pev->speed = 0.0f;
		SetAbsVelocity( g_vecZero );
		pev->flags &= ~FL_ONGROUND;
		fLanded = 0.0f;
		SetThink( &CSpiderMine::ThrowThink );
		SetNextThink( 0.0f );
	}
}

int CSpiderMine::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( !m_hEnemy )
		bProvoked = true;

	return BaseClass::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CSpiderMine::ClearEffects( void )
{
	EMIT_SOUND( edict(), CHAN_BODY, "common/null.wav", 0.1f, ATTN_IDLE );
}

void CSpiderMine::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	switch( RANDOM_LONG( 0, 4 ) )
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}

	BaseClass::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}