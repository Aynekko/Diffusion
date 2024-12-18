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
int stamina_icon = 0;
static float icon_size = 30;
// size of an invisible drawing field...
static float full_frame_h = 64;
static float full_frame_w = 280;
static float total_cells_width = full_frame_w - 20; // 10px borders from left and right
static const int total_cells = 50;
static float cell_width = 1.0f / ((total_cells + ((total_cells - 1) / 4.0f)) / (float)total_cells_width);
static float cell_height = full_frame_h / 3.0f;
static float cell_margin = cell_width * 0.25f;

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
	stamina_icon = LOAD_TEXTURE( "sprites/diffusion/stamina.dds", NULL, 0, 0 );

	icon_size = 30.0f * gHUD.fScale;
	full_frame_h = 64 * gHUD.fScale;
	full_frame_w = 280 * gHUD.fScale;
	total_cells_width = full_frame_w - 20; // 10px borders from left and right
	cell_width = 1.0f / ((total_cells + ((total_cells - 1) / 4.0f)) / (float)total_cells_width);
	cell_height = full_frame_h / 3.0f;
	cell_margin = cell_width * 0.25f;

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

	float pos_x = (ScreenWidth / 32) - 30;
	pos_x += gHUD.fCenteredPadding;
	float pos_y = ScreenHeight - (75 * gHUD.fScale);
	pos_y += (full_frame_h / 3.0f) + 10; // accounting for health bar

	if( stamina_icon )
	{
		GL_Bind( 0, stamina_icon );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		GL_Color4f( 70.f / 255.f, 169.f / 255.f, 1.0f, 1.0f );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( pos_x, pos_y - 5, pos_x + icon_size, pos_y + icon_size - 5 );
		gEngfuncs.pTriAPI->End();
	}

	if( gHUD.IsDrawingOfflineHUD || CL_IsDead() || gHUD.HUDSuitOffline )
	{
		DrawOfflineBar( pos_x, pos_y );
		return 1;
	}

	float r = 70.f / 255.f;
	float g = 169.f / 255.f;
	float b = 1.0f;
	float a = 0.65f;

	float stamina_val = m_iStaminaValue;
	
	if( stamina_val < 25.f )
	{
		r = 220.f;
		g = b = 0.0f;
		a += fabs( sin( tr.time * 3 ) ) * 0.25f;
	}

	float cell_start_x = pos_x + icon_size + 10;
	float cell_start_y = pos_y;
	int cell;

	for( cell = 0; cell < total_cells; cell++ )
	{
		if( cell >= stamina_val * 0.5f ) // draw grey cells
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
		else
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( r, g, b, a ) );
		cell_start_x += cell_width + cell_margin;
	}

	// draw number
	char number[5];
	sprintf_s( number, "%3d", (int)stamina_val );
	int text_pos_x = pos_x + icon_size + full_frame_w;
	int text_pos_y = pos_y;
	DrawString( text_pos_x, text_pos_y, number, 70, 169, 255 );

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
	int g = 0;
	int b = 0;

	OFFLINEFlashAlpha = 128 + (fabs( sin( tr.time * 3 ) ) * 128);
	OFFLINEFlashAlpha = bound( 128, OFFLINEFlashAlpha, 255 );

	ScaleColors( r, g, b, (int)OFFLINEFlashAlpha );

	DrawString( x + 10 + icon_size, y, "O F F L I N E", r, g, b );
}