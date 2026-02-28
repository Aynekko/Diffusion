#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "string.h"
#include "triangleapi.h"
#include <stdio.h>

DECLARE_MESSAGE( m_HealthVisual, HealthVisual )
DECLARE_MESSAGE( m_HealthVisual, HealthVisualAlice )

static float OFFLINEFlashAlphaH = 0;
static int health_icon = 0;
static int health_alice_img = 0;
static float icon_size = 30;

// size of an invisible drawing field...
static float full_frame_h = 64;
static float full_frame_w = 280;
static float total_cells_width = full_frame_w - 20; // 10px borders from left and right
static const int total_cells = 50;
static float cell_width = 1.0f / ((total_cells + ((total_cells - 1) / 4.0f)) / total_cells_width);
static float cell_height = full_frame_h / 3.0f;
static float cell_margin = cell_width * 0.25f;
static float flash_alpha = 0.0f;

static float alice_lerp_x = 0;

int CHudHealthVisual :: Init(void)
{
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE( HealthVisual );
	HOOK_MESSAGE( HealthVisualAlice );
	gHUD.AddHudElem(this);
	return 1;
};

int CHudHealthVisual :: VidInit(void)
{
	health_icon = LOAD_TEXTURE( "sprites/diffusion/health.dds", NULL, 0, 0 );
	health_alice_img = LOAD_TEXTURE( "sprites/diffusion/alice_health.dds", NULL, 0, 0 );
	alice_lerp_x = -500;

	full_frame_h = 64 * gHUD.fScale;
	full_frame_w = 280 * gHUD.fScale;
	total_cells_width = full_frame_w - 20; // 10px borders from left and right
	cell_width = 1.0f / ((total_cells + ((total_cells - 1) / 4.0f)) / total_cells_width);
	cell_height = full_frame_h / 3.0f;
	cell_margin = cell_width * 0.25f;
	icon_size = 30.0f * gHUD.fScale;
	flash_alpha = 0.0f;

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

int CHudHealthVisual::MsgFunc_HealthVisualAlice( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	bAliceDrawHealth = (READ_BYTE() > 0);
	iAliceHealth = READ_BYTE();
	END_READ();

	return 1;
}

void CHudHealthVisual::DrawAliceHealth( void )
{
	// pic size is 152x500, scale it down x0.65
	// big hud scale also scaled, to not affect it that much
	const float picsizex = 152 * 0.65f * gHUD.fScale * 0.75f;
	const float picsizey = 500 * 0.65f * gHUD.fScale * 0.75f;
	const float xpos = 35;
	const float y = ScreenHeight - 450 * gHUD.fScale;

	if( !bAliceDrawHealth )
	{
		if( alice_lerp_x < -500 )
			return;
		else
			alice_lerp_x = lerp( alice_lerp_x, -500, 5.0f * g_fFrametime );
	}
	else
	{
		if( alice_lerp_x < xpos )
			alice_lerp_x = lerp( alice_lerp_x, xpos + 10, 5.0f * g_fFrametime );
	}

	float x = alice_lerp_x;

	GL_Bind( 0, health_alice_img );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	GL_Color4f( 0.75f, 0.75f, 0.75f, 1.0f ); // grey

	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( x, y, x + picsizex, y + picsizey );
	gEngfuncs.pTriAPI->End();

	// draw vertical healthbar to the right of the Alice's figure
	// background bar
	const float barx = x + picsizex + 15;
	const float barh = picsizey * 0.75f;
	const float bary = y + picsizey * 0.25f;
	const float barw = 15 * gHUD.fScale;
	FillRoundedRGBA( barx, bary, barw, barh, 5, 0.8f, 0.8f, 0.8f, 0.8f );

	// red health
	if( iAliceHealth <= 0 )
		return;

	const float redbarh_full = barh * 0.95f;
	const float redbarh = redbarh_full * (float)(iAliceHealth * 0.01f);
	const float redbarw = barw * 0.9f;
	FillRoundedRGBA( barx + (barw - redbarw) * 0.5f, (bary + (barh - redbarh_full) * 0.5f) + (redbarh_full - redbarh), redbarw, redbarh, 5, 1.0f, 0.1f, 0.1f, 0.75f );
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

	DrawAliceHealth();

	float pos_x = (ScreenWidth / 32) - 30;
	pos_x += gHUD.fCenteredPadding;
	float pos_y = ScreenHeight - (75 * gHUD.fScale);

	if( health_icon )
	{
		GL_Bind( 0, health_icon );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		GL_Color4f( 0.275f, 0.663f, 1.0f, 1.0f ); // 70 169 255

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
		flash_alpha = 0.6f;
		GotHit = false;
	}

	pglEnable( GL_MULTISAMPLE_ARB );

	float cell_start_x = pos_x + icon_size + 10;
	float cell_start_y = pos_y;
	int cell;

	// flash health background when hit
	if( flash_alpha > 0.0f )
	{
		flash_alpha -= g_fFrametime;
		FillRoundedRGBA( cell_start_x - 5, cell_start_y - 5, total_cells_width + 10, cell_height + 10, 10, 1.0f, 0.0f, 0.0f, flash_alpha );
	}

	float transp = (health_val < m_iMaxHealth * 0.25f) ? (0.65f + fabs( sin( tr.time * 3 ) ) * 0.25f) : 0.65f;
	int num_red_cells = (float)total_cells * ((float)m_iHealth / (float)m_iMaxHealth);
	for( cell = 0; cell < total_cells; cell++ )
	{
		if( cell >= num_red_cells ) // draw grey cells
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, 0.5f, 0.5f, 0.5f, 0.5f );
		else
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, r, g, b, transp );
		cell_start_x += cell_width + cell_margin;
	}

	if( health_val > m_iMaxHealth ) // draw same stuff on top (only red cells)
	{
		float spec_cell_height = cell_height * 0.25f;
		cell_start_x = pos_x + icon_size + 10;
		cell_start_y -= spec_cell_height + 5;
		for( cell = 0; cell < total_cells; cell++ )
		{
			if( cell > ( health_val - m_iMaxHealth ) * 0.5f )
				break;

			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, spec_cell_height, 3, r, g, b, 0.8f );
			cell_start_x += cell_width + cell_margin;
		}
	}

	pglDisable( GL_MULTISAMPLE_ARB );

	// draw numbers
	char numbers[16];
	Q_snprintf( numbers, sizeof( numbers ), "%3d / %i", (int)health_val, m_iMaxHealth );
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
