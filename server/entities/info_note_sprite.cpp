#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"
#include "effects.h"

//=======================================================================================================================
// diffusion - simple sprite note, to reduce the amount of entites on the map
//=======================================================================================================================

#define NOTE_ICON "sprites/diffusion/dif_note.spr"

class CNoteSprite : public CBaseDelay
{
	DECLARE_CLASS( CNoteSprite, CBaseDelay );
public:
	void Precache( void );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void );

	bool AchievementCounted;
	DECLARE_DATADESC();
};

BEGIN_DATADESC( CNoteSprite )
	DEFINE_FIELD( AchievementCounted, FIELD_BOOLEAN ),
END_DATADESC();

LINK_ENTITY_TO_CLASS( info_note_sprite, CNoteSprite );

int CNoteSprite::ObjectCaps( void )
{
	return FCAP_IMPULSE_USE;
}

void CNoteSprite::Precache(void)
{
	PRECACHE_MODEL( NOTE_ICON );
}

void CNoteSprite::Spawn(void)
{
	Precache();
	
	if( !pev->message )
	{
		ALERT( at_error, "info_note_sprite \"%s\" has no message specified!\n", GetTargetname() );
		UTIL_Remove( this );
		return;
	}

	SET_MODEL( edict(), NOTE_ICON );
	UTIL_SetSize( pev, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ) );
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 200;
	pev->renderfx = kRenderFxPulseSlowWide;
	pev->scale = 0.05;
	if( !(pev->iuser4) )
		SetFadeDistance( 800 );

	AchievementCounted = false;
}

void CNoteSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;

	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( !pPlayer )
		return;
	
	MESSAGE_BEGIN( MSG_ONE, gmsgShowNote, NULL, pPlayer->pev );
		WRITE_STRING( STRING( pev->message ) );
	MESSAGE_END();

	if( !AchievementCounted )
	{
		pPlayer->SendAchievementStatToClient( ACH_NOTES, 1, ACHVAL_ADD );
		SUB_UseTargets( pActivator, USE_TOGGLE, 0 ); // fire target only once.
		AchievementCounted = true;

		// mark as read :)
		pev->renderamt = 25;
		pev->renderfx = 0;
	}
}