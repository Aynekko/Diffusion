#include "extdll.h"
#include "util.h"
#include "cbase.h"

//=====================================================================================
// diffusion - improvements in both changetarget and changeparent
// NEW: can now change target and parent in 5 different entities at once
// FIX: before, both entites picked only the first entity with the specified name.
// now, it will replace target and parent in ALL entities with the same name.
// ====================================================================================

class CTriggerChangeParent : public CBaseDelay
{
	DECLARE_CLASS(CTriggerChangeParent, CBaseDelay);
public:
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void ChangeParent( string_t TargetEnt, string_t NewParent, CBaseEntity *pActivator );

	DECLARE_DATADESC();
private:
	string_t	m_iszNewParent;
	string_t	m_iszNewParent2;
	string_t	m_iszNewParent3;
	string_t	m_iszNewParent4;
	string_t	m_iszNewParent5;
	// target1 is pev->target
	string_t	target2;
	string_t	target3;
	string_t	target4;
	string_t	target5;
};

LINK_ENTITY_TO_CLASS(trigger_changeparent, CTriggerChangeParent);

BEGIN_DATADESC(CTriggerChangeParent)
	DEFINE_KEYFIELD(m_iszNewParent, FIELD_STRING, "m_iszNewParent"),
	DEFINE_KEYFIELD(m_iszNewParent2, FIELD_STRING, "m_iszNewParent2"),
	DEFINE_KEYFIELD(m_iszNewParent3, FIELD_STRING, "m_iszNewParent3"),
	DEFINE_KEYFIELD(m_iszNewParent4, FIELD_STRING, "m_iszNewParent4"),
	DEFINE_KEYFIELD(m_iszNewParent5, FIELD_STRING, "m_iszNewParent5"),
	DEFINE_KEYFIELD(target2, FIELD_STRING, "target2"),
	DEFINE_KEYFIELD(target3, FIELD_STRING, "target3"),
	DEFINE_KEYFIELD(target4, FIELD_STRING, "target4"),
	DEFINE_KEYFIELD(target5, FIELD_STRING, "target5"),
END_DATADESC()

void CTriggerChangeParent::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszNewParent"))
	{
		m_iszNewParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewParent2"))
	{
		m_iszNewParent2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewParent3"))
	{
		m_iszNewParent3 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewParent4"))
	{
		m_iszNewParent4 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewParent5"))
	{
		m_iszNewParent5 = ALLOC_STRING(pkvd->szValue);
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

void CTriggerChangeParent::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (!FStringNull(pev->target))
		ChangeParent( pev->target, m_iszNewParent, pActivator );

	if( !FStringNull( target2 ) )
		ChangeParent( target2, m_iszNewParent2, pActivator );

	if( !FStringNull( target3 ) )
		ChangeParent( target3, m_iszNewParent3, pActivator );

	if( !FStringNull( target4 ) )
		ChangeParent( target4, m_iszNewParent4, pActivator );

	if( !FStringNull( target5 ) )
		ChangeParent( target5, m_iszNewParent5, pActivator );
}

void CTriggerChangeParent::ChangeParent( string_t TargetEnt, string_t NewParent, CBaseEntity *pActivator )
{
	CBaseEntity *pTarget = NULL;

	while( (pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( TargetEnt ), pActivator )) != NULL )
	{
		if( FStrEq( STRING( NewParent ), "*locus" ) )
		{
			if( pActivator )
				pTarget->SetParent( pActivator );
			else
				ALERT( at_error, "trigger_changeparent \"%s\" requires a locus!\n", STRING( pev->targetname ) );
		}
		else
		{
			if( NewParent != NULL_STRING )
				pTarget->SetParent( NewParent );
			else
				pTarget->SetParent( NULL );
		}
	}
}