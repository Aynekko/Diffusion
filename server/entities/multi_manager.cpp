#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/saverestore.h"
#include "game/gamerules.h"
#include "entities/trains.h"			// trigger_camera has train functionality
#include "game/game.h"
#include "talkmonster.h"
#include "weapons/weapons.h"
#include "triggers.h"

//**********************************************************
// The Multimanager Entity - when fired, will fire up to 16 targets 
// at specified times.
// FLAG:		THREAD (create clones when triggered)
// FLAG:		CLONE (this is a clone for a threaded execution)

#define SF_MULTIMAN_CLONE		0x80000000
#define SF_MULTIMAN_THREAD		BIT( 0 )
// used on a Valve maps
#define SF_MULTIMAN_LOOP		BIT( 2 )
#define SF_MULTIMAN_ONLYONCE		BIT( 3 )
#define SF_MULTIMAN_START_ON		BIT( 4 )	// same as START_ON

class CMultiManager : public CBaseDelay
{
	DECLARE_CLASS(CMultiManager, CBaseDelay);
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void ManagerThink(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
#if _DEBUG
	void ManagerReport(void);
#endif
	BOOL HasTarget(string_t targetname);
	int ObjectCaps(void) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_NOT_MASTER; }

	DECLARE_DATADESC();

	int	m_cTargets;			// the total number of targets in this manager's fire list.
	int	m_index;				// Current target
	float	m_startTime;			// Time we started firing
	int	m_iTargetName[MAX_MULTI_TARGETS];	// list if indexes into global string array
	float	m_flTargetDelay[MAX_MULTI_TARGETS];	// delay (in seconds) from time of manager fire to target fire
private:
	inline BOOL IsClone(void)
	{
		return (pev->spawnflags & SF_MULTIMAN_CLONE) ? TRUE : FALSE;
	}

	inline BOOL ShouldClone(void)
	{
		if (IsClone())
			return FALSE;
		return (FBitSet(pev->spawnflags, SF_MULTIMAN_THREAD)) ? TRUE : FALSE;
	}

	CMultiManager* Clone(void);
};

LINK_ENTITY_TO_CLASS(multi_manager, CMultiManager);

// Global Savedata for multi_manager
BEGIN_DATADESC(CMultiManager)
	DEFINE_FIELD(m_cTargets, FIELD_INTEGER),
	DEFINE_FIELD(m_index, FIELD_INTEGER),
	DEFINE_FIELD(m_startTime, FIELD_TIME),
	DEFINE_ARRAY(m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY(m_flTargetDelay, FIELD_FLOAT, MAX_MULTI_TARGETS ),
	DEFINE_FUNCTION(ManagerThink),
#if _DEBUG
	DEFINE_FUNCTION(ManagerReport),
#endif
END_DATADESC()

void CMultiManager::KeyValue(KeyValueData* pkvd)
{
	// UNDONE: Maybe this should do something like this:
	// CBaseToggle::KeyValue( pkvd );
	// if ( !pkvd->fHandled )
	// ... etc.

	if (FStrEq(pkvd->szKeyName, "delay"))
	{
		m_flDelay = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "parent") || FStrEq(pkvd->szKeyName, "movewith"))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if (m_cTargets < MAX_MULTI_TARGETS)
		{
			char tmp[128];

			UTIL_StripToken(pkvd->szKeyName, tmp);
			m_iTargetName[m_cTargets] = ALLOC_STRING(tmp);
			m_flTargetDelay[m_cTargets] = Q_atof(pkvd->szValue);
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

void CMultiManager::Spawn(void)
{
	SetThink(&CMultiManager::ManagerThink);

	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while (swapped)
	{
		swapped = 0;

		for (int i = 1; i < m_cTargets; i++)
		{
			if (m_flTargetDelay[i] < m_flTargetDelay[i - 1])
			{
				// swap out of order elements
				int name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				m_iTargetName[i] = m_iTargetName[i - 1];
				m_flTargetDelay[i] = m_flTargetDelay[i - 1];
				m_iTargetName[i - 1] = name;
				m_flTargetDelay[i - 1] = delay;
				swapped = 1;
			}
		}
	}

	// HACKHACK: fix env_laser on a c2a1
	if (FStrEq(STRING(gpGlobals->mapname), "c2a1") && FStrEq(GetTargetname(), "gargbeams_mm"))
		pev->spawnflags = 0;

	if (FBitSet(pev->spawnflags, SF_MULTIMAN_START_ON))
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.1);
	}

	m_iState = STATE_OFF;
}

BOOL CMultiManager::HasTarget(string_t targetname)
{
	for (int i = 0; i < m_cTargets; i++)
	{
		if (FStrEq(STRING(targetname), STRING(m_iTargetName[i])))
			return TRUE;
	}
	return FALSE;
}

// Designers were using this to fire targets that may or may not exist -- 
// so I changed it to use the standard target fire code, made it a little simpler.
void CMultiManager::ManagerThink(void)
{
	float	time;

	time = gpGlobals->time - m_startTime;
	while (m_index < m_cTargets && m_flTargetDelay[m_index] <= time)
	{
		UTIL_FireTargets(STRING(m_iTargetName[m_index]), m_hActivator, this, USE_TOGGLE, pev->frags);
		m_index++;
	}

	// have we fired all targets?
	if (m_index >= m_cTargets)
	{
		if (FBitSet(pev->spawnflags, SF_MULTIMAN_LOOP))
		{
			// starts new cycle
			m_startTime = m_flDelay + gpGlobals->time;
			m_iState = STATE_TURN_ON;
			SetNextThink(m_flDelay);
			SetThink(&CMultiManager::ManagerThink);
			m_index = 0;
		}
		else
		{
			m_iState = STATE_OFF;
			SetThink(NULL);
			DontThink();
		}

		if (IsClone() || FBitSet(pev->spawnflags, SF_MULTIMAN_ONLYONCE))
		{
			UTIL_Remove(this);
			return;
		}
	}
	else
	{
		pev->nextthink = m_startTime + m_flTargetDelay[m_index];
	}
}

CMultiManager* CMultiManager::Clone(void)
{
	CMultiManager* pMulti = GetClassPtr((CMultiManager*)NULL);

	edict_t* pEdict = pMulti->pev->pContainingEntity;
	memcpy(pMulti->pev, pev, sizeof(*pev));
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= SF_MULTIMAN_CLONE;
	pMulti->pev->spawnflags &= ~SF_MULTIMAN_THREAD; // g-cont. to prevent recursion
	memcpy(pMulti->m_iTargetName, m_iTargetName, sizeof(m_iTargetName));
	memcpy(pMulti->m_flTargetDelay, m_flTargetDelay, sizeof(m_flTargetDelay));
	pMulti->m_cTargets = m_cTargets;

	return pMulti;
}


// The USE function builds the time table and starts the entity thinking.
void CMultiManager::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_hActivator = pActivator;
	pev->frags = value;

	if (IsLockedByMaster())
		return;

	if (useType == USE_SET)
	{
		m_index = 0; // reset fire index
		while (m_index < m_cTargets)
		{
			// firing all targets instantly
			UTIL_FireTargets(m_iTargetName[m_index], this, this, USE_TOGGLE);
			m_index++;
			if (m_hActivator == this)
				break; // break if current target - himself
		}
	}

	if (FBitSet(pev->spawnflags, SF_MULTIMAN_LOOP))
	{
		if (m_iState != STATE_OFF) // if we're on, or turning on...
		{
			if (useType != USE_ON) // ...then turn it off if we're asked to.
			{
				m_iState = STATE_OFF;
				if (IsClone() || FBitSet(pev->spawnflags, SF_MULTIMAN_ONLYONCE))
				{
					SetThink(&CBaseEntity::SUB_Remove);
					SetNextThink(0.1);
				}
				else
				{
					SetThink(NULL);
					DontThink();
				}
			}
			return;
		}
		else if (useType == USE_OFF)
		{
			return;
		}
		// otherwise, start firing targets as normal.
	}

	// In multiplayer games, clone the MM and execute in the clone (like a thread)
	// to allow multiple players to trigger the same multimanager
	if (ShouldClone())
	{
		CMultiManager* pClone = Clone();
		pClone->Use(pActivator, pCaller, useType, value);
		return;
	}

	if (ShouldToggle(useType))
	{
		if (useType == USE_ON || useType == USE_TOGGLE || useType == USE_RESET)
		{
			if (m_iState == STATE_OFF || useType == USE_RESET)
			{
				m_startTime = m_flDelay + gpGlobals->time;
				m_iState = STATE_TURN_ON;
				SetNextThink(m_flDelay);
				SetThink(&CMultiManager::ManagerThink);
				m_index = 0;
			}
		}
		else if (useType == USE_OFF)
		{
			m_iState = STATE_OFF;
			SetThink(NULL);
			DontThink();
		}
	}
}

#if _DEBUG
void CMultiManager::ManagerReport(void)
{
	for (int cIndex = 0; cIndex < m_cTargets; cIndex++)
	{
		ALERT(at_console, "%s %f\n", STRING(m_iTargetName[cIndex]), m_flTargetDelay[cIndex]);
	}
}
#endif





//===============================================================================
// diffusion - same as multi_manager but in a brush form! :)
// can be activated only once, only by a player
//===============================================================================
class CFuncMultiManager : public CMultiManager
{
	DECLARE_CLASS( CFuncMultiManager, CMultiManager );
public:
	void Spawn( void );
	void UseTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CFuncMultiManager )
	DEFINE_FUNCTION( UseTouch ),
	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( m_index, FIELD_INTEGER ),
	DEFINE_FIELD( m_startTime, FIELD_TIME ),
	DEFINE_ARRAY( m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( m_flTargetDelay, FIELD_FLOAT, MAX_MULTI_TARGETS ),
	DEFINE_FUNCTION( ManagerThink ),
#if _DEBUG
	DEFINE_FUNCTION( ManagerReport ),
#endif
END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_multimanager, CFuncMultiManager );

void CFuncMultiManager::Spawn(void)
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;

	if( pev->movedir == g_vecZero )
	{
		pev->movedir = Vector( 1, 0, 0 );
		SetLocalAngles( g_vecZero );
	}

	SET_MODEL( edict(), GetModel() );
	SetTouch( &CFuncMultiManager::UseTouch );

	BaseClass::Spawn();
}

void CFuncMultiManager::UseTouch( CBaseEntity *pOther )
{
	if( !pOther )
		return;

	if( sv_ignore_triggers.value > 0 )
		return;

	if( pOther->IsPlayer() )
	{
		Use( pOther, pOther, USE_TOGGLE, 0 );
		SetTouch( NULL );
	}
}



//===============================================================================
// diffusion - instead of targets, there are car controls
//===============================================================================
class CFuncCarScript : public CMultiManager
{
	DECLARE_CLASS( CFuncCarScript, CMultiManager );
public:
	void Spawn( void );
	void ManagerThink( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void FindCar( void );

	CCar *pCar;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger_car_script, CFuncCarScript );

BEGIN_DATADESC( CFuncCarScript )
	DEFINE_FIELD( pCar, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( FindCar ),
	DEFINE_FUNCTION( ManagerThink ),
END_DATADESC()

void CFuncCarScript::Spawn( void )
{
	SetThink( &CFuncCarScript::ManagerThink );

	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while( swapped )
	{
		swapped = 0;

		for( int i = 1; i < m_cTargets; i++ )
		{
			if( m_flTargetDelay[i] < m_flTargetDelay[i - 1] )
			{
				// swap out of order elements
				int name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				m_iTargetName[i] = m_iTargetName[i - 1];
				m_flTargetDelay[i] = m_flTargetDelay[i - 1];
				m_iTargetName[i - 1] = name;
				m_flTargetDelay[i - 1] = delay;
				swapped = 1;
			}
		}
	}

	m_iState = STATE_OFF;

	SetThink( &CFuncCarScript::FindCar );
	SetNextThink( 1 );
}

void CFuncCarScript::FindCar(void)
{
	pCar = (CCar*)UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );

	if( !pCar )
	{
		ALERT( at_error, "trigger_car_script \"%s\" can't find specified vehicle! Removed.\n", STRING( pev->targetname ) );
		UTIL_Remove( this );
		return;
	}
	
	if( HasSpawnFlags(SF_MULTIMAN_START_ON) )
	{
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
}

void CFuncCarScript::KeyValue( KeyValueData *pkvd )
{
	// UNDONE: Maybe this should do something like this:
	// CBaseToggle::KeyValue( pkvd );
	// if ( !pkvd->fHandled )
	// ... etc.

	if( FStrEq( pkvd->szKeyName, "delay" ) )
	{
		m_flDelay = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ) )
	{
		m_iParent = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "target" ) )
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];

			UTIL_StripToken( pkvd->szKeyName, tmp );
			m_iTargetName[m_cTargets] = ALLOC_STRING( tmp );
			m_flTargetDelay[m_cTargets] = Q_atof( pkvd->szValue );
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

void CFuncCarScript::ManagerThink( void )
{
	if( !pCar )
	{
		m_iState = STATE_OFF;
		SetThink( NULL );
		DontThink();
		return;
	}
		
	float time;

	time = gpGlobals->time - m_startTime;
	while( m_index < m_cTargets && m_flTargetDelay[m_index] <= time )
	{
	//	UTIL_FireTargets( STRING( m_iTargetName[m_index] ), m_hActivator, this, USE_TOGGLE, pev->frags );
		if( FStrEq( STRING( m_iTargetName[m_index] ), "left-" ) )
			pev->button &= ~IN_MOVELEFT;
		else if( FStrEq( (char*)STRING( m_iTargetName[m_index] ), "left+" ) )
			pev->button |= IN_MOVELEFT;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "right-" ) )
			pev->button &= ~IN_MOVERIGHT;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "right+" ) )
			pev->button |= IN_MOVERIGHT;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "forward-" ) )
			pev->button &= ~IN_FORWARD;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "forward+" ) )
			pev->button |= IN_FORWARD;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "back-" ) )
			pev->button &= ~IN_BACK;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "back+" ) )
			pev->button |= IN_BACK;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "up-" ) )
			pev->button &= ~IN_JUMP;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "up+" ) )
			pev->button |= IN_JUMP;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "down-" ) )
			pev->button &= ~IN_DUCK;
		else if( FStrEq( (char *)STRING( m_iTargetName[m_index] ), "down+" ) )
			pev->button |= IN_DUCK;

		m_index++;
	}

	// have we fired all targets?
	if( m_index >= m_cTargets )
	{
		if( HasSpawnFlags(SF_MULTIMAN_LOOP) )
		{
			// starts new cycle
			m_startTime = m_flDelay + gpGlobals->time;
			m_iState = STATE_TURN_ON;
			SetNextThink( m_flDelay );
			SetThink( &CFuncCarScript::ManagerThink );
			m_index = 0;
			pev->button = 0;
		}
		else
		{
			m_iState = STATE_OFF;
			pev->button = 0;
			if( pCar )
				pCar->hDriver = NULL;
			SetThink( NULL );
			DontThink();
		}

		if( HasSpawnFlags(SF_MULTIMAN_ONLYONCE) )
		{
			UTIL_Remove( this );
			return;
		}
	}
	else
	{
		pev->nextthink = m_startTime + m_flTargetDelay[m_index];
	}
}

void CFuncCarScript::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	pev->frags = value;

	if( IsLockedByMaster() )
		return;

	if( HasSpawnFlags(SF_MULTIMAN_LOOP) )
	{
		if( m_iState != STATE_OFF ) // if we're on, or turning on...
		{
			if( useType != USE_ON ) // ...then turn it off if we're asked to.
			{
				m_iState = STATE_OFF;
				if( HasSpawnFlags( SF_MULTIMAN_ONLYONCE) )
				{
					SetThink( &CBaseEntity::SUB_Remove );
					SetNextThink( 0.1 );
				}
				else
				{
					SetThink( NULL );
					DontThink();
				}
			}
			return;
		}
		else if( useType == USE_OFF )
		{
			return;
		}
		// otherwise, start firing targets as normal.
	}

	if( ShouldToggle( useType ) )
	{
		if( useType == USE_ON || useType == USE_TOGGLE || useType == USE_RESET )
		{
			if( m_iState == STATE_OFF || useType == USE_RESET )
			{
				m_startTime = m_flDelay + gpGlobals->time;
				m_iState = STATE_TURN_ON;
				SetNextThink( m_flDelay );
				SetThink( &CFuncCarScript::ManagerThink );
				m_index = 0;
				// posess the car
				if( pCar != NULL )
				{
					if( FClassnameIs( pCar, "func_helicopter"))
						pCar->SetThink( &CHelicopter::Drive );
					else if( FClassnameIs( pCar, "func_boat" ) )
						pCar->SetThink( &CBoat::Drive );
					else
						pCar->SetThink( &CCar::Drive );
					
					pCar->hDriver = this;
					pCar->SetNextThink( 0 );
				}
			}
		}
		else if( useType == USE_OFF )
		{
			m_iState = STATE_OFF;
			pev->button = 0;
			if( pCar )
				pCar->hDriver = NULL;
			SetThink( NULL );
			DontThink();
		}
	}
}