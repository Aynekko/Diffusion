#include "extdll.h"
#include "util.h"
#include "cbase.h"
//#include "entities/trains.h"

//===================================================================================
// diffusion - delete up to 8 entities from the map at once (including the children)
//===================================================================================

#define SF_MKILLER_ALSOCHILD1 BIT(0) // delete children of the target too
#define SF_MKILLER_ALSOCHILD2 BIT(1)
#define SF_MKILLER_ALSOCHILD3 BIT(2)
#define SF_MKILLER_ALSOCHILD4 BIT(3)
#define SF_MKILLER_ALSOCHILD5 BIT(4)
#define SF_MKILLER_ALSOCHILD6 BIT(5)
#define SF_MKILLER_ALSOCHILD7 BIT(6)
#define SF_MKILLER_ALSOCHILD8 BIT(7)

class CTriggerMultikiller : public CBaseDelay
{
	DECLARE_CLASS(CTriggerMultikiller, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KillEntity( CBaseEntity *pKillEnt, string_t KillTarget, bool AlsoChildren );

	string_t killtarget1;
	string_t killtarget2;
	string_t killtarget3;
	string_t killtarget4;
	string_t killtarget5;
	string_t killtarget6;
	string_t killtarget7;
	string_t killtarget8;

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(multikiller, CTriggerMultikiller);

BEGIN_DATADESC(CTriggerMultikiller)
	DEFINE_KEYFIELD(killtarget1, FIELD_STRING, "killtarget1"),
	DEFINE_KEYFIELD(killtarget2, FIELD_STRING, "killtarget2"),
	DEFINE_KEYFIELD(killtarget3, FIELD_STRING, "killtarget3"),
	DEFINE_KEYFIELD(killtarget4, FIELD_STRING, "killtarget4"),
	DEFINE_KEYFIELD(killtarget5, FIELD_STRING, "killtarget5"),
	DEFINE_KEYFIELD(killtarget6, FIELD_STRING, "killtarget6"),
	DEFINE_KEYFIELD(killtarget7, FIELD_STRING, "killtarget7"),
	DEFINE_KEYFIELD(killtarget8, FIELD_STRING, "killtarget8"),
END_DATADESC();

void CTriggerMultikiller::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "killtarget1"))
	{
		killtarget1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget2"))
	{
		killtarget2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget3"))
	{
		killtarget3 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget4"))
	{
		killtarget4 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget5"))
	{
		killtarget5 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget6"))
	{
		killtarget6 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget7"))
	{
		killtarget7 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget8"))
	{
		killtarget8 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerMultikiller::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	// killing targets if they are set in the entity
	CBaseEntity *pKillEnt = NULL;

	if (!FStringNull(killtarget1))
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING(killtarget1) );
		KillEntity( pKillEnt, killtarget1, HasSpawnFlags( SF_MKILLER_ALSOCHILD1 ) ? true : false );
	}

	if( !FStringNull( killtarget2 ) )
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( killtarget2 ) );
		KillEntity( pKillEnt, killtarget2, HasSpawnFlags( SF_MKILLER_ALSOCHILD2 ) ? true : false );
	}

	if( !FStringNull( killtarget3 ) )
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( killtarget3 ) );
		KillEntity( pKillEnt, killtarget3, HasSpawnFlags( SF_MKILLER_ALSOCHILD3 ) ? true : false );
	}

	if( !FStringNull( killtarget4 ) )
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( killtarget4 ) );
		KillEntity( pKillEnt, killtarget4, HasSpawnFlags( SF_MKILLER_ALSOCHILD4 ) ? true : false );
	}

	if( !FStringNull( killtarget5 ) )
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( killtarget5 ) );
		KillEntity( pKillEnt, killtarget5, HasSpawnFlags( SF_MKILLER_ALSOCHILD5 ) ? true : false );
	}

	if( !FStringNull( killtarget6 ) )
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( killtarget6 ) );
		KillEntity( pKillEnt, killtarget6, HasSpawnFlags( SF_MKILLER_ALSOCHILD6 ) ? true : false );
	}

	if( !FStringNull( killtarget7 ) )
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( killtarget7 ) );
		KillEntity( pKillEnt, killtarget7, HasSpawnFlags( SF_MKILLER_ALSOCHILD7 ) ? true : false );
	}

	if( !FStringNull( killtarget8 ) )
	{
		pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( killtarget8 ) );
		KillEntity( pKillEnt, killtarget8, HasSpawnFlags( SF_MKILLER_ALSOCHILD8 ) ? true : false );
	}
}

void CTriggerMultikiller::KillEntity( CBaseEntity *pKillEnt, string_t KillTarget, bool AlsoChildren )
{
	if( !pKillEnt )
	{
		if( !FStringNull( KillTarget ) )
			ALERT( at_aiconsole, "Warning: Multikiller \"%s\" couldn't find entity named \"%s\"\n", GetTargetname(), STRING( KillTarget ) );

		return;
	}

	while( pKillEnt )
	{
		// recursive check for children (delete the whole tree)
		if( AlsoChildren )
		{
			CBaseEntity *pChild = pKillEnt->m_hChild;

			while( pChild )
			{
				CBaseEntity *pNext = pChild->m_hNextChild;
				KillEntity( pChild, NULL, true );
				pChild = pNext;
			}
		}

		ALERT( at_aiconsole, "Multikiller \"%s\" deleted %s \"%s\"\n", GetTargetname(), pKillEnt->GetClassname(), pKillEnt->GetTargetname() );

		// hack for ambient generic - stop sound if we are deleting this entity
		// I have to do this, otherwise the sounds will pop in random places (other edicts) because they can't find their parent edict, since it was deleted
		if( FClassnameIs( pKillEnt, "ambient_generic" ) )
			pKillEnt->Use( this, this, USE_OFF, 0 );

		UTIL_Remove( pKillEnt );
		pKillEnt = NULL; // it appears this doesn't become NULL after removal, so I have to do this manually (guess there's nothing wrong with that, but I'm not sure)

		if( !FStringNull(KillTarget) )
			pKillEnt = UTIL_FindEntityByTargetname( NULL, STRING( KillTarget ) );
	}
}