#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/saverestore.h"
#include "entities/func_break.h"
#include "decals.h"
#include "explode.h"

BEGIN_DATADESC(CPushable)
	DEFINE_FIELD(m_maxSpeed, FIELD_FLOAT),
	DEFINE_FIELD(m_soundTime, FIELD_TIME),
END_DATADESC()

LINK_ENTITY_TO_CLASS(func_pushable, CPushable);

const char* CPushable::m_soundNames[3] = { "debris/pushbox1.wav", "debris/pushbox2.wav", "debris/pushbox3.wav" };

void CPushable::Spawn(void)
{
	if (HasSpawnFlags(SF_PUSH_BREAKABLE))
		BaseClass::Spawn();
	else
		Precache();

	pev->movetype = MOVETYPE_PUSHSTEP;

	if( HasSpawnFlags( SF_PUSH_BSPCOLLISION ) )
		pev->solid = SOLID_BSP;
	else
		pev->solid = SOLID_BBOX;

	SET_MODEL(edict(), STRING(pev->model));

	if (pev->friction > 399)
		pev->friction = 399;

	m_maxSpeed = 400 - pev->friction;
	SetBits(pev->flags, FL_FLOAT);
	pev->friction = 0;

	Vector origin = GetLocalOrigin();
	origin.z += 1; // Pick up off of the floor
	UTIL_SetOrigin(this, origin);

	// Multiply by area of the box's cross-section (assume 1000 units^3 standard volume)
	pev->skin = (pev->skin * (pev->maxs.x - pev->mins.x) * (pev->maxs.y - pev->mins.y)) * 0.0005;
	m_soundTime = 0;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);
}


void CPushable::Precache(void)
{
	for (int i = 0; i < 3; i++)
		PRECACHE_SOUND(m_soundNames[i]);

	if (HasSpawnFlags(SF_PUSH_BREAKABLE))
		BaseClass::Precache();
}


void CPushable::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "size"))
	{
		int bbox = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

		switch (bbox)
		{
		case 0:	// Point
//			UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
			UTIL_SetSize( pev, Vector(-1,-1,-1), Vector(1,1,1) );
			break;

		case 2: // Big Hull!?!?	!!!BUGBUG Figure out what this hull really is
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN * 2, VEC_DUCK_HULL_MAX * 2);
			break;

		case 3: // Player duck
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			break;

		default:
		case 1: // Player
			UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
			break;
		}

	}
	else if (FStrEq(pkvd->szKeyName, "buoyancy"))
	{
		pev->skin = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}


// Pull the func_pushable
void CPushable::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
	{
		if (HasSpawnFlags(SF_PUSH_BREAKABLE))
			this->CBreakable::Use(pActivator, pCaller, useType, value);
		return;
	}

	if( HasSpawnFlags( SF_PUSH_USEPUSH ) )
	{
		// add force in player's direction
		if( (pActivator->pev->button & IN_FORWARD) && GetAbsVelocity().IsNull() )
		{
			UTIL_MakeVectors( pActivator->GetAbsAngles() );
			SetAbsVelocity( gpGlobals->v_forward * 500 );
		}
	}

	if( pActivator->GetAbsVelocity() != g_vecZero )
		Move( pActivator, 0 );
}


void CPushable::Touch(CBaseEntity* pOther)
{
	if( HasSpawnFlags( SF_PUSH_USEPUSH ) )
		return;
	
	if (pOther == g_pWorld)
		return;

	Move(pOther, 1);
}


void CPushable::Move(CBaseEntity* pOther, int push)
{
	entvars_t* pevToucher = pOther->pev;
	bool playerTouch = false;
	bool bUsePush = HasSpawnFlags( SF_PUSH_USEPUSH );

	// Now crossbow bolts, grenades roaches and other stuff with null size can't moving pushables anyway
	if (pOther->IsPointSized())
		return;

	// Is entity standing on this pushable ?
	if (FBitSet(pevToucher->flags, FL_ONGROUND) && pOther->GetGroundEntity() == this)
	{
		// Only push if floating
		if (pev->waterlevel > 0)
		{
			Vector vecVelocity = GetAbsVelocity();
			vecVelocity.z += pOther->GetAbsVelocity().z * 0.1;
			SetAbsVelocity(vecVelocity);
		}
		return;
	}

	// g-cont. fix pushable acceleration bug
	if( pOther->IsPlayer() )
	{
		if( !bUsePush )
		{
			// Don't push unless the player is pushing forward and NOT use (pull)
			if( push && !(pevToucher->button & (IN_FORWARD | IN_MOVERIGHT | IN_MOVELEFT | IN_BACK)) )
				return;
			// Require player walking back when applying '+use' on pushable
			if( !push && !(pevToucher->button & (IN_BACK)) )
				return;
		}

		playerTouch = true;
	}
	else
		bUsePush = false;

	TraceResult tr = UTIL_GetGlobalTrace();

	float factor = DotProduct(pevToucher->velocity.Normalize(), tr.vecPlaneNormal);

	if( bUsePush )
	{
		// quite simple, really
		Vector vPlayerVel = pOther->GetAbsVelocity();
		if( vPlayerVel.Length() > 100 )
		{
			// clamp player speed
			vPlayerVel = vPlayerVel.Normalize() * 100;
			pOther->SetAbsVelocity( vPlayerVel );
		}
		
		SetAbsVelocity( vPlayerVel );

		return;
	}

	if( push && factor >= -0.3f )
		return;	// just touching not pushed

	if( playerTouch )
	{
		if (!(pevToucher->flags & FL_ONGROUND))	// Don't push away from jumping/falling players unless in water
		{
			if (pev->waterlevel < 1)
				return;
			else
				factor = 0.1;
		}
		else
			factor = 1;
	}
	else
		factor = 0.25;

	Vector vecAbsVelocity = GetAbsVelocity();

	float length = vecAbsVelocity.Length2D();

	// diffusion - do not overaccelerate when pulling
	if( !push )
	{
		if( length < 150 )
		{
			vecAbsVelocity.x += pevToucher->velocity.x * factor;
			vecAbsVelocity.y += pevToucher->velocity.y * factor;
		}
	}
	else
	{
		vecAbsVelocity.x += pevToucher->velocity.x * factor;
		vecAbsVelocity.y += pevToucher->velocity.y * factor;
	}

	if (push && (length > MaxSpeed()))
	{
		vecAbsVelocity.x = (vecAbsVelocity.x * MaxSpeed() / length);
		vecAbsVelocity.y = (vecAbsVelocity.y * MaxSpeed() / length);
	}

	SetAbsVelocity(vecAbsVelocity);

	if( playerTouch )
	{
		Vector vecToucherVelocity = pOther->GetAbsVelocity();
		vecToucherVelocity.x = vecAbsVelocity.x;
		vecToucherVelocity.y = vecAbsVelocity.y;
		pOther->SetAbsVelocity(vecToucherVelocity);

		if ((gpGlobals->time - m_soundTime) > 0.7f)
		{
			m_soundTime = gpGlobals->time;
			if (length > 0 && FBitSet(pev->flags, FL_ONGROUND))
			{
				m_lastSound = RANDOM_LONG(0, 2);
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound], 0.5, ATTN_NORM);
			}
			else
				STOP_SOUND(ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound]);
		}
	}
}

int CPushable::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (HasSpawnFlags(SF_PUSH_BREAKABLE))
		return CBreakable::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);

	return 1;
}
