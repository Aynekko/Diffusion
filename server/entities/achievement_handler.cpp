#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//==========================================================================
// diffusion - achievement handler - apply a value to an achievement
//==========================================================================

#define SF_ACH_REMOVE BIT(0)
class CAchievementHandler : public CBaseDelay
{
	DECLARE_CLASS(CAchievementHandler, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	void Spawn(void);

	int Achievement; // achievement number
	int Value; // new value for the selected achievement
	int Mode;  // 3 modes: 0(def.): +Value, 1: -Value, 2: =Value 
	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(achievement_handler, CAchievementHandler);

BEGIN_DATADESC(CAchievementHandler)
	DEFINE_KEYFIELD(Achievement, FIELD_INTEGER, "achievement"),
	DEFINE_KEYFIELD(Value, FIELD_INTEGER, "value"),
	DEFINE_KEYFIELD(Mode, FIELD_INTEGER, "mode"),
END_DATADESC();

void CAchievementHandler::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "achievement"))
	{
		Achievement = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "value"))
	{
		Value = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "mode"))
	{
		Mode = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CAchievementHandler::Spawn(void)
{
	if (!Achievement)
	{
		ALERT(at_aiconsole, "Achievement field is empty in \"%s\"!\n", GetTargetname());
		UTIL_Remove(this);
		return;
	}
	else
		Achievement--; // achievements start from 0
}

void CAchievementHandler::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	const float PrevAchValue = pPlayer->AchievementStats[Achievement];

	pPlayer->SendAchievementStatToClient( Achievement, Value, Mode );

	ALERT(at_aiconsole, "Achievement %i affected by handler \"%s\" with value %i\n", Achievement, GetTargetname(), PrevAchValue, pPlayer->AchievementStats[Achievement], Value );

	if (HasSpawnFlags(SF_ACH_REMOVE))
		UTIL_Remove(this);
}