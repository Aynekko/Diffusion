#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== ENV_SPARK ==============================================

#define SF_SPARK_TOGGLE		BIT( 5 )
#define SF_SPARK_START_ON		BIT( 6 )

class CEnvSpark : public CBaseDelay
{
	DECLARE_CLASS( CEnvSpark, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void SparkThink( void );
	void SparkUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_spark, CEnvSpark );

BEGIN_DATADESC( CEnvSpark )
	DEFINE_FUNCTION( SparkThink ),
	DEFINE_FUNCTION( SparkUse ),
END_DATADESC()

void CEnvSpark::Precache( void )
{
	UTIL_PrecacheSound( "buttons/spark1.wav" );
	UTIL_PrecacheSound( "buttons/spark2.wav" );
	UTIL_PrecacheSound( "buttons/spark3.wav" );
//	UTIL_PrecacheSound( "buttons/spark4.wav" );
//	UTIL_PrecacheSound( "buttons/spark5.wav" );
//	UTIL_PrecacheSound( "buttons/spark6.wav" );
}

void CEnvSpark::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "MaxDelay" ) )
	{
		m_flDelay = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CEnvSpark::Spawn( void )
{
	Precache();

	SetThink( NULL );
	SetUse( NULL );

	if( HasSpawnFlags(SF_SPARK_TOGGLE) )
	{
		if( HasSpawnFlags( SF_SPARK_START_ON ) )
			SetThink( &CEnvSpark::SparkThink );	// start sparking

		SetUse( &CEnvSpark::SparkUse );
	}
	else
	{
		SetThink( &CEnvSpark::SparkThink );
	}

	SetNextThink( 0.1f + RANDOM_FLOAT( 0.0f, 1.5f ) );

	if( m_flDelay <= 0 )
		m_flDelay = 1.5f;
	m_iState = STATE_OFF;
}

void CEnvSpark::SparkThink( void )
{
	UTIL_DoSpark( pev, GetAbsOrigin() );
	SetNextThink( 0.1f + RANDOM_FLOAT( 0.0f, m_flDelay ) );
	m_iState = STATE_ON;
}

void CEnvSpark::SparkUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int active = (GetState() == STATE_ON);

	if( !ShouldToggle( useType, active ) )
		return;

	if( active )
	{
		m_iState = STATE_OFF;
		SetThink( NULL );
	}
	else
	{
		SetThink( &CEnvSpark::SparkThink );
		SetNextThink( 0.1f + RANDOM_FLOAT( 0.0f, m_flDelay ) );
	}
}