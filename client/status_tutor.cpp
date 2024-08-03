#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_cvars.h"
#include "r_local.h"
#include "triangleapi.h"

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
	x_direction = false;
	CurrentImage = 0;
	return 1;
}

int CHudTutorial::MsgFunc_StatusIconTutor( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	char *pszIconName = READ_STRING();
	END_READ();

	EnableTutorial( pszIconName );

	return 1;
}

void CHudTutorial::EnableTutorial( char *pszIconName )
{
	if( !cl_tutor->value )
		return;

	char Path[128];
	sprintf_s( Path, "sprites/tutor/%s", pszIconName );
	CurrentImage = LOAD_TEXTURE( Path, NULL, 0, 0 );

	if( !CurrentImage )
	{
		ConPrintf( "^1Error:^7 Tutorial image \"%s\" couldn't be loaded!\n", pszIconName );
		return;
	}

	TutorStartTime = gHUD.m_flTime;
	IsTutorDrawing = true;
	x_direction = false;
	x = -600; // HACKHACK starting position

	PlaySound( "misc/tutor.wav", 1.0 );

	m_iFlags |= HUD_ACTIVE;
}

int CHudTutorial::Draw( float flTime )
{
	if( gHUD.m_flTimeDelta == 0 ) // paused
		return 0;

	if( !IsTutorDrawing )
		return 0;

	if( !CurrentImage )
		return 0;

	// set the coordinates
	y = ScreenHeight * 0.15;

	if( !x_direction ) // tutor is now showing
	{
		if( x < 10 ) x += 600 * g_fFrametime;

		if( x > 10 ) x = 10;
	}
	else // tutor is hiding
	{
		x -= 200 * g_fFrametime;

		// fully hidden, finish up
		if( x < -600 )
		{
			IsTutorDrawing = false;
			m_iFlags &= ~HUD_ACTIVE;
			TutorStartTime = 0;
			return 0;
		}
	}

	int xmax = RENDER_GET_PARM( PARM_TEX_WIDTH, CurrentImage );
	int ymax = RENDER_GET_PARM( PARM_TEX_HEIGHT, CurrentImage );
	GL_Bind( 0, CurrentImage );
	GL_Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_Blend( GL_FALSE );
	pglBegin( GL_QUADS );
	DrawQuad( x, y, x + xmax, y + ymax );
	pglEnd();
	
	return 1;
}
