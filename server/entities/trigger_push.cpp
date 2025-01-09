#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "triggers.h"

// ========================== A TRIGGER THAT PUSHES YOU ===============================

#define SF_TRIG_PUSH_ONCE	BIT( 0 )
#define SF_TRIG_PUSH_CLIENTSONLY BIT(5) // diffusion - only affect clients and drone
#define SF_TRIG_PUSH_NOPLAYERDRONE BIT(6) // diffusion - don't push player's drone
#define SF_TRIG_PUSH_SETUP	BIT( 31 )

class CTriggerPush : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerPush, CBaseTrigger);
public:
	void Spawn(void);
	void Activate(void);
	void Touch(CBaseEntity* pOther);
	void UnderwaterPushThink(CBaseEntity* pOther); // diffusion - HACKHACK
	Vector m_vecPushDir;
};
LINK_ENTITY_TO_CLASS(trigger_push, CTriggerPush);

void CTriggerPush::Activate(void)
{
	if (!FBitSet(pev->spawnflags, SF_TRIG_PUSH_SETUP))
		return;	// already initialized

	ClearBits(pev->spawnflags, SF_TRIG_PUSH_SETUP);
	m_pGoalEnt = GetNextTarget();
}

/*QUAKED trigger_push (.5 .5 .5) ? TRIG_PUSH_ONCE
Pushes the player
*/

void CTriggerPush::Spawn()
{
	InitTrigger();

	if (pev->speed == 0)
		pev->speed = 100;

	if (pev->movedir == g_vecZero)
		pev->movedir = Vector(1.0f, 0.0f, 0.0f);

	// this flag was changed and flying barrels on c2a5 stay broken
	if (FStrEq(STRING(gpGlobals->mapname), "c2a5") && pev->spawnflags & 4)
		pev->spawnflags |= SF_TRIG_PUSH_ONCE;

	if (FBitSet(pev->spawnflags, SF_TRIGGER_PUSH_START_OFF))// if flagged to Start Turned Off, make trigger nonsolid.
		pev->solid = SOLID_NOT;

	SetUse(&CBaseTrigger::ToggleUse);
	RelinkEntity(); // Link into the list

	// trying to find push target like in Quake III Arena
	SetBits(pev->spawnflags, SF_TRIG_PUSH_SETUP);
}


void CTriggerPush::Touch(CBaseEntity* pOther)
{
	entvars_t* pevToucher = pOther->pev;

	if (pevToucher->solid == SOLID_NOT)
		return;

	if( pOther->IsProjectile() ) // diffusion - don't affect projectiles
		return;

	// UNDONE: Is there a better way than health to detect things that have physics? (clients/monsters)
	switch (pevToucher->movetype)
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:	// filter the movetype push here but pass the pushstep
	case MOVETYPE_NOCLIP:
	case MOVETYPE_FOLLOW:
		return;
	}

	if( HasSpawnFlags( SF_TRIG_PUSH_CLIENTSONLY ) && !pOther->IsPlayer() ) // also include player's drone
	{
		// this trigger pushes clients and their drones, but can allow the drone to pass
		// HACK kind of: use -662 check for player's drone (it sets on drone spawn)
		if( HasSpawnFlags( SF_TRIG_PUSH_NOPLAYERDRONE ) && pOther->pev->iuser3 == -662 )
			return;
		else if( pOther->pev->iuser3 != -662 )
			return;
	}

	// FIXME: If something is hierarchically attached, should we try to push the parent?
	if (pOther->m_hParent != NULL)
		return;

	if (!FNullEnt(m_pGoalEnt))
	{
		if (pev->dmgtime >= gpGlobals->time)
			return;

		if (!pOther->IsPlayer() || pOther->pev->movetype != MOVETYPE_WALK)
			return;

		float	time, dist, f;
		Vector	origin, velocity;

		origin = Center();

		// assume m_pGoalEnt is valid
		time = sqrt((m_pGoalEnt->pev->origin.z - origin.z) / (0.5f * CVAR_GET_FLOAT("sv_gravity")));

		if (!time)
		{
			UTIL_Remove(this);
			return;
		}

		velocity = m_pGoalEnt->GetAbsOrigin() - origin;
		velocity.z = 0.0f;
		dist = velocity.Length();
		velocity = velocity.Normalize();

		f = dist / time;
		velocity *= f;
		velocity.z = time * CVAR_GET_FLOAT("sv_gravity");

		pOther->ApplyAbsVelocityImpulse(velocity);

		if (pOther->GetAbsVelocity().z > 0)
			pOther->pev->flags &= ~FL_ONGROUND;

		//		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "world/jumppad.wav", VOL_NORM, ATTN_IDLE );

		pev->dmgtime = gpGlobals->time + (2.0f * gpGlobals->frametime);

		if (FBitSet(pev->spawnflags, SF_TRIG_PUSH_ONCE))
			UTIL_Remove(this);
		return;
	}

	m_vecPushDir = EntityToWorldTransform().VectorRotate(pev->movedir);

	// Instant trigger, just transfer velocity and remove
	if (FBitSet(pev->spawnflags, SF_TRIG_PUSH_ONCE))
	{
		pOther->ApplyAbsVelocityImpulse(pev->speed * m_vecPushDir);
		if (pOther->m_iActorType == ACTOR_DYNAMIC)
			WorldPhysic->AddForce(pOther, pev->speed * m_vecPushDir * (1.0f / gpGlobals->frametime) * 0.5f);

		if (pOther->GetAbsVelocity().z > 0)
			pOther->pev->flags &= ~FL_ONGROUND;
		UTIL_Remove(this);
	}
	else
	{
		// Push field, transfer to base velocity
		Vector vecPush = (pev->speed * m_vecPushDir);

		if (pevToucher->flags & FL_BASEVELOCITY)
			vecPush = vecPush + pOther->GetBaseVelocity();

		if (vecPush.z > 0 && FBitSet(pOther->pev->flags, FL_ONGROUND))
		{
			pOther->pev->flags &= ~FL_ONGROUND;
			Vector origin = pOther->GetAbsOrigin();
			origin.z += 1.0f;
			pOther->SetAbsOrigin(origin);
		}

		if (pOther->m_iActorType == ACTOR_DYNAMIC)
			WorldPhysic->AddForce(pOther, vecPush * (1.0f / gpGlobals->frametime) * 0.5f);
		pOther->SetBaseVelocity(vecPush);
		pevToucher->flags |= FL_BASEVELOCITY;
	}
}