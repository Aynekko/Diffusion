#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/skill.h"
#include "triggers.h"

//=======================================================================================================================
// diffusion - starts a desired timer when triggered (visible on HUD with a message)
// triggers the target after timer runs out
// iuser1: default timer in seconds
// iuser2: custom timer for medium skill
// iuser3: custom timer for hard skill
//=======================================================================================================================

#define TRTIMER_REMOVEONFIRE BIT(0)

class CTriggerTimer : public CBaseDelay
{
	DECLARE_CLASS( CTriggerTimer, CBaseDelay );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void CountDown( void );
	void ClearEffects( void );
	void EnableTimer( void );
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger_timer, CTriggerTimer );

BEGIN_DATADESC( CTriggerTimer )
DEFINE_FUNCTION( CountDown ),
END_DATADESC();

void CTriggerTimer::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ) )
		return;

	if( pev->iuser1 <= 0 )
		ALERT( at_error, "trigger_timer \"%s\" doesn't have timer set.\n", GetTargetname() );

	if( useType == USE_OFF )
	{
		if( m_iState == STATE_ON )
		{
			DontThink();
			ClearEffects();
		}
	}
	else if( useType == USE_ON )
	{
		if( m_iState == STATE_OFF )
			EnableTimer();
	}
	else if( useType == USE_RESET )
	{
		EnableTimer();
	}
	else // toggle
	{
		if( m_iState == STATE_OFF )
		{
			EnableTimer();
		}
		else // disable timer
		{
			DontThink();
			ClearEffects();
		}
	}
}

void CTriggerTimer::EnableTimer( void )
{
	if( g_iSkillLevel == SKILL_MEDIUM && pev->iuser2 > 0 )
		m_iCounter = pev->iuser2;
	else if( g_iSkillLevel == SKILL_HARD && pev->iuser3 > 0 )
		m_iCounter = pev->iuser3;
	else
		m_iCounter = pev->iuser1;

	m_flWait = m_iCounter * 0.2f; // remember the critical time here

	m_iState = STATE_ON;
	SetThink( &CTriggerTimer::CountDown );
	SetNextThink( 0 );
}

void CTriggerTimer::CountDown( void )
{
	if( m_iCounter <= 0 )
	{
		UTIL_FireTargets( pev->target, this, this, USE_TOGGLE, 0 );
		// disable timer HUD
		DontThink();
		ClearEffects();
		if( HasSpawnFlags( TRTIMER_REMOVEONFIRE ) )
			UTIL_Remove( this );
		return;
	}
	
	// update HUD
	MESSAGE_BEGIN( MSG_ALL, gmsgTempEnt );
	WRITE_BYTE( TE_TRIGGERTIMER );
	WRITE_BYTE( (m_iCounter <= (int)m_flWait) ? 2 : 1 ); // 1 - enabled, 2 - critical
	WRITE_STRING( pev->message ? STRING( pev->message ) : 0 );
	WRITE_SHORT( m_iCounter );
	MESSAGE_END();

	m_iCounter--;

	SetNextThink( 1.0f );
}

void CTriggerTimer::ClearEffects( void )
{
	// make sure timer is disabled if the entity is deleted from world
	MESSAGE_BEGIN( MSG_ALL, gmsgTempEnt );
	WRITE_BYTE( TE_TRIGGERTIMER );
	WRITE_BYTE( 0 );
	MESSAGE_END();
	m_iState = STATE_OFF;
}