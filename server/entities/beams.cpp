/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "customentity.h"
#include "effects.h"
#include "weapons/weapons.h"
#include "player.h"
#include "decals.h"
#include "shake.h"
#include "game/gamerules.h"


LINK_ENTITY_TO_CLASS( beam, CBeam );

BEGIN_DATADESC( CBeam )
	DEFINE_FUNCTION( TriggerTouch ),
END_DATADESC()

void CBeam::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT; // Remove model & collisions
}

void CBeam::Precache( void )
{
	if ( pev->owner )
		SetStartEntity( ENTINDEX( pev->owner ) );
	if ( pev->aiment )
		SetEndEntity( ENTINDEX( pev->aiment ) );
}

void CBeam::SetStartEntity( int entityIndex ) 
{ 
//	pev->sequence = (entityIndex & 0x0FFF) | ((pev->sequence&0xF000)<<12); 
	pev->sequence = (entityIndex & 0x0FFF) | (pev->sequence & 0xF000); // diffusion - fix from FWGS sdk
	pev->owner = INDEXENT( entityIndex );
}

void CBeam::SetEndEntity( int entityIndex ) 
{ 
//	pev->skin = (entityIndex & 0x0FFF) | ((pev->skin&0xF000)<<12); 
	pev->skin = (entityIndex & 0x0FFF) | (pev->skin & 0xF000); // diffusion - fix from FWGS sdk
	pev->aiment = INDEXENT( entityIndex );
}

// These don't take attachments into account
const Vector &CBeam::GetStartPos( void )
{
	if ( GetType() == BEAM_ENTS )
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));
		return pEntity->GetLocalOrigin();
	}
	return GetLocalOrigin();
}

const Vector &CBeam::GetEndPos( void )
{
	int type = GetType();

	if( type == BEAM_POINTS || type == BEAM_HOSE )
		return m_vecEndPos;

	CBaseEntity *pEntity;
	pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));
	if( pEntity ) return pEntity->GetLocalOrigin();

	return m_vecEndPos;
}

void CBeam::SetAbsStartPos( const Vector &pos )
{
	if( m_hParent == NULL )
	{
		SetStartPos( pos );
		return;
	}

	matrix4x4	worldToBeam = EntityToWorldTransform();
	Vector vecLocalPos = worldToBeam.VectorITransform( pos );

	SetStartPos( vecLocalPos );
}

void CBeam::SetAbsEndPos( const Vector &pos )
{
	if( m_hParent == NULL )
	{
		SetEndPos( pos );
		return;
	}

	matrix4x4	worldToBeam = EntityToWorldTransform();
	Vector vecLocalPos = worldToBeam.VectorITransform( pos );

	SetEndPos( vecLocalPos );
}

// These don't take attachments into account
const Vector &CBeam::GetAbsStartPos( void ) const
{
	if( GetType() == BEAM_ENTS && GetStartEntity( ))
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));

		if( pEntity )
			return pEntity->GetAbsOrigin();
		return GetAbsOrigin();
	}
	return GetAbsOrigin();
}

const Vector &CBeam::GetAbsEndPos( void ) const
{
	if( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE && GetEndEntity() ) 
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetEndEntity() ));

		if( pEntity )
			return pEntity->GetAbsOrigin();
	}

	if( const_cast<CBeam*>(this)->m_hParent == NULL )
		return m_vecEndPos;

	matrix4x4	beamToWorld = EntityToWorldTransform();
	const_cast<CBeam*>(this)->m_vecAbsEndPos = beamToWorld.VectorTransform( m_vecEndPos );

	return m_vecAbsEndPos;
}

CBeam *CBeam::BeamCreate( const char *pSpriteName, int width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam = GetClassPtr( (CBeam *)NULL );
	pBeam->pev->classname = MAKE_STRING( "beam" );
	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}

void CBeam::BeamInit( const char *pSpriteName, int width )
{
	pev->flags |= FL_CUSTOMENTITY;
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	pev->model = MAKE_STRING( pSpriteName );
	SetTexture( PRECACHE_MODEL( (char *)pSpriteName ) );
	SetWidth( width );
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}

void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::PointEntInit( const Vector &start, int endIndex )
{
	SetType( BEAM_ENTPOINT );
	SetStartPos( start );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( int startIndex, int endIndex )
{
	SetType( BEAM_ENTS );
	SetStartEntity( startIndex );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::RelinkBeam( void )
{
	const Vector &startPos = GetAbsStartPos();
	const Vector &endPos = GetAbsEndPos();

	pev->mins.x = Q_min( startPos.x, endPos.x );
	pev->mins.y = Q_min( startPos.y, endPos.y );
	pev->mins.z = Q_min( startPos.z, endPos.z );
	pev->maxs.x = Q_max( startPos.x, endPos.x );
	pev->maxs.y = Q_max( startPos.y, endPos.y );
	pev->maxs.z = Q_max( startPos.z, endPos.z );
	pev->mins = pev->mins - GetAbsOrigin();
	pev->maxs = pev->maxs - GetAbsOrigin();

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );
}

void CBeam::TriggerTouch( CBaseEntity *pOther )
{
	if( FBitSet( pOther->pev->flags, ( FL_CLIENT|FL_MONSTER ) ))
	{
		if( pev->owner )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
			pOwner->Use( pOther, this, USE_TOGGLE, 0 );
		}
		ALERT( at_console, "Firing targets!!!\n" );
	}
}

CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
	int total = 0;

	if( !szName || !*szName )
		return NULL;

	CBaseEntity *pEntity = NULL;
	CBaseEntity *pNewEntity = NULL;

	while(( pNewEntity = UTIL_FindEntityByTargetname( pNewEntity, szName )) != NULL )
	{
		total++;
		if( RANDOM_LONG( 0, total - 1 ) < 1 )
			pEntity = pNewEntity;
	}
	return pEntity;
}

void CBeam::DoSparks( const Vector &start, const Vector &end )
{
	if( FBitSet( pev->spawnflags, ( SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND )))
	{
		if( FBitSet( pev->spawnflags, SF_BEAM_SPARKSTART ))
			UTIL_Sparks( start );

		if( FBitSet( pev->spawnflags, SF_BEAM_SPARKEND ))
			UTIL_Sparks( end );
	}
}


class CLightning : public CBeam
{
	DECLARE_CLASS( CLightning, CBeam );
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Activate( void );

	void	StrikeThink( void );
	void	DamageThink( void );
	void	RandomArea( void );
	void	RandomPoint( const Vector &vecSrc );
	void	Zap( const Vector &vecSrc, const Vector &vecDest );
	void	StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	inline BOOL ServerSide( void )
	{
		if ( m_life == 0 && !( pev->spawnflags & SF_BEAM_RING ))
			return TRUE;
		return FALSE;
	}

	DECLARE_DATADESC();

	void	BeamUpdateVars( void );

	int	m_active;
	int	m_iszStartEntity;
	int	m_iszEndEntity;
	float	m_life;
	int	m_boltWidth;
	int	m_noiseAmplitude;
	int	m_speed;
	float	m_restrike;
	int	m_spriteTexture;
	int	m_iszSpriteName;
	int	m_frameStart;

	float	m_radius;
};

LINK_ENTITY_TO_CLASS( env_lightning, CLightning );
LINK_ENTITY_TO_CLASS( env_beam, CLightning );

// UNDONE: Jay -- This is only a test
#if _DEBUG
class CTripBeam : public CLightning
{
	DECLARE_CLASS( CTripBeam, CLightning );
public:
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trip_beam, CTripBeam );

void CTripBeam::Spawn( void )
{
	BaseClass::Spawn();
	SetTouch( &CBeam::TriggerTouch );
	pev->solid = SOLID_TRIGGER;
	RelinkBeam();
}
#endif

BEGIN_DATADESC( CLightning )
	DEFINE_FIELD( m_active, FIELD_INTEGER ),
	DEFINE_FIELD( m_spriteTexture, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iszStartEntity, FIELD_STRING, "LightningStart" ),
	DEFINE_KEYFIELD( m_iszEndEntity, FIELD_STRING, "LightningEnd" ),
	DEFINE_KEYFIELD( m_boltWidth, FIELD_INTEGER, "BoltWidth" ),
	DEFINE_KEYFIELD( m_noiseAmplitude, FIELD_INTEGER, "NoiseAmplitude" ),
	DEFINE_KEYFIELD( m_speed, FIELD_INTEGER, "TextureScroll" ),
	DEFINE_KEYFIELD( m_restrike, FIELD_FLOAT, "StrikeTime" ),
	DEFINE_KEYFIELD( m_iszSpriteName, FIELD_STRING, "texture" ),
	DEFINE_KEYFIELD( m_frameStart, FIELD_INTEGER, "framestart" ),
	DEFINE_KEYFIELD( m_radius, FIELD_FLOAT, "Radius" ),
	DEFINE_KEYFIELD( m_life, FIELD_FLOAT, "life" ),
	DEFINE_FUNCTION( StrikeThink ),
	DEFINE_FUNCTION( DamageThink ),
	DEFINE_FUNCTION( StrikeUse ),
	DEFINE_FUNCTION( ToggleUse ),
END_DATADESC()

void CLightning::Spawn( void )
{
	if( FStringNull( m_iszSpriteName ))
	{
		SetThink( &CBaseEntity::SUB_Remove );
		return;
	}

	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	pev->dmgtime = gpGlobals->time;

	//LRC- a convenience for mappers. Will this mess anything up?
	if (pev->rendercolor == g_vecZero)
		pev->rendercolor = Vector(255, 255, 255);

	if ( ServerSide() )
	{
		SetThink( NULL );
		if ( pev->dmg > 0 )
		{
			SetThink( &CLightning::DamageThink );
			pev->nextthink = gpGlobals->time + 0.1;
		}
		if ( pev->targetname )
		{
			if ( !(pev->spawnflags & SF_BEAM_STARTON) )
			{
				pev->effects |= EF_NODRAW;
				m_active = 0;
				DontThink();
			}
			else
				m_active = 1;
		
			SetUse(&CLightning::ToggleUse );
		}
	}
	else
	{
		m_active = 0;
		if ( !FStringNull(pev->targetname) )
		{
			SetUse(&CLightning::StrikeUse );
		}
		if ( FStringNull(pev->targetname) || FBitSet(pev->spawnflags, SF_BEAM_STARTON) )
		{
			SetThink(&CLightning::StrikeThink );
			pev->nextthink = gpGlobals->time + 1.0;
		}
	}
}

void CLightning::Precache( void )
{
	m_spriteTexture = PRECACHE_MODEL( (char *)STRING(m_iszSpriteName) );

	BaseClass::Precache();
}


void CLightning::Activate( void )
{
	if ( ServerSide() )
		BeamUpdateVars();
}


void CLightning::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LightningStart"))
	{
		m_iszStartEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningEnd"))
	{
		m_iszEndEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "life"))
	{
		m_life = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "BoltWidth"))
	{
		m_boltWidth = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		m_noiseAmplitude = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		m_speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "StrikeTime"))
	{
		m_restrike = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		m_frameStart = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Radius"))
	{
		m_radius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeam::KeyValue( pkvd );
}


void CLightning::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;
	if ( m_active )
	{
		m_active = 0;
		SUB_UseTargets( this, USE_OFF, 0 ); //LRC
		pev->effects |= EF_NODRAW;
		DontThink();
	}
	else
	{
		m_active = 1;
		SUB_UseTargets( this, USE_ON, 0 ); //LRC
		pev->effects &= ~EF_NODRAW;
		DoSparks( GetAbsStartPos(), GetAbsEndPos() );
		if ( pev->dmg > 0 )
		{
			pev->nextthink = gpGlobals->time;
			pev->dmgtime = gpGlobals->time;
		}
	}
}

void CLightning::StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;

	if ( m_active )
	{
		m_active = 0;
		SetThink( NULL );
	}
	else
	{
		SetThink(&CLightning::StrikeThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	if ( !FBitSet( pev->spawnflags, SF_BEAM_TOGGLE ) )
		SetUse( NULL );
}

int IsPointEntity( CBaseEntity *pEnt )
{
	if ( pEnt->m_hParent != NULL )
		return 0;

	//LRC- follow (almost) any entity that has a model
	if (pEnt->pev->modelindex && !(pEnt->pev->flags & FL_CUSTOMENTITY))
		return 0;

	return 1;
}

void CLightning::StrikeThink( void )
{
	if ( m_life != 0 && m_restrike != -1) //LRC non-restriking beams! what an idea!
	{
		if ( pev->spawnflags & SF_BEAM_RANDOM )
			SetNextThink( m_life + RANDOM_FLOAT( 0, m_restrike ) );
		else
			SetNextThink( m_life + m_restrike );
	}
	m_active = 1;

	if (FStringNull(m_iszEndEntity))
	{
		if (FStringNull(m_iszStartEntity))
			RandomArea( );	
		else
		{
			CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
			if (pStart != NULL)
				RandomPoint( pStart->GetAbsOrigin() );
			else
				ALERT( at_aiconsole, "env_beam: unknown entity \"%s\"\n", STRING(m_iszStartEntity) );
		}
		return;
	}

	CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
	CBaseEntity *pEnd = RandomTargetname( STRING(m_iszEndEntity) );

	if ( pStart != NULL && pEnd != NULL )
	{
		if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
		{
			if ( pev->spawnflags & SF_BEAM_RING)
			{
				// don't work
				return;
			}
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
			{
				if ( !IsPointEntity( pEnd ) )	// One point entity must be in pEnd
				{
					CBaseEntity *pTemp;
					pTemp = pStart;
					pStart = pEnd;
					pEnd = pTemp;
				}
				if ( !IsPointEntity( pStart ) )	// One sided
				{
					WRITE_BYTE( TE_BEAMENTPOINT );
					WRITE_SHORT( pStart->entindex() );
					WRITE_COORD( pEnd->GetAbsOrigin().x);
					WRITE_COORD( pEnd->GetAbsOrigin().y);
					WRITE_COORD( pEnd->GetAbsOrigin().z);
				}
				else
				{
					WRITE_BYTE( TE_BEAMPOINTS);
					WRITE_COORD( pStart->GetAbsOrigin().x);
					WRITE_COORD( pStart->GetAbsOrigin().y);
					WRITE_COORD( pStart->GetAbsOrigin().z);
					WRITE_COORD( pEnd->GetAbsOrigin().x);
					WRITE_COORD( pEnd->GetAbsOrigin().y);
					WRITE_COORD( pEnd->GetAbsOrigin().z);
				}


			}
			else
			{
				if ( pev->spawnflags & SF_BEAM_RING)
					WRITE_BYTE( TE_BEAMRING );
				else
					WRITE_BYTE( TE_BEAMENTS );
				WRITE_SHORT( pStart->entindex() );
				WRITE_SHORT( pEnd->entindex() );
			}

			WRITE_SHORT( m_spriteTexture );
			WRITE_BYTE( m_frameStart ); // framestart
			WRITE_BYTE( (int)pev->framerate); // framerate
			WRITE_BYTE( (int)(m_life*10.0) ); // life
			WRITE_BYTE( m_boltWidth );  // width
			WRITE_BYTE( m_noiseAmplitude );   // noise
			WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
			WRITE_BYTE( pev->renderamt );	// brightness
			WRITE_BYTE( m_speed );		// speed
		MESSAGE_END();
		DoSparks( pStart->GetAbsOrigin(), pEnd->GetAbsOrigin() );
		if ( pev->dmg || !FStringNull(pev->target))
		{
			TraceResult tr;
			UTIL_TraceLine( pStart->GetAbsOrigin(), pEnd->GetAbsOrigin(), dont_ignore_monsters, NULL, &tr );
			if( pev->dmg ) BeamDamageInstant( &tr, pev->dmg );

			//LRC - tripbeams
			CBaseEntity* pTrip;
			if (!FStringNull(pev->target) && (pTrip = GetTripEntity( &tr )) != NULL)
				UTIL_FireTargets( pev->target, pTrip, this, USE_TOGGLE, 0);
		}
	}
}

CBaseEntity *CBeam::GetTripEntity( TraceResult *ptr )
{
	CBaseEntity *pTrip;

	if( ptr->flFraction == 1.0 || ptr->pHit == NULL )
		return NULL;

	pTrip = CBaseEntity::Instance( ptr->pHit );
	if( pTrip == NULL )
		return NULL;

	if( FStringNull( pev->netname ))
	{
		if( pTrip->pev->flags & (FL_CLIENT|FL_MONSTER))
			return pTrip;
		else
			return NULL;
	}
	else if( FClassnameIs( pTrip->pev, STRING( pev->netname )))
		return pTrip;
	else if( FStrEq( pTrip->GetTargetname(), STRING( pev->netname )))
		return pTrip;

	return NULL;
}

void CBeam::BeamDamage( TraceResult *ptr )
{
	RelinkBeam();

	if( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( ptr->pHit );
		if ( pHit )
		{
			if( pev->dmg > 0 )
			{  
				ClearMultiDamage();
				pHit->TraceAttack( pev, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - GetAbsOrigin()).Normalize(), ptr, DMG_ENERGYBEAM );
				ApplyMultiDamage( pev, pev );
				if ( pev->spawnflags & SF_BEAM_DECALS )
				{
					if ( pHit->IsBSPModel() )
						UTIL_DecalTrace( ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0,4) );
				}
			}
			else
			{
				//LRC - beams that heal people
				pHit->TakeHealth( -(pev->dmg * (gpGlobals->time - pev->dmgtime)), DMG_GENERIC );
			}
		}
	}

	pev->dmgtime = gpGlobals->time;
}


void CLightning::DamageThink( void )
{
	SetNextThink( 0.1 );
	TraceResult tr;
	UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
	BeamDamage( &tr );

	//LRC - tripbeams
	if (!FStringNull(pev->target))
	{
		// nicked from monster_tripmine:
		//HACKHACK Set simple box using this really nice global!
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
		CBaseEntity *pTrip = GetTripEntity( &tr );
		if (pTrip)
		{
			if (!FBitSet(pev->spawnflags, SF_BEAM_TRIPPED))
			{
				UTIL_FireTargets( pev->target, pTrip, this, USE_TOGGLE, 0);
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
		{
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
		}
	}
}

void CLightning::Zap( const Vector &vecSrc, const Vector &vecDest )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_SHORT( m_spriteTexture );
		WRITE_BYTE( m_frameStart ); // framestart
		WRITE_BYTE( (int)pev->framerate); // framerate
		WRITE_BYTE( (int)(m_life*10.0) ); // life
		WRITE_BYTE( m_boltWidth );  // width
		WRITE_BYTE( m_noiseAmplitude );   // noise
		WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
		WRITE_BYTE( pev->renderamt );	// brightness
		WRITE_BYTE( m_speed );		// speed
	MESSAGE_END();

	DoSparks( vecSrc, vecDest );
}

void CLightning::RandomArea( void )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecSrc = GetAbsOrigin();

		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if (tr1.flFraction == 1.0)
			continue;

		Vector vecDir2;
		do {
			vecDir2 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		} while (DotProduct(vecDir1, vecDir2 ) > 0);
		vecDir2 = vecDir2.Normalize();
		TraceResult		tr2;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir2 * m_radius, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction == 1.0)
			continue;

		if ((tr1.vecEndPos - tr2.vecEndPos).Length() < m_radius * 0.1)
			continue;

		UTIL_TraceLine( tr1.vecEndPos, tr2.vecEndPos, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction != 1.0)
			continue;

		Zap( tr1.vecEndPos, tr2.vecEndPos );

		break;
	}
}


void CLightning::RandomPoint( const Vector &vecSrc )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if ((tr1.vecEndPos - vecSrc).Length() < m_radius * 0.1)
			continue;

		if (tr1.flFraction == 1.0)
			continue;

		Zap( vecSrc, tr1.vecEndPos );
		break;
	}
}



void CLightning::BeamUpdateVars( void )
{
	int pointStart, pointEnd;
	int beamType;

	CBaseEntity *pStart = UTIL_FindEntityByTargetname( NULL, STRING( m_iszStartEntity ));
	CBaseEntity *pEnd = UTIL_FindEntityByTargetname( NULL, STRING( m_iszEndEntity ));
	if( !pStart || !pEnd ) return;

	pointStart = IsPointEntity( pStart );
	pointEnd = IsPointEntity( pEnd );

	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
	pev->flags |= FL_CUSTOMENTITY;
	pev->model = m_iszSpriteName;
	SetTexture( m_spriteTexture );

	beamType = BEAM_ENTS;

	if( pointStart || pointEnd )
	{
		if( !pointStart )	// One point entity must be in pStart
		{
			CBaseEntity *pTemp;
			// Swap start & end
			pTemp = pStart;
			pStart = pEnd;
			pEnd = pTemp;
			int swap = pointStart;
			pointStart = pointEnd;
			pointEnd = swap;
		}

		if( !pointEnd )
			beamType = BEAM_ENTPOINT;
		else
			beamType = BEAM_POINTS;
	}

	SetType( beamType );

	if( beamType == BEAM_POINTS || beamType == BEAM_ENTPOINT || beamType == BEAM_HOSE )
	{
		SetStartPos( pStart->GetAbsOrigin( ));

		if ( beamType == BEAM_POINTS || beamType == BEAM_HOSE )
			SetEndPos( pEnd->GetAbsOrigin( ));
		else
			SetEndEntity( pEnd->entindex( ));
	}
	else
	{
		SetStartEntity( pStart->entindex( ));
		SetEndEntity( pEnd->entindex( ));
	}

	RelinkBeam();

	SetWidth( m_boltWidth );
	SetNoise( m_noiseAmplitude );
	SetFrame( m_frameStart );
	SetScrollRate( m_speed );

	if( pev->spawnflags & SF_BEAM_SHADEIN )
		SetFlags( BEAM_FSHADEIN );
	else if( pev->spawnflags & SF_BEAM_SHADEOUT )
		SetFlags( BEAM_FSHADEOUT );
	else if( pev->spawnflags & SF_BEAM_SOLID )
		SetFlags( BEAM_FSOLID );
}

LINK_ENTITY_TO_CLASS( env_laser, CLaser );

BEGIN_DATADESC( CLaser )
	DEFINE_FIELD( m_pSprite, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_iStoppedBy, FIELD_INTEGER, "m_iStoppedBy" ),
	DEFINE_KEYFIELD( m_iProjection, FIELD_INTEGER, "m_iProjection" ),
	DEFINE_KEYFIELD( m_iszSpriteName, FIELD_STRING, "EndSprite" ),
	DEFINE_FIELD( m_firePosition, FIELD_POSITION_VECTOR ),
	DEFINE_AUTO_ARRAY( m_pReflectedBeams, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( StrikeThink ),
END_DATADESC()

void CLaser::Spawn( void )
{
	if ( FStringNull( pev->model ) )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		return;
	}

	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache( );

	SetThink( &CLaser::StrikeThink );
	pev->flags |= FL_CUSTOMENTITY;

	SetBits( pev->spawnflags, SF_BEAM_INITIALIZE );
}

void CLaser::Precache( void )
{
	pev->modelindex = PRECACHE_MODEL( (char *)STRING( pev->model ));

	if ( m_iszSpriteName )
	{
		const char *ext = UTIL_FileExtension( STRING( m_iszSpriteName ));

		if( FStrEq( ext, "spr" ))
			PRECACHE_MODEL( (char *)STRING( m_iszSpriteName ));
	}
}

void CLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LaserTarget"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		SetNoise( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		SetScrollRate( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "EndSprite"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		pev->frame = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iProjection"))
	{
		m_iProjection = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iStoppedBy"))
	{
		m_iStoppedBy = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CLaser :: Activate( void )
{
	if( !FBitSet( pev->spawnflags, SF_BEAM_INITIALIZE ))
		return;

	ClearBits( pev->spawnflags, SF_BEAM_INITIALIZE );
	PointsInit( GetLocalOrigin(), GetLocalOrigin() );

	if ( m_iszSpriteName )
	{
		CBaseEntity *pTemp = UTIL_FindEntityByTargetname(NULL, STRING( m_iszSpriteName ));

		if( pTemp == NULL )
		{
			m_pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteName), GetLocalOrigin(), TRUE );
			if (m_pSprite)
				m_pSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );
		}
		else if( !FClassnameIs( pTemp->pev, "env_sprite" ))
		{
			ALERT(at_error, "env_laser \"%s\" found endsprite %s, but can't use: not an env_sprite\n", GetTargetname(), STRING(m_iszSpriteName));
			m_pSprite = NULL;
		}
		else
		{
			// use an env_sprite defined by the mapper
			m_pSprite = (CSprite *)pTemp;
			m_pSprite->pev->movetype = MOVETYPE_NOCLIP;
		}
	}
	else
		m_pSprite = NULL;

	if ( pev->targetname && !(pev->spawnflags & SF_BEAM_STARTON) )
		TurnOff();
	else
		TurnOn();
}

void CLaser::TurnOff( void )
{
	pev->effects |= EF_NODRAW;
	pev->nextthink = 0;

	if ( m_pSprite )
		m_pSprite->TurnOff();

	// remove all unused beams
	for( int i = 0; i < MAX_REFLECTED_BEAMS; i++ )
	{
		if( m_pReflectedBeams[i] )
		{
			UTIL_Remove( m_pReflectedBeams[i] );
			m_pReflectedBeams[i] = NULL;
		}
	}
}

void CLaser::TurnOn( void )
{
	pev->effects &= ~EF_NODRAW;

	if ( m_pSprite )
		m_pSprite->TurnOn();

	if( pev->spawnflags & SF_BEAM_SHADEIN )
		SetFlags( BEAM_FSHADEIN );
	else if( pev->spawnflags & SF_BEAM_SHADEOUT )
		SetFlags( BEAM_FSHADEOUT );
	else if( pev->spawnflags & SF_BEAM_SOLID )
		SetFlags( BEAM_FSOLID );

	pev->dmgtime = gpGlobals->time;
	SetNextThink( 0 );
}

void CLaser::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int active = (GetState() == STATE_ON);

	if ( !ShouldToggle( useType, active ) )
		return;

	if ( active )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}

bool CLaser::ShouldReflect( TraceResult &tr )
{
	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

	if( !pHit )
		return false;

	if( UTIL_GetModelType( pHit->pev->modelindex ) != mod_brush )
		return false;

	Vector vecStart = tr.vecEndPos + tr.vecPlaneNormal * -2;	// mirror thickness
	Vector vecEnd = tr.vecEndPos + tr.vecPlaneNormal * 2;
	const char *pTextureName = TRACE_TEXTURE( pHit->edict(), vecStart, vecEnd );

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "reflect", 7 ))
		return true; // valid surface

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "!reflect", 8 ) )
		return true; // valid surface

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "lasrefl", 7 ))
		return true; // diffusion - hack to allow reflection without rendering mirror

	return false;
}

void CLaser::FireAtPoint( const Vector &startpos, TraceResult &tr )
{
	SetAbsEndPos( tr.vecEndPos );

	if ( m_pSprite )
		UTIL_SetOrigin( m_pSprite, tr.vecEndPos );
	
	if(( pev->nextthink != 0.0f ) || ( gpGlobals->time >= ( pev->dmgtime + 0.1f )))
	{
		BeamDamage( &tr );
		DoSparks( startpos, tr.vecEndPos );
	}

	if( m_iProjection > 1 )
	{
		IGNORE_GLASS iIgnoreGlass;
		if( m_iStoppedBy % 2 ) // if it's an odd number
			iIgnoreGlass = ignore_glass;
		else
			iIgnoreGlass = dont_ignore_glass;

		IGNORE_MONSTERS iIgnoreMonsters;
		if( m_iStoppedBy <= 1 )
			iIgnoreMonsters = dont_ignore_monsters;
		else if( m_iStoppedBy <= 3 )
			iIgnoreMonsters = missile;
		else
			iIgnoreMonsters = ignore_monsters;

		Vector vecDir = (tr.vecEndPos - startpos).Normalize();
		edict_t *pentIgnore = tr.pHit;
		Vector vecSrc, vecDest;
		TraceResult tRef = tr;
		int i = 0;
		float n;

		// NOTE: determine texture name. We can reflect laser only at texture with name "reflect"
		for( i = 0; i < MAX_REFLECTED_BEAMS; i++ )
		{
			if( !ShouldReflect( tRef )) break; // no more reflect
			n = -DotProduct( tRef.vecPlaneNormal, vecDir );			
			if( n < 0.05f ) break; // diffusion - was 0.5f - I made a bigger angle

			vecDir = 2.0 * tRef.vecPlaneNormal * n + vecDir;
			vecSrc = tRef.vecEndPos + vecDir;
			vecDest = vecSrc + vecDir * 65000;

			if( !m_pReflectedBeams[i] ) // allocate new reflected beam
			{
				m_pReflectedBeams[i] = BeamCreate( GetModel(), pev->scale );
				m_pReflectedBeams[i]->pev->dmgtime = gpGlobals->time;
				m_pReflectedBeams[i]->pev->dmg = pev->dmg;
			}

			UTIL_TraceLine( vecSrc, vecDest, iIgnoreMonsters, iIgnoreGlass, pentIgnore, &tRef );

			CBaseEntity *pHit = CBaseEntity::Instance( tRef.pHit );
			pentIgnore = tRef.pHit;

			// hit a some bmodel (probably it picked by player)
			if( pHit && pHit->IsPushable( ))
			{
				// do additional trace for func_pushable (find a valid normal)
				UTIL_TraceModel( vecSrc, vecDest, point_hull, tRef.pHit, &tRef );
			}

			m_pReflectedBeams[i]->SetStartPos( vecSrc );
			m_pReflectedBeams[i]->SetEndPos( tRef.vecEndPos );

			// all other settings come from primary beam
			m_pReflectedBeams[i]->SetFlags( GetFlags());
			m_pReflectedBeams[i]->SetWidth( GetWidth());
			m_pReflectedBeams[i]->SetNoise( GetNoise());
			m_pReflectedBeams[i]->SetBrightness( GetBrightness());
			m_pReflectedBeams[i]->SetFrame( GetFrame());
			m_pReflectedBeams[i]->SetScrollRate( GetScrollRate());
			m_pReflectedBeams[i]->SetColor( pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z );

			if(( pev->nextthink != 0.0f ) || ( gpGlobals->time >= ( m_pReflectedBeams[i]->pev->dmgtime + 0.1f )))
			{
				m_pReflectedBeams[i]->BeamDamage( &tRef );
				m_pReflectedBeams[i]->DoSparks( vecSrc, tRef.vecEndPos );
			}
		}

		// ALERT( at_console, "%i beams used\n", i+1 );

		// remove all unused beams
		for( ; i < MAX_REFLECTED_BEAMS; i++ )
		{
			if( m_pReflectedBeams[i] )
			{
				UTIL_Remove( m_pReflectedBeams[i] );
				m_pReflectedBeams[i] = NULL;
			}
		}
	}
}

void CLaser::StrikeThink( void )
{
	Vector startpos = GetAbsStartPos();

	CBaseEntity *pEnd = RandomTargetname( STRING( pev->message ));

	if( pEnd )
	{
		m_firePosition = pEnd->GetAbsOrigin();
	}
	else if( m_iProjection )
	{
		UTIL_MakeVectors( GetAbsAngles() );
		m_firePosition = startpos + gpGlobals->v_forward * 65000; // diffusion - added 65000 instead of 4096
	}
	else
		m_firePosition = startpos;	// just in case

	TraceResult tr;

	//UTIL_TraceLine( GetAbsOrigin(), m_firePosition, dont_ignore_monsters, NULL, &tr );
	IGNORE_GLASS iIgnoreGlass;
	if( m_iStoppedBy % 2 ) // if it's an odd number
		iIgnoreGlass = ignore_glass;
	else
		iIgnoreGlass = dont_ignore_glass;

	IGNORE_MONSTERS iIgnoreMonsters;
	if( m_iStoppedBy <= 1 )
		iIgnoreMonsters = dont_ignore_monsters;
	else if( m_iStoppedBy <= 3 )
		iIgnoreMonsters = missile;
	else
		iIgnoreMonsters = ignore_monsters;

	if( m_iProjection )
	{
		UTIL_TraceLine( startpos, m_firePosition, iIgnoreMonsters, iIgnoreGlass, NULL, &tr );

		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

		// make additional trace for pushables (find a valid normal)
		if( pHit && pHit->IsPushable( ))
			UTIL_TraceModel( startpos, m_firePosition, point_hull, tr.pHit, &tr );
	}
	else
		UTIL_TraceLine( startpos, m_firePosition, iIgnoreMonsters, iIgnoreGlass, NULL, &tr );

	FireAtPoint( startpos, tr );

	// LRC - tripbeams
	// g-cont. FIXME: get support for m_iProjection = 2
	if( pev->target && m_iProjection != 2 )
	{
		// nicked from monster_tripmine:
		// HACKHACK Set simple box using this really nice global!
//		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine( startpos, m_firePosition, dont_ignore_monsters, NULL, &tr );
		CBaseEntity *pTrip = GetTripEntity( &tr );
		if( pTrip )
		{
			if( !FBitSet( pev->spawnflags, SF_BEAM_TRIPPED ))
			{
				UTIL_FireTargets( pev->target, pTrip, this, USE_TOGGLE, 0 );
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
	}

	// moving or reflected laser is thinking every frame
//	if( m_iProjection > 1 || m_hParent != NULL || ( pEnd && pEnd->m_hParent != NULL ))
//		SetNextThink( 0 );
//	else
		SetNextThink( 0 ); // diffusion - for smooth and faster response
}