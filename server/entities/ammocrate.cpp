#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "weapons/weapons.h"
#include "animation.h"

//==========================================================================
// diffusion - ammo crate (FIXME not solid, needs a brush - i.e. invisible one with metal texture)
//==========================================================================

#define MAX_EQUIP 32

#define SF_DYNAMIC_RESUPPLY BIT(0) // give the player what he needs the most

class CAmmoCrate : public CBaseAnimating
{
	DECLARE_CLASS(CAmmoCrate, CBaseAnimating);
public:
	void Precache(void);
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }
	void CrateEquip(void);
	void SimpleWait(void); // delay before crate can be opened again
	void ClearEffects( void );
	int Equipped;
	float EquipStartTime;
	int MaxOpen; // crate can be opened only # times
	int NumberOfUses; // keep count
	string_t m_weaponNames[MAX_EQUIP];
	int m_weaponCount[MAX_EQUIP];
	void GiveItems( CBasePlayer *pPlayer );
	void GiveDynamicAmmo( CBasePlayer *pPlayer, int WeaponID, float CurrentRatio );
	void DbgPrint( int Had, const char *AmmoType, int Max, int Given );
	int CalcGive( int MaxAmmo, float Ratio );

	// multiplayer only
	CSprite* AmmoIcon;
	EHANDLE m_hActivator;

	DECLARE_DATADESC();
};

BEGIN_DATADESC(CAmmoCrate)
	DEFINE_FUNCTION(CrateEquip),
	DEFINE_FUNCTION(SimpleWait),
	DEFINE_ARRAY(m_weaponCount, FIELD_INTEGER, MAX_EQUIP ),
	DEFINE_ARRAY(m_weaponNames, FIELD_STRING, MAX_EQUIP ),
	DEFINE_FIELD(EquipStartTime, FIELD_TIME),
	DEFINE_FIELD(Equipped, FIELD_INTEGER),
	DEFINE_FIELD(NumberOfUses, FIELD_INTEGER),
	DEFINE_FIELD(m_hActivator, FIELD_EHANDLE),
	DEFINE_KEYFIELD( MaxOpen, FIELD_INTEGER, "maxopen" ),
END_DATADESC();

LINK_ENTITY_TO_CLASS(ammocrate, CAmmoCrate);

void CAmmoCrate::KeyValue(KeyValueData* pkvd)
{
	CBaseAnimating::KeyValue(pkvd);

	if( FStrEq( pkvd->szKeyName, "maxopen" ) )
	{
		MaxOpen = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}

	if (!pkvd->fHandled)
	{
		for (int i = 0; i < MAX_EQUIP; i++)
		{
			if (!m_weaponNames[i])
			{
				char tmp[128];

				UTIL_StripToken(pkvd->szKeyName, tmp);

				m_weaponNames[i] = ALLOC_STRING(tmp);
				if (tmp != "gametext")
				{
					m_weaponCount[i] = Q_atoi(pkvd->szValue);
					m_weaponCount[i] = Q_max(1, m_weaponCount[i]);
				}
				pkvd->fHandled = TRUE;
				break;
			}
		}
	}
}

void CAmmoCrate::Precache( void )
{
	PRECACHE_SOUND( "gear/ammocrate_open.wav" );
	PRECACHE_SOUND( "gear/ammocrate_close.wav" );
	PRECACHE_MODEL( "models/ammocrate.mdl" );
}

void CAmmoCrate::Spawn(void)
{
	Precache();

	SET_MODEL(ENT(pev), "models/ammocrate.mdl");

	if (MaxOpen <= 0) // infinite
	{
		MaxOpen = -1;
		NumberOfUses = -2;
	}

	if (m_flWait < 0)
		m_flWait = 0;

	if (g_pGameRules->IsMultiplayer())
	{
		m_flWait = AMMO_RESPAWN_TIME;
		AmmoIcon = CSprite::SpriteCreate("sprites/diffusion/dif_ammo.spr", GetAbsOrigin() + Vector(0, 0, 30), FALSE);
		if (AmmoIcon)
		{
			AmmoIcon->SetTransparency(kRenderTransAdd, 0, 0, 0, 255, 0);
			AmmoIcon->SetScale(0.1);
			AmmoIcon->SetFadeDistance( 1000 );
		}
	}
}

void CAmmoCrate::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	if (NumberOfUses >= MaxOpen )
	{
		if (!HasFlag( F_ENTITY_UNUSEABLE ))
			SetFlag( F_ENTITY_UNUSEABLE );

		UTIL_ShowMessage("Crate is empty.", pActivator);
		return;
	}

	if ( MaxOpen > 0)
		NumberOfUses++;

	pev->sequence = LookupSequence("Open");
	pev->frame = 0;
	ResetSequenceInfo();

	EMIT_SOUND(edict(), CHAN_STATIC, "gear/ammocrate_open.wav", 1, ATTN_NORM);

	EquipStartTime = gpGlobals->time;
	Equipped = 0;

	SetFlag(F_ENTITY_BUSY);

	if (AmmoIcon)
		AmmoIcon->SetBrightness(0);

	m_hActivator = pActivator;

	SetThink(&CAmmoCrate::CrateEquip);
	SetNextThink(0);
}

void CAmmoCrate::CrateEquip(void)
{
	CBaseEntity* pActivator;
	CBasePlayer* pPlayer;

	if (m_hActivator == NULL)
		Equipped = 1;
	else
	{
		if (!(m_hActivator->IsAlive()))
			Equipped = 1;
		else
		{
			pActivator = UTIL_PlayerByIndex(m_hActivator->entindex());
			pPlayer = (CBasePlayer*)pActivator;
		}
	}

	if ((Equipped == 0) && (gpGlobals->time > EquipStartTime + 0.8))
	{
		GiveItems( pPlayer );

		// this is the last use. show an empty crate before closing the lid
		// UPD: no need
//		if( NumberOfUses == MaxOpen )
		pev->body = 1;

		SUB_UseTargets(pPlayer, USE_TOGGLE, 0);

		Equipped = 1;
	}

	if (Equipped == 1)
	{
		if( NumberOfUses == MaxOpen )
		{
			m_iFlags2 = 0;
			SetFlag( F_ENTITY_UNUSEABLE );
			return;
		}
		else
		{
			if( gpGlobals->time > EquipStartTime + 1.6 )
			{
				pev->sequence = LookupSequence( "Close" );
				pev->frame = 0;
				ResetSequenceInfo();
				Equipped = -1;
				m_iFlags2 = 0;
				SetFlag( F_ENTITY_UNUSEABLE );
			}
		}
	}

	// !!! this timing is hardcoded for the model animation
	// move sounds to model somehow?
	if ((Equipped == -1) && (gpGlobals->time > EquipStartTime + 2))
	{
		EMIT_SOUND(edict(), CHAN_STATIC, "gear/ammocrate_close.wav", 1, ATTN_NORM);
		SetThink(&CAmmoCrate::SimpleWait);
		SetNextThink(m_flWait);
		return;
	}

	SetThink(&CAmmoCrate::CrateEquip);
	SetNextThink(0.1);
}

void CAmmoCrate::SimpleWait(void)
{
	m_iFlags2 = 0;
	pev->sequence = LookupSequence("idle");
	pev->frame = 0;
	pev->body = 0;
	ResetSequenceInfo();
	if (AmmoIcon)
		AmmoIcon->SetBrightness(255);
	if (g_pGameRules->IsMultiplayer())
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, SND_AMMO_RESPAWN, 1, ATTN_NORM, 0, 150);
	SetThink(NULL);
	return;
}

void CAmmoCrate::GiveItems( CBasePlayer *pPlayer )
{
	if( !pPlayer )
		return;

	int i;

	pPlayer->SendAchievementStatToClient( ACH_AMMOCRATES, 1, ACHVAL_ADD );

	// give items written in the entity
	for( i = 0; i < MAX_EQUIP; i++ )
	{
		if( !m_weaponNames[i] )
			break;

		for( int j = 0; j < m_weaponCount[i]; j++ )
		{
			CBaseEntity *pEnt = Create( (char *)STRING( m_weaponNames[i] ), pPlayer->GetAbsOrigin(), g_vecZero );
			if( pEnt )
			{
				pEnt->pev->effects |= EF_NODRAW;
				// diffusion - with this flag the item will delete itself, if player can't have it (ammo full)
				// this is to prevent the ammo/weapon respawn, i.e. from ammo crates
				pEnt->SetFlag( F_FROM_AMMOBOX );
				DispatchTouch( pEnt->edict(), pPlayer->edict() );
			}
			else
			{
				ALERT( at_error, "ammocrate \"%s\" tried to spawn an unknown entity \"%s\"!\n", STRING( pev->targetname ), (char *)STRING( m_weaponNames[i] ) );
				break;
			}
		}
	}

	// dynamic resupply - give player what he needs the most
	if( !HasSpawnFlags( SF_DYNAMIC_RESUPPLY ) )
		return;

	// weapon ids which have the lowest ammo
	int LowestAmmoId1 = 0;
	int LowestAmmoId2 = 0;
	int LowestAmmoId3 = 0;
	float RatioAmmoId1 = 1.0f;
	float RatioAmmoId2 = 1.0f;
	float RatioAmmoId3 = 1.0f;
	float Ratio[TOTAL_WEAPONS];
	std::fill_n( Ratio, TOTAL_WEAPONS, 1.0f );
	bool bShotgunAmmoGiven = false;

	// starting from pistol
	for( i = 2; i < TOTAL_WEAPONS; i++ )
	{
		if( pPlayer->HasWeapon( i ) )
		{
			// check ammo and calculate ratio
			switch( i )
			{
			case WEAPON_BERETTA:
				Ratio[WEAPON_BERETTA] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_BERETTA] ) ) / _9MM_MAX_CARRY;
				break;
			case WEAPON_DEAGLE:
				Ratio[WEAPON_DEAGLE] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_DEAGLE] ) ) / DEAGLE_MAX_CARRY;
				break;
			case WEAPON_MRC:
				Ratio[WEAPON_MRC] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_MRC] ) ) / _9MM_MAX_CARRY;
				break;
			case WEAPON_CROSSBOW:
				Ratio[WEAPON_CROSSBOW] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_CROSSBOW] ) ) / BOLT_MAX_CARRY;
				break;
			case WEAPON_SHOTGUN:
				if( !bShotgunAmmoGiven )
				{
					Ratio[WEAPON_SHOTGUN] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_SHOTGUN] ) ) / BUCKSHOT_MAX_CARRY;
					bShotgunAmmoGiven = true;
				}
				break;
			case WEAPON_SHOTGUN_XM:
				if( !bShotgunAmmoGiven )
				{
					Ratio[WEAPON_SHOTGUN_XM] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_SHOTGUN_XM] ) ) / BUCKSHOT_MAX_CARRY;
					bShotgunAmmoGiven = true;
				}
				break;
		//	case WEAPON_GAUSS:
		//		Ratio[WEAPON_GAUSS] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_GAUSS] ) ) / URANIUM_MAX_CARRY;
		//		break;
		//	case WEAPON_AR2:
		//		Ratio[WEAPON_AR2] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_AR2] ) ) / AR2_MAX_CARRY;
		//		break;
			case WEAPON_HKMP5:
				Ratio[WEAPON_HKMP5] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_HKMP5] ) ) / _9MM_MAX_CARRY;
				break;
			case WEAPON_FIVESEVEN:
				Ratio[WEAPON_FIVESEVEN] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_FIVESEVEN] ) ) / _57_MAX_CARRY;
				break;
			case WEAPON_SNIPER:
				Ratio[WEAPON_SNIPER] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_SNIPER] ) ) / SNIPER_MAX_CARRY;
				break;
			case WEAPON_G36C:
				Ratio[WEAPON_G36C] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_G36C] ) ) / _9MM_MAX_CARRY;
				break;
		//	case WEAPON_RPG:
		//		Ratio[WEAPON_RPG] = (float)pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_RPG] ) ) / ROCKET_MAX_CARRY;
		//		break;
			}
		}
	}

	// lowest ammo 1
	float SmallestRatio = 1.0f;
	for( i = 2; i < TOTAL_WEAPONS; i++ )
	{
		if( pPlayer->HasWeapon( i ) && Ratio[i] < SmallestRatio )
		{
			SmallestRatio = Ratio[i];
			LowestAmmoId1 = i;
			RatioAmmoId1 = Ratio[i];
		}	
	}
	Ratio[LowestAmmoId1] = 1.0f;

	// lowest ammo 2
	SmallestRatio = 1.0f;
	for( i = 2; i < TOTAL_WEAPONS; i++ )
	{
		if( pPlayer->HasWeapon( i ) && Ratio[i] < SmallestRatio )
		{
			SmallestRatio = Ratio[i];
			LowestAmmoId2 = i;
			RatioAmmoId2 = Ratio[i];
		}
	}
	Ratio[LowestAmmoId2] = 1.0f;

	// lowest ammo 3
	SmallestRatio = 1.0f;
	for( i = 2; i < TOTAL_WEAPONS; i++ )
	{
		if( pPlayer->HasWeapon( i ) && Ratio[i] < SmallestRatio )
		{
			SmallestRatio = Ratio[i];
			LowestAmmoId3 = i;
			RatioAmmoId3 = Ratio[i];
		}
	}

	GiveDynamicAmmo( pPlayer, LowestAmmoId1, RatioAmmoId1 );
	GiveDynamicAmmo( pPlayer, LowestAmmoId2, RatioAmmoId2 * 0.8f );
	GiveDynamicAmmo( pPlayer, LowestAmmoId3, RatioAmmoId3 * 0.65f );

	PlayPickupSound( pPlayer );
}

void CAmmoCrate::DbgPrint( int Had, const char *AmmoType, int Max, int Given )
{
	ALERT( at_aiconsole, "GiveDynamicAmmo: player had %i of \"%s\" out of %i, given %i.\n", Had - Given, AmmoType, Max, Given );
}

int CAmmoCrate::CalcGive( int MaxAmmo, float Ratio )
{
	int out = (MaxAmmo - (MaxAmmo * Ratio)) * 0.35f;
	if( out < 1 ) out = 1;

	return out;
}

void CAmmoCrate::GiveDynamicAmmo( CBasePlayer *pPlayer, int WeaponID, float CurrentRatio )
{
	if( WeaponID == 0 || CurrentRatio == 1.0f || CurrentRatio < 0.0f )
		return;

	if( !pPlayer )
		return;

	int iGive = 0;

	switch( WeaponID )
	{
	case WEAPON_BERETTA:
		iGive = CalcGive( _9MM_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_BERETTA], _9MM_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_BERETTA] ) ), g_WpnAmmo[WEAPON_BERETTA], _9MM_MAX_CARRY, iGive );
		break;
	case WEAPON_DEAGLE:
		iGive = CalcGive( DEAGLE_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_DEAGLE], DEAGLE_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_DEAGLE] ) ), g_WpnAmmo[WEAPON_DEAGLE], DEAGLE_MAX_CARRY, iGive );
		break;
	case WEAPON_MRC:
		iGive = CalcGive( _9MM_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_MRC], _9MM_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_MRC] ) ), g_WpnAmmo[WEAPON_MRC], _9MM_MAX_CARRY, iGive );
		break;
	case WEAPON_CROSSBOW:
		iGive = CalcGive( BOLT_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_CROSSBOW], BOLT_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_CROSSBOW] ) ), g_WpnAmmo[WEAPON_CROSSBOW], BOLT_MAX_CARRY, iGive );
		break;
	case WEAPON_SHOTGUN:
		iGive = CalcGive( BUCKSHOT_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_SHOTGUN], BUCKSHOT_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_SHOTGUN] ) ), g_WpnAmmo[WEAPON_SHOTGUN], BUCKSHOT_MAX_CARRY, iGive );
		break;
	case WEAPON_GAUSS:
		iGive = CalcGive( URANIUM_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_GAUSS], URANIUM_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_GAUSS] ) ), g_WpnAmmo[WEAPON_GAUSS], URANIUM_MAX_CARRY, iGive );
		break;
	case WEAPON_AR2:
		iGive = CalcGive( AR2_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_AR2], AR2_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_AR2] ) ), g_WpnAmmo[WEAPON_AR2], AR2_MAX_CARRY, iGive );
		break;
	case WEAPON_HKMP5:
		iGive = CalcGive( _9MM_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_HKMP5], _9MM_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_HKMP5] ) ), g_WpnAmmo[WEAPON_HKMP5], _9MM_MAX_CARRY, iGive );
		break;
	case WEAPON_FIVESEVEN:
		iGive = CalcGive( _57_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_FIVESEVEN], _57_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_FIVESEVEN] ) ), g_WpnAmmo[WEAPON_FIVESEVEN], _57_MAX_CARRY, iGive );
		break;
	case WEAPON_SNIPER:
		iGive = CalcGive( SNIPER_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_SNIPER], SNIPER_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_SNIPER] ) ), g_WpnAmmo[WEAPON_SNIPER], SNIPER_MAX_CARRY, iGive );
		break;
	case WEAPON_SHOTGUN_XM:
		iGive = CalcGive( BUCKSHOT_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_SHOTGUN_XM], BUCKSHOT_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_SHOTGUN_XM] ) ), g_WpnAmmo[WEAPON_SHOTGUN_XM], BUCKSHOT_MAX_CARRY, iGive );
		break;
	case WEAPON_G36C:
		iGive = CalcGive( _9MM_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_G36C], _9MM_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_G36C] ) ), g_WpnAmmo[WEAPON_G36C], _9MM_MAX_CARRY, iGive );
		break;
	case WEAPON_RPG:
		iGive = CalcGive( ROCKET_MAX_CARRY, CurrentRatio );
		pPlayer->GiveAmmo( iGive, g_WpnAmmo[WEAPON_RPG], ROCKET_MAX_CARRY );
		DbgPrint( pPlayer->AmmoInventory( pPlayer->GetAmmoIndex( g_WpnAmmo[WEAPON_RPG] ) ), g_WpnAmmo[WEAPON_RPG], ROCKET_MAX_CARRY, iGive );
		break;
	}
}

void CAmmoCrate::ClearEffects( void )
{
	if( AmmoIcon )
	{
		UTIL_Remove( AmmoIcon );
		AmmoIcon = NULL;
	}
}