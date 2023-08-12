#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "triggers.h"

/*QUAKED trigger_multiple (.5 .5 .5) ? notouch
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "health" is set, the trigger must be killed to activate each time.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
If notouch is set, the trigger is only fired by other entities, not by touching.
NOTOUCH has been obsoleted by trigger_relay!
sounds
1)      secret
2)      beep beep
3)      large switch
4)
NEW
if a trigger has a NETNAME, that NETNAME will become the TARGET of the triggered object.
*/
class CTriggerMultiple : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerMultiple, CBaseTrigger);
public:
	void Precache( void );
	void Spawn(void);
	STATE GetState(void) { return (pev->nextthink > gpGlobals->time) ? STATE_OFF : STATE_ON; }
};

LINK_ENTITY_TO_CLASS(trigger_multiple, CTriggerMultiple);

void CTriggerMultiple::Precache(void)
{
	if( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CTriggerMultiple::Spawn(void)
{
	Precache();
	
	if (m_flWait == 0)
		m_flWait = 0.2;

	InitTrigger();
	SetTouch(&CBaseTrigger::MultiTouch);
}

/*QUAKED trigger_once (.5 .5 .5) ? notouch
Variable sized trigger. Triggers once, then removes itself.  You must set the key "target" to the name of another object in the level that has a matching
"targetname".  If "health" is set, the trigger must be killed to activate.
If notouch is set, the trigger is only fired by other entities, not by touching.
if "killtarget" is set, any objects that have a matching "target" will be removed when the trigger is fired.
if "angle" is set, the trigger will only fire when someone is facing the direction of the angle.  Use "360" for an angle of 0.
sounds
1)      secret
2)      beep beep
3)      large switch
4)
*/
class CTriggerOnce : public CTriggerMultiple
{
	DECLARE_CLASS(CTriggerOnce, CTriggerMultiple);
public:
	void Spawn(void);
};

LINK_ENTITY_TO_CLASS(trigger_once, CTriggerOnce);

void CTriggerOnce::Spawn(void)
{	
	m_flWait = -1;

	BaseClass::Spawn();
}