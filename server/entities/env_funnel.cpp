#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_FUNNEL_REVERSE		1 // funnel effect repels particles instead of attracting them.

//=========================================================
// FunnelEffect
//=========================================================
class CEnvFunnel : public CBaseDelay
{
	DECLARE_CLASS(CEnvFunnel, CBaseDelay);
public:
	void	Spawn(void);
	void	Precache(void);
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	int	m_iSprite;	// Don't save, precache
};

void CEnvFunnel::Precache(void)
{
	if (pev->netname)
		m_iSprite = PRECACHE_MODEL((char*)STRING(pev->netname));
	else
		m_iSprite = PRECACHE_MODEL("sprites/flare6.spr");
}

LINK_ENTITY_TO_CLASS(env_funnel, CEnvFunnel);

void CEnvFunnel::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Vector absOrigin = GetAbsOrigin();

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_LARGEFUNNEL);
	WRITE_COORD(absOrigin.x);
	WRITE_COORD(absOrigin.y);
	WRITE_COORD(absOrigin.z);
	WRITE_SHORT(m_iSprite);

	if (pev->spawnflags & SF_FUNNEL_REVERSE)// funnel flows in reverse?
	{
		WRITE_SHORT(1);
	}
	else
	{
		WRITE_SHORT(0);
	}


	MESSAGE_END();

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time;
}

void CEnvFunnel::Spawn(void)
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
}