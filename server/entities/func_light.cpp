#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "entities/func_break.h"

// =================== FUNC_LIGHT ==============================================

#define SF_LIGHT_START_ON		BIT( 0 )

class CFuncLight : public CBreakable
{
	DECLARE_CLASS(CFuncLight, CBreakable);
public:
	void Precache( void );
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType);
	void Flicker(void);
	void Die(void);
	void ClearEffects( void );

	DECLARE_DATADESC();
private:
	void SetLightStyle( const char *pattern );
	void TurnOn( void );
	void TurnOff( void );
	int	m_iFlickerMode;
	Vector	m_vecLastDmgPoint;
	float	m_flNextFlickerTime;
};
LINK_ENTITY_TO_CLASS(func_light, CFuncLight);

BEGIN_DATADESC(CFuncLight)
	DEFINE_FIELD(m_iFlickerMode, FIELD_INTEGER),
	DEFINE_FIELD(m_flNextFlickerTime, FIELD_FLOAT),
	DEFINE_FIELD(m_vecLastDmgPoint, FIELD_POSITION_VECTOR),
	DEFINE_FUNCTION(Flicker),
END_DATADESC()

void CFuncLight::SetLightStyle( const char *pattern )
{
	if( m_iStyle > 0 && m_iStyle < 256 )
		LIGHT_STYLE( m_iStyle, (char *)pattern );
}

void CFuncLight::TurnOn( void )
{
	pev->frame = 0; // light texture is on

	if( m_flDelay ) // flickering turn-on
	{
		m_iState = STATE_TURN_ON;
		SetLightStyle( "mmamammmmammamamaaamammma" );
		SetThink( &CFuncLight::Flicker );
		SetNextThink( m_flDelay );
	}
	else
	{
		m_iState = STATE_ON;
		DontThink(); // cancel a pending flicker
		SetLightStyle( "k" );
		UTIL_FireTargets( pev->target, this, this, USE_ON );
	}
}

void CFuncLight::TurnOff( void )
{
	DontThink();            // cancel a pending turn-on flicker
	m_iState = STATE_OFF;   // set the state BEFORE firing targets
	pev->frame = 1;         // light texture is off
	SetLightStyle( "a" );
	UTIL_FireTargets( pev->target, this, this, USE_OFF );
}

void CFuncLight::Precache( void )
{
	CBreakable::Precache();
	PRECACHE_SOUND( "buttons/spark1.wav" );
	PRECACHE_SOUND( "buttons/spark2.wav" );
	PRECACHE_SOUND( "buttons/spark3.wav" );
}

void CFuncLight::Spawn(void)
{
	m_Material = matGlass;

	CBreakable::Spawn();

	m_vecLastDmgPoint = Center();

	// probably map compiler doesn't have func_light support
	if( m_iStyle <= 0 || m_iStyle >= 256 )
	{
		if( GetTargetname()[0] )
			ALERT( at_error, "%s with name %s has bad lightstyle %i. Disabled\n", GetClassname(), GetTargetname(), m_iStyle );
		else
			ALERT( at_error, "%s [%i] has bad lightstyle %i. Disabled\n", GetClassname(), entindex(), m_iStyle );

		m_iState = STATE_DEAD; // lamp is dead
		pev->takedamage = DAMAGE_NO;
		return;
	}

	if( HasSpawnFlags( SF_LIGHT_START_ON ) )
		TurnOn();
	else
		TurnOff();

	pev->takedamage = (pev->health <= 0) ? DAMAGE_NO : DAMAGE_YES;
}

void CFuncLight::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;

	if( m_iState == STATE_DEAD )
		return; // lamp is broken

	m_hActivator = pActivator;

	if( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_OFF )
			useType = USE_ON;
		else useType = USE_OFF;
	}

	if( useType == USE_TOGGLE )
		useType = (m_iState == STATE_OFF) ? USE_ON : USE_OFF;

	if( !ShouldToggle( useType ) ) // already in the requested state
		return;

	if( useType == USE_ON )
		TurnOn();
	else if( useType == USE_OFF )
		TurnOff();
	else if( useType == USE_SET )
		Die(); // a script die (dramatic effect)
}

void CFuncLight::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( pev->takedamage == DAMAGE_NO || m_iState == STATE_DEAD )
		return;

	const char *pTextureName = NULL;

	if( ptr->pHit )
	{
		Vector start = ptr->vecEndPos - vecDir * 8;
		Vector end = ptr->vecEndPos + vecDir * 8;
		pTextureName = TRACE_TEXTURE( ptr->pHit, start, end );
	}

	if( pTextureName != NULL && (!Q_strncmp( pTextureName, "+0~", 3 ) || *pTextureName == '~') )
	{
		UTIL_Sparks( ptr->vecEndPos );
		m_vecLastDmgPoint = ptr->vecEndPos;
		// take damage only at light texture
		CBreakable::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
	}
}

int CFuncLight::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pev->takedamage == DAMAGE_NO || m_iState == STATE_DEAD )
		return 0;

	m_iLastHitDamageType = bitsDamageType;

	if( bitsDamageType & DMG_BLAST )
		flDamage *= 3.0f;
	else if( bitsDamageType & DMG_CLUB )
		flDamage *= 2.0f;
	else if( bitsDamageType & DMG_SONIC )
		flDamage *= 1.2f;
	else if( bitsDamageType & DMG_SHOCK )
		flDamage *= 10.0f;//!!!! over voltage
	else if( bitsDamageType & DMG_BULLET )
		flDamage *= 0.5f; // half damage at bullet

	pev->health -= flDamage;

	if( pev->health <= 0.0f )
	{
		Die();
		return 0;
	}

	CBreakable::DamageSound();

	return 1;
}

void CFuncLight::Die( void )
{
	if( m_iState == STATE_DEAD )
		return;

	// lamp is random choose die style
	if( m_iState == STATE_OFF )
	{
		pev->frame = 1; // light texture is off
		SetLightStyle( "a" );
		DontThink();
	}
	else
	{
		// simple randomization
		m_iFlickerMode = RANDOM_LONG( 1, 2 );
		SetThink( &CFuncLight::Flicker );
		SetNextThink( 0.1f + RANDOM_FLOAT( 0.1f, 0.2f ) );
	}

	m_iState = STATE_DEAD;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	UTIL_FireTargets( pev->target, this, this, USE_OFF );

	switch( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND( edict(), CHAN_VOICE, "debris/bustglass1.wav", 0.7, ATTN_IDLE );
		break;
	case 1:
		EMIT_SOUND( edict(), CHAN_VOICE, "debris/bustglass2.wav", 0.8, ATTN_IDLE );
		break;
	}

	Vector vecSpot = GetAbsOrigin() + (pev->mins + pev->maxs) * 0.5f;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );
		WRITE_COORD( pev->size.x );
		WRITE_COORD( pev->size.y );
		WRITE_COORD( pev->size.z );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_BYTE( 10 );
		WRITE_SHORT( m_idShard );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 25 );
		WRITE_BYTE( BREAK_GLASS );
	MESSAGE_END();
}

void CFuncLight::Flicker( void )
{
	if( m_iState != STATE_TURN_ON && m_iState != STATE_DEAD )
	{
		DontThink();
		return;
	}

	if( m_iState == STATE_TURN_ON )
	{
		m_iState = STATE_ON;
		SetLightStyle( "k" );
		UTIL_FireTargets( pev->target, this, this, USE_ON );
		DontThink();
		return;
	}

	if( m_iFlickerMode == 1 )
	{
		pev->frame = 1;
		SetLightStyle( "a" );
		SetThink( NULL );
		return;
	}

	if( m_iFlickerMode == 2 )
	{
		switch( RANDOM_LONG( 0, 3 ) )
		{
		case 0:
			SetLightStyle( "abcaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
			break;
		case 1:
			SetLightStyle( "acaaabaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
			break;
		case 2:
			SetLightStyle( "aaafbaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
			break;
		case 3:
			SetLightStyle( "aaaaaaaaaaaaagaaaaaaaaacaaaacaaaa" );
			break;
		}

		m_flNextFlickerTime = RANDOM_FLOAT( 0.5f, 10.0f );
		UTIL_Sparks( m_vecLastDmgPoint );

		switch( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND( edict(), CHAN_VOICE, "buttons/spark1.wav", 0.4, ATTN_IDLE );
			break;
		case 1:
			EMIT_SOUND( edict(), CHAN_VOICE, "buttons/spark2.wav", 0.3, ATTN_IDLE );
			break;
		case 2:
			EMIT_SOUND( edict(), CHAN_VOICE, "buttons/spark3.wav", 0.35, ATTN_IDLE );
			break;
		}

		if( m_flNextFlickerTime > 6.5f )
			m_iFlickerMode = 1; // stop sparking
	}

	SetNextThink( m_flNextFlickerTime );
}

//=========================================================================
// diffusion - the entity was marked for deletion. turn the light off
//=========================================================================
void CFuncLight::ClearEffects(void)
{
	SetLightStyle( "a" );
}