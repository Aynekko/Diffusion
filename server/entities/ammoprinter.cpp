#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons/weapons.h"

//===========================================================================================================
// diffusion - very specific entity for Chapter 3 Blue Dimension
// it's a "3D printer" for ammunition, but basically here's what it does:
// 1) gives player ammo based on his active item
// 2) activates entities after press (in my case, indicators (3d printer is a world brush) - func_light, sprites etc. (Target1-5))
// 3) plays appropriate sounds (success, or no more presses)
//===========================================================================================================

/*
pev->iuser1 = number of presses, currently limited to 5, it is set by mapper
pev->iuser2 = counter of uses
*/

#define SF_AMMOPRINT_ONLYPRIMARYAMMO BIT(0) // give only primary ammo
#define SF_AMMOPRINT_NOALIENWEAPONS BIT(1) // do not give ammo for gausniper and ar2

class CAmmoPrinter : public CBaseDelay
{
	DECLARE_CLASS( CAmmoPrinter, CBaseDelay );
public:
	void Precache( void );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
	bool InvalidWeapon( CBasePlayerItem *m_pActiveItem );

	int SndAccept; // sound when player is given ammo
	int SndDeny; // out of tries or invalid weapon
	string_t Target1; // targets activated after each keypress
	string_t Target2;
	string_t Target3;
	string_t Target4;
	string_t Target5;

	float NextUseTime;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( ammoprinter, CAmmoPrinter );

BEGIN_DATADESC( CAmmoPrinter )
	DEFINE_KEYFIELD( SndAccept, FIELD_STRING, "sndaccept" ),
	DEFINE_KEYFIELD( SndDeny, FIELD_STRING, "snddeny" ),
	DEFINE_KEYFIELD( Target1, FIELD_STRING, "target1" ),
	DEFINE_KEYFIELD( Target2, FIELD_STRING, "target2" ),
	DEFINE_KEYFIELD( Target3, FIELD_STRING, "target3" ),
	DEFINE_KEYFIELD( Target4, FIELD_STRING, "target4" ),
	DEFINE_KEYFIELD( Target5, FIELD_STRING, "target5" ),
	DEFINE_FIELD( NextUseTime, FIELD_TIME ),
END_DATADESC();

void CAmmoPrinter::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "sndaccept" ) )
	{
		SndAccept = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "snddeny" ) )
	{
		SndDeny = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "target1" ) )
	{
		Target1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "target2" ) )
	{
		Target2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "target3" ) )
	{
		Target3 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "target4" ) )
	{
		Target4 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "target5" ) )
	{
		Target5 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CAmmoPrinter::Precache(void)
{
	if( SndAccept )
		PRECACHE_SOUND( (char *)STRING( SndAccept ) );

	if( SndDeny )
		PRECACHE_SOUND( (char *)STRING( SndDeny ) );
}

void CAmmoPrinter::Spawn( void )
{
	Precache();

	pev->iuser2 = 0; // reset the use number if mapper entered it...
	NextUseTime = gpGlobals->time;
	if( m_flWait <= 0 )
		m_flWait = 0;
}

void CAmmoPrinter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( !pPlayer )
		return;

	if( pev->iuser2 >= pev->iuser1 )
	{
		ALERT( at_aiconsole, "AmmoPrinter: number of prints exceeded!\n" );

		MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );
		WRITE_STRING( "AMMOPRINTER_EXCEEDED" );
		MESSAGE_END();

		if( SndDeny )
			EMIT_SOUND( edict(), CHAN_STATIC, (char *)STRING( SndDeny ), 1, ATTN_NORM );
		return;
	}
	
	if( IsLockedByMaster() )
		return;

	if( gpGlobals->time < NextUseTime )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );
		WRITE_STRING( "AMMOPRINTER_NOTREADY" );
		MESSAGE_END();

		if( SndDeny )
			EMIT_SOUND( edict(), CHAN_STATIC, (char *)STRING( SndDeny ), 1, ATTN_NORM );
		return;
	}

	if( !pPlayer->m_pActiveItem )
	{
		ALERT( at_aiconsole, "AmmoPrinter: no active item!\n" );

		MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );
		WRITE_STRING( "AMMOPRINTER_NOACTIVEWPN" );
		MESSAGE_END();

		if( SndDeny )
			EMIT_SOUND( edict(), CHAN_STATIC, (char *)STRING( SndDeny ), 1, ATTN_NORM );
		return;
	}

	if( InvalidWeapon(pPlayer->m_pActiveItem) )
	{
		ALERT( at_aiconsole, "AmmoPrinter: invalid weapon!\n" );
		
		MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );
		WRITE_STRING( "AMMOPRINTER_WPNINVALID" );
		MESSAGE_END();

		if( SndDeny )
			EMIT_SOUND( edict(), CHAN_STATIC, (char *)STRING( SndDeny ), 1, ATTN_NORM );
		return;
	}

	// all checks should be passed, so do the required things - give ammo
	// for one press, it gives half of possible ammo for this weapon, but I use 0.6 for a higher round-up
	int Ammo1 = pPlayer->GiveAmmo( (int)(pPlayer->m_pActiveItem->iMaxAmmo1() * 0.6), (char*)pPlayer->m_pActiveItem->pszAmmo1(), pPlayer->m_pActiveItem->iMaxAmmo1() );
	int Ammo2;
	if( HasSpawnFlags( SF_AMMOPRINT_ONLYPRIMARYAMMO ) )
		Ammo2 = -1; // secondary ammo not allowed
	else
		Ammo2 = pPlayer->GiveAmmo( 1, (char *)pPlayer->m_pActiveItem->pszAmmo2(), pPlayer->m_pActiveItem->iMaxAmmo2() );

	if( Ammo1 == -1 && Ammo2 == -1 )
	{
		ALERT( at_aiconsole, "AmmoPrinter: player can't have this ammo (full?)!\n" );
		
		MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );
		WRITE_STRING( "AMMOPRINTER_AMMOFULL" );
		MESSAGE_END();

		if( SndDeny )
			EMIT_SOUND( edict(), CHAN_STATIC, (char *)STRING( SndDeny ), 1, ATTN_NORM );
		return;
	}

	// at this point we are sure the ammo was given.
	PlayPickupSound( pPlayer );

	if( SndAccept )
		EMIT_SOUND( edict(), CHAN_STATIC, (char *)STRING( SndAccept ), 1, ATTN_NORM );

	pev->iuser2++;

	// and now activate the targets.
	CBaseEntity *pTarget = NULL;

	// unique targets for each give
	if( Target1 && (pev->iuser2 == 1) )
		UTIL_FireTargets( STRING( Target1 ), pActivator, this, useType, value );

	if( Target2 && (pev->iuser2 == 2) )
		UTIL_FireTargets( STRING( Target2 ), pActivator, this, useType, value );

	if( Target3 && (pev->iuser2 == 3) )
		UTIL_FireTargets( STRING( Target3 ), pActivator, this, useType, value );

	if( Target4 && (pev->iuser2 == 4) )
		UTIL_FireTargets( STRING( Target4 ), pActivator, this, useType, value );

	if( Target5 && (pev->iuser2 == 5) )
		UTIL_FireTargets( STRING( Target5 ), pActivator, this, useType, value );

	// constant target whenever the ammo is given
	if( pev->target )
		UTIL_FireTargets( pev->target, pActivator, this, useType, value );

	// delay next use
	NextUseTime = gpGlobals->time + m_flWait;
}

//=========================================================
// InvalidWeapon: can't have ammo printed for this weapon
//=========================================================
bool CAmmoPrinter::InvalidWeapon( CBasePlayerItem *m_pActiveItem )
{
	if( (m_pActiveItem->pszAmmo1() == NULL) && (m_pActiveItem->pszAmmo2() == NULL) )
		return true;

	if( HasSpawnFlags( SF_AMMOPRINT_NOALIENWEAPONS ) )
	{
		if( m_pActiveItem->m_iId == WEAPON_AR2 )
			return true;

		if( m_pActiveItem->m_iId == WEAPON_GAUSS )
			return true;
	}

	if( m_pActiveItem->m_iId == WEAPON_DRONE )
		return true;

	if( m_pActiveItem->m_iId == WEAPON_KNIFE )
		return true;

	if( m_pActiveItem->m_iId == WEAPON_SENTRY )
		return true;

	if( m_pActiveItem->m_iId == WEAPON_SATCHEL )
		return true;

	if( m_pActiveItem->m_iId == WEAPON_HANDGRENADE )
		return true;

	if( m_pActiveItem->m_iId == WEAPON_TRIPMINE )
		return true;
	
	return false;
}