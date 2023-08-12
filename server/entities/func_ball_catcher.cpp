#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/saverestore.h"
#include "entities/trains.h"			// trigger_camera has train functionality
#include "game/gamerules.h"
#include "talkmonster.h"
#include "weapons/weapons.h"
#include "triggers.h"

//================================================================================
//diffusion - energy ball catcher - responds only to touches from env_ballentity
//================================================================================
#define ENVBALLC_REMOVE			BIT(0) // remove after firing target
#define ENVBALLC_DONOTABSORB	BIT(1) // do not absorb the balls, reflect
#define ENVBALLC_SILENT			BIT(2) // do not make sounds
#define ENVBALLC_NOTSOLID		BIT(3) // not solid

class CEnvBallCatcher : public CBaseDelay
{
	DECLARE_CLASS(CEnvBallCatcher, CBaseDelay);
public:
	void KeyValue(KeyValueData* pkvd);
	void Spawn(void);
	void Precache(void);
	void EnvBallTouch(CBaseEntity* pOther);
	void Wait(void);
	//	STATE GetState( void ) { return ( pev->nextthink > gpGlobals->time ) ? STATE_OFF : STATE_ON; }

	string_t BallName; // only accept balls with this name
	int AmountOfBalls; // number of balls to accept before firing the target
	int Counter; // this will count them

	DECLARE_DATADESC();
};

BEGIN_DATADESC(CEnvBallCatcher)
	DEFINE_KEYFIELD(BallName, FIELD_STRING, "ballname"),
	DEFINE_KEYFIELD(AmountOfBalls, FIELD_INTEGER, "amount"),
	DEFINE_FIELD(Counter, FIELD_INTEGER),
	DEFINE_FUNCTION(EnvBallTouch),
	DEFINE_FUNCTION(Wait),
END_DATADESC();

LINK_ENTITY_TO_CLASS(func_ball_catcher, CEnvBallCatcher);

void CEnvBallCatcher::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "amount"))
	{
		AmountOfBalls = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "ballname"))
	{
		BallName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CEnvBallCatcher::Spawn(void)
{
	Precache();

	if ( HasSpawnFlags(ENVBALLC_NOTSOLID) )
	{
		pev->solid = SOLID_TRIGGER;
		pev->movetype = MOVETYPE_NONE;

		if (pev->movedir == g_vecZero)
		{
			pev->movedir = Vector(1, 0, 0);
			SetLocalAngles(g_vecZero);
		}
	}
	else
	{
		pev->solid = SOLID_BSP;
		pev->movetype = MOVETYPE_PUSH;
		SetLocalAngles(g_vecZero);
	}

	pev->flags |= FL_WORLDBRUSH;

	SET_MODEL(edict(), GetModel());

	if (!AmountOfBalls || AmountOfBalls <= 0)
		AmountOfBalls = 1;

	Counter = 0;

	if (m_flWait == 0)
		m_flWait = 0.1;

	SetTouch(&CEnvBallCatcher::EnvBallTouch);

	//	ALERT( at_console, "Counter %3d\n", Counter );

}

void CEnvBallCatcher::Precache(void)
{
	if ( !HasSpawnFlags(ENVBALLC_SILENT) )
	{
		PRECACHE_SOUND("comball/accept1.wav");
		PRECACHE_SOUND("comball/accept2.wav");
		PRECACHE_SOUND("buttons/blip1.wav");
		PRECACHE_SOUND("comball/denied.wav");
	}
}

void CEnvBallCatcher::EnvBallTouch(CBaseEntity* pOther)
{
	if (pev->nextthink > gpGlobals->time)
		return;

	if (IsLockedByMaster(pOther))
		return;

	if (BallName && !FStrEq(pOther->GetTargetname(), STRING(BallName)) && FClassnameIs(pOther->pev, "env_ballentity"))
	{
		if (!HasSpawnFlags(ENVBALLC_SILENT))
			EMIT_SOUND(ENT(pev), CHAN_STATIC, "comball/denied.wav", 1, ATTN_NORM);
		return;
	}

	//----------------------------------------------------------

	if (FClassnameIs(pOther->pev, "env_ballentity"))
	{
		if (Counter < AmountOfBalls)
		{
			Counter++;
			//ALERT( at_console, "Counter %3d\n", Counter );
			if (Counter >= AmountOfBalls)
			{
				// activate the target and reset counter
				SUB_UseTargets(pOther, USE_TOGGLE, 0);
				Counter = 0;
				// absorb (delete) the ball if set
				if (!HasSpawnFlags(ENVBALLC_DONOTABSORB))
					UTIL_Remove(pOther);
				if (!HasSpawnFlags(ENVBALLC_SILENT))
				{
					switch (RANDOM_LONG(0, 1))
					{
					case 0: EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/accept1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(-10, 10)); break;
					case 1: EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/accept2.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(-10, 10)); break;
					}
				}
				// delete the catcher brush completely, if set
				if (HasSpawnFlags(ENVBALLC_REMOVE))
				{
					UTIL_Remove(this);
					return;
				}
			}
			else
			{
				if (!HasSpawnFlags(ENVBALLC_SILENT))
					EMIT_SOUND(ENT(pev), CHAN_STATIC, "buttons/blip1.wav", 1, ATTN_NORM);
				if (!HasSpawnFlags(ENVBALLC_DONOTABSORB))
					UTIL_Remove(pOther); // just absorb the ball
			}
		}
	}
	else
		return;

	//----------------------------------------------------------

	if (m_flWait > 0)
	{
		SetThink(&CEnvBallCatcher::Wait);
		SetNextThink(m_flWait);
	}
}

void CEnvBallCatcher::Wait(void)
{
	SetThink(NULL);
}