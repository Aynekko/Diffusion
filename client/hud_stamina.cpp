#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "r_local.h"
#include "string.h"
#include "event_api.h"
#include <stdio.h>

float OFFLINEFlashAlpha = 0;
float StaminaLowSoundTime = 0;

DECLARE_MESSAGE( m_Stamina, Stamina )

int CHudStamina :: Init(void)
{
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE( Stamina );
	gHUD.AddHudElem(this);
	return 1;
};

int CHudStamina:: MsgFunc_Stamina( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	m_iStaminaValue = READ_BYTE();
	END_READ();

	return 1;
}

int CHudStamina :: VidInit(void)
{
	int bar_empty = gHUD.GetSpriteIndex( "stamina_empty" );
	int bar_full = gHUD.GetSpriteIndex( "stamina_full" );
	int bar_offline = gHUD.GetSpriteIndex( "stamina_offline" );
	m_hBarEmpty = gHUD.GetSprite( bar_empty );
	m_hBarFull = gHUD.GetSprite( bar_full );
	m_hBarOffline = gHUD.GetSprite( bar_offline );
	m_prc_emp = &gHUD.GetSpriteRect( bar_empty );
	m_prc_full = &gHUD.GetSpriteRect( bar_full );
	m_prc_offline = &gHUD.GetSpriteRect( bar_offline );
	m_iBarWidth = m_prc_full->right - m_prc_full->left;

	return 1;
};

int CHudStamina :: Draw(float flTime)
{
	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH )
		return 0;

	if (!gHUD.HasWeapon( WEAPON_SUIT ))
		return 1;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 0;

	int stamina_val = m_iStaminaValue;
	int r, g, b, x, y = 255;
	
	UnpackRGB( r, g, b, 0x0046A9FF ); // 70,169,255 for Diffusion

	x = (ScreenWidth / 32) - 30;
	y = ScreenHeight - 40;
	SPR_Set(m_hBarEmpty, r, g, b );
	SPR_DrawAdditive( 0,  x, y, m_prc_emp);

	if( gHUD.IsDrawingOfflineHUD || CL_IsDead() || gHUD.HUDSuitOffline )
	{
		DrawOfflineBar( x, y );
		return 1;
	}
	
	if( stamina_val < 25 )
		UnpackRGB( r, g, b, 0x00FF4242 );
		
	int iOffset = m_iBarWidth * (100.0 - stamina_val) / 100;
	if( iOffset < m_iBarWidth )
	{
		wrect_t rc = *m_prc_full;
		rc.right -= iOffset;
		ScaleColors( r, g, b, 150 );
		SPR_Set( m_hBarFull, r, g, b );
		//		SPR_DrawAdditive( 0, x + iOffset, y, &rc);  // try to uncomment this for cool effect?
		SPR_DrawAdditive( 0, x, y, &rc );
	}

	// play sound
	if( StaminaLowSoundTime > gHUD.m_flTime )
		StaminaLowSoundTime = 0; // possible saverestore issues...

	if( (stamina_val < 1) && (gHUD.m_flTime > StaminaLowSoundTime + 2) )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( gEngfuncs.GetLocalPlayer()->index, NULL, CHAN_STATIC, "player/stamina_low.wav", 1, 0, 0, 100 );
		StaminaLowSoundTime = gHUD.m_flTime;
	}

	return 1;
}

void CHudStamina::DrawOfflineBar( int x, int y )
{	
	int r = 255;
	int g, b = 0;
		
	OFFLINEFlashAlpha = 128 + (fabs( sin( tr.time * 3 ) ) * 128 );
	OFFLINEFlashAlpha = bound( 128, OFFLINEFlashAlpha, 255 );

	ScaleColors( r, g, b, (int)OFFLINEFlashAlpha );

	SPR_Set( m_hBarOffline, r, g, b );
	SPR_DrawAdditive( 0, x, y, m_prc_offline );
}