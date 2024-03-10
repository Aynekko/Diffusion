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
#ifndef DOORS_H
#define DOORS_H

// doors
#define SF_DOOR_START_OPEN			BIT( 0 )
#define SF_DOOR_ROTATE_BACKWARDS		BIT( 1 )
#define SF_DOOR_ONOFF_MODE			BIT( 2 )	// was Door don't link from quake. This feature come from Spirit. Thx Laurie
#define SF_DOOR_PASSABLE			BIT( 3 )
#define SF_DOOR_ONEWAY			BIT( 4 )
#define SF_DOOR_NO_AUTO_RETURN		BIT( 5 ) // toggle
#define SF_DOOR_ROTATE_Z			BIT( 6 )
#define SF_DOOR_ROTATE_X			BIT( 7 )
#define SF_DOOR_USE_ONLY			BIT( 8 )	// door must be opened by player's use button.
#define SF_DOOR_NOMONSTERS			BIT( 9 )	// monster can't open
#define SF_DOOR_FORCETOUCHABLE		BIT( 10 ) //LRC- Opens when touched, even though it's named and/or "use only"       // diffusion - taken from Spirit of HL. Thank you! ^^
#define SF_DOOR_AUTOMATED			BIT( 11 ) // diffusion - my proudest achievement so far!!! LOL
#define SF_DOOR_SPECIALEFFECTS		BIT( 12 ) // diffusion - this is cool!
#define SF_DOOR_2NDMOVEMENT			BIT( 14 ) // diffusion - after door has opened, it will open again in different direction

#define SF_DOOR_SILENT			BIT( 13 ) // diffusion - was BIT 31. Why?

class CBaseDoor : public CBaseToggle
{
	DECLARE_CLASS( CBaseDoor, CBaseToggle );
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int	GetDoorMovementGroup( CBaseDoor *pDoorList[], int listMax );
	void	Blocked( CBaseEntity *pOther );
	void	OnChangeParent( void );
	void	OnClearParent( void );
	void	Activate( void );

	void	DoorOpenThink( void ); //diffusion new stuff
	void	DoorCloseThink( void );

	virtual int ObjectCaps( void )
	{
		if( HasSpawnFlags( SF_DOOR_USE_ONLY ) )
			return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_IMPULSE_USE | FCAP_SET_MOVEDIR;
		return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR;
	};

	DECLARE_DATADESC();

	virtual void SetToggleState( int state );
	virtual bool IsRotatingDoor() { return false; }
	virtual STATE GetState( void ) { return m_iState; };

	BOOL DoorActivate( void );
	void DoorGoUp( void );
	void DoorGoDown( void );
	void DoorHitTop( void );
	void DoorHitBottom( void );
	void DoorTouch( CBaseEntity *pOther );
	void ChainTouch( CBaseEntity *pOther );
	void ChainUse( USE_TYPE useType, float value );
	void ClearEffects( void );

	byte	m_bHealthValue;		// some doors are medi-kit doors, they give players health
	int	m_iMoveSnd;		// sound a door makes while moving
	int	m_iStopSnd;		// sound a door makes when it stops

	locksound_t m_ls;			// door lock sounds

	int	m_iLockedSound;		// ordinals from entity selection
	byte	m_bLockedSentence;
	int	m_iUnlockedSound;
	byte	m_bUnlockedSentence;

	string_t	m_iChainTarget;		// feature come from hl2
	bool	m_bDoorGroup;
	bool	m_bDoorTouched;		// don't save\restore this

	// diffusion additions:
	int AutoDoorRadius; // used with AUTOMATED experimental flag
	float AutoTime; // each [AutoTime] seconds door is checking the radius around it to see if somebody is here
	float CustomOpenSpeed; // custom opening speed
	float CustomCloseSpeed; // custom closing speed
	Vector MoveDir2; // secondary movement direction
	int FirstMovement; // we are doing 1st move, so don't call MoveDone just yet
	Vector m_vecPosition3; // third position of the door
	void SecondaryLinearMove( void );
	float SecondaryMoveSpeedScale; // speed of the 2nd movement is speed * scale
	float m_flLip2; // lip of the second movement
	float m_flAttenuation; // door sound radius
};

#endif//DOORS_H