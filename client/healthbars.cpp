#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "r_local.h"

#define DISAPPEAR_START_DIST 500 // it's not a real dist, more like a multiplier...
#define BAR_SCALE 2.5f

/*
barsize: 
0 - small, 1 - big, 2 - huge
3 - small, 4 - big, 5 - huge (same but centered!)
*/

DECLARE_MESSAGE( m_Healthbars, Healthbars );
DECLARE_MESSAGE( m_Healthbars, HealthbarCenter );

int CHealthbars::Init( void )
{
	HOOK_MESSAGE( Healthbars );
	HOOK_MESSAGE( HealthbarCenter );
	gHUD.AddHudElem( this );
	return 1;
}

int CHealthbars::VidInit( void )
{
	const int bar_emptyh = gHUD.GetSpriteIndex( "health_empty" );
	const int bar_fullh = gHUD.GetSpriteIndex( "health_full" );
	m_hBarEmpty = gHUD.GetSprite( bar_emptyh );
	m_hBarFull = gHUD.GetSprite( bar_fullh );
	m_prc_emp = &gHUD.GetSpriteRect( bar_emptyh );
	m_prc_full = &gHUD.GetSpriteRect( bar_fullh );
	m_iBarWidth = m_prc_full->right - m_prc_full->left;
	entindex = 0;
	m_iFlags |= HUD_ACTIVE;
	MonsterName[0] = '\0';
	
	return 1;
}

int CHealthbars::MsgFunc_Healthbars( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	health = READ_BYTE(); // % - bounded at 200 on server
	barsize = READ_BYTE();
	entindex = READ_SHORT();

	END_READ();

	if( barsize > 2 )
	{
		bCentered = true;
		barsize -= 3;
	}
	else
		bCentered = false;

	return 1;
}

int CHealthbars::MsgFunc_HealthbarCenter( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	health_center = READ_BYTE(); // %
	if( health_center == 255 )
		MonsterName[0] = '\0';
	else
		sprintf_s( MonsterName, READ_STRING() );

	END_READ();

	return 1;
}

void CHealthbars::DrawCentralBar( void )
{
	if( MonsterName[0] == '\0' )
		return;

	// 600 width, 30 height

	const int tex_background = LOAD_TEXTURE( "sprites/healthbar5.dds", NULL, 0, 0 );
	const int tex_bar = LOAD_TEXTURE( "sprites/healthbar6.dds", NULL, 0, 0 );
	float alpha = 1.0f;
	if( gHUD.m_HintObjectives.bShowMissionObjectives || gHUD.m_HintObjectives.bShowMissionObjectivesTimed )
		alpha = 0.1f; // dim the health to make objective more readable

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );

	// draw the background texture
	gEngfuncs.pTriAPI->Color4f( 0.275f, 0.663f, 1.0f, alpha ); // 70 169 255
	GL_SelectTexture( 0 );
	GL_Bind( 0, tex_background );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	float x_start = ScreenWidth * 0.5f;
	float y_start = ScreenHeight * 0.125f;
	DrawQuad( x_start - 300, y_start - 15, x_start + 300, y_start + 15 );
	gEngfuncs.pTriAPI->End();

	// draw the red health bar on top
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, alpha ); // red is baked into texture for now
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	GL_SelectTexture( 0 );
	GL_Bind( 0, tex_bar );

	static float hp = 0;
	hp = lerp( hp, health_center, g_fFrametime * 5 );
	const float newwidth = 300 * (hp / 100.0f) * 2; // no idea how but it works
	const float xmin = x_start - 300;
	const float ymin = y_start - 15;
	const float xmax = x_start - 300 + newwidth;
	const float ymax = y_start + 15;
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	// top left
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 ); gEngfuncs.pTriAPI->Vertex3f( xmin, ymin, 0 );
	// bottom left
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 ); gEngfuncs.pTriAPI->Vertex3f( xmin, ymax, 0 );
	// bottom right
	gEngfuncs.pTriAPI->TexCoord2f( (hp / 100.0f), 1 ); gEngfuncs.pTriAPI->Vertex3f( xmax, ymax, 0 );
	// top right
	gEngfuncs.pTriAPI->TexCoord2f( (hp / 100.0f), 0 ); gEngfuncs.pTriAPI->Vertex3f( xmax, ymin, 0 );
	gEngfuncs.pTriAPI->End();

	// draw the name above the bar
	const char *buf;

	// calculate width to align center...
	buf = MonsterName;
	int width = 0;
	while( *buf )
	{
		unsigned char c = *buf;
		width += TextMessageDrawChar( ScreenWidth * 2, ScreenHeight * 2, c, 0, 0, 0 );
		buf++;
	}

	int r = 255, g = 255, b = 255;
	ScaleColors( r, g, b, alpha * 255 );
	DrawString( (int)((ScreenWidth - width) * 0.5f), (ScreenHeight * 0.125f) - 15 - (gHUD.m_scrinfo.iCharHeight * 1.25f), MonsterName, r, g, b );
}

int CHealthbars::Draw( float flTime )
{	
	if( tr.time == tr.oldtime ) // not in paused
		return 1;

	DrawCentralBar(); // it is drawn regardless of healthbars cvar
	
	if( !cl_showhealthbars->value )
		return 1;
	
	if( entindex == 0 )
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 0;

	if( CL_IsDead() )
	{
		entindex = 0;
		return 1;
	}

	Vector screen;

	cl_entity_t *ent = GET_ENTITY( entindex );
	if( !ent )
		return 1;

	Vector org = ent->origin + ((ent->curstate.mins + ent->curstate.maxs) * 0.5f);
	if( !bCentered )
		org.z += (ent->curstate.mins - ent->curstate.maxs).Length() * 0.5f;

	float Transparency = 255.0f;

	// for FOV above 80 it is assumed that distance remains unchanged
	float current_fov = bound( 10.0f, gHUD.m_flFOV, 80.0f );
	float DistanceToEnt = (tr.viewparams.vieworg - org).Length();
	float DisappearStartDist = DISAPPEAR_START_DIST * (80 / current_fov) * (barsize + 1) * 3;
	if( DistanceToEnt > DisappearStartDist )
	{
		Transparency -= DistanceToEnt - DisappearStartDist;
		if( Transparency <= 0 )
			return 1;
	}

	R_WorldToScreen( org, screen );
	const int x = XPROJECT( screen[0] );
	const int y = YPROJECT( screen[1] );

	// total distance
	float dist_mult = DistanceToEnt / (DISAPPEAR_START_DIST + 255);
	dist_mult = bound( 0.1, dist_mult, 1 );
	// also scale by fov, taking 80 fov as default

	const float fov_mult = current_fov / 80.0f;
	dist_mult *= fov_mult;

	float width = (BAR_SCALE * 10 * (barsize+1)) / dist_mult;
	float height = BAR_SCALE / dist_mult;

	switch( barsize )
	{
	case 0:
		hptex = LOAD_TEXTURE( "sprites/healthbar1.dds", NULL, 0, 0 );
		hptex2 = LOAD_TEXTURE( "sprites/healthbar2.dds", NULL, 0, 0 );
		break;
	case 1:
		hptex = LOAD_TEXTURE( "sprites/healthbar3.dds", NULL, 0, 0 );
		hptex2 = LOAD_TEXTURE( "sprites/healthbar4.dds", NULL, 0, 0 );
		break;
	case 2:
		hptex = LOAD_TEXTURE( "sprites/healthbar5.dds", NULL, 0, 0 );
		hptex2 = LOAD_TEXTURE( "sprites/healthbar6.dds", NULL, 0, 0 );
		break;
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );

	// draw the background texture
	gEngfuncs.pTriAPI->Color4f( 70 / 255.0f, 169 / 255.0f, 1.0f, Transparency / 255.0f );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	GL_SelectTexture( 0 );
	GL_Bind( 0, hptex );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( x - width, y - height, x + width, y + height );
	gEngfuncs.pTriAPI->End();

	// draw the red health bar on top
	float Denom = 255.0f;
	if( health > 100 )
		Denom = 500.0f; // make it less brighter so second bar would be more visible
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, Transparency / Denom );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	GL_SelectTexture( 0 );
	GL_Bind( 0, hptex2 );

	const float hp1 = bound( 0, health, 100 );
	float newwidth = width * ( hp1 / 100.0f) * 2; // no idea how but it works
	float xmin = x - width;
	float ymin = y - height;
	float xmax = x - width + newwidth;
	float ymax = y + height;
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	// top left
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 ); gEngfuncs.pTriAPI->Vertex3f( xmin, ymin, 0 );
	// bottom left
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 ); gEngfuncs.pTriAPI->Vertex3f( xmin, ymax, 0 );
	// bottom right
	gEngfuncs.pTriAPI->TexCoord2f( (hp1 / 100.0f), 1 ); gEngfuncs.pTriAPI->Vertex3f( xmax, ymax, 0 );
	// top right
	gEngfuncs.pTriAPI->TexCoord2f( (hp1 / 100.0f), 0 ); gEngfuncs.pTriAPI->Vertex3f( xmax, ymin, 0 );
	gEngfuncs.pTriAPI->End();

	// draw same red bar on top
	if( health > 100 )
	{
		const float hp2 = bound( 0, health - 100, 100 );
		newwidth = width * (hp2 / 100.0f) * 2;
		xmin = x - width;
		ymin = y - height;
		xmax = x - width + newwidth;
		ymax = y + height;

		// draw the red health bar on top
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, Transparency / 255.0f );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );
		GL_SelectTexture( 0 );
		GL_Bind( 0, hptex2 );

		
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		// top left
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 ); gEngfuncs.pTriAPI->Vertex3f( xmin, ymin, 0 );
		// bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 ); gEngfuncs.pTriAPI->Vertex3f( xmin, ymax, 0 );
		// bottom right
		gEngfuncs.pTriAPI->TexCoord2f( (hp2 / 100.0f), 1 ); gEngfuncs.pTriAPI->Vertex3f( xmax, ymax, 0 );
		// top right
		gEngfuncs.pTriAPI->TexCoord2f( (hp2 / 100.0f), 0 ); gEngfuncs.pTriAPI->Vertex3f( xmax, ymin, 0 );
		gEngfuncs.pTriAPI->End();
	}

	return 1;
}