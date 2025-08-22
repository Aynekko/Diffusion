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
//			
//  hud.h
//
// class CHud declaration
//
// CHud handles the message, calculation, and drawing the HUD
//
#define RGB_YELLOWISH	0x00FFA000 // 255, 160, 0
#define RGB_REDISH		0x00FF1010 // 255, 160, 0
#define RGB_GREENISH	0x0000A000 // 0, 160, 0
#define RGB_RED			0x00FF3232 // 255, 50, 50
#define RGB_GREY		0x00777777 // 119, 119, 119

#define OBJECTIVE_TIMER 5.0f

#include "wrect.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <stdarg.h>
#include <ctype.h>
#endif
#include "mathlib.h"
#include "cdll_int.h"
#include "cdll_dll.h"
#include "render_api.h"
#include "enginecallback.h"
#include "randomrange.h"
#include "screenfade.h"
#include "r_view.h"
#include "ammo.h"
#include "achievementdata.h"
#include "animatex.h"

typedef struct cvar_s	cvar_t;

#define DHN_DRAWZERO 		1
#define DHN_2DIGITS  		2
#define DHN_3DIGITS  		4
#define MIN_ALPHA	 		100
#define FADE_TIME			100

#define MAX_PLAYERS			64
#define MAX_TEAMS			64
#define MAX_TEAM_NAME		16

#define HUD_ACTIVE			1
#define HUD_INTERMISSION	2

#define MAX_PLAYER_NAME_LENGTH	32
#define MAX_MOTD_LENGTH		1536

#define ROLL_CURVE_ZERO		20	// roll less than this is clamped to zero
#define ROLL_CURVE_LINEAR		90	// roll greater than this is copied out

#define PITCH_CURVE_ZERO		10	// pitch less than this is clamped to zero
#define PITCH_CURVE_LINEAR		45	// pitch greater than this is copied out
					// spline in between
			
//
//-----------------------------------------------------
//
class CHudBase
{
public:
	int m_iFlags; // active, moving,
	virtual ~CHudBase() {}
	virtual int Init( void ) { return 0; }
	virtual int VidInit( void ) { return 0; }
	virtual int Draw(float flTime) { return 0; }
	virtual void Think(void) {}
	virtual void Reset(void) {}
	virtual void InitHUDData( void ) {} // called every time a server is connected to

};

struct HUDLIST
{
	CHudBase	*p;
	HUDLIST	*pNext;
};

//
//-----------------------------------------------------
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "cl_entity.h"

//
//-----------------------------------------------------
//
class CHudAmmo: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void DrawOfflineAmmo( float x, float y );
	void Think( void );
	void Reset( void );
	int DrawWList( float flTime );
	int MsgFunc_CurWeapon( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeaponList( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_AmmoX( const char *pszName, int iSize, void *pbuf );

	void SlotInput( int iSlot );
	void _cdecl UserCmd_Slot1( void );
	void _cdecl UserCmd_Slot2( void );
	void _cdecl UserCmd_Slot3( void );
	void _cdecl UserCmd_Slot4( void );
	void _cdecl UserCmd_Slot5( void );
	void _cdecl UserCmd_Slot6( void );
	void _cdecl UserCmd_Slot7( void );
	void _cdecl UserCmd_Slot8( void );
	void _cdecl UserCmd_Slot9( void );
	void _cdecl UserCmd_Slot10( void );
	void _cdecl UserCmd_Close( void );
	void _cdecl UserCmd_NextWeapon( void );
	void _cdecl UserCmd_PrevWeapon( void );

	// to reduce calls to the engine, because I use different crosshairs
	void AmmoSetCrosshair(SpriteHandle hspr, wrect_t rc, int r, int g, int b);
	int GetPrimaryClipSize( void );
	bool PaintLowAmmo( void );

	int WeaponID; // copy of m_pWeapon->iId
	WEAPON *m_pWeapon;
private:
	float m_fFade;
	float m_fFade2;
	int	m_HUD_bucket0;
	int	m_HUD_selection;
	int m_HUD_divider;
	int iId; // weapon id
};

//
//-----------------------------------------------------
//

class CHudAmmoSecondary: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );

	int MsgFunc_SecAmmoVal( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_SecAmmoIcon( const char *pszName, int iSize, void *pbuf );

private:
	enum {
		MAX_SEC_AMMO_VALUES = 4
	};

	int m_HUD_ammoicon; // sprite indices
	int m_iAmmoAmounts[MAX_SEC_AMMO_VALUES];
	float m_fFade;
};


#include "health.h"

//
//-----------------------------------------------------
//
class CHudGeiger: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Geiger( const char *pszName, int iSize, void *pbuf );
private:
	int m_iGeigerRange;

};


//
//-----------------------------------------------------
//
class CHudTrain: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Train( const char *pszName, int iSize, void *pbuf );
private:
	SpriteHandle m_hSprite;
	int m_iPos;

};

class CHudMOTD : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );

	int MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf );

protected:
	char m_szMOTD[MAX_MOTD_LENGTH];
	int MOTD_DISPLAY_TIME;
	float m_flActiveTill;
	int m_iLines;
};

//
//-----------------------------------------------------
//
class CHudScoreboard: public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int DrawPlayers( int xoffset, float listslot, int nameoffset = 0, char *team = NULL ); // returns the ypos where it finishes drawing
	void UserCmd_ShowScores( void );
	void UserCmd_HideScores( void );
	int MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf );
	void DeathMsg( int killer, int victim );

	int m_iNumTeams;

	int m_iLastKilledBy;
	int m_fLastKillTime;
	int m_iPlayerNum;
	int m_iShowscoresHeld;

	int ypos_bottom;

	void GetAllPlayersInfo( void );
};

//
//-----------------------------------------------------
//
class CHudStatusBar : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	void ParseStatusString( int line_num );

	int MsgFunc_StatusText( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_StatusValue( const char *pszName, int iSize, void *pbuf );

protected:
	enum {
		MAX_STATUSTEXT_LENGTH = 128,
		MAX_STATUSBAR_VALUES = 8,
		MAX_STATUSBAR_LINES = 2,
	};

	char m_szStatusText[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];  // a text string describing how the status bar is to be drawn
	char m_szStatusBar[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];	// the constructed bar that is drawn
	int m_iStatusValues[MAX_STATUSBAR_VALUES];  // an array of values for use in the status bar

	int m_bReparseString; // set to TRUE whenever the m_szStatusBar needs to be recalculated

	// an array of colors...one color for each line
	float *m_pflNameColors[MAX_STATUSBAR_LINES];
};

struct extra_player_info_t
{
	short frags;
	short deaths;
	short playerclass;
	short teamnumber;
	char teamname[MAX_TEAM_NAME];
	bool isBot;
	bool isTalking; // using voice right now
};

struct team_info_t
{
	char name[MAX_TEAM_NAME];
	short frags;
	short deaths;
	short ping;
	short packetloss;
	short ownteam;
	short players;
	int already_drawn;
	int scores_overriden;
	int teamnumber;
};

//
//-----------------------------------------------------
//
class CHudDeathNotice : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf );

private:
	int m_HUD_d_skull;  // sprite index of skull icon
};

//
//-----------------------------------------------------
//
class CHudMenu : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	int MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf );

	void SelectMenuItem( int menu_item );

	int m_fMenuDisplayed;
	int m_bitsValidSlots;
	float m_flShutoffTime;
	int m_fWaitingForMore;
};

//
//-----------------------------------------------------
//
class CHudSayText : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_SayText( const char *pszName, int iSize, void *pbuf );
	void SayTextPrint( const char *pszBuf, int iBufSize, int clientIndex = -1 );
	void EnsureTextFitsInOneLineAndWrapIfHaveTo( int line );
	friend class CHudSpectator;
private:

	struct cvar_s *m_HUD_saytext;
	struct cvar_s *m_HUD_saytext_time;
};

//
//-----------------------------------------------------
//
// diffusion - no battery in game
/*
class CHudBattery: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_Battery(const char *pszName,  int iSize, void *pbuf );

private:
	SpriteHandle m_hSprite1;
	SpriteHandle m_hSprite2;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	int m_iBat;
	float m_fFade;
	int m_iHeight;		// width of the battery innards
};
*/

//
//-----------------------------------------------------
//
class CHudFlashlight: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	int MsgFunc_Flashlight( const char *pszName, int iSize, void *pbuf );
	float m_flBat;
	float m_flTurnOn; // diffusion - for smooth turn on
private:
	SpriteHandle m_hSprite1;
	SpriteHandle m_hSprite2;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	int m_iBat;
	int m_fOn;
	int m_iHeight;
	float MainAlpha;
};

// DiffusionSprint

class CHudStamina: public CHudBase
{
public:
  int Init( void );
  int VidInit( void );
  int Draw(float flTime);
  int m_iStaminaValue;
  int MsgFunc_Stamina( const char *pszName, int iSize, void *pbuf );
  void DrawOfflineBar( int x, int y );
};

// diffusion health visual
class CHudHealthVisual: public CHudBase
{
public:
  int Init( void );
  int VidInit( void );
  int Draw(float flTime);
  int m_iHealth;
  int m_iMaxHealth;
  bool GotHit; // got a message that we received a damage
  int MsgFunc_HealthVisual( const char *pszName, int iSize, void *pbuf );
  int MsgFunc_HealthVisualAlice( const char *pszName, int iSize, void *pbuf );
  void DrawOfflineBar( int x, int y );
  bool bAliceDrawHealth;
  int iAliceHealth; // percent
  void DrawAliceHealth( void );
};

class CHudDroneBars: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	bool CanUseDrone;
	int DroneAmmo;
	int DroneHealth;
	bool DroneDeployed;
	int DroneDistance;
private:
	SpriteHandle m_hBarFrame;
	SpriteHandle m_hBarHealth;
	SpriteHandle m_hBarAmmo;
	SpriteHandle m_hDroneIcon;
	wrect_t *m_prc_barframe;
	wrect_t *m_prc_barhealth;
	wrect_t *m_prc_barammo;
	wrect_t *m_prc_droneicon;
};

class CHudPuzzle : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Puzzle( const char *pszName, int iSize, void *pbuf );
	void Start( void );
	void MoveActiveBlock( int button );
	void MoveCorrectBlock( int direction );

	int field_size; // square field [field_size x field_size]
	Vector2D active_block_id;
	Vector2D correct_block_id;
	bool solved;
	float move_time; // difficulty - move the correct block with desired period
};

//
//-----------------------------------------------------
//
const int maxHUDMessages = 16;

struct message_parms_t
{
	client_textmessage_t *pMessage;
	float time;
	int x, y;
	int totalWidth, totalHeight;
	int width;
	int lines;
	int lineLength;
	int length;
	int r, g, b;
	int text;
	int fadeBlend;
	float charTime;
	float fadeTime;
};

//
//-----------------------------------------------------
//
class CHudTextMessage: public CHudBase
{
public:
	int Init( void );
	static char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size );
	static char *BufferedLocaliseTextString( const char *msg );
	char *LookupString( const char *msg_name, int *msg_dest = NULL );
	int MsgFunc_TextMsg(const char *pszName, int iSize, void *pbuf);
};

//
//-----------------------------------------------------
//
class CHudMessage: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_HudText(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_GameTitle(const char *pszName, int iSize, void *pbuf);

	float FadeBlend( float fadein, float fadeout, float hold, float localTime );
	int XPosition( float x, int width, int lineWidth );
	int YPosition( float y, int height );

	void MessageAdd( const char *pName, float time );
	void MessageAdd(client_textmessage_t * newMessage );
	void MessageDrawScan( client_textmessage_t *pMessage, float time );
	void MessageScanStart( void );
	void MessageScanNextChar( void );
	void Reset( void );

private:
	client_textmessage_t *m_pMessages[maxHUDMessages];
	float m_startTime[maxHUDMessages];
	message_parms_t m_parms;
	float m_gameTitleTime;
	client_textmessage_t *m_pGameTitle;
	int m_HUD_title_life;
	int m_HUD_title_half;
};

//
//-----------------------------------------------------
//
#define MAX_SPRITE_NAME_LENGTH	32

class CHudStatusIcons: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw(float flTime);
	int MsgFunc_StatusIcon( const char *pszName, int iSize, void *pbuf );
	int IsAdditive;

	enum {
		MAX_ICONSPRITENAME_LENGTH = MAX_SPRITE_NAME_LENGTH,
		MAX_ICONSPRITES = 8,
	};


	// had to make these public so CHud could access them (to enable concussion icon)
	// could use a friend declaration instead...
	void EnableIcon( char *pszIconName, byte red, byte green, byte blue );
	void DisableIcon( char *pszIconName );

private:
	typedef struct
	{
		char szSpriteName[MAX_ICONSPRITENAME_LENGTH];
		SpriteHandle spr;
		wrect_t rc;
		unsigned char r, g, b;
	} icon_sprite_t;

	icon_sprite_t m_IconList[MAX_ICONSPRITES];
};

class CHudTutorial: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_StatusIconTutor( const char *pszName, int iSize, void *pbuf );

	enum {
		MAX_ICONSPRITENAME_LENGTH = MAX_SPRITE_NAME_LENGTH,
		MAX_ICONSPRITES = 4,
	};

	float TutorStartTime;
	bool IsTutorDrawing;
	float alpha;
	bool alpha_direction; // false - alpha+, true - alpha-
	void EnableTutorial( const char *pszTutorialName );
	int CurrentImage;
	void MessageDraw( client_textmessage_t *pMessage, int x, int y, bool GetSize = false );

	client_textmessage_t *tutorial;
	char tutorial_text[2048];
	int Twidth;
	int Theight;
};

class CHudAchievement: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	void _cdecl UserCmd_RefreshAchievementFile( void );
	void _cdecl UserCmd_ResetAchievementFile( void );
	void _cdecl UserCmd_ReportAchievementsToConsole( void );
	float x, y;
	float AchStartTime;
	bool IsAchDrawing;

	FILE *achStatsFile;
	bool bAchievements; // disabled if the goal file wasn't loaded
	achievement_data_t ach_data;

	void EnableAchievement(char* pszIconName );

	void LoadAchievementFile( void );
	void SaveAchievementFile( bool backup = false );
	void CheckAchievement( void );
	void CreateDefaultAchievementFile( void );
	float AchievementCheckTime;
	int CurrentImage;
	client_textmessage_t *pTitle;
	client_textmessage_t *pText;
};

class CHudCrosshairStatic: public CHudBase
{
public:
	int Init(void);
	int VidInit(void);
	int Draw(float flTime);
	int x; int y;
	SpriteHandle m_hCrosshairStatic;
	int MsgFunc_CrosshairStatic( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_GaussHUD( const char *pszName, int iSize, void *pbuf );
	int CrosshairSprite;
	int WeaponID;
	bool DroneControl;
	float ZoomBlur;

	// zoomed crosshairs (dds images)
	int SniperCrosshairImage;
	int CrossbowCrosshairImage;
	int AlienCrosshairImage;
	int G36CCrosshairImage;

	// hitmarker:
	bool EnableHitMarker;
	int HMSprite;
	float HMTransparency;
	int ConfirmedHit;
	int DamageDealt;

	// gauss stuff
	int m_hGAmmo;
	int m_hGCharge;
	int m_hGFrame;
	wrect_t *m_prc_Gcharge;
	wrect_t *m_prc_Gammo;
	wrect_t *m_prc_Gframe;
	int GaussAmmo;
	int GaussCharge;
	int GaussSound;

	void Reset( void );
	void LoadCrosshairForWeapon( int WeaponID );
	void DrawGaussZoomedHUD( void );
private:
	typedef struct
	{
		char szSpriteName[MAX_SPRITE_NAME_LENGTH];
		SpriteHandle spr;
		wrect_t rc;
		unsigned char r, g, b;
	} crosshair_static_t;

	WEAPON	*m_pWeapon;

	crosshair_static_t m_CrosshairStatic;
	crosshair_static_t m_HM;
};

class CZoom : public CHudBase
{
public:
	int Init(void);
	void Think(void);
	int MsgFunc_Zoom(const char* pszName, int iSize, void* pbuf);
private:
	int WeaponID;
	int ZoomMode;
};

class CHudBlastIcons : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int MsgFunc_BlastIcons( const char *pszName, int iSize, void *pbuf );
	int Draw( float flTime );
private:
	int BlastAbilityLVL;
	int BlastChargesReady;
	bool CanElectroBlast;
	float AlphaFade1, AlphaFade2, AlphaFade3;
	float circlea1, circlea2, circlea3;
};

class CHudTriggerTimer : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int timer;
	char message[255];
	int enabled;
	bool critical;
};

class CHudSubtitle : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Subtitle( const char *pszName, int iSize, void *pbuf );

	float draw_time;
	float drawstart_time;
	float alpha;
	char pText[2048];
	char pName[128];
	int subtitle_width;
	int subtitle_height;
};

class CHudHintObjective : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Hint( const char *pszName, int iSize, void *pbuf );
	void DrawHintPopUp( void );

	bool bShowMissionObjectives;
	bool bShowMissionObjectivesTimed;

	// hint
	float hint_alpha;
	char pHint[512];
	int hint_width;
	int hint_height;
	float hint_drawstart_time;
	float hint_ypos;
	int HintImage;
	void SetupHint( void );

	// mission objective stuff
	char pObjective[2][512];
	void SetupObjectives( void );
	int obj_width;
	int obj_height;
	int obj_primarytext_height;
	int obj_secondarytext_height;
	float obj_alpha;
	float obj_ypos;
	float fForcedTimer;
};

class CHudCodeInput : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int MsgFunc_CodeInput( const char *pszName, int iSize, void *pbuf );
	int Draw( float flTime );
	int num[4];
	int num_user[4]; // what user entered
	int entindex; // the entity number we will use in case of success...

	bool CodeInputScreenIsOn;
	int InputStep;
	bool CodeSuccess;
	void DisableCodeScreen( void );
	void EnterCode( int num );
	CAnimatex CodeInputSpr;
	Vector InputOrigin; // disable screen if player goes away
	float CloseTime; // time in future when screen disappears
	int r;
	int g;
	int b;
};

class CScreenEffects : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void DrawVignette( void );
	void DrawCinematicBorder( void );
	void DrawSpeedometer( void );
	void DrawShieldVignette( void );
	void DrawGameSaved( void );
	void DrawVoiceIcon( void );
	bool ShouldDrawVoiceIcon;
	bool ShouldDrawGameSaved;
	bool SaveIcon_Reset;
	bool InCar;
	int Gear;
private:
	int Vignette;
	int VignetteShield;
	int Speedometer;
	int SpeedometerArrow;
	CAnimatex SpeedometerGears;
	int SaveIcon;
	float SaveIcon_RotSpeed;
	float SaveIcon_Alpha;
	float SaveIcon_StartTime;
	float VignetteAlpha;
	float ShieldAlpha;
	float SpeedArrowRotation;
	float LastOriginUpdate; // multiplayer issues!
	Vector LastOrigin;
};

class CPseudoGUI:public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_ShowNote( const char *pszName, int iSize, void *pbuf );
	void MessageDraw( client_textmessage_t *pMessage, int x, int y );
	void Enable( void );
	void CloseWindow( bool mouse = false );
	
	char m_szMOTD[MAX_MOTD_LENGTH];
	int scrolled_lines;

	typedef struct
	{
		int	x, y, w, h;
		float r, g, b, a;
	} rectangle_t;

	rectangle_t rFrame;
	rectangle_t rClose;

	bool DotInRect( rectangle_t *rect, int x, int y );
};

class CMouseCursor:public CHudBase
{
public:
	int x, y;
	int Init( void );
	int VidInit( void );
	void DrawCursor( void );
	bool GetMousePosition( void );
};

class CHealthbars:public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Healthbars( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_HealthbarCenter( const char *pszName, int iSize, void *pbuf );
	int health;
	int barsize;
	int entindex;
	int hptex;
	int hptex2;
	bool bCentered;
	void DrawCentralBar( void );
	char MonsterName[256]; // for central bar only
	int health_center; // for center bar

	SpriteHandle m_hBarEmpty;
	SpriteHandle m_hBarFull;
	wrect_t *m_prc_emp;
	wrect_t *m_prc_full;
	int m_iBarWidth;
};

class CUseIcon: public CHudBase
{
public:
	int Init(void);
	int VidInit(void);
	int Draw(float flTime);
	int x; int y;
	int UsePressed;
	SpriteHandle m_hUseIcon;
	int MsgFunc_UseIcon( const char *pszName, int iSize, void *pbuf );
	int UseIconSprite;
	void EnableIcon( int UsePressed );
	Vector UseableEntOrigin;
private:
	typedef struct
	{
		char szSpriteName[MAX_SPRITE_NAME_LENGTH];
		SpriteHandle spr;
		wrect_t rc;
		unsigned char r, g, b;
	} useicon_t;

	useicon_t m_UseIcon;
};

//
//-----------------------------------------------------
//
class CHud
{
private:
	HUDLIST *m_pHudList;
	client_sprite_t *m_pSpriteList;
	int m_iSpriteCount;
	int m_iSpriteCountAllRes;
	float m_flMouseSensitivity;
	int m_iConcussionEffect;
	SpriteHandle m_hsprLogo;
	int m_iLogo;
public:
	float m_flTime;	 // the current client time
	float m_fOldTime;	 // the time at which the HUD was last redrawn
	double m_flTimeDelta;// the difference between flTime and fOldTime
	Vector m_vecOrigin;
	Vector m_vecAngles;
	int m_iKeyBits;
	int m_iHideHUDDisplay;
	float m_flFOV;
	int m_Teamplay;
	int m_iRes;
	cvar_t *m_pCvarDraw;
	cvar_t *default_fov;
	int m_iCameraMode;
	int m_iFontHeight;
	int DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b );
	int DrawHudString( int x, int y, int iMaxX, char *szString, int r, int g, int b );
	int DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b );
	int DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b, bool forward = false );
	int GetNumWidth( int iNumber, int iFlags );
	int m_iHUDColor;
	bool IsZooming; // true if player is zooming, changing the fov on client (ignore server changes); false if the zoom level is fully set
	bool IsZoomed; // true if we are in any state of zoom
	bool IsDrawingOfflineHUD; // true when we are drawing "offline" in health and stamina
	bool CanJump;
	bool CanSprint;
	bool CanMove; // WASD
	bool HUDSuitOffline; // draw "OFFLINE" on health and stamina
	bool CanSelectEmptyWeapon;
	bool InCar;
	float CarAddFovMult;
	bool PlayingDrums;
	bool WeaponLowered;
	int DrunkLevel;
	bool CanShoot;
	float CarSpeed; // kph
	bool BreathingEffect; // breathing effect
	bool WigglingEffect;
	bool ShieldOn;
	float NextDrawingOfflineHUDTime; // time in the future when we are allowed to draw "offline" again
	Vector DroneColor;

	// saved cursor movement
	Vector2D MxMy;

	// for WaterDrops shader
	int ScreenDrips_RainDripsPerSecond;
	float ScreenDrips_CurVisibility;
	bool ScreenDrips_Visible;
	float ScreenDrips_DripIntensity;
	float ScreenDrips_OverrideTime;
	float Weather_Intensity;

	// viewmodel lag
	Vector m_vecLastFacing;

	// glitch shader
	float GlitchAmount;
	float GlitchHoldTime;

	// lensflare shader
	float LensFlareAlpha;

	// diffusion - client screen shake
	typedef struct
	{
		float		time;
		float		duration;
		float		amplitude;
		float		frequency;
		float		next_shake;
		Vector		offset;
		float		angle;
		Vector		applied_offset;
		float		applied_angle;
	} screen_shake_t;
	screen_shake_t	shake;	// screen shake
private:
	// the memory for these arrays are allocated in the first call to CHud::VidInit()
	// when the hud.txt and associated sprites are loaded. freed in ~CHud()
	SpriteHandle *m_rghSprites; // the sprites loaded from hud.txt
	wrect_t *m_rgrcRects;
	char *m_rgszSpriteNames;
public:
	SpriteHandle GetSprite( int index ) { return (index < 0) ? 0 : m_rghSprites[index]; }
	wrect_t& GetSpriteRect( int index ) { return m_rgrcRects[index]; }
          int InitHUDMessages( void ); // init hud messages
	int GetSpriteIndex( const char *SpriteName ); // gets a sprite index, for use in the m_rghSprites[] array

	CMouseCursor m_Cursor;
	CHudAmmo		m_Ammo;
	CHudHealth	m_Health;
	CHudGeiger	m_Geiger;
//	CHudBattery	m_Battery; // diffusion - no battery in game
	CHudTrain		m_Train;
	CHudFlashlight	m_Flash;
	CHudMessage	m_Message;
	CHudScoreboard	m_Scoreboard;
	CHudStatusBar	m_StatusBar;
	CHudDeathNotice	m_DeathNotice;
	CHudSayText	m_SayText;
	CHudMenu		m_Menu;
	CHudAmmoSecondary	m_AmmoSecondary;
	CHudTextMessage	m_TextMessage;
	CHudStatusIcons	m_StatusIcons;
	CHudTutorial m_StatusIconsTutor;
	CHudAchievement m_StatusIconsAchievement;
	CHudMOTD		m_MOTD;
	CHudStamina		m_Stamina; //DiffusionSprintRegister
	CHudHealthVisual		m_HealthVisual; // diffusion health visual
	CHudDroneBars m_DroneBars;
	CHudCrosshairStatic	m_CrosshairStatic;
	CHudCrosshairStatic	m_GaussHUD;
	CZoom m_Zoom;
	CUseIcon m_UseIcon;
	CScreenEffects m_ScreenEffects;
	CHudBlastIcons m_BlastIcons;
	CHealthbars m_Healthbars;
	CHudCodeInput m_CodeInput;
	CPseudoGUI m_PseudoGUI;
	CHudTriggerTimer m_TriggerTimer;
	CHudPuzzle m_Puzzle;
	CHudSubtitle m_Subtitle;
	CHudHintObjective m_HintObjectives;
	
	void Init( void );
	void VidInit( void );
	void Think(void);
	int Redraw( float flTime, int intermission );
	int UpdateClientData( client_data_t *cdata, float time );

	void SetFOV( int newfov );
	void SetupScale( void );
	float fScale;
	float fCenteredPadding;
	void GL_HUD_StartConstantSize( bool aligned_right = false );
	void GL_HUD_EndConstantSize( void );

	CHud() : m_iSpriteCount(0), m_pHudList(NULL) {}
	~CHud();	// destructor, frees allocated memory

	// user messages
	int _cdecl MsgFunc_Damage( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_GameMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Logo( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ResetHUD( const char *pszName,  int iSize, void *pbuf );
	int _cdecl MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf);
	int _cdecl MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_RainData( const char *pszName, int iSize, void *pbuf ); 
	int _cdecl MsgFunc_HUDColor(const char *pszName,  int iSize, void *pbuf);
	int _cdecl MsgFunc_SetBody( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetSkin( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Particle( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_KillPart( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_KillDecals( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_WeaponAnim( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_MusicFade( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Weapons( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_StudioDecal( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetupBones( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_TempEnt( const char *pszName, int iSize, void *pbuf ); // diffusion - I moved some tempentity from engine to client
	int _cdecl MsgFunc_WaterSplash( const char *pszName, int iSize, void *pbuf ); // watersplash
	int _cdecl MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf );
		
	// Screen information
	SCREENINFO m_scrinfo;

	byte m_iWeaponBits[MAX_WEAPON_BYTES];
	int m_fPlayerDead;
	int m_iIntermission;

	// sprite indexes
	int m_HUD_number_0;
	
	// error sprite
	int m_HUD_error;
	SpriteHandle m_hHudError;
	
	void AddHudElem( CHudBase *p );
	float GetSensitivity() { return m_flMouseSensitivity; }
	BOOL HasWeapon( int weaponnum ) { return FBitSet( m_iWeaponBits[weaponnum >> 3], BIT( weaponnum & 7 )); }
	void AddWeapon( int weaponnum ) { SetBits( m_iWeaponBits[weaponnum >> 3], BIT( weaponnum & 7 )); }

	void DrumsInput( int Slot );

	bool emptyclipspawned[TOTAL_WEAPONS];

	char m_szServerName[96];
};

extern CHud	gHUD;
extern hud_player_info_t	g_PlayerInfoList[MAX_PLAYERS+1];	// player info from the engine
extern extra_player_info_t	g_PlayerExtraInfo[MAX_PLAYERS+1];	// additional player info sent directly to the client dll
extern team_info_t		g_TeamInfo[MAX_TEAMS+1];

extern int g_iPlayerClass;
extern int g_iTeamNumber;
extern int g_iUser1;
extern int g_iUser2;
extern int g_iUser3;
extern float g_fFrametime; // = pparams->frametime
extern bool ApplyGaussBlur;
extern bool ApplyDamageMonochrome;

// diffusion - for local shoot animations (TEST)
extern float localanim_NextPAttackTime;
extern float localanim_NextSAttackTime;
extern int localanim_WeaponID;
extern bool localanim_AllowRpgShoot;

// local_weapon_anims.cpp
void V_LocalWeaponAnimations( struct ref_params_s *pparams );
bool LocalWeaponAnims( void );
bool CheckForLocalWeaponShootAnimation( int seq );