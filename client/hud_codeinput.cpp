#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "triangleapi.h"
#include "r_shader.h"
#include "event_api.h"

//===========================================================================================
// diffusion - draw code input screen
//===========================================================================================

int ImageWidth = 0;
int ImageHeight = 0;

DECLARE_MESSAGE( m_CodeInput, CodeInput );

int CHudCodeInput::Init( void )
{
	HOOK_MESSAGE( CodeInput );
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudCodeInput::VidInit(void)
{
	DisableCodeScreen();
	CodeInputSpr.Init( "sprites/diffusion/code_input/code_input" );
	if( CodeInputSpr.Initialized() )
	{
		ImageWidth = RENDER_GET_PARM( PARM_TEX_WIDTH, CodeInputSpr.Texture[0] );
		ImageHeight = RENDER_GET_PARM( PARM_TEX_HEIGHT, CodeInputSpr.Texture[0] );
	}
	return 1;
}

void CHudCodeInput::DisableCodeScreen(void)
{
	// reset everything on a new map or saverestore
	CodeInputScreenIsOn = false;
	InputStep = 0;
	memset( num, 0, sizeof( num ) );
	memset( num_user, 0, sizeof( num_user ) );
	CloseTime = 0.0f;
	CodeSuccess = false;
}

void CHudCodeInput::EnterCode( int num )
{
	if( InputStep == 5 )
		return;
	
	if( num == 10 )
		num = 0; // :)

	if( InputStep < 4 )
		num_user[InputStep] = num;

	InputStep++;

	gEngfuncs.pEventAPI->EV_PlaySound( gEngfuncs.GetLocalPlayer()->index, gEngfuncs.GetLocalPlayer()->origin, CHAN_ITEM, "misc/code_push.wav", 1.0, 0, 0, PITCH_NORM );
}

int CHudCodeInput::MsgFunc_CodeInput( const char *pszName, int iSize, void *pbuf )
{
	// when a code is received, it means we start to render the code screen
	// and awaiting the user inputs
	BEGIN_READ( pszName, pbuf, iSize );
	entindex = READ_SHORT();
	num[0] = READ_BYTE();
	num[1] = READ_BYTE();
	num[2] = READ_BYTE();
	num[3] = READ_BYTE();
	END_READ();

	InputOrigin = tr.viewparams.vieworg;

	CodeInputScreenIsOn = true;
	InputStep = 0; // user didn't enter anything yet
	CodeSuccess = false;
	r = 255;
	g = 255;
	b = 255;
	memset( num_user, 0, sizeof( num_user ) );

	return 1;
}

int CHudCodeInput::Draw( float flTime )
{
	// player's view isn't near the code object
	float DistanceFromInputStart = (tr.viewparams.vieworg - InputOrigin).Length();
	if( DistanceFromInputStart > 75 )
	{
		DisableCodeScreen();
	}
	
	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( InputStep == 4 ) // time to check the code
	{
		CodeSuccess = true;
		for( int i = 0; i < 4; i++ )
		{
			if( num[i] != num_user[i] )
			{
				CodeSuccess = false;
				break;
			}
		}

		if( CodeSuccess )
		{
		//	ConPrintf( "CODE SUCCESS!\n" );
			r = 25;
			g = 255;
			b = 25;
			char szbuf[32];
			sprintf_s( szbuf, "code %d %d%d%d%d\n", entindex, num[0], num[1], num[2], num[3] );
			ClientCmd( szbuf );
		}
		else
		{
		//	ConPrintf( "CODE FAIL!\n" );
			r = 255;
			g = 25;
			b = 25;
		}

		InputStep = 5;
		CloseTime = tr.time + 1;
	}

	if( !CodeInputScreenIsOn )
		return 1;

	int x = (ScreenWidth / 2) - (ImageWidth / 2);
	int y = (ScreenHeight / 2) - (ImageHeight / 2);
	int xmax = (ScreenWidth / 2) + (ImageWidth / 2);
	int ymax = (ScreenHeight / 2) + (ImageHeight / 2);

	// slightly blacken the screen
	FillRoundedRGBA( x - 50, y - 50, ImageWidth + 100, ImageHeight + 100, 50, Vector4D( 0.0f, 0.0f, 0.0f, (150 - (DistanceFromInputStart * 2.0f)) / 255.0f ) );
	
	// draw user inputs
	if( CodeInputSpr.Initialized() )
	{
		CodeInputSpr.SetRenderMode( kRenderTransAdd );
		CodeInputSpr.SetColor( r, g, b );
		CodeInputSpr.SetPos( x, y, xmax, ymax );
		if( CodeSuccess )
			CodeInputSpr.DrawFrame( 6 );
		else
			CodeInputSpr.DrawFrame( InputStep );
	}

	if( InputStep == 5 )
	{
		if( CloseTime > 0.1f && tr.time > CloseTime )
		{
			if( CodeSuccess )
			{
				DisableCodeScreen();
			}
			else
			{
				InputStep = 0;
				CloseTime = 0.0f;
				r = 255;
				g = 255;
				b = 255;
			}
		}
	}

	const char *buf;
	char text[64];
	if( num[0] == 255 ) // unknown code
		_snprintf_s( text, sizeof( text ), "CODE: < unknown >" );
	else
		_snprintf_s( text, sizeof( text ), "CODE: %d %d %d %d", num[0], num[1], num[2], num[3] );

	// calculate width to align center...
	buf = text;
	int width = 0;
	while( *buf )
	{
		unsigned char c = *buf;
		width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 );
		buf++;
	}

	DrawString( (int)((ScreenWidth - width) * 0.5f), (int)(ScreenHeight * 0.8), text, 255, 255, 255 );

	return 1;
}