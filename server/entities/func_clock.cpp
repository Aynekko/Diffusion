#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/client.h"
#include "player.h"

// =================== FUNC_CLOCK ==============================================

#define SECONDS_PER_MINUTE	60
#define SECONDS_PER_HOUR	3600
#define SECODNS_PER_DAY	43200

class CFuncClock : public CBaseDelay
{
	DECLARE_CLASS(CFuncClock, CBaseDelay);
public:
	void Spawn(void);
	void Think(void);
	void Activate(void);
	void KeyValue(KeyValueData* pkvd);
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
private:
	Vector	m_vecCurtime;
	Vector	m_vecFinaltime;
	float	m_flCurTime;	// current unconverted time in seconds
	int	m_iClockType;	// also contain seconds count for different modes
	int	m_iHoursCount;
	BOOL	m_fInit;		// this clock already init
};

LINK_ENTITY_TO_CLASS(func_clock, CFuncClock);

BEGIN_DATADESC(CFuncClock)
	DEFINE_FIELD(m_vecCurtime, FIELD_VECTOR),
	DEFINE_FIELD(m_vecFinaltime, FIELD_VECTOR),
	DEFINE_FIELD(m_flCurTime, FIELD_VECTOR),
	DEFINE_FIELD(m_iClockType, FIELD_INTEGER),
	DEFINE_FIELD(m_iHoursCount, FIELD_INTEGER),
	DEFINE_FIELD(m_fInit, FIELD_BOOLEAN),
END_DATADESC()

void CFuncClock::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		switch (Q_atoi(pkvd->szValue))
		{
		case 1:
			m_iClockType = SECONDS_PER_HOUR;
			break;
		case 2:
			m_iClockType = SECODNS_PER_DAY;
			break;
		default:
			m_iClockType = SECONDS_PER_MINUTE;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "curtime"))
	{
		Q_atov( m_vecCurtime, pkvd->szValue, 3 );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "event"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue(pkvd);
}

void CFuncClock::Spawn(void)
{
	CBaseToggle::AxisDir(pev);

	m_iState = STATE_ON;
	pev->solid = SOLID_NOT;

	SET_MODEL(edict(), GetModel());

	if (m_iClockType == SECODNS_PER_DAY)
	{
		// normalize our time
		if (m_vecCurtime.x > 11) m_vecCurtime.x = 0;
		if (m_vecCurtime.y > 59) m_vecCurtime.y = 0;
		if (m_vecCurtime.z > 59) m_vecCurtime.z = 0;

		// member full hours
		m_iHoursCount = m_vecCurtime.x;

		// calculate seconds
		m_vecFinaltime.z = m_vecCurtime.z * (SECONDS_PER_MINUTE / 60);
		m_vecFinaltime.y = m_vecCurtime.y * (SECONDS_PER_HOUR / 60) + m_vecFinaltime.z;
		m_vecFinaltime.x = m_vecCurtime.x * (SECODNS_PER_DAY / 12) + m_vecFinaltime.y;
	}
}

void CFuncClock::Activate(void)
{
	if (m_fInit) return;

	if (m_iClockType == SECODNS_PER_DAY && m_vecCurtime != g_vecZero)
	{
		// try to find minutes and seconds entity
		CBaseEntity* pEntity = NULL;
		while (pEntity = UTIL_FindEntityInSphere(pEntity, GetLocalOrigin(), pev->size.z))
		{
			if (FClassnameIs(pEntity, "func_clock"))
			{
				CFuncClock* pClock = (CFuncClock*)pEntity;
				// write start hours, minutes and seconds
				switch (pClock->m_iClockType)
				{
				case SECODNS_PER_DAY:
					// NOTE: here we set time for himself through FindEntityInSphere
					pClock->m_flCurTime = m_vecFinaltime.x;
					break;
				case SECONDS_PER_HOUR:
					pClock->m_flCurTime = m_vecFinaltime.y;
					break;
				default:
					pClock->m_flCurTime = m_vecFinaltime.z;
					break;
				}
			}
		}
	}

	// clock start	
	SetNextThink(0);
	m_fInit = 1;
}

void CFuncClock::Think(void)
{
	float seconds, ang, pos;

	seconds = gpGlobals->time + m_flCurTime;
	pos = seconds / m_iClockType;
	pos = pos - floor(pos);
	ang = 360 * pos;

	SetLocalAngles(pev->movedir * ang);

	if (m_iClockType == SECODNS_PER_DAY)
	{
		int hours = GetLocalAngles().Length() / 30;
		if (m_iHoursCount != hours)
		{
			// member new hour
			m_iHoursCount = hours;
			if (hours == 0) hours = 12;	// merge for 0.00.00

			// send hours info
			UTIL_FireTargets(pev->netname, this, this, USE_SET, hours);
			UTIL_FireTargets(pev->netname, this, this, USE_ON);
		}
	}

	RelinkEntity(FALSE);

	// set clock resolution
	SetNextThink(1.0f);
}