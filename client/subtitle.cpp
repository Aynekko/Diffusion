#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "parsemsg.h"

//===========================================================================================
// diffusion - for dialogues
// input - message from server with two titles.txt headers for speaker name and sentence
// used by scripted_sentence only
//===========================================================================================

static int char_height = 0;
const float fadein_speed = 2.0f;
const float fadeout_speed = 0.35f;

DECLARE_MESSAGE( m_Subtitle, Subtitle )

int CHudSubtitle::Init( void )
{
	HOOK_MESSAGE( Subtitle );
	gHUD.AddHudElem( this );
	return 1;
}

int CHudSubtitle::VidInit( void )
{
	m_iFlags |= HUD_ACTIVE;
	pName[0] = '\0';
	pText[0] = '\0';
	draw_time = 0.0f;
	char_height = gHUD.m_scrinfo.iCharHeight + 5; // add some space between lines
	alpha = 0.0f;
	return 1;
}

int CHudSubtitle::MsgFunc_Subtitle( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	Q_strncpy( pName, READ_STRING(), 63 );
	Q_strncpy( pText, READ_STRING(), 2047 );
	END_READ();

	if( cl_subtitles->value <= 0 )
	{
		draw_time = tr.time; // let it fade out
		return 1; // read but don't accept it if disabled
	}

	// get the name of the speaker from titles.txt
	client_textmessage_t *TitlesMsg;

	TitlesMsg = TextMessageGet( pName );
	if( TitlesMsg )
		Q_strncpy( pName, TitlesMsg->pMessage, 63 );

	// get his speech from there too
	TitlesMsg = TextMessageGet( pText );
	if( TitlesMsg )
		Q_strncpy( pText, TitlesMsg->pMessage, 2047 );

	// get maximum dialogue width and height
	const char *txt = pText;
	subtitle_width = 0;
	int tmp_width = 0;
	int num_chars = 0;
	subtitle_height = char_height; // 1 line with the name already added
	unsigned char c;
	while( *txt )
	{
		c = *txt;
		if( c == '\n' )
		{
			subtitle_height += char_height; // got 1 line
			tmp_width = 0;
		}
		else
		{
			tmp_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 ); // hack to get actual character width
			num_chars++;
			if( tmp_width > subtitle_width )
				subtitle_width = tmp_width;
		}
		txt++;
	}

	// add 1 line height because last line doesn't have the \n
	subtitle_height += char_height;

	// need to check name width too...it can be longer than actual message
	txt = pName;
	tmp_width = 0;
	while( *txt )
	{
		c = *txt;
		tmp_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 ); // hack to get actual character width
		if( tmp_width > subtitle_width )
			subtitle_width = tmp_width;
		txt++;
	}

	// average reading speed is 238 WPM which is ~20 CPS
	// with this in mind, calculate how long the subtitle is going to be visible on screen
	draw_time = tr.time + (float)num_chars / 20.0f;

	return 1;
}

int CHudSubtitle::Draw( float flTime )
{
	if( draw_time <= 0.0f )
		return 1;

	if( tr.time == tr.oldtime )
		return 1;
	
	if( tr.time > draw_time )
	{
		// fade out
		if( alpha > 0.0f )
		{
			alpha -= fadeout_speed * g_fFrametime;
			if( alpha <= 0.0f )
			{
				alpha = 0.0f;
				return 1;
			}
		}
		else
			return 1;
	}
	else // fade in
	{
		if( alpha < 1.0f )
		{
			alpha += fadein_speed * g_fFrametime;
			if( alpha > 1.0f )
				alpha = 1.0f;
		}
	}

	// text position
	int x = ScreenWidth * 0.1f;
	int y = ScreenHeight * 0.85f;

	// draw background
	const int frameborder = 15;
	int frame_height = subtitle_height + frameborder + frameborder;
	int frame_width = subtitle_width + frameborder + frameborder;
	y -= frame_height;

	FillRoundedRGBA( x - frameborder, y - frameborder, frame_width, frame_height, 10.0f, Vector4D( 0.1f, 0.1f, 0.1f, alpha * 0.75f ) );

	int tmp_x = x;
	int tmp_y = y;
	const char *tmp_text;
	unsigned char c;
	int r, g, b;
	r = 70;
	g = 169;
	b = 255;
	ScaleColors( r, g, b, alpha * 255.0f );

	// draw name
	tmp_text = pName;
	while( *tmp_text )
	{
		c = *tmp_text;
		tmp_x += TextMessageDrawChar( tmp_x, tmp_y, c, r, g, b );

		tmp_text++;
	}
	
	// draw dialogue
	r = 255;
	g = 255;
	b = 255;
	ScaleColors( r, g, b, alpha * 255.0f );
	tmp_x = x;
	tmp_y = y + char_height;
	tmp_text = pText;	
	while( *tmp_text )
	{
		c = *tmp_text;
		if( c == '\n' )
		{
			// next line, reset
			tmp_x = x;
			tmp_y += char_height;
		}
		else
		{
			tmp_x += TextMessageDrawChar( tmp_x, tmp_y, c, r, g, b );
		}
		tmp_text++;
	}

	return 1;
}
