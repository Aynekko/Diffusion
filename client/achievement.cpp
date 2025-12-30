
#include "triangleapi.h"
#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "pm_defs.h"
#include "r_local.h"
#include "r_cvars.h"

#include "achievementdata.h"

#include "virtualfs.h"

#define SHOW_ACHIEVEMENT_TIME 7

const float fTOTAL_WIDTH = 666.f; // minimum value
const float fTOTAL_HEIGHT = 200.f;
const float fBORDER = 20.f;
const float fPIC_FRAME_SIZE = fTOTAL_HEIGHT - fBORDER - fBORDER;
const float fBORDER2 = 10.f;
const float fPIC_SIZE = fPIC_FRAME_SIZE - fBORDER2 - fBORDER2;
int text_max_width = 0;

DECLARE_COMMAND( m_StatusIconsAchievement, RefreshAchievementFile );
DECLARE_COMMAND( m_StatusIconsAchievement, ResetAchievementFile );
DECLARE_COMMAND( m_StatusIconsAchievement, ReportAchievementsToConsole );

static bool y_direction = false; // false - up, true - down

int CHudAchievement::Init( void )
{
	HOOK_COMMAND( "ach_refresh", RefreshAchievementFile );
	HOOK_COMMAND( "ach_reset", ResetAchievementFile );
	HOOK_COMMAND( "ach_report", ReportAchievementsToConsole );
	gHUD.AddHudElem( this );
	m_iFlags &= ~HUD_ACTIVE;

	return 1;
}

int CHudAchievement::VidInit( void )
{
	AchStartTime = 0;
	IsAchDrawing = false;
	AchievementCheckTime = tr.time + ACHIEVEMENT_CHECK_TIME;

	// save achievement data on map change, in case of a crash
	SaveAchievementFile();

	return 1;
}

int CHudAchievement::Draw( float flTime )
{
	if( tr.time == tr.oldtime ) // paused
		return 0;

	if( !IsAchDrawing )
		return 0;

	if( gHUD.m_flTime > AchStartTime + SHOW_ACHIEVEMENT_TIME )
		y_direction = true;

	int final_width = fBORDER + fPIC_FRAME_SIZE + fBORDER + text_max_width + fBORDER;
	if( final_width < fTOTAL_WIDTH )
		final_width = fTOTAL_WIDTH;

	// screen center
	x = (ScreenWidth - final_width) * 0.5f;

	if( !y_direction ) // achievement sprite is now showing
	{
		y = lerp( y, ScreenHeight - 250, 7.0f * g_fFrametime );
	}
	else // hiding
	{
		y += 75.0f * g_fFrametime;

		// fully hidden, finish up
		if( y > ScreenHeight )
		{
			IsAchDrawing = false;
			m_iFlags &= ~HUD_ACTIVE;
			return 0;
		}
	}

	// draw main frame
	FillRoundedRGBA( x, y, final_width, fTOTAL_HEIGHT, 20, Vector4D( 0.05f, 0.05f, 0.05f, 0.8f ) );

	// draw square frame RGB 70 169 255
	FillRoundedRGBA( x + fBORDER, y + fBORDER, fPIC_FRAME_SIZE, fPIC_FRAME_SIZE, 10, Vector4D( 0.275f, 0.663f, 1.0f, 0.8f ) );

	// draw the picture in the frame
	GL_Bind( 0, CurrentImage );
	GL_Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( x + fBORDER + fBORDER2, y + fBORDER + fBORDER2, x + fBORDER + fBORDER2 + fPIC_SIZE, y + fBORDER + fBORDER2 + fPIC_SIZE );
	gEngfuncs.pTriAPI->End();

	// draw achievement title and text from titles.txt
	int r, g, b;
	if( pTitle )
	{
		UnpackRGB( r, g, b, gHUD.m_iHUDColor );
		DrawString( x + fBORDER + fPIC_FRAME_SIZE + fBORDER, y + fBORDER, pTitle->pMessage, r, g, b );
	}
	if( pText )
	{
		UnpackRGB( r, g, b, 0x00FFFFFF );
		DrawString( x + fBORDER + fPIC_FRAME_SIZE + fBORDER, y + fBORDER + gHUD.m_scrinfo.iCharHeight + fBORDER, pText->pMessage, r, g, b );
	}

	return 1;
}

void CHudAchievement::EnableAchievement( char *pszIconName )
{
	char Path[128];
	Q_snprintf( Path, sizeof( Path ), "textures/!ach/%s", pszIconName );
	CurrentImage = LOAD_TEXTURE( Path, NULL, 0, 0 );

	if( !CurrentImage )
	{
		ConPrintf( "^1Error:^7 Achievement image \"%s\" couldn't be loaded!\n", pszIconName );
		CurrentImage = tr.defaultTexture;
	}

	pTitle = TextMessageGet( pszIconName ); // titles.txt
	char txt[128];
	Q_snprintf( txt, sizeof( txt ), "%s_text", pszIconName );
	pText = TextMessageGet( txt ); // titles.txt

	// get text width
	// it can be long, so I need to make the frame bigger if needed
	text_max_width = 0;
	if( pText )
	{
		const char *tmp = pText->pMessage;
		int tmpwidth = 0;
		while( *tmp && *tmp != '\n' )
		{
			unsigned char c = *tmp;
			tmpwidth += TextMessageDrawChar( -1, -1, c, 0, 0, 0 );
			tmp++;
		}
		text_max_width = tmpwidth;
	}
	if( pTitle )
	{
		const char *tmp = pTitle->pMessage;
		int tmpwidth = 0;
		while( *tmp && *tmp != '\n' )
		{
			unsigned char c = *tmp;
			tmpwidth += TextMessageDrawChar( -1, -1, c, 0, 0, 0 );
			tmp++;
		}
		if( tmpwidth > text_max_width )
			text_max_width = tmpwidth;
	}

	y = ScreenHeight + 200; // starting position below the bottom of the screen
	IsAchDrawing = true;
	y_direction = false;
	AchStartTime = gHUD.m_flTime;

	PlaySound( "misc/achievement.wav", 1.0 );

	m_iFlags |= HUD_ACTIVE;
}

void CHudAchievement::LoadAchievementFile( void )
{
	bAchievements = true;

	char szFilename[MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), "data/achievements.bin" );

	byte *aMemFile = LOAD_FILE( szFilename, NULL );

	if( !aMemFile )
	{
		Msg( "^2Achievements:^7 Couldn't load achievement file. Creating a default one...\n" );
		CreateDefaultAchievementFile();

		// try again.
		aMemFile = LOAD_FILE( szFilename, NULL );
		if( !aMemFile )
		{
			Msg( "^2Achievements:^7 ^1Error:^7 Couldn't load achievement file due to unknown error. Achievements are disabled.\n" );
			bAchievements = false;
			return;
		}
	}

	achievement_data_t *pData = (achievement_data_t*)aMemFile;
#ifndef _WIN32
	memcpy( &ach_data, pData, sizeof( ach_data ));
#else
	memcpy_s( &ach_data, sizeof( ach_data ), pData, sizeof( ach_data ) );
#endif

	gEngfuncs.COM_FreeFile( aMemFile );

	Msg( "^2Achievements:^7 loaded %s.\n", szFilename );
}

void CHudAchievement::SaveAchievementFile( bool backup )
{
	if( !bAchievements )
		return;

	char szFilename[MAX_PATH];
	if( !backup )
		Q_snprintf( szFilename, sizeof( szFilename ), "data/achievements.bin" );
	else
		Q_snprintf( szFilename, sizeof( szFilename ), "data/achievements_backup.bin" );

	if( !gRenderfuncs.pfnSaveFile( szFilename, &ach_data, sizeof( ach_data ) ) )
		Msg( "^2Achievements:^7 ^1Error:^7 SaveAchievementFile: couldn't save %s\n", szFilename );
	else
		Msg( "^2Achievements:^7 %s saved.\n", szFilename );
}

void CHudAchievement::UserCmd_ReportAchievementsToConsole(void)
{
	if( !bAchievements )
		return;
	
	const char *cYes = "^2YES^7";
	const char *cNo = "^1NO^7";
	Msg( "^2Achievements:^7 currently in memory:\n" );
	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		Msg( "%i Achievement \"%s\", current %i, goal %i, Completed? %s\n", i, ach_data.name[i], ach_data.value[i], ach_data.goal[i], ach_data.completion[i] ? cYes : cNo );
	}
	Msg( "^2Achievements:^7 end of report.\n" );
}

void CHudAchievement::CheckAchievement(void)
{
	if( AchievementCheckTime > tr.time + ACHIEVEMENT_CHECK_TIME )
		AchievementCheckTime = tr.time + ACHIEVEMENT_CHECK_TIME;
	
	if( tr.time < AchievementCheckTime )
		return;

	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		if( (ach_data.value[i] >= ach_data.goal[i]) && !ach_data.completion[i] )
		{
			ach_data.completion[i] = true;
			if( cl_achievement_notify->value )
			{
				EnableAchievement( ach_data.name[i] );
			}

			Msg( "^2Achievements:^7 unlocked \"%s\"!\n", ach_data.name[i] );
			SaveAchievementFile(); // save the file too
			break;
		}
	}

	AchievementCheckTime = tr.time + ACHIEVEMENT_CHECK_TIME;
}

void CHudAchievement::UserCmd_RefreshAchievementFile(void)
{
	SaveAchievementFile();
}

void CHudAchievement::UserCmd_ResetAchievementFile( void )
{
	Msg( "^2Achievements:^7 resetting to default values.\n" );
	SaveAchievementFile( true ); // create backup file with current values
	CreateDefaultAchievementFile();
	LoadAchievementFile();
}

//====================================================================================
// CreateDefaultStatsFile: creates a file with 0 stats, 0 completion, and goals
//====================================================================================
void CHudAchievement::CreateDefaultAchievementFile(void)
{
	// populate achievement defaults
	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		ach_data.goal[i] = AchievementGoals[i];
		ach_data.completion[i] = false;
		ach_data.value[i] = 0;
		Q_strncpy( ach_data.name[i], AchievementNames[i], sizeof( ach_data.name[i] ));
	}

	SaveAchievementFile();
}
