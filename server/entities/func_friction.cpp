#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/saverestore.h"
#include "entities/trains.h"			// trigger_camera has train functionality
#include "game/gamerules.h"
#include "talkmonster.h"
#include "weapons/weapons.h"
#include "triggers.h"

class CFrictionModifier : public CBaseEntity
{
	DECLARE_CLASS(CFrictionModifier, CBaseEntity);
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void ChangeFriction(CBaseEntity* pOther);
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	float		m_frictionFraction;		// Sorry, couldn't resist this name :)
};

LINK_ENTITY_TO_CLASS(func_friction, CFrictionModifier);

// Global Savedata for changelevel friction modifier
BEGIN_DATADESC(CFrictionModifier)
	DEFINE_FIELD(m_frictionFraction, FIELD_FLOAT),
	DEFINE_FUNCTION(ChangeFriction),
END_DATADESC()

// Modify an entity's friction
void CFrictionModifier::Spawn(void)
{
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->movetype = MOVETYPE_NONE;
	SetTouch(&CFrictionModifier::ChangeFriction);
}


// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier::ChangeFriction(CBaseEntity* pOther)
{
	switch (pOther->pev->movetype)
	{
	case MOVETYPE_WALK:
	case MOVETYPE_STEP:
	case MOVETYPE_FLY:
	case MOVETYPE_TOSS:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_PUSHSTEP:
		pOther->pev->friction = m_frictionFraction;
		break;
	}
}

// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "modifier"))
	{
		m_frictionFraction = Q_atof(pkvd->szValue) / 100.0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue(pkvd);
}