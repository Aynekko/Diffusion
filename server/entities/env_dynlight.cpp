#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"

// =================== ENV_DYNLIGHT ==============================================

#define SF_DYNLIGHT_START_ON			BIT(0)
#define SF_DYNLIGHT_NOSHADOWS			BIT(1)
#define SF_DYNLIGHT_NOLIGHT_IN_SOLID	BIT(2)
#define SF_DYNLIGHT_ONLYBRUSHSHADOWS	BIT(3)
#define SF_DYNLIGHT_ONLYFORCEDSHADOWS	BIT(4)

class CEnvDynamicLight : public CBaseDelay
{
	DECLARE_CLASS(CEnvDynamicLight, CBaseDelay);
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual STATE GetState(void) { return FBitSet(pev->effects, EF_NODRAW) ? STATE_OFF : STATE_ON; };
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	// diffusion - think functions for smooth turning
	void TurnOn( void );
	void TurnOff( void );
	bool LightOn;

	float TurnOnTime;
	float TurnOffTime;
	float Brightness; // actual brightness is fuser1. This value is only needed to remember the initial brightness.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(env_dynlight, CEnvDynamicLight);

BEGIN_DATADESC( CEnvDynamicLight )
	DEFINE_KEYFIELD( TurnOnTime, FIELD_FLOAT, "turnontime"),
	DEFINE_KEYFIELD( TurnOffTime, FIELD_FLOAT, "turnofftime" ),
	DEFINE_FIELD( Brightness, FIELD_FLOAT ),
	DEFINE_FIELD( LightOn, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( TurnOn ),
	DEFINE_FUNCTION( TurnOff ),
END_DATADESC()

void CEnvDynamicLight::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->scale = Q_atof(pkvd->szValue) * (1.0f / 8.0f);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "lightstyle"))
	{
		pev->renderfx = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "brightness" ) )
	{
		Brightness = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "turnontime" ) )
	{
		TurnOnTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "turnofftime" ) )
	{
		TurnOffTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue(pkvd);
}

void CEnvDynamicLight::Spawn(void)
{
	pev->effects |= (EF_DYNAMIC_LIGHT | EF_NODRAW);
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 255;
	pev->movetype = MOVETYPE_NOCLIP;

	LightOn = false;

	if( !Brightness )
		Brightness = 1.0f;
	
	if (!pev->scale)
		pev->scale = 300 * (1.0f / 8.0f);

	pev->frags = pev->scale; // member radius

	SetNullModel();
	SetBits(m_iFlags, MF_POINTENTITY);
	//	SetLocalAvelocity( Vector( 30, 30, 30 ));
	RelinkEntity( FALSE );

	if (HasSpawnFlags(SF_DYNLIGHT_NOLIGHT_IN_SOLID))
		pev->iuser1 |= CF_NOLIGHT_IN_SOLID;

	if( HasSpawnFlags(SF_DYNLIGHT_NOSHADOWS) )
		pev->iuser1 |= CF_NOSHADOWS;

	if( HasSpawnFlags(SF_DYNLIGHT_ONLYBRUSHSHADOWS) )
		pev->iuser1 |= CF_ONLYBRUSHSHADOWS;

	if( HasSpawnFlags( SF_DYNLIGHT_ONLYFORCEDSHADOWS ) )
		pev->iuser1 |= CF_ONLYFORCEDSHADOWS;

	// to prevent disappearing from PVS (scale is premultiplied by 0.125)
	UTIL_SetSize(pev, Vector(-pev->scale, -pev->scale, -pev->scale), Vector(pev->scale, pev->scale, pev->scale));

	// enable dynlight
	if (HasSpawnFlags(SF_DYNLIGHT_START_ON) || (FStringNull(pev->targetname) && FStringNull(m_iParent)) )
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.2);
	}
}

void CEnvDynamicLight::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (useType == USE_SET)
	{
		// set radius
		value = bound(0.0f, value, 1.0f);
		pev->scale = pev->frags * value;
		return;
	}

	if( useType == USE_ON )
		TurnOn();
	else if( useType == USE_OFF )
		TurnOff();
	else if( useType == USE_TOGGLE )
	{
		if( LightOn )
			TurnOff();
		else
			TurnOn();
	}
}

void CEnvDynamicLight::TurnOn(void)
{
	if( pev->effects & EF_NODRAW )
		pev->effects &= ~EF_NODRAW;

	LightOn = true;
	
	if( !TurnOnTime )
	{
		pev->fuser1 = Brightness;
		DontThink();
		return;
	}
	
	if( pev->fuser1 < Brightness )
	{
		pev->fuser1 += Brightness / (TurnOnTime / gpGlobals->frametime);
		if( pev->fuser1 >= Brightness )
		{
			pev->fuser1 = Brightness;
			DontThink();
		}
		else
		{
			SetThink( &CEnvDynamicLight::TurnOn );
			SetNextThink( 0 );
		}
	}
}

void CEnvDynamicLight::TurnOff( void )
{
	LightOn = false;

	if( !TurnOffTime )
	{
		pev->fuser1 = 0;
		pev->effects |= EF_NODRAW;
		DontThink();
		return;
	}

	if( pev->fuser1 > 0 )
	{
		pev->fuser1 -= Brightness / (TurnOffTime / gpGlobals->frametime);
		if( pev->fuser1 < 0.01f )
		{
			pev->effects |= EF_NODRAW;
			pev->fuser1 = 0;
			DontThink();
		}
		else
		{
			SetThink( &CEnvDynamicLight::TurnOff );
			SetNextThink( 0 );
		}
	}
}