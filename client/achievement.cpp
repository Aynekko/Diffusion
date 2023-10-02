
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

DECLARE_MESSAGE( m_StatusIconsAchievement, StatusIconAchievement );
DECLARE_COMMAND( m_StatusIconsAchievement, RefreshAchievementFile );
DECLARE_COMMAND( m_StatusIconsAchievement, ResetAchievementFile );

static bool y_direction = false; // false - up, true - down

int CHudAchievement::Init( void )
{
	HOOK_MESSAGE( StatusIconAchievement );
	HOOK_COMMAND( "ach_refresh", RefreshAchievementFile );
	HOOK_COMMAND( "ach_reset", ResetAchievementFile );
	gHUD.AddHudElem( this );
	m_iFlags &= ~HUD_ACTIVE;

	sprintf_s( AchievementName[ACH_BULLETSFIRED], "ach_firebullets" );
	sprintf_s( AchievementName[ACH_JUMPS], "ach_jump" );
	sprintf_s( AchievementName[ACH_AMMOCRATES], "ach_ammocrates" );
	sprintf_s( AchievementName[ACH_DISARMEDMINES], "ach_disarm" );
	sprintf_s( AchievementName[ACH_KILLENEMIES], "ach_killenemies" );
	sprintf_s( AchievementName[ACH_INFLICTDAMAGE], "ach_inflictdamage" );
	sprintf_s( AchievementName[ACH_KILLENEMIESSNIPER], "ach_killenemiessniper" );
	sprintf_s( AchievementName[ACH_CH1], "ach_chapter1" );
	sprintf_s( AchievementName[ACH_CH2], "ach_chapter2" );
	sprintf_s( AchievementName[ACH_CH3], "ach_chapter3" );
	sprintf_s( AchievementName[ACH_CH4], "ach_chapter4" );
	sprintf_s( AchievementName[ACH_CH5], "ach_chapter5" );
	sprintf_s( AchievementName[ACH_GENERAL30SEC], "ach_general30sec" );
	sprintf_s( AchievementName[ACH_HPREGENERATE], "ach_hpregenerate" );
	sprintf_s( AchievementName[ACH_RECEIVEDAMAGE], "ach_receivedamage" );
	sprintf_s( AchievementName[ACH_OVERCOOK], "ach_overcook" );
	sprintf_s( AchievementName[ACH_DRONESEC], "ach_dronesec" );
	sprintf_s( AchievementName[ACH_DRONEALIEN], "ach_dronealien" );
	sprintf_s( AchievementName[ACH_CROSSBOW], "ach_crossbow" );
	sprintf_s( AchievementName[ACH_TANKBALL], "ach_tankball" );
	sprintf_s( AchievementName[ACH_DASH], "ach_dash" );
	sprintf_s( AchievementName[ACH_NOTES], "ach_notes" );
	sprintf_s( AchievementName[ACH_SECRETS], "ach_secrets" );
	sprintf_s( AchievementName[ACH_KILLENEMIESBALLS], "ach_killenemiesballs" );
	sprintf_s( AchievementName[ACH_REDDWELLER], "ach_reddweller" );
	sprintf_s( AchievementName[ACH_ASSEMBLEBLASTLEVEL], "ach_assembleblastlevel" );
	sprintf_s( AchievementName[ACH_BROKENCAR], "ach_brokencar" );
	sprintf_s( AchievementName[ACH_CARDISTANCE], "ach_cardistance" );
	sprintf_s( AchievementName[ACH_WATERJETDISTANCE], "ach_waterjetdistance" );
	sprintf_s( AchievementName[ACH_KILLBOTS], "ach_killbots" );
	sprintf_s( AchievementName[ACH_CH3_NOKILLDW], "ach_dwellerch3" );
	sprintf_s( AchievementName[ACH_CH3_3MINS], "ach_ch3_3mins" );

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
	if( gHUD.m_flTimeDelta == 0 ) // paused
		return 0;

	if( !IsAchDrawing )
		return 0;

	if( gHUD.m_flTime > AchStartTime + SHOW_ACHIEVEMENT_TIME )
		y_direction = true;

	// screen center
	x = (ScreenWidth / 2) - ((m_AchievementSpr.rc.right - m_AchievementSpr.rc.left) / 2);

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

	if( m_AchievementSpr.spr )
	{
		SPR_Set( m_AchievementSpr.spr, m_AchievementSpr.r, m_AchievementSpr.g, m_AchievementSpr.b );	
		SPR_Draw( 0, x, y, &m_AchievementSpr.rc );
	}

	return 1;
}

int CHudAchievement::MsgFunc_StatusIconAchievement( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	char *pszIconName = READ_STRING();
	END_READ();

	EnableAchievement( pszIconName );

	return 1;
}

void CHudAchievement::EnableAchievement( char *pszIconName )
{
	// the sprite must be listed in hud.txt
	int spr_index = gHUD.GetSpriteIndex( pszIconName );
	m_AchievementSpr.spr = gHUD.GetSprite( spr_index );
	if( !m_AchievementSpr.spr )
	{
		spr_index = gHUD.GetSpriteIndex( "error" );
		m_AchievementSpr.spr = gHUD.GetSprite( spr_index );
	}
	m_AchievementSpr.rc = gHUD.GetSpriteRect( spr_index );
	m_AchievementSpr.r = m_AchievementSpr.g = m_AchievementSpr.b = 255;
	Q_strcpy( m_AchievementSpr.szSpriteName, pszIconName );

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

	int i;
	char *pfile;
	char token[1024];
	char *afile = (char *)gEngfuncs.COM_LoadFile( "data/achievements.txt", 5, NULL );
	if( !afile )
	{
		gEngfuncs.Con_Printf( "^3Warning:^7 Couldn't load achievement file. Creating a default one...\n" );

		CreateDefaultAchievementFile();

		// try again.
		afile = (char *)gEngfuncs.COM_LoadFile( "data/achievements.txt", 5, NULL );
		if( !afile )
		{
			gEngfuncs.Con_Printf( "^3Warning:^7 Couldn't load achievement file due to unknown error. Achievements are disabled.\n" );
			bAchievements = false;
			return;
		}
	}

	// file is found, parse it
	pfile = afile;
	for( i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		pfile = COM_ParseLine( pfile, token );
		if( !pfile )
		{
			// line is incomplete
			gEngfuncs.Con_Printf( "^3Error:^7 achievement file has an incomplete line 1. Achievements are disabled.\n" );
			bAchievements = false;
			return;
		}
		AchievementStats[i] = Q_atoi( token );
		//	gEngfuncs.Con_Printf( "s %i %i\n", i, AchievementStats[i] );
	}

	// second line is the goals
	pfile = COM_ParseFile( pfile, token );

	for( i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		if( i > 0 ) pfile = COM_ParseLine( pfile, token );
		if( !pfile )
		{
			// line is incomplete
			gEngfuncs.Con_Printf( "^3Error:^7 achievement file has an incomplete line 2. Achievements are disabled.\n" );
			bAchievements = false;
			return;
		}
		AchievementGoal[i] = Q_atoi( token );
		//	gEngfuncs.Con_Printf( "g %i %i\n", i, AchievementGoal[i] );
	}

	// third line is the numbers, 0 or 1, checking if the achievement is completed
	pfile = COM_ParseFile( pfile, token );

	for( i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		if( i > 0 ) pfile = COM_ParseLine( pfile, token );
		if( !pfile )
		{
			// line is incomplete
			gEngfuncs.Con_Printf( "^3Error:^7 achievement file has an incomplete line 3. Achievements are disabled.\n" );
			bAchievements = false;
			return;
		}
		AchievementComplete[i] = (Q_atoi( token ) > 0);
		//	gEngfuncs.Con_Printf( "c %i %i\n", i, AchievementComplete[i] );
	}

	gEngfuncs.COM_FreeFile( afile );
}

void CHudAchievement::SaveAchievementFile(void)
{
	if( !bAchievements )
		return;
	
	// I have to do this, otherwise I can't get the file if it exists and hidden (we are doing a reset)
	SetFileAttributes( "diffusion/data/achievements.txt", FILE_ATTRIBUTE_NORMAL );

	achStatsFile = fopen( "diffusion/data/achievements.txt", "w" ); // empty the file
	if( !achStatsFile )
		return;

	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ ) // write new stuff
	{
		if( i < TOTAL_ACHIEVEMENTS - 1 )
			fprintf( achStatsFile, "%i ", AchievementStats[i] );
		else
			fprintf( achStatsFile, "%i\n", AchievementStats[i] );
	}

	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ ) // write new stuff
	{
		if( i < TOTAL_ACHIEVEMENTS - 1 )
			fprintf( achStatsFile, "%i ", AchievementGoal[i] );
		else
			fprintf( achStatsFile, "%i\n", AchievementGoal[i] );
	}

	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ ) // write new stuff
	{
		if( i < TOTAL_ACHIEVEMENTS - 1 )
			fprintf( achStatsFile, "%i ", AchievementComplete[i] );
		else
			fprintf( achStatsFile, "%i\n", AchievementComplete[i] );
	}
	fclose( achStatsFile );

	SetFileAttributes( "diffusion/data/achievements.txt", FILE_ATTRIBUTE_HIDDEN );
}

void CHudAchievement::ReportAchievementsToConsole(void)
{
	if( !bAchievements )
		return;
	
	char Completed[5];
	ALERT( at_console, "- - - - - - - - - - - - - - - - - - - -\n- - - - - - - - - - - - - - - - - - - -\n" );
	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		if( AchievementComplete[i] )
			sprintf_s( Completed, "YES" );
		else
			sprintf_s( Completed, "NO" );
		gEngfuncs.Con_Printf( "%i Achievement \"%s\", current %i, goal %i, Completed? %s\n", i, AchievementName[i], AchievementStats[i], AchievementGoal[i], Completed );
	}
	ALERT( at_console, "- - - - - - - - - - - - - - - - - - - -\n- - - - - - - - - - - - - - - - - - - -\n" );
}

void CHudAchievement::CheckAchievement(void)
{
	if( AchievementCheckTime > tr.time + ACHIEVEMENT_CHECK_TIME )
		AchievementCheckTime = tr.time + ACHIEVEMENT_CHECK_TIME;
	
	if( tr.time < AchievementCheckTime )
		return;

	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		if( (AchievementStats[i] >= AchievementGoal[i]) && !AchievementComplete[i] )
		{
			AchievementComplete[i] = true;
			if( cl_achievement_notify->value )
			{
				EnableAchievement( AchievementName[i] );
			}

			ConPrintf( "ACHIEVEMENT \"%s\" UNLOCKED!\n", AchievementName[i] );
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
	// reset everything accumulated during this session
	for( int i = 0; i < TOTAL_ACHIEVEMENTS; i++)
	{
		AchievementStats[i] = 0;
		AchievementComplete[i] = 0;
		AchievementGoal[i] = 0;
	}
	
	CreateDefaultAchievementFile();

	LoadAchievementFile();
}

//====================================================================================
// CreateDefaultStatsFile: creates a file with 0 stats, 0 completion, and goals
//====================================================================================
void CHudAchievement::CreateDefaultAchievementFile(void)
{
	int i;

	// I have to do this, otherwise I can't get the file if it exists and hidden (we are doing a reset)
	SetFileAttributes( "diffusion/data/achievements.txt", FILE_ATTRIBUTE_NORMAL );

	achStatsFile = fopen( "diffusion/data/achievements.txt", "w" ); // empty file
	if( !achStatsFile )
		return;
	
	// first line is for current stats
	for( i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		if( i < TOTAL_ACHIEVEMENTS - 1 )
			fprintf( achStatsFile, "0 " );
		else
			fprintf( achStatsFile, "0\n" );
	}

	// second line is for goals
	for( i = 0; i < TOTAL_ACHIEVEMENTS; i++ )
	{
		switch( i )
		{
		case ACH_BULLETSFIRED: fprintf( achStatsFile, "100000 " ); break;
		case ACH_JUMPS: fprintf( achStatsFile, "1000 " ); break;
		case ACH_AMMOCRATES: fprintf( achStatsFile, "100 " ); break;
		case ACH_DISARMEDMINES: fprintf( achStatsFile, "30 " ); break;
		case ACH_KILLENEMIES: fprintf( achStatsFile, "500 " ); break;
		case ACH_INFLICTDAMAGE: fprintf( achStatsFile, "10000 " ); break;
		case ACH_KILLENEMIESSNIPER: fprintf( achStatsFile, "100 " ); break;
		case ACH_CH1: fprintf( achStatsFile, "1 " ); break;
		case ACH_CH2: fprintf( achStatsFile, "1 " ); break;
		case ACH_CH3: fprintf( achStatsFile, "1 " ); break;
		case ACH_CH4: fprintf( achStatsFile, "1 " ); break;
		case ACH_CH5: fprintf( achStatsFile, "1 " ); break;
		case ACH_GENERAL30SEC: fprintf( achStatsFile, "1 " ); break;
		case ACH_HPREGENERATE: fprintf( achStatsFile, "10000 " ); break;
		case ACH_RECEIVEDAMAGE: fprintf( achStatsFile, "10000 " ); break;
		case ACH_OVERCOOK: fprintf( achStatsFile, "1 " ); break;
		case ACH_DRONESEC: fprintf( achStatsFile, "999 " ); break;
		case ACH_DRONEALIEN: fprintf( achStatsFile, "999 " ); break;
		case ACH_CROSSBOW: fprintf( achStatsFile, "10 " ); break;
		case ACH_TANKBALL: fprintf( achStatsFile, "999 " ); break;
		case ACH_DASH: fprintf( achStatsFile, "100 " ); break;
		case ACH_NOTES: fprintf( achStatsFile, "50 " ); break;
		case ACH_SECRETS: fprintf( achStatsFile, "999 " ); break;
		case ACH_KILLENEMIESBALLS: fprintf( achStatsFile, "999 " ); break;
		case ACH_REDDWELLER: fprintf( achStatsFile, "1 " ); break;
		case ACH_ASSEMBLEBLASTLEVEL: fprintf( achStatsFile, "1 " ); break;
		case ACH_BROKENCAR: fprintf( achStatsFile, "1 " ); break;
		case ACH_CARDISTANCE: fprintf( achStatsFile, "10000 " ); break;
		case ACH_WATERJETDISTANCE: fprintf( achStatsFile, "5000 " ); break;
		case ACH_KILLBOTS: fprintf( achStatsFile, "1000 " ); break;
		case ACH_CH3_NOKILLDW: fprintf( achStatsFile, "1 " ); break;
		case ACH_CH3_3MINS: fprintf( achStatsFile, "1\n" ); break;
		default: fprintf( achStatsFile, "0 " ); break;
		}
	}

	// third line is for completion marks (0/1)
	for( i = 0; i < TOTAL_ACHIEVEMENTS; i++ ) // mark them all as incomplete (print 0)
	{
		if( i < TOTAL_ACHIEVEMENTS - 1 )
			fprintf( achStatsFile, "0 " );
		else
			fprintf( achStatsFile, "0\n" );
	}

	fclose( achStatsFile );

	SetFileAttributes( "diffusion/data/achievements.txt", FILE_ATTRIBUTE_HIDDEN );
}