#include "extdll.h"
#include "util.h"
#include "cbase.h"

//==========================================================================
//diffusion - randomizer (pretty bad, but...it works, so... :| )
//==========================================================================
#define RAND_LOOP		BIT(0)
#define RAND_STARTON	BIT(1)

class CTriggerRandomizer : public CBaseDelay
{
	DECLARE_CLASS(CTriggerRandomizer, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn(void);
	void Loop(void);
	float Used;
	float Deviation;
	void FireRandomTarget( void );

	int TotalTargets;
	int LastUsedTarget;
	string_t firetarget1;
	string_t firetarget2;
	string_t firetarget3;
	string_t firetarget4;
	string_t firetarget5;
	string_t firetarget6;
	string_t firetarget7;
	string_t firetarget8;
	string_t firetarget9;
	string_t firetarget10;
	string_t firetarget11;
	string_t firetarget12;
	string_t firetarget13;
	string_t firetarget14;
	string_t firetarget15;
	string_t firetarget16;

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(trigger_randomizer, CTriggerRandomizer);

BEGIN_DATADESC(CTriggerRandomizer)
	DEFINE_KEYFIELD(TotalTargets, FIELD_INTEGER, "totaltargets"),
	DEFINE_KEYFIELD(firetarget1, FIELD_STRING, "firetarget1"),
	DEFINE_KEYFIELD(firetarget2, FIELD_STRING, "firetarget2"),
	DEFINE_KEYFIELD(firetarget3, FIELD_STRING, "firetarget3"),
	DEFINE_KEYFIELD(firetarget4, FIELD_STRING, "firetarget4"),
	DEFINE_KEYFIELD(firetarget5, FIELD_STRING, "firetarget5"),
	DEFINE_KEYFIELD(firetarget6, FIELD_STRING, "firetarget6"),
	DEFINE_KEYFIELD(firetarget7, FIELD_STRING, "firetarget7"),
	DEFINE_KEYFIELD(firetarget8, FIELD_STRING, "firetarget8"),
	DEFINE_KEYFIELD(firetarget9, FIELD_STRING, "firetarget9"),
	DEFINE_KEYFIELD(firetarget10, FIELD_STRING, "firetarget10"),
	DEFINE_KEYFIELD(firetarget11, FIELD_STRING, "firetarget11"),
	DEFINE_KEYFIELD(firetarget12, FIELD_STRING, "firetarget12"),
	DEFINE_KEYFIELD(firetarget13, FIELD_STRING, "firetarget13"),
	DEFINE_KEYFIELD(firetarget14, FIELD_STRING, "firetarget14"),
	DEFINE_KEYFIELD(firetarget15, FIELD_STRING, "firetarget15"),
	DEFINE_KEYFIELD(firetarget16, FIELD_STRING, "firetarget16"),
	DEFINE_FIELD(LastUsedTarget, FIELD_INTEGER),
	DEFINE_KEYFIELD(Deviation, FIELD_FLOAT, "deviation"),
	DEFINE_FUNCTION(Loop),
END_DATADESC();

void CTriggerRandomizer::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "firetarget1"))
	{
		firetarget1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget2"))
	{
		firetarget2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget3"))
	{
		firetarget3 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget4"))
	{
		firetarget4 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget5"))
	{
		firetarget5 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget6"))
	{
		firetarget6 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget7"))
	{
		firetarget7 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget8"))
	{
		firetarget8 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget9"))
	{
		firetarget9 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget10"))
	{
		firetarget10 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget11"))
	{
		firetarget11 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget12"))
	{
		firetarget12 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget13"))
	{
		firetarget13 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget14"))
	{
		firetarget14 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget15"))
	{
		firetarget15 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget16"))
	{
		firetarget16 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "totaltargets"))
	{
		TotalTargets = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deviation"))
	{
		Deviation = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerRandomizer::Spawn(void)
{
	m_iState = STATE_OFF;
	
	if( HasSpawnFlags(RAND_STARTON) )
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.2);
	}

	if( HasSpawnFlags(RAND_LOOP) )
	{
		if (m_flWait < 0.1)
			m_flWait = 0.1;
	}
}

void CTriggerRandomizer::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	if (TotalTargets <= 1)
	{
		ALERT( at_error, "trigger_randomizer \"%s\": Not enough targets to randomize!\n", STRING( pev->targetname ) );
		UTIL_Remove( this );
		return;
	}

	if( HasSpawnFlags( RAND_LOOP ) )
	{
		// already on, then turn it off
		if( (m_iState == STATE_ON && useType == USE_TOGGLE) || (useType == USE_OFF) )
		{
			DontThink();
			return;
		}

		Used = gpGlobals->time;
		m_iState = STATE_ON;
		SetThink( &CTriggerRandomizer::Loop );
		if( Deviation > 0 )
		{
			float RandomDelay = m_flWait + RANDOM_FLOAT( -Deviation, Deviation );
			RandomDelay = bound( 0.1, RandomDelay, 999 ); // safety check
			SetNextThink( RandomDelay );
		}
		else
		{
			SetNextThink( m_flWait );
		}
	}
	else
		FireRandomTarget();
}

void CTriggerRandomizer::Loop(void)
{
	if( Deviation > 0 )
	{
		float RandomDelay = m_flWait + RANDOM_FLOAT( -Deviation, Deviation );
		RandomDelay = bound( 0.1, RandomDelay, 999 ); // safety check
		SetNextThink( RandomDelay );
	}
	else
		SetNextThink( m_flWait );
	
	if( gpGlobals->time > Used )
		FireRandomTarget();
}

void CTriggerRandomizer::FireRandomTarget( void )
{
	// UNDONE pActivator should be player who activated the trigger
	CBaseEntity *pActivator = this;
	USE_TYPE useType = USE_TOGGLE;
	
	switch( RANDOM_LONG( 1, TotalTargets ) )
	{
	case 1:
		if( LastUsedTarget == 1 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget1, pActivator, this, useType ); LastUsedTarget = 1; } break;
	case 2:
		if( LastUsedTarget == 2 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget2, pActivator, this, useType ); LastUsedTarget = 2; } break;
	case 3:
		if( LastUsedTarget == 3 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget3, pActivator, this, useType ); LastUsedTarget = 3; } break;
	case 4:
		if( LastUsedTarget == 4 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget4, pActivator, this, useType ); LastUsedTarget = 4; } break;
	case 5:
		if( LastUsedTarget == 5 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget5, pActivator, this, useType ); LastUsedTarget = 5; } break;
	case 6:
		if( LastUsedTarget == 6 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget6, pActivator, this, useType ); LastUsedTarget = 6; } break;
	case 7:
		if( LastUsedTarget == 7 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget7, pActivator, this, useType ); LastUsedTarget = 7; } break;
	case 8:
		if( LastUsedTarget == 8 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget8, pActivator, this, useType ); LastUsedTarget = 8; } break;
	case 9:
		if( LastUsedTarget == 9 )  { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget9, pActivator, this, useType ); LastUsedTarget = 9; } break;
	case 10:
		if( LastUsedTarget == 10 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget10, pActivator, this, useType ); LastUsedTarget = 10; } break;
	case 11:
		if( LastUsedTarget == 11 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget11, pActivator, this, useType ); LastUsedTarget = 11; } break;
	case 12:
		if( LastUsedTarget == 12 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget12, pActivator, this, useType ); LastUsedTarget = 12; } break;
	case 13:
		if( LastUsedTarget == 13 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget13, pActivator, this, useType ); LastUsedTarget = 13; } break;
	case 14:
		if( LastUsedTarget == 14 )  { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget14, pActivator, this, useType ); LastUsedTarget = 14; } break;
	case 15:
		if( LastUsedTarget == 15 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget15, pActivator, this, useType ); LastUsedTarget = 15; } break;
	case 16:
		if( LastUsedTarget == 16 ) { FireRandomTarget(); }
		else { UTIL_FireTargets( firetarget16, pActivator, this, useType ); LastUsedTarget = 16; } break;
	}
}