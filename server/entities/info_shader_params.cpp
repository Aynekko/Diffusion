#include "extdll.h"
#include "util.h"
#include "cbase.h"

//======================================================================
// diffusion - set params for client shaders
//======================================================================

/*
		fuser1 = sunshaft brightness
		iuser1 = disable blur of 3D sky
		iuser2 = disable lensflare
		iuser3 = enable sun disk
		sequence = enable heat distortion effect
*/

class CInfoShaderParams : public CPointEntity
{
	DECLARE_CLASS( CInfoShaderParams, CPointEntity );
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( info_shader_params, CInfoShaderParams );

void CInfoShaderParams::Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );

	SetNullModel();
	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	// hacks to distinguish this entity from possible others
	pev->effects |= EF_SKIPPVS;
	pev->renderfx = kRenderFxFullbright;
}