#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "triangleapi.h"

//===========================================================================================
// diffusion - draw icons for player's available electroblasts
//===========================================================================================

DECLARE_MESSAGE( m_BlastIcons, BlastIcons );

static int img_blasticon = 0;
static int img_blasticon_empty = 0;
static int img_circle = 0;
static int LastChargesReady = -1;

int CHudBlastIcons::Init( void )
{
	HOOK_MESSAGE( BlastIcons );
	gHUD.AddHudElem( this );
	return 1;
}

int CHudBlastIcons::VidInit(void)
{
	img_blasticon = LOAD_TEXTURE( "sprites/diffusion/electroblast.dds", NULL, 0, 0 );
	img_blasticon_empty = LOAD_TEXTURE( "sprites/diffusion/electroblast_empty.dds", NULL, 0, 0 );
	img_circle = LOAD_TEXTURE( "sprites/use_interact.dds", NULL, 0, 0 );
	AlphaFade1 = AlphaFade2 = AlphaFade3 = 0.0f;
	iRechargeTimer = 0;

	return 1;
}

int CHudBlastIcons::MsgFunc_BlastIcons( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	BlastAbilityLVL = READ_BYTE(); // this determines the number of icons (0-3)
	BlastChargesReady = READ_BYTE(); // ready charge is blue, not ready - red
	CanElectroBlast = (READ_BYTE() > 0); // suspended by script - can't use, draw grey icons
	iRechargeTimer = READ_BYTE();
	END_READ();

	if( BlastAbilityLVL > 0 )
	{
		m_iFlags |= HUD_ACTIVE;

		// make sure number of charges is increased
		if( LastChargesReady < BlastChargesReady )
		{
			if( BlastChargesReady == 1 )
				circlea1 = 1.0f;
			if( BlastChargesReady == 2 )
				circlea2 = 1.0f;
			if( BlastChargesReady == 3 )
				circlea3 = 1.0f;
		}

		LastChargesReady = BlastChargesReady;
	}
	else
		m_iFlags &= ~HUD_ACTIVE;

	return 1;
}

int CHudBlastIcons::Draw( float flTime )
{
	if( gHUD.m_iHideHUDDisplay & (HIDEHUD_HEALTH | HIDEHUD_ALL) )
		return 1;

	if( !gHUD.HasWeapon( WEAPON_SUIT ) )
		return 1;

	if( tr.time == tr.oldtime ) // paused
		return 1;

	if( gHUD.IsDrawingOfflineHUD || CL_IsDead() || gHUD.HUDSuitOffline )
		return 1;

	if( CVAR_TO_BOOL( ui_is_active ) )
		return 0;

	float x = gHUD.fCenteredPadding; // starting offset
	const float padding = 10 * gHUD.fScale;
	int r1 = 0, g1 = 0, b1 = 0;
	int r2 = 0, g2 = 0, b2 = 0;
	int r3 = 0, g3 = 0, b3 = 0;
	const float icon_size = 48 * gHUD.fScale;
	const float y = ScreenHeight - (72 * gHUD.fScale);

	bool bGrey = true;
	bool bTimerDrawn = false; // draw timer only on one icon

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );

	//====================
	// first icon
	//====================
	if( BlastAbilityLVL > 0 )
	{
		x += ((ScreenWidth / 32) + 360) * gHUD.fScale;

		bGrey = true;
		if( CanElectroBlast && BlastChargesReady > 0 )
			bGrey = false;

		if( !bGrey && CanElectroBlast )
			UnpackRGB( r1, g1, b1, gHUD.m_iHUDColor );
		else
			UnpackRGB( r1, g1, b1, RGB_GREY ); // grey

		if( AlphaFade1 < 255 )
			AlphaFade1 += 150 * g_fFrametime;

		if( img_circle && CanElectroBlast && circlea1 > 0.0f )
		{
			const float scale = 200.0f / (0.75f + circlea1);
			GL_Bind( 0, img_circle );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			GL_Color4f( 0.275f, 0.663f, 1.0f, circlea1 ); // 70 169 255
			DrawQuad( x + icon_size - scale, y + icon_size - scale, x + scale, y + scale );
			gEngfuncs.pTriAPI->End();
		}

		AlphaFade1 = bound( 0, AlphaFade1, 255 );

		if( AlphaFade1 > 0.0f )
		{
			GL_Color4f( r1 / 255.f, g1 / 255.f, b1 / 255.f, AlphaFade1 / 255.f );
			GL_Bind( 0, (bGrey && CanElectroBlast) ? img_blasticon_empty : img_blasticon );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			DrawQuad( x, y, x + icon_size, y + icon_size );
			gEngfuncs.pTriAPI->End();
		}

		if( bGrey && CanElectroBlast && iRechargeTimer > 0 )
		{
			// draw timer
			int width = 0;
			char num[32];
			Q_snprintf( num, sizeof( num ), "%i", iRechargeTimer );
			const char *buf = num;
			while( *buf )
			{
				width += gHUD.m_scrinfo.charWidths[*buf];
				buf++;
			}
			const float text_x = x + ((icon_size - width) * 0.5f);
			const float text_y = y + ((icon_size - gHUD.m_scrinfo.iCharHeight) * 0.5f);
			UnpackRGB( r1, g1, b1, RGB_GREY );
			ScaleColors( r1, g1, b1, AlphaFade1 );
			DrawString( text_x, text_y, num, r1, g1, b1 );
			bTimerDrawn = true;
		}
	}

	//====================
	// second icon
	//====================
	if( BlastAbilityLVL > 1 )
	{
		x += padding + icon_size;

		bGrey = true;
		if( CanElectroBlast && BlastChargesReady > 1 )
			bGrey = false;

		if( !bGrey && CanElectroBlast )
			UnpackRGB( r2, g2, b2, gHUD.m_iHUDColor );
		else
			UnpackRGB( r2, g2, b2, RGB_GREY ); // grey

		if( AlphaFade2 < 255 )
			AlphaFade2 += 150 * g_fFrametime;

		if( img_circle && CanElectroBlast && circlea2 > 0.0f )
		{
			const float scale = 200.0f / (0.75f + circlea2);
			GL_Bind( 0, img_circle );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			GL_Color4f( 0.275f, 0.663f, 1.0f, circlea2 ); // 70 169 255
			DrawQuad( x + icon_size - scale, y + icon_size - scale, x + scale, y + scale );
			gEngfuncs.pTriAPI->End();
		}

		AlphaFade2 = bound( 0, AlphaFade2, 255 );

		if( AlphaFade2 > 0.0f )
		{
			GL_Color4f( r2 / 255.f, g2 / 255.f, b2 / 255.f, AlphaFade2 / 255.f );
			GL_Bind( 0, (bGrey && CanElectroBlast) ? img_blasticon_empty : img_blasticon );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			DrawQuad( x, y, x + icon_size, y + icon_size );
			gEngfuncs.pTriAPI->End();
		}

		if( bGrey && CanElectroBlast && iRechargeTimer > 0 )
		{
			// draw timer
			char num[32];
			if( !bTimerDrawn )
				Q_snprintf( num, sizeof( num ), "%i", iRechargeTimer );
			else
				Q_snprintf( num, sizeof( num ), "--" );
			const char *buf = num;
			int width = 0;
			while( *buf )
			{
				width += gHUD.m_scrinfo.charWidths[*buf];
				buf++;
			}
			const float text_x = x + ((icon_size - width) * 0.5f);
			const float text_y = y + ((icon_size - gHUD.m_scrinfo.iCharHeight) * 0.5f);
			UnpackRGB( r2, g2, b2, RGB_GREY );
			ScaleColors( r2, g2, b2, AlphaFade2 );
			DrawString( text_x, text_y, num, r2, g2, b2 );
			bTimerDrawn = true;
		}
	}

	//====================
	// third icon
	//====================
	if( BlastAbilityLVL > 2 )
	{
		x += padding + icon_size;

		bGrey = true;
		if( CanElectroBlast && BlastChargesReady > 2 )
			bGrey = false;

		if( !bGrey && CanElectroBlast )
			UnpackRGB( r3, g3, b3, gHUD.m_iHUDColor );
		else
			UnpackRGB( r3, g3, b3, RGB_GREY ); // grey

		if( AlphaFade3 < 255 )
			AlphaFade3 += 150 * g_fFrametime;

		if( img_circle && CanElectroBlast && circlea3 > 0.0f )
		{
			const float scale = 200.0f / (0.75f + circlea3);
			GL_Bind( 0, img_circle );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			GL_Color4f( 0.275f, 0.663f, 1.0f, circlea3 ); // 70 169 255
			DrawQuad( x + icon_size - scale, y + icon_size - scale, x + scale, y + scale );
			gEngfuncs.pTriAPI->End();
		}

		AlphaFade3 = bound( 0, AlphaFade3, 255 );

		if( AlphaFade3 > 0.0f )
		{
			GL_Color4f( r3 / 255.f, g3 / 255.f, b3 / 255.f, AlphaFade3 / 255.f );
			GL_Bind( 0, (bGrey && CanElectroBlast) ? img_blasticon_empty : img_blasticon );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			DrawQuad( x, y, x + icon_size, y + icon_size );
			gEngfuncs.pTriAPI->End();
		}

		if( bGrey && CanElectroBlast && iRechargeTimer > 0 )
		{
			// draw timer
			char num[32];
			if( !bTimerDrawn )
				Q_snprintf( num, sizeof( num ), "%i", iRechargeTimer );
			else
				Q_snprintf( num, sizeof( num ), "--" );
			const char *buf = num;
			int width = 0;
			while( *buf )
			{
				width += gHUD.m_scrinfo.charWidths[*buf];
				buf++;
			}
			const float text_x = x + ((icon_size - width) * 0.5f);
			const float text_y = y + ((icon_size - gHUD.m_scrinfo.iCharHeight) * 0.5f);
			UnpackRGB( r3, g3, b3, RGB_GREY );
			ScaleColors( r3, g3, b3, AlphaFade3 );
			DrawString( text_x, text_y, num, r3, g3, b3 );
			bTimerDrawn = true;
		}
	}

	if( circlea1 > 0.0f )
		circlea1 -= 2.0f * g_fFrametime;
	if( circlea2 > 0.0f )
		circlea2 -= 2.0f * g_fFrametime;
	if( circlea3 > 0.0f )
		circlea3 -= 2.0f * g_fFrametime;

	return 1;
}