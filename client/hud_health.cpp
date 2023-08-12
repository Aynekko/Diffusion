#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "string.h"
#include <stdio.h>

DECLARE_MESSAGE( m_HealthVisual, HealthVisual )

float OFFLINEFlashAlphaH = 0;

int CHudHealthVisual :: Init(void)
{
	m_iFlags |= HUD_ACTIVE;

	HOOK_MESSAGE( HealthVisual );

	gHUD.AddHudElem(this);
	return 1;
};

int CHudHealthVisual :: VidInit(void)
{
	int bar_emptyh = gHUD.GetSpriteIndex( "health_empty" );
	int bar_fullh = gHUD.GetSpriteIndex( "health_full" );
	int bar_offline = gHUD.GetSpriteIndex( "health_offline" );
	m_hBarEmpty = gHUD.GetSprite( bar_emptyh );
	m_hBarFull = gHUD.GetSprite( bar_fullh );
	m_hBarOffline = gHUD.GetSprite( bar_offline );
	m_prc_emp = &gHUD.GetSpriteRect( bar_emptyh );
	m_prc_full = &gHUD.GetSpriteRect( bar_fullh );
	m_prc_offline = &gHUD.GetSpriteRect( bar_offline );
	m_iBarWidth = m_prc_full->right - m_prc_full->left;

	return 1;
};

int CHudHealthVisual:: MsgFunc_HealthVisual( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	GotHit = READ_BYTE();
	m_iHealthVisual = READ_BYTE();
	END_READ();

	return 1;
}

int CHudHealthVisual :: Draw(float flTime)
{
	if( gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH )
		return 1;

	if( !gHUD.HasWeapon( WEAPON_SUIT ) )
		return 1;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 0;

//	int health_val = m_iHealthVisual;
	static float health_val = 0.0f;
	health_val = CL_UTIL_Approach( m_iHealthVisual, health_val, g_fFrametime * 500 );

	int r, g, b, x, y = 255;

	UnpackRGB( r, g, b, 0x0046A9FF ); // 70,169,255 for Diffusion

	x = (ScreenWidth / 32) - 30;
	y = ScreenHeight - 75;
	SPR_Set(m_hBarEmpty, r, g, b );
	SPR_DrawAdditive( 0, x, y, m_prc_emp);

	if( gHUD.IsDrawingOfflineHUD || CL_IsDead() || gHUD.HUDSuitOffline )
	{
		DrawOfflineBar( x, y );
		return 1;
	}

	int iOffset = m_iBarWidth * (100.0 - health_val) / 100;

	if( UpdateTime > gHUD.m_flTime )
	{
		UpdateTime = 0;
		FlashingMode = false;
		Transparency = 150;
		GotHit = 0;
	}

	if( GotHit == 1 )
	{
		if( health_val <= 100 )
		{
			FlashingMode = true;
			Transparency = 255;
		}
		else
			Transparency = 150;

		GotHit = 0;
	}

	if( FlashingMode == true )
	{
		if( gHUD.m_flTime > UpdateTime )
		{
			if( Transparency > 150 )
				Transparency -= 50 * g_fFrametime;

			UpdateTime = gHUD.m_flTime;
		}
		else if( Transparency <= 150 )
		{
			FlashingMode = false;
			Transparency = 150;
		}
	}
	else
		Transparency = 150;

	if( iOffset < m_iBarWidth )
	{
		wrect_t rc = *m_prc_full;
		rc.right -= iOffset;
		UnpackRGB( r, g, b, 0x00EB4034 ); // 235 64 52
		ScaleColors( r, g, b, Transparency );
		SPR_Set( m_hBarFull, r, g, b );
		//		SPR_DrawAdditive( 0, x + iOffset, y, &rc);  // try to uncomment this for cool effect?
		SPR_DrawAdditive( 0, x, y, &rc );
	}

	// for megahealth, just draw the same stuff on top
	if( health_val > 100 )
	{
		int megahealth_val = health_val - 100;
		wrect_t rc2 = *m_prc_full;
		int iOffset2 = m_iBarWidth * (100.0 - megahealth_val) / 100;
		rc2.right -= iOffset2;
		UnpackRGB( r, g, b, 0x00EB4034 ); // 235 64 52
		ScaleColors( r, g, b, 150 );
		SPR_Set( m_hBarFull, r, g, b );
		SPR_DrawAdditive( 0, x, y, &rc2 );
	}

	return 1;
}

void CHudHealthVisual::DrawOfflineBar( int x, int y )
{
	int r = 255;
	int g, b = 0;
		
	OFFLINEFlashAlphaH = 128 + (fabs( sin( tr.time * 3 ) ) * 128);
	OFFLINEFlashAlphaH = bound( 128, OFFLINEFlashAlphaH, 255 );

	ScaleColors( r, g, b, (int)OFFLINEFlashAlphaH );

	SPR_Set( m_hBarOffline, r, g, b );
	SPR_DrawAdditive( 0, x, y, m_prc_offline );
}
