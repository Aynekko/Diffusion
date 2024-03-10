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
#ifndef TRAINS_H
#define TRAINS_H

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH		0x0001
#define SF_TRACKTRAIN_NOCONTROL	0x0002
#define SF_TRACKTRAIN_FORWARDONLY	0x0004
#define SF_TRACKTRAIN_PASSABLE	0x0008
#define SF_TRACKTRAIN_NO_FIRE_ON_PASS	0x0010
#define SF_TRACKTRAIN_UNBLOCKABLE	0x0020
#define SF_TRACKTRAIN_SPEEDBASED_PITCH	0x0040

// Spawnflag for CPathTrack
#define SF_PATH_DISABLED		0x00000001
#define SF_PATH_FIREONCE		0x00000002
#define SF_PATH_ALTREVERSE		0x00000004
#define SF_PATH_DISABLE_TRAIN		0x00000008
#define SF_PATH_TELEPORT		0x00000010
#define SF_PATH_ALTERNATE		0x00008000

// Spawnflags of CPathCorner
#define SF_CORNER_WAITFORTRIG		0x001
#define SF_CORNER_TELEPORT		0x002
#define SF_CORNER_FIREONCE		0x004

#define SF_PLAT_TOGGLE		BIT( 0 )

// func_train
#define SF_TRAIN_WAIT_RETRIGGER	BIT( 0 )
#define SF_TRAIN_SETORIGIN		BIT( 1 )
#define SF_TRAIN_START_ON		BIT( 2 )	// Train is initially moving
#define SF_TRAIN_PASSABLE		BIT( 3 )	// Train is not solid -- used to make water trains

#define SF_TRAIN_REVERSE		BIT( 31 )	// hidden flag

enum TrainVelocityType_t
{
        TrainVelocity_Instantaneous = 0,
        TrainVelocity_LinearBlend,
        TrainVelocity_EaseInEaseOut,
};

enum TrainOrientationType_t
{
        TrainOrientation_Fixed = 0,
        TrainOrientation_AtPathTracks,
        TrainOrientation_LinearBlend,
        TrainOrientation_EaseInEaseOut,
};

enum TrackOrientationType_t
{
	TrackOrientation_Fixed = 0,
	TrackOrientation_FacePath,
	TrackOrientation_FacePathAngles,
};

class CFuncTrain;

// train sequence
#define DIRECTION_NONE		0
#define DIRECTION_FORWARDS		1
#define DIRECTION_BACKWARDS		2
#define DIRECTION_STOP		3
#define DIRECTION_DESTINATION		4

#define SF_TRAINSEQ_REMOVE		BIT( 1 )
#define SF_TRAINSEQ_DIRECT		BIT( 2 )
#define SF_TRAINSEQ_DEBUG		BIT( 3 )

class CTrainSequence : public CBaseEntity
{
	DECLARE_CLASS( CTrainSequence, CBaseEntity );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	STATE GetState( void ) { return (m_pTrain) ? STATE_ON : STATE_OFF; }

	void EndThink( void );
	void StopSequence( void );
	void ArrivalNotify( void );

	DECLARE_DATADESC();

	string_t		m_iszEntity;
	string_t		m_iszDestination;
	string_t		m_iszTerminate;
	int		m_iDirection;

	CFuncTrain *m_pTrain;
	CBaseEntity *m_pDestination;
};

//#define PATH_SPARKLE_DEBUG		// This makes a particle effect around path_track entities for debugging
class CPathTrack : public CPointEntity
{
	DECLARE_CLASS( CPathTrack, CPointEntity );
public:
	CPathTrack();

	void		Spawn( void );
	void		Activate( void );
	void		KeyValue( KeyValueData* pkvd);
	STATE		GetState ( void );
	
	void		SetPrevious( CPathTrack *pprevious );
	void		Link( void );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CPathTrack	*ValidPath( CPathTrack *ppath, int testFlag = true ); // Returns ppath if enabled, NULL otherwise
	void		Project( CPathTrack *pstart, CPathTrack *pend, Vector &origin, float dist );
	int		GetOrientationType( void ) { return m_eOrientationType; }

	Vector		GetOrientation( bool bForwardDir );
	CPathTrack	*GetNextInDir( bool bForward );

	static CPathTrack *Instance( edict_t *pent );

	CPathTrack	*LookAhead( Vector &origin, float dist, int move, CPathTrack **pNextNext = NULL );
	CPathTrack	*Nearest( const Vector &origin );

	CPathTrack	*GetNext( void );
	CPathTrack	*GetPrevious( void );

	DECLARE_DATADESC();

#ifdef PATH_SPARKLE_DEBUG
	void 		Sparkle(void);
#endif
	float		m_length;
	string_t		m_altName;
	CPathTrack	*m_pnext;
	CPathTrack	*m_pprevious;
	CPathTrack	*m_paltpath;
	int		m_eOrientationType;

	string_t		m_iszFireFow;
	string_t		m_iszFireRev;
};

class CBasePlatTrain : public CBaseToggle
{
	DECLARE_CLASS( CBasePlatTrain, CBaseToggle );
public:
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );
	void Precache( void );

	// This is done to fix spawn flag collisions between this class and a derived class
	virtual BOOL IsTogglePlat( void ) { return (pev->spawnflags & SF_PLAT_TOGGLE) ? TRUE : FALSE; }

	DECLARE_DATADESC();

	int	m_iMoveSnd;	// sound a plat makes while moving
	int	m_iStopSnd;	// sound a plat makes when it stops
	float	m_volume;		// Sound volume
	float	m_flFloor;	// Floor number
};

class CFuncTrain : public CBasePlatTrain
{
	DECLARE_CLASS( CFuncTrain, CBasePlatTrain );
public:
	void Spawn( void );
	void Precache( void );
	void Activate( void );
	void OverrideReset( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE GetState( void ) { return m_iState; }

	Vector CalcPosition( CBaseEntity *pTarg )
	{
		Vector nextPos = pTarg->GetLocalOrigin();
		if( !FBitSet( pev->spawnflags, SF_TRAIN_SETORIGIN ) )
			nextPos -= (pev->mins + pev->maxs) * 0.5f;
		return nextPos;
	}

	void Wait( void );
	void Next( void );
	void SoundSetup( void );
	void Stop( void );

	void StartSequence( CTrainSequence *pSequence );
	void StopSequence( void );

	void ClearEffects( void );

	DECLARE_DATADESC();

	CTrainSequence *m_pSequence;
	EHANDLE		m_hCurrentTarget;
	BOOL		m_activated;
};

class CBaseTrainDoor;

class CFuncTrackTrain : public CBaseDelay
{
	DECLARE_CLASS( CFuncTrackTrain, CBaseDelay );
public:
	CFuncTrackTrain();

	void Spawn( void );
	void Precache( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* pkvd );

	void Next( void );
	void Find( void );
	void NearestPath( void );
	void DeadEnd( void );

	void Stop( void );
	void ClearEffects( void );

	bool IsDirForward() { return ( m_dir == 1 ); }
	void SetDirForward( bool bForward );
	void SetSpeed( float flSpeed, float flAccel = 0.0f );
	void SetSpeedExternal( float flSpeed );

	void SetTrainDoor( CBaseTrainDoor *pDoor ) { m_pDoor = pDoor; }
	void SetTrack( CPathTrack *track ) { m_ppath = track->Nearest( GetLocalOrigin( )); }
	void SetControls( CBaseEntity *pControls );
	BOOL OnControls( CBaseEntity *pTest );

	void StopSound ( void );
	void UpdateSound ( void );
	
	static CFuncTrackTrain *Instance( edict_t *pent );
	void TeleportToPathTrack( CPathTrack *pTeleport );
	void UpdateTrainVelocity( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateTrainOrientation( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationAtPathTracks( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationBlend( int eOrientationType, CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval );
	void DoUpdateOrientation( const Vector &curAngles, const Vector &angles, float flInterval );
	float GetSpeed( void ) { return m_flDesiredSpeed; }
	float GetMaxSpeed( void ) { return m_maxSpeed; }
	void SetMaxSpeed( float fNewSpeed ) { m_maxSpeed = fNewSpeed; }

	DECLARE_DATADESC();

	virtual STATE	GetState( void ) { return (pev->speed != 0) ? STATE_ON : STATE_OFF; }
	virtual int	ObjectCaps( void )
	{
		return (CBaseDelay :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DIRECTIONAL_USE | FCAP_HOLD_ANGLES;
	}

	void		ArriveAtNode( CPathTrack *pNode );
	virtual void	OverrideReset( void );
	void		UpdateOnRemove( void );

	CBaseTrainDoor	*m_pDoor;
	CPathTrack	*m_ppath;
	CBaseEntity	*m_pSpeedControl;
	float		m_length;
	float		m_height;
	float		m_maxSpeed;
	float		m_startSpeed;
	Vector		m_controlMins;
	Vector		m_controlMaxs;
	Vector		m_controlOrigin;
	int		m_soundPlaying;
	int		m_sounds;
	int		m_soundStart;
	int		m_soundStop;
	float		m_flVolume;
	float		m_flBank;
	float		m_oldSpeed;
	float		m_dir;
	int		m_eOrientationType;
	int		m_eVelocityType;

	float		m_flDesiredSpeed;
	float		m_flAccelSpeed;
	float		m_flReachedDist;
};

typedef enum
{
	TD_CLOSED,
	TD_SHIFT_UP,
	TD_SLIDING_UP,
	TD_OPENED,
	TD_SLIDING_DOWN,
	TD_SHIFT_DOWN
} TRAINDOOR_STATE;

#define SF_TRAINDOOR_INVERSE			BIT( 0 )
#define SF_TRAINDOOR_OPEN_IN_MOVING		BIT( 1 )
#define SF_TRAINDOOR_ONOFF_MODE		BIT( 2 )

class CBaseTrainDoor : public CBaseToggle
{
	DECLARE_CLASS( CBaseTrainDoor, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Blocked( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return ((CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR; }
	virtual bool IsDoorControl( void ) { return !FBitSet( pev->spawnflags, SF_TRAINDOOR_OPEN_IN_MOVING ) && GetState() != STATE_OFF; }
	void ClearEffects( void );

	DECLARE_DATADESC();

	// local functions
	void FindTrain( void );
	void DoorGoUp( void );
	void DoorGoDown( void );
	void DoorHitTop( void );
	void DoorSlideUp( void );
	void DoorSlideDown( void );
	void DoorSlideWait( void );		// wait before sliding
	void DoorHitBottom( void );
	void ActivateTrain( void );
	
	int		m_iMoveSnd;	// sound a door makes while moving
	int		m_iStopSnd;	// sound a door makes when it stops

	CFuncTrackTrain	*m_pTrain;	// my train pointer
	Vector		m_vecOldAngles;

	TRAINDOOR_STATE	door_state;
	virtual void	OverrideReset( void );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE	GetState ( void );
};

#endif//TRAINS_H