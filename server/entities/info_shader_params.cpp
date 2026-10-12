#include "extdll.h"
#include "util.h"
#include "cbase.h"

//======================================================================
// diffusion - set params for client shaders
//======================================================================

/*
		fuser1 = sunshaft brightness
		iuser1 = disable blur of 3D sky
		iuser2 = disable lensflare
		iuser3 = disable sun shadows
		sequence = enable heat distortion effect
*/

class CInfoShaderParams : public CPointEntity
{
	DECLARE_CLASS( CInfoShaderParams, CPointEntity );
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( info_shader_params, CInfoShaderParams );

void CInfoShaderParams::Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );

	SetNullModel();
	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	// hacks to distinguish this entity from possible others
	pev->effects |= EF_SKIPPVS;
	pev->renderfx = kRenderFxFullbright;
}

//================================================================================
// info_shader_params_changer - change values in the main entity by using this one
//================================================================================

class CInfoShaderParamsChanger : public CPointEntity
{
	DECLARE_CLASS( CInfoShaderParamsChanger, CPointEntity );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int Mode_Sunshafts;
	int Mode_Blur3DSky;
	int Mode_LensFlare;
	int Mode_SunShadows;
	int Mode_HeatEffect;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( info_shader_params_changer, CInfoShaderParamsChanger );

BEGIN_DATADESC( CInfoShaderParamsChanger )
	DEFINE_KEYFIELD( Mode_Sunshafts, FIELD_INTEGER, "Mode_Sunshafts" ),
	DEFINE_KEYFIELD( Mode_Blur3DSky, FIELD_INTEGER, "Mode_Blur3DSky" ),
	DEFINE_KEYFIELD( Mode_LensFlare, FIELD_INTEGER, "Mode_LensFlare" ),
	DEFINE_KEYFIELD( Mode_SunShadows, FIELD_INTEGER, "Mode_SunShadows" ),
	DEFINE_KEYFIELD( Mode_HeatEffect, FIELD_INTEGER, "Mode_HeatEffect" ),
END_DATADESC()

void CInfoShaderParamsChanger::Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );
}

void CInfoShaderParamsChanger::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "Mode_Sunshafts" ) )
	{
		Mode_Sunshafts = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "Mode_Blur3DSky" ) )
	{
		Mode_Blur3DSky = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "Mode_LensFlare" ) )
	{
		Mode_LensFlare = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "Mode_SunShadows" ) )
	{
		Mode_SunShadows = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "Mode_HeatEffect" ) )
	{
		Mode_HeatEffect = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CInfoShaderParamsChanger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pParams = NULL;
	pParams = UTIL_FindEntityByClassname( pParams, "info_shader_params" );

	if( !pParams )
	{
		ALERT( at_warning, "info_shader_params_changer \"%s\" can't be used: no info_shader_params on the map.\n", GetTargetname() );
		return;
	}

	// sunshafts
	switch( Mode_Sunshafts )
	{
	default:
	case 0:
		break;
	case 1: // set value
		pParams->pev->fuser1 = pev->fuser1;
		break;
	case 2: // add
		pParams->pev->fuser1 += pev->fuser1;
		pParams->pev->fuser1 = bound( 0.0f, pParams->pev->fuser1, 10.0f );
		break;
	case 3: // subtract
		pParams->pev->fuser1 -= pev->fuser1;
		pParams->pev->fuser1 = bound( 0.0f, pParams->pev->fuser1, 10.0f );
		break;
	}

	// disable blur for 3D sky
	switch( Mode_Blur3DSky )
	{
	default:
	case 0:
		break;
	case 1: // set
		pParams->pev->iuser1 = pev->iuser1;
		break;
	}

	// disable lens flare
	switch( Mode_LensFlare )
	{
	default:
	case 0:
		break;
	case 1: // set
		pParams->pev->iuser2 = pev->iuser2;
		break;
	}

	// disable sun shadows
	switch( Mode_SunShadows )
	{
	default:
	case 0:
		break;
	case 1: // set
		pParams->pev->iuser3 = pev->iuser3;
		break;
	}

	// enable heat effect
	switch( Mode_HeatEffect )
	{
	default:
	case 0:
		break;
	case 1: // set
		pParams->pev->sequence = pev->sequence;
		break;
	}
}