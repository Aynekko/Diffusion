#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//==========================================================================
// diffusion - change objective text
//==========================================================================
#define SF_OBJECTIVE_NOSOUND	BIT(0)
#define SF_OBJECTIVE_CLEAR1		BIT(1)
#define SF_OBJECTIVE_CLEAR2		BIT(2)

class CObjectiveChanger : public CBaseDelay
{
	DECLARE_CLASS(CObjectiveChanger, CBaseDelay);
public:
	void Precache(void);
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	string_t NewObjective;
	int Mode;
	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(objective, CObjectiveChanger);

BEGIN_DATADESC(CObjectiveChanger)
	DEFINE_KEYFIELD(NewObjective, FIELD_STRING, "newobjective"),
	DEFINE_KEYFIELD(Mode, FIELD_INTEGER, "mode"),
END_DATADESC();

void CObjectiveChanger::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "newobjective"))
	{
		NewObjective = ALLOC_STRING(pkvd->szValue);
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

void CObjectiveChanger::Spawn(void)
{
	Precache();
}

void CObjectiveChanger::Precache(void)
{
	PRECACHE_SOUND("misc/objective.wav");
}

void CObjectiveChanger::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if( !pPlayer )
		return;

	if (IsLockedByMaster(pActivator))
		return;

	switch (Mode)
	{
	case 0:
	{
		if( NewObjective )
			pPlayer->Objective = NewObjective;
	}
	break;
	case 1:
	{
		if( NewObjective )
			pPlayer->Objective2 = NewObjective;
	}
	break;
	}

	if( HasSpawnFlags(SF_OBJECTIVE_CLEAR1) )
		pPlayer->Objective = NULL;

	if( HasSpawnFlags( SF_OBJECTIVE_CLEAR2 ) )
		pPlayer->Objective2 = NULL;

	if( !(HasSpawnFlags(SF_OBJECTIVE_NOSOUND)) )
	{
		if ( NewObjective )
		{
			UTIL_ShowMessage("OBJ_UPDATED", pPlayer); // titles.txt
			EMIT_SOUND(pActivator->edict(), CHAN_STATIC, "misc/objective.wav", 1, 0);
		}
	}

}