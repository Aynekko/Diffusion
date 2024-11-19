#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"

#define SF_WARPBALL_REMOVE_ON_FIRE	BIT(0)
#define SF_WARPBALL_KILL_CENTER		BIT(1)
#define SF_WARPBALL_NOSOUND			BIT(2)
#define SF_WARPBALL_ANDREW			BIT(3) // Andrew Rich boss warp effect (custom beam color and sound)

class CEnvWarpBall : public CBaseEntity
{
	DECLARE_CLASS(CEnvWarpBall, CBaseEntity);
public:
	void Precache(void);
	void Spawn(void) { Precache(); }
	void Think(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	Vector vecOrigin;
};

LINK_ENTITY_TO_CLASS(env_warpball, CEnvWarpBall);

void CEnvWarpBall::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->button = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "warp_target"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "damage_delay"))
	{
		pev->frags = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue(pkvd);
}

void CEnvWarpBall::Precache(void)
{
	PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_MODEL("sprites/Fexplo1.spr");

	if( HasSpawnFlags( SF_WARPBALL_ANDREW ) )
	{
		PRECACHE_SOUND( "hgrunt/rich_warp.wav" );
	}
	else
	{
		PRECACHE_SOUND( "debris/beamstart2.wav" );
		PRECACHE_SOUND( "debris/beamstart7.wav" );
	}
}

void CEnvWarpBall::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam* pBeam;
	CBaseEntity* pEntity = UTIL_FindEntityByTargetname(NULL, STRING(pev->message));
	edict_t* pos;

	if (pEntity) // target found ?
	{
		vecOrigin = pEntity->GetAbsOrigin();
		pos = pEntity->edict();
	}
	else
	{	
		// use as center
		vecOrigin = GetAbsOrigin();
		pos = edict();
	}

	if( HasSpawnFlags( SF_WARPBALL_ANDREW ) ) // custom Andrew Rich mode
	{
		if( !HasSpawnFlags( SF_WARPBALL_NOSOUND ) )
			EMIT_SOUND( edict(), CHAN_BODY, "hgrunt/rich_warp.wav", 1, 0.1 );
		UTIL_ScreenShake( vecOrigin, 6, 160, 1.0, 666 );
		CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", vecOrigin, TRUE );
		pSpr->AnimateAndDie( 18 );
		pSpr->SetTransparency( kRenderGlow, 77, 210, 130, 255, kRenderFxNoDissipation );
		pSpr->SetScale( 2.0f );
		int iBeams = RANDOM_LONG( 20, 40 );
		while( iDrawn < iBeams && iTimes < (iBeams * 3) )
		{
			vecDest = 300 * (Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize());
			UTIL_TraceLine( vecOrigin, vecOrigin + vecDest, ignore_monsters, NULL, &tr );
			if( tr.flFraction != 1.0 )
			{
				// we hit something.
				iDrawn++;
				pBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 200 );
				pBeam->PointsInit( vecOrigin, tr.vecEndPos );
				if( RANDOM_LONG( 0, 100 ) > 80 ) // chance for red beam
					pBeam->SetColor( 255, 25, 25 );
				else // shades of blue-ish
				{
					pBeam->pev->rendercolor.x = 90;
					pBeam->pev->rendercolor.y = RANDOM_LONG( 110, 140 );
					pBeam->pev->rendercolor.z = 240;
				}
				pBeam->SetNoise( 65 );
				pBeam->SetBrightness( 220 );
				pBeam->SetWidth( 15 );
				pBeam->SetScrollRate( 35 );
				pBeam->SetThink( &CBeam::SUB_Remove );
				pBeam->pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.5, 1.6 );
			}
			iTimes++;
		}

		Vector LightOrg = GetAbsOrigin();
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, LightOrg );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( LightOrg.x );		// origin
		WRITE_COORD( LightOrg.y );
		WRITE_COORD( LightOrg.z );
		WRITE_BYTE( 35 );	// radius
		WRITE_BYTE( 150 );	// R
		WRITE_BYTE( 220 );	// G
		WRITE_BYTE( 230 );	// B
		WRITE_BYTE( 10 );	// life * 10
		WRITE_BYTE( 25 ); // decay
		WRITE_BYTE( 125 ); // brightness
		WRITE_BYTE( 0 ); // shadows
		MESSAGE_END();
	}
	else // original warpball
	{
		if( !HasSpawnFlags( SF_WARPBALL_NOSOUND ) )
		{
			EMIT_SOUND( pos, CHAN_BODY, "debris/beamstart2.wav", 1, ATTN_NORM );
			EMIT_SOUND( pos, CHAN_ITEM, "debris/beamstart7.wav", 1, ATTN_NORM );
		}
		UTIL_ScreenShake( vecOrigin, 6, 160, 1.0, pev->button );
		CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", vecOrigin, TRUE );
		pSpr->SetParent( this );
		pSpr->AnimateAndDie( 18 );
		pSpr->SetTransparency( kRenderGlow, 77, 210, 130, 255, kRenderFxNoDissipation );
		int iBeams = RANDOM_LONG( 20, 40 );
		while( iDrawn < iBeams && iTimes < (iBeams * 3) )
		{
			vecDest = pev->button * (Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize());
			UTIL_TraceLine( vecOrigin, vecOrigin + vecDest, ignore_monsters, NULL, &tr );
			if( tr.flFraction != 1.0 )
			{
				// we hit something.
				iDrawn++;
				pBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 200 );
				pBeam->PointsInit( vecOrigin, tr.vecEndPos );
				pBeam->SetColor( 20, 243, 20 );
				pBeam->SetNoise( 65 );
				pBeam->SetBrightness( 220 );
				pBeam->SetWidth( 30 );
				pBeam->SetScrollRate( 35 );
				//			pBeam->SetParent( this );
				pBeam->SetThink( &CBeam::SUB_Remove );
				pBeam->pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.5, 1.6 );
			}
			iTimes++;
		}
	}

	SetNextThink( pev->frags > 0.0f ? pev->frags : 0.0f );
}

void CEnvWarpBall::Think(void)
{
	SUB_UseTargets(this, USE_TOGGLE, 0);

	if( HasSpawnFlags( SF_WARPBALL_KILL_CENTER ) )
	{
		CBaseEntity* pMonster = NULL;

		while ((pMonster = UTIL_FindEntityInSphere(pMonster, vecOrigin, 72)) != NULL)
		{
			if (FBitSet(pMonster->pev->flags, FL_MONSTER) || FClassnameIs(pMonster->pev, "player"))
				pMonster->TakeDamage(pev, pev, 100, DMG_GENERIC);
		}
	}
	if( HasSpawnFlags( SF_WARPBALL_REMOVE_ON_FIRE ) )
		UTIL_Remove( this );
}