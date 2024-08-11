#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"

DECLARE_MESSAGE( m_Flash, Flashlight )

int CHudFlashlight::Init( void) 
{
	m_fOn = 0;
	m_flTurnOn = 0.01f;

	HOOK_MESSAGE( Flashlight );

	m_iFlags |= HUD_ACTIVE;

	gHUD.AddHudElem( this );

	return 1;
}

void CHudFlashlight::Reset( void )
{
	m_fOn = 0;
	m_flTurnOn = 0.01f;
}

int CHudFlashlight::VidInit( void )
{
	int HUD_flash_empty = gHUD.GetSpriteIndex( "newflash_empty" );
	int HUD_flash_full = gHUD.GetSpriteIndex( "newflash_full" );
	m_hSprite1 = gHUD.GetSprite( HUD_flash_empty );
	m_hSprite2 = gHUD.GetSprite( HUD_flash_full );
	m_prc1 = &gHUD.GetSpriteRect( HUD_flash_empty );
	m_prc2 = &gHUD.GetSpriteRect( HUD_flash_full );
	m_iHeight = m_prc2->bottom - m_prc2->top;

	return 1;
}

int CHudFlashlight:: MsgFunc_Flashlight( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	m_fOn = READ_BYTE();
	m_iBat = READ_BYTE();
	END_READ();

	m_flBat = ((float)m_iBat) / 100.0f;

	return 1;
}

int CHudFlashlight::Draw( float flTime )
{
	if( gHUD.m_iHideHUDDisplay & ( HIDEHUD_FLASHLIGHT|HIDEHUD_ALL ))
		return 1;

	if (!gHUD.HasWeapon( WEAPON_SUIT ))
		return 1;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 0;

	// diffusion - for smooth turning on, see SetupFlashlight function
	if( !m_fOn )
	{
	//	if( m_flTurnOn > 0.01f )
	//		m_flTurnOn -= 0.5 * g_fFrametime;

	//	if( m_flTurnOn < 0.01f ) // gotta keep it this way, otherwise if 0 the light will be bright as 1.0 in the first frame
			m_flTurnOn = 0.01f;
	}
	else if( m_flTurnOn < 1.0f )
		m_flTurnOn += 4 * g_fFrametime;
	else if( m_flTurnOn > 1.0f )
		m_flTurnOn = 1.0f;

	if( !m_fOn && (m_flBat == 1.0f) && (MainAlpha <= 0.0f) )
		return 0;

	int r, g, b, x, y, a;
	wrect_t rc;

	// draw main case
	x = (ScreenWidth / 32) - 35;
	y = ScreenHeight - 200;

	rc = *m_prc1;

	// when not used and fully charged, slowly fade out the icon
	if( m_flBat == 1.0f )
		MainAlpha -= 50 * g_fFrametime;
	else if( m_fOn )
		MainAlpha = MIN_ALPHA;

	MainAlpha = bound( 0, MainAlpha, 100 );

	if( m_fOn )
		a = 150;
	else
	{
		a = MainAlpha;
		rc.top += 20;
		y += 20;
	}

	r = g = b = 255;
	ScaleColors( r, g, b, a );
	
	SPR_Set( m_hSprite1, r, g, b );
	SPR_DrawAdditive( 0, x, y, &rc );

	// draw bar
	int iOffset = m_iHeight * (1.0f - m_flBat);
	rc = *m_prc2;
	x += 17;
	if( m_fOn )
		y += 45;
	else
		y += 25;

	rc.bottom -= iOffset;
	if( rc.bottom < 1 )
		rc.bottom = 1;
	r = 255;
	g = 255;
	b = 255;

	if( m_flBat < 0.25f )
		UnpackRGB( r, g, b, RGB_REDISH );

	ScaleColors( r, g, b, a );
	SPR_Set( m_hSprite2, r, g, b );
	SPR_DrawAdditive( 0, x, y + iOffset, &rc );

	return 1;
}