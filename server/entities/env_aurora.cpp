#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

// =================== ENV_AURORA ==============================================

#define SF_PARTICLE_START_ON		BIT( 0 )

class CBaseParticle : public CPointEntity
{
	DECLARE_CLASS(CBaseParticle, CPointEntity);
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual STATE GetState(void) { return (pev->renderfx == kRenderFxAurora) ? STATE_ON : STATE_OFF; };
	void StartMessage(CBasePlayer* pPlayer);
};

LINK_ENTITY_TO_CLASS(env_aurora, CBaseParticle);

void CBaseParticle::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "aurora"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attachment"))
	{
		pev->impulse = Q_atoi(pkvd->szValue);
		pev->impulse = bound(1, pev->impulse, 4);
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue(pkvd);
}

void CBaseParticle::StartMessage(CBasePlayer* pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgParticle, NULL, pPlayer->pev);
	WRITE_ENTITY(entindex());
	WRITE_STRING(STRING(pev->message));
	WRITE_BYTE(pev->impulse);
	MESSAGE_END();
}

void CBaseParticle::Spawn(void)
{
	SetNullModel();
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	RelinkEntity();

	if (FBitSet(pev->spawnflags, SF_PARTICLE_START_ON))
		pev->renderfx = kRenderFxAurora;
}

void CBaseParticle::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (useType == USE_TOGGLE)
	{
		if (GetState() == STATE_ON)
			useType = USE_OFF;
		else useType = USE_ON;
	}

	if (useType == USE_ON)
		pev->renderfx = kRenderFxAurora;
	else if (useType == USE_OFF)
		pev->renderfx = kRenderFxNone;
}