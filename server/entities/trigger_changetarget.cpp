#include "extdll.h"
#include "util.h"
#include "cbase.h"

//=====================================================================================
// diffusion - improvements in both changetarget and changeparent
// NEW: can now change target and parent in 5 different entities at once
// FIX: before, both entites picked only the first entity with the specified name.
// now, it will replace target and parent in ALL entities with the same name.
// ====================================================================================

class CTriggerChangeTarget : public CBaseDelay
{
	DECLARE_CLASS(CTriggerChangeTarget, CBaseDelay);
public:
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void ChangeTarget( string_t TargetEnt, string_t NewTarget, CBaseEntity *pActivator );

	DECLARE_DATADESC();
private:
	string_t	m_iszNewTarget;
	string_t	m_iszNewTarget2;
	string_t	m_iszNewTarget3;
	string_t	m_iszNewTarget4;
	string_t	m_iszNewTarget5;
	// target1 is pev->target
	string_t	target2;
	string_t	target3;
	string_t	target4;
	string_t	target5;

};

LINK_ENTITY_TO_CLASS(trigger_changetarget, CTriggerChangeTarget);

BEGIN_DATADESC(CTriggerChangeTarget)
	DEFINE_KEYFIELD(m_iszNewTarget, FIELD_STRING, "m_iszNewTarget"),
	DEFINE_KEYFIELD(m_iszNewTarget2, FIELD_STRING, "m_iszNewTarget2"),
	DEFINE_KEYFIELD(m_iszNewTarget3, FIELD_STRING, "m_iszNewTarget3"),
	DEFINE_KEYFIELD(m_iszNewTarget4, FIELD_STRING, "m_iszNewTarget4"),
	DEFINE_KEYFIELD(m_iszNewTarget5, FIELD_STRING, "m_iszNewTarget5"),
	DEFINE_KEYFIELD(target2, FIELD_STRING, "target2"),
	DEFINE_KEYFIELD(target3, FIELD_STRING, "target3"),
	DEFINE_KEYFIELD(target4, FIELD_STRING, "target4"),
	DEFINE_KEYFIELD(target5, FIELD_STRING, "target5"),
END_DATADESC()

void CTriggerChangeTarget::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszNewTarget"))
	{
		m_iszNewTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewTarget2"))
	{
		m_iszNewTarget2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewTarget3"))
	{
		m_iszNewTarget3 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewTarget4"))
	{
		m_iszNewTarget4 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewTarget5"))
	{
		m_iszNewTarget5 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "target2"))
	{
		target2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "target3"))
	{
		target3 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "target4"))
	{
		target4 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "target5"))
	{
		target5 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerChangeTarget::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (!FStringNull(pev->target))
		ChangeTarget( pev->target, m_iszNewTarget, pActivator );

	if( !FStringNull( target2 ) )
		ChangeTarget( target2, m_iszNewTarget2, pActivator );

	if( !FStringNull( target3 ) )
		ChangeTarget( target3, m_iszNewTarget3, pActivator );

	if( !FStringNull( target4 ) )
		ChangeTarget( target4, m_iszNewTarget4, pActivator );

	if( !FStringNull( target5 ) )
		ChangeTarget( target5, m_iszNewTarget5, pActivator );
}

void CTriggerChangeTarget::ChangeTarget( string_t TargetEnt, string_t NewTarget, CBaseEntity *pActivator )
{
	CBaseEntity *pTarget = NULL;

	while( (pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( TargetEnt ), pActivator )) != NULL )
	{
		if( FStrEq( STRING( NewTarget ), "*locus" ) )
		{
			if( pActivator )
				pTarget->pev->target = pActivator->pev->targetname;
			else
				ALERT( at_error, "trigger_changetarget \"%s\" requires a locus!\n", STRING( pev->targetname ) );
		}
		else
			pTarget->pev->target = NewTarget;

		CBaseMonster *pMonster = pTarget->MyMonsterPointer();
		if( pMonster )
			pMonster->m_pGoalEnt = NULL;

		if( pTarget->IsFuncScreen() )
			pTarget->Activate(); // HACKHACK update portal camera
	}
}