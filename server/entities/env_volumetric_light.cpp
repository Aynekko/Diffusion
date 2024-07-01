#include "extdll.h"
#include "util.h"
#include "cbase.h"

//===================================================================
// diffusion - env_volumetric_light
// creates a cone of spotlight sprites with desired size and angles
//===================================================================

/*
flagged as iuser3 = -664
always additive
starts from origin
uses angles
rendercolor - color
renderamt - transparency
vuser1.x - light length
vuser1.y - light width (of a single sprite)
vuser1.z - amount of sprites in a cone
vuser2.x - start cone spread scale (more - wider)
vuser2.y - end cone spread scale (more - wider)
vuser2.z - use dust particles or not (0-1)
*/

#define SF_VOLLIGHT_STARTOFF BIT(0)
#define SF_VOLLIGHT_DUSTPARTICLES BIT(1)

class CEnvVolumetricLight : public CPointEntity
{
	DECLARE_CLASS( CEnvVolumetricLight, CPointEntity );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int light_length;
	int light_width;
	int number_of_sprites;
	float start_cone_scale;
	float end_cone_scale;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_volumetric_light, CEnvVolumetricLight );

BEGIN_DATADESC( CEnvVolumetricLight )
	DEFINE_KEYFIELD( light_length, FIELD_INTEGER, "light_length" ),
	DEFINE_KEYFIELD( light_width, FIELD_INTEGER, "light_width" ),
	DEFINE_KEYFIELD( number_of_sprites, FIELD_INTEGER, "number_of_sprites" ),
	DEFINE_KEYFIELD( start_cone_scale, FIELD_FLOAT, "start_cone_scale" ),
	DEFINE_KEYFIELD( end_cone_scale, FIELD_FLOAT, "end_cone_scale" ),
END_DATADESC()

void CEnvVolumetricLight::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "light_length" ) )
	{
		light_length = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "light_width" ) )
	{
		light_width = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "number_of_sprites" ) )
	{
		number_of_sprites = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "start_cone_scale" ) )
	{
		start_cone_scale = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "end_cone_scale" ) )
	{
		end_cone_scale = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CEnvVolumetricLight::Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );
	pev->iuser3 = -664; // flag for client

	if( HasSpawnFlags( SF_VOLLIGHT_STARTOFF ) )
	{
		pev->iuser3 = 0;
		pev->effects |= EF_NODRAW;
	}

	SetNullModel();
	UTIL_SetSize( pev, Vector( -256, -256, -256 ), Vector( 256, 256, 256 ) ); // FIXME calculate something here...

	// make sure it gets to transparent entity list on client
	pev->rendermode = kRenderTransAdd;

	if( !pev->renderamt )
		pev->renderamt = 150;

	if( !number_of_sprites )
		number_of_sprites = 7;
	if( !light_length )
		light_length = 100;
	if( !light_width )
		light_width = 35;
	if( !start_cone_scale )
		start_cone_scale = 0;
	if( !end_cone_scale )
		end_cone_scale = 3;

	// light length
	pev->vuser1.x = bound( 3, light_length, 9999 );
	// light width
	pev->vuser1.y = bound( 1, light_width, 3000 );
	// number of sprites
	pev->vuser1.z = bound( 1, number_of_sprites, 50 ); // 50 is client limit!
	// spread scales
	pev->vuser2.x = bound( 0, start_cone_scale, 9999 );
	pev->vuser2.y = bound( 0, end_cone_scale, 9999 );
	pev->vuser2.z = 0.0f;

	// diffusion - important. when mapper didn't set angles, it doesn't assume that it's zero vector
	if( GetAbsAngles() == g_vecZero )
		SetAbsAngles( g_vecZero );

	if( HasSpawnFlags( SF_VOLLIGHT_DUSTPARTICLES ) )
		pev->vuser2.z = 1.0f;
}

void CEnvVolumetricLight::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;

	// enable/disable
	if( pev->iuser3 == -664 )
	{
		pev->iuser3 = 0;
		pev->effects |= EF_NODRAW;
	}
	else
	{
		pev->iuser3 = -664;
		pev->effects = 0;
	}
}