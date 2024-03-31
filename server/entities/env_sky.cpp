#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== ENV_SKY ==============================================
#define SF_ENVSKY_START_OFF	BIT( 0 )

class CEnvSky : public CPointEntity
{
	DECLARE_CLASS(CEnvSky, CPointEntity);
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};

LINK_ENTITY_TO_CLASS(env_sky, CEnvSky);

void CEnvSky::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "fov"))
	{
		pev->fuser2 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "customfog" ) )
	{
		int fog[4];
		UTIL_StringToIntArray( fog, 4, pkvd->szValue );
		pev->controller[0] = fog[0];
		pev->controller[1] = fog[1];
		pev->controller[2] = fog[2];
		pev->controller[3] = fog[3];
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}

void CEnvSky::Spawn(void)
{
	if (!FBitSet(pev->spawnflags, SF_ENVSKY_START_OFF))
		pev->effects |= (EF_MERGE_VISIBILITY | EF_SKYCAMERA);

	SetNullModel();
	SetBits(m_iFlags, MF_POINTENTITY);
	RelinkEntity(FALSE);

	SetFlag(F_NOBACKCULL);

	// disabled for now, because not used
/*
	// don't server-back-cull entities in skybox, ever
	// only first 512 ents will be counted
	edict_t *SkyEnt = UTIL_EntitiesInPVS( edict() );
	int SkyEntsCount = 0;
	while( !FNullEnt(SkyEnt) )
	{
		CBaseEntity *pEnt = (CBaseEntity *)GET_PRIVATE( SkyEnt );
		if ( !(pEnt->HasFlag(F_NOBACKCULL)) )
		{
			pEnt->SetFlag(F_NOBACKCULL);
			SkyEntsCount++;
			if( SkyEntsCount > 512 )
			{
				ALERT( at_warning, "Too many entities in skybox! > 512\n", SkyEntsCount );
				break;
			}
		}
			
		SkyEnt = SkyEnt->v.chain;
	}

	ALERT( at_console, "Entities in skybox PVS: %i\n", SkyEntsCount );
*/
}

void CEnvSky::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	int m_active = FBitSet(pev->effects, EF_SKYCAMERA);

	if (!ShouldToggle(useType, m_active))
		return;

	if (m_active)
		pev->effects &= ~(EF_MERGE_VISIBILITY | EF_SKYCAMERA);
	else
		pev->effects |= (EF_MERGE_VISIBILITY | EF_SKYCAMERA);
}