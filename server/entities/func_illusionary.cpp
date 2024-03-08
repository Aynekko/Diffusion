#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== FUNC_ILLUSIONARY ==============================================

#define SF_ILLUSIONARY_STARTOFF BIT(0) // spawn invisible

class CFuncIllusionary : public CBaseEntity
{
	DECLARE_CLASS(CFuncIllusionary, CBaseEntity);
public:
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn(void);

	// diffusion - toggle visibility
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};

LINK_ENTITY_TO_CLASS(func_illusionary, CFuncIllusionary);

void CFuncIllusionary::Spawn(void)
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT; // always solid_not 
	SET_MODEL(edict(), GetModel());

	// diffusion - toggle visibility
	if (HasSpawnFlags(SF_ILLUSIONARY_STARTOFF))
		pev->effects |= EF_NODRAW;
}

void CFuncIllusionary::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if( IsLockedByMaster() )
		return;
	
	if( useType == USE_OFF )
		pev->effects |= EF_NODRAW;
	else if( useType == USE_ON )
		pev->effects &= ~EF_NODRAW;
	else // toggle
	{
		if( pev->effects & EF_NODRAW )
			pev->effects &= ~EF_NODRAW;
		else
			pev->effects |= EF_NODRAW;
	}
}