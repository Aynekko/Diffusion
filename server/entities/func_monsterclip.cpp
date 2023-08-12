#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== FUNC_MONSTERCLIP ==============================================

class CFuncMonsterClip : public CBaseEntity
{
	DECLARE_CLASS(CFuncMonsterClip, CBaseEntity);
public:
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn(void);
};

LINK_ENTITY_TO_CLASS(func_monsterclip, CFuncMonsterClip);

void CFuncMonsterClip::Spawn(void)
{
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	SetBits(pev->flags, FL_MONSTERCLIP);
	SetBits(m_iFlags, MF_TRIGGER);
	pev->effects |= EF_NODRAW;

	// link into world
	SET_MODEL(edict(), GetModel());
}