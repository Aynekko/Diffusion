#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "string.h"
#include "triangleapi.h"
#include <stdio.h>

DECLARE_MESSAGE( m_HealthVisual, HealthVisual )

float OFFLINEFlashAlphaH = 0;
int health_icon = 0;
const int icon_size = 30;

// size of an invisible drawing field...
const int full_frame_h = 64;
const int full_frame_w = 280;
const int total_cells_width = full_frame_w - 20; // 10px borders from left and right
const int total_cells = 50;
const float cell_width = 1.0f / ((total_cells + ((total_cells - 1) / 4.0f)) / (float)total_cells_width);
const float cell_height = full_frame_h / 3.0f;
const float cell_margin = cell_width * 0.25f;

int CHudHealthVisual :: Init(void)
{
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE( HealthVisual );
	gHUD.AddHudElem(this);
	return 1;
};

int CHudHealthVisual :: VidInit(void)
{
	health_icon = LOAD_TEXTURE( "sprites/diffusion/health.dds", NULL, 0, 0 );
	return 1;
};

int CHudHealthVisual:: MsgFunc_HealthVisual( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	GotHit = (READ_BYTE() > 0);
	m_iHealth = READ_BYTE();
	m_iMaxHealth = READ_BYTE();
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

	float pos_x = (ScreenWidth / 32) - 30;
	float pos_y = ScreenHeight - 75;

	if( health_icon )
	{
		GL_Bind( 0, health_icon );
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

	static float health_val = 0.0f;
	health_val = CL_UTIL_Approach( m_iHealth, health_val, g_fFrametime * 500 );

	float r = 0.8f;
	float g = 0.08f;
	float b = 0.08f;

	if( GotHit )
	{
		if( health_val <= 100 )
			Transparency = 1.0f;
		else
			Transparency = 0.65f;

		GotHit = false;
	}

	if( Transparency > 0.65f )
		Transparency -= 0.1f * g_fFrametime;

	if( Transparency < 0.65f )
		Transparency = 0.65f;

	float cell_start_x = pos_x + icon_size + 10;
	float cell_start_y = pos_y;
	int cell;

	for( cell = 0; cell < total_cells; cell++ )
	{
		if( cell >= health_val * 0.5f ) // draw grey cells
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
		else
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( r, g, b, (health_val < 25.f) ? (Transparency + fabs( sin( tr.time * 3 ) ) * 0.25f) : Transparency ) );
		cell_start_x += cell_width + cell_margin;
	}

	if( health_val > 100 ) // draw same stuff on top (only red cells)
	{
		cell_start_x = pos_x + icon_size + 10;
		for( cell = 0; cell < total_cells; cell++ )
		{
			if( cell > ( health_val - 100 ) * 0.5f )
				break;
			
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( r, g, b, Transparency ) );
			cell_start_x += cell_width + cell_margin;
		}
	}

	// draw numbers
	char numbers[16];
	sprintf_s( numbers, "%3d / %i", (int)health_val, m_iMaxHealth );
	int text_pos_x = pos_x + icon_size + full_frame_w;
	int text_pos_y = pos_y;
	DrawString( text_pos_x, text_pos_y, numbers, 70, 169, 255 );

	return 1;
}

void CHudHealthVisual::DrawOfflineBar( int x, int y )
{
	int r = 255;
	int g = 0;
	int b = 0;
		
	OFFLINEFlashAlphaH = 128 + (fabs( sin( tr.time * 3 ) ) * 128);
	OFFLINEFlashAlphaH = bound( 128, OFFLINEFlashAlphaH, 255 );

	ScaleColors( r, g, b, (int)OFFLINEFlashAlphaH );

	DrawString( x + 10 + icon_size, y, "O F F L I N E", r, g, b );
}
