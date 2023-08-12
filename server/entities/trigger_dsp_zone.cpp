#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"

class CTriggerDSPZone : public CTriggerInOut
{
	DECLARE_CLASS(CTriggerDSPZone, CTriggerInOut);
public:
	void KeyValue(KeyValueData* pkvd);
	virtual void FireOnEntry(CBaseEntity* pOther);
	virtual void FireOnLeaving(CBaseEntity* pOther);
};

void CTriggerDSPZone::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "roomtype"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CTriggerDSPZone::FireOnEntry(CBaseEntity* pOther)
{
	// Only save on clients
	if (!pOther->IsPlayer()) return;
	pev->button = ((CBasePlayer*)pOther)->m_iSndRoomtype;
	((CBasePlayer*)pOther)->m_iSndRoomtype = pev->impulse;
}

void CTriggerDSPZone::FireOnLeaving(CBaseEntity* pOther)
{
	if (!pOther->IsPlayer()) return;
	((CBasePlayer*)pOther)->m_iSndRoomtype = pev->button;
}

LINK_ENTITY_TO_CLASS(trigger_dsp_zone, CTriggerDSPZone);