#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "talkmonster.h"
#include "triggers.h"

#define SF_RELAY_FIREONCE		0x0001

class CTriggerRelay : public CBaseDelay
{
	DECLARE_CLASS(CTriggerRelay, CBaseDelay);
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int ObjectCaps(void) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
private:
	USE_TYPE	triggerType;
	string_t	m_iszAltTarget;
	int			SkillLevel; // diffusion - skill level condition must be met to activate this entity
};
LINK_ENTITY_TO_CLASS(trigger_relay, CTriggerRelay);

BEGIN_DATADESC(CTriggerRelay)
	DEFINE_FIELD(triggerType, FIELD_INTEGER),
	DEFINE_KEYFIELD(m_iszAltTarget, FIELD_STRING, "m_iszAltTarget"),
	DEFINE_KEYFIELD(SkillLevel, FIELD_INTEGER, "skilllevel"), // diffusion addition
END_DATADESC()

void CTriggerRelay::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = Q_atoi(pkvd->szValue);
		switch (type)
		{
		case 0:
			triggerType = (USE_TYPE)-1;	// -1: will be changed in spawn
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		case 3:
			triggerType = USE_SET;
			break;
		case 4:
			triggerType = USE_RESET;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "value"))
	{
		pev->scale = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszAltTarget"))
	{
		m_iszAltTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "skilllevel"))
	{
		SkillLevel = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}

void CTriggerRelay::Spawn(void)
{
	if (triggerType == -1)
		triggerType = USE_OFF;	// "triggerstate" is present and set to 'OFF'
	else if (!triggerType)
		triggerType = USE_ON;	// "triggerstate" is missing - defaulting to 'ON' // diffusion - can this happen?

	// HACKHACK: allow Hihilanth working on a c4a3
	if (FStrEq(STRING(gpGlobals->mapname), "c4a3") && FStrEq(GetTargetname(), "n_end_relay"))
		triggerType = USE_OFF;
}

void CTriggerRelay::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
	{
		if (m_iszAltTarget)
			SUB_UseTargets(this, triggerType, (pev->scale != 0.0f) ? pev->scale : value, m_iszAltTarget);

		if (FBitSet(pev->spawnflags, SF_RELAY_FIREONCE))
			UTIL_Remove(this);
		return;
	}

	if (SkillLevel == 1)
	{
		if (g_iSkillLevel == SKILL_EASY)
			SUB_UseTargets(this, triggerType, (pev->scale != 0.0f) ? pev->scale : value);
	}
	else if (SkillLevel == 2)
	{
		if (g_iSkillLevel == SKILL_MEDIUM)
			SUB_UseTargets(this, triggerType, (pev->scale != 0.0f) ? pev->scale : value);
	}
	else if (SkillLevel == 3)
	{
		if (g_iSkillLevel == SKILL_HARD)
			SUB_UseTargets(this, triggerType, (pev->scale != 0.0f) ? pev->scale : value);
	}
	else
		SUB_UseTargets(this, triggerType, (pev->scale != 0.0f) ? pev->scale : value);

	if (FBitSet(pev->spawnflags, SF_RELAY_FIREONCE))
		UTIL_Remove(this);
}