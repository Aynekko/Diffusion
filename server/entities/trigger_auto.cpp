#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

//==================================================================================
// This trigger will fire when the level spawns (or respawns if not fire once)
// It will check a global state before firing.  It supports delay and killtargets
//==================================================================================

#define SF_AUTO_FIREONCE		0x0001

class CAutoTrigger : public CBaseDelay
{
	DECLARE_CLASS(CAutoTrigger, CBaseDelay);
public:
	void KeyValue(KeyValueData* pkvd);
	void Spawn(void);
	void Precache(void);
	void Think(void);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

private:
	int	m_globalstate;
	USE_TYPE	triggerType;
};
LINK_ENTITY_TO_CLASS(trigger_auto, CAutoTrigger);

BEGIN_DATADESC(CAutoTrigger)
	DEFINE_KEYFIELD(m_globalstate, FIELD_STRING, "globalstate"),
	DEFINE_FIELD(triggerType, FIELD_INTEGER),
END_DATADESC()

void CAutoTrigger::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "globalstate"))
	{
		m_globalstate = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi(pkvd->szValue);
		switch (type)
		{
		case 0:
			triggerType = (USE_TYPE)-1;	// will be changed in spawn
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}


void CAutoTrigger::Spawn(void)
{
	if (triggerType == -1)
		triggerType = USE_OFF;	// "triggerstate" is present and set to 'OFF'
	else if (!triggerType)
		triggerType = USE_ON;	// "triggerstate" is missing - defaulting to 'ON'

	// HACKHACK: run circles on a final map
	if (FStrEq(STRING(gpGlobals->mapname), "c5a1") && FStrEq(GetTarget(), "hoop_1"))
		triggerType = USE_ON;
#if 0
	// don't confuse level-designers with firing after loading savedgame
	if (m_globalstate == NULL_STRING)
		SetBits(pev->spawnflags, SF_AUTO_FIREONCE);
#endif
	Precache();
}

void CAutoTrigger::Precache(void)
{
	SetNextThink(0.1);
}

void CAutoTrigger::Think(void)
{
	if (!m_globalstate || gGlobalState.EntityGetState(m_globalstate) == GLOBAL_ON)
	{
		CBaseEntity* pClient;
		// diffusion - activator is client.
		pClient = UTIL_PlayerByIndex(1);

		if (pClient)
			SUB_UseTargets(pClient, triggerType, 0);

		if (HasSpawnFlags(SF_AUTO_FIREONCE))
			UTIL_Remove(this);
	}
}