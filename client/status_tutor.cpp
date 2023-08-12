#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_cvars.h"

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
//	if( IsTutorDrawing ) // don't override currently showing tutor
//		return;

	if( !cl_tutor->value )
		return;

	// the sprite must be listed in hud.txt
	int spr_index;
	spr_index = gHUD.GetSpriteIndex( pszIconName );
	m_TutorSpr.spr = gHUD.GetSprite( spr_index );
	if( !m_TutorSpr.spr )
	{
		spr_index = gHUD.GetSpriteIndex( "error" );
		m_TutorSpr.spr = gHUD.GetSprite( spr_index );
	}
	m_TutorSpr.rc = gHUD.GetSpriteRect( spr_index );
	m_TutorSpr.r = m_TutorSpr.g = m_TutorSpr.b = 255;
	Q_strcpy( m_TutorSpr.szSpriteName, pszIconName );

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

//	if( gHUD.m_flTime > TutorStartTime + 1 )
//	{
//		if( gHUD.m_iKeyBits & IN_USE )
//			x_direction = true; // let's hide the tutor
//	}

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

	if( m_TutorSpr.spr )
	{
		SPR_Set( m_TutorSpr.spr, m_TutorSpr.r, m_TutorSpr.g, m_TutorSpr.b );
		SPR_Draw( 0, x, y, &m_TutorSpr.rc );
	}
	
	return 1;
}
