#include "extdll.h"
#include "util.h"
#include "cbase.h"

// Screen shake
class CShake : public CBaseDelay//CPointEntity
{
	DECLARE_CLASS(CShake, CBaseDelay);
public:
	void	Spawn(void);
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void	KeyValue(KeyValueData* pkvd);

	inline	float Amplitude(void) { return pev->scale; }
	inline	float Frequency(void) { return pev->dmg_save; }
	inline	float Duration(void) { return pev->dmg_take; }
	inline	float Radius(void) { return pev->dmg; }

	inline	void SetAmplitude(float amplitude) { pev->scale = amplitude; }
	inline	void SetFrequency(float frequency) { pev->dmg_save = frequency; }
	inline	void SetDuration(float duration) { pev->dmg_take = duration; }
	inline	void SetRadius(float radius) { pev->dmg = radius; }

	// diffusion
	bool ContinuousState;
	void ContinuousShake(void);
	//	float m_flWait;

	DECLARE_DATADESC();
private:
};

BEGIN_DATADESC(CShake)
	DEFINE_FIELD(ContinuousState, FIELD_BOOLEAN),
	DEFINE_FUNCTION(ContinuousShake),
END_DATADESC()

LINK_ENTITY_TO_CLASS(env_shake, CShake);

// pev->scale is amplitude
// pev->dmg_save is frequency
// pev->dmg_take is duration
// pev->dmg is radius
// radius of 0 means all players

#define SF_SHAKE_EVERYONE	BIT(0)		// Don't check radius
//#define SF_SHAKE_DISRUPT	BIT(1)		// Disrupt controls
#define SF_SHAKE_INAIR		BIT(2)		// Shake players in air
#define SF_SHAKE_CONTINUOUS	BIT(3)		// diffusion - continuous shake
#define SF_SHAKE_CONT_ON	BIT(4)		// continuous - start ON

void CShake::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;

	if (pev->spawnflags & SF_SHAKE_EVERYONE)
		pev->dmg = 0;

	if (pev->spawnflags & SF_SHAKE_CONTINUOUS)
	{
		if (pev->spawnflags & SF_SHAKE_CONT_ON)
		{
			if ((m_flWait < 0.2) || !(m_flWait))
				m_flWait = 0.2;
			ContinuousState = true;
			SetThink(&CShake::ContinuousShake);
			SetNextThink(0.1);
		}
		else
			ContinuousState = false;
	}
}


void CShake::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "amplitude"))
	{
		SetAmplitude( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		SetFrequency( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		SetRadius( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}


void CShake::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (pev->spawnflags & SF_SHAKE_CONTINUOUS)
	{
		if (ContinuousState == false)
		{
			ContinuousState = true;
			SetThink(&CShake::ContinuousShake);
			SetNextThink(0.1);
		}
		else
		{
			ContinuousState = false;
			SetThink(NULL);
		}
	}
	else
		UTIL_ScreenShake(GetAbsOrigin(), Amplitude(), Frequency(), Duration(), Radius(), FBitSet(pev->spawnflags, SF_SHAKE_INAIR) ? true : false);
}

void CShake::ContinuousShake(void)
{
	UTIL_ScreenShake(GetAbsOrigin(), Amplitude(), Frequency(), Duration(), Radius(), FBitSet(pev->spawnflags, SF_SHAKE_INAIR) ? true : false);
	SetThink(&CShake::ContinuousShake);
	SetNextThink(m_flWait);
}