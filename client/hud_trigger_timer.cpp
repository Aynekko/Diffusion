#include "hud.h"
#include "utils.h"
#include "r_local.h"

//===========================================================================================
// diffusion - HUD for trigger_timer
//===========================================================================================

int CHudTriggerTimer::Init( void )
{
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );
	return 1;
}

int CHudTriggerTimer::VidInit(void)
{
	message[0] = '\0';
	enabled = 0;
	return 1;
}

int CHudTriggerTimer::Draw( float flTime )
{
	if( !enabled )
		return 1;
	
	if( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 1;

	const char *pText;
	int text_pos_x;
	int text_pos_y;
	int text_width;
	bool frame_drawn = false;

	// draw objective message
	// figure out text width using this hack
	if( message[0] != '\0' )
	{
		text_width = 0;
		pText = message;
		while( *pText )
		{
			unsigned char c = *pText;
			text_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 );
			pText++;
		}
		// draw the frame first
		FillRoundedRGBA( ScreenWidth - text_width - 60, (ScreenHeight * 0.1f) - 5, text_width + 20, gHUD.m_scrinfo.iCharHeight * 3.0f, 5, Vector4D( 0.314f, 0.314f, 0.314f, 0.8f ) );
		frame_drawn = true;
		text_pos_x = ScreenWidth - text_width - 50;
		text_pos_y = ScreenHeight * 0.1f;
		DrawString( text_pos_x, text_pos_y, message, 70, 169, 255 );
	}

	// draw the timer message
	int seconds = timer;
	int minutes = seconds / 60.0f;
	seconds = seconds - minutes * 60;
	char timer_text[20];
	if( seconds >= 10 )
		Q_snprintf( timer_text, sizeof( timer_text ), "Time left: %i:%i", minutes, seconds );
	else
		Q_snprintf( timer_text, sizeof( timer_text ), "Time left: %i:0%i", minutes, seconds );

	// figure out text width using this hack
	text_width = 0;
	pText = timer_text;
	while( *pText )
	{
		unsigned char c = *pText;
		text_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 );
		pText++;
	}

	// draw the frame if it wasn't yet
	if( !frame_drawn )
		FillRoundedRGBA( ScreenWidth - text_width - 60, (ScreenHeight * 0.1f) - 5, text_width + 20, gHUD.m_scrinfo.iCharHeight * 1.5f, 5, Vector4D( 0.314f, 0.314f, 0.314f, 0.8f ) );

	text_pos_x = ScreenWidth - text_width - 50;
	text_pos_y = ScreenHeight * 0.1f;
	if( message[0] != '\0' ) // if the message was drawn, draw the timer after it
		text_pos_y += gHUD.m_scrinfo.iCharHeight * 1.5f;

	if( critical ) // less than 20% of time left
		DrawString( text_pos_x, text_pos_y, timer_text, 255, 0, 0 );
	else
		DrawString( text_pos_x, text_pos_y, timer_text, 70, 169, 255 );

	return 1;
}
