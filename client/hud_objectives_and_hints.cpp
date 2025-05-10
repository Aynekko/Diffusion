#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "parsemsg.h"
#include "event_api.h"

//===========================================================================================
// diffusion - this code contains hint rendering (pops up from the bottom upon activating
// hint entity) and mission objective text rendering (pops from the top when TAB is pressed)
//===========================================================================================

const float HINT_DRAW_TIME = 6.0f;
const float HINT_ICON_SIZE = 60.0f;
const int FRAMEBORDER = 15; // also a border between icon and the text
static const char *default_obj_text = "OBJ_NOACTIVE";

static int custom_char_height = 0;
const float fadein_speed = 2.0f;
const float fadeout_speed = 0.5f;

DECLARE_MESSAGE( m_HintObjectives, Hint )

int CHudHintObjective::Init( void )
{
	HOOK_MESSAGE( Hint );
	gHUD.AddHudElem( this );
	pObjective[0][0] = '\0';
	pObjective[1][0] = '\0';
	client_textmessage_t *DefaultObj = TextMessageGet( default_obj_text );
	if( DefaultObj )
		_snprintf_s( pObjective[0], sizeof( pObjective[0] ) - 1, DefaultObj->pMessage );
	else
		_snprintf_s( pObjective[0], sizeof( pObjective[0] ) - 1, default_obj_text );
	return 1;
}

int CHudHintObjective::VidInit( void )
{
	m_iFlags |= HUD_ACTIVE;
	pHint[0] = '\0';
	custom_char_height = gHUD.m_scrinfo.iCharHeight + 5; // add some space between lines
	hint_alpha = 0.0f;
	HintImage = LOAD_TEXTURE( "sprites/diffusion/hint.dds", NULL, 0, 0 );
	hint_drawstart_time = 0.0f;
	obj_alpha = 0.0f;
	fForcedTimer = 0.0f;
	obj_ypos = -100;
	
	return 1;
}

void CHudHintObjective::SetupHint( void )
{
	// get the hint header from titles.txt
	client_textmessage_t *TitlesMsg = TextMessageGet( pHint );
	if( TitlesMsg )
	{
		pHint[0] = '\0';
		_snprintf_s( pHint, sizeof( pHint ) - 1, TitlesMsg->pMessage );
	}

	pHint[sizeof( pHint ) - 1] = '\0';

	// now replace the keybindings like #attack# etc.
	char temp[sizeof( pHint )];
	UTIL_ReplaceKeyBindings( pHint, sizeof( pHint ), temp );
	_snprintf_s( pHint, sizeof( pHint ), temp );

	// get maximum hint width and height
	const char *txt = pHint;
	// start with icon dimensions as minimum hint size
	hint_width = HINT_ICON_SIZE + FRAMEBORDER;
	hint_height = 0;
	int tmp_width = HINT_ICON_SIZE + FRAMEBORDER;
	unsigned char c;
	while( *txt )
	{
		c = *txt;
		if( c == '\n' )
		{
			hint_height += custom_char_height; // got 1 line
			tmp_width = HINT_ICON_SIZE + FRAMEBORDER;
		}
		else
		{
			tmp_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 ); // hack to get actual character width
			if( tmp_width > hint_width )
				hint_width = tmp_width;
		}
		txt++;
	}

	// make sure icon won't draw beyond the hint frame
	if( hint_height < HINT_ICON_SIZE )
		hint_height = HINT_ICON_SIZE;
	else // add last line height
		hint_height += custom_char_height;

	// prepare to draw the hint
	hint_drawstart_time = tr.time;
	hint_ypos = ScreenHeight + hint_height; // start outside the screen area
	hint_alpha = 0.0f;

	// play hint sound
	gEngfuncs.pEventAPI->EV_PlaySound( 0, Vector( 0, 0, 0 ), CHAN_STATIC, "player/hint.wav", 1.0, 0, 0, 100 );
}

void CHudHintObjective::SetupObjectives( void )
{
	client_textmessage_t *TitlesMsg;
	const char *def_obj = default_obj_text;
	client_textmessage_t *DefaultObj = TextMessageGet( default_obj_text );
	if( DefaultObj )
		def_obj = DefaultObj->pMessage;

	if( pObjective[0][0] != '\0' )
	{
		// get the primary objective header from titles.txt
		TitlesMsg = TextMessageGet( pObjective[0] );
		pObjective[0][0] = '\0';
		if( TitlesMsg )
			_snprintf_s( pObjective[0], sizeof( pObjective[0] ) - 1, TitlesMsg->pMessage );
		else
			_snprintf_s( pObjective[0], sizeof( pObjective[0] ) - 1, def_obj );
	}
	else
		_snprintf_s( pObjective[0], sizeof( pObjective[0] ) - 1, def_obj );

	if( pObjective[1][0] != '\0' )
	{
		// get the secondary objective header from titles.txt
		TitlesMsg = TextMessageGet( pObjective[1] );
		pObjective[1][0] = '\0';
		if( TitlesMsg )
			_snprintf_s( pObjective[1], sizeof( pObjective[1] ) - 1, TitlesMsg->pMessage );
		else
			pObjective[1][0] = '\0'; // secondary objective can be missing, just null it
	}
	else
		pObjective[1][0] = '\0';

	// get dimensions for objective frame, accounting both objectives
	const char *txt;
	obj_height = FRAMEBORDER + FRAMEBORDER; // top and bottom borders
	txt = pObjective[0];
	obj_primarytext_height = 0;
	obj_secondarytext_height = 0;
	int tmp_width = 0;
	int text_width = 0;
	int num_lines = 0;
	unsigned char c;
	while( *txt )
	{
		c = *txt;
		if( c == '\n' )
		{
			obj_primarytext_height += custom_char_height; // got 1 line
			tmp_width = 0;
			num_lines++;
		}
		else
		{
			tmp_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 ); // hack to get actual character width
			if( tmp_width > text_width )
				text_width = tmp_width;
		}
		txt++;
	}

	obj_primarytext_height += gHUD.m_scrinfo.iCharHeight;

	obj_height += obj_primarytext_height;
	
	// add the dimensions of secondary objective text, if it's present
	if( pObjective[1][0] != '\0' )
	{
		// first, add margin between two objectives
		obj_height += FRAMEBORDER;
		txt = pObjective[1];
		tmp_width = 0;
		num_lines = 0;
		while( *txt )
		{
			c = *txt;
			if( c == '\n' )
			{
				obj_secondarytext_height += custom_char_height; // got 1 line
				tmp_width = 0;
				num_lines++;
			}
			else
			{
				tmp_width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 ); // hack to get actual character width
				if( tmp_width > text_width )
					text_width = tmp_width;
			}
			txt++;
		}

		obj_secondarytext_height += gHUD.m_scrinfo.iCharHeight;

		obj_height += obj_secondarytext_height;
	}

	// now make full width (height is done)
	obj_width = FRAMEBORDER + text_width + FRAMEBORDER;
}

int CHudHintObjective::MsgFunc_Hint( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int get = READ_BYTE();

	switch( get )
	{
	case 0:
		_snprintf_s( pHint, 64, READ_STRING() );
		SetupHint();
		break;
	case 1:
		_snprintf_s( pObjective[0], 64, READ_STRING() );
		_snprintf_s( pObjective[1], 64, READ_STRING() );
		SetupObjectives();
		break;
	}

	END_READ();

	return 1;
}

void CHudHintObjective::DrawHintPopUp( void )
{
	if( tr.time == tr.oldtime )
		return;

	if( hint_drawstart_time == 0.0f )
		return;

	if( tr.time > hint_drawstart_time + HINT_DRAW_TIME )
	{
		// fade out
		if( hint_alpha > 0.0f )
		{
			hint_alpha -= fadeout_speed * g_fFrametime;
			if( hint_alpha <= 0.0f )
			{
				hint_alpha = 0.0f;
				hint_drawstart_time = 0.0f;
				return;
			}
		}
		else
			return;
	}
	else // fade in
	{
		if( hint_alpha < 1.0f )
		{
			hint_alpha += fadein_speed * g_fFrametime;
			if( hint_alpha > 1.0f )
				hint_alpha = 1.0f;
		}
	}

	int frame_height = hint_height + FRAMEBORDER + FRAMEBORDER;
	int frame_width = hint_width + FRAMEBORDER + FRAMEBORDER;
	float xpos = (ScreenWidth * 0.5f) - (frame_width * 0.5f);
	float ypos = ScreenHeight - frame_height - 100; // final position we are lerping to

	hint_ypos = lerp( hint_ypos, ypos, g_fFrametime * 10.0f );

	// draw background
	FillRoundedRGBA( xpos, hint_ypos, frame_width, frame_height, 10.0f, Vector4D( 0.1f, 0.1f, 0.1f, hint_alpha * 0.75f ) );

	// draw hint icon
	float img_x = xpos + FRAMEBORDER;
	float img_y = hint_ypos + FRAMEBORDER;
	GL_Bind( 0, HintImage );
	GL_Color4f( 1.0f, 1.0f, 1.0f, hint_alpha );
	GL_Blend( GL_TRUE );
	pglBegin( GL_QUADS );
	DrawQuad( img_x, img_y, img_x + HINT_ICON_SIZE, img_y + HINT_ICON_SIZE );
	pglEnd();

	// draw hint text
	int r, g, b;
	r = 255;
	g = 255;
	b = 255;
	ScaleColors( r, g, b, hint_alpha * 255.0f );
	float tmp_x = xpos + FRAMEBORDER + HINT_ICON_SIZE + FRAMEBORDER;
	float tmp_y = hint_ypos + FRAMEBORDER;
	const char *tmp_text = pHint;
	unsigned char c;
	while( *tmp_text )
	{
		c = *tmp_text;
		if( c == '\n' )
		{
			// next line, reset
			tmp_x = xpos + FRAMEBORDER + HINT_ICON_SIZE + FRAMEBORDER;
			tmp_y += custom_char_height;
		}
		else
		{
			tmp_x += TextMessageDrawChar( tmp_x, tmp_y, c, r, g, b );
		}
		tmp_text++;
	}
}

int CHudHintObjective::Draw( float flTime )
{
	DrawHintPopUp();

	// ---- draw the mission objectives ----

	if( gEngfuncs.GetMaxClients() > 1 )
		return 1; // no objectives in multiplayer, but hints are available

	if( tr.time == tr.oldtime )
		return 1;

	bShowMissionObjectivesTimed = (tr.time < fForcedTimer);
	
	if( !bShowMissionObjectives && !bShowMissionObjectivesTimed && obj_alpha <= 0.0f )
		return 1;

	// draw objectives panel
	if( (bShowMissionObjectives || bShowMissionObjectivesTimed) && obj_alpha < 1.0f )
	{
		// bring the panels' visibility if TAB is pressed
		obj_alpha = lerp( obj_alpha, 1.1f, g_fFrametime * 10.0f );
		if( obj_alpha > 1.0f )
			obj_alpha = 1.0f;
	}
	else if( !bShowMissionObjectives && !bShowMissionObjectivesTimed && obj_alpha > 0.0f )
	{
		// no TAB pressed - fade out
		if( obj_alpha > 0.0f )
		{
			obj_alpha = lerp( obj_alpha, -0.1f, g_fFrametime * 10.0f );
			if( obj_alpha <= 0.0f )
			{
				obj_alpha = 0.0f;
				return 1;
			}
		}
	}

	int frame_height = obj_height;
	int frame_width = obj_width;
	float xpos = (ScreenWidth * 0.5f) - (frame_width * 0.5f);

	// final position we are lerping to
	float ypos;
	if( bShowMissionObjectives || bShowMissionObjectivesTimed )
		ypos = 100;
	else
		ypos = -frame_height;

	obj_ypos = lerp( obj_ypos, ypos, g_fFrametime * 10.0f );

	// draw background
	FillRoundedRGBA( xpos, obj_ypos, frame_width, frame_height, 10.0f, Vector4D( 0.1f, 0.1f, 0.1f, obj_alpha * 0.75f ) );

	// draw objective texts
	int r, g, b;
	float tmp_x, tmp_y;
	const char *tmp_text;
	r = 70;
	g = 169;
	b = 255;
	ScaleColors( r, g, b, obj_alpha * 255.0f );
	tmp_x = xpos + FRAMEBORDER;
	tmp_y = obj_ypos + FRAMEBORDER;
	tmp_text = pObjective[0];
	unsigned char c;
	while( *tmp_text )
	{
		c = *tmp_text;
		if( c == '\n' )
		{
			// next line, reset
			tmp_x = xpos + FRAMEBORDER;
			tmp_y += custom_char_height;
		}
		else
		{
			tmp_x += TextMessageDrawChar( tmp_x, tmp_y, c, r, g, b );
		}
		tmp_text++;
	}

	// secondary objective if present
	if( pObjective[1][0] != '\0' )
	{
		r = 255;
		g = 180;
		b = 140;
		ScaleColors( r, g, b, obj_alpha * 255.0f );
		tmp_x = xpos + FRAMEBORDER;
		tmp_y = obj_ypos + FRAMEBORDER + obj_primarytext_height + FRAMEBORDER;
		tmp_text = pObjective[1];
		unsigned char c;
		while( *tmp_text )
		{
			c = *tmp_text;
			if( c == '\n' )
			{
				// next line, reset
				tmp_x = xpos + FRAMEBORDER;
				tmp_y += custom_char_height;
			}
			else
			{
				tmp_x += TextMessageDrawChar( tmp_x, tmp_y, c, r, g, b );
			}
			tmp_text++;
		}
	}

	// draw timed bar to force show the objective text
	if( bShowMissionObjectivesTimed )
	{
		// it's always 5 seconds
		const float taketh = (fForcedTimer - tr.time) / OBJECTIVE_TIMER;
		// scale the total width by this value
		const float timedbar_width = frame_width * taketh;
		// how much should I move the x coord?
		const float timedbar_xpos = xpos + ((frame_width - timedbar_width) * 0.5f);
		FillRoundedRGBA( timedbar_xpos, obj_ypos + frame_height + 7, timedbar_width, 8.0f, 6.0f, Vector4D( 0.1f, 0.1f, 0.1f, obj_alpha * 0.75f ) );
		if( timedbar_width > 4 )
			FillRoundedRGBA( timedbar_xpos + 2, obj_ypos + frame_height + 9, timedbar_width - 4, 4.0f, 6.0f, Vector4D( 0.275f, 0.666f, 1.0f, obj_alpha * 0.75f ) );
	}

	return 1;
}