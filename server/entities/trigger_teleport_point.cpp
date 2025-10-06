#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"
#include "monsters.h"

//=======================================================================================================================
// diffusion - teleports entity regardless of its position (no teleport brush needed) - this entity also acts as a target
//=======================================================================================================================

#define PT_KEEP_VELOCITY	BIT(0) // keep the velocity of the entity
#define PT_KEEP_ANGLES		BIT(1) // don't snap to new angles
#define PT_REMOVEONFIRE		BIT(2)

// UNDONE: redirect player's velocity to entity's view origin?

class CTriggerTeleportPoint : public CBaseDelay
{
	DECLARE_CLASS(CTriggerTeleportPoint, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	string_t Entity;
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(trigger_teleport_point, CTriggerTeleportPoint);

BEGIN_DATADESC(CTriggerTeleportPoint)
	DEFINE_KEYFIELD(Entity, FIELD_STRING, "entityname"),
END_DATADESC();

void CTriggerTeleportPoint::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "entityname"))
	{
		Entity = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerTeleportPoint::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	CBaseEntity* pEntity = NULL;

	if (FStrEq(STRING(Entity), "player") || !Entity)
		pEntity = pPlayer;
	else
		pEntity = UTIL_FindEntityByTargetname(pEntity, STRING(Entity));

	if (!pEntity)
	{
		ALERT(at_error, "trigger_teleport_point \"%s\" can't find specified entity!\n", STRING(pev->targetname));
		return;
	}

	// this entity acts as a teleport target
	Vector TargetOrg = GetAbsOrigin();
	Vector TargetAng = GetAbsAngles();

	Vector EntityVelocity = g_vecZero;
	if (HasSpawnFlags(PT_KEEP_VELOCITY) )
		EntityVelocity = pEntity->GetAbsVelocity();

	Vector EntityAngles = TargetAng;
	if( HasSpawnFlags( PT_KEEP_ANGLES ) )
		EntityAngles = pEntity->GetAbsAngles();

	// offset to correctly teleport the player...
	if( pEntity->IsPlayer() )
	{
		TargetOrg.z += 36;
		if( !HasSpawnFlags( PT_KEEP_ANGLES ) )
		{
			pEntity->SetAbsAngles( TargetAng );
			pev->fixangle = TRUE;
		}
	}

	// wake up monster - if I teleport it, I want him up and running
	if( pEntity->IsMonster() && pEntity->HasSpawnFlags( SF_MONSTER_ASLEEP ) )
		pEntity->pev->spawnflags &= ~SF_MONSTER_ASLEEP;

	pEntity->Teleport(&TargetOrg, &EntityAngles, &EntityVelocity);

	UTIL_FireTargets( pev->target, pActivator, pCaller, USE_TOGGLE, 0 );

	if( HasSpawnFlags( PT_REMOVEONFIRE ) )
		UTIL_Remove( this );
}