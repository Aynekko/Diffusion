/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "ammohistory.h"
#include "pm_defs.h"
#include "event_api.h"
#include "r_local.h"
#include "WEAPONINFO.H"
#include "triangleapi.h"

int		g_weaponselect = 0;
WEAPON		*gpActiveSel;	// NULL means off, 1 means just the menu bar, otherwise
WEAPON		*gpLastSel;	// Last weapon menu selection 
static wrect_t	nullRc;
WeaponsResource	gWR;

extern float localanim_NextPAttackTime;
extern float localanim_NextSAttackTime;
extern int localanim_WeaponID;
extern bool localanim_AllowRpgShoot;

int WeaponsResource :: HasAmmo( WEAPON *p )
{
	if( !p )
		return FALSE;

	// weapons with no max ammo can always be selected
	if( p->iMax1 == -1 )
		return TRUE;

	return (p->iAmmoType == -1) || p->iClip > 0 || CountAmmo( p->iAmmoType ) 
		|| CountAmmo( p->iAmmo2Type ) || ( p->iFlags & WEAPON_FLAGS_SELECTONEMPTY );
}

void WeaponsResource :: LoadWeaponSprites( WEAPON *pWeapon )
{
	int i, iRes;
	
	if( ScreenWidth < 640 )
		iRes = 320;
	else iRes = 640;

	char sz[128];

	if ( !pWeapon ) return;

	memset( &pWeapon->rcActive, 0, sizeof( wrect_t ));
	memset( &pWeapon->rcInactive, 0, sizeof( wrect_t ));
	memset( &pWeapon->rcAmmo, 0, sizeof( wrect_t ));
	memset( &pWeapon->rcAmmo2, 0, sizeof( wrect_t ));
	pWeapon->hInactive = 0;
	pWeapon->hActive = 0;
	pWeapon->hAmmo = 0;
	pWeapon->hAmmo2 = 0;
	
	Q_snprintf( sz, sizeof( sz ), "sprites/%s.txt", pWeapon->szName );
	client_sprite_t *pList = SPR_GetList( sz, &i );

	if( !pList ) return;

	client_sprite_t *p;
	
	p = GetSpriteList( pList, "crosshair", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hCrosshair = SPR_Load( sz );
		pWeapon->rcCrosshair = p->rc;
	}
	else pWeapon->hCrosshair = 0;

	p = GetSpriteList( pList, "autoaim", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hAutoaim = SPR_Load( sz );
		pWeapon->rcAutoaim = p->rc;
	}
	else pWeapon->hAutoaim = 0;

	p = GetSpriteList( pList, "zoom", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hZoomedCrosshair = SPR_Load( sz );
		pWeapon->rcZoomedCrosshair = p->rc;
	}
	else
	{
		pWeapon->hZoomedCrosshair = pWeapon->hCrosshair; // default to non-zoomed crosshair
		pWeapon->rcZoomedCrosshair = pWeapon->rcCrosshair;
	}

	p = GetSpriteList( pList, "zoom_autoaim", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hZoomedAutoaim = SPR_Load( sz );
		pWeapon->rcZoomedAutoaim = p->rc;
	}
	else
	{
		pWeapon->hZoomedAutoaim = pWeapon->hZoomedCrosshair;  // default to zoomed crosshair
		pWeapon->rcZoomedAutoaim = pWeapon->rcZoomedCrosshair;
	}

	p = GetSpriteList( pList, "weapon", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hInactive = SPR_Load( sz );
		pWeapon->rcInactive = p->rc;
		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
	{
		pWeapon->hInactive = gHUD.m_hHudError;
		pWeapon->rcInactive = gHUD.GetSpriteRect( gHUD.m_HUD_error );
		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}

	p = GetSpriteList( pList, "weapon_s", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hActive = SPR_Load( sz );
		pWeapon->rcActive = p->rc;
	}
	else
	{
		pWeapon->hActive = gHUD.m_hHudError;
		pWeapon->rcActive = gHUD.GetSpriteRect( gHUD.m_HUD_error );
	}

	p = GetSpriteList( pList, "ammo", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hAmmo = SPR_Load( sz );
		pWeapon->rcAmmo = p->rc;
		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else pWeapon->hAmmo = 0;

	p = GetSpriteList( pList, "ammo2", iRes, i );
	if( p )
	{
		Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
		pWeapon->hAmmo2 = SPR_Load( sz );
		pWeapon->rcAmmo2 = p->rc;
		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else pWeapon->hAmmo2 = 0;
}

// Returns the first weapon for a given slot.
WEAPON *WeaponsResource :: GetFirstPos( int iSlot )
{
	WEAPON *pret = NULL;

	for( int i = 0; i < MAX_WEAPON_POSITIONS; i++ )
	{
		if( rgSlots[iSlot][i] && (HasAmmo( rgSlots[iSlot][i] ) || gHUD.CanSelectEmptyWeapon) )
		{
			pret = rgSlots[iSlot][i];
			break;
		}
	}
	return pret;
}

WEAPON* WeaponsResource :: GetNextActivePos( int iSlot, int iSlotPos )
{
	if( iSlotPos >= MAX_WEAPON_POSITIONS || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;

	WEAPON *p = gWR.rgSlots[iSlot][iSlotPos+1];
	
	if( !p || (!gWR.HasAmmo(p) && !gHUD.CanSelectEmptyWeapon) )
		return GetNextActivePos( iSlot, iSlotPos + 1 );

	return p;
}

int	giBucketHeight;		// Ammo Bar width and height
int	giBucketWidth;
int	giABHeight;
int	giABWidth;

SpriteHandle	ghsprBuckets;		// Sprite for top row of weapons menu

DECLARE_MESSAGE( m_Ammo, CurWeapon  );	// Current weapon and clip
DECLARE_MESSAGE( m_Ammo, WeaponList );	// new weapon type
DECLARE_MESSAGE( m_Ammo, AmmoX );	// update known ammo type's count
DECLARE_MESSAGE( m_Ammo, AmmoPickup );	// flashes an ammo pickup record
DECLARE_MESSAGE( m_Ammo, WeapPickup );	// flashes a weapon pickup record
DECLARE_MESSAGE( m_Ammo, HideWeapon );	// hides the weapon, ammo, and crosshair displays temporarily
DECLARE_MESSAGE( m_Ammo, ItemPickup );

DECLARE_COMMAND( m_Ammo, Slot1 );
DECLARE_COMMAND( m_Ammo, Slot2 );
DECLARE_COMMAND( m_Ammo, Slot3 );
DECLARE_COMMAND( m_Ammo, Slot4 );
DECLARE_COMMAND( m_Ammo, Slot5 );
DECLARE_COMMAND( m_Ammo, Slot6 );
DECLARE_COMMAND( m_Ammo, Slot7 );
DECLARE_COMMAND( m_Ammo, Slot8 );
DECLARE_COMMAND( m_Ammo, Slot9 );
DECLARE_COMMAND( m_Ammo, Slot10 );
DECLARE_COMMAND( m_Ammo, Close );
DECLARE_COMMAND( m_Ammo, NextWeapon );
DECLARE_COMMAND( m_Ammo, PrevWeapon );

// width of ammo fonts
#define AMMO_SMALL_WIDTH	10
#define AMMO_LARGE_WIDTH	20
#define HISTORY_DRAW_TIME	"5"

int CHudAmmo::Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( CurWeapon );
	HOOK_MESSAGE( WeaponList );
	HOOK_MESSAGE( AmmoPickup );
	HOOK_MESSAGE( WeapPickup );
	HOOK_MESSAGE( ItemPickup );
	HOOK_MESSAGE( HideWeapon );
	HOOK_MESSAGE( AmmoX );

	HOOK_COMMAND( "slot1", Slot1 );
	HOOK_COMMAND( "slot2", Slot2 );
	HOOK_COMMAND( "slot3", Slot3 );
	HOOK_COMMAND( "slot4", Slot4 );
	HOOK_COMMAND( "slot5", Slot5 );
	HOOK_COMMAND( "slot6", Slot6 );
	HOOK_COMMAND( "slot7", Slot7 );
	HOOK_COMMAND( "slot8", Slot8 );
	HOOK_COMMAND( "slot9", Slot9 );
	HOOK_COMMAND( "slot10", Slot10 );
	HOOK_COMMAND( "cancelselect", Close );
	HOOK_COMMAND( "invnext", NextWeapon );
	HOOK_COMMAND( "invprev", PrevWeapon );

	Reset();

	CVAR_REGISTER( "hud_drawhistory_time", HISTORY_DRAW_TIME, 0 );

	// controls whether or not weapons can be selected in one keypress
	CVAR_REGISTER( "hud_fastswitch", "0", FCVAR_ARCHIVE );

	m_iFlags |= HUD_ACTIVE; //!!!

	gWR.Init();
	gHR.Init();

	return 1;
}

void CHudAmmo::Reset( void )
{
	m_fFade = 0;
	m_fFade2 = 0;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay = 0;

	gWR.Reset();
	gHR.Reset();

	AmmoSetCrosshair( 0, nullRc, 0, 0, 0 );	// reset crosshair
	m_pWeapon = NULL;			// reset last weapon
}

void CHudAmmo::AmmoSetCrosshair(SpriteHandle hspr, wrect_t rc, int r, int g, int b)
{
	if (CVAR_GET_FLOAT("crosshair") == 0)
		return;

	SetCrosshair(hspr, rc, r, g, b);
}

int CHudAmmo::VidInit( void )
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex( "bucket1" );
	m_HUD_selection = gHUD.GetSpriteIndex( "selection" );
	m_HUD_divider = gHUD.GetSpriteIndex( "divider" );

	ghsprBuckets = gHUD.GetSprite( m_HUD_bucket0 );
	giBucketWidth = gHUD.GetSpriteRect( m_HUD_bucket0 ).right - gHUD.GetSpriteRect( m_HUD_bucket0 ).left;
	giBucketHeight = gHUD.GetSpriteRect( m_HUD_bucket0 ).bottom - gHUD.GetSpriteRect( m_HUD_bucket0 ).top;

	gHR.iHistoryGap = max( gHR.iHistoryGap, gHUD.GetSpriteRect( m_HUD_bucket0 ).bottom - gHUD.GetSpriteRect( m_HUD_bucket0 ).top );

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	if( ScreenWidth >= 640 )
	{
		giABWidth = 20;
		giABHeight = 4;
	}
	else
	{
		giABWidth = 10;
		giABHeight = 2;
	}

	return 1;
}

//
// Think:
//  Used for selection of weapon menu item.
//
void CHudAmmo::Think( void )
{
	if( gHUD.m_fPlayerDead )
		return;

	if( memcmp( gHUD.m_iWeaponBits, gWR.iOldWeaponBits, MAX_WEAPON_BYTES ))
	{
		memcpy( gWR.iOldWeaponBits, gHUD.m_iWeaponBits, MAX_WEAPON_BYTES );

		for( int i = MAX_WEAPONS - 1; i > 0; i-- )
		{
			WEAPON *p = gWR.GetWeapon( i );

			if( p )
			{
				if( gHUD.HasWeapon( p->iId ))
					gWR.PickupWeapon( p );
				else
					gWR.DropWeapon( p );
			}
		}
	}

	if( !gpActiveSel )
		return;

	// has the player selected one?
	if( gHUD.m_iKeyBits & IN_ATTACK )
	{
		if( gpActiveSel != (WEAPON *)1 )
		{
			ServerCmd( gpActiveSel->szName );
			g_weaponselect = gpActiveSel->iId;
		}

		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		switch( RANDOM_LONG( 0, 2 ) ) // diffusion - more sounds
		{
		case 0: PlaySound( "common/wpn_select1.wav", 1 ); break;
		case 1: PlaySound( "common/wpn_select2.wav", 1 ); break;
		case 2: PlaySound( "common/wpn_select3.wav", 1 ); break;
		}
	}

}

//
// Helper function to return a Ammo pointer from id
//
SpriteHandle* WeaponsResource :: GetAmmoPicFromWeapon( int iAmmoId, wrect_t& rect )
{
	for( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if( rgWeapons[i].iAmmoType == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo;
			return &rgWeapons[i].hAmmo;
		}
		else if( rgWeapons[i].iAmmo2Type == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo2;
			return &rgWeapons[i].hAmmo2;
		}
	}
	return NULL;
}


// Menu Selection Code
void WeaponsResource :: SelectSlot( int iSlot, int fAdvance, int iDirection )
{
	if( gHUD.m_Menu.m_fMenuDisplayed && ( fAdvance == FALSE ) && ( iDirection == 1 ))	
	{
		// menu is overriding slot use commands
		gHUD.m_Menu.SelectMenuItem( iSlot + 1 );  // slots are one off the key numbers
		return;
	}

	if( iSlot >= MAX_WEAPON_SLOTS )
		return;

	if( gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & ( HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM))
		return;

	if( !gHUD.HasWeapon( WEAPON_SUIT ))
		return;

	if ( !memcmp( gHUD.m_iWeaponBits, nullbits, sizeof( gHUD.m_iWeaponBits )))
		return;

	WEAPON *p = NULL;
	bool fastSwitch = CVAR_GET_FLOAT( "hud_fastswitch" ) != 0;

	if(( gpActiveSel == NULL ) || ( gpActiveSel == (WEAPON *)1 ) || ( iSlot != gpActiveSel->iSlot ))
	{
		PlaySound( "common/wpn_hudon.wav", 1 );
		p = GetFirstPos( iSlot );

		if( p && fastSwitch ) // check for fast weapon switch mode
		{
			// if fast weapon switch is on, then weapons can be selected in a single keypress
			// but only if there is only one item in the bucket
			WEAPON *p2 = GetNextActivePos( p->iSlot, p->iSlotPos );

			if( !p2 )
			{	
				// only one active item in bucket, so change directly to weapon
				ServerCmd( p->szName );
				g_weaponselect = p->iId;
				return;
			}

		}
	}
	else
	{
		PlaySound( "common/wpn_moveselect.wav", 1 );
		if( gpActiveSel )
			p = GetNextActivePos( gpActiveSel->iSlot, gpActiveSel->iSlotPos );
		if( !p )
			p = GetFirstPos( iSlot );
	}

	
	if( !p )  // no selection found
	{
		// just display the weapon list, unless fastswitch is on just ignore it
		if( !fastSwitch )
			gpActiveSel = (WEAPON *)1;
		else
			gpActiveSel = NULL;
	}
	else 
		gpActiveSel = p;
}

//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
// 
int CHudAmmo::MsgFunc_AmmoX( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	gWR.SetAmmo( iIndex, abs( iCount ));

	m_fFade2 = 100.0f; //!!!

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	// Add ammo to the history
	gHR.AddToHistory( HISTSLOT_AMMO, iIndex, abs( iCount ));

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int iIndex = READ_BYTE();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, iIndex );

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_ITEM, szName );

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	
	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if (gEngfuncs.IsSpectateOnly())
		return 1;

	if(( m_pWeapon == NULL ) || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM)))
	{
		gpActiveSel = NULL;
		AmmoSetCrosshair( 0, nullRc, 0, 0, 0 );
	}
	else
	{
		AmmoSetCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}

	END_READ();

	return 1;
}

// 
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type 
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf )
{	
	int fOnTarget = FALSE;

	BEGIN_READ( pszName, pbuf, iSize );

	int iState = READ_BYTE();
	iId = READ_CHAR();
	int iClip = READ_CHAR();

	// detect if we're also on target
	if( iState > 1 )
		fOnTarget = TRUE;

	if( iId < 1 )
	{
		AmmoSetCrosshair( 0, nullRc, 0, 0, 0 );
		m_pWeapon = NULL;
		return 0;
	}

	if (g_iUser1 != OBS_IN_EYE)
	{
		// Is player dead???
		if ((iId == -1) && (iClip == -1))
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
			return 1;
		}
		gHUD.m_fPlayerDead = FALSE;
	}

	gHUD.m_fPlayerDead = FALSE;

	WEAPON *pWeapon = gWR.GetWeapon( iId );

	if( !pWeapon )
		return 0;

	if( iClip < -1 )
		pWeapon->iClip = abs( iClip );
	else
		pWeapon->iClip = iClip;

	if( iState == 0 )	// we're not the current weapon, so update no more
		return 1;

	m_pWeapon = pWeapon;
	WeaponID = m_pWeapon->iId; // local copy

	if( gHUD.m_flFOV >= 90.0f )
	{ 
		// normal crosshairs
		if( fOnTarget && m_pWeapon->hAutoaim )
			AmmoSetCrosshair( m_pWeapon->hAutoaim, m_pWeapon->rcAutoaim, 255, 255, 255 );
		else AmmoSetCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}
	else
	{	// zoomed crosshairs
		if( fOnTarget && m_pWeapon->hZoomedAutoaim )
			AmmoSetCrosshair( m_pWeapon->hZoomedAutoaim, m_pWeapon->rcZoomedAutoaim, 255, 255, 255 );
		else AmmoSetCrosshair( m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255 );

	}

	m_fFade = 100.0f; //!!!
	m_iFlags |= HUD_ACTIVE;

	END_READ();

	if( iClip > 0 )
		gHUD.emptyclipspawned[WeaponID] = false; // we were reloaded to some extent which means we can spawn an empty clip again for this weapon
	
	return 1;
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	
	WEAPON Weapon;

	Q_strcpy( Weapon.szName, READ_STRING() );
	Weapon.iAmmoType = (int)READ_CHAR();	
	
	Weapon.iMax1 = READ_BYTE();
	if( Weapon.iMax1 == 255 )
		Weapon.iMax1 = -1;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2 = READ_BYTE();
	if( Weapon.iMax2 == 255 )
		Weapon.iMax2 = -1;

	Weapon.iSlot = READ_CHAR();
	Weapon.iSlotPos = READ_CHAR();
	Weapon.iId = READ_CHAR();
	Weapon.iFlags = READ_BYTE();
	Weapon.iClip = 0;

	gWR.AddWeapon( &Weapon );

	END_READ();

	return 1;

}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// Slot button pressed
void CHudAmmo::SlotInput( int iSlot )
{
	if( gHUD.PlayingDrums )
	{
		gHUD.DrumsInput( iSlot );
		return;
	}
	
	if( gHUD.m_CodeInput.CodeInputScreenIsOn )
		gHUD.m_CodeInput.EnterCode( iSlot + 1 );
	else
		gWR.SelectSlot( iSlot, FALSE, 1 );
}

void CHudAmmo::UserCmd_Slot1( void )
{
	SlotInput( 0 );
}

void CHudAmmo::UserCmd_Slot2( void )
{
	SlotInput( 1 );
}

void CHudAmmo::UserCmd_Slot3( void )
{
	SlotInput( 2 );
}

void CHudAmmo::UserCmd_Slot4( void )
{
	SlotInput( 3 );
}

void CHudAmmo::UserCmd_Slot5( void )
{
	SlotInput( 4 );
}

void CHudAmmo::UserCmd_Slot6( void )
{
	SlotInput( 5 );
}

void CHudAmmo::UserCmd_Slot7( void )
{
	SlotInput( 6 );
}

void CHudAmmo::UserCmd_Slot8( void )
{
	SlotInput( 7 );
}

void CHudAmmo::UserCmd_Slot9( void )
{
	SlotInput( 8 );
}

void CHudAmmo::UserCmd_Slot10( void )
{
	SlotInput( 9 );
}

void CHudAmmo::UserCmd_Close( void )
{
	if( gHUD.m_CodeInput.CodeInputScreenIsOn )
	{
		gHUD.m_CodeInput.DisableCodeScreen();
		return;
	}
	
	if( gpActiveSel )
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		PlaySound( "common/wpn_hudoff.wav", 1 );
	}
	else
		ClientCmd( "escape" ); // go into menu
}


// Selects the next item in the weapon menu
void CHudAmmo::UserCmd_NextWeapon( void )
{
	if( gHUD.m_PseudoGUI.m_iFlags & HUD_ACTIVE )
	{
		gHUD.m_PseudoGUI.scrolled_lines++; // scroll down
		return;
	}
	
	if( gHUD.m_fPlayerDead || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM)))
		return;

	if( !gpActiveSel || gpActiveSel == (WEAPON *)1 )
		gpActiveSel = m_pWeapon;

	int pos = 0;
	int slot = 0;

	if( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos + 1;
		slot = gpActiveSel->iSlot;
	}

	for( int loop = 0; loop <= 1; loop++ )
	{
		for( ; slot < MAX_WEAPON_SLOTS; slot++ )
		{
			for( ; pos < MAX_WEAPON_POSITIONS; pos++ )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if( wsp && (gWR.HasAmmo( wsp ) || gHUD.CanSelectEmptyWeapon))
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = 0;
		}

		slot = 0;  // start looking from the first slot again
	}

	gpActiveSel = NULL;
}

// Selects the previous item in the menu
void CHudAmmo::UserCmd_PrevWeapon( void )
{
	if( gHUD.m_PseudoGUI.m_iFlags & HUD_ACTIVE )
	{
		gHUD.m_PseudoGUI.scrolled_lines--; // scroll up
		return;
	}
	
	if( gHUD.m_fPlayerDead || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM)))
		return;

	if( !gpActiveSel || gpActiveSel == (WEAPON *)1 )
		gpActiveSel = m_pWeapon;

	int pos = MAX_WEAPON_POSITIONS - 1;
	int slot = MAX_WEAPON_SLOTS - 1;

	if( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos - 1;
		slot = gpActiveSel->iSlot;
	}
	
	for( int loop = 0; loop <= 1; loop++ )
	{
		for( ; slot >= 0; slot-- )
		{
			for( ; pos >= 0; pos-- )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if( wsp && (gWR.HasAmmo( wsp ) || gHUD.CanSelectEmptyWeapon))
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = MAX_WEAPON_POSITIONS - 1;
		}
		
		slot = MAX_WEAPON_SLOTS - 1;
	}

	gpActiveSel = NULL;
}

void CHudAmmo::SetWeaponNameText( void )
{
	switch( WeaponID )
	{
	default:
	case WEAPON_NONE:
	case WEAPON_KNIFE:
	case WEAPON_CYCLER:
	case WEAPON_EGON:
	case WEAPON_HORNETGUN:
	case WEAPON_SNARK:
	case WEAPON_DRONE:
	case WEAPON_SENTRY:
		hud_wpn_name[0] = '\0';
		break;
	case WEAPON_BERETTA:
		sprintf_s( hud_wpn_name, "Beretta M9" );
		break;
	case WEAPON_DEAGLE:
		sprintf_s( hud_wpn_name, "Desert Eagle" );
		break;
	case WEAPON_MRC:
		sprintf_s( hud_wpn_name, "Crye MR-C" );
		break;
	case WEAPON_CROSSBOW:
		sprintf_s( hud_wpn_name, "Crossbow" );
		break;
	case WEAPON_SHOTGUN:
		sprintf_s( hud_wpn_name, "Kel-Tec KSG" );
		break;
	case WEAPON_RPG:
		sprintf_s( hud_wpn_name, "Laser-guided Rocket Launcher" );
		break;
	case WEAPON_GAUSS:
		sprintf_s( hud_wpn_name, "Alien Sniper Rifle" );
		break;
	case WEAPON_HANDGRENADE:
		sprintf_s( hud_wpn_name, "CT XM49 HE Grenade" );
		break;
	case WEAPON_TRIPMINE:
		sprintf_s( hud_wpn_name, "CT LM05 Laser Mine" );
		break;
	case WEAPON_SATCHEL:
		sprintf_s( hud_wpn_name, "Explosive Pack" );
		break;
	case WEAPON_AR2:
		sprintf_s( hud_wpn_name, "Alien Rifle" );
		break;
	case WEAPON_HKMP5:
		sprintf_s( hud_wpn_name, "H&K MP5" );
		break;
	case WEAPON_FIVESEVEN:
		sprintf_s( hud_wpn_name, "FN Five-seven" );
		break;
	case WEAPON_SNIPER:
		sprintf_s( hud_wpn_name, "Barrett M82" );
		break;
	case WEAPON_SHOTGUN_XM:
		sprintf_s( hud_wpn_name, "Benelli M1014" );
		break;
	case WEAPON_G36C:
		sprintf_s( hud_wpn_name, "H&K G36C" );
		break;
	case WEAPON_SMOKEGRENADE:
		sprintf_s( hud_wpn_name, "CT M69 Smoke Grenade" );
		break;
	}
}

int CHudAmmo::GetPrimaryClipSize( void )
{
	switch( WeaponID )
	{
	default:
	case WEAPON_NONE:
	case WEAPON_KNIFE:
	case WEAPON_CYCLER:
	case WEAPON_EGON:
	case WEAPON_HORNETGUN:
	case WEAPON_SNARK:
	case WEAPON_DRONE:
	case WEAPON_SENTRY:
		return 1;
	case WEAPON_BERETTA:
		return BERETTA_MAX_CLIP;
	case WEAPON_DEAGLE:
		return DEAGLE_MAX_CLIP;
	case WEAPON_MRC:
		return MRC_MAX_CLIP;
	case WEAPON_CROSSBOW:
		return CROSSBOW_MAX_CLIP;
	case WEAPON_SHOTGUN:
		return SHOTGUN_MAX_CLIP;
	case WEAPON_RPG:
		return RPG_MAX_CLIP;
	case WEAPON_GAUSS:
		return GAUSS_MAX_CLIP;
	case WEAPON_HANDGRENADE:
		return HANDGRENADE_MAX_CARRY;
	case WEAPON_TRIPMINE:
		return TRIPMINE_MAX_CARRY;
	case WEAPON_SATCHEL:
		return SATCHEL_MAX_CARRY;
	case WEAPON_AR2:
		return AR2_MAX_CARRY;
	case WEAPON_HKMP5:
		return MP5_MAX_CLIP;
	case WEAPON_FIVESEVEN:
		return FIVESEVEN_MAX_CLIP;
	case WEAPON_SNIPER:
		return SNIPER_MAX_CLIP;
	case WEAPON_SHOTGUN_XM:
		return SHOTGUNXM_MAX_CLIP;
	case WEAPON_G36C:
		return G36C_MAX_CLIP;
	case WEAPON_SMOKEGRENADE:
		return SMOKEGRENADE_MAX_CARRY;
	}
}

bool CHudAmmo::PaintLowAmmo( void )
{
	switch( WeaponID )
	{
	case WEAPON_SMOKEGRENADE:
	case WEAPON_SATCHEL:
	case WEAPON_TRIPMINE:
	case WEAPON_HANDGRENADE:
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------
int CHudAmmo::Draw( float flTime )
{
	int a, x, y, r, g, b;
	int AmmoWidth;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if (!gHUD.HasWeapon( WEAPON_SUIT ))
		return 1;

	if(( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM)))
		return 1;

	// Draw Weapon Menu
	DrawWList( flTime );

	// Draw ammo pickup history
	gHR.DrawAmmoHistory( flTime );

	if( WeaponID == WEAPON_DRONE || WeaponID == WEAPON_SENTRY )
		return 1;

	if( !m_pWeapon )
		return 0;

	WEAPON *pw = m_pWeapon; // shorthand

	// SPR_Draw Ammo
	if(( pw->iAmmoType < 0 ) && ( pw->iAmmo2Type < 0 ))
		return 0;

	int iFlags = DHN_DRAWZERO; // draw 0 values

	AmmoWidth = gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).right - gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).left;

	a = (int)max( MIN_ALPHA, m_fFade );

	if( m_fFade > 0 )
		m_fFade -= (gHUD.m_flTimeDelta * 200);
	else
		m_fFade = 0;

	UnpackRGB( r, g, b, gHUD.m_iHUDColor );

	ScaleColors( r, g, b, a );

	// Does this weapon have a clip?
	y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;

	// width is 280px
	// diffusion hud color is 70 169 255
	const int ammo_frame_h = 64;
	const int ammo_frame_w = 280;
	const int icon_size = 0;
	int pos_x = ScreenWidth - ammo_frame_w - icon_size - 10; // 10px border
	int pos_y = ScreenHeight - 75; // same as hud_health

	// Does weapon have any ammo at all?
	if( m_pWeapon->iAmmoType > 0 )
	{
		if( cl_oldammohud->value <= 0 )
		{
			// RGB 0 111 210 - same color, but scaled down in brightness
		//	FillRoundedRGBA( pos_x, pos_y, ammo_frame_w, ammo_frame_h, 10, Vector4D( 0.0f, 111.f / 255.f, 210.f / 255.f, 0.5f ) );

			// draw the ammo cells, figure out their positions
			const int total_cells_width = ammo_frame_w - 20; // 10px borders from left and right
			int total_cells = GetPrimaryClipSize(); // equals the maximum clip capacity
			bool draw_cells = (total_cells <= 50); // draw a single bar if we have way too many cells...
			int weapon_clip = (pw->iClip >= 0) ? pw->iClip : gWR.CountAmmo( pw->iAmmoType );
			if( WeaponID == WEAPON_RPG )
			{
				weapon_clip = gWR.CountAmmo( pw->iAmmoType ) + pw->iClip;
				total_cells = ROCKET_MAX_CARRY;
			}
			/*
			==========================================================
			x = cell width
			x / 4 = cell margin
			z = total cells (ammo)
			y = total width of the whole thing
			y = zx - (z-1)*(x/4)
			cell width: x = 1 / ((z + (z-1)*4) / y);
			==========================================================
			*/
			float cell_width = 1.0f / ((total_cells + ((total_cells - 1) / 4.0f)) / (float)total_cells_width);
			float cell_height = ammo_frame_h / 3.0f;
			float cell_margin = cell_width * 0.25f;
			float cell_start_x = pos_x + 10;
			float cell_start_y = pos_y + (ammo_frame_h / 2.f) - (cell_height / 2.f);
			const float cell_r = 70.f / 255.f;
			const float cell_g = 169.f / 255.f;
			const float cell_b = 1.0f;

			// draw the current clip
			if( draw_cells )
			{
				for( int cell = 0; cell < total_cells; cell++ )
				{
					if( cell >= weapon_clip ) // draw grey cells
						FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
					else if( PaintLowAmmo() && (float)weapon_clip / (float)total_cells <= 0.25f ) // low ammo!
						FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( 0.8f, 0.08f, 0.08f, 0.65f + (fabs( sin( tr.time * 3 ) ) * 0.25f) ) );
					else
						FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( cell_r, cell_g, cell_b, 0.65f + (m_fFade / 255.f) ) );
					cell_start_x += cell_width + cell_margin;
				}
			}
			else // we have too much ammo to draw separate cells, use single bar
			{
				// draw the full bar (dark)
				FillRoundedRGBA( cell_start_x, cell_start_y, total_cells_width, cell_height, 3, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
				// draw the bar of actual ammo on top of it
				if( PaintLowAmmo() && (float)weapon_clip / (float)total_cells <= 0.25f ) // low ammo!
					FillRoundedRGBA( cell_start_x, cell_start_y, ((float)total_cells_width / (float)pw->iMax1) * (float)gWR.CountAmmo( pw->iAmmoType ), cell_height, 3, Vector4D( 0.8f, 0.08f, 0.08f, 0.65f + (fabs( sin( tr.time * 3 ) ) * 0.25f) ) );
				else
					FillRoundedRGBA( cell_start_x, cell_start_y, ((float)total_cells_width / (float)pw->iMax1) * (float)gWR.CountAmmo( pw->iAmmoType ), cell_height, 3, Vector4D( cell_r, cell_g, cell_b, 0.65f + (m_fFade / 255.f) ) );
			}

			// draw the ammo left in backpack, visualized by line
			if( pw->iClip >= 0 && WeaponID != WEAPON_RPG ) // an exception for better look...
			{
				float line_pos_x = pos_x + 10;
				float line_pos_y = cell_start_y + cell_height + 5;
				// draw the full bar (dark)
				FillRoundedRGBA( line_pos_x, line_pos_y, total_cells_width - 28, 5, 2, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
				// draw the bar of actual ammo on top of it
				FillRoundedRGBA( line_pos_x, line_pos_y, ((((float)total_cells_width - 28) / (float)pw->iMax1) * (float)gWR.CountAmmo( pw->iAmmoType )), 5, 2, Vector4D( cell_r, cell_g, cell_b, 0.65f + (m_fFade / 255.f) ) );
				char ammo[8];
				sprintf_s( ammo, "%i", gWR.CountAmmo( pw->iAmmoType ) );
				DrawStringReverse( line_pos_x + total_cells_width, line_pos_y - 3, ammo, 70, 169, 255 );
			}
		}
		else // old code (left by Camblu request)
		{
			int iIconWidth = m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left;

			if( pw->iClip >= 0 )
			{
				// room for the number and the '|' and the current ammo
				x = ScreenWidth - ( 8 * AmmoWidth ) - iIconWidth;
				x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b );

				wrect_t rc;
				rc.top = 0;
				rc.left = 0;
				rc.right = AmmoWidth;
				rc.bottom = 100;

				int iBarWidth =  AmmoWidth / 10;

				x += AmmoWidth / 2;

				UnpackRGB( r, g, b, gHUD.m_iHUDColor );

				// draw the | bar
		//		FillRGBA( x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a );
				ScaleColors( r, g, b, a );
				SPR_Set( gHUD.GetSprite( m_HUD_divider ), r, g, b );
				SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( m_HUD_divider ));

				x += iBarWidth + AmmoWidth / 2;

				x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo( pw->iAmmoType ), r, g, b );
			}
			else
			{
				// SPR_Draw a bullets only line
				x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
				x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo( pw->iAmmoType ), r, g, b );
			}

			// Draw the ammo Icon
			int iOffset = ( m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top ) / 8;
			SPR_Set( m_pWeapon->hAmmo, r, g, b );
			SPR_DrawAdditive( 0, x, y - iOffset, &m_pWeapon->rcAmmo );
		}
	}

	if( m_fFade2 > 0 )
		m_fFade2 -= (gHUD.m_flTimeDelta * 150);
	else
		m_fFade2 = 0;

	// Does weapon have secondary ammo?
	if( pw->iAmmo2Type > 0 )
	{
		if( cl_oldammohud->value <= 0 )
		{
			// draw the ammo cells, figure out their positions
			const float total_cells_width2 = (ammo_frame_w - 20) / 3.0f; // 10px border from the right
			int total_cells2 = m_pWeapon->iMax2; // equals the maximum clip capacity
			float cell_width2 = 1.0f / ((total_cells2 + ((total_cells2 - 1) / 4.0f)) / (float)total_cells_width2);
			float cell_height2 = ammo_frame_h / 6.0f;
			float cell_margin2 = cell_width2 * 0.25f;
			float cell_start_x = pos_x + 10 + total_cells_width2 * 2;
			float cell_start_y = pos_y + (ammo_frame_h / 10.f);
			float cell2_r = 70.f / 255.f;
			float cell2_g = 169.f / 255.f;
			float cell2_b = 1.0f;
			int secondary_ammo_amount = gWR.CountAmmo( gHUD.m_Ammo.m_pWeapon->iAmmo2Type );

			// draw the current secondary clip
			for( int cell = 0; cell < total_cells2; cell++ )
			{
				if( cell >= secondary_ammo_amount ) // draw grey cells
					FillRoundedRGBA( cell_start_x, cell_start_y, cell_width2, cell_height2, 3, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
				else
					FillRoundedRGBA( cell_start_x, cell_start_y, cell_width2, cell_height2, 3, Vector4D( cell2_r, cell2_g, cell2_b, 0.65f + (m_fFade2 / 255.f) ) );
				cell_start_x += cell_width2 + cell_margin2;
			}
		}
		else // old code (left by Camblu request)
		{
			int iIconWidth = m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left;

			// Do we have secondary ammo?
			if(( pw->iAmmo2Type != 0 ) && ( gWR.CountAmmo(pw->iAmmo2Type ) > 0))
			{
				y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight / 4;
				x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
				x = gHUD.DrawHudNumber( x, y, iFlags|DHN_3DIGITS, gWR.CountAmmo( pw->iAmmo2Type ), r, g, b );

				// Draw the ammo Icon
				SPR_Set( m_pWeapon->hAmmo2, r, g, b );
				int iOffset = ( m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top ) / 8;
				SPR_DrawAdditive( 0, x, y - iOffset, &m_pWeapon->rcAmmo2 );
			}
		}
	}

	if( cl_oldammohud->value <= 0 )
	{
		// lastly, draw the weapon name
		SetWeaponNameText();
		if( hud_wpn_name[0] != '\0' )
		{
			int text_pos_x = pos_x + 10;
			int text_pos_y = pos_y + 2;
			//	DrawConsoleString( text_pos_x, text_pos_y, hud_wpn_name );
			DrawString( text_pos_x, text_pos_y, hud_wpn_name, 70, 169, 255 );
		}
	}

	return 1;
}

#include <mathlib.h>

//
// Draws the ammo bar on the hud
//
int DrawBar( int x, int y, int width, int height, float f )
{
	int r, g, b;

	f = bound( 0.0f, f, 1.0f );

	if( f )
	{
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if( w <= 0 ) w = 1;

		UnpackRGB( r, g, b, RGB_GREENISH );
		FillRGBA( x, y, w, height, r, g, b, 255 );
		x += w;
		width -= w;
	}

	UnpackRGB( r, g, b, gHUD.m_iHUDColor );

	FillRGBA( x, y, width, height, r, g, b, 128 );

	return (x + width);
}

void DrawAmmoBar( WEAPON *p, int x, int y, int width, int height )
{
	if( !p )
		return;
	
	if( p->iAmmoType != -1 )
	{
		if( !gWR.CountAmmo( p->iAmmoType ))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType) / (float)p->iMax1;
		
		x = DrawBar( x, y, width, height, f );

		// Do we have secondary ammo too?
		if( p->iAmmo2Type != -1 )
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type) / (float)p->iMax2;

			x += 5; //!!!
			DrawBar( x, y, width, height, f );
		}
	}
}

//
// Draw Weapon Menu
//
int CHudAmmo::DrawWList( float flTime )
{
	int r, g, b, a;
	int x, y, i;

	if( !gpActiveSel )
		return 0;

	int iActiveSlot;

	if( gpActiveSel == (WEAPON *)1 )
		iActiveSlot = -1;	// current slot has no weapons
	else 
		iActiveSlot = gpActiveSel->iSlot;

	x = 10; //!!!
	y = 10; //!!!

	// Ensure that there are available choices in the active slot
	if( iActiveSlot > 0 )
	{
		if( !gWR.GetFirstPos( iActiveSlot ))
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}
		
	// Draw top line
	for( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		int iWidth;

		UnpackRGB( r, g, b, gHUD.m_iHUDColor );
	
		if( iActiveSlot == i )
			a = 255;
		else
			a = 192;

		ScaleColors( r, g, b, 255 );
		SPR_Set( gHUD.GetSprite( m_HUD_bucket0 + i ), r, g, b );

		// make active slot wide enough to accomodate gun pictures
		if( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos(iActiveSlot);

			if( p )
				iWidth = p->rcActive.right - p->rcActive.left;
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( m_HUD_bucket0 + i ));
		
		x += iWidth + 5;
	}

	a = 128; //!!!
	x = 10;

	// Draw all of the buckets
	for( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		y = giBucketHeight + 10;

		// If this is the active slot, draw the bigger pictures,
		// otherwise just draw boxes
		if( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos( i );
			int iWidth = giBucketWidth;

			if( p )
				iWidth = p->rcActive.right - p->rcActive.left;

			for( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				p = gWR.GetWeaponSlot( i, iPos );

				if( !p || !p->iId )
					continue;

				UnpackRGB( r, g, b, gHUD.m_iHUDColor );
			
				// if active, then we must have ammo.
				if( gpActiveSel == p )
				{
					// diffusion - except when we can select it
					if( gHUD.CanSelectEmptyWeapon )
					{
						if( !gWR.HasAmmo( p ) )
						{
							UnpackRGB( r, g, b, RGB_REDISH );
							ScaleColors( r, g, b, 128 );
						}
					}
					
					SPR_Set( p->hActive, r, g, b );
					SPR_DrawAdditive( 0, x, y, &p->rcActive );

					SPR_Set( gHUD.GetSprite( m_HUD_selection ), r, g, b );
					SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( m_HUD_selection ));
				}
				else
				{
					// Draw Weapon as Red if no ammo
					if( gWR.HasAmmo( p ))
					{
						ScaleColors( r, g, b, 192 );
					}
					else
					{
						UnpackRGB( r, g, b, RGB_REDISH );
						ScaleColors( r, g, b, 128 );
					}

					SPR_Set( p->hInactive, r, g, b );
					SPR_DrawAdditive( 0, x, y, &p->rcInactive );
				}

				// Draw Ammo Bar
				DrawAmmoBar( p, x + giABWidth / 2, y, giABWidth, giABHeight );
				y += p->rcActive.bottom - p->rcActive.top + 5;
			}

			x += iWidth + 5;

		}
		else
		{
			// Draw Row of weapons.
			UnpackRGB( r, g, b, gHUD.m_iHUDColor );

			for( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				WEAPON *p = gWR.GetWeaponSlot( i, iPos );
				
				if( !p || !p->iId )
					continue;

				if( gWR.HasAmmo( p ))
				{
					UnpackRGB( r, g, b, gHUD.m_iHUDColor );
					a = 128;
				}
				else
				{
					UnpackRGB( r, g, b, RGB_REDISH );
					a = 96;
				}

				FillRGBA( x, y, giBucketWidth, giBucketHeight, r, g, b, a );
				y += giBucketHeight + 5;
			}
			x += giBucketWidth + 5;
		}
	}	

	return 1;

}