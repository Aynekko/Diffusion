#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "game/gamerules.h"
#include "cbase.h"
#include "player.h"
#include "weapons/weapons.h"

//==================================================================
//LRC- Shockwave effect, like when a Houndeye attacks.
//==================================================================
#define SF_SHOCKWAVE_CENTERED		BIT(0)
#define SF_SHOCKWAVE_REPEATABLE		BIT(1)

class CEnvShockwave : public CPointEntity
{
	DECLARE_CLASS( CEnvShockwave, CPointEntity );
public:
	void	Precache( void );
	void	Spawn( void )
	{ 
		Precache();
		if( !m_iSpriteTexture )
		{
			UTIL_Remove( this );
			return;
		}
		if( !m_iType )
			m_iType = TE_BEAMCYLINDER;
	}
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	float m_fTime;
	int m_iRadius;
	int	m_iHeight;
	int m_iScrollRate;
	int m_iNoise;
	int m_iFrameRate;
	int m_iStartFrame;
	int m_iSpriteTexture;
	char m_iType;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_shockwave, CEnvShockwave )

BEGIN_DATADESC( CEnvShockwave )
	DEFINE_KEYFIELD( m_iHeight, FIELD_INTEGER, "m_iHeight" ),
	DEFINE_KEYFIELD( m_fTime, FIELD_FLOAT, "m_fTime" ),
	DEFINE_KEYFIELD( m_iRadius, FIELD_INTEGER, "m_iRadius" ),
	DEFINE_KEYFIELD( m_iScrollRate, FIELD_INTEGER, "m_iScrollRate" ),
	DEFINE_KEYFIELD( m_iNoise, FIELD_INTEGER, "m_iNoise" ),
	DEFINE_KEYFIELD( m_iFrameRate, FIELD_INTEGER, "m_iFrameRate" ),
	DEFINE_KEYFIELD( m_iStartFrame, FIELD_INTEGER, "m_iStartFrame" ),
	DEFINE_FIELD( m_iSpriteTexture, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iType, FIELD_INTEGER, "m_iType" ),
END_DATADESC();

void CEnvShockwave::Precache( void )
{
	m_iSpriteTexture = PRECACHE_MODEL( STRING( pev->netname ) );
}

void CEnvShockwave::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_fTime" ) )
	{
		m_fTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iRadius" ) )
	{
		m_iRadius = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iHeight" ) )
	{
		m_iHeight = Q_atoi( pkvd->szValue ) / 2; //LRC- the actual height is doubled when drawn
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iScrollRate" ) )
	{
		m_iScrollRate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iNoise" ) )
	{
		m_iNoise = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iFrameRate" ) )
	{
		m_iFrameRate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iStartFrame" ) )
	{
		m_iStartFrame = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iType" ) )
	{
		m_iType = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvShockwave::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos = GetAbsOrigin();

	if( !HasSpawnFlags(SF_SHOCKWAVE_CENTERED) )
		vecPos.z += m_iHeight;

	// blast circle
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY, vecPos );
		WRITE_BYTE( m_iType );
		WRITE_COORD( vecPos.x );// coord coord coord (center position)
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		WRITE_COORD( vecPos.x );// coord coord coord (axis and radius)
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z + m_iRadius );
		WRITE_SHORT( m_iSpriteTexture ); // short (sprite index)
		WRITE_BYTE( m_iStartFrame ); // byte (starting frame)
		WRITE_BYTE( m_iFrameRate ); // byte (frame rate in 0.1's)
		WRITE_BYTE( (int)(m_fTime * 10) ); // byte (life in 0.1's)
		WRITE_BYTE( m_iHeight );  // byte (line width in 0.1's)
		WRITE_BYTE( m_iNoise );   // byte (noise amplitude in 0.01's)
		WRITE_BYTE( pev->rendercolor.x );   // byte,byte,byte (color)
		WRITE_BYTE( pev->rendercolor.y );
		WRITE_BYTE( pev->rendercolor.z );
		WRITE_BYTE( pev->renderamt );  // byte (brightness)
		WRITE_BYTE( m_iScrollRate );	// byte (scroll speed in 0.1's)
	MESSAGE_END();

	if( !HasSpawnFlags(SF_SHOCKWAVE_REPEATABLE) )
	{
		SetThink( &CEnvShockwave::SUB_Remove );
		SetNextThink(0);
	}
}