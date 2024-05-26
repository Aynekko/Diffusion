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
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "player.h"
#include "effects.h"
#include "game/gamerules.h"

#define TRIPMINE_PRIMARY_VOLUME	450

#define SF_TRIPMINE_INSTANT_ON	BIT(0)

class CTripmineGrenade : public CGrenade
{
	DECLARE_CLASS( CTripmineGrenade, CGrenade );

	void Spawn( void );
	void Precache( void );

	DECLARE_DATADESC();

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	
	void PowerupThink( void );
	void BeamBreakThink( void );
	void DelayDeathThink( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	void TransferReset( void );
	void OnChangeLevel( void );

	void MakeBeam( void );

	void ClearEffects(void);

	virtual int ObjectCaps( void )
	{
		int flags = 0;
		if( !g_pGameRules->IsMultiplayer() )
			flags = FCAP_IMPULSE_USE;

		return BaseClass :: ObjectCaps() | flags;
	}
	void DisarmUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void DisarmThink(void);
	void BreakTouch( CBaseEntity *pOther );
	float DisarmStartTime;
	bool Disarmed;

	float	m_flPowerUp;
	Vector	m_vecDir;
	Vector	m_vecEnd;
	float	m_flBeamLength;

	CBeam	*m_pBeam;
	edict_t	*m_pRealOwner; // tracelines don't hit pev->owner, which means a player couldn't detonate his own trip mine, so we store the owner here.

	CSprite *m_pSprite; // diffusion - glow sprite at the end of laser

	float skin_change_time; // not saved
	bool IsTripped; // do not the beam!
};

LINK_ENTITY_TO_CLASS( monster_tripmine, CTripmineGrenade );

BEGIN_DATADESC( CTripmineGrenade )
	DEFINE_FIELD( m_flPowerUp, FIELD_TIME ),
	DEFINE_FIELD( m_pBeam, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pRealOwner, FIELD_EDICT ),
	DEFINE_FIELD( m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecEnd, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flBeamLength, FIELD_FLOAT ),
	DEFINE_FUNCTION( PowerupThink ),
	DEFINE_FUNCTION( BeamBreakThink ),
	DEFINE_FUNCTION( DelayDeathThink ),
	DEFINE_FUNCTION( DisarmUse ),
	DEFINE_FUNCTION( DisarmThink ),
	DEFINE_FUNCTION( ClearEffects ),
	DEFINE_FUNCTION( BreakTouch ),
	DEFINE_FIELD( DisarmStartTime, FIELD_TIME ),
	DEFINE_FIELD( Disarmed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_pSprite, FIELD_CLASSPTR ),
END_DATADESC()

void CTripmineGrenade :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	SET_MODEL( edict(), "models/weapons/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_WORLD;
	ResetSequenceInfo( );
	pev->framerate = 0;
	SetBits( pev->effects, EF_NOINTERP );
		
	UTIL_SetSize( pev, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ));

	if( !m_fSetAngles )
		SetLocalAngles( g_vecZero );

	if (HasSpawnFlags(SF_TRIPMINE_INSTANT_ON))
	{
		// power up quickly
		m_flPowerUp = gpGlobals->time + 1.0;
	}
	else
	{
		// power up in 2.5 seconds
		m_flPowerUp = gpGlobals->time + 2.5;
	}

	// indicate to show BUSY on the screen
	SetFlag( F_ENTITY_BUSY | F_NOT_A_MONSTER );

	pev->takedamage = DAMAGE_YES;
	pev->dmg = 150;
	pev->health = 1; // don't let die normally

	if (pev->owner != NULL )
	{
		// play deploy sound
		EMIT_SOUND( edict(), CHAN_VOICE, "weapons/mine_deploy.wav", 1.0, ATTN_NORM );
		EMIT_SOUND( edict(), CHAN_BODY, "weapons/mine_charge.wav", 0.2, ATTN_NORM ); // chargeup

		m_pRealOwner = pev->owner;// see CTripmineGrenade for why.
	}

	Disarmed = false;

	if( m_hOwner == NULL )
	{
		// find an owner
		edict_t *oldowner = pev->owner;
		pev->owner = NULL;

		if( m_hParent != NULL )
			m_hOwner = m_hParent; // g-cont. my owner is parent!
		else
			m_hOwner = g_pWorld;
	}

	SetThink( &CTripmineGrenade::PowerupThink );
	SetNextThink( 0.2 );
}

void CTripmineGrenade :: Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_tripmine.mdl");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("weapons/mine_activate.wav");
	PRECACHE_SOUND("weapons/mine_charge.wav");
	PRECACHE_SOUND("weapons/mine_disarm.wav");
	PRECACHE_SOUND("weapons/mine_tripped.wav");
	PRECACHE_MODEL( "sprites/glow01.spr" );
}

void CTripmineGrenade::DisarmUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
	pPlayer->EnableControl( FALSE );
	int ObjectCaps = 0;
	EMIT_SOUND( ENT(pev), CHAN_BODY, "weapons/mine_disarm.wav", 0.75, ATTN_NORM );
	SetUse( NULL );
	SetFlag(F_ENTITY_BUSY); // indicate to show BUSY on the screen
	DisarmStartTime = gpGlobals->time;
	SetThink(&CTripmineGrenade::DisarmThink );
}

void CTripmineGrenade::DisarmThink(void)
{
	if( gpGlobals->time > DisarmStartTime + 2 )
	{
		RemoveFlag(F_ENTITY_BUSY);
		SetFlag(F_ENTITY_UNUSEABLE);
		ClearEffects();
	//	SetTouch(&CTripmineGrenade::BreakTouch );
		Disarmed = true;
		SetThink( NULL );
		CBasePlayer *pPlayer = NULL;
		while ( ((pPlayer = (CBasePlayer*)UTIL_FindEntityByClassname( pPlayer, "player" )) != NULL) && (!FNullEnt(pPlayer->edict())) )
		{
			pPlayer->EnableControl( TRUE );
		//	pPlayer->AchievementStats[ACH_DISARMEDMINES]++;
			pPlayer->SendAchievementStatToClient( ACH_DISARMEDMINES, 1, 0 );
		}
		pev->dmg *= 0.5; // do half damage when disarmed
	}
	else
	{
		if( m_pBeam )
		{
			if( m_pBeam->pev->renderamt > 2 )
				m_pBeam->pev->renderamt -= 1;
		}
		if( m_pSprite )
			m_pSprite->pev->renderamt *= 0.9;
		SetThink(&CTripmineGrenade::DisarmThink );
		SetNextThink( 0.1 );
	}
}

void CTripmineGrenade::BreakTouch( CBaseEntity *pOther )
{
	if( !pOther )
		return;

	DontThink();
	pev->owner = m_pRealOwner;
	pev->health = 0;
	CGrenade::ExplodeTouch( pOther );

	/*
	float flDamage;
	entvars_t*	pevToucher = pOther->pev;
	
	// only players can break these right now
	if ( !pOther->IsPlayer() || !IsBreakable() )
		return;

	flDamage = pevToucher->velocity.Length() * 0.01;

	if (flDamage >= pev->health)
	{
		SetTouch( NULL );
		DontThink();
		TakeDamage(pevToucher, pevToucher, flDamage, DMG_CRUSH);
	}
	*/
}

void CTripmineGrenade :: OnChangeLevel( void )
{
	// NOTE: beam for moveable entities can't stay here
	// because entity will moved on a next level but beam is not
	ClearEffects();

	// NOTE: clear parent. We can't moving it properly for non-global entities
	SetParent( 0 );

	m_hOwner = NULL;	// i lost my star in Krasnodar :-)
}

void CTripmineGrenade :: TransferReset( void )
{
	// NOTE: i'm need to do it here because my parent already has
	// landmark offset but i'm is not
	Vector origin = GetAbsOrigin();
	origin += gpGlobals->vecLandmarkOffset;
	SetAbsOrigin( origin );

	// changelevel issues
	if ( m_hParent == NULL && m_hOwner == NULL )
	{
		TraceResult tr;

		UTIL_MakeVectors( GetAbsAngles() );
		Vector vecSrc = GetAbsOrigin() + gpGlobals->v_forward * -32;
		Vector vecDst = GetAbsOrigin() + gpGlobals->v_forward * 32;

		UTIL_TraceHull( vecSrc, vecDst, ignore_monsters, head_hull, edict(), &tr );

		if( tr.pHit && ENTINDEX( tr.pHit ) != 0 )
		{
			CBaseEntity *pNewParent = CBaseEntity::Instance( tr.pHit );

			if( pNewParent && pNewParent->IsBSPModel( ))
			{
				ALERT( at_aiconsole, "SetNewParent: %s\n", pNewParent->GetClassname());
				SetParent( pNewParent );
				m_hOwner = pNewParent;
			}
			else
				m_hOwner = g_pWorld;
		}
		else
			m_hOwner = g_pWorld;
	}

	SetBits( pev->effects, EF_NOINTERP );

	// create new beam on a next level
	if ( !m_pBeam )
		MakeBeam( );
}

void CTripmineGrenade :: PowerupThink( void  )
{
	if (gpGlobals->time > m_flPowerUp)
	{
		// make solid
		pev->solid = SOLID_BBOX;
		RelinkEntity( TRUE );

		MakeBeam();

		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/mine_activate.wav", 0.5, ATTN_NORM, 1.0, 75 ); // play enabled sound

		RemoveFlag(F_ENTITY_BUSY);

		return;
	}

	SetNextThink( 0.1 );
}

void CTripmineGrenade::ClearEffects(void)
{
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}

	if( m_pSprite )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}
}

void CTripmineGrenade :: MakeBeam( void )
{
	TraceResult tr;

	// g-cont. moved here from spawn
	UTIL_MakeVectors( GetAbsAngles() );

	m_vecDir = gpGlobals->v_forward;
	m_vecEnd = GetAbsOrigin() + m_vecDir * 2048;

	UTIL_TraceLine( GetAbsOrigin(), m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );
	m_flBeamLength = tr.flFraction;

	Vector vecTmpEnd = GetAbsOrigin() + m_vecDir * 2048 * m_flBeamLength;
	Vector vecStart = GetAbsOrigin() - m_vecDir * 7;

	m_pBeam = CBeam::BeamCreate( g_pModelNameLaser, 10 );

	if( m_pBeam )
	{
		m_pBeam->PointsInit( vecStart, vecTmpEnd ); //m_pBeam->PointEntInit( vecTmpEnd, entindex() );
		m_pBeam->SetColor( 255, 255, 255 ); //  0, 214, 198
		m_pBeam->SetScrollRate( 255 );
		m_pBeam->SetBrightness( 20 ); // 64
	//	m_pBeam->SetParent( this ); // this doesn't work with BEAM_POINTS beam!!!
		m_pBeam->SetWidth( 2 );
	}

	
	if( !m_pSprite )
		m_pSprite = CSprite::SpriteCreate( "sprites/glow01.spr", vecTmpEnd, FALSE );

	if( m_pSprite )
	{
		m_pSprite->SetTransparency( kRenderConstantGlow, 255, 255, 255, 100, 0 );
		m_pSprite->SetScale( 0.15 );
		m_pSprite->SetFadeDistance( 500 );
	}

	// setting use function here, only after the beam is fully created
	// otherwise the beam can remain on the map if spamming USE while mine spawns
	if( !(g_pGameRules->IsMultiplayer()) )
		SetUse(&CTripmineGrenade::DisarmUse );
	SetTouch( &CTripmineGrenade::BreakTouch ); // don't stand on them ;) 

	// set to follow laser spot
	SetThink( &CTripmineGrenade::BeamBreakThink );

	pev->skin = 1;

	// Delay first think slightly so beam has time
	// to appear if person right in front of it
	SetNextThink( 1.0 );

#if 0	// for testing parent system
	CBaseEntity *pEnt = CBaseEntity::Create( "info_target", tr.endpos + tr.plane.normal * 8, g_vecZero, NULL );
	pEnt->SetModel( "sprites/laserdot.spr" );
	pEnt->SetParent( this );
	pEnt->pev->rendermode = kRenderGlow;
	pEnt->pev->renderfx = kRenderFxNoDissipation;
	pEnt->pev->renderamt = 255;
#endif
}

void CTripmineGrenade :: BeamBreakThink( void  )
{
	BOOL bBlowup = 0;

	// respawn detect. 
	if( !m_pBeam )
	{
		MakeBeam();
		if( m_hParent )
			m_hOwner = m_hParent;// reset owner too
		else if( m_hOwner == NULL )
			m_hOwner = g_pWorld;// world-placed mine
	}

	TraceResult tr;

	// HACKHACK Set simple box using this really nice global!
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine( GetAbsOrigin(), m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	if( fabs( m_flBeamLength - tr.flFraction ) > 0.001 )
		bBlowup = 1;
	else
	{
		if (m_hOwner == NULL)
			bBlowup = 1;
	}

	if (bBlowup)
	{
		// a bit of a hack, but all CGrenade code passes pev->owner along to make sure the proper player gets credit for the kill
		// so we have to restore pev->owner from pRealOwner, because an entity's tracelines don't strike it's pev->owner which meant
		// that a player couldn't trigger his own tripmine. Now that the mine is exploding, it's safe the restore the owner so the 
		// CGrenade code knows who the explosive really belongs to.
		pev->owner = m_pRealOwner;
		pev->health = 0;
		IsTripped = true;
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/mine_tripped.wav", 1.0, ATTN_NORM );
		Killed( VARS( pev->owner ), GIB_NORMAL );
		return;
	}

	// blink
	if( gpGlobals->time > skin_change_time )
	{
		pev->skin == 1 ? pev->skin = 2 : pev->skin = 1;
		skin_change_time = gpGlobals->time + 0.5f;
	}

	SetNextThink( 0 ); // diffusion - think every frame to make sure it will be tripped
}

int CTripmineGrenade :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if (gpGlobals->time < m_flPowerUp && flDamage < pev->health)
	{
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( 0.1 );
		ClearEffects();
		return FALSE;
	}
	return CGrenade::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CTripmineGrenade::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;
	
	if ( pevAttacker && ( pevAttacker->flags & FL_CLIENT ) )
	{
		// some client has destroyed this mine, he'll get credit for any kills
		pev->owner = ENT( pevAttacker );
	}

//	EMIT_SOUND( ENT(pev), CHAN_BODY, "common/null.wav", 0, ATTN_NORM ); // shut off chargeup

	// diffusion - activate targets
	CBaseEntity *pActivator = NULL;
	if( pevAttacker != NULL )
		pActivator = CBaseEntity::Instance( pevAttacker );
	if( pActivator )
		UTIL_FireTargets( GetTarget(), pActivator, pActivator, USE_TOGGLE, 0 );

	float Delay;

	if( IsTripped )
		Delay = 0.5f;
	else
		Delay = RANDOM_FLOAT( 0.1, 0.3 );

	SetThink(&CTripmineGrenade::DelayDeathThink );
	SetNextThink( Delay );
}

void CTripmineGrenade::DelayDeathThink( void )
{
	ClearEffects();
	TraceResult tr;

	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecDir = gpGlobals->v_forward;

	EMIT_SOUND( ENT( pev ), CHAN_BODY, "common/null.wav", 0, ATTN_NORM ); // shut off chargeup

	UTIL_TraceLine ( GetAbsOrigin() + vecDir * 8, GetAbsOrigin() - vecDir * 64, dont_ignore_monsters, edict(), &tr );
	Explode( &tr, DMG_BLAST );
}

class CTripmine : public CBasePlayerWeapon
{
	DECLARE_CLASS( CTripmine, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_TRIPMINE + 1; }
	int GetItemInfo(ItemInfo *p);
	void SetObjectCollisionBox( void )
	{
		//!!!BUGBUG - fix the model!
		pev->absmin = GetAbsOrigin() + Vector(-16, -16, -5);
		pev->absmax = GetAbsOrigin() + Vector( 16, 16,  28); 
	}

	void PrimaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
	void WeaponIdle( void );
};

LINK_ENTITY_TO_CLASS( weapon_tripmine, CTripmine );

void CTripmine::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_TRIPMINE;
	SET_MODEL(ENT(pev), "models/weapons/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_GROUND;
	// ResetSequenceInfo( );
	pev->framerate = 0;

	FallInit();// get ready to fall down

	m_iDefaultAmmo = TRIPMINE_DEFAULT_GIVE;

	if ( !g_pGameRules->IsDeathmatch() )
	{
		UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 28) ); 
	}
}

void CTripmine::Precache( void )
{
	PRECACHE_MODEL ("models/weapons/v_tripmine.mdl");
	PRECACHE_MODEL ("models/weapons/p_tripmine.mdl");
	UTIL_PrecacheOther( "monster_tripmine" );
}

int CTripmine::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Trip Mine";
	p->iMaxAmmo1 = TRIPMINE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = WPN_SLOT_TRIPMINE;
	p->iPosition = WPN_POS_TRIPMINE;
	p->iId = m_iId = WEAPON_TRIPMINE;
	p->iWeight = TRIPMINE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CTripmine::Deploy( )
{
	pev->body = 0;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = gpGlobals->time + DEFAULT_DEPLOY_TIME;
	return DefaultDeploy( "models/weapons/v_tripmine.mdl", "models/weapons/p_tripmine.mdl", TRIPMINE_DRAW, "trip" );
}

void CTripmine::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		// out of mines
		m_pPlayer->RemoveWeapon( WEAPON_TRIPMINE );
		SetThink( &CBasePlayerItem::DestroyItem );
		SetNextThink( 0.1 );
	}

	SendWeaponAnim( TRIPMINE_HOLSTER );
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 0, ATTN_NORM);

	BaseClass::Holster();
}

void CTripmine::PrimaryAttack( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 128, ignore_monsters, m_pPlayer->edict(), &tr );

	if (tr.flFraction < 1.0)
	{
		// ALERT( at_console, "hit %f\n", tr.flFraction );

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if (pEntity && (pEntity->IsBSPModel() || pEntity->IsCustomModel()) && !(pEntity->pev->flags & FL_CONVEYOR))
		{
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );

			CBaseEntity *pEnt = CBaseEntity::Create( "monster_tripmine", tr.vecEndPos + tr.vecPlaneNormal * 8, angles, m_pPlayer->edict() );

			// g-cont. attach tripmine to the wall
			// NOTE: we should always attach the tripmine because our parent it's our owner too
			if( pEntity != g_pWorld )
				pEnt->SetParent( pEntity );

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
			{
				SendWeaponAnim( TRIPMINE_DRAW );
			}
			else
			{
				// no more mines! 
				RetireWeapon();
				return;
			}
		}
		else
		{
			// ALERT( at_console, "no deploy\n" );
		}
	}
	else
	{

	}

	m_flNextPrimaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
}

void CTripmine::WeaponIdle( void )
{
	pev->body = 0; // diffusion - https://github.com/FreeSlave/hlsdk-xash3d/commit/0fe9d6074c0a114bde725da45865862cb49c6c50
	
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		SendWeaponAnim( TRIPMINE_DRAW );
	}
	else
	{
		RetireWeapon(); 
		return;
	}

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.25)
	{
		iAnim = TRIPMINE_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 90.0 / 30.0;
	}
	else if (flRand <= 0.75)
	{
		iAnim = TRIPMINE_IDLE2;
		m_flTimeWeaponIdle = gpGlobals->time + 60.0 / 30.0;
	}
	else
	{
		iAnim = TRIPMINE_FIDGET;
		m_flTimeWeaponIdle = gpGlobals->time + 10.0f;
	}

	SendWeaponAnim( iAnim );
}