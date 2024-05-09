/*
func_monitor.cpp - realtime monitors and portals
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "studio.h"

// =================== FUNC_MONITOR ==============================================

#define SF_MONITOR_START_ON		BIT( 0 )
#define SF_MONITOR_PASSABLE		BIT( 1 )
#define SF_MONITOR_USEABLE		BIT( 2 )
#define SF_MONITOR_HIDEHUD		BIT( 3 )
#define SF_MONITOR_MONOCRHOME	BIT( 4 )	// black & white

class CFuncMonitor : public CBaseDelay
{
	DECLARE_CLASS( CFuncMonitor, CBaseDelay );
public:
	void Precache( void );
	void Spawn( void );
	void ChangeCamera( string_t newcamera );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual BOOL IsFuncScreen( void ) { return TRUE; }
	bool AllowToFindClientInPVS( CBaseEntity *pTarget );
	void StartMessage( CBasePlayer *pPlayer );
	virtual STATE GetState( void ) { return (pev->iuser3 == -670) ? STATE_ON : STATE_OFF; };
	virtual int ObjectCaps( void );
	BOOL OnControls( CBaseEntity *pTest );
	void Activate( void );
	void VisThink( void );
	void SetCameraVisibility( bool fEnable );
	EHANDLE m_hCachedCamera; // diffusion - in case we change it with trigger_changetarget, mergevisibility must be removed on the cached camera

	DECLARE_DATADESC();

	CBasePlayer *m_pController;	// player pointer
	Vector m_vecControllerUsePos; // where was the player standing when he used me?
	float m_flDisableTime;	// portal disable time
};

LINK_ENTITY_TO_CLASS( func_monitor, CFuncMonitor );

BEGIN_DATADESC( CFuncMonitor )
	DEFINE_FIELD( m_pController, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_FIELD( m_flDisableTime, FIELD_TIME ),
	DEFINE_FIELD( m_hCachedCamera, FIELD_EHANDLE ),
	DEFINE_FUNCTION( VisThink ),
END_DATADESC()

void CFuncMonitor::Precache( void )
{
	PRECACHE_MODEL( GetModel() );
}

void CFuncMonitor::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "camera" ))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "fov" ))
	{
		pev->fuser2 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CFuncMonitor :: Spawn( void )
{
	if( FStringNull( pev->target ) )
	{
		ALERT( at_error, "func_monitor \"%s\" without target! Removed.", GetTargetname() );
		UTIL_Remove( this );
		return;
	}
	
	Precache();	
	pev->solid = SOLID_NOT;

	SET_MODEL( edict(), GetModel() );

	pev->effects |= EF_SCREEN;
	m_iState = STATE_OFF;

	SetLocalAngles( m_vecTempAngles );
	UTIL_SetOrigin( this, GetLocalOrigin() );

	if( UTIL_GetModelType( pev->modelindex ) == mod_brush )
	{
		pev->movetype = MOVETYPE_PUSH;

		if( HasSpawnFlags(SF_MONITOR_PASSABLE) )
			pev->solid = SOLID_NOT;
		else pev->solid = SOLID_BSP;

		SET_MODEL( edict(), GetModel() ); // yes, again

		if( HasSpawnFlags(SF_MONITOR_MONOCRHOME) )
			pev->iuser1 |= CF_MONOCHROME;
	}
	else // studio model
	{
		studiohdr_t *pstudiohdr;
		pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( edict() );
		if( pev->scale <= 0 )
			pev->scale = 1;
		pev->startpos = Vector( pev->scale, pev->scale, pev->scale );

		if( pstudiohdr == NULL )
		{
			UTIL_SetSize( pev, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ) );
			ALERT( at_error, "func_monitor: studio: unable to fetch model pointer!\n" );
		}

		mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
		UTIL_SetSize( pev, pseqdesc[0].bbmin * pev->startpos, pseqdesc[0].bbmax * pev->startpos );
	}

	// enable monitor
	if( HasSpawnFlags(SF_MONITOR_START_ON))
	{
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
}

void CFuncMonitor::Activate( void )
{
	ChangeCamera( pev->target );
}

void CFuncMonitor :: StartMessage( CBasePlayer *pPlayer )
{
	// dr.Tressi request: camera who attached with player
	if( FStrEq( STRING( pev->target ), "*player" ) && !g_pGameRules->IsMultiplayer())
		ChangeCamera( pev->target );
}

int CFuncMonitor :: ObjectCaps( void ) 
{
	if( HasSpawnFlags(SF_MONITOR_USEABLE) )
		return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE | FCAP_HOLD_ANGLES;
	return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION)| FCAP_HOLD_ANGLES;
}

void CFuncMonitor::ChangeCamera( string_t newcamera )
{
	CBaseEntity *pCamera = UTIL_FindEntityByTargetname( NULL, STRING( newcamera ));
	CBaseEntity *pOldCamera = m_hCachedCamera;// UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

	if( pOldCamera )
	{
		// now old camera can't merge PVS anymore
		ClearBits( pOldCamera->pev->effects, EF_MERGE_VISIBILITY );
		pev->aiment = NULL;
	}
         
	if( pCamera )
	{
		if( FClassnameIs( pCamera, "info_target" ) && !pCamera->pev->model )
			pCamera->SetNullModel(); // force to client
		
		// update the visible state
		if( m_iState == STATE_ON )
			SetBits( pCamera->pev->effects, EF_MERGE_VISIBILITY );

		m_hCachedCamera = pCamera;
		pev->aiment = pCamera->edict(); // tell engine about portal entity with camera
	}

	pev->target = newcamera;
}

void CFuncMonitor :: SetCameraVisibility( bool fEnable )
{
	// set camera PVS
	if( pev->aiment > 0 )
	{
		CBaseEntity *pTarget = CBaseEntity::Instance( pev->aiment );

		if( !pTarget )
			return;

		// allow camera merge visibility for enabled or disabled portals
		if( fEnable )
			SetBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
		else
			ClearBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
	}
}

bool CFuncMonitor :: AllowToFindClientInPVS( CBaseEntity *pTarget )
{
	if( !pTarget ) // no target
		return false;

	if( pTarget->IsPlayer() )	// target is player
		return false;

	CBaseEntity *pParent = pTarget->GetRootParent();

	if( pParent->IsPlayer() )	// target is attached to player
		return false;	

	return true; // non-client camera
}

void CFuncMonitor :: VisThink( void )
{
	if( pev->aiment != 0 )
	{
		CBaseEntity *pTarget = CBaseEntity::Instance( pev->aiment );

		// no reason to find in PVS itself
		if( AllowToFindClientInPVS( pTarget ))
		{
			CBaseEntity *pClient = CBaseEntity::Instance( FIND_CLIENT_IN_PVS( edict() ) );
			
			if( !pClient )
				ClearBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
			else
			{
				// diffusion - also check distance!
				// doesn't work in multiplayer
				if( (gpGlobals->maxClients == 1) && (pev->iuser4 > 0) )
				{
					if( (pClient->GetAbsOrigin() - GetAbsOrigin()).Length() > (pev->iuser4 + 255) )
						ClearBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
					else
						SetBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
				}
				else
					SetBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
			}
		}
	}

	SetNextThink( 0.1 );
}

void CFuncMonitor :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( ShouldToggle( useType ))
	{
		if(( useType == USE_SET || useType == USE_RESET ) && pActivator->IsPlayer() && HasSpawnFlags(SF_MONITOR_USEABLE) )
		{
			if( useType == USE_SET )
			{
				if( m_iState == STATE_ON )
				{
					UTIL_SetView( pActivator, pev->target );
					m_pController = (CBasePlayer *)pActivator;
					if( HasSpawnFlags(SF_MONITOR_HIDEHUD) )
						m_pController->m_iHideHUD |= HIDEHUD_ALL;
					m_pController->m_pMonitor = this;

					// remember where the player's standing, so we can tell when he walks away
					if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
					{
						// transform controller pos into local space because parent can be moving
						m_vecControllerUsePos = m_pController->EntityToWorldTransform().VectorITransform( m_pController->GetAbsOrigin() );
					}
					else m_vecControllerUsePos = m_pController->GetAbsOrigin();
				}
			}
			else if( useType == USE_RESET )
			{
				if( HasSpawnFlags(SF_MONITOR_HIDEHUD) )
					((CBasePlayer *)pActivator)->m_iHideHUD &= ~HIDEHUD_ALL;
				UTIL_SetView( pActivator );
			}
			return;
		}

		if( pev->iuser3 == -670 )
			pev->iuser3 = 0;
		else
			pev->iuser3 = -670;
		m_iState = (pev->iuser3 == -670) ? STATE_ON : STATE_OFF;

		SetThink( &CFuncMonitor::VisThink );

		if( pev->iuser3 == -670 )
		{
			SetCameraVisibility( TRUE );
			SetNextThink( 0.1f );
		}
		else
		{
			SetCameraVisibility( FALSE );
			DontThink();
		}
	}
}

BOOL CFuncMonitor :: OnControls( CBaseEntity *pTest )
{
	if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
	{
		// transform local controller pos through tankcontrols pos
		Vector vecTransformedControlerUsePos = EntityToWorldTransform().VectorTransform( m_vecControllerUsePos );
		if(( vecTransformedControlerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
			return TRUE;
	}
	else if(( m_vecControllerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
		return TRUE;

	return FALSE;
}





// =================== FUNC_PORTAL ==============================================

#define SF_PORTAL_START_OFF		BIT( 0 )
#define SF_PORTAL_CLIENTONLYFIRE	BIT( 1 )

class CFuncPortal : public CFuncMonitor
{
	DECLARE_CLASS( CFuncPortal, CFuncMonitor );
public:
	void Spawn( void );
	void Touch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void PortalSleep( float seconds ) { m_flDisableTime = gpGlobals->time + seconds; }
	virtual STATE GetState( void ) { return m_iState; };
	virtual BOOL IsPortal( void ) { return TRUE; }
	void StartMessage( CBasePlayer *pPlayer ) {};
	void Activate( void );

	string_t filter;
};

LINK_ENTITY_TO_CLASS( func_portal, CFuncPortal );

void CFuncPortal :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_PORTAL;

	// semisolid portals looks ugly. Disabled
	pev->rendermode = kRenderNormal;
	pev->renderamt = 255;

	SET_MODEL( edict(), GetModel() );

	if( !UTIL_CanRotate( this ))
	{
		ALERT( at_error, "func_portal [%i] can't work without origin brush. Removed.\n", entindex());
		UTIL_Remove( this );
		return;
	}

	SetThink(&CFuncMonitor::VisThink );

	if( HasSpawnFlags(SF_PORTAL_START_OFF) )
	{
		m_iState = STATE_OFF;
		pev->effects |= EF_NODRAW;
		DontThink();
	}
	else
	{
		m_iState = STATE_ON;
		pev->effects &= ~EF_NODRAW;
		SetCameraVisibility( TRUE );
		SetNextThink( 0.1f );
	}
}

void CFuncPortal :: Activate( void )
{
	ChangeCamera( pev->target );
}

void CFuncPortal :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "firetarget" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "filter" ) ) // diffusion - filter by classname or targetname
	{
		pev->noise = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CFuncPortal :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( ShouldToggle( useType ))
	{
		if( m_iState == STATE_ON )
		{
			SetCameraVisibility( FALSE );
			pev->effects |= EF_NODRAW;
			m_iState = STATE_OFF;
			DontThink();
		}
		else if( m_iState == STATE_OFF )
		{
			SetCameraVisibility( TRUE );
			pev->effects &= ~EF_NODRAW;
			m_iState = STATE_ON;
			SetNextThink( 0.1f );
		}
	}
}

void CFuncPortal :: Touch( CBaseEntity *pOther )
{
	CBaseEntity *pTarget = NULL;

	if( IsLockedByMaster( pOther ))
		return;

	if( m_iState == STATE_OFF || m_flDisableTime > gpGlobals->time )
		return; // disabled

	if( pOther->m_iTeleportFilter )
		return;	// we already teleporting somewhere

	// Only teleport monsters or clients or physents
	if( !pOther->IsPlayer() && !pOther->IsMonster() && !pOther->IsPushable() && !pOther->IsProjectile( ) && !pOther->IsRigidBody())
		return;

	pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ));
	if( !pTarget ) return;

	// disable portal to avoid recursion
	m_flDisableTime = gpGlobals->time + 0.5f;

	if( FBitSet( pev->effects, EF_PORTAL ))
	{
		// Build a this --> remote transformation
		matrix4x4 matMyModelToWorld = EntityToWorldTransform();

		// for prevent two-side portals recursion
		if( DotProduct( matMyModelToWorld.GetForward(), pOther->GetAbsVelocity().Normalize()) <= 0.0f )
			return;	// bad direction

		// Teleport our object
		matrix4x4 matRemotePortalTransform = pTarget->EntityToWorldTransform();
		Vector ptNewOrigin, vLook, vNewLook, vVelocity;

		if ( pOther->IsPlayer() )
			UTIL_MakeVectorsPrivate( pOther->pev->v_angle, vLook, NULL, NULL );
		else
			pOther->GetVectors( &vLook, NULL, NULL );

		// Move origin
		ptNewOrigin = matMyModelToWorld.VectorITransform( pOther->GetAbsOrigin() );
		ptNewOrigin = matRemotePortalTransform.VectorTransform( ptNewOrigin );

		TraceResult tr;

//		UTIL_TraceHull( ptNewOrigin, ptNewOrigin, dont_ignore_monsters, head_hull, pOther->edict(), &tr );
		UTIL_TraceEntity( pOther, ptNewOrigin, ptNewOrigin, &tr );

		// make sure that it's enough space here
		if( tr.fAllSolid || tr.fStartSolid )
		{
			ALERT( at_aiconsole, "func_portal: No free space\n" );
			pOther->SetAbsVelocity( g_vecZero );	// g-cont. test thing
			return;
		}

		// Re-aim camera
		vNewLook = matMyModelToWorld.VectorIRotate( vLook );
		vNewLook = matRemotePortalTransform.VectorRotate( vNewLook );

		// Reorient the physics
		vVelocity = matMyModelToWorld.VectorIRotate( pOther->GetAbsVelocity() );
		vVelocity = matRemotePortalTransform.VectorRotate( vVelocity );

		Vector qNewAngles = UTIL_VecToAngles( vNewLook );
		
		if( pOther->IsPlayer() )
			pOther->pev->punchangle.z = pTarget->GetAbsAngles().z; // apply ROLL too

		pOther->Teleport( &ptNewOrigin, &qNewAngles, &vVelocity );
	}
	else
	{
		Vector tmp = pTarget->GetAbsOrigin();
		Vector pAngles = pTarget->GetAbsAngles();

		if( pOther->IsPlayer( ))
			tmp.z -= pOther->pev->mins.z; // make origin adjustments
		tmp.z++;

		pOther->Teleport( &tmp, &pAngles, &g_vecZero );
	}
          
	ChangeCamera( pev->target ); // update PVS
	pOther->pev->flags &= ~FL_ONGROUND;

	if( HasSpawnFlags(SF_PORTAL_CLIENTONLYFIRE) && !pOther->IsPlayer() )
		return;

	UTIL_FireTargets( pev->netname, pOther, this, USE_TOGGLE ); // fire target
}

// =================== INFO_PORTAL_DESTINATION ==============================================

class CInfoPortalDest : public CPointEntity
{
	DECLARE_CLASS( CInfoPortalDest, CPointEntity );
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( info_portal_destination, CInfoPortalDest );

void CInfoPortalDest :: Spawn( void )
{
	SetNullModel();
	SetBits( m_iFlags, MF_POINTENTITY );

	// diffusion - important. when mapper didn't set angles, it doesn't assume that it's zero vector
	if( GetAbsAngles() == g_vecZero )
		SetAbsAngles( g_vecZero );
}