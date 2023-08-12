#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"

//===========================================================================================
// diffusion - draw icons for player's available electroblasts
//===========================================================================================

DECLARE_MESSAGE( m_BlastIcons, BlastIcons );

int CHudBlastIcons::Init( void )
{
	HOOK_MESSAGE( BlastIcons );
	gHUD.AddHudElem( this );
	return 1;
}

int CHudBlastIcons::VidInit(void)
{
	int spr = gHUD.GetSpriteIndex( "hud_electroblast" );
	m_hBlastIcon1 = gHUD.GetSprite( spr );
	rc1 = &gHUD.GetSpriteRect( spr );
	m_hBlastIcon3 = m_hBlastIcon2 = m_hBlastIcon1;
	rc3 = rc2 = rc1;
	AlphaFade1 = AlphaFade2 = AlphaFade3 = 0.0f;

	return 1;
}

int CHudBlastIcons::MsgFunc_BlastIcons( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	BlastAbilityLVL = READ_BYTE(); // this determines the number of icons (0-3)
	BlastChargesReady = READ_BYTE(); // ready charge is blue, not ready - red
	CanElectroBlast = (READ_BYTE() > 0); // suspended by script - can't use, draw grey icons
	END_READ();

	if( BlastAbilityLVL > 0)
		m_iFlags |= HUD_ACTIVE;
	else
		m_iFlags &= ~HUD_ACTIVE;

	return 1;
}

int CHudBlastIcons::Draw( float flTime )
{
	if( gHUD.m_iHideHUDDisplay & (HIDEHUD_HEALTH | HIDEHUD_ALL) )
		return 1;

	if( !gHUD.HasWeapon( WEAPON_SUIT ) )
		return 1;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 0;

	int r1, g1, b1, x1, y1 = 0;
	int r2, g2, b2, x2, y2 = 0;
	int r3, g3, b3, x3, y3 = 0;
	int IconWidth = 54; // 48 + offset

	y1 = y2 = y3 = ScreenHeight - 75;

	//====================
	// first icon
	//====================
	if( BlastAbilityLVL > 0 )
	{
		x1 = (ScreenWidth / 32) + 270;

		if( BlastChargesReady > 0 )
		{
			if( CanElectroBlast )
				UnpackRGB( r1, g1, b1, gHUD.m_iHUDColor );
			else
				UnpackRGB( r1, g1, b1, RGB_GREY ); // grey

			if( AlphaFade1 < 255 )
				AlphaFade1 += 150 * g_fFrametime;
		}
		else
		{
			if( CanElectroBlast )
				UnpackRGB( r1, g1, b1, RGB_RED );
			else
				UnpackRGB( r1, g1, b1, RGB_GREY );

			if( AlphaFade1 > 0 )
				AlphaFade1 -= 250 * g_fFrametime;
		}

		AlphaFade1 = bound( 0, AlphaFade1, 255 );
		ScaleColors( r1, g1, b1, (int)AlphaFade1 );

		SPR_Set( m_hBlastIcon1, r1, g1, b1 );
		SPR_DrawAdditive( 0, x1, y1, rc1 );
	}

	//====================
	// second icon
	//====================
	if( BlastAbilityLVL > 1 )
	{
		x2 = (ScreenWidth / 32) + 270 + IconWidth;

		if( BlastChargesReady > 1 )
		{
			if( CanElectroBlast )
				UnpackRGB( r2, g2, b2, gHUD.m_iHUDColor );
			else
				UnpackRGB( r2, g2, b2, RGB_GREY ); // grey

			if( AlphaFade2 < 255 )
				AlphaFade2 += 150 * g_fFrametime;
		}
		else
		{
			if( CanElectroBlast )
				UnpackRGB( r2, g2, b2, RGB_RED );
			else
				UnpackRGB( r2, g2, b2, RGB_GREY ); // grey

			if( AlphaFade2 > 0 )
				AlphaFade2 -= 250 * g_fFrametime;
		}

		AlphaFade2 = bound( 0, AlphaFade2, 255 );
		ScaleColors( r2, g2, b2, (int)AlphaFade2 );

		SPR_Set( m_hBlastIcon2, r2, g2, b2 );
		SPR_DrawAdditive( 0, x2, y2, rc2 );
	}

	//====================
	// third icon
	//====================
	if( BlastAbilityLVL > 2 )
	{
		x3 = (ScreenWidth / 32) + 270 + IconWidth + IconWidth;

		if( BlastChargesReady > 2 )
		{
			if( CanElectroBlast )
				UnpackRGB( r3, g3, b3, gHUD.m_iHUDColor );
			else
				UnpackRGB( r3, g3, b3, RGB_GREY ); // grey

			if( AlphaFade3 < 255 )
				AlphaFade3 += 150 * g_fFrametime;
		}
		else
		{
			if( CanElectroBlast )
				UnpackRGB( r3, g3, b3, RGB_RED );
			else
				UnpackRGB( r3, g3, b3, RGB_GREY ); // grey

			if( AlphaFade3 > 0 )
				AlphaFade3 -= 250 * g_fFrametime;
		}

		AlphaFade3 = bound( 0, AlphaFade3, 255 );
		ScaleColors( r3, g3, b3, (int)AlphaFade3 );

		SPR_Set( m_hBlastIcon3, r3, g3, b3 );
		SPR_DrawAdditive( 0, x3, y3, rc3 );
	}


	return 1;
}