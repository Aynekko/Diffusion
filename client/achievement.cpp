
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
	
	return 1;
}

int CHudAchievement::Draw( float flTime )
{
	if( tr.time == tr.oldtime ) // paused
		return 0;

	if( !IsAchDrawing )
		return 0;

	if( !CurrentImage )
		return 0;

	if( gHUD.m_flTime > AchStartTime + SHOW_ACHIEVEMENT_TIME )
		y_direction = true;

	int xmax = RENDER_GET_PARM( PARM_TEX_WIDTH, CurrentImage );
	int ymax = RENDER_GET_PARM( PARM_TEX_HEIGHT, CurrentImage );

	// screen center
	x = (ScreenWidth - xmax) / 2;

	if( !y_direction ) // achievement sprite is now showing
	{
		if( y > ScreenHeight - 250 ) y -= 700 * g_fFrametime;

		if( y < ScreenHeight - 250 ) y = ScreenHeight - 250;
	}
	else // hiding
	{
		y += 50 * g_fFrametime;

		// fully hidden, finish up
		if( y > ScreenHeight )
		{
			IsAchDrawing = false;
			m_iFlags &= ~HUD_ACTIVE;
			return 0;
		}
	}

	GL_Bind( 0, CurrentImage );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( x, y, x + xmax, y + ymax );
	gEngfuncs.pTriAPI->End();

	return 1;
}

void CHudAchievement::EnableAchievement( char *pszIconName )
{
	char Path[128];
	sprintf_s( Path, "sprites/ach/%s", pszIconName );
	CurrentImage = LOAD_TEXTURE( Path, NULL, 0, 0 );

	if( !CurrentImage )
	{
		ConPrintf( "^1Error:^7 Achievement image \"%s\" couldn't be loaded!\n", pszIconName );
		return;
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
	_snprintf_s( szFilename, sizeof( szFilename ), "data/achievements.bin" );

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
	memcpy_s( &ach_data, sizeof( ach_data ), pData, sizeof( ach_data ) );

	gEngfuncs.COM_FreeFile( aMemFile );

	Msg( "^2Achievements:^7 loaded %s.\n", szFilename );
}

void CHudAchievement::SaveAchievementFile(void)
{
	if( !bAchievements )
		return;

	char szFilename[MAX_PATH];
	_snprintf_s( szFilename, sizeof( szFilename ), "data/achievements.bin" );

	if( !gRenderfuncs.pfnSaveFile( szFilename, &ach_data, sizeof( ach_data ) ) )
		Msg( "^2Achievements:^7 ^1Error:^7 SaveAchievementFile: couldn't save %s\n", szFilename );
	else
		Msg( "^2Achievements:^7 %s saved.\n", szFilename );
}

void CHudAchievement::UserCmd_ReportAchievementsToConsole(void)
{
	if( !bAchievements )
		return;
	
	char Completed[7];
	Msg( "^2Achievements:^7 currently in memory:\n" );
	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		if( ach_data.completion[i] )
			sprintf_s( Completed, "^2YES^7" );
		else
			sprintf_s( Completed, "^1NO^7" );
		Msg( "%i Achievement \"%s\", current %i, goal %i, Completed? %s\n", i, ach_data.name[i], ach_data.value[i], ach_data.goal[i], Completed );
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
		sprintf_s( ach_data.name[i], AchievementNames[i] );
	}

	SaveAchievementFile();
}