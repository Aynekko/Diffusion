#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== ENV_BUBBLES =================================================
// diffusion: new version on particles which aims to be identical to original entity
// =================================================================================

/*
iuser4 = particle visible distance
fuser1 = density
fuser2 = frequency
fuser3 = sin speed
*/

#define SF_BUBBLES_STARTOFF		0x0001

class CBubbles : public CBaseEntity
{
	DECLARE_CLASS( CBubbles, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_bubbles, CBubbles );

void CBubbles::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "density" ) )
	{
		pev->fuser1 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "frequency" ) )
	{
		pev->fuser2 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "current" ) )
	{
		pev->fuser3 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CBubbles::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	pev->effects |= EF_DIMLIGHT;
	pev->iuser3 = -663; // this entity is distinguished on client by "dimlight + iuser3"

	SET_MODEL( edict(), GetModel() );

	if( HasSpawnFlags( SF_BUBBLES_STARTOFF ) )
		pev->effects |= EF_NODRAW;
}

void CBubbles::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// toggle the bubbles
	if( pev->effects & EF_NODRAW )
		pev->effects &= ~EF_NODRAW;
	else
		pev->effects |= EF_NODRAW;
}