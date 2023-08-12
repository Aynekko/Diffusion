#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//======================================================================
// diffusion - client-side rotating entity, brush or model.
// not solid at all times, designed to reduce traffic and to be used
// for simple things, like decorative rotating fans etc.
//======================================================================
#define SF_ROTCL_Y			BIT(0)
#define SF_ROTCL_X			BIT(1)
#define SF_ROTCL_REVERSE	BIT(2)
#define SF_ROTCL_STARTON	BIT(3)

/*
		iuser1 = state: 0 off, 1 on
		iuser2 = speed
		vuser1 = direction

		NOTENOTE: the position is obviously not saved.
*/

class CFuncRotatingClient : public CBaseDelay
{
	DECLARE_CLASS( CFuncRotatingClient, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( func_rotating_client, CFuncRotatingClient );

void CFuncRotatingClient::Precache( void )
{
	PRECACHE_MODEL( GetModel() );
}

void CFuncRotatingClient::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;

	SET_MODEL( edict(), GetModel() );

	pev->effects |= EF_ROTATING;

	// choose axis
	// UPD: this still needs fixes - this works well only for 0 0 0 angles
	if( HasSpawnFlags( SF_ROTCL_X ) )
		pev->vuser1 = Vector( 0, 0, 1 );
	else if( HasSpawnFlags( SF_ROTCL_Y ) )
		pev->vuser1 = Vector( 1, 0, 0 );
	else
		pev->vuser1 = Vector( 0, 1, 0 ); // y-axis

	// reverse direction
	if( HasSpawnFlags( SF_ROTCL_REVERSE ) )
		pev->vuser1 *= -1;

	// set speed
	if( pev->iuser2 == 0 )
		pev->iuser2 = RANDOM_LONG(100,350);

	// start ON
	if( HasSpawnFlags( SF_ROTCL_STARTON ) )
		pev->iuser1 = 1;
}

void CFuncRotatingClient::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	switch( pev->iuser1 )
	{
	case 0: pev->iuser1 = 1; break; // switch on
	case 1: pev->iuser1 = 0; break; // off
	}
}