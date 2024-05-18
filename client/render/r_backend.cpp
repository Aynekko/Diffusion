//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "r_sprite.h"
#include "mathlib.h"
#include "r_shader.h"

/*
=============
R_GetSpriteTexture

=============
*/
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame )
{
	if( !m_pSpriteModel || m_pSpriteModel->type != mod_sprite || !m_pSpriteModel->cache.data )
		return 0;

	return R_GetSpriteFrame( m_pSpriteModel, frame )->gl_texturenum;
}

void GL_DepthRange( GLfloat depthmin, GLfloat depthmax )
{
	pglDepthRange( depthmin, depthmax );
}

void GL_Texture2D( GLint enable )
{
	if( enable )
		pglEnable( GL_TEXTURE_2D );
	else
		pglDisable( GL_TEXTURE_2D );
}

void GL_DepthTest( GLint enable )
{
	if( enable ) 
		pglEnable( GL_DEPTH_TEST );
	else 
		pglDisable( GL_DEPTH_TEST );
}

void GL_Color4f( GLfloat r, GLfloat g, GLfloat b, GLfloat a )
{
	pglColor4f( r, g, b, a );
}

/*
==============
GL_DepthMask
==============
*/
void GL_DepthMask( GLint enable )
{
	pglDepthMask( enable );
}

/*
==============
GL_AlphaTest
==============
*/
void GL_AlphaTest( GLint enable )
{
	if( enable )
		pglEnable( GL_ALPHA_TEST );
	else
		pglDisable( GL_ALPHA_TEST );
#if 0
	if( enable ) pglEnable( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB );
	else pglDisable( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB );
#endif
}

void GL_AlphaFunc( GLenum func, GLclampf ref )
{
	pglAlphaFunc( func, ref );
}

/*
==============
GL_Blend
==============
*/
void GL_Blend( GLint enable )
{
	if( enable )
		pglEnable( GL_BLEND );
	else
		pglDisable( GL_BLEND );
}

void GL_BlendFunc( GLenum sfactor, GLenum dfactor )
{
	pglBlendFunc( sfactor, dfactor );
}

/*
=================
GL_Cull
=================
*/
void GL_Cull( GLenum cull )
{
	// to avoid useless OpenGL API calls
	if( glState.faceCull == cull )
		return;
	
	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_FrontFace
=================
*/
void GL_FrontFace( GLenum front )
{
	pglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

/*
=================
GL_Setup2D
=================
*/
void GL_Setup2D( void )
{
	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	GL_DepthTest( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_Blend( GL_FALSE );

	GL_Cull( GL_FRONT );
	GL_Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, glState.width, glState.height );
}

/*
=================
GL_Setup3D
=================
*/
void GL_Setup3D( void )
{
	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI->projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->worldviewMatrix );

	GL_DepthTest( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_Blend( GL_FALSE );
	GL_DepthMask( GL_TRUE );
	GL_Cull( GL_FRONT );
}

/*
=================
GL_LoadTexMatrix
=================
*/
void GL_LoadTexMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	GL_LoadTextureMatrix( dest );
}

/*
=================
GL_LoadMatrix
=================
*/
void GL_LoadMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	pglLoadMatrixf( dest );
}

/*
==============
GL_CleanupAllTextureUnits
==============
*/
void GL_CleanupAllTextureUnits( void )
{
	// force to cleanup all the units
	GL_SelectTexture( glConfig.max_texture_units - 1 );
	GL_CleanUpTextureUnits( 0 );
}

/*
==============
GL_CleanupDrawState
==============
*/
void GL_CleanupDrawState( void )
{
	GL_CleanupAllTextureUnits();
	pglBindVertexArray( GL_FALSE );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	GL_BindShader( NULL );
}

/*
================
R_BeginDrawProjectionGLSL

Setup texture matrix for light texture
================
*/
bool R_BeginDrawProjectionGLSL( plight_t *pl, float lightscale )
{
	word shaderNum = GL_UberShaderForDlightGeneric( pl );
	GLfloat	gl_lightViewProjMatrix[16];

	if( !shaderNum || tr.nodlights )
		return false;

	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	pglBlendFunc( GL_ONE, GL_ONE );
	pglEnable( GL_SCISSOR_TEST );

	if( glState.drawTrans )
		pglDepthFunc( GL_LEQUAL );
	else pglDepthFunc( GL_LEQUAL );
	RI->currentlight = pl;

	float y2 = (float)RI->viewport[3] - pl->h - pl->y;
	pglScissor( pl->x, y2, pl->w, pl->h );

	GL_BindShader( &glsl_programs[shaderNum] );			
	ASSERT( RI->currentshader != NULL );
	R_LoadIdentity();

	Vector lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
	pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );

	// write constants
	pglUniformMatrix4fvARB( RI->currentshader->u_LightViewProjectionMatrix, 1, GL_FALSE, &gl_lightViewProjMatrix[0] );
	float shadowWidth = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, pl->shadowTexture[0] );
	float shadowHeight = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, pl->shadowTexture[0] );

	Vector4D light_params[6];
	// light dir + fov
	light_params[0] = Vector4D( lightdir.x, lightdir.y, lightdir.z, pl->fov );
	// light diffuse
	light_params[1] = Vector4D( pl->color.r / 255.0f, pl->color.g / 255.0f, pl->color.b / 255.0f, pl->lightFalloff );
	// shadow params
	light_params[2] = Vector4D( shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
	// light origin
	light_params[3] = Vector4D( pl->origin.x, pl->origin.y, pl->origin.z, (1.0f / pl->radius) );
	// fog params
	light_params[4] = Vector4D( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
	// lightscale and brightness
	light_params[5] = Vector4D( lightscale, pl->brightness, 0.0f, 0.0f );
	// send all out
	pglUniform4fvARB( RI->currentshader->u_LightParams, 6, &light_params[0][0] );

	GL_Bind( GL_TEXTURE1, pl->projectionTexture );
	GL_Bind( GL_TEXTURE2, pl->shadowTexture[0] );

	return true;
}

/*
================
R_EndDrawProjection

Restore identity texmatrix
================
*/
void R_EndDrawProjectionGLSL( void )
{
	GL_BindShader( NULL );

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	pglDisable( GL_SCISSOR_TEST );
	pglDepthFunc( GL_LEQUAL );
	RI->currentlight = NULL;
	GL_Blend( GL_FALSE );
}

int R_AllocFrameBuffer( int viewport[4] )
{
	int i = tr.num_framebuffers;

	if( i >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_AllocateFrameBuffer: FBO limit exceeded!\n" );
		return -1; // disable
	}

	gl_fbo_t *fbo = &tr.frame_buffers[i];
	tr.num_framebuffers++;

	if( fbo->init )
	{
		ALERT( at_warning, "R_AllocFrameBuffer: buffer %i already initialized\n", i );
		return i;
	}

	// create a depth-buffer
	pglGenRenderbuffers( 1, &fbo->renderbuffer );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglRenderbufferStorage( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, viewport[2], viewport[3] );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, 0 );

	// create frame-buffer
	pglGenFramebuffers( 1, &fbo->framebuffer );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
	pglFramebufferRenderbuffer( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglReadBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	fbo->init = true;

	return i;
}

void R_FreeFrameBuffer( int buffer )
{
	if( buffer < 0 || buffer >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_FreeFrameBuffer: invalid buffer enum %i\n", buffer );
		return;
	}

	gl_fbo_t *fbo = &tr.frame_buffers[buffer];

	pglDeleteRenderbuffers( 1, &fbo->renderbuffer );
	pglDeleteFramebuffers( 1, &fbo->framebuffer );
	memset( fbo, 0, sizeof( *fbo ));
}

void GL_BindFrameBuffer( int buffer, int texture )
{
	gl_fbo_t *fbo = NULL;

	if( buffer >= 0 && buffer < MAX_FRAMEBUFFERS )
		fbo = &tr.frame_buffers[buffer];

	if( !fbo )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
		glState.frameBuffer = buffer;
		return;
	}

	// at this point FBO index is always positive
	if((unsigned int)glState.frameBuffer != fbo->framebuffer )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
		glState.frameBuffer = fbo->framebuffer;
	}		

	if( fbo->texture != texture )
	{
		// change texture attachment
		GLuint texnum = RENDER_GET_PARM( PARM_TEX_TEXNUM, texture );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texnum, 0 );
		fbo->texture = texture;
	}
}

/*
==============
GL_BindFBO
==============
*/
void GL_BindFBO( GLint buffer )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return;

	if( glState.frameBuffer == buffer )
		return;

	if( buffer < 0 )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
		glState.frameBuffer = buffer;
		return;
	}

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, buffer );
	glState.frameBuffer = buffer;
}

void GL_AlphaToCoverage( bool enable )
{
	// TODO store state locally to avoid GL-calls to get current state
	if( pglIsEnabled( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB ) == enable )
		return;

	if( enable && 1 )// CVAR_TO_BOOL( gl_alpha2coverage ) )
	{
		pglEnable( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB );
	}
	else 
	{
		pglDisable( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB );
	}
}

bool GL_UsingAlphaToCoverage( void )
{
	return (CVAR_GET_FLOAT( "gl_msaa" ) > 0.0f);
}

//=====================================================================================
// FillRoundedRGBA
// credits: https://stackoverflow.com/users/2521214/spektre
//=====================================================================================
void FillRoundedRGBA( float posx, float posy, float w, float h, float r, Vector4D rgba )
{
	// so it would have the same behavior as FillRGBA
	posx += w * 0.5f;
	posy += h * 0.5f;
	rgba.x = bound( 0.0f, rgba.x, 1.0f );
	rgba.y = bound( 0.0f, rgba.y, 1.0f );
	rgba.z = bound( 0.0f, rgba.z, 1.0f );
	rgba.w = bound( 0.0f, rgba.w, 1.0f );
	if( r > w * 0.5f ) r = w * 0.5f;
	if( r > h * 0.5f ) r = h * 0.5f;

	int i;
	float x0, y0, x, y;
	const float sina[45] = { 
		0,
		0.1736482,
		0.3420201,
		0.5,
		0.6427876,
		0.7660444,
		0.8660254,
		0.9396926,
		0.9848077,
		1,
		0.9848078,
		0.9396927,
		0.8660255,
		0.7660446,
		0.6427878,
		0.5000002,
		0.3420205,
		0.1736485,
		3.894144E-07,
		-0.1736478,
		-0.3420197,
		-0.4999996,
		-0.6427872,
		-0.7660443,
		-0.8660252,
		-0.9396925,
		-0.9848077,
		-1,
		-0.9848078,
		-0.9396928,
		-0.8660257,
		-0.7660449,
		-0.6427881,
		-0.5000006,
		-0.3420208,
		-0.1736489,
		0,
		0.1736482,
		0.3420201,
		0.5,
		0.6427876,
		0.7660444,
		0.8660254,
		0.9396926,
		0.9848077 };
	const float *cosa = sina + 9;
	w -= r + r;
	h -= r + r;

	GL_Texture2D( GL_FALSE );
	if( rgba.w < 1.0f )
	{
		GL_Blend( GL_TRUE );
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	GL_Color4f( rgba.x, rgba.y, rgba.z, rgba.w );

	pglBegin( GL_TRIANGLE_FAN );
	pglVertex2f( posx, posy );
	x0 = posx + (0.5 * w);
	y0 = posy + (0.5 * h);
	for( i = 0; i < 9; i++ )
	{
		x = x0 + (r * cosa[i]);
		y = y0 + (r * sina[i]);
		pglVertex2f( x, y );
	}
	x0 -= w;
	for( ; i < 18; i++ )
	{
		x = x0 + (r * cosa[i]);
		y = y0 + (r * sina[i]);
		pglVertex2f( x, y );
	}
	y0 -= h;
	for( ; i < 27; i++ )
	{
		x = x0 + (r * cosa[i]);
		y = y0 + (r * sina[i]);
		pglVertex2f( x, y );
	}
	x0 += w;
	for( ; i < 36; i++ )
	{
		x = x0 + (r * cosa[i]);
		y = y0 + (r * sina[i]);
		pglVertex2f( x, y );
	}
	pglVertex2f( x, posy + (0.5 * h) );
	pglEnd();

	GL_Texture2D( GL_TRUE );
	if( rgba.w < 1.0f )
		GL_Blend( GL_FALSE );
}