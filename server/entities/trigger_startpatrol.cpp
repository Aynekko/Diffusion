#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "triggers.h"

//===========================================================
//LRC- trigger_startpatrol
//===========================================================
class CTriggerSetPatrol : public CBaseDelay
{
	DECLARE_CLASS(CTriggerSetPatrol, CBaseDelay);
public:
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS(trigger_startpatrol, CTriggerSetPatrol);

void CTriggerSetPatrol::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszPath"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerSetPatrol::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBaseEntity* pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target), pActivator);
	CBaseEntity* pPath = UTIL_FindEntityByTargetname(NULL, STRING(pev->message), pActivator);

	if (pTarget && pPath)
	{
		CBaseMonster* pMonster = pTarget->MyMonsterPointer();
		if (pMonster) pMonster->StartPatrol(pPath);
	}
}