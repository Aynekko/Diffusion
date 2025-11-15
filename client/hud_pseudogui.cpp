#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "r_local.h"
#include "keydefs.h"

/*
====================================================================================
--- funny experiments! (^_^) ---
The only purpose of this whole thing here is to show a fixed-sized window
with a fixed-sized CLOSE button, and a text of any kind.
Player reads the text and closes the window, that's all there is to it.
The texts are taken from titles.txt or feeded by MOTD from server.
====================================================================================
*/

char Note[255];
float active_button_alpha = 0;
float text_start_x = 0;
float text_start_y = 0;
int text_frame_width = 0;
const char *BtnText = NULL;
int BtnTextWidth;

#define FRAME_WIDTH 666
#define FRAME_HEIGHT 666
#define BUTTON_HEIGHT 50

DECLARE_MESSAGE( m_PseudoGUI, ShowNote );

void CPseudoGUI::CloseWindow( bool mouse )
{
	if( mouse )
	{
		if( !DotInRect( &rClose, gHUD.m_Cursor.x, gHUD.m_Cursor.y ) )
			return;
	}
	
	// close the window
	m_iFlags &= ~HUD_ACTIVE;
	curText = NULL;
	m_szMOTD[0] = '\0'; // terminate motd too
}

bool CPseudoGUI::DotInRect( rectangle_t *rect, int x, int y )
{
	if( !rect )
		return false;
	
	if( x < rect->x )
		return false;

	if( x > rect->x + rect->w )
		return false;

	if( y < rect->y )
		return false;

	if( y > rect->y + rect->h )
		return false;

	return true;
}

int CPseudoGUI::Init( void )
{
	HOOK_MESSAGE( ShowNote );
	m_szMOTD[0] = '\0';
	curText = NULL;
	gHUD.AddHudElem( this );
	return 1;
}

int CPseudoGUI::VidInit( void )
{
	m_iFlags = 0;
	active_button_alpha = 0;
	m_szMOTD[0] = '\0';
	curText = NULL;
	scrolled_lines = 0;

	// init frame
	rFrame.w = FRAME_WIDTH * gHUD.fScale;
	rFrame.h = FRAME_HEIGHT * gHUD.fScale;
	rFrame.x = (ScreenWidth / 2) - (rFrame.w / 2);
	rFrame.y = (ScreenHeight / 2) - (rFrame.h / 2);
	rFrame.r = rFrame.g = rFrame.b = 80.f / 255.f;
	rFrame.a = 0.8f;

	// init Close button
	rClose.w = rFrame.w * 0.5f;
	rClose.h = BUTTON_HEIGHT * gHUD.fScale;
	rClose.x = rFrame.x + (rFrame.w / 2) - (rClose.w / 2);
	rClose.y = (rFrame.y + rFrame.h) - rClose.h - 20 * gHUD.fScale; // frame bottom - button height - 20
	client_textmessage_t *MsgClose = TextMessageGet( "BTTN_CLOSE" );
	BtnText = NULL;
	if( MsgClose )
	{
		// we need to count the length to figure out the coordinates...
		BtnText = MsgClose->pMessage;
		BtnTextWidth = 0;
		while( *BtnText )
		{
			if( *BtnText == '\n' )
				break;
			else
			{
				unsigned char c = *BtnText;
				BtnTextWidth += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 );
			}

			BtnText++;
		}

		BtnText = MsgClose->pMessage;
		rClose.w = BtnTextWidth + 300 * gHUD.fScale;
		rClose.x = rFrame.x + (rFrame.w / 2) - (rClose.w / 2);
	}
	return 1;
}

int CPseudoGUI::MsgFunc_ShowNote( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	Q_strncpy( Note, READ_STRING(), sizeof( Note ));
	END_READ();

	Enable();

	return 1;
}

//==============================================================================
// setup the message here to not do some calculations every frame
// this can also be called from motd.cpp
//==============================================================================
void CPseudoGUI::Enable( void )
{
	// is it MOTD?
	if( m_szMOTD[0] != '\0' )
	{
		curText = m_szMOTD;
	}
	else
	{
		client_textmessage_t *tmp = TextMessageGet( Note );
		if( !tmp )
		{
			ConPrintf( "^1Error:^7 Couldn't get message \"%s\" from titles.txt\n", Note );
			return;
		}

		curText = tmp->pMessage;
	}

	// Count lines
	num_lines = 1;
	const char *tmp_text = curText;
	int width = 0;
	int totaltextWidth = 0;

	while( *tmp_text )
	{
		if( *tmp_text == '\n' )
		{
			num_lines++;
			if( width > totaltextWidth )
				totaltextWidth = width;
			width = 0;
		}
		else
		{
			unsigned char c = *tmp_text; // note: this is important! always use this, don't just put *tmp_text into TextMessageDrawChar, it won't get width correctly!
			width += TextMessageDrawChar( -1, -1, c, 0, 0, 0 );
		}
		tmp_text++;
	}
	
	totaltextWidth += 5; // add a bit

	border = 60 * gHUD.fScale;
	text_start_x = rFrame.x + border;
	text_start_y = rFrame.y + border;
	text_frame_width = rFrame.w - border - border;

	// re-init the frame if text is wider
	if( totaltextWidth > text_frame_width )
	{
		float new_frame_width = border + totaltextWidth + border;
		rFrame.w = new_frame_width;
		rFrame.h = FRAME_HEIGHT * gHUD.fScale;
		rFrame.x = (ScreenWidth / 2) - (rFrame.w / 2);
		rFrame.y = (ScreenHeight / 2) - (rFrame.h / 2);
		rFrame.r = rFrame.g = rFrame.b = 80.f / 255.f;
		rFrame.a = 0.8f;

		// init Close button
		rClose.w = rFrame.w * 0.5f;
		rClose.h = BUTTON_HEIGHT * gHUD.fScale;
		rClose.x = rFrame.x + (rFrame.w / 2) - (rClose.w / 2);
		rClose.y = (rFrame.y + rFrame.h) - rClose.h - 20 * gHUD.fScale; // frame bottom - button height - 20

		client_textmessage_t *MsgClose = TextMessageGet( "BTTN_CLOSE" );
		BtnText = NULL;
		if( MsgClose )
		{
			// we need to count the length to figure out the coordinates...
			BtnText = MsgClose->pMessage;
			BtnTextWidth = 0;
			while( *BtnText )
			{
				if( *BtnText == '\n' )
					break;
				else
				{
					unsigned char c = *BtnText;
					BtnTextWidth += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 );
				}

				BtnText++;
			}

			BtnText = MsgClose->pMessage;
			rClose.w = BtnTextWidth + 300 * gHUD.fScale;
			rClose.x = rFrame.x + (rFrame.w / 2) - (rClose.w / 2);
		}

		text_start_x = rFrame.x + border;
		text_start_y = rFrame.y + border;
		text_frame_width = rFrame.w - border - border;
	}

	m_iFlags |= HUD_ACTIVE;
	active_button_alpha = 0;
	scrolled_lines = 0;

	// set cursor position to a plausible place
	gEngfuncs.pfnSetMousePos( gEngfuncs.GetWindowCenterX(), gEngfuncs.GetWindowCenterY() + ScreenHeight * 0.15f );
}

int CPseudoGUI::Draw( float flTime )
{
	// not in paused
	if( tr.time == tr.oldtime )
		return 1;

	if( !gHUD.m_Cursor.GetMousePosition() )
		return 1;

	if( curText == NULL )
		return 1;

	// draw darken frame
	FillRoundedRGBA( rFrame.x, rFrame.y, rFrame.w, rFrame.h, 20, Vector4D( rFrame.r, rFrame.g, rFrame.b, rFrame.a ) );

	// draw black background for text
	const float bord_from_below = rClose.h + 40 * gHUD.fScale;
	const float bord1 = 15 * gHUD.fScale;
	gEngfuncs.pfnFillRGBABlend( text_start_x - bord1, text_start_y - bord1, text_frame_width + bord1 * 2.0f, rFrame.h - border - bord_from_below + bord1, 0, 0, 0, 100 );
	const float bord2 = 10 * gHUD.fScale;
	gEngfuncs.pfnFillRGBABlend( text_start_x - bord2, text_start_y - bord2, text_frame_width + bord2 * 2.0f, rFrame.h - border - bord_from_below + bord2 * 0.5f, 0, 0, 0, 100 );

	// draw text
	MessageDraw( curText, text_start_x, text_start_y );

	// draw "Close" button
	rClose.r = rClose.g = rClose.b = 0.1f;// 25 / 255;
	rClose.a = 0.75f;

	// inactive button
	FillRoundedRGBA( rClose.x, rClose.y, rClose.w, rClose.h, 10, Vector4D( rClose.r, rClose.g, rClose.b, rClose.a ) );

	// active button
	if( DotInRect( &rClose, gHUD.m_Cursor.x, gHUD.m_Cursor.y ) )
		active_button_alpha = CL_UTIL_Approach( 1.0f, active_button_alpha, 10 * g_fFrametime );
	else
		active_button_alpha = CL_UTIL_Approach( 0.0f, active_button_alpha, 10 * g_fFrametime );

	rClose.r = 0.275f;// 70 / 255;
	rClose.g = 0.663f;// 169 / 255;
	rClose.b = 1.0f;
	rClose.a = active_button_alpha;

	if( active_button_alpha > 0 )
		FillRoundedRGBA( rClose.x, rClose.y, rClose.w, rClose.h, 10, Vector4D( rClose.r, rClose.g, rClose.b, rClose.a ) );

	// draw "close" text
	if( BtnText )
		DrawString( rClose.x + (rClose.w / 2) - (BtnTextWidth / 2), rClose.y + (rClose.h / 2) - (gHUD.m_scrinfo.iCharHeight / 2), BtnText, 255, 255, 255 );

	// draw cursor last
	gHUD.m_Cursor.DrawCursor();

	return 1;
}

// basically a copy-paste from hudmessages
void CPseudoGUI::MessageDraw( const char *pText, int x, int y )
{
	int i, j;
	const char *pLineStart;
	const int totalHeight = (num_lines * gHUD.m_scrinfo.iCharHeight);
	const int text_end_pos = (ScreenHeight / 2) - (rFrame.h / 2) + rFrame.h - (rClose.h * 3) + gHUD.m_scrinfo.iCharHeight;
	const int text_start_pos = y;
	// we need to know how many lines went below the allowed drawing position
	int lines_below = (totalHeight - (text_end_pos - text_start_pos)) / gHUD.m_scrinfo.iCharHeight;
	const bool enable_scrollbar = (lines_below - 1 > 0);
	int tmp_y, tmp_x;

	if( enable_scrollbar )
	{
		scrolled_lines = bound( 0, scrolled_lines, lines_below - 1 );
		tmp_y = y - scrolled_lines * gHUD.m_scrinfo.iCharHeight;
	}
	else
		tmp_y = y;

	const char *tmp_text = pText;

	int lineLength = 0;

	for( i = 0; i < num_lines; i++ )
	{
		if( enable_scrollbar )
		{
			if( i > num_lines - lines_below + scrolled_lines )
				break;
		}
		
		lineLength = 0;
		pLineStart = tmp_text;
		while( *tmp_text && *tmp_text != '\n' )
		{
			unsigned char c = *tmp_text;
			lineLength++;
			tmp_text++;
		}

		tmp_text++; // Skip LF
		tmp_x = x;

		// if we scrolled down, we don't draw text lines above the allowed zone - skip
		if( enable_scrollbar && i < scrolled_lines )
		{
			tmp_y += gHUD.m_scrinfo.iCharHeight;
			continue;
		}

		for( j = 0; j < lineLength; j++ )
		{
			unsigned char text = (unsigned char)pLineStart[j];
			int next = tmp_x;

			if( tmp_x >= 0 && tmp_y >= 0 && next <= ScreenWidth )
				next += TextMessageDrawChar( tmp_x, tmp_y, text, 255, 255, 255 );
			tmp_x = next;
		}
		tmp_y += gHUD.m_scrinfo.iCharHeight;
	}

	if( enable_scrollbar && lines_below > 0 )
	{
		pglEnable( GL_MULTISAMPLE_ARB );
		const float sbar_w = 20 * gHUD.fScale;
		const float sbar_x = rFrame.x + rFrame.w - ((border - ( 15 * gHUD.fScale )) * 0.5f) - (sbar_w * 0.5f);
		const float sbar_h = (text_end_pos - text_start_pos) + gHUD.m_scrinfo.iCharHeight;
		FillRoundedRGBA( sbar_x, y, sbar_w, sbar_h, 2, Vector4D( 0.1f, 0.1f, 0.1f, 0.8f ) );

		// scrollbar handle
		const float handle_x = sbar_x;
		const float handle_y = y + ((sbar_h / num_lines) * scrolled_lines);
		const float handle_h = sbar_h - ((sbar_h / num_lines) * (lines_below - 1));

		FillRoundedRGBA( handle_x, handle_y, sbar_w, handle_h, 2, Vector4D( 0.5f, 0.5f, 0.5f, 0.8f ) );
		pglDisable( GL_MULTISAMPLE_ARB );
	}
}
