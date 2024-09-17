#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "talkmonster.h"

//================================================================================
//diffusion - player customize
//================================================================================

#define SF_PLCUSTOMIZE_AFFECTALL BIT(0) // affect all players

class CPlayerCustomize : public CBaseDelay
{
	DECLARE_CLASS(CPlayerCustomize, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	//  virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	int BrokenSuit; // 0 no change, 1 toggle, 2 on, 3 off
	int RegenActive;
	int HideWeapons;
	int CanDash;
	int CanUse;
	int CanJump;
	int CanSprint;
	int CanMove;
	int WeaponLowered;
	int CanShoot;
	int BlastAbilityLVL;
	int HUDSuitOffline;
	int CanSelectEmptyWeapon;
	int BreathingEffect;
	int WigglingEffect;
	int ShieldAvailableLVL;
	int UpsideDown;
	int LoudWeaponsRestricted;
	int PlayingDrums;
	int DrunkLevel;
	int CanUseDrone;
	bool DeletePlayerTurrets;
	string_t DroneTarget_OnDeploy;
	string_t DroneTarget_OnReturn;
	string_t DroneTarget_OnEnteringFirstPerson;
	string_t DroneTarget_OnLeavingFirstPerson;
	void KeyValue(KeyValueData* pkvd);
	void Affect( CBasePlayer *pPlayer );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(player_customize, CPlayerCustomize);

BEGIN_DATADESC(CPlayerCustomize)
	DEFINE_KEYFIELD(BrokenSuit, FIELD_INTEGER, "brokensuit"),
	DEFINE_KEYFIELD(RegenActive, FIELD_INTEGER, "regeneration"),
	DEFINE_KEYFIELD(HideWeapons, FIELD_INTEGER, "hideweapons"),
	DEFINE_KEYFIELD(CanDash, FIELD_INTEGER, "dash"),
	DEFINE_KEYFIELD(CanUse, FIELD_INTEGER, "use"),
	DEFINE_KEYFIELD(CanJump, FIELD_INTEGER, "jump"),
	DEFINE_KEYFIELD(CanSprint, FIELD_INTEGER, "sprint"),
	DEFINE_KEYFIELD(WeaponLowered, FIELD_INTEGER, "wpn_lowered"),
	DEFINE_KEYFIELD(CanShoot, FIELD_INTEGER, "canshoot"),
	DEFINE_KEYFIELD( CanMove, FIELD_INTEGER, "move" ),
	DEFINE_KEYFIELD( BlastAbilityLVL, FIELD_INTEGER, "blastabilitylvl" ),
	DEFINE_KEYFIELD( HUDSuitOffline, FIELD_INTEGER, "suitoffline" ),
	DEFINE_KEYFIELD( CanSelectEmptyWeapon, FIELD_INTEGER, "canselectempw"),
	DEFINE_KEYFIELD( BreathingEffect, FIELD_INTEGER, "breathingeff" ),
	DEFINE_KEYFIELD( WigglingEffect, FIELD_INTEGER, "wigglingeff" ),
	DEFINE_KEYFIELD( ShieldAvailableLVL, FIELD_INTEGER, "shieldlevel" ),
	DEFINE_KEYFIELD( UpsideDown, FIELD_INTEGER, "upsidedown" ),
	DEFINE_KEYFIELD( LoudWeaponsRestricted, FIELD_INTEGER, "loudwep"),
	DEFINE_KEYFIELD( PlayingDrums, FIELD_INTEGER, "drums" ),
	DEFINE_KEYFIELD( DrunkLevel, FIELD_INTEGER, "drunklevel"),
	DEFINE_KEYFIELD( CanUseDrone, FIELD_INTEGER, "canusedrone" ),
	DEFINE_KEYFIELD( DeletePlayerTurrets, FIELD_BOOLEAN, "delturrets" ),
	DEFINE_KEYFIELD( DroneTarget_OnDeploy, FIELD_STRING, "ondeploydrone" ),
	DEFINE_KEYFIELD( DroneTarget_OnReturn, FIELD_STRING, "onreturndrone" ),
	DEFINE_KEYFIELD( DroneTarget_OnEnteringFirstPerson, FIELD_STRING, "onentering1stperson" ),
	DEFINE_KEYFIELD( DroneTarget_OnLeavingFirstPerson, FIELD_STRING, "onleaving1stperson" ),
END_DATADESC()

void CPlayerCustomize::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "brokensuit"))
	{
		BrokenSuit = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "regeneration"))
	{
		RegenActive = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "hideweapons"))
	{
		HideWeapons = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "dash"))
	{
		CanDash = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "use"))
	{
		CanUse = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "jump"))
	{
		CanJump = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sprint"))
	{
		CanSprint = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wpn_lowered" ) )
	{
		WeaponLowered = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "canshoot" ) )
	{
		CanShoot = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "move" ) )
	{
		CanMove = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "blastabilitylvl" ) )
	{
		BlastAbilityLVL = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "suitoffline" ) )
	{
		HUDSuitOffline = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "canselectempw" ) )
	{
		CanSelectEmptyWeapon = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "breathingeff" ) )
	{
		BreathingEffect = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wigglingeff" ) )
	{
		WigglingEffect = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "shieldlevel" ) )
	{
		ShieldAvailableLVL = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "upsidedown" ) )
	{
		UpsideDown = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "loudwep" ) )
	{
		LoudWeaponsRestricted = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "drums" ) )
	{
		PlayingDrums = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "drunklevel" ) )
	{
		DrunkLevel = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "canusedrone" ) )
	{
		CanUseDrone = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "delturrets" ) )
	{
		DeletePlayerTurrets = (Q_atoi( pkvd->szValue ) > 0);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "ondeploydrone" ) )
	{
		DroneTarget_OnDeploy = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "onreturndrone" ) )
	{
		DroneTarget_OnReturn = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "onentering1stperson" ) )
	{
		DroneTarget_OnEnteringFirstPerson = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "onleaving1stperson" ) )
	{
		DroneTarget_OnLeavingFirstPerson = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CPlayerCustomize::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( HasSpawnFlags(SF_PLCUSTOMIZE_AFFECTALL) )
	{
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
			Affect( pPlayer );
		}
	}
	else
		Affect( pPlayer );
}

void CPlayerCustomize::Affect( CBasePlayer *pPlayer )
{
	if( !pPlayer )
		return;

	switch( RegenActive )
	{
	case 1: pPlayer->CanRegenerateHealth = !pPlayer->CanRegenerateHealth; break; // toggle
	case 2: pPlayer->CanRegenerateHealth = true; break; // on
	case 3: pPlayer->CanRegenerateHealth = false; break; // off
	}

	switch( BrokenSuit )
	{
	case 1: pPlayer->BrokenSuit = !pPlayer->BrokenSuit; break;// toggle
	case 2: pPlayer->BrokenSuit = true; break; // on
	case 3: pPlayer->BrokenSuit = false; break; // off
	}

	switch( HideWeapons )
	{
	case 1: // toggle
		if( pPlayer->m_iHideHUD & HIDEHUD_WPNS )
		{
			pPlayer->m_iHideHUD &= ~HIDEHUD_WPNS_CUSTOM; // remove flag first to allow HideWeapons to work
			pPlayer->HideWeapons( FALSE );
		}
		else
		{
			pPlayer->HideWeapons( TRUE );
			pPlayer->m_iHideHUD |= HIDEHUD_WPNS_CUSTOM;
		}
		break;
	case 2: // hide weapons
		pPlayer->HideWeapons( TRUE );
		pPlayer->m_iHideHUD |= HIDEHUD_WPNS_CUSTOM;
		break;
	case 3: // show weapons
		pPlayer->m_iHideHUD &= ~HIDEHUD_WPNS_CUSTOM;
		pPlayer->HideWeapons( FALSE );
		break;
	}

	switch( CanDash )
	{
	case 1: pPlayer->CanDash = !pPlayer->CanDash; break; // toggle
	case 2: pPlayer->CanDash = true; break; // on
	case 3: pPlayer->CanDash = false; break; // off
	}

	switch( CanUse )
	{
	case 1: pPlayer->CanUse = !pPlayer->CanUse; break;// toggle
	case 2: pPlayer->CanUse = true; break; // on
	case 3: pPlayer->CanUse = false; break; // off
	}

	switch( CanJump )
	{
	case 1: pPlayer->CanJump = !pPlayer->CanJump; break; // toggle
	case 2: pPlayer->CanJump = true; break; // on
	case 3: pPlayer->CanJump = false; break; // off
	}

	switch( CanSprint )
	{
	case 1: pPlayer->CanSprint = !pPlayer->CanSprint; break; // toggle
	case 2: pPlayer->CanSprint = true; break; // on
	case 3: pPlayer->CanSprint = false; break; // off
	}

	switch( WeaponLowered )
	{
	case 1: pPlayer->WeaponLowered = !pPlayer->WeaponLowered; break; // toggle
	case 2: pPlayer->WeaponLowered = true; break; // on
	case 3: pPlayer->WeaponLowered = false; break; // off
	}

	switch( CanShoot )
	{
	case 1: pPlayer->CanShoot = !pPlayer->CanShoot; break; // toggle
	case 2: pPlayer->CanShoot = true; break; // on
	case 3: pPlayer->CanShoot = false; break; // off
	}

	switch( CanMove )
	{
	case 1: pPlayer->CanMove = !pPlayer->CanMove; break; // toggle
	case 2: pPlayer->CanMove = true; break; // on
	case 3: pPlayer->CanMove = false; break; // off
	}

	switch( BlastAbilityLVL )
	{
	case 1: pPlayer->BlastAbilityLVL = -1; UTIL_ShowMessage( "SUIT_BLAST1", pPlayer ); break; // we need to show the tutorial for first-time upgrade
	case 2: pPlayer->BlastAbilityLVL = 2; UTIL_ShowMessage( "SUIT_BLAST2", pPlayer ); break;
	case 3: pPlayer->BlastAbilityLVL = 3; UTIL_ShowMessage( "SUIT_BLAST3", pPlayer ); break;
	case 4: pPlayer->BlastAbilityLVL = 0; break; // disable the ability
	case 5:
		pPlayer->CanElectroBlast = false;
		MESSAGE_BEGIN( MSG_ONE, gmsgBlastIcons, NULL, pPlayer->pev );
		WRITE_BYTE( pPlayer->BlastAbilityLVL );
		WRITE_BYTE( pPlayer->BlastChargesReady );
		WRITE_BYTE( pPlayer->CanElectroBlast );
		MESSAGE_END();
		pPlayer->m_iClientBlastAbilityLVL = pPlayer->BlastAbilityLVL;
		break; // don't allow to use ability (if present)
	case 6:
		pPlayer->CanElectroBlast = true;
		MESSAGE_BEGIN( MSG_ONE, gmsgBlastIcons, NULL, pPlayer->pev );
		WRITE_BYTE( pPlayer->BlastAbilityLVL );
		WRITE_BYTE( pPlayer->BlastChargesReady );
		WRITE_BYTE( pPlayer->CanElectroBlast );
		MESSAGE_END();
		pPlayer->m_iClientBlastAbilityLVL = pPlayer->BlastAbilityLVL;
		break; // allow to use (if present)
	case 7: // increase ability by 1 lvl
		if( pPlayer->BlastAbilityLVL == 0 )
		{
			pPlayer->BlastAbilityLVL = -1;
			UTIL_ShowMessage( "SUIT_BLAST1", pPlayer ); break;
		}
		else
		{
			if( pPlayer->BlastAbilityLVL < 3 )
				pPlayer->BlastAbilityLVL++;

			switch( pPlayer->BlastAbilityLVL )
			{
			case 2: UTIL_ShowMessage( "SUIT_BLAST2", pPlayer ); break;
			case 3: UTIL_ShowMessage( "SUIT_BLAST3", pPlayer ); break;
			}
		}
		break;
	case 8:
		pPlayer->CanElectroBlast = !pPlayer->CanElectroBlast;
		MESSAGE_BEGIN( MSG_ONE, gmsgBlastIcons, NULL, pPlayer->pev );
		WRITE_BYTE( pPlayer->BlastAbilityLVL );
		WRITE_BYTE( pPlayer->BlastChargesReady );
		WRITE_BYTE( pPlayer->CanElectroBlast );
		MESSAGE_END();
		pPlayer->m_iClientBlastAbilityLVL = pPlayer->BlastAbilityLVL;
		break; // toggle
	}

	switch( HUDSuitOffline )
	{
	case 1: pPlayer->HUDSuitOffline = !pPlayer->HUDSuitOffline; break;// toggle
	case 2: pPlayer->HUDSuitOffline = true; break; // on
	case 3: pPlayer->HUDSuitOffline = false; break; // off
	}

	switch( CanSelectEmptyWeapon )
	{
	case 1: pPlayer->CanSelectEmptyWeapon = !pPlayer->CanSelectEmptyWeapon; break;// toggle
	case 2: pPlayer->CanSelectEmptyWeapon = true; break; // on
	case 3: pPlayer->CanSelectEmptyWeapon = false; break; // off
	}

	switch( BreathingEffect )
	{
	case 1: pPlayer->BreathingEffect = !pPlayer->BreathingEffect; break;// toggle
	case 2: pPlayer->BreathingEffect = true; break; // on
	case 3: pPlayer->BreathingEffect = false; break; // off
	}

	switch( WigglingEffect )
	{
	case 1: pPlayer->WigglingEffect = !pPlayer->WigglingEffect; break;// toggle
	case 2: pPlayer->WigglingEffect = true; break; // on
	case 3: pPlayer->WigglingEffect = false; break; // off
	}

	switch( ShieldAvailableLVL )
	{
	case 1: pPlayer->ShieldAvailableLVL = 1; break; // lvl 1
	case 2: pPlayer->ShieldAvailableLVL = 2; break; // lvl 2
	case 3: pPlayer->ShieldAvailableLVL = 3; break; // lvl 3
	case 4: if( pPlayer->ShieldAvailableLVL < 3 ) pPlayer->ShieldAvailableLVL++; break; // add 1 level
	case 5: if( pPlayer->ShieldAvailableLVL > 0 ) pPlayer->ShieldAvailableLVL--; break; // remove 1 level
	case 6: pPlayer->ShieldAvailableLVL = 0; break; // remove shield ability
	case 7: pPlayer->ShieldOn = true; break; // turn shield on (for multiplayer)
	case 8: pPlayer->ShieldOn = false; break; // turn off
	}

	switch( UpsideDown )
	{
	case 1: // toggle
	{
		if( pPlayer->pev->effects & EF_UPSIDEDOWN )
			pPlayer->pev->effects &= ~EF_UPSIDEDOWN;
		else
			pPlayer->pev->effects |= EF_UPSIDEDOWN;
	}
	break;
	case 2: pPlayer->pev->effects |= EF_UPSIDEDOWN; break; // on
	case 3: pPlayer->pev->effects &= ~EF_UPSIDEDOWN; break; // off
	}

	switch( LoudWeaponsRestricted )
	{
	case 1: pPlayer->LoudWeaponsRestricted = !pPlayer->LoudWeaponsRestricted; break; // toggle
	case 2: pPlayer->LoudWeaponsRestricted = true; break; // on
	case 3: pPlayer->LoudWeaponsRestricted = false; break; // off
	}

	switch( PlayingDrums )
	{
	case 1: pPlayer->PlayingDrums = !pPlayer->PlayingDrums; break;// toggle
	case 2: pPlayer->PlayingDrums = true; break; // on
	case 3: pPlayer->PlayingDrums = false; break; // off
	}

	switch( DrunkLevel )
	{
	case 1: if( pPlayer->DrunkLevel < 5 ) pPlayer->DrunkLevel++; break; // increase
	case 2: if( pPlayer->DrunkLevel > 0 ) pPlayer->DrunkLevel--; break; // decrease
	case 3: pPlayer->DrunkLevel = 5; break; // full drunk
	case 4: pPlayer->DrunkLevel = 0; break; // sober
	}

	switch( CanUseDrone )
	{
	case 1: pPlayer->CanUseDrone = !pPlayer->CanUseDrone; break; // toggle
	case 2: pPlayer->CanUseDrone = true; break; // on
	case 3: pPlayer->CanUseDrone = false; break; // off
	}

	// apply drone new color
	if( !pev->rendercolor.IsNull() )
		pPlayer->DroneColor = pev->rendercolor;

	if( !FStringNull( DroneTarget_OnDeploy) )
	{
		if( FStrEq( STRING( DroneTarget_OnDeploy ), "_666" ) ) // this clears target
			pPlayer->DroneTarget_OnDeploy = NULL;
		else
			pPlayer->DroneTarget_OnDeploy = DroneTarget_OnDeploy;
	}

	if( !FStringNull( DroneTarget_OnReturn ) )
	{
		if( FStrEq( STRING( DroneTarget_OnReturn ), "_666" ) ) // this clears target
			pPlayer->DroneTarget_OnReturn = NULL;
		else
			pPlayer->DroneTarget_OnReturn = DroneTarget_OnReturn;
	}

	if( !FStringNull( DroneTarget_OnEnteringFirstPerson ) )
	{
		if( FStrEq( STRING( DroneTarget_OnEnteringFirstPerson ), "_666" ) ) // this clears target
			pPlayer->DroneTarget_OnEnteringFirstPerson = NULL;
		else
			pPlayer->DroneTarget_OnEnteringFirstPerson = DroneTarget_OnEnteringFirstPerson;
	}

	if( !FStringNull( DroneTarget_OnLeavingFirstPerson ) )
	{
		if( FStrEq( STRING( DroneTarget_OnLeavingFirstPerson ), "_666" ) ) // this clears target
			pPlayer->DroneTarget_OnLeavingFirstPerson = NULL;
		else
			pPlayer->DroneTarget_OnLeavingFirstPerson = DroneTarget_OnLeavingFirstPerson;
	}

	if( DeletePlayerTurrets )
	{
		CBaseEntity *pTurret = NULL;
		while( (pTurret = UTIL_FindEntityByClassname( pTurret, "_playersentry" )) != NULL )
		{
			// not sure but just in case - a turret already marked for deletion
			if( pTurret->pev->flags & FL_KILLME )
				continue;
			
			// only deleting the turrets of specific player
			CBaseEntity *pTurretOwner = CBaseEntity::Instance( pTurret->pev->owner );
			if( pTurretOwner && pTurretOwner == pPlayer )
				UTIL_Remove( pTurret );
		}
	}
}