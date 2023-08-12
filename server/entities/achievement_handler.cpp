#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//==========================================================================
// diffusion - achievement handler - apply a value to an achievement
//==========================================================================
/*
=========================================================================
------------ ACHIEVEMENT NUMBERS ------------------
this should match Achievements_e in player.h, but the number here is +1
 1 = total bullets fired
 2 = number of jumps
 3 = find all ammo crates
 4 = disarm enemy tripmines
 5 = kill # enemies
 6 = inflict a total of # damage
 7 = kill # enemies with a stationary sniper rifle
 8 = complete chapter 1
 9 = complete chapter 2
 10 = complete chapter 3
 11 = complete chapter 4
 12 = kill security general under 30 sec
 13 = regenerate a total of # health
 14 = receive a total of # damage
 15 = overcook the grenade
 16 = kill # security drones
 17 = kill # alien drones
 18 = kill # enemies with crossbow
 19 = kill the alien military ship with balls
 20 = dash # times
 21 = find # (all?) notes
 22 = find # (all?) secrets
 23 = kill # enemies with balls (weapon_ar2 or func_tankball)
 24 = help the red dweller
 25 = get the first blast level by assembling the module on ch2map2
 26 = break the car completely in chapter 1 intro
 27 = travelled distance by car
 28 = travelled distance by water jet
 29 = kill # bots in multiplayer
 30 = don't kill any dwellers in chapter 3
============================================================================
*/

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

	float PrevAchValue = pPlayer->AchievementStats[Achievement];

//	switch (Mode)
//	{
//	case 0: pPlayer->AchievementStats[Achievement] += Value; break;
//	case 1: pPlayer->AchievementStats[Achievement] -= Value; break;
//	case 2: pPlayer->AchievementStats[Achievement] = Value; break;
//	}
	pPlayer->SendAchievementStatToClient( Achievement, Value, Mode );

	ALERT(at_aiconsole, "Achievement %i affected by handler \"%s\" with value %i\n", Achievement, GetTargetname(), PrevAchValue, pPlayer->AchievementStats[Achievement], Value );

	if (HasSpawnFlags(SF_ACH_REMOVE))
		UTIL_Remove(this);
}