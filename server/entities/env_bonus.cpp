#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "shake.h"
#include "weapons/weapons.h"

//==========================================================================
// diffusion - bonus entity for multiplayer, spawned on the place of dead player
//==========================================================================

extern int gEvilImpulse101;

class CEnvBonus : public CBaseDelay
{
	DECLARE_CLASS( CEnvBonus, CBaseDelay );
public:
	void Precache( void );
	void Spawn( void );
	void BonusTouch( CBaseEntity *pOther );
	void SpawnItemFor( char *szName, CBasePlayer *pPlayer );
	int EquipmentRandom;
};

LINK_ENTITY_TO_CLASS( env_bonus, CEnvBonus );

void CEnvBonus::Precache(void)
{
	
}

void CEnvBonus::Spawn( void )
{
	if( !g_pGameRules->IsMultiplayer() )
	{
		UTIL_Remove( this );
		return;
	}

	SET_MODEL( edict(), "models/bonus.mdl" );
	UTIL_SetSize( this, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->framerate = 1.0;
	pev->renderfx = kRenderFxFullbright;
	SetTouch( &CEnvBonus::BonusTouch );
	UTIL_DropToFloor( this );
	EquipmentRandom = RANDOM_LONG( 0, 8 );
	EMIT_SOUND( edict(), CHAN_STATIC, "items/bonus_spawn.wav", 1.0, ATTN_IDLE );
	SetFadeDistance( 800 );

	UTIL_SetOrigin( this, GetAbsOrigin() );
}

void CEnvBonus::BonusTouch( CBaseEntity *pOther )
{
	if( !pOther->IsPlayer() )
		return;

	if( !pOther->IsAlive() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	if( !pPlayer )
		return;
	
	EMIT_SOUND( edict(), CHAN_STATIC, "items/bonus_pickup.wav", 1.0, ATTN_IDLE );
	UTIL_ScreenFade( pOther, Vector( 255, 255, 255 ), 0.25, 0, 100, FFADE_IN );

	// equip the player
	pPlayer->pev->health += 25;
	pPlayer->m_flStaminaValue = 100;

	switch( EquipmentRandom )
	{
	default:
	case 0:
		SpawnItemFor( "weapon_crossbow", pPlayer );
		SpawnItemFor( "ammo_crossbow", pPlayer );
		break;
	case 1:
		SpawnItemFor( "weapon_tripmine", pPlayer );
		SpawnItemFor( "weapon_tripmine", pPlayer );
		SpawnItemFor( "weapon_tripmine", pPlayer );
		break;
	case 2:
		SpawnItemFor( "weapon_9mmAR", pPlayer );
		SpawnItemFor( "ammo_ARgrenades", pPlayer );
		break;
	case 3:
		SpawnItemFor( "weapon_ar2", pPlayer );
		SpawnItemFor( "ammo_ar2", pPlayer );
		SpawnItemFor( "ammo_ar2ball", pPlayer );
		SpawnItemFor( "ammo_ar2ball", pPlayer );
		break;
	case 4:
		SpawnItemFor( "weapon_rpg", pPlayer );
		SpawnItemFor( "ammo_rpgclip", pPlayer );
		SpawnItemFor( "ammo_rpgclip", pPlayer );
		SpawnItemFor( "ammo_rpgclip", pPlayer );
		break;
	case 5:
		SpawnItemFor( "weapon_sentry", pPlayer );
		break;
	case 6:
		SpawnItemFor( "weapon_sniper", pPlayer );
		SpawnItemFor( "ammo_sniper", pPlayer );
		break;
	case 7:
		SpawnItemFor( "weapon_g36c", pPlayer );
		SpawnItemFor( "ammo_g36c", pPlayer );
		break;
	case 8:
		SpawnItemFor( "weapon_satchel", pPlayer );
		SpawnItemFor( "weapon_satchel", pPlayer );
		break;
	}

	SetTouch( NULL );
	UTIL_Remove( this );
}

void CEnvBonus::SpawnItemFor( char *szName, CBasePlayer *pPlayer )
{
	if( !pPlayer )
		return;
	
	CBaseEntity *pEnt = Create( szName, pPlayer->GetAbsOrigin(), g_vecZero );
	if( pEnt )
	{
		pEnt->pev->effects |= EF_NODRAW;
		// diffusion - with this flag the item will delete itself, if player can't have it (ammo full)
		// this is to prevent the ammo/weapon respawn, i.e. from ammo crates
		pEnt->SetFlag( F_FROM_AMMOBOX );
		DispatchTouch( pEnt->edict(), pPlayer->edict() );
	}
}