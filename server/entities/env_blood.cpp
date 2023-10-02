#include "extdll.h"
#include "util.h"
#include "cbase.h"

// Blood effects
class CBlood : public CPointEntity
{
	DECLARE_CLASS(CBlood, CPointEntity);
public:
	void	Spawn(void);
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }
	void	KeyValue(KeyValueData* pkvd);

	inline	int Color(void) { return pev->impulse; }
	inline	float BloodAmount(void) { return pev->dmg; }

	inline	void SetColor(int color) { pev->impulse = color; }
	inline	void SetBloodAmount(float amount) { pev->dmg = amount; }

	Vector	Direction(void);
	Vector	BloodPosition(CBaseEntity* pActivator);
};

LINK_ENTITY_TO_CLASS(env_blood, CBlood);

#define SF_BLOOD_RANDOM		0x0001
#define SF_BLOOD_STREAM		0x0002
#define SF_BLOOD_PLAYER		0x0004
#define SF_BLOOD_DECAL		0x0008

void CBlood::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;
}

void CBlood::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color = Q_atoi(pkvd->szValue);
		switch (color)
		{
		case 1:
			SetColor(BLOOD_COLOR_YELLOW);
			break;
		default:
			SetColor(BLOOD_COLOR_RED);
			break;
		}

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "amount"))
	{
		SetBloodAmount( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}

Vector CBlood::Direction(void)
{
	if (pev->spawnflags & SF_BLOOD_RANDOM)
		return UTIL_RandomBloodVector();

	return pev->movedir;
}

Vector CBlood::BloodPosition(CBaseEntity* pActivator)
{
	if (pev->spawnflags & SF_BLOOD_PLAYER)
	{
		edict_t* pPlayer;

		if (pActivator && pActivator->IsPlayer())
		{
			pPlayer = pActivator->edict();
		}
		else
			pPlayer = INDEXENT(1);
		if (pPlayer)
			return (pPlayer->v.origin + pPlayer->v.view_ofs) + Vector(RANDOM_FLOAT(-10, 10), RANDOM_FLOAT(-10, 10), RANDOM_FLOAT(-10, 10));
	}

	return GetAbsOrigin();
}


void CBlood::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (pev->spawnflags & SF_BLOOD_STREAM)
		UTIL_BloodStream(BloodPosition(pActivator), Direction(), (Color() == BLOOD_COLOR_RED) ? 70 : Color(), BloodAmount());
	else
		UTIL_BloodDrips(BloodPosition(pActivator), Direction(), Color(), BloodAmount());

	if (pev->spawnflags & SF_BLOOD_DECAL)
	{
		Vector forward = Direction();
		Vector start = BloodPosition(pActivator);
		TraceResult tr;

		UTIL_TraceLine(start, start + forward * BloodAmount() * 2, ignore_monsters, NULL, &tr);
		if (tr.flFraction != 1.0)
			UTIL_BloodDecalTrace(&tr, Color());
	}
}