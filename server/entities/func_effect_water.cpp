#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

#define SF_WATEREF_STARTOFF BIT(0)

class CWaterEffect : public CBaseTrigger
{
	DECLARE_CLASS( CWaterEffect, CBaseTrigger );
public:
	void Spawn( void );
	void Precache( void );
	void Touch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SoundThink( void );
	float Volume;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( func_effect_water, CWaterEffect );

BEGIN_DATADESC( CWaterEffect )
	DEFINE_FUNCTION( SoundThink ),
	DEFINE_FIELD( Volume, FIELD_FLOAT ),
END_DATADESC()

void CWaterEffect::Precache(void)
{
	PRECACHE_SOUND( "ambience/water_stream_loop.wav" );
}

void CWaterEffect::Spawn( void )
{
	Precache();
	InitTrigger();
	if( HasSpawnFlags( SF_WATEREF_STARTOFF ) )
		m_iState = STATE_OFF;
	else
		m_iState = STATE_ON;
}

void CWaterEffect::Touch( CBaseEntity *pOther )
{	
	// Only player
	if( !pOther->IsPlayer() )
		return;

	if( m_iState == STATE_OFF )
	{
		STOP_SOUND( ENT( pev ), CHAN_BODY, "ambience/water_stream_loop.wav" );
		DontThink();
		return;
	}

	Volume = 1.0f;

	SetThink( &CWaterEffect::SoundThink );
	SetNextThink( 0 );

	pOther->pev->vuser1.x = 15;
}

void CWaterEffect::SoundThink(void)
{
	Volume = bound( 0, Volume, 1 );
	if( Volume > 0.0f )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "ambience/water_stream_loop.wav", Volume, ATTN_NORM, SND_CHANGE_VOL, PITCH_NORM );
		Volume -= gpGlobals->frametime;
		SetNextThink( 0 );
	}
	else
	{
		STOP_SOUND( ENT( pev ), CHAN_BODY, "ambience/water_stream_loop.wav" );
		DontThink();
		return;
	}
}

void CWaterEffect::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ) )
		return;

	switch(m_iState)
	{
	case STATE_OFF:
		m_iState = STATE_ON;
		break;
	case STATE_ON:
		m_iState = STATE_OFF;
		break;
	}
}