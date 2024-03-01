#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "entities/trains.h"			// trigger_camera has train functionality
#include "triggers.h"

#define SF_CAMERA_PLAYER_POSITION		BIT(0)
#define SF_CAMERA_PLAYER_TARGET			BIT(1)
#define SF_CAMERA_PLAYER_TAKECONTROL	BIT(2)
#define SF_CAMERA_PLAYER_HIDEHUD		BIT(3)
#define SF_CAMERA_CINEMATICBORDER		BIT(4)
#define SF_CAMERA_ABSORG				BIT(5)

class CTriggerCamera : public CBaseDelay
{
	DECLARE_CLASS(CTriggerCamera, CBaseDelay);
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	STATE GetState(void) { return m_state ? STATE_ON : STATE_OFF; }
	void FollowTarget(void);
	void Move(void);
	void Stop(void);

	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	EHANDLE m_hPlayer;
	EHANDLE m_hTarget;
	EHANDLE m_hViewEntity; // diffusion
	CBaseEntity* m_pPath;
	string_t m_sPath;
	string_t m_iszViewEntity;
	float m_flReturnTime;
	float m_flStopTime;
	float m_moveDistance;
	float m_targetSpeed;
	float m_initialSpeed;
	float m_acceleration;
	float m_deceleration;
	float SnapSpeed; // diffusion - how quickly the camera will turn its view to the target
	int CameraFOV; // diffusion - set desired fov for camera view
	int m_state;
};

LINK_ENTITY_TO_CLASS(trigger_camera, CTriggerCamera);

// Global Savedata for changelevel friction modifier
BEGIN_DATADESC(CTriggerCamera)
	DEFINE_FIELD(m_hPlayer, FIELD_EHANDLE),
	DEFINE_FIELD(m_hTarget, FIELD_EHANDLE),
	DEFINE_FIELD(m_pPath, FIELD_CLASSPTR),
	DEFINE_KEYFIELD(m_sPath, FIELD_STRING, "moveto"),
	DEFINE_FIELD(m_flReturnTime, FIELD_TIME),
	DEFINE_FIELD(m_flStopTime, FIELD_TIME),
	DEFINE_FIELD(m_moveDistance, FIELD_FLOAT),
	DEFINE_FIELD(m_targetSpeed, FIELD_FLOAT),
	DEFINE_FIELD(m_initialSpeed, FIELD_FLOAT),
	DEFINE_KEYFIELD(SnapSpeed, FIELD_INTEGER, "snapspeed"),
	DEFINE_KEYFIELD(CameraFOV, FIELD_INTEGER, "fov"),
	DEFINE_KEYFIELD(m_acceleration, FIELD_FLOAT, "acceleration"),
	DEFINE_KEYFIELD(m_deceleration, FIELD_FLOAT, "deceleration"),
	DEFINE_KEYFIELD(m_iszViewEntity, FIELD_STRING, "m_iszViewEntity"),
	DEFINE_FIELD( m_hViewEntity, FIELD_EHANDLE ),
	DEFINE_FIELD(m_state, FIELD_INTEGER),
	DEFINE_FUNCTION(FollowTarget),
END_DATADESC()

void CTriggerCamera::Spawn(void)
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;		// Remove model & collisions
	pev->renderamt = 0;			// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;

	m_initialSpeed = pev->speed;

	if (!SnapSpeed || (SnapSpeed < 1))
		SnapSpeed = 40; // was default HL value

	if (SnapSpeed > 4000)
		SnapSpeed = 4000;

	if (m_acceleration == 0)
		m_acceleration = 500;
	if (m_deceleration == 0)
		m_deceleration = 500;
}

void CTriggerCamera::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "moveto"))
	{
		m_sPath = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "acceleration"))
	{
		m_acceleration = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deceleration"))
	{
		m_deceleration = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszViewEntity"))
	{
		m_iszViewEntity = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "snapspeed"))
	{
		SnapSpeed = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fov"))
	{
		CameraFOV = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}

void CTriggerCamera::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!ShouldToggle(useType, m_state))
		return;

	// Toggle state
	m_state = !m_state;

	if (m_state == 0)
	{
		Stop();
		return;
	}

	if (!pActivator || !pActivator->IsPlayer())
		pActivator = UTIL_PlayerByIndex(1);

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	pPlayer->HideWeapons(TRUE);

	// diffusion - apply custom fov
	if (CameraFOV)
		pPlayer->m_flFOV = CameraFOV;
	else // reset fov if previous camera had different FOV, just in case
		pPlayer->m_flFOV = 0;

	// disable drone control
	if( pPlayer->DroneControl )
		pPlayer->DroneControl = false;

	m_hPlayer = pActivator;

	m_flReturnTime = gpGlobals->time + m_flWait;

	pev->speed = m_initialSpeed;
	m_targetSpeed = m_initialSpeed;

	if (HasSpawnFlags(SF_CAMERA_PLAYER_TARGET))
		m_hTarget = m_hPlayer;
	else
		m_hTarget = GetNextTarget();

	// Nothing to look at!
	// diffusion - camera will be looking using angles, if target was not specified
	if (m_hTarget == NULL)
	{
	//	ALERT(at_console, "%s couldn't find target %s\n", GetClassname(), GetTarget());
	//	return;
	}

	if (HasSpawnFlags(SF_CAMERA_PLAYER_TAKECONTROL))
		((CBasePlayer*)pActivator)->EnableControl(FALSE);

	if (HasSpawnFlags(SF_CAMERA_PLAYER_HIDEHUD))
		((CBasePlayer*)pActivator)->m_iHideHUD |= HIDEHUD_ALL;

	if (m_sPath)
		m_pPath = UTIL_FindEntityByTargetname(NULL, STRING(m_sPath));
	else
		m_pPath = NULL;

	m_flStopTime = gpGlobals->time;
	if (m_pPath)
	{
		if (m_pPath->pev->speed != 0)
			m_targetSpeed = m_pPath->pev->speed;

		m_flStopTime += m_pPath->GetDelay();
	}

	// copy over player information
	if (HasSpawnFlags(SF_CAMERA_PLAYER_POSITION))
	{
		UTIL_SetOrigin(this, pActivator->EyePosition());
		Vector vecAngles;
		vecAngles.x = -pActivator->GetAbsAngles().x;
		vecAngles.y = pActivator->GetAbsAngles().y;
		vecAngles.z = 0;
		SetLocalVelocity(pActivator->GetAbsVelocity());
		SetLocalAngles(vecAngles);
	}
	else
		SetLocalVelocity(g_vecZero);

	if (m_iszViewEntity)
	{
		m_hViewEntity = UTIL_FindEntityByTargetname(NULL, STRING(m_iszViewEntity));

		if ( m_hViewEntity != NULL )
			SET_VIEW(pActivator->edict(), m_hViewEntity->edict());
		else
		{
			ALERT(at_error, "%s couldn't find view entity %s\n", GetClassname(), STRING(m_iszViewEntity));
			SET_VIEW(pActivator->edict(), edict());
			SET_MODEL(edict(), pActivator->GetModel());
		}
	}
	else
	{
		SET_VIEW(pActivator->edict(), edict());
		SET_MODEL(edict(), pActivator->GetModel());
	}

	// now copy the name to player who's using us
	if( pev->targetname != NULL )
	{
		Q_strcpy( ((CBasePlayer *)((CBaseEntity *)m_hPlayer))->CameraEntity, GetTargetname() );
		if(HasSpawnFlags(SF_CAMERA_CINEMATICBORDER))
			m_hPlayer->pev->effects |= EF_PLAYERUSINGCAMERA;
	}

	SetFlag(F_ENTITY_BUSY);
	// follow the player down
	SetThink(&CTriggerCamera::FollowTarget);
	SetNextThink(0);

	m_moveDistance = 0;
	Move();
}

void CTriggerCamera::FollowTarget(void)
{
	if (m_hPlayer == NULL)
		return;

	// player's camera has changed, switch off
	if ( pev->targetname )
	{
		if( Q_strcmp( ((CBasePlayer*)((CBaseEntity*)m_hPlayer))->CameraEntity, GetTargetname()) )
		{
			RemoveFlag(F_ENTITY_BUSY);
			SetLocalVelocity(g_vecZero);
			SetLocalAvelocity(g_vecZero);
			DontThink();
			m_state = 0;
			return;
		}
	}

	// update this to player
	if( m_hViewEntity != NULL )
	{
		((CBasePlayer *)((CBaseEntity *)m_hPlayer))->CameraOrigin = m_hViewEntity->GetAbsOrigin();
		((CBasePlayer *)((CBaseEntity *)m_hPlayer))->CameraAngles = m_hViewEntity->GetAbsAngles();
	}
	else
	{
		((CBasePlayer *)((CBaseEntity *)m_hPlayer))->CameraOrigin = GetAbsOrigin();
		((CBasePlayer *)((CBaseEntity *)m_hPlayer))->CameraAngles = GetAbsAngles();
	}

	// diffusion - update target, maybe it was changed by changetarget.
	if (!HasSpawnFlags(SF_CAMERA_PLAYER_TARGET)) // without this check the game will crash upon using camera
		m_hTarget = GetNextTarget();

	if( m_hTarget != NULL )
	{
		if( (m_hTarget->IsMonster() && !m_hTarget->IsAlive())
			|| !m_hPlayer->IsAlive() || (m_flWait != -1 && m_flReturnTime < gpGlobals->time) )
		{
			Stop();
			return;
		}
	}

	Vector angles = GetLocalAngles();
	Vector vecGoal = angles;

	if( m_hTarget == NULL )
	{
		// skip condition
	}
	else if(HasSpawnFlags(SF_CAMERA_ABSORG))
		vecGoal = UTIL_VecToAngles(m_hTarget->GetAbsOrigin() - GetAbsOrigin()); // diffusion - changed localorg to absorg, the camera now looks to parented objects (but not always works!!!)
	else
		vecGoal = UTIL_VecToAngles( m_hTarget->GetLocalOrigin() - GetLocalOrigin() );

	if (angles.y > 360)
		angles.y -= 360;

	if (angles.y < 0)
		angles.y += 360;

	SetLocalAngles(angles);

	float dx = vecGoal.x - angles.x;
	float dy = vecGoal.y - angles.y;

	if (dx < -180)
		dx += 360;
	if (dx > 180)
		dx = dx - 360;

	if (dy < -180)
		dy += 360;
	if (dy > 180)
		dy = dy - 360;

	Vector vecAvel;

	vecAvel.x = dx * SnapSpeed * 0.01;  // 0.01 instead of frametime
	vecAvel.y = dy * SnapSpeed * 0.01;
	vecAvel.z = 0;

	SetLocalAvelocity(vecAvel);

	if (!(FBitSet(pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL)))
	{
		SetLocalVelocity(GetLocalVelocity() * 0.8f);
		if (GetLocalVelocity().Length() < 10.0f)
			SetLocalVelocity(g_vecZero);
	}

	SetNextThink(0);

	Move();
}

void CTriggerCamera::Stop(void)
{
	if (m_hPlayer != NULL)
	{
		SET_VIEW(m_hPlayer->edict(), m_hPlayer->edict());
		((CBasePlayer*)((CBaseEntity*)m_hPlayer))->EnableControl(TRUE);
		((CBasePlayer*)((CBaseEntity*)m_hPlayer))->CameraEntity[0] = '\0'; // set to NULL
	}

	if (HasSpawnFlags(SF_CAMERA_PLAYER_HIDEHUD))
		((CBasePlayer*)((CBaseEntity*)m_hPlayer))->m_iHideHUD &= ~HIDEHUD_ALL;

	((CBasePlayer*)((CBaseEntity*)m_hPlayer))->m_flFOV = 0; // reset fov to default
	((CBasePlayer*)((CBaseEntity*)m_hPlayer))->HideWeapons(FALSE);

	RemoveFlag(F_ENTITY_BUSY);

	//	SUB_UseTargets( this, USE_TOGGLE, 0 );  // diffusion - this isn't needed
	SetLocalVelocity(g_vecZero);
	SetLocalAvelocity(g_vecZero);
	DontThink();
	m_state = 0;
}

void CTriggerCamera::Move(void)
{
	// Not moving on a path, return
	if (!m_pPath)
	{
		SetLocalVelocity( g_vecZero ); // diffusion - FIX? After reaching last path_corner, the camera continued to move.
		return;
	}

	// Subtract movement from the previous frame
	m_moveDistance -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if (m_moveDistance <= 0)
	{
		// Fire the passtarget if there is one
		if (m_pPath->pev->message)
		{
			UTIL_FireTargets(STRING(m_pPath->pev->message), this, this, USE_TOGGLE, 0);
			if (m_pPath->HasSpawnFlags(SF_CORNER_FIREONCE))
				m_pPath->pev->message = 0;
		}
		// Time to go to the next target
		m_pPath = m_pPath->GetNextTarget();

		// Set up next corner
		if (!m_pPath)
		{
			SetLocalVelocity(g_vecZero);
		}
		else
		{
			if (m_pPath->pev->speed != 0)
				m_targetSpeed = m_pPath->pev->speed;

			Vector delta = m_pPath->GetLocalOrigin() - GetLocalOrigin();
			m_moveDistance = delta.Length();
			pev->movedir = delta.Normalize();
			m_flStopTime = gpGlobals->time + m_pPath->GetDelay();
		}
	}

	if (m_flStopTime > gpGlobals->time)
		pev->speed = UTIL_Approach(0, pev->speed, m_deceleration * gpGlobals->frametime);
	else
		pev->speed = UTIL_Approach(m_targetSpeed, pev->speed, m_acceleration * gpGlobals->frametime);

	float fraction = 2 * gpGlobals->frametime;
	SetLocalVelocity(((pev->movedir * pev->speed) * fraction) + (GetLocalVelocity() * (1.0f - fraction)));

	RelinkEntity();
}