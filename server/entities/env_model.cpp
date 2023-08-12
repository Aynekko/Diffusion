#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "studio.h"
#include "animation.h"

// =================== ENV_MODEL ==============================================

#define SF_ENVMODEL_OFF				BIT( 0 )
#define SF_ENVMODEL_DROPTOFLOOR		BIT( 1 )
#define SF_ENVMODEL_SOLID			BIT( 2 )
// diffusion - new stuff
#define SF_ENVMODEL_TOGGLEVISIBLE	BIT( 3 ) // toggle model visibility on use
#define SF_ENVMODEL_TRANSLUCENT		BIT( 4 ) // initially translucent
#define SF_ENVMODEL_REMOVEONUSE		BIT( 5 ) // delete model on use

#define SF_ENVMODEL_OWNERDAMAGE		BIT(31) // internal flag, being set by cars

class CEnvModel : public CBaseAnimating
{
	DECLARE_CLASS(CEnvModel, CBaseAnimating);
public:
	void Spawn(void);
	void Precache(void);
	void Think(void);
	void KeyValue(KeyValueData* pkvd);
	STATE GetState(void) { return FBitSet(pev->spawnflags, SF_ENVMODEL_OFF) ? STATE_OFF : STATE_ON; }
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void SetSequence(void);
	void AutoSetSize(void);
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );

	DECLARE_DATADESC();

	string_t m_iszSequence_On;
	string_t m_iszSequence_Off;
	int m_iAction_On;
	int m_iAction_Off;
};

LINK_ENTITY_TO_CLASS(env_model, CEnvModel);

BEGIN_DATADESC(CEnvModel)
	DEFINE_KEYFIELD(m_iszSequence_On, FIELD_STRING, "m_iszSequence_On"),
	DEFINE_KEYFIELD(m_iszSequence_Off, FIELD_STRING, "m_iszSequence_Off"),
	DEFINE_KEYFIELD(m_iAction_On, FIELD_INTEGER, "m_iAction_On"),
	DEFINE_KEYFIELD(m_iAction_Off, FIELD_INTEGER, "m_iAction_Off"),
END_DATADESC()

void CEnvModel::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszSequence_On"))
	{
		m_iszSequence_On = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSequence_Off"))
	{
		m_iszSequence_Off = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_On"))
	{
		m_iAction_On = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_Off"))
	{
		m_iAction_Off = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}

void CEnvModel::Spawn(void)
{
	Precache();
	
	SET_MODEL(edict(), STRING(pev->model));
	
	RelinkEntity(TRUE);
	
	SetBoneController(0, 0);
	SetBoneController(1, 0);
	
	SetSequence();

	if( !pev->scale )
		pev->scale = 1;

	if ( HasSpawnFlags(SF_ENVMODEL_SOLID))
	{
		if (UTIL_AllowHitboxTrace(this))
			pev->solid = SOLID_BBOX;
		else pev->solid = SOLID_SLIDEBOX;
		AutoSetSize();
	}

	if ( HasSpawnFlags(SF_ENVMODEL_DROPTOFLOOR))
	{
		Vector origin = GetLocalOrigin();
		origin.z += 1;
		SetLocalOrigin(origin);
		UTIL_DropToFloor(this);
	}

	if (HasSpawnFlags(SF_ENVMODEL_TRANSLUCENT) && HasSpawnFlags(SF_ENVMODEL_TOGGLEVISIBLE))
		pev->effects |= EF_NODRAW;

	SetNextThink(0.1);
}

void CEnvModel::Precache(void)
{
	PRECACHE_MODEL(GetModel());
}

void CEnvModel::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (pev->spawnflags & SF_ENVMODEL_REMOVEONUSE)
	{
		UTIL_Remove(this);
		return;
	}

	if (ShouldToggle(useType))
	{
		if (pev->spawnflags & SF_ENVMODEL_OFF)
			pev->spawnflags &= ~SF_ENVMODEL_OFF;
		else pev->spawnflags |= SF_ENVMODEL_OFF;

		if (pev->spawnflags & SF_ENVMODEL_TOGGLEVISIBLE)
		{
			if( pev->effects & EF_NODRAW )
				pev->effects &= ~EF_NODRAW;
			else
				pev->effects |= EF_NODRAW;
		}

		SetSequence();
		SetNextThink(0.1);
	}
}

// automatically set collision box
void CEnvModel::AutoSetSize(void)
{
	studiohdr_t* pstudiohdr;
	pstudiohdr = (studiohdr_t*)GET_MODEL_PTR(edict());

	if (pstudiohdr == NULL)
	{
		UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10));
		ALERT(at_error, "env_model: unable to fetch model pointer!\n");
		return;
	}

	mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex);
	Vector mins = pseqdesc[pev->sequence].bbmin;
	Vector maxs = pseqdesc[pev->sequence].bbmax;
	UTIL_SetSize(pev, mins, maxs);
}

void CEnvModel::Think(void)
{
	StudioFrameAdvance(); // set m_fSequenceFinished if necessary

	if (m_fSequenceFinished && !m_fSequenceLoops)
	{
		int iTemp;

		if (pev->spawnflags & SF_ENVMODEL_OFF)
			iTemp = m_iAction_Off;
		else
			iTemp = m_iAction_On;
		
		switch (iTemp)
		{
		case 2:	// change state
			if (pev->spawnflags & SF_ENVMODEL_OFF)
				pev->spawnflags &= ~SF_ENVMODEL_OFF;
			else pev->spawnflags |= SF_ENVMODEL_OFF;
			SetSequence();
			break;
		default:	// remain frozen
			return;
		break;
		}
	}
	SetNextThink(0.1);
}

void CEnvModel::SetSequence(void)
{
	int iszSeq;

	if (pev->spawnflags & SF_ENVMODEL_OFF)
		iszSeq = m_iszSequence_Off;
	else
		iszSeq = m_iszSequence_On;

	// diffusion - sometimes the mapper doesn't set any action sequences, but model keeps thinking
	if( !iszSeq )
		pev->sequence = 0;
	else
		pev->sequence = LookupSequence(STRING(iszSeq));

	if (pev->sequence == -1)
	{
		if (pev->targetname)
			ALERT(at_error, "env_model %s: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSeq));
		else
			ALERT(at_error, "env_model: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSeq));
		pev->sequence = 0;
	}

	pev->frame = 0;

//	ResetSequenceInfo();
	void *pmodel = GET_MODEL_PTR( ENT( pev ) );
	GetSequenceInfo( pmodel, pev->sequence, &m_flFrameRate, &m_flGroundSpeed );
	m_fSequenceLoops = ((GetSequenceFlags() & STUDIO_LOOPING) != 0);
	pev->animtime = gpGlobals->time;
	if( pev->framerate == 0.0f ) // diffusion - mapper should be able to set custom framerate...
		pev->framerate = 1.0f;
	m_fSequenceFinished = FALSE;
	m_flLastEventCheck = gpGlobals->time;

	if (pev->spawnflags & SF_ENVMODEL_OFF)
	{
		if (m_iAction_Off == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
	else
	{
		if (m_iAction_On == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
}

void CEnvModel::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( HasSpawnFlags( SF_ENVMODEL_OWNERDAMAGE ) ) // just transfer everything to owner (a vehicle)
	{
		if( pev->owner != NULL )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
			pOwner->TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
		}
	}
}

int CEnvModel::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( HasSpawnFlags( SF_ENVMODEL_OWNERDAMAGE ))  // just transfer everything to owner (a vehicle)
	{
		if( pev->owner != NULL )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
			pOwner->TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
		}
	}

	return 0;
}