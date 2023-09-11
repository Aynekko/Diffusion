#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "triggers.h"

/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.
If nomessage is not set, it will print "1 more.. " etc when triggered and
"sequence complete" when finished.  After the counter has been triggered "cTriggersLeft"
times (default 2), it will fire all of it's targets and remove itself.
*/
class CTriggerCounter : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerCounter, CBaseTrigger);
public:
	void Spawn(void);
};
LINK_ENTITY_TO_CLASS(trigger_counter, CTriggerCounter);

void CTriggerCounter::Spawn(void)
{
	// By making the flWait be -1, this counter-trigger will disappear after it's activated
	// (but of course it needs cTriggersLeft "uses" before that happens).
	m_flWait = -1;

	if (m_cTriggersLeft == 0)
		m_cTriggersLeft = 2;
	SetUse(&CBaseTrigger::CounterUse);
}

void CBaseTrigger::CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_cTriggersLeft--;
	m_hActivator = pActivator;

	if( m_cTriggersLeft < 0 )
		return;

	bool fTellActivator = (m_hActivator != 0) && m_hActivator->IsPlayer() && !HasSpawnFlags( SPAWNFLAG_NOMESSAGE );

	if( m_cTriggersLeft != 0 )
	{
		if( fTellActivator )
		{
			// UNDONE: I don't think we want these Quakesque messages
			switch( m_cTriggersLeft )
			{
			case 1:		ALERT( at_console, "Only 1 more to go..." );		break;
			case 2:		ALERT( at_console, "Only 2 more to go..." );		break;
			case 3:		ALERT( at_console, "Only 3 more to go..." );		break;
			default:	ALERT( at_console, "There are more to go..." );	break;
			}
		}
		return;
	}

	// !!!UNDONE: I don't think we want these Quakesque messages
	if( fTellActivator )
		ALERT( at_console, "Sequence completed!" );

	ActivateMultiTrigger( m_hActivator );
}