#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== INFO_TARGET ==============================================

#define SF_TARGET_HACK_VISIBLE	BIT( 0 )

class CInfoTarget : public CPointEntity
{
	DECLARE_CLASS(CInfoTarget, CPointEntity);
public:
	void Spawn(void);
};

LINK_ENTITY_TO_CLASS(info_target, CInfoTarget);

void CInfoTarget::Spawn(void)
{
	pev->solid = SOLID_NOT;
	SetBits(m_iFlags, MF_POINTENTITY);

	if (HasSpawnFlags(SF_TARGET_HACK_VISIBLE))
	{
		SetNullModel();
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
	}
}