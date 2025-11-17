#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//==========================================================================
// diffusion - set specific health to player (or any other entity)
//==========================================================================
class CTriggerHealth : public CBaseDelay
{
	DECLARE_CLASS(CTriggerHealth, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	int NewHealth;
	int Mode;
	int ModeMaxHealth;
	string_t Entity;
	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(trigger_health, CTriggerHealth);

BEGIN_DATADESC(CTriggerHealth)
	DEFINE_KEYFIELD(NewHealth, FIELD_INTEGER, "newhp"),
	DEFINE_KEYFIELD(Entity, FIELD_STRING, "entityname"),
	DEFINE_KEYFIELD(Mode, FIELD_INTEGER, "mode"),
	DEFINE_KEYFIELD( ModeMaxHealth, FIELD_INTEGER, "modemaxhealth" ),
END_DATADESC();

void CTriggerHealth::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "newhp"))
	{
		NewHealth = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "entityname"))
	{
		Entity = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "mode"))
	{
		Mode = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "modemaxhealth" ) )
	{
		ModeMaxHealth = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerHealth::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	if( NewHealth < 0 )
		NewHealth = 0;

	CBaseEntity* pEntity = NULL;

	if( !Entity || FStrEq( STRING( Entity ), "player" ) )
	{
		pEntity = pPlayer;

		if( NewHealth > 0 )
		{
			switch( Mode )
			{
			case 0: pEntity->pev->health = NewHealth; break;
			case 1: pEntity->pev->health += NewHealth; break;
			case 2: pEntity->pev->health -= NewHealth; break;
			}
		}

		// set new max health
		if( pev->max_health > 0 )
		{
			switch( ModeMaxHealth )
			{
			case 0: pEntity->pev->max_health = pev->max_health; break;
			case 1: pEntity->pev->max_health += pev->max_health; break;
			case 2: pEntity->pev->max_health -= pev->max_health; break;
			}
		}
	}
	else
	{
		while( (pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( Entity ) )) != NULL )
		{
			if( NewHealth > 0 )
			{
				switch( Mode )
				{
				case 0: pEntity->pev->health = NewHealth; break;
				case 1: pEntity->pev->health += NewHealth; break;
				case 2: pEntity->pev->health -= NewHealth; break;
				}
			}

			// set new max health
			if( pev->max_health > 0 )
			{
				switch( ModeMaxHealth )
				{
				case 0: pEntity->pev->max_health = pev->max_health; break;
				case 1: pEntity->pev->max_health += pev->max_health; break;
				case 2: pEntity->pev->max_health -= pev->max_health; break;
				}
			}

			// bump max health if new health are bigger
			if( pEntity->pev->max_health < pEntity->pev->health )
				pEntity->pev->max_health = pEntity->pev->health;

			// force update HUD
			if( pEntity->IsPlayer() )
			{
				CBasePlayer *PL = (CBasePlayer *)pEntity;
				PL->m_iClientHealth = -1;
			}
		}
	}
}