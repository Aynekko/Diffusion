#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/saverestore.h"
#include "entities/trains.h"			// trigger_camera has train functionality
#include "game/gamerules.h"
#include "talkmonster.h"
#include "weapons/weapons.h"
#include "triggers.h"

class CLadder : public CBaseTrigger
{
	DECLARE_CLASS(CLadder, CBaseTrigger);
public:
	void Spawn(void);
	void Precache(void);
};

LINK_ENTITY_TO_CLASS(func_ladder, CLadder);

//=========================================================
// func_ladder - makes an area vertically negotiable
// iuser1 is used - sets custom ladder sound!
//=========================================================
void CLadder::Precache(void)
{
	// Do all of this in here because we need to 'convert' old saved games
	pev->solid = SOLID_NOT;
	pev->skin = CONTENTS_LADDER;

	if (!CVAR_GET_FLOAT("showtriggers"))
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 0;
	}

	pev->effects &= ~EF_NODRAW;
}


void CLadder::Spawn(void)
{
	Precache();

	SET_MODEL(edict(), GetModel()); // set size and link into world
	pev->movetype = MOVETYPE_PUSH;
	SetBits(m_iFlags, MF_LADDER);
}