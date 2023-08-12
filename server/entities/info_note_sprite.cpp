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
	bool NoteUsed;
	CSprite *NoteSpr;
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( info_note_sprite, CNoteSprite );

BEGIN_DATADESC( CNoteSprite )
	DEFINE_FIELD( NoteUsed, FIELD_BOOLEAN),
	DEFINE_FIELD( NoteSpr, FIELD_CLASSPTR ),
END_DATADESC();

int CNoteSprite::ObjectCaps( void )
{
	if( NoteUsed )
		return 0;

	return FCAP_IMPULSE_USE;
}

void CNoteSprite::Precache(void)
{
	PRECACHE_MODEL( NOTE_ICON );
	if( pev->model )
		PRECACHE_MODEL( STRING( pev->model ) );
}

void CNoteSprite::Spawn(void)
{
	Precache();
	
	if( !(pev->model) )
	{
		ALERT( at_error, "info_note_sprite \"%s\" has no model specified!\n", GetTargetname() );
		UTIL_Remove( this );
		return;
	}

	NoteSpr = CSprite::SpriteCreate( NOTE_ICON, GetAbsOrigin(), FALSE );
	if( NoteSpr )
	{
		NoteSpr->SetTransparency( kRenderTransAdd, 0, 0, 0, 255, 0 );
		NoteSpr->SetScale( 0.05 );
		if( !(pev->iuser4) )
			NoteSpr->SetFadeDistance( 800 );
		else
			NoteSpr->SetFadeDistance( pev->iuser4 );
		NoteSpr->SetParent( this );
	}
	else
	{
		ALERT( at_error, "info_note_sprite \"%s\" - couldn't create the model!\n", STRING( pev->targetname ) );
		UTIL_Remove( this );
		return;
	}

	if( !(pev->iuser4) )
		SetFadeDistance( 800 );
	else
		SetFadeDistance( pev->iuser4 );
}

void CNoteSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;

	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	NoteUsed = true;
	ObjectCaps();

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( NoteSpr )
	{
	//	NoteSpr->pev->rendermode = kRenderNormal; rendering issues!
		NoteSpr->SetTransparency( kRenderTransTexture, 0, 0, 0, 255, 0 );
		NoteSpr->SetModel( STRING( pev->model ) );
	}
	

//	pPlayer->AchievementStats[ACH_NOTES]++;
	pPlayer->SendAchievementStatToClient( ACH_NOTES, 1, 0 );

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}