#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "string.h"

int CHudDroneBars::Init( void )
{
	m_iFlags |= HUD_ACTIVE;

	gHUD.AddHudElem( this );
	return 1;
};

int CHudDroneBars::VidInit( void )
{
	int barframe = gHUD.GetSpriteIndex( "drone_bars" );
	int barhealth = gHUD.GetSpriteIndex( "drone_hp" );
	int barammo = gHUD.GetSpriteIndex( "drone_ammo" );
	m_hBarFrame = gHUD.GetSprite( barframe );
	m_hBarHealth = gHUD.GetSprite( barhealth );
	m_hBarAmmo = gHUD.GetSprite( barammo );
	m_prc_barframe = &gHUD.GetSpriteRect( barframe );
	m_prc_barhealth = &gHUD.GetSpriteRect( barhealth );
	m_prc_barammo = &gHUD.GetSpriteRect( barammo );

	int droneicon = gHUD.GetSpriteIndex( "drone_icon" );
	m_hDroneIcon = gHUD.GetSprite( droneicon );
	m_prc_droneicon = &gHUD.GetSpriteRect( droneicon );

	return 1;
};

int CHudDroneBars::Draw( float flTime )
{
	if( !CanUseDrone )
		return 1;
	
	if( gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH )
		return 1;

	if( !gHUD.HasWeapon( WEAPON_DRONE ) )
		return 1;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 0;

	int r, g, b, x, y = 255;
	int offset = 0;
	wrect_t temp_rect;

	// draw the bars' frame
	UnpackRGB( r, g, b, 0x0046A9FF ); // 70,169,255 for Diffusion

	int default_x = (ScreenWidth / 32) - 30;
	int default_y = ScreenHeight / 2;
	x = default_x;
	y = default_y;

	SPR_Set( m_hBarFrame, r, g, b );
	SPR_DrawAdditive( 0, x, y, m_prc_barframe );

	// draw the health bar
	int bar_height = 128; // ! hardcoded sprite height
	// x and y are still same
	r = 255;
	g = 0;
	b = 0;
	SPR_Set( m_hBarHealth, r, g, b );
	temp_rect = *m_prc_barhealth;
	offset = bar_height * (1.0f - (DroneHealth / 500.0f));
	if( offset >= 128 ) offset = 127;
	temp_rect.top += offset;
	SPR_DrawAdditive( 0, x, y + offset, &temp_rect );

	// draw the ammo bar
	// y is still same
	r = 255;
	g = 100;
	b = 0;
	x = default_x + 24;
	SPR_Set( m_hBarAmmo, r, g, b );
	temp_rect = *m_prc_barammo;
	offset = bar_height * (1.0f - (DroneAmmo / 500.0f));
	if( offset >= 128 ) offset = 127;
	temp_rect.top += offset;
	SPR_DrawAdditive( 0, x, y + offset, &temp_rect );

	// write distance
	x = default_x;
	y = default_y - 28;
	if( !DroneDeployed )
		DrawString( x, y, "OFF", 255, 25, 25 );
	else
	{
		static char buf[64];
		Q_snprintf( buf, sizeof( buf ), "%i m", DroneDistance );
		DrawString( x, y, buf, 25, 255, 25 );
	}

	// draw the drone icon above the bars and text
	x = default_x;
	y = default_y - 72;
	if( !DroneDeployed )
		SPR_Set( m_hDroneIcon, 100, 100, 100 );
	else
		SPR_Set( m_hDroneIcon, 25, 255, 25 );

	SPR_DrawAdditive( 0, x, y, m_prc_droneicon );

	return 1;
}