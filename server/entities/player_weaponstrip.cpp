#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "game/game.h"

#define SF_REMOVE_SUIT		BIT( 0 )
#define SF_REMOVE_CYCLER	BIT( 1 )

class CStripWeapons : public CPointEntity
{
	DECLARE_CLASS(CStripWeapons, CPointEntity);
public:
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};

LINK_ENTITY_TO_CLASS(player_weaponstrip, CStripWeapons);

void CStripWeapons::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = NULL;

	if (pActivator && pActivator->IsPlayer())
		pPlayer = (CBasePlayer*)pActivator;
	else if (!g_pGameRules->IsDeathmatch())
		pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(1);

	if (pPlayer)
		pPlayer->RemoveAllItems(HasSpawnFlags(SF_REMOVE_SUIT), HasSpawnFlags(SF_REMOVE_CYCLER));
}