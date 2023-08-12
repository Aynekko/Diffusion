#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "game/game.h"
#include "triggers.h"

LINK_ENTITY_TO_CLASS(trigger, CBaseTrigger);

BEGIN_DATADESC(CBaseTrigger)
	DEFINE_FUNCTION(TeleportTouch),
	DEFINE_FUNCTION(MultiTouch),
	DEFINE_FUNCTION(HurtTouch),
	DEFINE_FUNCTION(MultiWaitOver),
	DEFINE_FUNCTION(CounterUse),
	DEFINE_FUNCTION(ToggleUse),
END_DATADESC()

/*
================
InitTrigger
================
*/
void CBaseTrigger::InitTrigger()
{
	// trigger angles are used for one-way touches.  An angle of 0 is assumed
	// to mean no restrictions, so use a yaw of 360 instead.
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;

	// diffusion
	if( pev->iuser1 <= 0 )
		pev->iuser1 = USE_TOGGLE;
	else if( pev->iuser1 == 1 )
		pev->iuser1 = USE_ON;
	else if( pev->iuser1 == 2 )
		pev->iuser1 = USE_OFF;

	// did mapper forget to set angles?
	if (pev->movedir == g_vecZero)
	{
		pev->movedir = Vector(1, 0, 0);
		SetLocalAngles(g_vecZero);
	}

	SET_MODEL(edict(), GetModel());    // set size and link into world
	SetBits(m_iFlags, MF_TRIGGER);
	pev->effects = EF_NODRAW;
}

BOOL CBaseTrigger::CanTouch(CBaseEntity* pOther)
{
	if( sv_ignore_triggers.value > 0 )
		return FALSE;
	
	if (FStringNull(pev->netname))
	{
		// Only touch clients, monsters, or pushables (depending on flags)
		if (pOther->pev->flags & FL_CLIENT)
			return !HasSpawnFlags(SF_TRIGGER_NOCLIENTS);
		else if (pOther->pev->flags & FL_MONSTER)
			return HasSpawnFlags(SF_TRIGGER_ALLOWMONSTERS);
		else if (pOther->IsPushable())
			return HasSpawnFlags(SF_TRIGGER_PUSHABLES);
		else if (pOther->IsRigidBody())
			return HasSpawnFlags(SF_TRIGGER_ALLOWPHYSICS);
		return FALSE;
	}
	else
	{
		// If netname is set, it's an entity-specific trigger; we ignore the spawnflags.
		if (!FClassnameIs(pOther, STRING(pev->netname)) &&
			(!pOther->pev->targetname || !FStrEq(pOther->GetTargetname(), STRING(pev->netname))))
			return FALSE;
	}
	return TRUE;
}

//
// Cache user-entity-field values until spawn is called.
//

void CBaseTrigger::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "count"))
	{
		m_cTriggersLeft = (int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damagetype"))
	{
		m_bitsDamageInflict = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue(pkvd);
}

//
// ToggleUse - If this is the USE function for a trigger, its state will toggle every time it's fired
//
void CBaseTrigger::ToggleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!ShouldToggle(useType, (pev->solid == SOLID_TRIGGER)))
		return;

	if (pev->solid == SOLID_NOT)
	{
		// if the trigger is off, turn it on
		pev->solid = SOLID_TRIGGER;
		// Force retouch
		gpGlobals->force_retouch++;
	}
	else
		pev->solid = SOLID_NOT; // turn the trigger off

	RelinkEntity(TRUE);
}

// When touched, a hurt trigger does DMG points of damage each half-second
void CBaseTrigger::HurtTouch(CBaseEntity* pOther)
{
	float fldmg;

	if( sv_ignore_triggers.value > 0 )
		return;

	if (!pOther->pev->takedamage)
		return;

	if (HasSpawnFlags(SF_TRIGGER_HURT_CLIENTONLYTOUCH) && !pOther->IsPlayer())
	{
		// this trigger is only allowed to touch clients, and this ain't a client.
		return;
	}

	if (HasSpawnFlags(SF_TRIGGER_HURT_NO_CLIENTS) && pOther->IsPlayer())
		return;

	// HACKHACK -- In multiplayer, players touch this based on packet receipt.
	// So the players who send packets later aren't always hurt.  Keep track of
	// how much time has passed and whether or not you've touched that player
	if (g_pGameRules->IsMultiplayer())
	{
		if (pev->dmgtime > gpGlobals->time)
		{
			if (gpGlobals->time != pev->pain_finished)
			{// too early to hurt again, and not same frame with a different entity
				if (pOther->IsPlayer())
				{
					int playerMask = 1 << (pOther->entindex() - 1);

					// If I've already touched this player (this time), then bail out
					if (pev->impulse & playerMask)
						return;

					// Mark this player as touched
					// BUGBUG - There can be only 32 players!
					pev->impulse |= playerMask;
				}
				else
					return;
			}
		}
		else
		{
			// New clock, "un-touch" all players
			pev->impulse = 0;
			if (pOther->IsPlayer())
			{
				int playerMask = 1 << (pOther->entindex() - 1);

				// Mark this player as touched
				// BUGBUG - There can be only 32 players!
				pev->impulse |= playerMask;
			}
		}
	}
	else	// Original code -- single player
	{
		if (pev->dmgtime > gpGlobals->time && gpGlobals->time != pev->pain_finished)
			return; // too early to hurt again, and not same frame with a different entity
	}



	// If this is time_based damage (poison, radiation), override the pev->dmg with a 
	// default for the given damage type.  Monsters only take time-based damage
	// while touching the trigger.  Player continues taking damage for a while after
	// leaving the trigger

	fldmg = pev->dmg * 0.5;	// 0.5 seconds worth of damage, pev->dmg is damage/second


	// JAY: Cut this because it wasn't fully realized.  Damage is simpler now.
#if 0
	switch (m_bitsDamageInflict)
	{
	default: break;
	case DMG_POISON:		fldmg = POISON_DAMAGE / 4; break;
	case DMG_NERVEGAS:		fldmg = NERVEGAS_DAMAGE / 4; break;
	case DMG_RADIATION:		fldmg = RADIATION_DAMAGE / 4; break;
	case DMG_PARALYZE:		fldmg = PARALYZE_DAMAGE / 4; break; // UNDONE: cut this? should slow movement to 50%
	case DMG_ACID:			fldmg = ACID_DAMAGE / 4; break;
	case DMG_SLOWBURN:		fldmg = SLOWBURN_DAMAGE / 4; break;
	case DMG_SLOWFREEZE:	fldmg = SLOWFREEZE_DAMAGE / 4; break;
	}
#endif

	if (fldmg < 0)
		pOther->TakeHealth(-fldmg, m_bitsDamageInflict);
	else
		pOther->TakeDamage(pev, pev, fldmg, m_bitsDamageInflict);

	// Store pain time so we can get all of the other entities on this frame
	pev->pain_finished = gpGlobals->time;

	// Apply damage every half second
	pev->dmgtime = gpGlobals->time + 0.5;// half second delay until this trigger can hurt toucher again



	if (pev->target)
	{
		// trigger has a target it wants to fire. 
		if (HasSpawnFlags(SF_TRIGGER_HURT_CLIENTONLYFIRE))
		{
			// if the toucher isn't a client, don't fire the target!
			if (!pOther->IsPlayer())
			{
				return;
			}
		}

		SUB_UseTargets(pOther, USE_TOGGLE, 0);
		if (HasSpawnFlags(SF_TRIGGER_HURT_TARGETONCE))
			pev->target = 0;
	}
}

void CBaseTrigger::MultiTouch(CBaseEntity* pOther)
{
	entvars_t* pevToucher;

	pevToucher = pOther->pev;

	// Only touch clients, monsters, or pushables (depending on flags)
	if (CanTouch(pOther))
	{

		// if the trigger has an angles field, check player's facing direction
		if (pev->movedir != g_vecZero && HasSpawnFlags(SF_TRIGGER_CHECKANGLES))
		{
			Vector vecDir = EntityToWorldTransform().VectorRotate(pev->movedir);

			UTIL_MakeVectors(pevToucher->angles);
			if (DotProduct(gpGlobals->v_forward, vecDir) < 0.0f)
				return; // not facing the right way
		}

		ActivateMultiTrigger(pOther);
	}
}


//
// the trigger was just touched/killed/used
// self.enemy should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
//
void CBaseTrigger::ActivateMultiTrigger(CBaseEntity* pActivator)
{
	if (pev->nextthink > gpGlobals->time)
		return;         // still waiting for reset time

	if (IsLockedByMaster(pActivator))
		return;

	if (FClassnameIs(pev, "trigger_secret"))
	{
		if (pev->enemy == NULL || !FClassnameIs(pev->enemy, "player"))
			return;
		gpGlobals->found_secrets++;
	}

	if (!FStringNull(pev->noise))
		EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise), 1, ATTN_NORM);

	// don't trigger again until reset
	// pev->takedamage = DAMAGE_NO;

	m_hActivator = pActivator;
	
	SUB_UseTargets(m_hActivator, (USE_TYPE)pev->iuser1, 0); // diffusion - use iuser1 for usetype.
	
	if (pev->message && pActivator->IsPlayer())
	{
		UTIL_ShowMessage(STRING(pev->message), pActivator);
		//		CLIENT_PRINTF( ENT( pActivator->pev ), print_center, STRING(pev->message) );
	}
	
	if (m_flWait > 0)
	{
		SetThink(&CBaseTrigger::MultiWaitOver);
		SetNextThink(m_flWait);
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch(NULL);
		
		SetNextThink(0.2);
		SetThink(&CBaseEntity::SUB_Remove);
	}
}


// the wait time has passed, so set back up for another activation
void CBaseTrigger::MultiWaitOver(void)
{
	SetThink(NULL);
}

// ========================= COUNTING TRIGGER =====================================

//
// GLOBALS ASSUMED SET:  g_eoActivator
//
