#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"

// =================== ENV_RAIN ==============================================

class CEnvRain : public CPointEntity
{
	DECLARE_CLASS(CEnvRain, CPointEntity);
public:
	void KeyValue(KeyValueData* pkvd)
	{
		if (FStrEq(pkvd->szKeyName, "m_flDistance"))
		{
			pev->frags = Q_atof(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "m_iMode"))
		{
			pev->impulse = Q_atoi(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else
		{
			CPointEntity::KeyValue(pkvd);
		}
	}
};

LINK_ENTITY_TO_CLASS(env_rain, CEnvRain);

// =================== ENV_RAINMODIFY ==============================================

#define SF_RAIN_CONSTANT	BIT( 0 )

class CEnvRainModify : public CPointEntity
{
	DECLARE_CLASS(CEnvRainModify, CPointEntity);
public:
	void	Spawn(void);
	void	KeyValue(KeyValueData* pkvd);
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};

LINK_ENTITY_TO_CLASS(env_rainmodify, CEnvRainModify);

void CEnvRainModify::Spawn(void)
{
	if( FStringNull(pev->targetname) )
		SetBits(pev->spawnflags, SF_RAIN_CONSTANT);
}

void CEnvRainModify::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iDripsPerSecond"))
	{
		pev->impulse = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flWindX"))
	{
		pev->fuser1 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flWindY"))
	{
		pev->fuser2 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flRandX"))
	{
		pev->fuser3 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flRandY"))
	{
		pev->fuser4 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flTime"))
	{
		pev->dmg = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue(pkvd);
	}
}

void CEnvRainModify::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if( HasSpawnFlags(SF_RAIN_CONSTANT) )
		return; // constant

//	CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(1);
	// diffusion - fixed: send to all players
	CBasePlayer *pPlayer = NULL;

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if( !pPlayer )
			continue;

		if( pev->dmg )
		{
			// write to 'ideal' settings
			pPlayer->m_iRainIdealDripsPerSecond = pev->impulse;
			pPlayer->m_flRainIdealRandX = pev->fuser3;
			pPlayer->m_flRainIdealRandY = pev->fuser4;
			pPlayer->m_flRainIdealWindX = pev->fuser1;
			pPlayer->m_flRainIdealWindY = pev->fuser2;

			pPlayer->m_flRainEndFade = gpGlobals->time + pev->dmg;
			pPlayer->m_flRainNextFadeUpdate = gpGlobals->time + 1.0f;
		}
		else
		{
			pPlayer->m_iRainDripsPerSecond = pev->impulse;
			pPlayer->m_flRainRandX = pev->fuser3;
			pPlayer->m_flRainRandY = pev->fuser4;
			pPlayer->m_flRainWindX = pev->fuser1;
			pPlayer->m_flRainWindY = pev->fuser2;

			pPlayer->m_bRainNeedsUpdate = true;
		}
	}

	// diffusion - now copy this info into env_rain
	// 
	// search for env_rain entity
	CBaseEntity *pFind;
	pFind = UTIL_FindEntityByClassname( NULL, "env_rain" );
	if( !FNullEnt( pFind ) )
	{
		// rain allowed on this map
		CBaseEntity *pEnt = CBaseEntity::Instance( pFind->edict() );

		pEnt->pev->fuser1 = pev->fuser1;
		pEnt->pev->fuser2 = pev->fuser2;
		pEnt->pev->fuser3 = pev->fuser3;
		pEnt->pev->fuser4 = pev->fuser4;
		pEnt->pev->iuser1 = gpGlobals->time + pev->dmg;
		pEnt->pev->iuser2 = pev->impulse; // impulse is already busy with "mode"
	}
}
