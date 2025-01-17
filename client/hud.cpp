//=======================================================================
//			Copyright (C) XashXT Group 2007
//=======================================================================

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"

bool g_bDuckToggled; // diffusion - http://articles.thewavelength.net/378

#define SCALE_2K 1.5f
#define SCALE_4K 1.8f

void CHud :: Init( void )
{
	InitHUDMessages();

	// NOTENOTE - ordering here directly affects the rendering order: last will be on top
	m_CrosshairStatic.Init();
	m_Ammo.Init();
	m_Health.Init();
	m_Geiger.Init();
	m_Train.Init();
//	m_Battery.Init(); // diffusion - no battery in game
	m_Flash.Init();
	m_Scoreboard.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_StatusIconsTutor.Init(); // diffusion
	m_StatusIconsAchievement.Init();
 	m_SayText.Init();
	m_Menu.Init();
	m_MOTD.Init();
	m_Stamina.Init();  //DiffusionSprint
	m_HealthVisual.Init(); // diffusion health visual
	m_BlastIcons.Init();
	m_DroneBars.Init();
	m_UseIcon.Init();
	m_Zoom.Init();
	m_Healthbars.Init();
	m_CodeInput.Init();
	m_ScreenEffects.Init();
	m_PseudoGUI.Init();
	m_TriggerTimer.Init();
	m_Puzzle.Init();
	m_Message.Init(); // hud text messages (like subtitles) go next to last - always visible
	m_Subtitle.Init();
	m_Cursor.Init(); // cursor is last
		
//	MsgFunc_ResetHUD( 0, 0, NULL ); // diffusion - now server does this

	g_bDuckToggled = false;

	m_szServerName[0] = '\0';
}

CHud :: ~CHud()
{
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

	if( m_pHudList )
	{
		HUDLIST *pList;
		while( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}
}

//========================================================================
// keeps some HUD elements fixed size - no need to use hud_scale because
// it scales the whole HUD
// don't forget to reset viewport after using this!
// thx to Yanny for this function :)
//========================================================================
#define GetHeightByWidth(width) (int)((width) * (ScreenHeight / (float)ScreenWidth))
#define ScaleWidth(x) (int)(ScreenWidth * (ScreenWidth / (float)x))
void CHud::GL_HUD_StartConstantSize( bool aligned_right )
{
	if( fScale == 1.0f )
		return;

	int width = 1920; // for 2K and 4K

	const int new_scale = ScaleWidth( width );

	if( !aligned_right )
	{
		pglViewport(
			0,
			(ScreenHeight - GetHeightByWidth( new_scale )),
			new_scale, GetHeightByWidth( new_scale )
		);
	}
	else
	{
		pglViewport(
			(ScreenWidth - new_scale),
			0,
			new_scale, GetHeightByWidth( new_scale )
		);
	}
}

void CHud::GL_HUD_EndConstantSize( void )
{
	if( fScale == 1.0f )
		return;

	pglViewport( 0, 0, ScreenWidth, ScreenHeight );
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns -1 if sprite not found
int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for( int i = 0; i < m_iSpriteCount; i++ )
	{
		if( !Q_strncmp( SpriteName, m_rgszSpriteNames + ( i * MAX_SPRITE_NAME_LENGTH ), MAX_SPRITE_NAME_LENGTH ))
			return i;
	}
	return -1; // invalid sprite
}

void CHud::SetupScale( void )
{
	fScale = 1.0f;
	fCenteredPadding = 0.0f;

	if( cl_largehud->value == 1 )
	{
		fScale = SCALE_2K;
		if( ScreenWidth > 2560 && ScreenHeight > 1440 ) // even larger for above 2K
			fScale = SCALE_4K;
	}
	else if( cl_largehud->value > 0 ) // automatic
	{
		// use larger size if screen is bigger than FullHD
		if( ScreenWidth > 1920 && ScreenHeight > 1080 )
			fScale = SCALE_2K;
		else if( ScreenWidth > 2560 && ScreenHeight > 1440 ) // even larger for above 2K
			fScale = SCALE_4K;
	}

	// this will probably help for ultrawide monitors, I guess (applies to health, ammo, and maybe something else)
	if( cl_centerhud->value > 0 )
	{
		float wideness = (float)ScreenWidth / (float)ScreenHeight;
		fCenteredPadding = ((float)ScreenWidth * 0.1f) * wideness;
		if( fCenteredPadding > (float)ScreenWidth * 0.2f )
			fCenteredPadding = (float)ScreenWidth * 0.2f;
	}
}

void CHud :: VidInit( void )
{
	m_scrinfo.iSize = sizeof( m_scrinfo );
	GetScreenInfo( &m_scrinfo );

	// ----------
	// Load Sprites
	// ---------

	m_hHudError = 0;
	
	if( ScreenWidth < 640 )
		m_iRes = 320;
	else m_iRes = 640;

	SetupScale();
	
	// Only load this once
	if( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList( "sprites/hud.txt", &m_iSpriteCountAllRes );

		if( m_pSpriteList )
		{
			// count the number of sprites of the appropriate res
			client_sprite_t *p = m_pSpriteList;
			m_iSpriteCount = 0;

			int j;
			for( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
 			m_rghSprites = new SpriteHandle[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;

			for( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if( p->iRes == m_iRes )
				{
					char sz[256];
					Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
					m_rghSprites[index] = SPR_Load( sz );
					m_rgrcRects[index] = p->rc;
					Q_strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH - 1 );
					m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH + (MAX_SPRITE_NAME_LENGTH - 1)] = '\0';

					index++;
				}
				p++;
			}
		}
		else
		{
			ALERT( at_warning, "hud.txt couldn't load\n");
			m_pCvarDraw->value = 0;
			return;
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;

		for( int j = 0, index = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if( p->iRes == m_iRes )
			{
				char sz[256];
				Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load( sz );
				index++;
			}
			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;
	
	// loading error sprite
	m_HUD_error = GetSpriteIndex( "error" );
	m_hHudError = GetSprite( m_HUD_error );
	
	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
//	m_Battery.VidInit(); // diffusion - no battery in game
	m_Flash.VidInit();
	m_MOTD.VidInit();
	m_Scoreboard.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
	m_StatusIconsTutor.VidInit();
	m_StatusIconsAchievement.VidInit();
	m_Stamina.VidInit();   //DiffusionSprint
	m_HealthVisual.VidInit(); // diffusion health visual
	m_DroneBars.VidInit();
	m_UseIcon.VidInit();  // use icons
	m_BlastIcons.VidInit(); // electro-blast icons
	m_Healthbars.VidInit();
	m_CrosshairStatic.VidInit(); // new simple crosshairs
	m_CodeInput.VidInit();
	m_ScreenEffects.VidInit();
	m_PseudoGUI.VidInit();
	m_TriggerTimer.VidInit();
	m_Puzzle.VidInit();
	m_Message.VidInit();
	m_Subtitle.VidInit();
	m_Cursor.VidInit();

	memset( &gHUD.shake, 0.0f, sizeof( gHUD.shake ) ); // diffusion - reset screen shake

	gHUD.GlitchAmount = 0.0f;
	gHUD.GlitchHoldTime = 0.0f;
	gHUD.LensFlareAlpha = 0.0f;

	memset( emptyclipspawned, 0, sizeof( emptyclipspawned ) );
}

void CHud::AddHudElem( CHudBase *phudelem )
{
	HUDLIST *pdl, *ptemp;

	if( !phudelem ) return;

	pdl = (HUDLIST *)malloc( sizeof( HUDLIST ));
	if( !pdl ) return;

	memset( pdl, 0, sizeof( HUDLIST ));
	pdl->p = phudelem;

	if( !m_pHudList )
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while( ptemp->pNext )
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}