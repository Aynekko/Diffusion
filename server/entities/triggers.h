#pragma once

// triggers
#define SF_TRIGGER_ALLOWMONSTERS	1	// monsters allowed to fire this trigger
#define SF_TRIGGER_NOCLIENTS		2	// players not allowed to fire this trigger
#define SF_TRIGGER_PUSHABLES		4	// only pushables can fire this trigger
#define SF_TRIGGER_CHECKANGLES	8	// Quake style triggers - fire when activator angles matched with trigger angles
#define SF_TRIGGER_ALLOWPHYSICS	16	// react on a rigid-bodies

#define SF_TRIGGER_PUSH_START_OFF	2	// spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_TARGETONCE	1	// Only fire hurt target once
#define SF_TRIGGER_HURT_START_OFF	2	// spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_NO_CLIENTS	8	// spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_CLIENTONLYFIRE	16	// trigger hurt will only fire its target if it is hurting a client
#define SF_TRIGGER_HURT_CLIENTONLYTOUCH 32	// only clients may touch this trigger.

extern DLL_GLOBAL BOOL		g_fGameOver;

extern Vector VecBModelOrigin(entvars_t* pevBModel);

class CBaseTrigger : public CBaseToggle
{
	DECLARE_CLASS(CBaseTrigger, CBaseToggle);
public:
	void TeleportTouch(CBaseEntity* pOther);
	void KeyValue(KeyValueData* pkvd);
	void MultiTouch(CBaseEntity* pOther);
	void HurtTouch(CBaseEntity* pOther);
	void ActivateMultiTrigger(CBaseEntity* pActivator);
	void MultiWaitOver(void);
	void CounterUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void ToggleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	BOOL CanTouch(CBaseEntity* pOther);
	void InitTrigger(void);

	virtual int ObjectCaps(void) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }

	DECLARE_DATADESC();
};

class CTriggerInOut;

class CInOutRegister : public CPointEntity
{
	DECLARE_CLASS(CInOutRegister, CPointEntity);
public:
	// returns true if found in the list
	BOOL IsRegistered(CBaseEntity* pValue);
	// remove all invalid entries from the list, trigger their targets as appropriate
	// returns the new list
	CInOutRegister* Prune(void);
	// adds a new entry to the list
	CInOutRegister* Add(CBaseEntity* pValue);
	BOOL IsEmpty(void) { return m_pNext ? FALSE : TRUE; };

	DECLARE_DATADESC();

	CTriggerInOut *m_pField;
	CInOutRegister* m_pNext;
	EHANDLE m_hValue;
};

class CTriggerInOut : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerInOut, CBaseTrigger);
public:
	void Spawn(void);
	void Touch(CBaseEntity* pOther);
	void Think(void);
	int Restore(CRestore& restore);
	virtual void FireOnEntry(CBaseEntity* pOther);
	virtual void FireOnLeaving(CBaseEntity* pOther);
	virtual void OnRemove(void);
	void KeyValue(KeyValueData* pkvd);

	DECLARE_DATADESC();

	STATE GetState() { return m_pRegister->IsEmpty() ? STATE_OFF : STATE_ON; }

	string_t m_iszAltTarget;
	string_t m_iszBothTarget;
	CInOutRegister* m_pRegister;
};