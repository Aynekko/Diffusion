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
//  ammohistory.cpp
//


#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "ammohistory.h"
#include "r_local.h"

#define AMMO_PICKUP_GAP		(gHR.iHistoryGap + 5)
#define AMMO_PICKUP_PICK_HEIGHT	(32 + (gHR.iHistoryGap * 2))
#define AMMO_PICKUP_HEIGHT_MAX	(ScreenHeight - 100)
#define MAX_ITEM_NAME		32

HistoryResource gHR;
int HISTORY_DRAW_TIME = 5;

// keep a list of items
struct ITEM_INFO
{
	char	szName[MAX_ITEM_NAME];
	SpriteHandle	spr;
	wrect_t	rect;
};

// titles.txt
const char AmmoNames[TOTAL_WEAPONS][32] =
{
	"'\0'", // WEAPON_NONE
	"'\0'", // WEAPON_KNIFE
	"AMMO_BERETTA", // WEAPON_BERETTA
	"AMMO_DEAGLE", // WEAPON_DEAGLE
	"AMMO_MRC", // WEAPON_MRC
	"'\0'", // WEAPON_CYCLER
	"AMMO_CROSSBOW", // WEAPON_CROSSBOW
	"AMMO_SHOTGUN", // WEAPON_SHOTGUN
	"AMMO_RPG", // WEAPON_RPG
	"AMMO_GAUSNIPER", // WEAPON_GAUSS
	"'\0'", // WEAPON_EGON
	"'\0'", // WEAPON_HORNETGUN
	"AMMO_HEGRENADE", // WEAPON_HANDGRENADE
	"AMMO_TRIPMINE", // WEAPON_TRIPMINE
	"AMMO_SATCHEL", // WEAPON_SATCHEL
	"'\0'", // WEAPON_SNARK
	"AMMO_AR2", // WEAPON_AR2
	"'\0'", // WEAPON_DRONE
	"'\0'", // WEAPON_SENTRY
	"AMMO_HKMP5", // WEAPON_HKMP5
	"AMMO_57", // WEAPON_FIVESEVEN
	"AMMO_SNIPER", // WEAPON_SNIPER
	"AMMO_SHOTGUN", // WEAPON_SHOTGUN_XM
	"AMMO_G36C", // WEAPON_G36C
	"AMMO_SMOKEGRENADE", // WEAPON_SMOKEGRENADE
};

const char Ammo2Names[TOTAL_WEAPONS][32] =
{
	"'\0'", // WEAPON_NONE
	"'\0'", // WEAPON_KNIFE
	"'\0'", // WEAPON_BERETTA
	"'\0'", // WEAPON_DEAGLE
	"AMMO2_MRC", // WEAPON_MRC
	"'\0'", // WEAPON_CYCLER
	"'\0'", // WEAPON_CROSSBOW
	"'\0'", // WEAPON_SHOTGUN
	"'\0'", // WEAPON_RPG
	"'\0'", // WEAPON_GAUSS
	"'\0'", // WEAPON_EGON
	"'\0'", // WEAPON_HORNETGUN
	"'\0'", // WEAPON_HANDGRENADE
	"'\0'", // WEAPON_TRIPMINE
	"'\0'", // WEAPON_SATCHEL
	"'\0'", // WEAPON_SNARK
	"AMMO2_AR2", // WEAPON_AR2
	"'\0'", // WEAPON_DRONE
	"'\0'", // WEAPON_SENTRY
	"'\0'", // WEAPON_HKMP5
	"'\0'", // WEAPON_FIVESEVEN
	"'\0'", // WEAPON_SNIPER
	"'\0'", // WEAPON_SHOTGUN_XM
	"'\0'", // WEAPON_G36C
	"'\0'", // WEAPON_SMOKEGRENADE
};

void HistoryResource :: AddToHistory( int iType, int iId, int iCount )
{
	if( iType == HISTSLOT_AMMO && !iCount )
		return;  // no amount, so don't add

	if(((( AMMO_PICKUP_GAP * iCurrentHistorySlot ) + AMMO_PICKUP_PICK_HEIGHT ) > AMMO_PICKUP_HEIGHT_MAX ) || ( iCurrentHistorySlot >= MAX_HISTORY ))
	{	
		// the pic would have to be drawn too high
		// so start from the bottom
		iCurrentHistorySlot = 0;
	}
	
	HIST_ITEM *freeslot = &rgAmmoHistory[iCurrentHistorySlot++];  // default to just writing to the first slot
	HISTORY_DRAW_TIME = CVAR_GET_FLOAT( "hud_drawhistory_time" );

	freeslot->type = iType;
	freeslot->iId = iId;
	freeslot->iCount = iCount;
	freeslot->DisplayTime = gHUD.m_flTime + HISTORY_DRAW_TIME;
	freeslot->x_lerp = 1.0f;
	freeslot->ypos_lerp = ScreenHeight - 200;
}

void HistoryResource :: AddToHistory( int iType, const char *szName, int iCount )
{
	if( iType != HISTSLOT_ITEM )
		return;

	if(((( AMMO_PICKUP_GAP * iCurrentHistorySlot ) + AMMO_PICKUP_PICK_HEIGHT ) > AMMO_PICKUP_HEIGHT_MAX ) || ( iCurrentHistorySlot >= MAX_HISTORY ))
	{	
		// the pic would have to be drawn too high
		// so start from the bottom
		iCurrentHistorySlot = 0;
	}

	HIST_ITEM *freeslot = &rgAmmoHistory[iCurrentHistorySlot++];  // default to just writing to the first slot

	// I am really unhappy with all the code in this file
	int i = gHUD.GetSpriteIndex( szName );
	if( i == -1 ) return;  // unknown sprite name, don't add it to history

	freeslot->iId = i;
	freeslot->type = iType;
	freeslot->iCount = iCount;

	HISTORY_DRAW_TIME = CVAR_GET_FLOAT( "hud_drawhistory_time" );
	freeslot->DisplayTime = gHUD.m_flTime + HISTORY_DRAW_TIME;
	freeslot->x_lerp = 1.0f;
	freeslot->ypos_lerp = ScreenHeight - 200;
}


void HistoryResource :: CheckClearHistory( void )
{
	for( int i = 0; i < MAX_HISTORY; i++ )
	{
		if( rgAmmoHistory[i].type )
			return;
	}

	iCurrentHistorySlot = 0;
}

//
// Draw Ammo pickup history
//
int HistoryResource :: DrawAmmoHistory( float flTime )
{
	gHUD.GL_HUD_StartConstantSize( true );

	float ypos = ScreenHeight - 200;

	for( int i = 0; i < MAX_HISTORY; i++ )
	{
		if( rgAmmoHistory[i].type )
		{
			rgAmmoHistory[i].DisplayTime = Q_min( rgAmmoHistory[i].DisplayTime, gHUD.m_flTime + HISTORY_DRAW_TIME );

			if ( rgAmmoHistory[i].DisplayTime <= flTime )
			{ 
				// pic drawing time has expired
				memset( &rgAmmoHistory[i], 0, sizeof( HIST_ITEM ));
				CheckClearHistory();
			}
			else if( rgAmmoHistory[i].type == HISTSLOT_AMMO )
			{
				int ammotype = 0;
				int wpn_id = gWR.GetWeaponIdForAmmo( rgAmmoHistory[i].iId, &ammotype );

				if( ammotype == 0 )
					continue;

				const char *ammoname = (ammotype == 1 ? AmmoNames[wpn_id] : Ammo2Names[wpn_id] );

				// get position, lerp to it
				ypos -= 30 + gHUD.m_scrinfo.iCharHeight;
				rgAmmoHistory[i].ypos_lerp = lerp( rgAmmoHistory[i].ypos_lerp, ypos, 5 * g_fFrametime );
				float ypos_new = rgAmmoHistory[i].ypos_lerp;

				float xpos = ScreenWidth - 60;
				client_textmessage_t *pMsg = TextMessageGet( ammoname );
				int text_width = 0;
				const char *pText;			

				// get text length
				pText = pMsg ? pMsg->pMessage : ammoname;
				while( *pText && *pText != '\n' )
				{
					unsigned char c = *pText;
					text_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 );
					pText++;
				}

				if( text_width > 0 )
				{
					// lerp to position
					rgAmmoHistory[i].x_lerp = lerp( rgAmmoHistory[i].x_lerp, 0.0f, 5 * g_fFrametime * ((i * 0.5f) + 1) );
					xpos += 300 * rgAmmoHistory[i].x_lerp;

					// RGB 70 169 255
					// draw frame and amount
					float scale = ((rgAmmoHistory[i].DisplayTime - flTime) * 80);
					FillRoundedRGBA( xpos - text_width - (60 * gHUD.fScale), ypos_new - 10, text_width + (80 * gHUD.fScale), gHUD.m_scrinfo.iCharHeight + 20, 10, 0.25f, 0.25f, 0.25f, 0.002f * scale ); // grey frame background
					FillRoundedRGBA( xpos - text_width - (10 * gHUD.fScale), ypos_new - 5, text_width + (25 * gHUD.fScale), gHUD.m_scrinfo.iCharHeight + 10, 10, 0.275f, 0.663f, 1.0f, 0.002f * scale ); // blue frame for text
					int r, g, b;
					UnpackRGB( r, g, b, gHUD.m_iHUDColor );
					ScaleColors( r, g, b, Q_min( scale, 255 ) );
					gHUD.DrawHudNumberString( xpos - text_width - (30 * gHUD.fScale), ypos_new, xpos - text_width - (100 * gHUD.fScale), rgAmmoHistory[i].iCount, r, g, b );
					// draw text
					pText = pMsg ? pMsg->pMessage : ammoname;
					int textpos_x = xpos - text_width;
					
					r = 255; g = 255; b = 255;
					ScaleColors( r, g, b, Q_min( scale, 255 ) );
					DrawString( textpos_x, ypos_new, pText, r, g, b );
				}
			}
			else if( rgAmmoHistory[i].type == HISTSLOT_WEAP )
			{
				WEAPON *weap = gWR.GetWeapon( rgAmmoHistory[i].iId );

				if( !weap )
					continue;  // we don't know about the weapon yet, so don't draw anything
				
				int r, g, b;
				UnpackRGB( r,g,b, gHUD.m_iHUDColor );

				if( !gWR.HasAmmo( weap ))
					UnpackRGB( r, g, b, RGB_REDISH ); // if the weapon doesn't have ammo, display it as red

				float scale = (rgAmmoHistory[i].DisplayTime - flTime) * 80;
				ScaleColors( r, g, b, Q_min( scale, 255 ));

				// get position, lerp to it
				ypos -= 35 + AMMO_PICKUP_PICK_HEIGHT;
				rgAmmoHistory[i].ypos_lerp = lerp( rgAmmoHistory[i].ypos_lerp, ypos, 5 * g_fFrametime );
				float ypos_new = rgAmmoHistory[i].ypos_lerp;
				float xpos = ScreenWidth - ( weap->rcInactive.right - weap->rcInactive.left );

				// lerp to position
				rgAmmoHistory[i].x_lerp = lerp( rgAmmoHistory[i].x_lerp, 0.0f, 5 * g_fFrametime * ((i * 0.5f) + 1) );
				xpos += 300 * rgAmmoHistory[i].x_lerp;

				SPR_Set( weap->hInactive, r, g, b );
				SPR_DrawAdditive( 0, xpos, ypos_new, &weap->rcInactive );
			}
			else if( rgAmmoHistory[i].type == HISTSLOT_ITEM )
			{
				int r, g, b;

				if( !rgAmmoHistory[i].iId )
					continue;  // sprite not loaded

				wrect_t rect = gHUD.GetSpriteRect( rgAmmoHistory[i].iId );

				UnpackRGB( r, g, b, gHUD.m_iHUDColor );
				float scale = ( rgAmmoHistory[i].DisplayTime - flTime ) * 80;
				ScaleColors( r, g, b, Q_min( scale, 255 ));

				// get position, lerp to it
				ypos -= 30 + AMMO_PICKUP_PICK_HEIGHT;
				rgAmmoHistory[i].ypos_lerp = lerp( rgAmmoHistory[i].ypos_lerp, ypos, 5 * g_fFrametime );
				float ypos_new = rgAmmoHistory[i].ypos_lerp;
				int xpos = ScreenWidth - (rect.right - rect.left) - 10;

				SPR_Set( gHUD.GetSprite( rgAmmoHistory[i].iId ), r, g, b );
				SPR_DrawAdditive( 0, xpos, ypos_new, &rect );
			}
		}
	}

	gHUD.GL_HUD_EndConstantSize();

	return 1;
}
