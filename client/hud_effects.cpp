
// diffusion
// just a place for all custom on-screen effects

#include "hud.h"
#include "utils.h"
#include "triangleapi.h"
#include "r_local.h"
#include "pm_defs.h"
#include "r_efx.h"
#include "r_world.h"
#include "event_api.h"

#define SPEED_ARROW_STEP 2
#define SPEED_ARROW_FRAMES 121
#define SPEEDOMETER_Y_OFFSET 5
#define SAVE_SQUARE_SIZE 100

int CScreenEffects::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE; // always active
	return 1;
}

int CScreenEffects::VidInit(void)
{
	Vignette = LOAD_TEXTURE( "sprites/ef_vignette.dds", NULL, 0, 0 );
	VignetteShield = LOAD_TEXTURE( "sprites/ef_vignette_shield.dds", NULL, 0, 0 );
	Speedometer = LOAD_TEXTURE( "sprites/diffusion/speedometer.dds", NULL, 0, 0 );
	SpeedometerArrow = LOAD_TEXTURE( "sprites/diffusion/speedometer_arrow.dds", NULL, 0, 0 );
	SpeedometerGears.Init( "sprites/diffusion/speed_gears/speed_gears" );
	LastOrigin = g_vecZero;
	SaveIcon = LOAD_TEXTURE( "sprites/diffusion/saveicon.dds", NULL, 0, 0 );

	return 1;
}

int CScreenEffects::Draw( float flTime )
{
	DrawSpeedometer();
	DrawShieldVignette();
	DrawVignette();
	DrawCinematicBorder();
	DrawGameSaved();

	return 1;
}

//==========================================================================
// draw "game saved" icon (150õ150 image)
// it's a still picture, and all animation is done here in the code
//==========================================================================
void CScreenEffects::DrawGameSaved(void)
{
	if( !SaveIcon )
		return;

	static float alpha = 0.0f;
	static bool bFadeOut = false;
	const int rotation = 360 * 3;
	static float curRotation = 0.0f;
	static int fadeout_stage = 0;
	
	if( SaveIcon_Reset )
	{
		SaveIcon_RotSpeed = 0.0f;
		SaveIcon_Alpha = 0.0f;
		SaveIcon_StartTime = tr.time;
		curRotation = 0.0f;
		alpha = 0.0f;
		bFadeOut = false;
		SaveIcon_Reset = false;
	}
	
	if( !ShouldDrawGameSaved )
		return;

	if( alpha < 1.0f && !bFadeOut )
		alpha += 0.65f * g_fFrametime;

	alpha = bound( 0.0f, alpha, 1.0f );
	if( curRotation < rotation )
	{
		SaveIcon_RotSpeed = bound( 30.0f, (rotation - curRotation) * 5.0f, 3000.0f );
		curRotation += SaveIcon_RotSpeed * g_fFrametime;
	}

	if( curRotation >= rotation )
	{
		curRotation = rotation;
		if( !bFadeOut )
		{
			bFadeOut = true;
			fadeout_stage = 0;
		}
	}

	if( bFadeOut )
	{
		switch( fadeout_stage ) // pulse 1 time :D
		{
		case 0:
			alpha = CL_UTIL_Approach( 0.25f, alpha, 2.5f * g_fFrametime );
			if( alpha <= 0.25f )
				fadeout_stage = 1;
			break;
		case 1:
			alpha = CL_UTIL_Approach( 1.0f, alpha, 2.5f * g_fFrametime );
			if( alpha >= 1.0f )
				fadeout_stage = 2;
			break;
		case 2:
			alpha -= 1.5f * g_fFrametime;
			break;
		}
		
		if( alpha <= 0.0f )
		{
			ShouldDrawGameSaved = false;
			return;
		}
	}

	GL_Bind( 0, SaveIcon );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, alpha );

	pglPushMatrix();
	pglTranslatef( (ScreenWidth * 0.5f) - (SAVE_SQUARE_SIZE / 2), ScreenHeight - 75 - SAVE_SQUARE_SIZE, 0 );
	pglRotatef( curRotation, 0, 0, 1 );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( -SAVE_SQUARE_SIZE / 2, -SAVE_SQUARE_SIZE / 2, SAVE_SQUARE_SIZE / 2, SAVE_SQUARE_SIZE / 2 );
	gEngfuncs.pTriAPI->End();
	pglPopMatrix();
}

//===============================================
// draw speedometer when in car or boat
//===============================================
void CScreenEffects::DrawSpeedometer(void)
{	
	if( !gHUD.InCar )
	{
		gHUD.CarSpeed = 0.0f;
		SpeedArrowRotation = 0;
		return;
	}

	if( tr.time == tr.oldtime ) // not in paused
		return;
	
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_SelectTexture( 0 );
	GL_Bind( 0, Speedometer );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( ScreenWidth - 320, ScreenHeight - 320, ScreenWidth - 20, ScreenHeight - 20 );
	gEngfuncs.pTriAPI->End();

	// kilometers
	float Distance = (gEngfuncs.GetLocalPlayer()->prevstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length() * 0.01905f * 0.001f;
	float timedelta = g_fFrametime;
	if( GET_MAX_CLIENTS() > 1 )
		timedelta = gEngfuncs.GetLocalPlayer()->curstate.msg_time - gEngfuncs.GetLocalPlayer()->prevstate.msg_time;
	
	gHUD.CarSpeed = Distance / (timedelta / 3600.0f);
//	gEngfuncs.Con_NPrintf( 1,"%i\n", (int)gHUD.CarSpeed );

	// draw the arrow
	SpeedArrowRotation = CL_UTIL_Approach( gHUD.CarSpeed, SpeedArrowRotation, 25 * g_fFrametime );
	if( SpeedArrowRotation > 240 )
		SpeedArrowRotation = 240;
	GL_Bind( 0, SpeedometerArrow );
	pglPushMatrix();
	pglTranslatef( ScreenWidth - 320 + 150, ScreenHeight - 320 + 150, 0 );
	pglRotatef( SpeedArrowRotation, 0, 0, 1 );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( -128, -128, 128, 128 );
	gEngfuncs.pTriAPI->End();
	pglPopMatrix();

	// draw gears
	// 9 frames in total (EMPTY-1-2-3-4-5-6-7-R)
	SpeedometerGears.SetPos( ScreenWidth - 320, ScreenHeight - 320 - SPEEDOMETER_Y_OFFSET, ScreenWidth - 20, ScreenHeight - 20 - SPEEDOMETER_Y_OFFSET );
	SpeedometerGears.SetRenderMode( kRenderTransAdd );
	SpeedometerGears.DrawFrame( Gear );
}

//===============================================
// draw cinematic border (spectator mode and trigger_camera using this)
//===============================================
void CScreenEffects::DrawCinematicBorder( void )
{
//	if( tr.time == tr.oldtime ) // paused
//		return;

	int BorderTransparency = 0;

	if( g_iUser1 )
		BorderTransparency = 125;

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERUSINGCAMERA )
		BorderTransparency = 200;

	if( BorderTransparency > 0 )
	{
		gEngfuncs.pfnFillRGBABlend( 0, ScreenHeight - (ScreenHeight / 10), ScreenWidth, ScreenHeight / 10, 0, 0, 0, BorderTransparency ); // bottom border
		gEngfuncs.pfnFillRGBABlend( 0, 0, ScreenWidth, ScreenHeight / 10, 0, 0, 0, BorderTransparency ); // top border
	}
}

//===============================================
// draw a vignette when crouched or paused
//===============================================
void CScreenEffects::DrawVignette( void )
{
	if( (gEngfuncs.pEventAPI->EV_LocalPlayerDucking() && tr.viewparams.onground) || (tr.time == tr.oldtime) )
	{
		if( VignetteAlpha < 1 )
			VignetteAlpha += 2 * g_fFrametime;
	}
	else
	{
		if( VignetteAlpha > 0 )
			VignetteAlpha -= 2 * g_fFrametime;
	}

	VignetteAlpha = bound( 0, VignetteAlpha, 1 );

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERDRONECONTROL )
		return;

	if( VignetteAlpha > 0 )
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, VignetteAlpha );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );

		GL_SelectTexture( 0 );

		GL_Bind( 0, Vignette );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( 0, 0, ScreenWidth, ScreenHeight );
		gEngfuncs.pTriAPI->End();
	}
}

void CScreenEffects::DrawShieldVignette(void)
{
	if( !gHUD.ShieldOn )
	{
		if( ShieldAlpha == 0.0f )
			return;
		
		if( ShieldAlpha > 0 )
		{
			ShieldAlpha -= 3 * g_fFrametime;
			if( ShieldAlpha <= 0 )
				ShieldAlpha = 0;
		}
	}
	else
	{
		if( ShieldAlpha < 1 )
			ShieldAlpha += 3 * g_fFrametime;
	}

	ShieldAlpha = bound( 0, ShieldAlpha, 1 );

	if( ShieldAlpha > 0 )
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		gEngfuncs.pTriAPI->Color4f( 70 / 255.0f, 169 / 255.0f, 1.0f, ShieldAlpha );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );

		GL_SelectTexture( 0 );

		GL_Bind( 0, VignetteShield );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( 0, 0, ScreenWidth, ScreenHeight );
		gEngfuncs.pTriAPI->End();
	}
}