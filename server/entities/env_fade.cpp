#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "shake.h"

// pev->dmg_take is duration
// pev->dmg_save is hold duration
#define SF_FADE_IN			BIT(0)		// Fade in, not out
#define SF_FADE_MODULATE	BIT(1)	// Modulate, don't blend
#define SF_FADE_ONLYONE		BIT(2)
#define SF_FADE_PERMANENT	BIT(3)	// LRC - hold permanently
#define SF_FADE_BLINK		BIT(4) // diffusion - blink effect (f.e. for switching between the cameras)

class CFade : public CPointEntity
{
	DECLARE_CLASS(CFade, CPointEntity);
public:
	void	Spawn(void);
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void	KeyValue(KeyValueData* pkvd);
	void	DelayedUse( void );
	void	Blink(void); // diffusion

	inline	float Duration(void) { return pev->dmg_take; }
	inline	float HoldTime(void) { return pev->dmg_save; }

	inline	void SetDuration(float duration) { pev->dmg_take = duration; }
	inline	void SetHoldTime(float hold) { pev->dmg_save = hold; }
	EHANDLE m_hActivator;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(env_fade, CFade);

BEGIN_DATADESC( CFade )
	DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),
	DEFINE_FUNCTION(Blink),
	DEFINE_FUNCTION(DelayedUse),
END_DATADESC();

void CFade::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;

	m_hActivator = NULL;
}


void CFade::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}


void CFade::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	int fadeFlags = 0;

	if( pActivator )
		m_hActivator = pActivator;

	if (!HasSpawnFlags(SF_FADE_IN))
		fadeFlags |= FFADE_OUT;

	if (pev->spawnflags & SF_FADE_MODULATE)
		fadeFlags |= FFADE_MODULATE;

	if (pev->spawnflags & SF_FADE_PERMANENT)	//LRC
		fadeFlags |= FFADE_STAYOUT;				//LRC

	if (HasSpawnFlags(SF_FADE_ONLYONE))
	{
		if (pActivator->IsNetClient())
			UTIL_ScreenFade(pActivator, pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags);
	}
	else
		UTIL_ScreenFadeAll(pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags);

	if ( HasSpawnFlags(SF_FADE_BLINK) && !HasSpawnFlags(SF_FADE_PERMANENT) )
	{
		UTIL_ScreenFadeAll(pev->rendercolor, Duration(), HoldTime(), pev->renderamt, FFADE_OUT);
		SetThink(&CFade::Blink);
		SetNextThink(Duration() + HoldTime());
		return;
	}

	if( fadeFlags & FFADE_OUT )
	{
		// doing normal fade. delay target usage.
		SetThink( &CFade::DelayedUse );
		SetNextThink( Duration() + HoldTime() );
		return;
	}

	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0);
}

void CFade::Blink(void)
{
	UTIL_ScreenFadeAll(pev->rendercolor, Duration(), 0, pev->renderamt, FFADE_IN);
	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 ); // use after the fade. the target can be another fade too!
	DontThink();
}

void CFade::DelayedUse( void )
{
	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	DontThink();
}