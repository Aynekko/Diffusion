#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

//===========================================================
//LRC - trigger_inout, a trigger which fires _only_ when
// the player enters or leaves it.
//   If there's more than one entity it can trigger off, then
// it will trigger for each one that enters and leaves.
//===========================================================


// CInOutRegister method bodies:
BEGIN_DATADESC(CInOutRegister)
	DEFINE_FIELD(m_pField, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pNext, FIELD_CLASSPTR),
	DEFINE_FIELD(m_hValue, FIELD_EHANDLE),
END_DATADESC()

LINK_ENTITY_TO_CLASS(inout_register, CInOutRegister);

BOOL CInOutRegister::IsRegistered(CBaseEntity* pValue)
{
	if (m_hValue == pValue)
		return TRUE;
	else if (m_pNext)
		return m_pNext->IsRegistered(pValue);
	else
		return FALSE;
}

CInOutRegister* CInOutRegister::Add(CBaseEntity* pValue)
{
	if (m_hValue == pValue)
	{
		// it's already in the list, don't need to do anything
		return this;
	}
	else if (m_pNext)
	{
		// keep looking
		m_pNext = m_pNext->Add(pValue);
		return this;
	}
	else
	{
		// reached the end of the list; add the new entry, and trigger
		CInOutRegister* pResult = GetClassPtr((CInOutRegister*)NULL);
		pResult->m_hValue = pValue;
		pResult->m_pNext = this;
		pResult->m_pField = m_pField;
		pResult->pev->classname = MAKE_STRING("inout_register");

		m_pField->FireOnEntry(pValue);
		return pResult;
	}
}

CInOutRegister* CInOutRegister::Prune(void)
{
	if (m_hValue)
	{
		ASSERTSZ(m_pNext != NULL, "invalid InOut registry terminator\n");
		if (m_pField->TriggerIntersects(m_hValue) && !FBitSet(m_pField->pev->flags, FL_KILLME))
		{
			// this entity is still inside the field, do nothing
			m_pNext = m_pNext->Prune();
			return this;
		}
		else
		{
			// this entity has just left the field, trigger
			m_pField->FireOnLeaving(m_hValue);
			SetThink(&CInOutRegister::SUB_Remove);
			SetNextThink(0.1);
			return m_pNext->Prune();
		}
	}
	else
	{	// this register has a missing or null value
		if (m_pNext)
		{
			// this is an invalid list entry, remove it
			SetThink(&CInOutRegister::SUB_Remove);
			SetNextThink(0.1);
			return m_pNext->Prune();
		}
		else
		{
			// this is the list terminator, leave it.
			return this;
		}
	}
}

// CTriggerInOut method bodies:
LINK_ENTITY_TO_CLASS(trigger_inout, CTriggerInOut);

BEGIN_DATADESC(CTriggerInOut)
DEFINE_KEYFIELD(m_iszAltTarget, FIELD_STRING, "m_iszAltTarget"),
DEFINE_KEYFIELD(m_iszBothTarget, FIELD_STRING, "m_iszBothTarget"),
DEFINE_FIELD(m_pRegister, FIELD_CLASSPTR),
END_DATADESC()

void CTriggerInOut::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszAltTarget"))
	{
		m_iszAltTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszBothTarget"))
	{
		m_iszBothTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}

int CTriggerInOut::Restore(CRestore& restore)
{
	CInOutRegister* pRegister;

	if (restore.IsGlobalMode())
	{
		// we already have the valid chain.
		// Don't break it with bad pointers from previous level
		pRegister = m_pRegister;
	}

	int status = BaseClass::Restore(restore);
	if (!status)
		return 0;

	if (restore.IsGlobalMode())
	{
		// restore our chian here
		m_pRegister = pRegister;
	}

	return status;
}

void CTriggerInOut::Spawn(void)
{
	InitTrigger();
	// create a null-terminator for the registry
	m_pRegister = GetClassPtr((CInOutRegister*)NULL);
	m_pRegister->m_hValue = NULL;
	m_pRegister->m_pNext = NULL;
	m_pRegister->m_pField = this;
	m_pRegister->pev->classname = MAKE_STRING("inout_register");
}

void CTriggerInOut::Touch(CBaseEntity* pOther)
{
	if (!CanTouch(pOther))
		return;

	m_pRegister = m_pRegister->Add(pOther);

	if (pev->nextthink <= 0.0f && !m_pRegister->IsEmpty())
		SetNextThink(0.05);
}

void CTriggerInOut::Think(void)
{
	// Prune handles all Intersects tests and fires targets as appropriate
	m_pRegister = m_pRegister->Prune();

	if (m_pRegister->IsEmpty())
		DontThink();
	else
		SetNextThink(0.05);
}

void CTriggerInOut::FireOnEntry(CBaseEntity* pOther)
{
	if (!IsLockedByMaster(pOther))
	{
		//Msg( "FireOnEntry( %s )\n", STRING( pev->target ));
		UTIL_FireTargets(m_iszBothTarget, pOther, this, USE_ON, 0);
		UTIL_FireTargets(pev->target, pOther, this, USE_TOGGLE, 0);
	}
}

void CTriggerInOut::FireOnLeaving(CBaseEntity* pOther)
{
	if (!IsLockedByMaster(pOther))
	{
		//Msg( "FireOnLeaving( %s )\n", STRING( m_iszAltTarget ));
		UTIL_FireTargets(m_iszBothTarget, pOther, this, USE_OFF, 0);
		UTIL_FireTargets(m_iszAltTarget, pOther, this, USE_TOGGLE, 0);
	}
}

void CTriggerInOut::OnRemove(void)
{
	if (!m_pRegister) return; // e.g. moved from another level

	// Prune handles all Intersects tests and fires targets as appropriate
	m_pRegister = m_pRegister->Prune();
}