#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "r_local.h"

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
int cursor_img = 0;
#define CURSOR_SIZE 24

DECLARE_MESSAGE( m_PseudoGUI, ShowNote );

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
	gHUD.AddHudElem( this );
	return 1;
}

int CPseudoGUI::VidInit( void )
{
	m_iFlags = 0;
	active_button_alpha = 0;
	MousePressed = false;
	cursor_img = LOAD_TEXTURE( "sprites/diffusion/cursor.dds", NULL, 0, 0 );
	cursor_x = ScreenWidth / 2;
	cursor_y = ScreenHeight / 2;
	m_szMOTD[0] = '\0';
	return 1;
}

int CPseudoGUI::MsgFunc_ShowNote( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	sprintf_s( Note, READ_STRING() );
	END_READ();

	Enable();

	return 1;
}

void CPseudoGUI::Think( void )
{
	// I need to do this trickery so we wouldn't shoot a gun while and after pressing the button
	if( gHUD.m_iKeyBits & IN_ATTACK )
	{
		MousePressed = true;
		gHUD.m_iKeyBits &= ~IN_ATTACK;
	}
}

void CPseudoGUI::Enable( void )
{
	m_iFlags |= HUD_ACTIVE;
	active_button_alpha = 0;
	MousePressed = false;
}

int CPseudoGUI::Draw( float flTime )
{
	// not in paused
	if( tr.time == tr.oldtime )
		return 1;

	gEngfuncs.GetMousePosition( &cursor_x, &cursor_y );

	if( IS_NAN( cursor_x ) || IS_NAN( cursor_y ) )
		return 1;

	// draw darken frame
	rectangle_t frame;
	frame.w = 666;
	frame.h = 666;
	frame.x = (ScreenWidth / 2) - (frame.w / 2);
	frame.y = (ScreenHeight / 2) - (frame.h / 2);
	frame.r = frame.g = frame.b = 80.f / 255.f;
	frame.a = 0.8f;
	FillRoundedRGBA( frame.x, frame.y, frame.w, frame.h, 20, Vector4D( frame.r, frame.g, frame.b, frame.a ) );

	// draw text
	// is it MOTD?
	if( m_szMOTD[0] != '\0' )
	{
		client_textmessage_t MOTD;
		MOTD.pMessage = m_szMOTD;
		MOTD.r1 = MOTD.g1 = MOTD.b1 = 255;
		MessageDraw( &MOTD, frame.x + frame.w * 0.1, frame.y + frame.h * 0.1 );
	}
	else // it's a note
		MessageDraw( TextMessageGet( Note ), frame.x + frame.w * 0.1, frame.y + frame.h * 0.1 );

	// draw "Close" button
	rectangle_t Close;
	Close.r = Close.g = Close.b = 25.f / 255.f;
	Close.a = 0.75f;
	Close.w = frame.w * 0.5f;
	Close.h = 50;
	Close.x = frame.x + (frame.w / 2) - (Close.w / 2);
	Close.y = (frame.y + frame.h) - Close.h - 20; // frame bottom - button height - 20

	// inactive button
	FillRoundedRGBA( Close.x, Close.y, Close.w, Close.h, 10, Vector4D( Close.r, Close.g, Close.b, Close.a ) );

	// active button
	if( DotInRect( &Close, cursor_x, cursor_y ) )
	{
		active_button_alpha = CL_UTIL_Approach( 1.0f, active_button_alpha, 10 * g_fFrametime );

		if( MousePressed )
		{
			// close the window
			m_iFlags &= ~HUD_ACTIVE;
			MousePressed = false;
			m_szMOTD[0] = '\0'; // terminate motd too
		}
	}
	else
		active_button_alpha = CL_UTIL_Approach( 0.0f, active_button_alpha, 10 * g_fFrametime );

	Close.r = 70.f / 255.f;
	Close.g = 169.f / 255.f;
	Close.b = 1.0f;
	Close.a = active_button_alpha;

	if( active_button_alpha > 0 )
		FillRoundedRGBA( Close.x, Close.y, Close.w, Close.h, 10, Vector4D( Close.r, Close.g, Close.b, Close.a ) );

	// draw "close" text
	char bttn_close[12];
	sprintf_s( bttn_close, "BTTN_CLOSE" );
	client_textmessage_t *MsgClose = TextMessageGet( bttn_close );
	if( MsgClose )
	{
		// we need to count the length to figure out the coordinates...
		const char *pText = MsgClose->pMessage;
		int width = 0;
		while( *pText )
		{
			if( *pText == '\n' )
				break;
			else
				width += gHUD.m_scrinfo.charWidths[*pText];

			pText++;
		}

		MessageDraw( MsgClose, Close.x + (Close.w / 2) - (width / 2), Close.y + (Close.h / 2) - (gHUD.m_scrinfo.iCharHeight / 2) );
	}

	// draw cursor last
	if( cursor_img )
	{
		GL_Bind( GL_TEXTURE0, cursor_img );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		GL_Color4f( 1, 1, 1, 1 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( cursor_x, cursor_y, cursor_x + CURSOR_SIZE, cursor_y + CURSOR_SIZE );
		gEngfuncs.pTriAPI->End();
	}
	else // no image?
		gEngfuncs.pfnFillRGBA( cursor_x, cursor_y, 10, 10, 255, 255, 255, 255 );

	// reset mouse press
	MousePressed = false;

	return 1;
}

// basically a copy-paste from hudmessages
void CPseudoGUI::MessageDraw( client_textmessage_t *pMessage, int x, int y )
{
	if( !pMessage )
		return;

	int i, j, length, width;
	const char *pText;
	const char *pLineStart;
	message_parms_t m_parms;

	pText = pMessage->pMessage;
	// Count lines
	m_parms.lines = 1;
	m_parms.pMessage = pMessage;
	length = 0;
	width = 0;
	m_parms.totalWidth = 0;

	while( *pText )
	{
		if( *pText == '\n' )
		{
			m_parms.lines++;
			if( width > m_parms.totalWidth )
				m_parms.totalWidth = width;
			width = 0;
		}
		else
			width += gHUD.m_scrinfo.charWidths[*pText];
		pText++;
		length++;
	}

	m_parms.length = length;
	m_parms.totalHeight = (m_parms.lines * gHUD.m_scrinfo.iCharHeight);

	m_parms.y = y;
	pText = pMessage->pMessage;

	for( i = 0; i < m_parms.lines; i++ )
	{
		m_parms.lineLength = 0;
		m_parms.width = 0;
		pLineStart = pText;
		while( *pText && *pText != '\n' )
		{
			unsigned char c = *pText;
		//	m_parms.width += gHUD.m_scrinfo.charWidths[c];
			m_parms.width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 ); // thx to a1batross for idea (not ideal, but it will have to do...)
			m_parms.lineLength++;
			pText++;
		}

		pText++; // Skip LF

		m_parms.x = x;

		for( j = 0; j < m_parms.lineLength; j++ )
		{
			m_parms.text = (unsigned char)pLineStart[j];
			int next = m_parms.x;

			if( m_parms.x >= 0 && m_parms.y >= 0 && next <= ScreenWidth )
				next += TextMessageDrawChar( m_parms.x, m_parms.y, m_parms.text, pMessage->r1, pMessage->g1, pMessage->b1 );
			m_parms.x = next;
		}
		m_parms.y += gHUD.m_scrinfo.iCharHeight;
	}
}