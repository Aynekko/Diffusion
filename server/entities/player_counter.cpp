#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//==========================================================================
// diffusion - count some numbers to carry throughout the game
//==========================================================================
#define SF_PLRCNT_REMOVE		BIT(0) // remove on fire
#define SF_PLRCNT_RESETSLOT1	BIT(1) // reset slot 1 to zero
#define SF_PLRCNT_RESETSLOT2	BIT(2)
#define SF_PLRCNT_RESETSLOT3	BIT(3)
#define SF_PLRCNT_RESETSLOT4	BIT(4)
#define SF_PLRCNT_RESETSLOT5	BIT(5)
#define SF_PLRCNT_SUBTRACT		BIT(6) // subtract the value after trigger

class CPlayerCounterAdd : public CBaseDelay
{
	DECLARE_CLASS(CPlayerCounterAdd, CBaseDelay);
public:
	void Precache(void);
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	int Slot; // which slot the value will be applied to (currently 5 slots)
	int Mode; // add value (0 - default), set value (1)
	int Value;
	int m_iszPickupSound;
	
	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(player_counter_add, CPlayerCounterAdd);

BEGIN_DATADESC(CPlayerCounterAdd)
	DEFINE_KEYFIELD(Slot, FIELD_INTEGER, "slot"),
	DEFINE_KEYFIELD(Mode, FIELD_INTEGER, "mode"),
	DEFINE_KEYFIELD(Value, FIELD_INTEGER, "value"),
	DEFINE_KEYFIELD(m_iszPickupSound, FIELD_STRING, "sound"),
END_DATADESC();

void CPlayerCounterAdd::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "slot"))
	{
		Slot = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "mode"))
	{
		Mode = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "value"))
	{
		Value = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sound"))
	{
		m_iszPickupSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CPlayerCounterAdd::Precache(void)
{
	if (m_iszPickupSound)
		PRECACHE_SOUND((char*)STRING(m_iszPickupSound));
}

void CPlayerCounterAdd::Spawn(void)
{
	Precache();

	if (!Slot)
		Slot = 0;
	else
		Slot--; // HACKHACK slots begin from 0

	if (!Mode)
		Mode = 0;
}

void CPlayerCounterAdd::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	switch (Mode)
	{
	case 0: pPlayer->CountSlot[Slot] += Value; break;
	case 1: pPlayer->CountSlot[Slot] = Value; break;
	}

	if (m_iszPickupSound)
		EMIT_SOUND(edict(), CHAN_STATIC, STRING(m_iszPickupSound), 1, ATTN_NORM);

	// do this stuff after all changes above, in case the mapper (me) fucked up
	if ( HasSpawnFlags(SF_PLRCNT_RESETSLOT1) )
		pPlayer->CountSlot[0] = 0;
	if ( HasSpawnFlags( SF_PLRCNT_RESETSLOT2))
		pPlayer->CountSlot[1] = 0;
	if ( HasSpawnFlags( SF_PLRCNT_RESETSLOT3))
		pPlayer->CountSlot[2] = 0;
	if ( HasSpawnFlags( SF_PLRCNT_RESETSLOT4))
		pPlayer->CountSlot[3] = 0;
	if ( HasSpawnFlags( SF_PLRCNT_RESETSLOT5))
		pPlayer->CountSlot[4] = 0;

	//	for( int i = 0; i < 5; i++ )
	//		ALERT(at_aiconsole, "Slot has %3d\n", pPlayer->CountSlot[i]);

	if( HasSpawnFlags(SF_PLRCNT_REMOVE) )
		UTIL_Remove(this);
}




//==========================================================================
// diffusion - if player has enough items, fire a target
//==========================================================================

class CPlayerCounterTrigger : public CBaseDelay
{
	DECLARE_CLASS(CPlayerCounterTrigger, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	void Precache(void);
	void Spawn(void);
	void Deny(CBaseEntity* pActivator);
	void Accept(CBaseEntity* pActivator);
	int Slot; // which slot the value will be checked in
	int Value; // value to meet (greater or equal)
	int Value2; // only used in "in range" mode
	int m_iszAcceptSound; // enough inv., trigger and play sound
	int m_iszDenySound; // when there's not enough inv. in the slot
	int Mode; // 0 - must be greater or equal, 1 - must be equal, 2 - must be less
	string_t AltTarget; // fire another target when condition is not met
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(player_counter_trigger, CPlayerCounterTrigger);

BEGIN_DATADESC(CPlayerCounterTrigger)
	DEFINE_KEYFIELD(Slot, FIELD_INTEGER, "slot"),
	DEFINE_KEYFIELD(Value, FIELD_INTEGER, "value"),
	DEFINE_KEYFIELD( Value2, FIELD_INTEGER, "value2" ),
	DEFINE_KEYFIELD(Mode, FIELD_INTEGER, "mode"),
	DEFINE_KEYFIELD(m_iszAcceptSound, FIELD_STRING, "soundaccept"),
	DEFINE_KEYFIELD(m_iszDenySound, FIELD_STRING, "sounddeny"),
	DEFINE_KEYFIELD( AltTarget, FIELD_STRING, "alttarget" ),
	DEFINE_FUNCTION(Accept),
	DEFINE_FUNCTION(Deny),
END_DATADESC();

void CPlayerCounterTrigger::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "slot"))
	{
		Slot = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "value"))
	{
		Value = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "value2" ) )
	{
		Value2 = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "mode"))
	{
		Mode = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "soundaccept"))
	{
		m_iszAcceptSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sounddeny"))
	{
		m_iszDenySound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "alttarget" ) )
	{
		AltTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CPlayerCounterTrigger::Spawn(void)
{
	Precache();

	if (!Slot)
		Slot = 0;
	else
		Slot--; // HACKHACK slots begin from 0

	if (!Mode)
		Mode = 0;

	if (!Value)
		Value = 0;

	if( Mode == 3 )
	{
		if( !Value2 )
		{
			ALERT( at_error, "CPlayerCounterTrigger: no second value in InRange mode! Set to (Value + 100).\n" );
			Value2 = Value + 100;
		}
		
		if( Value2 <= Value )
		{
			ALERT( at_error, "CPlayerCounterTrigger: second value is less than first! Set to (Value + 100).\n" );
			Value2 = Value + 100;
		}
	}
}

void CPlayerCounterTrigger::Precache(void)
{
	if (m_iszAcceptSound)
		PRECACHE_SOUND((char*)STRING(m_iszAcceptSound));

	if (m_iszDenySound)
		PRECACHE_SOUND((char*)STRING(m_iszDenySound));
}

void CPlayerCounterTrigger::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	//	for( int i = 0; i < 5; i++ )
	//		ALERT(at_aiconsole, "Slot has %3d\n", pPlayer->CountSlot[i]);

	switch (Mode)
	{
	case 0:
		if (pPlayer->CountSlot[Slot] >= Value) // got enough or more than enough
		{
			if( HasSpawnFlags( SF_PLRCNT_SUBTRACT ))
				pPlayer->CountSlot[Slot] -= Value; // remove the value
			Accept(pActivator);
		}
		else
			Deny(pActivator);
	break;

	case 1:
		if (pPlayer->CountSlot[Slot] == Value) // exact amount needed
		{
			if( HasSpawnFlags( SF_PLRCNT_SUBTRACT ) )
				pPlayer->CountSlot[Slot] -= Value; // remove the value
			Accept(pActivator);
		}
		else
			Deny(pActivator);
	break;

	case 2:
		if( pPlayer->CountSlot[Slot] < Value ) // only trigger if we have less than needed
		{
			if( HasSpawnFlags( SF_PLRCNT_SUBTRACT ) )
				pPlayer->CountSlot[Slot] = 0; // nullify it
			Accept( pActivator );
		}
		else
			Deny(pActivator);
	break;

	case 3:
		// "in range" mode. check if the slot's value is between value1 and value2
		if( (pPlayer->CountSlot[Slot] <= Value2) && (pPlayer->CountSlot[Slot] >= Value) )
		{
			if( HasSpawnFlags( SF_PLRCNT_SUBTRACT ) )
				pPlayer->CountSlot[Slot] -= Value; // remove the value
			Accept( pActivator );
		}
		else
			Deny( pActivator );
	break;
	}
}

void CPlayerCounterTrigger::Deny(CBaseEntity* pActivator)
{
	if (m_iszDenySound)
		EMIT_SOUND(edict(), CHAN_STATIC, STRING(m_iszDenySound), 1, ATTN_NORM);

	if (pev->message)
		UTIL_ShowMessage(STRING(pev->message), pActivator);

	if( !FStringNull( AltTarget ) )
		UTIL_FireTargets( AltTarget, pActivator, this, USE_TOGGLE, 0 );
}

void CPlayerCounterTrigger::Accept(CBaseEntity* pActivator)
{
	SUB_UseTargets(pActivator, USE_TOGGLE, 0);

	if (m_iszAcceptSound)
		EMIT_SOUND(edict(), CHAN_STATIC, STRING(m_iszAcceptSound), 1, ATTN_NORM);

	if (HasSpawnFlags(SF_PLRCNT_REMOVE) )
		UTIL_Remove(this);
}