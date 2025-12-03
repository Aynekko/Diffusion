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
#ifndef PLAYER_H
#define PLAYER_H

//START BOT
class CBotCam;
//END BOT


#include "pm_materials.h"
#include "entities/func_car.h"

#define PLAYER_FATAL_FALL_SPEED	1024	// approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED	540//580	// approx 20 feet
#define DAMAGE_FOR_FALL_SPEED		100.0f / ( PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED )// damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED	200
#define PLAYER_FALL_PUNCH_THRESHHOLD	320.0f // won't punch player's screen/make scrape noise unless player falling at least this fast.

#define USEICON_NOICON 0
#define USEICON_CANUSE 1
#define USEICON_PRESSED 2
#define USEICON_BUSY 3
#define USEICON_LOCKED 4
#define USEICON_INTERACTION 5 // animated circle when pressing button

//
// Player PHYSICS FLAGS bits
//
#define		PFLAG_ONLADDER		( 1<<0 )
#define		PFLAG_ONSWING		( 1<<0 )
#define		PFLAG_ONTRAIN		( 1<<1 )
#define		PFLAG_ONBARNACLE	( 1<<2 )
#define		PFLAG_DUCKING		( 1<<3 )		// In the process of ducking, but totally squatted yet
#define		PFLAG_USING			( 1<<4 )		// Using a continuous entity
#define		PFLAG_OBSERVER		( 1<<5 )		// player is locked in stationary cam mode. Spectators can move, observers can't.
#define		PFLAG_ONROPE		( 1<<6 )

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST		4		// max of 4 suit sentences queued up at any time

#define SUIT_GROUP			TRUE
#define SUIT_SENTENCE		FALSE

#define SUIT_REPEAT_OK		0
#define SUIT_NEXT_IN_30SEC		30
#define SUIT_NEXT_IN_1MIN		60
#define SUIT_NEXT_IN_5MIN		300
#define SUIT_NEXT_IN_10MIN		600
#define SUIT_NEXT_IN_30MIN		1800
#define SUIT_NEXT_IN_1HOUR		3600

#define CSUITNOREPEAT		32

#define SOUND_FLASHLIGHT_ON		"items/flashlight1.wav"
#define SOUND_FLASHLIGHT_OFF	"items/flashlight1.wav"

#define TEAM_NAME_LENGTH	16

/*
typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
} PLAYER_ANIM;*/
// nextday
typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
	PLAYER_RELOAD,
} PLAYER_ANIM;

#define MAX_ID_RANGE 2048
#define SBAR_STRING_SIZE 128

#include "achievementdata.h"

#define HUD_TEXT_DELAY 0.4f

enum sbar_data
{
	SBAR_ID_TARGETNAME = 1,
	SBAR_ID_TARGETHEALTH,
	SBAR_ID_TARGETARMOR,
	SBAR_END,
};

class CRope;

#define CHAT_INTERVAL 1.0f
#define CHAT_SOUND_INTERVAL 10.0f

#define MAX_KEYCATCHERS	MAX_ENTITYARRAY

typedef struct
{
	const char	*buttonName;
	int		buttonCode;
} catchtable_t;

class CPlayerKeyCatcher : public CBaseDelay
{
	DECLARE_CLASS( CPlayerKeyCatcher, CBaseDelay );
public:
	string_t	m_iszKeyPressed;
	string_t	m_iszKeyReleased;
	string_t	m_iszKeyHoldDown;

	DECLARE_DATADESC();	

	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	CatchButton( CBaseEntity *pActivator, int buttons, int pressed, int released );
	int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int Restore( CRestore &restore );
};

class CBasePlayer : public CBaseMonster
{
	DECLARE_CLASS( CBasePlayer, CBaseMonster );
public:
	int		random_seed;    // See that is shared between client & server for shared weapons code

	int		m_iPlayerSound;// the index of the sound list slot reserved for this player
	int		m_iTargetVolume;// ideal sound volume. 
	int		m_iWeaponVolume;// how loud the player's weapon is right now.
	int		m_iExtraSoundTypes;// additional classification for this weapon's sound
	int		m_iWeaponFlash;// brightness of the weapon flash
	float		m_flStopExtraSoundTime;
	
	float		m_flFlashLightTime;	// Time until next battery draw/Recharge
	int		m_iFlashBattery;		// Flashlight Battery Draw

	int		m_afButtonLast;
	int		m_afButtonPressed;
	int		m_afButtonReleased;
	
	edict_t		*m_pentSndLast;			// last sound entity to modify player room type
	float		m_flSndRange;			// dist from player to sound entity
	float		m_flSndRoomtype;
	float		m_flFallVelocity;
	
	int		m_rgItems[MAX_ITEMS];
	int		m_fKnownItem;		// True when a new item needs to be added
	int		m_fNewAmmo;			// True when a new item has been added

	unsigned int	m_afPhysicsFlags;	// physics flags - set when 'normal' physics should be revisited or overriden
	float		m_fNextSuicideTime; // the time after which the player can next use the suicide command


// these are time-sensitive things that we keep track of
	float		m_flTimeStepSound;	// when the last stepping sound was made
	float		m_flTimeWeaponIdle; // when to play another weapon idle animation.
	float		m_flSwimTime;		// how long player has been underwater
	float		m_flDuckTime;		// how long we've been ducking

	float		m_flSuitUpdate;					// when to play next suit update
	int		m_rgSuitPlayList[CSUITPLAYLIST];// next sentencenum to play for suit update
	int		m_iSuitPlayNext;				// next sentence slot for queue storage;
	int		m_rgiSuitNoRepeat[CSUITNOREPEAT];		// suit sentence no repeat list
	float		m_rgflSuitNoRepeatTime[CSUITNOREPEAT];	// how long to wait before allowing repeat
	int		m_lastDamageAmount;		// Last damage taken
	float		m_tbdPrev;				// Time-based damage timer

	float		m_flgeigerRange;		// range to nearest radiation source
	float		m_flgeigerDelay;		// delay per update of range msg to client
	int		m_igeigerRangePrev;
	int		m_iStepLeft;			// alternate left/right foot stepping sound
	char		m_szTextureName[CBTEXTURENAMEMAX];	// current texture name we're standing on
	char		m_chTextureType;		// current texture type

	int		m_idrowndmg;			// track drowning damage taken
	int		m_idrownrestored;		// track drowning damage restored

	int		m_bitsHUDDamage;		// Damage bits for the current fame. These get sent to 
												// the hude via the DAMAGE message
	BOOL		m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	BOOL		m_fGameHUDInitialized;
	int		m_iTrain;				// Train control position
	BOOL		m_fWeapon;				// Set this to FALSE to force a reset of the current weapon HUD info

	EHANDLE		m_pTank;				// the tank which the player is currently controlling,  NULL if no tank
	EHANDLE		m_pMonitor;
	EHANDLE		m_pHoldableItem;
	float		m_fDeadTime;			// the time at which the player died  (used in PlayerDeathThink())

	BOOL		m_fNoPlayerSound;	// a debugging feature. Player makes no sound if this is true. 
	BOOL		m_fLongJump; // does this player have the longjump module?

	float		m_tSneaking;
	int		m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int		m_iClientHealth;	// the health currently known by the client.  If this changes, send a new
	int		m_iClientBattery;	// the Battery currently known by the client.  If this changes, send a new
	int		m_iHideHUD;		// the players hud weapon info is to be hidden
	int		m_iClientHideHUD;
	int		m_iClientWeaponID; // new crosshair system is using it, this is client's known m_pActiveItem->m_iId
	int		m_iClientStamina;

	int m_iClientUseIcon;
	int m_iUseIcon; // 0 no icon, 1 - blue icon (useable), 2 - green icon (pressed), 3 - busy icon, 4 - denied icon
	float LastUseCheckTime; // we are always looking for an object to use, don't do this every frame
	EHANDLE m_hCachedUseObject; // cache last found object because we don't do check every frame
	Vector UseEntOrg; // useable entity origin
	Vector ClientUseEntOrg;

	float m_flFOV;	// field of view // diffusion - changed to float
	int m_iClientFOV;	// client's known FOV

	byte		m_iClientWeapons[MAX_WEAPON_BYTES];	// client's known weapon flags
	int		m_iClientSndRoomtype;	// client last roomtype set by sound entity

	// usable player items 
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];
	CBasePlayerItem	*m_pActiveItem;
	CBasePlayerItem	*m_pClientActiveItem;  // client version of the active item
	CBasePlayerItem	*m_pLastItem;

	// don't save restore this
	EHANDLE		m_hKeyCatchers[MAX_KEYCATCHERS];
	int		m_iNumKeyCatchers;

	// shared ammo slots
	int		m_rgAmmo[MAX_AMMO_SLOTS];
	int		m_rgAmmoLast[MAX_AMMO_SLOTS];

	Vector		m_vecAutoAim;
	BOOL		m_fOnTarget;
	int		m_iDeaths;
	float		m_iRespawnFrames;	// used in PlayerDeathThink() to make sure players can always respawn

	int		m_lastx, m_lasty;  // These are the previous update's crosshair angles, DON"T SAVE/RESTORE

	int		m_nCustomSprayFrames;// Custom clan logo frames for this player
	float		m_flNextDecalTime;// next time this player can spray a decal

	char		m_szTeamName[TEAM_NAME_LENGTH];

	virtual void Spawn( void );
	void Pain( void );

//	virtual void Think( void );
	virtual void Jump( void );
	virtual void Duck( void );
	virtual void PreThink( void );
	virtual void PostThink( void );
	virtual Vector GetGunPosition( void );
	virtual int TakeHealth( float flHealth, int bitsDamageType );
	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center( ) + pev->view_ofs * RANDOM_FLOAT( 0.5, 1.1 ); };		// position to shoot at
	virtual void StartSneaking( void ) { m_tSneaking = gpGlobals->time - 1; }
	virtual void StopSneaking( void ) { m_tSneaking = gpGlobals->time + 30; }
	virtual BOOL IsSneaking( void ) { return m_tSneaking <= gpGlobals->time; }
	virtual BOOL IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual BOOL ShouldFadeOnDeath( void ) { return FALSE; }
	virtual BOOL IsPlayer( void ) { return TRUE; }		// Spectators should return FALSE for this, they aren't "players" as far as game logic is concerned

	virtual BOOL IsNetClient( void ) { return TRUE; }		// Bots should return FALSE for this, they can't receive NET messages
															// Spectators should return TRUE for this
	virtual const char *TeamID( void );

	virtual int Restore( CRestore &restore );

	void RenewItems(void);
	void PackDeadPlayerItems( void );
	void RemoveAllItems( BOOL removeSuit, BOOL removeCycler = TRUE );
	BOOL SwitchWeapon( CBasePlayerItem *pWeapon );

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void UpdateClientData( void );
	
	DECLARE_DATADESC();

	// Player is moved across the transition by other means
	virtual int	ObjectCaps( void ) { return CBaseMonster :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Precache( void );
	BOOL		IsOnLadder( void );
	BOOL		FlashlightIsOn( void );
	void		FlashlightTurnOn( void );
	void		FlashlightTurnOff( void );
	
	void UpdatePlayerSound ( void );
	void DeathSound ( void );
	void OnTeleport( void );

	void TransferReset( void );
	void UpdateKeyCatchers( void );

	int Classify ( void );
	void SetAnimation( PLAYER_ANIM playerAnim );
	void SetWeaponAnimType( const char *szExtention );
	char m_szAnimExtention[32];

	// custom player functions
	virtual void ImpulseCommands( void );
	void CheatImpulseCommands( int iImpulse );

	BOOL m_bConnected;		// we set it in Spawn() so it will be TRUE only after player was spawned
	BOOL m_bPutInServer;	// we set it after PutInServer finished
	BOOL IsConnected() { return m_bConnected; }
	void Disconnect() { m_bConnected = FALSE; m_bPutInServer = FALSE; }

	void StartDeathCam( void );
//	void StartObserver( Vector vecPosition, Vector vecViewAngle );
	float m_flNextSpectatorCommand;
	int IsObserver() { return pev->iuser1; };
	void StartObserver(void);
	void StopObserver(void);
	void Observer_FindNextPlayer(bool bReverse);
	void Observer_FindNextSpot(bool bReverse);
	void Observer_HandleButtons();
	void Observer_SetMode(int iMode);
	void Observer_CheckTarget();
	void Observer_CheckProperties();
	EHANDLE m_hObserverTarget;
	float m_flNextObserverInput;
	int m_iObserverMode;
	int	m_iObserverWeapon;	// weapon of current tracked target
	int	m_iObserverLastMode;// last used observer mode

	void AddPoints( int score, BOOL bAllowNegativeScore );
	void AddPointsToTeam( int score, BOOL bAllowNegativeScore );
	BOOL AddPlayerItem( CBasePlayerItem *pItem );
	BOOL RemovePlayerItem( CBasePlayerItem *pItem );
	void DropPlayerItem ( const char *pszItemName );
	BOOL HasPlayerItem( CBasePlayerItem *pCheckItem );
	BOOL HasNamedPlayerItem( const char *pszItemName );
	BOOL HasWeapons( void );// do I have ANY weapons?
	void SelectPrevItem( int iItem );
	void SelectNextItem( int iItem );
	void SelectLastItem( void );
	void SelectItem( const char *pstr );
	void ItemPreFrame( void );
	void ItemPostFrame( void );
	void GiveNamedItem( const char *szName );
	void CreateNamedItem( const char *szName, int Despawn ); // diffusion
	void EnableControl(BOOL fControl);
	void HideWeapons( BOOL fHideWeapons );
	void SnapEyeAngles( const Vector &viewAngles );
	void RefreshScore( void );

	int  GiveAmmo( int iAmount, const char *szName, int iMax );
	void SendAmmoUpdate(CBasePlayer* pPlayer);

	void WaterMove( void );
	void PlayerDeathThink( void );
	void PlayerUse( void );
	void ManageRope(void);
	void ManageTrain(void);

	void CheckSuitUpdate();
	void SetSuitUpdate(const char *name, int fgroup, int iNoRepeat);
	void UpdateGeigerCounter( void );
	void CheckTimeBasedDamage( void );

	BOOL FBecomeProne ( void );
	void BarnacleVictimBitten ( entvars_t *pevBarnacle );
	void BarnacleVictimReleased ( void );
	static int GetAmmoIndex(const char *psz);
	int AmmoInventory( int iAmmoIndex );
	int Illumination( void );

	void ResetAutoaim( void );
	Vector GetAutoaimVector( float flDelta  );
	Vector AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  );

	void ForceClientDllUpdate( void );  // Forces all client .dll specific data to be resent to client.

	void DeathMessage( entvars_t *pevKiller );
	void SendStartMessages( void );
 
	void SetCustomDecalFrames( int nFrames );
	int GetCustomDecalFrames( void );

	bool IsOnRope() const { return ( m_afPhysicsFlags & PFLAG_ONROPE ) != 0; }

	void SetOnRopeState( bool bOnRope )
	{
		if( bOnRope )
			m_afPhysicsFlags |= PFLAG_ONROPE;
		else
			m_afPhysicsFlags &= ~PFLAG_ONROPE;
	}

	CRope* GetRope() { return m_pRope; }

	void SetRope( CRope* pRope )
	{
		m_pRope = pRope;
	}

	void SetIsClimbing( const bool bIsClimbing )
	{
		m_bIsClimbing = bIsClimbing;
	}

	void CheckCompatibility( void );

	void UpdateHoldableItem( void );
	void PickHoldableItem( CBaseEntity *pObject );
	void DropHoldableItem( int Velocity );
	float DropHoldItemVel;

	//Player ID
	void InitStatusBar( void );
	void UpdateStatusBar( void );
	int m_izSBarState[ SBAR_END ];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	float m_flHealthbarsDisappearDelay;
	char m_SbarString0[ SBAR_STRING_SIZE ];
	char m_SbarString1[ SBAR_STRING_SIZE ];
	
	float m_flNextChatTime;
	float m_flNextChatSoundTime;
	int m_iStartMessage;	
	int	m_iSndRoomtype;	// last roomtype set by sound entity
 
	float	m_flHoldableItemDistance;
	Vector	m_vecHoldableItemPosition;
	CRope*	m_pRope;

	float m_flDeathAnimationStartTime;

	float	m_flLastClimbTime;
	bool	m_bIsClimbing;

	// rain variables
	int	m_iRainDripsPerSecond;
	float	m_flRainWindX;
	float	m_flRainWindY;
	float	m_flRainRandX;
	float	m_flRainRandY;

	int	m_iRainIdealDripsPerSecond;
	float	m_flRainIdealWindX;
	float	m_flRainIdealWindY;
	float	m_flRainIdealRandX;
	float	m_flRainIdealRandY;

	float	m_flRainEndFade;		// 0 means off
	float	m_flRainNextFadeUpdate;

	bool	m_bRainNeedsUpdate;		// don't save\restore this

	float	m_flStaminaValue;    // DiffusionSprintKu2zoff
	float	m_flStaminaWait;

	float	m_fTimeLastHurt;     // DiffusionRegen
	bool CanRegenerateHealth;
	float	EnterWaterTime; // play sound when entering water
	float	m_flUseReleaseTime; // time when player released USE button (to prevent use spam)

	int m_iBonusWeaponLevel; // multiplayer. ++ when you die, -- when you kill someone.
	int m_iBonusWeaponID; // remember the weapon id, don't drop it when you die

	// in multiplayer, player spawn in an observer state first
	bool m_bInWelcomeCam;
	void StartWelcomeCam( void );
	void StopWelcomeCam( void );

	Vector m_vecLastViewAngles; // Don't allow dead model to rotate until DeathCam or Spawn happen (CopyToBodyQue)

	void Dash(void);
	Vector DashRememberVelocity;
	void RegenerateHealth(void);
	void ManageStamina(void);

	// activated by ClientCommand
	bool DashButton; // not saved
	bool ElectroblastButton; // not saved

	float LastDashTime; // last time player dashed
	bool Dashed; // true if the player just dashed, becomes false after a second
	bool CanDash; // can player dash?
	bool CanUse; // can player USE things?
	bool CanElectroBlast; // can player use electroblast (if present)? 
	bool CanJump; // can player jump?
	bool CanSprint; // can player run?
	bool CanMove; // can player use WASD buttons?
	bool CanSelectEmptyWeapon; // player can select empty weapons (needed for Chapter 3...)
	bool HUDSuitOffline; // show "offline" for health and stamina
	bool BreathingEffect; // breathing steam effect
	bool WigglingEffect; // roll wiggling, like standing on a windy cliff
	bool ShieldOn;
	bool PlayingDrums;
	bool WeaponLowered;
	bool CanShoot;
	int DrunkLevel;
	// client cache
	bool CanJump_CL;
	bool CanSprint_CL;
	bool CanMove_CL;
	bool HUDSuitOffline_CL;
	bool CanSelectEmptyWeapon_CL;
	bool InCar_CL;
	bool BreathingEffect_CL;
	bool WigglingEffect_CL;
	bool ShieldOn_CL;
	bool PlayingDrums_CL;
	bool WeaponLowered_CL;
	bool CanShoot_CL;
	int DrunkLevel_CL;
	int SunLightScale_CL;

	string_t Objective; // objective name from titles.txt
	string_t Objective2; // secondary objective
	string_t Objective_CL; // objective name from titles.txt
	string_t Objective2_CL; // secondary objective
	float LastSwimSound; // last time we played pl_swim sound
	float LastSwimUpSound; // last time we played a sound of watersplash when we got from underwater
	bool Submerged; // reached pev->waterlevel 3
	float ButtonFreezeTime; // the time when the player was freezed by a button
	bool IsUpsideDown; // I need this to activate certain effects when changing the state
	bool BhopEnabled;

	// weapon_drone
	bool DroneDeployed; // true if a friendly drone is present on the map
	bool DroneDeployed_CL;
	float DroneHealth;  // when we pick up our flying drone, we need to remember its health for future spawn
	int DroneHealth_CL;
	float DroneAmmo; // for player-controlled drone: max 500; float for regenerating purposes
	int DroneAmmo_CL;
	EHANDLE m_hDrone; // pointer to our drone
	bool DroneControl; // true when the player controls drone in 1st person mode
	void ManageDrone( void ); // pre-think function
	bool DroneCrosshairUpdate; // force update crosshair when switching to 1st person and back (not saved)
	int drone_forwmove, drone_sidemove, drone_upmove;
	float drone_Speed, drone_UpdateTime, drone_DirChange;
	Vector drone_currentdir;
	Vector DroneColor;
	Vector DroneColor_CL;
	bool CanUseDrone;
	bool CanUseDrone_CL;
	float DroneInfoUpdateTime;
	string_t DroneTarget_OnDeploy;
	string_t DroneTarget_OnReturn;
	string_t DroneTarget_OnEnteringFirstPerson;
	string_t DroneTarget_OnLeavingFirstPerson;
	int DroneDistance;
	int DroneDistance_CL;

	hudtextparms_t	m_DroneTextParms; // hud text for weapon_drone (shown when the drone is active)
	hudtextparms_t	m_AliceTextParms; // Alice HP shown on screen  // UNDONE decided to leave this unused right now
	void SetHUDTexts(void); // all parameters for those two are set here, activates on player spawn

	// broken suit - smash the player with electro-damage once in a while
	bool BrokenSuit;
	float NextBrokenDmgTime; // set the next time when the player will be damaged again

	float NextShieldChangeTime; // don't spam the shield button - not saved

	// so-called "inventory". It simply keeps the number (of "items") you pick up on the map
	// the items themselves are made by mapper, in Diffusion it's batteries and crystals, for example.
	// these values are used by player_counter_add and player_counter_trigger entities
	int CountSlot[5]; // 5 slots for now

	virtual void CheckTutorMessage( int m_iId ); // diffusion - show a tutorial sprite if player picks up a weapon
	float m_flLastTimeTutorWasShown; // to prevent spamming the tutors
	// we need to remember the weapons we had throughout the playthrough - don't show the tutor again
	bool HadWeapon[MAX_WEAPONS];

	int ConfirmedHit; // an indicator to display a hit marker on the crosshair - 1: hit, 2: killed
	int DamageDealt; // show on screen how much damage player made to enemy

	char CameraEntity[128]; // if player is using a trigger_camera, copy its name here.
							// if this field changes (player switched to another camera), the camera will deactivate 
	Vector CameraOrigin; // origin and angles of the said camera
	Vector CameraAngles;

	int ZoomState; // 0 reset, 1-2-etc are zoom states. utilized by weapons (crossbow etc.)

	float SpawnProtectTime; // the time in the future where we disable spawn protection (mp only)
	bool IsSpawnProtected;
	float HUDtextTime; // last time hud text was shown ("press x to respawn and such" - too much network spam...)

	CBaseEntity *m_pFlashlightMonster; // pointer to flashlight monster
	void CreateFlashlightMonster( void ); // recreate it after changelevel, or first spawn

	Vector NotInWaterVelocity; // copy of the velocity when waterlevel == 0. Need this to calculate damage when hitting water.
								// equals g_vecZero when waterlevel > 0           P.S. not saved!

	float m_flLastWeaponSwitchTime; // no spam pls

	int KillingSpreeLevel;
	int ConsecutiveKills;
	int ConsecutiveKillLevel;
	int PrevConsecutiveKillLevel;
	float LastConsecutiveKillTime; // for doublekill, multikill etc
	void KillingSpreeSounds( void );
	void ConsecutiveKillSounds( void );
	float SpreeCheckTime;
	

	// ===== ACHIEVEMENTS =====
	bool bCheatsWereUsed;
	void CheckVehicleAchievement( void );
	float AchievementCheckTime; // last time we checked for a completed achievement (this is so few ach-s won't activate at the same time)
	float AchievementStats[TOTAL_ACHIEVEMENTS]; // here we keep pure stats of what the player did and then compare them to goals
	void SendAchievementStatToClient( int AchNum, int Value, int Mode );

	// electro-blast ability
	void ManageElectroBlast( void );
	int BlastAbilityLVL; // 0-3: how many time player is able to blast
	int m_iClientBlastAbilityLVL;
	int BlastChargesReady; // "ammo" - how many blasts are available right now
	int m_iClientBlastChargesReady;
	float LastBlastTime; // only blast once in 3 seconds
	bool BlastDMGOverride; // need to disable the damage during blast to not hurt ourselves

	int ShieldAvailableLVL; // 0-3 levels

	// car stuff xD
	CCar* pCar; // my active car
	bool InCar; // true if driving car
	void ManageCar( void ); // post-think function managing the active car

	int healthbarscache[4];

	bool LoudWeaponsRestricted;
	void FireLoudWeaponRestrictionEntity( void );

	Vector m_vLookAtTarget;
	void LookAtPlayers( void );
	float headyaw;

	int cached_weapon_id; // hack for SetAnimation!!!
	int cached_waterlevel; // hack for SetAnimation!!!

	float gtbdPrev; // for CheckTimeBasedDamage()

	EHANDLE m_hKiller;
	float fLerpToKiller;
};

#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669

enum
{
	SPREE_SND_KILLINGSPREE = 1,
	SPREE_SND_DOMINATING,
	SPREE_SND_RAMPAGE,
	SPREE_SND_GODLIKE,
	SPREE_SND_WICKEDSICK,
	SPREE_SND_HEADSHOT,
	SPREE_SND_SPREE_ENDED,
	SPREE_SND_DOUBLEKILL,
	SPREE_SND_TRIPLEKILL,
	SPREE_SND_MULTIKILL,
	SPREE_SND_MEGAKILL,
	SPREE_SND_ULTRAKILL,
	SPREE_SND_MONSTERKILL,
};

extern BOOL gInitHUD;

#endif // PLAYER_H
