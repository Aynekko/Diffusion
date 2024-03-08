#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_GLOW_STARTOFF BIT(0)

class CGlow : public CPointEntity
{
	DECLARE_CLASS(CGlow, CPointEntity);
public:
	void Spawn(void);
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think(void);
	void Animate(float frames);

	DECLARE_DATADESC();

	float m_lastTime;
	float m_maxFrame;
};

LINK_ENTITY_TO_CLASS(env_glow, CGlow);

BEGIN_DATADESC(CGlow)
	DEFINE_FIELD(m_lastTime, FIELD_TIME),
	DEFINE_FIELD(m_maxFrame, FIELD_FLOAT),
END_DATADESC()

void CGlow::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;

	PRECACHE_MODEL((char*)STRING(pev->model));
	SET_MODEL(ENT(pev), STRING(pev->model));

	m_maxFrame = (float)MODEL_FRAMES(pev->modelindex) - 1;
	if (m_maxFrame > 1.0 && pev->framerate != 0)
		SetNextThink(0.1);

	m_lastTime = gpGlobals->time;

	if( HasSpawnFlags( SF_GLOW_STARTOFF ) )
		pev->effects |= EF_NODRAW;
}


void CGlow::Think(void)
{
	Animate(pev->framerate * (gpGlobals->time - m_lastTime));

	SetNextThink( 0 ); // diffusion - smooth :)
	m_lastTime = gpGlobals->time;
}

void CGlow::Animate(float frames)
{
	if (m_maxFrame > 0)
		pev->frame = fmod(pev->frame + frames, m_maxFrame);
}

// diffusion - simple use function for on/off
void CGlow::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;

	if( useType == USE_OFF )
		pev->effects |= EF_NODRAW;
	else if( useType == USE_ON )
		pev->effects &= ~EF_NODRAW;
	else // toggle
	{
		if( pev->effects & EF_NODRAW )
			pev->effects &= ~EF_NODRAW;
		else
			pev->effects |= EF_NODRAW;
	}
}