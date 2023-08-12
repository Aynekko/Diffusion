#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/client.h"
#include "player.h"
#include "entities/func_break.h"

// =================== FUNC_WALL ==============================================

#define SF_WALL_START_OFF		BIT(0)
#define SF_WALL_DOORPART		BIT(1) // diffusion - this func_wall is (i.e.) a part of the door. Allow nodes to build the path through it

class CFuncWall : public CBaseDelay
{
	DECLARE_CLASS(CFuncWall, CBaseDelay);
public:
	void Spawn(void);
	virtual void TurnOn(void);
	virtual void TurnOff(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );
	int MonstersCanSeeThrough; // diffusion

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(func_wall, CFuncWall);

BEGIN_DATADESC( CFuncWall )
	DEFINE_KEYFIELD( MonstersCanSeeThrough, FIELD_BOOLEAN, "monstersee" ),
END_DATADESC()

void CFuncWall::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "monstersee" ) )
	{
		MonstersCanSeeThrough = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CFuncWall::Spawn(void)
{
	if (HasSpawnFlags(SF_WALL_DOORPART))
		pev->flags &= ~FL_WORLDBRUSH;
	else
		pev->flags |= FL_WORLDBRUSH;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	m_iState = STATE_OFF;

	SetLocalAngles(g_vecZero);
	SET_MODEL(edict(), GetModel());

	// LRC (support generic switchable texlight)
	if (m_iStyle >= 32)
		LIGHT_STYLE(m_iStyle, "a");
	else if (m_iStyle <= -32)
		LIGHT_STYLE(-m_iStyle, "z");

	if (m_hParent != NULL || FClassnameIs(pev, "func_wall_toggle"))
		m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);
	else
		m_pUserData = WorldPhysic->CreateStaticBodyFromEntity(this);

	if( MonstersCanSeeThrough == 1 )
		SetFlag( F_MONSTER_CAN_SEE_THROUGH );
}

void CFuncWall::TurnOff(void)
{
	pev->frame = 0;
	m_iState = STATE_OFF;
}

void CFuncWall::TurnOn(void)
{
	pev->frame = 1;
	m_iState = STATE_ON;
}

void CFuncWall::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (ShouldToggle(useType))
	{
		if (GetState() == STATE_ON)
			TurnOff();
		else TurnOn();

		// LRC (support generic switchable texlight)
		if (m_iStyle >= 32)
		{
			if (pev->frame)
				LIGHT_STYLE(m_iStyle, "z");
			else
				LIGHT_STYLE(m_iStyle, "a");
		}
		else if (m_iStyle <= -32)
		{
			if (pev->frame)
				LIGHT_STYLE(-m_iStyle, "a");
			else
				LIGHT_STYLE(-m_iStyle, "z");
		}
	}
}

// =================== FUNC_WALL_TOGGLE ==============================================

class CFuncWallToggle : public CFuncWall
{
	DECLARE_CLASS(CFuncWallToggle, CFuncWall);
public:
	void Spawn(void);
	void TurnOff(void);
	void TurnOn(void);
};

LINK_ENTITY_TO_CLASS(func_wall_toggle, CFuncWallToggle);

void CFuncWallToggle::Spawn(void)
{
	BaseClass::Spawn();

	if (HasSpawnFlags(SF_WALL_START_OFF))
		TurnOff();
	else TurnOn();
}

void CFuncWallToggle::TurnOff(void)
{
	WorldPhysic->EnableCollision(this, FALSE);
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	RelinkEntity(FALSE);
	m_iState = STATE_OFF;
}

void CFuncWallToggle::TurnOn(void)
{
	WorldPhysic->EnableCollision(this, TRUE);
	pev->solid = SOLID_BSP;
	pev->effects &= ~EF_NODRAW;
	RelinkEntity(FALSE);
	m_iState = STATE_ON;
}