#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "player.h"
#include "entities/soundent.h"
#include "game/gamerules.h"

enum squeak_e
{
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_UP,
	SQUEAK_THROW
};

class CSqueak : public CBasePlayerWeapon
{
	DECLARE_CLASS(CSqueak, CBasePlayerWeapon);
public:
	void Spawn(void);
	void Precache(void);
	int iItemSlot(void) { return 5; }
	int GetItemInfo(ItemInfo* p);

	void PrimaryAttack(void);
	BOOL Deploy(void);
	void Holster(void);
	void WeaponIdle(void);
	int m_fJustThrown;
};

//LINK_ENTITY_TO_CLASS(weapon_snark, CSqueak); // diffusion - no snarks.

void CSqueak::Spawn(void)
{
	Precache();
	m_iId = WEAPON_SNARK;
	SET_MODEL(ENT(pev), "models/w_sqknest.mdl");

	FallInit();//get ready to fall down.

	m_iDefaultAmmo = SNARK_DEFAULT_GIVE;

	pev->sequence = 1;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;
}

void CSqueak::Precache(void)
{
	PRECACHE_MODEL("models/w_sqknest.mdl");
	PRECACHE_MODEL("models/v_squeak.mdl");
	PRECACHE_MODEL("models/p_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	UTIL_PrecacheOther("monster_snark");
}

int CSqueak::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Snarks";
	p->iMaxAmmo1 = SNARK_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_SNARK;
	p->iWeight = SNARK_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CSqueak::Deploy(void)
{
	// play hunt sound
	float flRndSound = RANDOM_FLOAT(0, 1);

	if (flRndSound <= 0.5)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 100);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 100);

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;

	return DefaultDeploy("models/v_squeak.mdl", "models/p_squeak.mdl", SQUEAK_UP, "squeak");
}

void CSqueak::Holster(void)
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		m_pPlayer->RemoveWeapon(WEAPON_SNARK);
		SetThink(&CBasePlayerItem::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	SendWeaponAnim(SQUEAK_DOWN);
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 0, ATTN_NORM);

	BaseClass::Holster();
}

void CSqueak::PrimaryAttack(void)
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		UTIL_MakeVectors(m_pPlayer->pev->v_angle);
		TraceResult tr;

		// find place to toss monster
		UTIL_TraceLine(m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 20, m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr);

		if (tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25)
		{
			SendWeaponAnim(SQUEAK_THROW);

			// player "shoot" animation
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);

			CBaseEntity* pSqueak = CBaseEntity::Create("monster_snark", tr.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer->edict());
			Vector vecGround;

			if (m_pPlayer->GetGroundEntity())
				vecGround = m_pPlayer->GetGroundEntity()->GetAbsVelocity();
			else vecGround = g_vecZero;

			pSqueak->SetAbsVelocity(gpGlobals->v_forward * 200 + m_pPlayer->GetAbsVelocity() + vecGround);

			// play hunt sound
			float flRndSound = RANDOM_FLOAT(0, 1);

			if (flRndSound <= 0.5)
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 105);
			else
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105);

			m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

			m_fJustThrown = 1;

			m_flNextPrimaryAttack = gpGlobals->time + 0.3;
			m_flTimeWeaponIdle = gpGlobals->time + 1.0;
		}
	}
}

void CSqueak::WeaponIdle(void)
{
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_fJustThrown)
	{
		m_fJustThrown = 0;

		if (!m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()])
		{
			RetireWeapon();
			return;
		}

		SendWeaponAnim(SQUEAK_UP);
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
		return;
	}

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.75)
	{
		iAnim = SQUEAK_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = SQUEAK_FIDGETFIT;
		m_flTimeWeaponIdle = gpGlobals->time + 70.0 / 16.0;
	}
	else
	{
		iAnim = SQUEAK_FIDGETNIP;
		m_flTimeWeaponIdle = gpGlobals->time + 80.0 / 16.0;
	}

	SendWeaponAnim(iAnim);
}