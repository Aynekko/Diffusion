#ifndef SHOCKBEAM_H
#define SHOCKBEAM_H

#define SHOCK_ALIENSHIP BIT(0) // flag for monster_alien_ship

//=========================================================
// Shockrifle projectile
//=========================================================
class CShock : public CBaseAnimating
{
	DECLARE_CLASS(CShock, CBaseAnimating);
public:
	void Spawn(void);
	void Precache();

	static void Shoot(entvars_t *pevOwner, const Vector angles, const Vector vecStart, const Vector vecVelocity);
	void Touch(CBaseEntity *pOther);
	void EXPORT FlyThink();
	virtual BOOL IsProjectile( void ) { return TRUE; }

//	virtual int		Save(CSave &save);
//	virtual int		Restore(CRestore &restore);
//	static	TYPEDESCRIPTION m_SaveData[];

	void CreateEffects();
	void ClearEffects();
	void UpdateOnRemove();

	CBeam *m_pBeam;
	CBeam *m_pNoise;
	bool SetSpriteModel;
	CSprite *m_pSprite2;

	int EffectsCreated;
	int BeamsRelinked; // relink beams on saverestore.

	bool GotOwnerClass; // we need to get the class of our owner, so we don't hurt our allies
	int m_iClass;

	DECLARE_DATADESC();
};
#endif