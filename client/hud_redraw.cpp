/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "r_quakeparticle.h"
#include "event_api.h"

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};

//===================================================================
// diffusion - handle the "offline" HUD thingy
//===================================================================
#define SUIT_OFFLINE_TIME 60 // do not spam offline from shock damage
static void SuitIsOffline(void)
{	
	if( gHUD.m_Health.m_bitsDamage & 256 ) // got DMG_SHOCK
	{
		if( !gHUD.IsDrawingOfflineHUD && (gHUD.m_flTime > gHUD.NextDrawingOfflineHUDTime) ) // make sure it's not already set
		{
			gHUD.IsDrawingOfflineHUD = true;
			gHUD.NextDrawingOfflineHUDTime = gHUD.m_flTime + SUIT_OFFLINE_TIME + 5;
		}
	}

	if( gHUD.IsDrawingOfflineHUD )
	{
		// possible changelevel issues? try this
		if( gHUD.NextDrawingOfflineHUDTime > gHUD.m_flTime + SUIT_OFFLINE_TIME + 5 )
			gHUD.NextDrawingOfflineHUDTime = gHUD.m_flTime + SUIT_OFFLINE_TIME + 5;

		if( gHUD.m_flTime > gHUD.NextDrawingOfflineHUDTime - SUIT_OFFLINE_TIME - 3 )
			gHUD.IsDrawingOfflineHUD = false;
	}
}

const char *DrumSamples[]
{
	"drums/kick.wav",
	"drums/snare.wav",
	"drums/tomlow.wav",
	"drums/tommed.wav",
	"drums/tomhigh.wav",
	"drums/ride.wav",
	"drums/hihat.wav",
	"drums/crash.wav",
};

void CHud::DrumsInput( int Slot )
{
	const char *sample = NULL;

	switch( Slot )
	{
	case 4: sample = DrumSamples[0]; break;
	case 3: sample = DrumSamples[1]; break;
	case 2: sample = DrumSamples[2]; break;
	case 1: sample = DrumSamples[3]; break;
	case 0: sample = DrumSamples[4]; break;
	// cymbals
	case 5: sample = DrumSamples[5]; break;
	case 6: sample = DrumSamples[6]; break;
	case 7: sample = DrumSamples[7]; break;
	}

	if( sample != NULL )
		gEngfuncs.pEventAPI->EV_PlaySound( gEngfuncs.GetLocalPlayer()->index, gEngfuncs.GetLocalPlayer()->origin, CHAN_STATIC, sample, 1.0, 1.0, 0, 100 );
}

void CHud::Think( void )
{
	// diffusion - hud_fontscale quick change support
	static float cachescale = 0.0f;
	if( cachescale != fScale )
	{
		CVAR_SET_FLOAT( "hud_fontscale", fScale );
		cachescale = fScale;
	}
	m_scrinfo.iSize = sizeof( m_scrinfo );
	GetScreenInfo( &m_scrinfo );

	// think about default fov
	static float cachefov = -1.0f;
	if( cachefov == -1.0f )
		cachefov = default_fov->value;

	if( cachefov != default_fov->value )
	{
		cl_entity_t *viewent = GET_ENTITY( tr.viewparams.viewentity );
		if( viewent && viewent->player )
		{
			m_flFOV = 0.0f;
			cachefov = default_fov->value;
		}
	}

	if( m_flFOV == 0.0f )
	{
		// only let players adjust up in fov, and only if they are not overriden by something else
		m_flFOV = default_fov->value;
	}
	
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p->m_iFlags & HUD_ACTIVE )
			pList->p->Think();
		pList = pList->pNext;
	}

	SuitIsOffline();

	if( m_StatusIconsAchievement.bAchievements )
		m_StatusIconsAchievement.CheckAchievement();

	// make breathing effect
	static float lastbreathtime = 0;
	if( lastbreathtime > tr.time )
		lastbreathtime = tr.time;
	if( gHUD.BreathingEffect && (tr.time != tr.oldtime) && (tr.time > lastbreathtime + 3) )
	{
		Vector PlayerAng = tr.viewparams.viewangles;
		Vector forw;
		gEngfuncs.pfnAngleVectors( PlayerAng, forw, NULL, NULL );
		Vector BreathOrg = gEngfuncs.GetLocalPlayer()->origin + Vector(0,0,20) + forw * 20;

		CQuakePart breath = InitializeParticle();
		breath.m_vecOrigin = BreathOrg;
		breath.m_vecVelocity = gEngfuncs.GetLocalPlayer()->curstate.basevelocity * 0.75 + gEngfuncs.GetLocalPlayer()->curstate.velocity * 0.75 + Vector(0,0,5);
		breath.m_flAlpha = 0.25;
		breath.m_flAlphaVelocity = -0.25;
		breath.m_flRadius = 25;
		breath.m_flRadiusVelocity = 0.1;
		breath.m_flRotation = RANDOM_LONG( 0, 360 );
		breath.m_flRotationVelocity = RANDOM_LONG(-50,50);
		breath.ParticleType = TYPE_SMOKE;
		g_pParticles.AddParticle( &breath, g_pParticles.m_hSmoke, FPART_NOTWATER );

		lastbreathtime = tr.time;
	}

	// in multiplayer, change default names (check every 5 seconds)
	if( tr.viewparams.maxclients > 1 )
	{
		static float next_name_check = 0;
		if( next_name_check > tr.time + 5 )
			next_name_check = 0;
		if( tr.time >= next_name_check )
		{
			char *name = (char *)CVAR_GET_STRING( "name" );
			if( !Q_strnicmp( name, "player", 6 ) || !Q_strnicmp( name, "unnamed", 7 ) )
			{
				char NewName[15] = "";
				char Cmd[20] = "";
				Q_sprintf( NewName, "Newbie N%i", RANDOM_LONG( 0, 9999 ) );
				Q_sprintf( Cmd, "name \"%s\"\n", NewName );
				ClientCmd( Cmd );
			}
			next_name_check = tr.time + 5;
		}
	}
}

int CHud :: Redraw( float flTime, int intermission )
{
	GL_AlphaTest( GL_FALSE );
	
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	
	// Clock was reset, reset delta
	if( m_flTimeDelta < 0 ) m_flTimeDelta = 0;

	m_iIntermission = intermission;

	if( m_pCvarDraw->value )
	{
		HUDLIST *pList = m_pHudList;

		while( pList )
		{
			if( !intermission )
			{
				if(( pList->p->m_iFlags & HUD_ACTIVE ) && !( m_iHideHUDDisplay & HIDEHUD_ALL ) )
					pList->p->Draw( flTime );
			}
			else
			{
				// it's an intermission, so only draw hud elements that are set
				// to draw during intermissions
				if( pList->p->m_iFlags & HUD_INTERMISSION )
					pList->p->Draw( flTime );
			}

			pList = pList->pNext;
		}
	}

	// diffusion: HUD is hidden, but show anyway( i.e. text messages )
	if( m_iHideHUDDisplay & HIDEHUD_ALL )
	{
		m_ScreenEffects.Draw( flTime );
		m_StatusIconsAchievement.Draw( flTime );
		m_Message.Draw( flTime );
		m_Subtitle.Draw( flTime );
		m_HintObjectives.Draw( flTime );
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if( m_iLogo )
	{
		int x, y, i;

		if( m_hsprLogo == 0 )
			m_hsprLogo = LoadSprite( "sprites/%d_logo.spr" );

		SPR_Set( m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width( m_hsprLogo, 0 );
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0) / 2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive( i, x, y, NULL );
	}

 	return 1;
}

int CHud :: DrawHudString( int xpos, int ypos, int iMaxX, char *szString, int r, int g, int b )
{
	// draw the string until we hit the null character or a newline character
	for( byte *szIt = (byte *)szString; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		int next = xpos + gHUD.m_scrinfo.charWidths[*szIt]; // variable-width fonts look cool
		if( next > iMaxX )
			return xpos;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
		xpos = next;		
	}
	return xpos;
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b, bool forward )
{
	char szString[32];

	Q_snprintf( szString, sizeof( szString ), "%d", iNumber );

	if( forward )
		return DrawString( xpos, ypos, szString, r, g, b );

	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	// find the end of the string
	byte *szIt;
	for( szIt = (byte *)szString; *szIt != 0; szIt++ )
	{
		// we should count the length?		
	}

	// iterate throug the string in reverse
	for( szIt--; szIt != (byte *)(szString - 1); szIt-- )	
	{
		int next = xpos - gHUD.m_scrinfo.charWidths[*szIt]; // variable-width fonts look cool
		if( next < iMinX )
			return xpos;
		xpos = next;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
	}
	return xpos;
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b )
{
	int iWidth = GetSpriteRect( m_HUD_number_0 ).right - GetSpriteRect( m_HUD_number_0 ).left;
	int k;
	
	if( iNumber > 0 )
	{
		// SPR_Draw 100's
		if( iNumber >= 100 )
		{
			k = iNumber/100;
			SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( m_HUD_number_0 + k ));
			x += iWidth;
		}
		else if( iFlags & ( DHN_3DIGITS ))
		{
			x += iWidth;
		}

		// SPR_Draw 10's
		if( iNumber >= 10 )
		{
			k = (iNumber % 100) / 10;
			SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( m_HUD_number_0 + k ));
			x += iWidth;
		}
		else if( iFlags & ( DHN_3DIGITS | DHN_2DIGITS ))
		{
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect( m_HUD_number_0 + k ));
		x += iWidth;
	} 
	else if( iFlags & DHN_DRAWZERO ) 
	{
		SPR_Set( GetSprite( m_HUD_number_0 ), r, g, b );

		// SPR_Draw 100's
		if( iFlags & ( DHN_3DIGITS ))
		{
			x += iWidth;
		}

		if( iFlags & ( DHN_3DIGITS|DHN_2DIGITS ))
			x += iWidth;

		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect( m_HUD_number_0 ));
		x += iWidth;
	}
	return x;
}

int CHud :: GetNumWidth( int iNumber, int iFlags )
{
	if( iFlags & ( DHN_3DIGITS ))
		return 3;

	if( iFlags & ( DHN_2DIGITS ))
		return 2;

	if( iNumber <= 0 )
	{
		if( iFlags & ( DHN_DRAWZERO ))
			return 1;
		return 0;
	}

	if( iNumber < 10 )
		return 1;

	if( iNumber < 100 )
		return 2;

	return 3;
}