#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_cvars.h"
#include "r_local.h"
#include "triangleapi.h"


/*
Example tutorial in titles.txt:

tutor_intro
{
#showscores# - view current objective
#forward# #moveleft# #back# #moveright# - movement
#sprint# - sprint
#duck# - duck
#jump# - jump
#use# - interact

Press #use# to dismiss.
}
*/

DECLARE_MESSAGE( m_StatusIconsTutor, StatusIconTutor );

int CHudTutorial::Init( void )
{
	HOOK_MESSAGE( StatusIconTutor );
	gHUD.AddHudElem( this );
	m_iFlags &= ~HUD_ACTIVE;
	return 1;
}

int CHudTutorial::VidInit( void )
{
	IsTutorDrawing = false;
	TutorStartTime = 0;
	alpha_direction = false;
	CurrentImage = 0;
	return 1;
}

int CHudTutorial::MsgFunc_StatusIconTutor( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	char pszTutorialName[128];
	Q_snprintf( pszTutorialName, sizeof( pszTutorialName ), READ_STRING() );
	const char *img = READ_STRING();
	END_READ();

	CurrentImage = -1;
	if( img[0] != '\0'  )
		CurrentImage = LOAD_TEXTURE( img, NULL, 0, 0 );
	
	EnableTutorial( pszTutorialName );

	return 1;
}

void CHudTutorial::EnableTutorial( const char *pszTutorialName )
{
	if( !cl_tutor->value )
		return;

	tutorial = TextMessageGet( pszTutorialName ); // titles.txt

	if( !tutorial )
	{
		ConPrintf( "^1Error:^7 Couldn't get tutorial \"%s\" from titles.txt\n", pszTutorialName );
		return;
	}

	// copy the message from titles.txt as is
	tutorial_text[0] = '\0';
	Q_snprintf( tutorial_text, sizeof( tutorial_text ), tutorial->pMessage );
	
	// now replace the keybindings like #attack# etc.
	char temp[sizeof( tutorial_text )];
	UTIL_ReplaceKeyBindings( tutorial_text, sizeof( tutorial_text ), temp );
	Q_snprintf( tutorial_text, sizeof( tutorial_text ), temp );

	// get tutorial window width and height (this draws text offscreen to get proper values)
	MessageDraw( tutorial, 0, 0, true );

	TutorStartTime = gHUD.m_flTime;
	IsTutorDrawing = true;
	alpha_direction = false;
	alpha = 0.f;

	PlaySound( "misc/tutor.wav", 1.0 );

	m_iFlags |= HUD_ACTIVE;
}

int CHudTutorial::Draw( float flTime )
{
	if( tr.time == tr.oldtime ) // paused
		return 0;

	if( !IsTutorDrawing )
		return 0;

	// set the coordinates
	// offset to the right if the drone bars are visible
	const float drone_x_offset = (gHUD.m_DroneBars.CanUseDrone ? 80.0f : 0.0f);
	const float x = 30 + drone_x_offset;
	const float y = ScreenHeight * 0.25f;

	if( !alpha_direction ) // tutor is now showing
	{
		alpha = lerp( alpha, 1.2f, 0.5f * g_fFrametime );
		if( alpha > 1.0f ) alpha = 1.0f;
	}
	else // tutor is hiding
	{
		alpha -= 2 * g_fFrametime;

		// fully hidden, finish up
		if( alpha <= 0 )
		{
			alpha = 0;
			IsTutorDrawing = false;
			m_iFlags &= ~HUD_ACTIVE;
			TutorStartTime = 0;
			return 0;
		}
	}

	const int border = 20;
	int image_w = 0;
	int image_h = 0;

	// get image dimensions
	if( CurrentImage != -1 )
	{
		if( CurrentImage > 0 )
		{
			image_w = RENDER_GET_PARM( PARM_TEX_WIDTH, CurrentImage );
			image_h = RENDER_GET_PARM( PARM_TEX_HEIGHT, CurrentImage );
		}
		else // default texture
		{
			image_w = 128;
			image_h = 128;
		}

		// bound and scale the dimensions to prevent stretching
		if( image_w > 512 )
		{
			image_h *= 512.0f / image_w;
			image_w = 512;

		}
		if( image_h > 512 )
		{
			image_w *= 512.0f / image_h;
			image_h = 512;
		}

		if( Twidth < image_w )
			Twidth += image_w - Twidth;
	}

	// draw tutorial frame/background
	FillRoundedRGBA( x, y, Twidth + (border * 2.0f), Theight + image_h + (border * (CurrentImage != -1 ? 3.0f : 2.0f)), 10.0f, Vector4D( 0.5f, 0.5f, 0.5f, alpha * 0.8f ) );

	// draw the image if present
	if( CurrentImage != -1 )
	{
		GL_Bind( 0, CurrentImage );
		GL_Color4f( 1.0f, 1.0f, 1.0f, alpha );
		GL_Blend( GL_TRUE );
		pglBegin( GL_QUADS );
		DrawQuad( x + border, y + border, x + border + image_w, y + border + image_h );
		pglEnd();
	}

	// draw the tutorial text
	MessageDraw( tutorial, x + border, y + border + (CurrentImage != -1 ? border : 0) + image_h );
	
	return 1;
}

void CHudTutorial::MessageDraw( client_textmessage_t *pMessage, int x, int y, bool GetSize )
{
	if( !pMessage )
		return;

	int i, j, length, width;
	const char *pText;
	const char *pLineStart;
	message_parms_t m_parms;

	pText = tutorial_text;
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
			{
				m_parms.totalWidth = width;
			}
			width = 0;
		}
		else
			width += gHUD.m_scrinfo.charWidths[*pText];
		pText++;
		length++;
	}

	m_parms.length = length;
	m_parms.totalHeight = (m_parms.lines * gHUD.m_scrinfo.iCharHeight);
	if( GetSize )
		Theight = m_parms.totalHeight;

	m_parms.y = y;

	pText = tutorial_text;

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

		if( GetSize && Twidth < m_parms.width )
			Twidth = m_parms.width;

		pText++; // Skip LF

		m_parms.x = x;

		for( j = 0; j < m_parms.lineLength; j++ )
		{
			m_parms.text = (unsigned char)pLineStart[j];
			int next = m_parms.x;

			const int txt_a = alpha * 255;
			if( m_parms.x >= 0 && m_parms.y >= 0 && next <= ScreenWidth )
				next += TextMessageDrawChar( m_parms.x, m_parms.y, m_parms.text, txt_a, txt_a, txt_a );
			m_parms.x = next;
		}

		m_parms.y += gHUD.m_scrinfo.iCharHeight;
	}
}
