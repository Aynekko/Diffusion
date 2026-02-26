/*
r_shader.cpp - glsl shaders
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "const.h"
#include "ref_params.h"
#include "r_local.h"
#include <mathlib.h>
#include <stringlib.h>
#include "r_shader.h"
#include "virtualfs.h"
#include "r_particle.h"
#include "r_world.h"
#include "crclib.h"
#include "r_shaderlist.h"

static bool bPrecompiled = false;
static bool bCompilingShaders = false;
static int compiled_outdated_total = 0;
static int shaders_processed_total = 0;
static int compiled_count_total = 0;
static int compiled_failed_total = 0;

#define SHADERS_HASH_SIZE	(MAX_GLSL_PROGRAMS >> 2)
#define MAX_FILE_STACK	64

static char	filenames_stack[MAX_FILE_STACK][256];
glsl_program_t	glsl_programs[MAX_GLSL_PROGRAMS];
glsl_program_t	*glsl_programsHashTable[SHADERS_HASH_SIZE];
unsigned int	num_glsl_programs;
static int	file_stack_pos;
ref_shaders_t	glsl;

static const char *GL_GetDriverString( void )
{
	static char driver[256] = { 0 };
	if( driver[0] ) return driver;

	const char *vendor = (const char *)pglGetString( GL_VENDOR );
	const char *renderer = (const char *)pglGetString( GL_RENDERER );
	const char *version = (const char *)pglGetString( GL_VERSION );

	Q_snprintf( driver, sizeof( driver ), "%s_%s_%s", vendor ? vendor : "", renderer ? renderer : "", version ? version : "" );
	return driver;
}

static char *GL_GetShaderCacheFilename( const char *glname, const char *options, char *filename, size_t size )
{
	const char *driver = GL_GetDriverString();
	char unique_key[512];
	Q_snprintf( unique_key, sizeof( unique_key ), "%s%s%s", glname, options, driver );

	unsigned int crc = 0;
	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, unique_key, Q_strlen( unique_key ) );
	crc = CRC32_Final( crc );

	Q_snprintf( filename, size, ".shadercache/%08x.bin", crc );
	return filename;
}

static bool GL_IsSourceNewerThanCache( const char *vpname, const char *fpname, const char *defines, const char *cache_filename )
{
	// We need to check both VP and FP if they exist
	char vp_path[256] = { 0 };
	char fp_path[256] = { 0 };

	Q_snprintf( vp_path, sizeof( vp_path ), "glsl/%s_vp.glsl", vpname );
	Q_snprintf( fp_path, sizeof( fp_path ), "glsl/%s_fp.glsl", fpname );

	int retval = 0;

	// Compare VP vs cache
	if( COMPARE_FILE_TIME( vp_path, cache_filename, &retval ) )
	{
		if( retval > 0 ) // glsl in newer
			return true;
	}

	// Compare FP vs cache
	if( COMPARE_FILE_TIME( fp_path, cache_filename, &retval ) )
	{
		if( retval > 0 ) // glsl in newer
			return true;
	}

	return false;
}

void GL_EncodeNormal( char *options, int texturenum )
{
	if( RENDER_GET_PARM( PARM_TEX_GLFORMAT, texturenum ) == GL_COMPRESSED_RED_GREEN_RGTC2_EXT )
	{
		GL_AddShaderDirective( options, "NORMAL_RG_PARABOLOID" );
	}
	else if( RENDER_GET_PARM( PARM_TEX_GLFORMAT, texturenum ) == GL_COMPRESSED_LUMINANCE_ALPHA_3DC_ATI )
	{
		GL_AddShaderDirective( options, "NORMAL_3DC_PARABOLOID" );
	}
	else if( RENDER_GET_PARM( PARM_TEX_ENCODE, texturenum ) == DXT_ENCODE_NORMAL_AG_PARABOLOID )
	{
		GL_AddShaderDirective( options, "NORMAL_AG_PARABOLOID" );
	}
	else if( RENDER_GET_PARM( PARM_TEX_GLFORMAT, texturenum ) == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT )
	{
		// implicit DXT5NM format (Paranoia2 v 1.2 old stuff)
		if( FBitSet( RENDER_GET_PARM( PARM_TEX_FLAGS, texturenum ), TF_HAS_ALPHA ) )
			GL_AddShaderDirective( options, "NORMAL_AG_PARABOLOID" );
	}
}

static char *GL_PrintInfoLog( GLhandleARB object )
{
	static char	msg[8192];
	int		maxLength = 0;

	pglGetObjectParameterivARB( object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &maxLength );

	if( maxLength >= sizeof( msg ))
	{
		ALERT( at_warning, "GL_PrintInfoLog: message exceeds %i symbols\n", sizeof( msg ));
		maxLength = sizeof( msg ) - 1;
	}

	pglGetInfoLogARB( object, maxLength, &maxLength, msg );

	return msg;
}

static char *GL_PrintShaderSource( GLhandleARB object )
{
	static char	msg[8192];
	int		maxLength = 0;
	
	pglGetObjectParameterivARB( object, GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, &maxLength );

	if( maxLength >= sizeof( msg ))
	{
		ALERT( at_warning, "GL_PrintShaderSource: message exceeds %i symbols\n", sizeof( msg ));
		maxLength = sizeof( msg ) - 1;
	}

	pglGetShaderSourceARB( object, maxLength, &maxLength, msg );

	return msg;
}

static bool GL_PushFileStack( const char *filename )
{
	if( file_stack_pos < MAX_FILE_STACK )
	{
		Q_strncpy( filenames_stack[file_stack_pos], filename, sizeof( filenames_stack[0] ));
		file_stack_pos++;
		return true;
	}

	ALERT( at_error, "GL_PushFileStack: stack overflow\n" );
	return false;
}

static bool GL_PopFileStack( void )
{
	file_stack_pos--;

	if( file_stack_pos < 0 )
	{
		ALERT( at_error, "GL_PushFileStack: stack underflow\n" );
		return false;
	}

	return true;
}

static bool GL_CheckFileStack( const char *filename )
{
	for( int i = 0; i < file_stack_pos; i++ )
	{
		if( !Q_stricmp( filenames_stack[i], filename ))
			return true;
	}
	return false;
}

static bool GL_LoadSource( const char *filename, CVirtualFS *file )
{
	if( GL_CheckFileStack( filename ))
	{
		ALERT( at_error, "recursive include for %s\n", filename );
		return false;
	}

	int size;
	char *source = (char *)gEngfuncs.COM_LoadFile((char *)filename, 5, &size );
	if( !source )
	{
		ALERT( at_error, "couldn't load %s\n", filename );
		return false;
	}

	GL_PushFileStack( filename );
	file->Write( source, size );
	file->Seek( 0, SEEK_SET ); // rewind

	gEngfuncs.COM_FreeFile( source );

	return true;
}

static void GL_ParseFile( const char *filename, CVirtualFS *file, CVirtualFS *out )
{
	char	*pfile, token[256];
	int	ret, fileline = 1;
	char	line[2048];

//	out->Printf( "#file %s\n", filename );	// OpenGL doesn't support #file :-(
	out->Printf( "#line 0\n" );

	do
	{
		ret = file->Gets( line, sizeof( line ));
		pfile = line;

		// NOTE: if first keyword it's not an '#include' just ignore it
		pfile = COM_ParseFile( pfile, token );
		if( !Q_strcmp( token, "#include" ))
		{
			CVirtualFS incfile;
			char incname[256];

			pfile = COM_ParseLine( pfile, token );
			Q_snprintf( incname, sizeof( incname ), "glsl/%s", token );
			if( !GL_LoadSource( incname, &incfile ))
			{
				fileline++;
				continue;
                              }
			GL_ParseFile( incname, &incfile, out );
//			out->Printf( "#file %s\n", filename );	// OpenGL doesn't support #file :-(
			out->Printf( "#line %i\n", fileline );
		}
		else out->Printf( "%s\n", line );
		fileline++;
	} while( ret != EOF );

	GL_PopFileStack();
}

static bool GL_ProcessShader( const char *filename, CVirtualFS *out, bool compatible, const char *defines = NULL )
{
	CVirtualFS file;

	file_stack_pos = 0;	// debug

	if( !GL_LoadSource( filename, &file ))
		return false;

	// add internal defines
	out->Printf( "#version 130\n" );
	out->Printf( "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n" );
	out->Printf( "#ifndef M_PI2\n#define M_PI2 6.28318530717958647692\n#endif\n" );
	if( GL_Support( R_EXT_GPU_SHADER4 ))
	{
		out->Printf( "#extension GL_EXT_gpu_shader4 : require\n" ); // support bitwise ops
		out->Printf( "#define GLSL_gpu_shader4\n" );
		if( GL_Support( R_TEXTURE_ARRAY_EXT ))
			out->Printf( "#define GLSL_ALLOW_TEXTURE_ARRAY\n" );
	}
	else if( GL_Support( R_TEXTURE_ARRAY_EXT ))
	{
	//	out->Printf( "#extension GL_EXT_texture_array : require\n" ); // support texture arrays
		out->Printf( "#define GLSL_ALLOW_TEXTURE_ARRAY\n" );
	}
	if( defines ) out->Print( defines );

	// user may override this constants
	out->Printf( "#ifndef MAXSTUDIOBONES\n#define MAXSTUDIOBONES %i\n#endif\n", glConfig.max_skinning_bones );
	out->Printf( "#ifndef MAX_LIGHTSTYLES\n#define MAX_LIGHTSTYLES %i\n#endif\n", MAX_LIGHTSTYLES );
	out->Printf( "#ifndef MAXLIGHTMAPS\n#define MAXLIGHTMAPS %i\n#endif\n", MAXLIGHTMAPS );
	out->Printf( "#ifndef GRASS_ANIM_DIST\n#define GRASS_ANIM_DIST %f\n#endif\n", GRASS_ANIM_DIST );

	GL_ParseFile( filename, &file, out );
	out->Write( "", 1 ); // terminator

	return true;
}

static void GL_LoadGPUShader( glsl_program_t *shader, const char *name, GLenum shaderType, bool compatible, const char *defines = NULL )
{
	char		filename[256];
	GLhandleARB	object;
	CVirtualFS	source;
	GLint		compiled;

	ASSERT( shader != NULL );

	switch( shaderType )
	{
	case GL_VERTEX_SHADER_ARB:
		Q_snprintf( filename, sizeof( filename ), "glsl/%s_vp.glsl", name );
		break;
	case GL_FRAGMENT_SHADER_ARB:
		Q_snprintf( filename, sizeof( filename ), "glsl/%s_fp.glsl", name );
		break;
	default:
		ALERT( at_error, "GL_LoadGPUShader: unknown shader type %p\n", shaderType );
		return;
	}

	// load includes, add some directives
	if( !GL_ProcessShader( filename, &source, compatible, defines ))
		return;

	GLcharARB *buffer = (GLcharARB *)source.GetBuffer();
	int bufferSize = (int)source.GetSize();

	ALERT( at_aiconsole, "\nloading '%s'\n", filename );
	object = pglCreateShaderObjectARB( shaderType );
	pglShaderSourceARB( object, GL_TRUE, (const GLcharARB **)&buffer, &bufferSize );

	// compile shader
	pglCompileShaderARB( object );

	// check if shader compiled
	pglGetObjectParameterivARB( object, GL_OBJECT_COMPILE_STATUS_ARB, &compiled );

	if( !compiled )
	{
		if( developer_level ) Msg( "%s", GL_PrintInfoLog( object ));
		if( developer_level ) Msg( "Shader options:%s\n", GL_PretifyListOptions( defines ));
		ALERT( at_error, "Couldn't compile %s\n", filename ); 
		return;
	}

	if( shaderType == GL_VERTEX_SHADER_ARB )
		shader->status |= SHADER_VERTEX_COMPILED;
	else shader->status |= SHADER_FRAGMENT_COMPILED;

	// attach shader to program
	pglAttachObjectARB( shader->handle, object );

	// delete shader, no longer needed
	pglDeleteObjectARB( object );
}

static void GL_LinkProgram( glsl_program_t *shader )
{
	GLint	linked = 0;

	if( !shader ) return;

	pglLinkProgramARB( shader->handle );

	pglGetObjectParameterivARB( shader->handle, GL_OBJECT_LINK_STATUS_ARB, &linked );
	if( !linked ) ALERT( at_error, "%s\n%s shader failed to link\n", GL_PrintInfoLog( shader->handle ), shader->name );
	else shader->status |= SHADER_PROGRAM_LINKED;
}

static bool GL_ValidateProgram( glsl_program_t *shader )
{
	GLint validated = 0;

	if( !shader ) return false;

	pglValidateProgramARB( shader->handle );

	pglGetObjectParameterivARB( shader->handle, GL_OBJECT_VALIDATE_STATUS_ARB, &validated );

	if( !validated )
		ALERT( at_error, "%s\n%s shader failed to validate\n", GL_PrintInfoLog( shader->handle ), shader->name );

	return validated;
}

int GL_UniformTypeToDwordCount( GLuint type, bool align = false )
{
	switch( type )
	{
	case GL_INT:
	case GL_UNSIGNED_INT:
	case GL_SAMPLER_1D_ARB:
	case GL_SAMPLER_2D_ARB:
	case GL_SAMPLER_3D_ARB:
	case GL_SAMPLER_CUBE_ARB:
	case GL_SAMPLER_1D_SHADOW_ARB:
	case GL_SAMPLER_2D_SHADOW_ARB:
	case GL_SAMPLER_2D_RECT_ARB:
	case GL_SAMPLER_2D_RECT_SHADOW_ARB:
	case GL_SAMPLER_CUBE_SHADOW_EXT:
		return align ? 4 : 1;	// int[1]
	case GL_FLOAT:
		return align ? 4 : 1;	// float[1]
	case GL_FLOAT_VEC2_ARB:
		return align ? 4 : 2;	// float[2]
	case GL_FLOAT_VEC3_ARB:
		return align ? 4 : 3;	// float[3]
	case GL_FLOAT_VEC4_ARB:
		return 4;	// float[4]
	case GL_FLOAT_MAT2_ARB:
		return 4;	// float[2][2]
	case GL_FLOAT_MAT3_ARB:
		return align ? 12 : 9;	// float[3][3]
	case GL_FLOAT_MAT4_ARB:
		return 16;// float[4][4]
	default: return align ? 4 : 1; // assume error
	}
}

const char *GL_UniformTypeToName( GLuint type )
{
	switch( type )
	{
	case GL_INT:
		return "int";
	case GL_UNSIGNED_INT:
		return "uint";
	case GL_FLOAT:
		return "float";
	case GL_FLOAT_VEC2_ARB:
		return "vec2";
	case GL_FLOAT_VEC3_ARB:
		return "vec3";
	case GL_FLOAT_VEC4_ARB:
		return "vec4";
	case GL_FLOAT_MAT2_ARB:
		return "mat2";
	case GL_FLOAT_MAT3_ARB:
		return "mat3";
	case GL_FLOAT_MAT4_ARB:
		return "mat4";
	case GL_SAMPLER_1D_ARB:
		return "sampler1D";
	case GL_SAMPLER_2D_ARB:
		return "sampler2D";
	case GL_SAMPLER_3D_ARB:
		return "sampler3D";
	case GL_SAMPLER_CUBE_ARB:
		return "samplerCube";
	case GL_SAMPLER_1D_ARRAY_EXT:
		return "sampler1DArray";
	case GL_SAMPLER_2D_ARRAY_EXT:
		return "sampler2DArray";
	case GL_SAMPLER_1D_SHADOW_ARB:
		return "sampler1DShadow";
	case GL_SAMPLER_2D_SHADOW_ARB:
		return "sampler2DShadow";
	case GL_SAMPLER_2D_RECT_ARB:
		return "sampler2DRect";
	case GL_SAMPLER_2D_RECT_SHADOW_ARB:
		return "sampler2DRectShadow";
	case GL_SAMPLER_CUBE_SHADOW_EXT:
		return "samplerCubeShadow";
	default:	return "???";
	}
}

void GL_ShowProgramUniforms( glsl_program_t *shader )
{
	int	count, size;
	char	uniformName[256];
	int	total_uniforms_used = 0;
	GLuint	type;

	if( !shader || developer_level < DEV_EXTENDED )
		return;
	
	// install the executables in the program object as part of current state.
	pglUseProgramObjectARB( shader->handle );

	// query the number of active uniforms
	pglGetObjectParameterivARB( shader->handle, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &count );

	// Loop over each of the active uniforms, and set their value
	for( int i = 0; i < count; i++ )
	{
		pglGetActiveUniformARB( shader->handle, i, sizeof( uniformName ), NULL, &size, &type, uniformName );
		if( developer_level >= DEV_EXTENDED )
		{
			if( size != 1 )
			{
				char *end = Q_strchr( uniformName, '[' );
				if( end ) *end = '\0'; // cutoff [0]
			}

			if( size == 1 ) ALERT( at_aiconsole, "uniform %s %s;\n", GL_UniformTypeToName( type ), uniformName );
			else ALERT( at_aiconsole, "uniform %s %s[%i];\n", GL_UniformTypeToName( type ), uniformName, size );
		}
		total_uniforms_used += GL_UniformTypeToDwordCount( type ) * size;
	}

	if( total_uniforms_used >= glConfig.max_vertex_uniforms )
		ALERT( at_error, "used uniforms %i is overflowed max count %i\n", total_uniforms_used, glConfig.max_vertex_uniforms );
	else ALERT( at_aiconsole, "used uniforms %i from %i\n", total_uniforms_used, glConfig.max_vertex_uniforms );

	int max_shader_uniforms = total_uniforms_used;

	if( max_shader_uniforms > ( glConfig.max_skinning_bones * 7 ))
		max_shader_uniforms -= ( glConfig.max_skinning_bones * 7 );

	ALERT( at_aiconsole, "%s used %i uniforms\n", shader->name, max_shader_uniforms );

	if( max_shader_uniforms > glConfig.peak_used_uniforms )
	{
		glConfig.peak_used_uniforms = max_shader_uniforms;
		tr.show_uniforms_peak = true;
	}

	pglUseProgramObjectARB( GL_NONE );
}

void GL_BindShader( glsl_program_t *shader )
{
	if( !shader && RI->currentshader )
	{
		pglUseProgram( GL_NONE );
		RI->currentshader = NULL;
	}
	else if( shader != RI->currentshader && shader != NULL && shader->handle )
	{
		pglUseProgram( shader->handle );
		r_stats.num_shader_binds++;
		RI->currentshader = shader;
	}
}

static glsl_program_t *GL_InitGPUShader( const char *glname, const char *vpname, const char *fpname, const char *defines = NULL )
{
	const char	*find;
	unsigned int	i, hash;
	glsl_program_t	*prog;

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return NULL;

	ASSERT( glname != NULL );

	find = va( "%s %s", glname, defines );
	hash = COM_HashKey( find, SHADERS_HASH_SIZE );

	// check for coexist
	for( prog = glsl_programsHashTable[hash]; prog != NULL; prog = prog->nextHash )
	{
		if( !Q_strcmp( prog->name, glname ) && !Q_strcmp( prog->options, defines ) )
		{
			prog->status |= SHADER_STATUS_HASHED;
			return prog;
		}
	}

	// find free spot
	for( i = 1; i < num_glsl_programs; i++ )
		if( !glsl_programs[i].name[0] )
			break;

	if( num_glsl_programs >= MAX_GLSL_PROGRAMS )
	{
		ALERT( at_error, "GL_InitGPUShader: GLSL shaders limit exceeded (%i max)\n", MAX_GLSL_PROGRAMS );
		return NULL;
	}

	// alloc new shader
	glsl_program_t *shader = &glsl_programs[i];
	memset( shader, 0, sizeof( *shader ) );

	if( i == num_glsl_programs )
		num_glsl_programs++;

	Q_strncpy( shader->name, glname, sizeof( shader->name ));
	Q_strncpy( shader->options, defines, sizeof( shader->options ));
	shader->handle = pglCreateProgramObjectARB();

	if( !shader->handle )
	{
		ALERT( at_error, "GL_InitGPUShader: ^1failed to create program object for %s^7\n", glname );
		memset( shader, 0, sizeof( *shader ) );
		if( i == num_glsl_programs - 1 ) num_glsl_programs--;
		return NULL;
	}

	const bool bSupportBinaryShader = GL_Support( R_BINARY_SHADER_EXT );

	// try load from file
	if( bSupportBinaryShader )
	{
		pglProgramParameteriARB( shader->handle, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );

		char filename[256];
		GL_GetShaderCacheFilename( glname, defines ? defines : "", filename, sizeof( filename ) );

		int filesize;
		byte *data = (byte *)gEngfuncs.COM_LoadFile( filename, 5, &filesize );

		if( data && filesize > (int)sizeof( GLuint ) )
		{
			if( !GL_IsSourceNewerThanCache( vpname, fpname, defines, filename ) )
			{
				GLuint format = *(GLuint *)data;
				byte *binary = data + sizeof( GLuint );
				int bin_size = filesize - sizeof( GLuint );

				pglProgramBinaryARB( shader->handle, format, binary, bin_size );

				GLint linked = 0;
				pglGetObjectParameterivARB( shader->handle, GL_OBJECT_LINK_STATUS_ARB, &linked );

				if( linked )
				{
					shader->status = SHADER_VERTEX_COMPILED | SHADER_FRAGMENT_COMPILED | SHADER_PROGRAM_LINKED | SHADER_STATUS_FROMBINARY;
					shader->nextHash = NULL;

					ALERT( at_aiconsole, "\n^2Loaded shader binary:^7 %s\n", filename );

					gEngfuncs.COM_FreeFile( data );

					// add to hash table
					shader->nextHash = glsl_programsHashTable[hash];
					glsl_programsHashTable[hash] = shader;
					return shader;
				}
				else
				{
					ALERT( at_warning, "^1Cached binary %s FAILED to link:^7 %s\n", filename, GL_PrintInfoLog( shader->handle ) );
					pglDeleteObjectARB( shader->handle );
					shader->handle = pglCreateProgramObjectARB();  // Recreate
					pglProgramParameteriARB( shader->handle, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );
				}
			}
			else
			{
				ALERT( at_aiconsole, "^3Shader binary for %s is outdated and will be recompiled.^7\n", glname );
				compiled_outdated_total++;
			}

			gEngfuncs.COM_FreeFile( data );
		}
	}

	// Fallback: compile from source
	if( vpname )
		GL_LoadGPUShader( shader, vpname, GL_VERTEX_SHADER_ARB, false, defines );

	if( fpname )
		GL_LoadGPUShader( shader, fpname, GL_FRAGMENT_SHADER_ARB, false, defines );

	if( vpname && FBitSet( shader->status, SHADER_VERTEX_COMPILED ) )
	{
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_POSITION, "attr_Position" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD0, "attr_TexCoord0" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD1, "attr_TexCoord1" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD2, "attr_TexCoord2" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_NORMAL, "attr_Normal" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BONE_INDEXES, "attr_BoneIndexes" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BONE_WEIGHTS, "attr_BoneWeights" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_STYLES, "attr_LightStyles" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_COLOR, "attr_LightColor" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_VECS, "attr_LightVecs" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TANGENT, "attr_Tangent" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BINORMAL, "attr_Binormal" );
	}

	GL_LinkProgram( shader );

	if( !FBitSet( shader->status, SHADER_PROGRAM_LINKED ) || !FBitSet( shader->status, SHADER_VERTEX_COMPILED ) || !FBitSet( shader->status, SHADER_FRAGMENT_COMPILED ) )
	{
		if( shader->handle ) pglDeleteObjectARB( shader->handle );
		memset( shader, 0, sizeof( *shader ) );
		if( i == num_glsl_programs - 1 ) num_glsl_programs--;
		return NULL;
	}

	// Cache binary if supported
	if( bSupportBinaryShader )
	{
		char filename[256];
		GL_GetShaderCacheFilename( glname, defines ? defines : "", filename, sizeof( filename ) );

		GLint bin_length = 0;
		pglGetProgramiv( shader->handle, GL_PROGRAM_BINARY_LENGTH, &bin_length );
		if( bin_length <= 0 )
		{
			// Fallback for drivers that only accept old name
			pglGetObjectParameterivARB( shader->handle, GL_PROGRAM_BINARY_LENGTH, &bin_length );
		}

		ALERT( at_aiconsole, "Program binary length for %s: %d bytes\n", glname, bin_length );

		if( bin_length > 0 )
		{
			byte *buffer = (byte *)Mem_Alloc( bin_length + sizeof( GLuint ) );
			GLuint format = 0;
			GLint ret_len = 0;

			pglGetProgramBinaryARB( shader->handle, bin_length, &ret_len, &format, buffer + sizeof( GLuint ) );

			if( ret_len == bin_length )
			{
				*(GLuint *)buffer = format;
				gRenderfuncs.pfnSaveFile( filename, buffer, bin_length + sizeof( GLuint ) );
				ALERT( at_aiconsole, "^2Saved shader cache to:^7 %s (%d bytes)\n", filename, bin_length );
			}
			else
			{
				ALERT( at_warning, "^1Failed to retrieve program binary for %s^7\n", glname );
			}

			Mem_Free( buffer );
		}
	}

	// add to hash table
	shader->nextHash = glsl_programsHashTable[hash];
	glsl_programsHashTable[hash] = shader;

	return shader;
}

static void GL_FreeGPUShader( glsl_program_t *shader )
{
	if( shader && shader->handle )
	{
		unsigned int	hash;
		glsl_program_t	*cur;
		glsl_program_t	**prev;
		const char	*find;

		find = va( "%s %s", shader->name, shader->options );

		// remove from hash table
		hash = COM_HashKey( find, SHADERS_HASH_SIZE );
		prev = &glsl_programsHashTable[hash];

		while( 1 )
		{
				cur = *prev;
				if( !cur ) break;

				if( cur == shader )
			{
					*prev = cur->nextHash;
				break;
			}
				prev = &cur->nextHash;
		}

		pglDeleteObjectARB( shader->handle );
		memset( shader, 0, sizeof( *shader ));
	}
}

static glsl_program_t *GL_CreateUberShader( GLuint slot, const char *glname, const char *defines = NULL, pfnShaderCallback pfnInitUniforms = NULL )
{
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return NULL;

	if( num_glsl_programs >= MAX_GLSL_PROGRAMS )
	{
		ALERT( at_error, "GL_CreateUberShader: GLSL shaders limit exceeded (%i max)\n", MAX_GLSL_PROGRAMS );
		return NULL;
	}

	// alloc new shader
	glsl_program_t *shader = &glsl_programs[slot];
	memset( shader, 0, sizeof( *shader ) );

	const bool bSupportBinaryShader = GL_Support( R_BINARY_SHADER_EXT );

	if( bSupportBinaryShader )
	{
		char filename[256];
		GL_GetShaderCacheFilename( glname, defines, filename, sizeof( filename ) );

		int filesize;
		byte *data = (byte *)gEngfuncs.COM_LoadFile( filename, 5, &filesize );

		if( data && filesize > sizeof( GLuint ) )
		{
			if( !GL_IsSourceNewerThanCache( glname, glname, defines, filename ) )
			{
				GLuint format = *(GLuint *)data;
				byte *binary = data + sizeof( GLuint );
				int bin_size = filesize - sizeof( GLuint );

				shader->handle = pglCreateProgramObjectARB();
				pglProgramBinaryARB( shader->handle, format, binary, bin_size );
				pglProgramParameteriARB( shader->handle, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );

				if( FBitSet( shader->status, SHADER_VERTEX_COMPILED ) )
				{
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_POSITION, "attr_Position" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD0, "attr_TexCoord0" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD1, "attr_TexCoord1" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD2, "attr_TexCoord2" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_NORMAL, "attr_Normal" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BONE_INDEXES, "attr_BoneIndexes" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BONE_WEIGHTS, "attr_BoneWeights" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_STYLES, "attr_LightStyles" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_COLOR, "attr_LightColor" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_VECS, "attr_LightVecs" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TANGENT, "attr_Tangent" );
					pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BINORMAL, "attr_Binormal" );
				}

				GLint linked = 0;
				pglGetObjectParameterivARB( shader->handle, GL_OBJECT_LINK_STATUS_ARB, &linked );

				if( linked )
				{
					shader->status = SHADER_VERTEX_COMPILED | SHADER_FRAGMENT_COMPILED | SHADER_PROGRAM_LINKED | SHADER_UBERSHADER | SHADER_STATUS_FROMBINARY;
					Q_strncpy( shader->name, glname, sizeof( shader->name ) );
					Q_strncpy( shader->options, defines, sizeof( shader->options ) );

					if( pfnInitUniforms ) pfnInitUniforms( shader );
					GL_ShowProgramUniforms( shader );  // If dev mode

					gEngfuncs.COM_FreeFile( data );
					ALERT( at_aiconsole, "\n^2GL_CreateUberShader: Loaded shader binary:^7 %s\n", filename );

					if( slot == num_glsl_programs )
					{
						if( num_glsl_programs == MAX_GLSL_PROGRAMS )
						{
							ALERT( at_error, "^1GL_CreateUberShader: GLSL shaders limit exceeded (%i max)^7\n", MAX_GLSL_PROGRAMS );
							GL_FreeGPUShader( shader );
							return NULL;
						}
						num_glsl_programs++;
					}

					return shader;
				}
				else
				{
					ALERT( at_warning, "^1GL_CreateUberShader: Failed to load shader binary %s:^7 %s\n", filename, GL_PrintInfoLog( shader->handle ) );
					pglDeleteObjectARB( shader->handle );
					shader->handle = 0;
				}
			}
			else
				ALERT( at_aiconsole, "^3GL_CreateUberShader Shader binary for %s is outdated and will be recompiled.^7\n", glname );

			gEngfuncs.COM_FreeFile( data );
		}
	}

	shader->handle = pglCreateProgramObjectARB();
	if( !shader->handle )
		return NULL; // some bad happens

	Q_strncpy( shader->name, glname, sizeof( shader->name ));
	Q_strncpy( shader->options, defines, sizeof( shader->options ));

	GL_LoadGPUShader( shader, glname, GL_VERTEX_SHADER_ARB, true, defines );
	GL_LoadGPUShader( shader, glname, GL_FRAGMENT_SHADER_ARB, true, defines );

	if( FBitSet( shader->status, SHADER_VERTEX_COMPILED ))
	{
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_POSITION, "attr_Position" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD0, "attr_TexCoord0" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD1, "attr_TexCoord1" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD2, "attr_TexCoord2" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_NORMAL, "attr_Normal" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BONE_INDEXES, "attr_BoneIndexes" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BONE_WEIGHTS, "attr_BoneWeights" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_STYLES, "attr_LightStyles" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_COLOR, "attr_LightColor" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_LIGHT_VECS, "attr_LightVecs" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TANGENT, "attr_Tangent" );
		pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BINORMAL, "attr_Binormal" );
	}

	GL_LinkProgram( shader );

	if( pfnInitUniforms != NULL && FBitSet( shader->status, SHADER_PROGRAM_LINKED ))
		pfnInitUniforms( shader ); // register shader uniforms

	shader->status |= SHADER_UBERSHADER; // it's UberShader!

	ALERT( at_aiconsole, "^2CompileUberShader #%i:^7 %s\n%s\n", slot, glname, defines );

	if( developer_level && bPrecompiled )
	{
		// development aid - log shaders here if they were compiled/loaded long after the initialization stage
		char log_filename[256] = "shaders_unlisted.log";
		FILE *log_file = fopen( log_filename, "a" );  // Append mode
		if( log_file )
		{
			fprintf( log_file, "%s | %s\n", glname, defines ? defines : "" );  // Format: glname | defines
			fclose( log_file );
		}
	}

	if( bSupportBinaryShader )
	{
		char filename[256];
		GL_GetShaderCacheFilename( glname, defines, filename, sizeof( filename ) );

		GLint bin_length = 0;
		pglGetProgramiv( shader->handle, GL_PROGRAM_BINARY_LENGTH, &bin_length );
		if( bin_length <= 0 )
		{
			// Fallback for drivers that only accept old name
			pglGetObjectParameterivARB( shader->handle, GL_PROGRAM_BINARY_LENGTH, &bin_length );
		}

		if( bin_length > 0 )
		{
			byte *buffer = (byte *)Mem_Alloc( bin_length + sizeof( GLuint ) );
			GLuint format;
			GLint ret_len = 0;

			pglGetProgramBinaryARB( shader->handle, bin_length, &ret_len, &format, buffer + sizeof( GLuint ) );

			if( ret_len == bin_length )
			{
				*(GLuint *)buffer = format;
				gRenderfuncs.pfnSaveFile( filename, buffer, bin_length + sizeof( GLuint ) );
				ALERT( at_aiconsole, "^2Cached shader binary:^7 %s\n", filename );
			}

			Mem_Free( buffer );
		}
	}

	if( slot == num_glsl_programs )
	{
		if( num_glsl_programs == MAX_GLSL_PROGRAMS )
		{
			ALERT( at_error, "GL_CreateUberShader: GLSL shaders limit exceeded (%i max)\n", MAX_GLSL_PROGRAMS );
			GL_FreeGPUShader( shader );
			return NULL;
		}
		num_glsl_programs++;
	}

	// all done, program is loaded

	return shader;
}

static glsl_program_t *GL_FindUberShader( const char *glname, const char *options = NULL, pfnShaderCallback pfnInitUniforms = NULL )
{
	const char	*find;
	unsigned int	i, hash;
	glsl_program_t	*prog;

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return NULL;

	ASSERT( glname != NULL );

	find = va( "%s %s", glname, options );
	hash = COM_HashKey( find, SHADERS_HASH_SIZE );

	// check for coexist
	for( prog = glsl_programsHashTable[hash]; prog != NULL; prog = prog->nextHash )
	{
		if( !Q_strcmp( prog->name, glname ) && !Q_strcmp( prog->options, options ) )
		{
			prog->status |= SHADER_STATUS_HASHED;
			return prog;
		}
	}

	// find free spot
	for( i = 1; i < num_glsl_programs; i++ )
		if( !glsl_programs[i].name[0] )
			break;

	prog = GL_CreateUberShader( i, glname, options, pfnInitUniforms );

	if( prog != NULL )
	{
		// add to hash table
		prog->nextHash = glsl_programsHashTable[hash];
		glsl_programsHashTable[hash] = prog;
	}

	return prog;
}

void GL_AddShaderDirective( char *options, const char *directive )
{
	Q_strncat( options, va( "#define %s\n", directive ), MAX_OPTIONS_LENGTH );
}

void GL_AddShaderDefine( char *options, const char *directive )
{
	Q_strncat( options, directive, MAX_OPTIONS_LENGTH );
}

bool GL_FindShaderDirective( glsl_program_t *shader, const char *directive )
{
	if( Q_stristr( shader->options, directive ))
		return true;
	return false;
}

static void GL_InitSolidBmodelUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	const bool bWater = GL_FindShaderDirective( shader, "BMODEL_WATER" );
	const bool bRefractedWater = (bWater && GL_FindShaderDirective( shader, "BMODEL_WATER_REFRACTION" ));

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_LightMap = pglGetUniformLocationARB( shader->handle, "u_LightMap" );
	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );

	if( GL_FindShaderDirective( shader, "REFLECTION_CUBEMAP" ) ) // diffusioncubemaps
	{
		shader->u_CubemapBox = pglGetUniformLocationARB( shader->handle, "u_CubemapBox" );
		shader->u_Cubemap = pglGetUniformLocationARB( shader->handle, "u_Cubemap" );
		shader->u_ReflectScale = pglGetUniformLocationARB( shader->handle, "u_ReflectScale" );
	}

	if( GL_FindShaderDirective( shader, "BMODEL_INTERIOR" ) )
	{
		shader->u_InteriorMap = pglGetUniformLocationARB( shader->handle, "u_InteriorMap" );
		shader->u_RealTime = pglGetUniformLocationARB( shader->handle, "u_RealTime" );
		shader->u_InteriorParams = pglGetUniformLocationARB( shader->handle, "u_InteriorParams" );
	}
	else if( GL_FindShaderDirective( shader, "BMODEL_REFLECTION_PLANAR" ) || GL_FindShaderDirective( shader, "BMODEL_WATER_PLANAR" ) )
	{
		shader->u_PlanarReflectScale = pglGetUniformLocationARB( shader->handle, "u_PlanarReflectScale" );
		if( GL_FindShaderDirective( shader, "BMODEL_WATER_PLANAR" ) )
			shader->u_WaterTex = pglGetUniformLocationARB( shader->handle, "u_WaterTex" );
	}

	shader->u_BrushParams = pglGetUniformLocationARB( shader->handle, "u_BrushParams" );

	if( bRefractedWater )
	{
		shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );
	//	shader->u_zFar = pglGetUniformLocationARB( shader->handle, "u_zFar" );
	}

	if( !bWater )
		shader->u_DeluxeMap = pglGetUniformLocationARB( shader->handle, "u_DeluxeMap" );

	if( GL_FindShaderDirective( shader, "BMODEL_SPECULAR" ) )
	{
		shader->u_GlossScale = pglGetUniformLocationARB( shader->handle, "u_GlossScale" );
		shader->u_GlossSmoothness = pglGetUniformLocationARB( shader->handle, "u_GlossSmoothness" );
	}

	if( GL_FindShaderDirective( shader, "BMODEL_EMBOSS" ) )
		shader->u_EmbossScale = pglGetUniformLocationARB( shader->handle, "u_EmbossScale" );

	shader->u_Fresnel = pglGetUniformLocationARB( shader->handle, "u_Fresnel" );

	if( GL_FindShaderDirective( shader, "BMODEL_MULTI_LAYERS" ) )
		shader->u_LayerMap = pglGetUniformLocationARB( shader->handle, "u_LayerMap" );

	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );

	shader->u_LightStyleValues = pglGetUniformLocationARB( shader->handle, "u_LightStyleValues" );
	shader->u_ModelMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelMatrix" );
	shader->u_TexOffset = pglGetUniformLocationARB( shader->handle, "u_TexOffset" );
	shader->u_RenderColor = pglGetUniformLocationARB( shader->handle, "u_RenderColor" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_LightMap, GL_TEXTURE1 );
	if( !bWater )
		pglUniform1iARB( shader->u_DeluxeMap, GL_TEXTURE2 );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE3 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE4 );

	if( GL_FindShaderDirective( shader, "BMODEL_MULTI_LAYERS" ) )
		pglUniform1iARB( shader->u_LayerMap, GL_TEXTURE5 );
	else if( GL_FindShaderDirective( shader, "BMODEL_INTERIOR" ) )
		pglUniform1iARB( shader->u_InteriorMap, GL_TEXTURE5 );
	else if( GL_FindShaderDirective( shader, "BMODEL_WATER_PLANAR" ) )
		pglUniform1iARB( shader->u_WaterTex, GL_TEXTURE5 );

	if( bRefractedWater )
		pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE6 );

	if( GL_FindShaderDirective( shader, "REFLECTION_CUBEMAP" ) )  // diffusioncubemaps
	{
		if( bWater )
			pglUniform1iARB( shader->u_CubemapBox, GL_TEXTURE2 ); // water won't use deluxmap so I'll use it here
		else
			pglUniform1iARB( shader->u_CubemapBox, GL_TEXTURE6 ); // other brushes won't use depthmap so I'll use it here
	}

	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBmodelDlightUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_ProjectMap = pglGetUniformLocationARB( shader->handle, "u_ProjectMap" );
	shader->u_ShadowMap = pglGetUniformLocationARB( shader->handle, "u_ShadowMap" ); // shadow2D or shadowCube
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	if( GL_FindShaderDirective( shader, "BMODEL_WATER_REFRACTION" ) )
	{
		shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
		shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );
	}

	if( GL_FindShaderDirective( shader, "BMODEL_MULTI_LAYERS" ))
		shader->u_LayerMap = pglGetUniformLocationARB( shader->handle, "u_LayerMap" );

	shader->u_ModelMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelMatrix" );
	shader->u_TexOffset = pglGetUniformLocationARB( shader->handle, "u_TexOffset" );
	shader->u_LightViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_LightViewProjectionMatrix" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );
	shader->u_RenderColor = pglGetUniformLocationARB( shader->handle, "u_RenderColor" );
	shader->u_DynLightBrightness = pglGetUniformLocationARB( shader->handle, "u_DynLightBrightness" );
	shader->u_LightParams = pglGetUniformLocationARB( shader->handle, "u_LightParams" );
	if( GL_FindShaderDirective( shader, "BMODEL_SPECULAR" ) )
	{
		shader->u_GlossScale = pglGetUniformLocationARB( shader->handle, "u_GlossScale" );
		shader->u_GlossSmoothness = pglGetUniformLocationARB( shader->handle, "u_GlossSmoothness" );
	}
	if( GL_FindShaderDirective( shader, "BMODEL_EMBOSS" ) )
		shader->u_EmbossScale = pglGetUniformLocationARB( shader->handle, "u_EmbossScale" );

	if( GL_FindShaderDirective( shader, "BMODEL_INTERIOR" ) )
	{
		shader->u_InteriorMap = pglGetUniformLocationARB( shader->handle, "u_InteriorMap" );
		shader->u_RealTime = pglGetUniformLocationARB( shader->handle, "u_RealTime" );
		shader->u_InteriorParams = pglGetUniformLocationARB( shader->handle, "u_InteriorParams" );
	}

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ProjectMap, GL_TEXTURE2 );	// projection lights only
	pglUniform1iARB( shader->u_ShadowMap, GL_TEXTURE3 );	// shadowmap or cubemap
	if( GL_FindShaderDirective( shader, "BMODEL_WATER_REFRACTION" ) )
		pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE4 );

	if( GL_FindShaderDirective( shader, "BMODEL_MULTI_LAYERS" ))
		pglUniform1iARB( shader->u_LayerMap, GL_TEXTURE5 );

	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE6 );

	if( GL_FindShaderDirective( shader, "BMODEL_INTERIOR" ) )
		pglUniform1iARB( shader->u_InteriorMap, GL_TEXTURE7 );

	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBmodelDepthFillUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_TexOffset = pglGetUniformLocationARB( shader->handle, "u_TexOffset" );
	shader->u_ModelMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelMatrix" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBmodelDecalUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_DecalMap = pglGetUniformLocationARB( shader->handle, "u_DecalMap" );
	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_LightMap = pglGetUniformLocationARB( shader->handle, "u_LightMap" );

	shader->u_LightStyleValues = pglGetUniformLocationARB( shader->handle, "u_LightStyleValues" );
	shader->u_ModelMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelMatrix" );
	shader->u_FogParams = pglGetUniformLocationARB( shader->handle, "u_FogParams" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_DecalMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE1 ); // detect alpha holes
	pglUniform1iARB( shader->u_LightMap, GL_TEXTURE2 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitSolidStudioUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_BoneQuaternion = pglGetUniformLocationARB( shader->handle, "u_BoneQuaternion" );
	shader->u_BonePosition = pglGetUniformLocationARB( shader->handle, "u_BonePosition" );
	if( GL_FindShaderDirective( shader, "STUDIO_SPECULAR" ) )
	{
		shader->u_GlossScale = pglGetUniformLocationARB( shader->handle, "u_GlossScale" );
		shader->u_GlossSmoothness = pglGetUniformLocationARB( shader->handle, "u_GlossSmoothness" );
	}
	if( GL_FindShaderDirective( shader, "STUDIO_EMBOSS" ) )
		shader->u_EmbossScale = pglGetUniformLocationARB( shader->handle, "u_EmbossScale" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_ColorMask = pglGetUniformLocationARB( shader->handle, "u_ColorMask" );
	shader->u_MeshParams = pglGetUniformLocationARB( shader->handle, "u_MeshParams" );
	shader->u_StudioLighting = pglGetUniformLocationARB( shader->handle, "u_StudioLighting" );

	if( GL_FindShaderDirective( shader, "REFLECTION_CUBEMAP" ) ) // diffusioncubemaps
	{
		shader->u_CubemapBox = pglGetUniformLocationARB( shader->handle, "u_CubemapBox" );
		shader->u_Cubemap = pglGetUniformLocationARB( shader->handle, "u_Cubemap" );
		shader->u_ReflectScale = pglGetUniformLocationARB( shader->handle, "u_ReflectScale" );
		shader->u_Fresnel = pglGetUniformLocationARB( shader->handle, "u_Fresnel" );
	}
	
	if( GL_FindShaderDirective( shader, "STUDIO_INTERIOR" ) )
	{
		shader->u_InteriorMap = pglGetUniformLocationARB( shader->handle, "u_InteriorMap" );
		shader->u_InteriorParams = pglGetUniformLocationARB( shader->handle, "u_InteriorParams" );
	}
	else if( GL_FindShaderDirective( shader, "STUDIO_TEXTURE_BLEND" ) )
	{
		shader->u_BlendTexture = pglGetUniformLocationARB( shader->handle, "u_BlendTexture" );
	}

	if( GL_FindShaderDirective( shader, "STUDIO_VERTEX_LIGHTING" ))
	{ 
		shader->u_GammaTable = pglGetUniformLocationARB( shader->handle, "u_GammaTable" );
	}

	shader->u_RenderColor = pglGetUniformLocationARB( shader->handle, "u_RenderColor" );
	shader->u_StudioParams = pglGetUniformLocationARB( shader->handle, "u_StudioParams" );
	if( GL_FindShaderDirective( shader, "STUDIO_SWAY_FOLIAGE" ) )
		shader->u_FoliageSwayHeight = pglGetUniformLocationARB( shader->handle, "u_FoliageSwayHeight" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE1 );
	if( GL_FindShaderDirective( shader, "REFLECTION_CUBEMAP" ) )  // diffusioncubemaps
		pglUniform1iARB( shader->u_CubemapBox, GL_TEXTURE2 );
	
	if( GL_FindShaderDirective( shader, "STUDIO_INTERIOR" ) )
		pglUniform1iARB( shader->u_InteriorMap, GL_TEXTURE4 );
	else if( GL_FindShaderDirective( shader, "STUDIO_TEXTURE_BLEND" ) ) // they can't be together
	{
		pglUniform1iARB( shader->u_BlendTexture, GL_TEXTURE4 );
		// the amount of dirt on the car is being sent through u_MeshParams[2].y
	}

	if( GL_FindShaderDirective( shader, "STUDIO_HAS_COLORMASK" ) )
		pglUniform1iARB( shader->u_ColorMask, GL_TEXTURE5 );

	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitStudioDlightUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_ProjectMap = pglGetUniformLocationARB( shader->handle, "u_ProjectMap" );
	shader->u_ShadowMap = pglGetUniformLocationARB( shader->handle, "u_ShadowMap" );
	shader->u_BoneQuaternion = pglGetUniformLocationARB( shader->handle, "u_BoneQuaternion" );
	shader->u_BonePosition = pglGetUniformLocationARB( shader->handle, "u_BonePosition" );
	shader->u_DynLightBrightness = pglGetUniformLocationARB( shader->handle, "u_DynLightBrightness" );
	shader->u_RenderColor = pglGetUniformLocationARB( shader->handle, "u_RenderColor" );
	if( GL_FindShaderDirective( shader, "STUDIO_SPECULAR" ) )
	{
		shader->u_GlossScale = pglGetUniformLocationARB( shader->handle, "u_GlossScale" );
		shader->u_GlossSmoothness = pglGetUniformLocationARB( shader->handle, "u_GlossSmoothness" );
	}
	if( GL_FindShaderDirective( shader, "STUDIO_EMBOSS" ) )
		shader->u_EmbossScale = pglGetUniformLocationARB( shader->handle, "u_EmbossScale" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_ColorMask = pglGetUniformLocationARB( shader->handle, "u_ColorMask" );
	shader->u_LightViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_LightViewProjectionMatrix" );
	shader->u_LightParams = pglGetUniformLocationARB( shader->handle, "u_LightParams" );
	shader->u_FogParams = pglGetUniformLocationARB( shader->handle, "u_FogParams" );
	shader->u_MeshParams = pglGetUniformLocationARB( shader->handle, "u_MeshParams" );

	if( GL_FindShaderDirective( shader, "STUDIO_INTERIOR" ) )
	{
		shader->u_InteriorMap = pglGetUniformLocationARB( shader->handle, "u_InteriorMap" );
		shader->u_InteriorParams = pglGetUniformLocationARB( shader->handle, "u_InteriorParams" );
	}
	else if( GL_FindShaderDirective( shader, "STUDIO_TEXTURE_BLEND" ) )
	{
		shader->u_BlendTexture = pglGetUniformLocationARB( shader->handle, "u_BlendTexture" );
	}

	shader->u_RealTime = pglGetUniformLocationARB( shader->handle, "u_RealTime" );
	shader->u_FoliageSwayHeight = pglGetUniformLocationARB( shader->handle, "u_FoliageSwayHeight" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ProjectMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_ShadowMap, GL_TEXTURE2 ); // optional
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE3 );
	if( GL_FindShaderDirective( shader, "STUDIO_INTERIOR" ) )
		pglUniform1iARB( shader->u_InteriorMap, GL_TEXTURE4 );
	else if( GL_FindShaderDirective( shader, "STUDIO_TEXTURE_BLEND" ) )
		pglUniform1iARB( shader->u_BlendTexture, GL_TEXTURE4 );

	if( GL_FindShaderDirective( shader, "STUDIO_HAS_COLORMASK" ) )
		pglUniform1iARB( shader->u_ColorMask, GL_TEXTURE5 );

	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitStudioDepthFillUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_RealTime = pglGetUniformLocationARB( shader->handle, "u_RealTime" );
	shader->u_FoliageSwayHeight = pglGetUniformLocationARB( shader->handle, "u_FoliageSwayHeight" );

	shader->u_BoneQuaternion = pglGetUniformLocationARB( shader->handle, "u_BoneQuaternion" );
	shader->u_BonePosition = pglGetUniformLocationARB( shader->handle, "u_BonePosition" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGrassSolidUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );

	shader->u_ModelMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelMatrix" );
	shader->u_LightStyleValues = pglGetUniformLocationARB( shader->handle, "u_LightStyleValues" );
	shader->u_GammaTable = pglGetUniformLocationARB( shader->handle, "u_GammaTable" );
	shader->u_GrassParams = pglGetUniformLocationARB( shader->handle, "u_GrassParams" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );
	shader->u_GrassWind = pglGetUniformLocationARB( shader->handle, "u_GrassWind" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGrassDlightUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_ProjectMap = pglGetUniformLocationARB( shader->handle, "u_ProjectMap" );
	shader->u_ShadowMap = pglGetUniformLocationARB( shader->handle, "u_ShadowMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );

	shader->u_LightViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_LightViewProjectionMatrix" );
	shader->u_ModelMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelMatrix" );
	shader->u_LightParams = pglGetUniformLocationARB( shader->handle, "u_LightParams" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );
	shader->u_GrassWind = pglGetUniformLocationARB( shader->handle, "u_GrassWind" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ProjectMap, GL_TEXTURE1 );// projection texture or XY attenuation
	pglUniform1iARB( shader->u_ShadowMap, GL_TEXTURE2 ); // shadowmap or cubemap
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE3 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGenericDlightUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_ProjectMap = pglGetUniformLocationARB( shader->handle, "u_ProjectMap" );
	shader->u_ShadowMap = pglGetUniformLocationARB( shader->handle, "u_ShadowMap" );

	shader->u_LightViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_LightViewProjectionMatrix" );
	shader->u_LightParams = pglGetUniformLocationARB( shader->handle, "u_LightParams" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ProjectMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_ShadowMap, GL_TEXTURE2 ); // optional
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitStudioDecalUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_DecalMap = pglGetUniformLocationARB( shader->handle, "u_DecalMap" );
	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_FogParams = pglGetUniformLocationARB( shader->handle, "u_FogParams" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_DecalMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitSkyBoxUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );

	shader->u_LightDir = pglGetUniformLocationARB( shader->handle, "u_LightDir" );
	shader->u_LightDiffuse = pglGetUniformLocationARB( shader->handle, "u_LightDiffuse" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );
	shader->u_FogParams = pglGetUniformLocationARB( shader->handle, "u_FogParams" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitPostProcessUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_BlurFactor = pglGetUniformLocationARB( shader->handle, "u_BlurFactor" );
	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBilateralBlurUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGenericFogUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );

	shader->u_FogParams = pglGetUniformLocationARB( shader->handle, "u_FogParams" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGenShaftsUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_zFar = pglGetUniformLocationARB( shader->handle, "u_zFar" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitDrawShaftsUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_Sunshafts = pglGetUniformLocationARB( shader->handle, "u_Sunshafts" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE2 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGenSSAOUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_zFar = pglGetUniformLocationARB( shader->handle, "u_zFar" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGenHBAOUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_BlendTexture = pglGetUniformLocationARB( shader->handle, "u_BlendTexture" );
	shader->u_HBAOParams = pglGetUniformLocationARB( shader->handle, "u_HBAOParams" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_BlendTexture, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitDrawSSAOUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_AOMap = pglGetUniformLocationARB( shader->handle, "u_AOMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );
	shader->u_FogParams = pglGetUniformLocationARB( shader->handle, "u_FogParams" );
	shader->u_zFar = pglGetUniformLocationARB( shader->handle, "u_zFar" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_AOMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE2 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitMotionBlurUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_MotionBlur = pglGetUniformLocationARB( shader->handle, "u_MotionBlur" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitHorizontalBlurUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_Accum = pglGetUniformLocationARB( shader->handle, "u_Accum" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitHeatDistortionUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitLensFlareUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_LightOrigin = pglGetUniformLocationARB( shader->handle, "u_LightOrigin" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );
	shader->u_Accum = pglGetUniformLocationARB( shader->handle, "u_Accum" );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitMonochromeUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_Accum = pglGetUniformLocationARB( shader->handle, "u_Accum" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitEnhanceUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitFSRUniforms( glsl_program_t *shader, bool bEASU )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );
	if( bEASU )
		shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitSMAAEdgeDetectUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitSMAABlendWeightUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_SearchTex = pglGetUniformLocationARB( shader->handle, "u_SearchTex" );
	shader->u_AreaTex = pglGetUniformLocationARB( shader->handle, "u_AreaTex" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_SearchTex, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_AreaTex, GL_TEXTURE2 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitSMAANeighborBlendUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_BlendTexture = pglGetUniformLocationARB( shader->handle, "u_BlendTexture" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_BlendTexture, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBloomUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitTonemapUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_HDRExposure = pglGetUniformLocationARB( shader->handle, "u_HDRExposure" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_HDRExposure, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGenLuminanceUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGenExposureUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_TimeDelta = pglGetUniformLocationARB( shader->handle, "u_TimeDelta" );
	shader->u_MipLod = pglGetUniformLocationARB( shader->handle, "u_MipLod" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBlurMipUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );
	shader->u_MipLod = pglGetUniformLocationARB( shader->handle, "u_MipLod" );
	shader->u_TexCoordClamp = pglGetUniformLocationARB( shader->handle, "u_TexCoordClamp" );
	shader->u_BloomFirstPass = pglGetUniformLocationARB( shader->handle, "u_BloomFirstPass" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitScreenWaterUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_ScreenWater = pglGetUniformLocationARB( shader->handle, "u_ScreenWater" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitGlitchUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_Glitch = pglGetUniformLocationARB( shader->handle, "u_Glitch" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitDroneScreenUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_DroneScreen = pglGetUniformLocationARB( shader->handle, "u_DroneScreen" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitWaterDropsUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_WaterDrops = pglGetUniformLocationARB( shader->handle, "u_WaterDrops" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

const char *GL_PretifyListOptions( const char *options, bool newlines )
{
	static char output[MAX_OPTIONS_LENGTH];
	const char *pstart = options;
	const char *pend = options + Q_strlen( options );
	char *pout = output;

	*pout = '\0';

	while( pstart < pend )
	{
		const char *pfind = Q_strstr( pstart, "#define" );
		if( !pfind ) break;

		pstart = pfind + Q_strlen( "#define" );

		for( ; *pstart != '\n'; pstart++, pout++ )
			*pout = *pstart;
		if( newlines )
			*pout++ = *pstart++;
		else pstart++; // skip '\n'
	}

	if( pout == output )
		return ""; // nothing found

	*pout++ = ' ';
	*pout = '\0';

	return output;
}

word GL_UberShaderForSolidBmodel( msurface_t *s, bool translucent )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	mfaceinfo_t *landscape = NULL;
	mextrasurf_t *es = s->info;
	bool fullBright = false;
	bool mirror = false;
	bool portal = false;
	bool IsLandscape = FBitSet( s->flags, SURF_LANDSCAPE );
	bool movie = FBitSet( s->flags, SURF_MOVIE );

	bool use_cubemaps = false;
	bool use_emboss = false;
	bool use_interior = false;
	bool use_specular = false;
	bool use_bump = false;

	ASSERT( worldmodel != NULL );

	if( FBitSet( s->flags, SURF_REFLECT ) && s->info->subtexture[glState.stack_position] > 0 )
		mirror = true;
	if( FBitSet( s->flags, SURF_PORTAL ) )
		portal = true;

	landscape = s->texinfo->faceinfo;

	if( es->shaderNum[mirror] && es->glsl_sequence[mirror] == tr.glsl_valid_sequence )
		return es->shaderNum[mirror]; // valid

	Q_strncpy( glname, "BmodelSolid", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	texture_t *tx = s->texinfo->texture;

	if( FBitSet( s->flags, SURF_REFLECT ) && !FBitSet( s->flags, SURF_WATER ) && CVAR_TO_BOOL( r_mirrorquality ))
		fullBright = true;

	if( portal && CVAR_TO_BOOL( gl_allow_portals ) )
	{
		fullBright = true;
		GL_AddShaderDirective( options, "BMODEL_PORTAL" );
	}

	if( FBitSet( s->flags, SURF_SCREEN ) && CVAR_TO_BOOL( gl_allow_screens ))
	{
		if( RI->currententity && FBitSet( RI->currententity->curstate.iuser1, CF_MONOCHROME ))
			GL_AddShaderDirective( options, "BMODEL_MONOCHROME" );
		fullBright = true;
	}

	// solid water with lightmaps looks ugly
	// diffusion - allow water to use lightmap
	// P.S. NOTENOTE - translucent is edited and completely excludes all RenderModeTexture brushes, so all glass/water uses lightmaps (not a bad thing tho)
	if( /*FBitSet( s->flags, SURF_WATER ) || */translucent )
		fullBright = true;

	if( tr.materials[tx->gl_texturenum].fullbright )
		fullBright = true;

	if( RI->currententity ) // diffusion
	{
		if( (RI->currententity->curstate.renderfx == kRenderFxFullbright) || (RI->currententity->curstate.renderfx == kRenderFxFullbrightNoShadows) )
			fullBright = true;

		if( RI->currententity->curstate.rendermode == kRenderTransTexture )
			GL_AddShaderDirective( options, "BMODEL_DEFAULTALPHATEST" ); // do not use 0.5 threshold
	}

	if( FBitSet( s->flags, SURF_FULLBRIGHT ) || R_FullBright( ) || fullBright )
	{
		GL_AddShaderDirective( options, "BMODEL_FULLBRIGHT" );
	}
	else
	{
		// process lightstyles
		for( int i = 0; i < MAXLIGHTMAPS && s->styles[i] != LS_NONE; i++ )
			GL_AddShaderDirective( options, va( "BMODEL_APPLY_STYLE%i", i ));
	}

	if( RI->currententity && RI->currententity->curstate.rendermode == kRenderTransAlpha && !IsLandscape )
		GL_AddShaderDirective( options, "ALPHA_RESCALING" );

	if( FBitSet( s->flags, SURF_WATER ))
	{
		GL_AddShaderDirective( options, "BMODEL_WATER" );

		if( RI->currententity )
		{
			if( !tr.lowmemory && (gl_water_refraction->value > 0) && (RI->currententity->curstate.renderfx != kRenderFxNoRefraction) )
			{
				if( tr.waterTextures.Count() > 0 )
				{
					GL_AddShaderDirective( options, "BMODEL_WATER_REFRACTION" );
					GL_EncodeNormal( options, tr.waterTextures[0] );
				}
			}
		}
	}

	if( !IsLandscape )
	{
		if( gl_emboss->value > 0 )
		{
			if( tr.materials[tx->gl_texturenum].EmbossScale > 0.0f )
			{
				GL_AddShaderDirective( options, "BMODEL_EMBOSS" );
				use_emboss = true;
			}
		}

		if( gl_specular->value > 0 )
		{
			if( tr.materials[tx->gl_texturenum].GlossScale > 0.0f )
			{
				GL_AddShaderDirective( options, "BMODEL_SPECULAR" );
				use_specular = true;
			}
		}

		if( gl_bump->value > 0 )
		{
			if( tr.materials[tx->gl_texturenum].gl_normalmap_id > 0 )
			{
				GL_AddShaderDirective( options, "BMODEL_BUMP" );
				GL_EncodeNormal( options, tr.materials[tx->gl_texturenum].gl_normalmap_id );
				use_bump = true;
			}
		}
	}
	else // landscape
	{
		if( gl_emboss->value > 0 )
		{
			if( landscape && landscape->terrain && landscape->terrain->layermap.has_emboss )
			{
				GL_AddShaderDirective( options, "BMODEL_EMBOSS" );
				use_emboss = true;
			}
		}

		if( gl_specular->value > 0 )
		{
			if( landscape && landscape->terrain && landscape->terrain->layermap.has_specular )
			{
				GL_AddShaderDirective( options, "BMODEL_SPECULAR" );
				use_specular = true;
			}
		}

		if( gl_bump->value > 0 )
		{
			if( landscape && landscape->terrain && landscape->terrain->layermap.gl_normalmap_id > 0 )
			{
				GL_AddShaderDirective( options, "BMODEL_BUMP" );
				GL_EncodeNormal( options, landscape->terrain->layermap.gl_normalmap_id );
				use_bump = true;
			}
		}
	}

	// diffusioncubemaps	
	if( CVAR_TO_BOOL( gl_cubemaps ) && world->cubemaps_ready && (tr.materials[tx->gl_texturenum].ReflectScale[0] > 0.01f) )
	{
		GL_AddShaderDirective( options, "REFLECTION_CUBEMAP" );
		use_cubemaps = true;
	}

	if( tr.materials[tx->gl_texturenum].gl_interiormap_id > 0 )
	{
		GL_AddShaderDirective( options, "BMODEL_INTERIOR" );
		use_interior = true;
	}

#if 0
	// can't properly draw for beams particles through glass. g-cont
	// disabled for now
	if( translucent )
		GL_AddShaderDirective( options, "BMODEL_TRANSLUCENT" );
#endif
	if( IsLandscape )
	{
		if( landscape && landscape->terrain && landscape->terrain->layermap.gl_diffuse_id != 0 )
		{
			GL_AddShaderDirective( options, va( "TERRAIN_NUM_LAYERS %i", landscape->terrain->numLayers ));
			GL_AddShaderDirective( options, "BMODEL_MULTI_LAYERS" );
		}
	}

	if( FBitSet( s->flags, SURF_REFLECT ) && s->info->subtexture[glState.stack_position] > 0 )
	{
		GL_AddShaderDirective( options, "BMODEL_REFLECTION_PLANAR" );
		
		if( FBitSet( s->flags, SURF_WATER ) && gl_water_planar->value > 0 )
			GL_AddShaderDirective( options, "BMODEL_WATER_PLANAR" );
	}

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitSolidBmodelUniforms );

	if( !shader )
	{
		tr.fClearScreen = true; // to avoid ugly blur
		SetBits( s->flags, SURF_NODRAW );
		return 0; // something bad happens
	}

	word shaderNum = (shader - glsl_programs);

	if( use_bump )
		shader->status |= SHADER_USE_BUMP;
	if( use_emboss )
		shader->status |= SHADER_USE_EMBOSS;
	if( use_interior )
		shader->status |= SHADER_USE_INTERIOR;
	if( use_specular )
		shader->status |= SHADER_USE_SPECULAR;
	if( use_cubemaps )
		shader->status |= SHADER_USE_CUBEMAPS;

	es->glsl_sequence[mirror] = tr.glsl_valid_sequence;
	ClearBits( s->flags, SURF_NODRAW );
	es->shaderNum[mirror] = shaderNum;
	
	return shaderNum;
}

word GL_UberShaderForBmodelDlight( const plight_t *pl, msurface_t *s, bool translucent )
{
	bool shadows = (!FBitSet( pl->flags, CF_NOSHADOWS )) ? true : false;

	if( tr.shadows_notsupport || !CVAR_TO_BOOL( gl_shadows ) )
		shadows = false;

	if( pl->pointlight && tr.omni_shadows_notsupport )
		shadows = false;

	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	mextrasurf_t *es = s->info;
	bool mirrorSurface = false;
	bool portalSurface = false;
	mfaceinfo_t *landscape = NULL;
	bool IsLandscape = FBitSet( s->flags, SURF_LANDSCAPE );

	bool use_emboss = false;
	bool use_interior = false;
	bool use_specular = false;
	bool use_bump = false;
	
	Q_strncpy( glname, "BmodelDlight", sizeof( glname ));

	memset( options, 0, sizeof( options ));

	landscape = s->texinfo->faceinfo;

	if( pl->pointlight )
	{
		if( es->omniLightShaderNum[shadows] && es->glsl_sequence_omni[shadows] == tr.glsl_valid_sequence )
			return es->omniLightShaderNum[shadows]; // valid

		GL_AddShaderDirective( options, "BMODEL_LIGHT_OMNIDIRECTIONAL" );
	}
	else
	{
		if( es->projLightShaderNum[shadows] && es->glsl_sequence_proj[shadows] == tr.glsl_valid_sequence )
			return es->projLightShaderNum[shadows]; // valid
		GL_AddShaderDirective( options, "BMODEL_LIGHT_PROJECTION" );
	}

	texture_t *tx = s->texinfo->texture;

	if( !IsLandscape )
	{
		if( gl_emboss->value > 0 )
		{
			if( tr.materials[tx->gl_texturenum].EmbossScale > 0.0f )
			{
				GL_AddShaderDirective( options, "BMODEL_EMBOSS" );
				use_emboss = true;
			}
		}

		if( gl_specular->value > 0 )
		{
			if( tr.materials[tx->gl_texturenum].GlossScale > 0.0f )
			{
				GL_AddShaderDirective( options, "BMODEL_SPECULAR" );
				use_specular = true;
			}
		}

		if( gl_bump->value > 0 )
		{
			if( tr.materials[tx->gl_texturenum].gl_normalmap_id > 0 )
			{
				GL_AddShaderDirective( options, "BMODEL_BUMP" );
				GL_EncodeNormal( options, tr.materials[tx->gl_texturenum].gl_normalmap_id );
				use_bump = true;
			}
		}
	}
	else // landscape
	{
		if( landscape && landscape->terrain && landscape->terrain->layermap.gl_diffuse_id != 0 )
		{
			GL_AddShaderDirective( options, va( "TERRAIN_NUM_LAYERS %i", landscape->terrain->numLayers ) );
			GL_AddShaderDirective( options, "BMODEL_MULTI_LAYERS" );
		}

		if( gl_emboss->value > 0 )
		{
			if( landscape && landscape->terrain && landscape->terrain->layermap.has_emboss )
			{
				GL_AddShaderDirective( options, "BMODEL_EMBOSS" );
				use_emboss = true;
			}
		}

		if( gl_specular->value > 0 )
		{
			if( landscape && landscape->terrain && landscape->terrain->layermap.has_specular )
			{
				GL_AddShaderDirective( options, "BMODEL_SPECULAR" );
				use_specular = true;
			}
		}

		if( gl_bump->value > 0 )
		{
			if( landscape && landscape->terrain && landscape->terrain->layermap.gl_normalmap_id > 0 )
			{
				GL_AddShaderDirective( options, "BMODEL_BUMP" );
				GL_EncodeNormal( options, landscape->terrain->layermap.gl_normalmap_id );
				use_bump = true;
			}
		}
	}

#if 0
	// can't properly draw for beams particles through glass. g-cont
	// disabled for now
	if( translucent )
		GL_AddShaderDirective( options, "BMODEL_TRANSLUCENT" );
#endif

	// diffusion - don't light up glass too much
	if( RI->currententity && RI->currententity->curstate.rendermode == kRenderTransTexture )
		GL_AddShaderDirective( options, "BMODEL_KRENDERTRANSTEXTURE" );

	if( FBitSet( s->flags, SURF_WATER ))
	{
		GL_AddShaderDirective( options, "BMODEL_WATER" );

		if( !tr.lowmemory && RI->currententity && (gl_water_refraction->value > 0) && (RI->currententity->curstate.renderfx != kRenderFxNoRefraction) )
		{
			if( tr.waterTextures.Count() > 0 )
			{
				GL_AddShaderDirective( options, "BMODEL_WATER_REFRACTION" );
				GL_EncodeNormal( options, tr.waterTextures[0] );
			}
		}
	}

	if( tr.materials[tx->gl_texturenum].gl_interiormap_id > 0 )
	{
		GL_AddShaderDirective( options, "BMODEL_INTERIOR" );
		use_interior = true;
	}

	if( shadows )
		GL_AddShaderDirective( options, "BMODEL_HAS_SHADOWS" );

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitBmodelDlightUniforms );
	if( !shader )
	{
		SetBits( s->flags, SURF_NODLIGHT );
		return 0; // something bad happens
	}

	word shaderNum = (shader - glsl_programs);

	// done
	ClearBits( s->flags, SURF_NODLIGHT );

	if( pl->pointlight )
	{
		es->omniLightShaderNum[shadows] = shaderNum;
		es->glsl_sequence_omni[shadows] = tr.glsl_valid_sequence;
	}
	else
	{
		es->projLightShaderNum[shadows] = shaderNum;
		es->glsl_sequence_proj[shadows] = tr.glsl_valid_sequence;
	}

	if( use_bump )
		shader->status |= SHADER_USE_BUMP;
	if( use_emboss )
		shader->status |= SHADER_USE_EMBOSS;
	if( use_interior )
		shader->status |= SHADER_USE_INTERIOR;
	if( use_specular )
		shader->status |= SHADER_USE_SPECULAR;

	return shaderNum;
}

word GL_UberShaderForGrassSolid( msurface_t *s, grass_t *g )
{
	if( g->vbo.shaderNum && g->vbo.glsl_sequence == tr.glsl_valid_sequence )
		return g->vbo.shaderNum; // valid

	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	Q_strncpy( glname, "GrassSolid", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( R_FullBright( ))
	{
		GL_AddShaderDirective( options, "GRASS_FULLBRIGHT" );
	}
	else
	{
		// process lightstyles
		for( int i = 0; i < MAXLIGHTMAPS && s->styles[i] != LS_NONE; i++ )
			GL_AddShaderDirective( options, va( "GRASS_APPLY_STYLE%i", i ));
	}

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitGrassSolidUniforms );
	if( !shader )
	{
		SetBits( g->vbo.flags, FGRASS_NODRAW );
		return 0; // something bad happens
	}

	g->vbo.glsl_sequence = tr.glsl_valid_sequence;
	ClearBits( g->vbo.flags, FGRASS_NODRAW );

	return (shader - glsl_programs);
}

word GL_UberShaderForGrassDlight( plight_t *pl, struct grass_s *g )
{
	bool shadows = (!FBitSet( pl->flags, CF_NOSHADOWS )) ? true : false;

	if( tr.shadows_notsupport || !CVAR_TO_BOOL( gl_shadows ) )
		shadows = false;

	if( pl->pointlight && tr.omni_shadows_notsupport )
		shadows = false;

	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	Q_strncpy( glname, "GrassDlight", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( pl->pointlight )
		GL_AddShaderDirective( options, "GRASS_LIGHT_OMNIDIRECTIONAL" );
	else
		GL_AddShaderDirective( options, "GRASS_LIGHT_PROJECTION" );

	if( shadows )
		GL_AddShaderDirective( options, "GRASS_HAS_SHADOWS" );

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitGrassDlightUniforms );
	if( !shader )
	{
		SetBits( g->vbo.flags, FGRASS_NODLIGHT );
		return 0; // something bad happens
	}

	ClearBits( g->vbo.flags, FGRASS_NODLIGHT );

	return (shader - glsl_programs);
}

word GL_UberShaderForBmodelDecal( decal_t *decal )
{
	char glname[64];
	bool fullbright = R_ModelOpaque( RI->currententity->curstate.rendermode, RI->currententity->curstate.renderamt ) ? false : true;
	char options[MAX_OPTIONS_LENGTH];
	msurface_t *s = decal->psurface;
	mextrasurf_t *es = s->info;

	ASSERT( worldmodel != NULL );

	if( decal->shaderNum && decal->glsl_sequence == tr.glsl_valid_sequence )
		return decal->shaderNum; // valid

	if( r_decals_ext->value > 0 )
	{
		// NOTE: this code executes when decal is placed.
		// this must be remade, so external textures would only load at startup (but how?)
		char diffuse[128];
		Q_snprintf( diffuse, sizeof( diffuse ), "textures/!decals/%s.dds", GET_TEXTURE_NAME( decal->texture ) );
		if( FILE_EXISTS( diffuse ) )
		{
			decal->texture = LOAD_TEXTURE( diffuse, NULL, 0, 0 );
		}
	}

	Q_strncpy( glname, "BmodelDecal", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	// process lightstyles
	for( int i = 0; i < MAXLIGHTMAPS && s->styles[i] != LS_NONE && !fullbright; i++ )
	{
		GL_AddShaderDirective( options, va( "DECAL_APPLY_STYLE%i", i ));
	}

	if( FBitSet( s->flags, SURF_TRANSPARENT ))
		GL_AddShaderDirective( options, "DECAL_ALPHATEST" );

	if( fullbright )
		GL_AddShaderDirective( options, "DECAL_FULLBRIGHT" );

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitBmodelDecalUniforms );
	if( !shader ) return 0; // something bad happens

	word shaderNum = (shader - glsl_programs);
	decal->glsl_sequence = tr.glsl_valid_sequence;
	decal->shaderNum = shaderNum;

	return shaderNum;
}

word GL_UberShaderForSolidStudio( mstudiomaterial_t *mat, bool vertex_lighting, bool bone_weighting, bool fullbright, int numbones )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	bool use_cubemaps = false;
	bool use_emboss = false;
	bool use_interior = false;
	bool use_specular = false;
	bool use_bump = false;

	if( mat->shaderNum && mat->glsl_sequence == tr.glsl_valid_sequence )
		return mat->shaderNum; // valid

	Q_strncpy( glname, "StudioSolid", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	const bool bMasked = FBitSet( mat->flags, STUDIO_NF_MASKED ) ? true : false;

	if( tr.materials[mat->gl_diffuse_id].fullbright ) // .monitor automatically sets this too
		fullbright = true;

	if( RI->currententity && RI->currententity->curstate.rendermode == kRenderTransTexture )
		GL_AddShaderDirective( options, "STUDIO_DEFAULTALPHATEST" ); // do not use 0.5 threshold

	if( numbones > 0 && glConfig.max_skinning_bones < MAXSTUDIOBONES && glConfig.uniforms_economy )
	{
		int num_bones = Q_min( numbones, glConfig.max_skinning_bones );
		GL_AddShaderDefine( options, va( "#define MAXSTUDIOBONES %d\n", num_bones ));
	}
	else if( numbones == 1 )
	{
		GL_AddShaderDefine( options, "#define MAXSTUDIOBONES 1\n" );
	}

	if( bone_weighting )
		GL_AddShaderDirective( options, "STUDIO_BONEWEIGHTING" );

	if( FBitSet( mat->flags, STUDIO_NF_CHROME ))
		GL_AddShaderDirective( options, "STUDIO_HAS_CHROME" );

	if( vertex_lighting )
		GL_AddShaderDirective( options, "STUDIO_VERTEX_LIGHTING" );

	if( fullbright || FBitSet( mat->flags, STUDIO_NF_FULLBRIGHT ) )
	{
		GL_AddShaderDirective( options, "STUDIO_FULLBRIGHT" );
	}

	if( FBitSet( mat->flags, STUDIO_NF_ADDITIVE ) )
	{
		GL_AddShaderDirective( options, "STUDIO_ADDITIVE" );
	}
	else
	{
		if( FBitSet( mat->flags, STUDIO_NF_FLATSHADE ))
			GL_AddShaderDirective( options, "STUDIO_LIGHT_FLATSHADE" );
	}

	if( gl_emboss->value > 0 )
	{
		if( tr.materials[mat->gl_diffuse_id].EmbossScale > 0.0f )
		{
			GL_AddShaderDirective( options, "STUDIO_EMBOSS" );
			use_emboss = true;
		}
	}

	if( gl_specular->value > 0 )
	{
		if( tr.materials[mat->gl_diffuse_id].GlossScale > 0.0f )
		{
			GL_AddShaderDirective( options, "STUDIO_SPECULAR" );
			use_specular = true;
		}
	}

	if( CVAR_TO_BOOL( gl_bump ) && tr.materials[mat->gl_diffuse_id].gl_normalmap_id > 0 )
	{
		GL_AddShaderDirective( options, "STUDIO_BUMP" );
		GL_EncodeNormal( options, tr.materials[mat->gl_diffuse_id].gl_normalmap_id );
		use_bump = true;
	}

	if( bMasked )
		GL_AddShaderDirective( options, "ALPHA_RESCALING" );

	// diffusioncubemaps		
	if( CVAR_TO_BOOL( gl_cubemaps ) && world->cubemaps_ready && (tr.materials[mat->gl_diffuse_id].ReflectScale[0] > 0.01f) )
	{
		GL_AddShaderDirective( options, "REFLECTION_CUBEMAP" );
		use_cubemaps = true;
	}

	if( tr.materials[mat->gl_diffuse_id].gl_interiormap_id > 0 )
	{
		GL_AddShaderDirective( options, "STUDIO_INTERIOR" );
		use_interior = true;
	}
	else if( tr.materials[mat->gl_diffuse_id].gl_blendtex_id > 0 )
		GL_AddShaderDirective( options, "STUDIO_TEXTURE_BLEND" );

	if( tr.materials[mat->gl_diffuse_id].gl_colormask_id > 0 )
		GL_AddShaderDirective( options, "STUDIO_HAS_COLORMASK" );

	if( tr.materials[mat->gl_diffuse_id].FoliageSwayHeight != 0 )
		GL_AddShaderDirective( options, "STUDIO_SWAY_FOLIAGE" );

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitSolidStudioUniforms );
	if( !shader )
	{
		SetBits( mat->flags, STUDIO_NF_NODRAW );
		return 0; // something bad happens
	}

	word shaderNum = (shader - glsl_programs);

	// done
	mat->glsl_sequence = tr.glsl_valid_sequence;
	ClearBits( mat->flags, STUDIO_NF_NODRAW );
	mat->shaderNum = shaderNum;

	if( use_bump )
		shader->status |= SHADER_USE_BUMP;
	if( use_emboss )
		shader->status |= SHADER_USE_EMBOSS;
	if( use_interior )
		shader->status |= SHADER_USE_INTERIOR;
	if( use_specular )
		shader->status |= SHADER_USE_SPECULAR;
	if( use_cubemaps )
		shader->status |= SHADER_USE_CUBEMAPS;

	return shaderNum;
}

word GL_UberShaderForDlightStudio( const plight_t *pl, struct mstudiomat_s *mat, bool bone_weighting, int numbones )
{
	bool shadows = (!FBitSet( pl->flags, CF_NOSHADOWS )) ? true : false;

	if( tr.shadows_notsupport || !CVAR_TO_BOOL( gl_shadows ) )
		shadows = false;

	if( pl->pointlight && tr.omni_shadows_notsupport )
		shadows = false;

	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	bool use_emboss = false;
	bool use_interior = false;
	bool use_specular = false;
	bool use_bump = false;

	Q_strncpy( glname, "StudioDlight", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( pl->pointlight )
	{
		if( mat->omniLightShaderNum[shadows] && mat->glsl_sequence_omni[shadows] == tr.glsl_valid_sequence )
			return mat->omniLightShaderNum[shadows]; // valid
		GL_AddShaderDirective( options, "STUDIO_LIGHT_OMNIDIRECTIONAL" );
	}
	else
	{
		if( mat->projLightShaderNum[shadows] && mat->glsl_sequence_proj[shadows] == tr.glsl_valid_sequence )
			return mat->projLightShaderNum[shadows]; // valid
		GL_AddShaderDirective( options, "STUDIO_LIGHT_PROJECTION" );
	}

	if( numbones > 0 && glConfig.max_skinning_bones < MAXSTUDIOBONES && glConfig.uniforms_economy )
	{
		int num_bones = Q_min( numbones, glConfig.max_skinning_bones );
		GL_AddShaderDefine( options, va( "#define MAXSTUDIOBONES %d\n", num_bones ));
	}
	else if( numbones == 1 )
	{
		GL_AddShaderDefine( options, "#define MAXSTUDIOBONES 1\n" );
	}

	if( bone_weighting )
		GL_AddShaderDirective( options, "STUDIO_BONEWEIGHTING" );

	if( FBitSet( mat->flags, STUDIO_NF_CHROME ))
		GL_AddShaderDirective( options, "STUDIO_HAS_CHROME" );

	if( gl_emboss->value > 0 )
	{
		if( tr.materials[mat->gl_diffuse_id].EmbossScale > 0.0f )
		{
			GL_AddShaderDirective( options, "STUDIO_EMBOSS" );
			use_emboss = true;
		}
	}

	if( gl_specular->value > 0 )
	{
		if( tr.materials[mat->gl_diffuse_id].GlossScale > 0.0f )
		{
			GL_AddShaderDirective( options, "STUDIO_SPECULAR" );
			use_specular = true;
		}
	}

	if( CVAR_TO_BOOL( gl_bump ) && tr.materials[mat->gl_diffuse_id].gl_normalmap_id > 0 )
	{
		GL_AddShaderDirective( options, "STUDIO_BUMP" );
		GL_EncodeNormal( options, tr.materials[mat->gl_diffuse_id].gl_normalmap_id );
		use_bump = true;
	}

	if( tr.materials[mat->gl_diffuse_id].gl_interiormap_id > 0 )
	{
		GL_AddShaderDirective( options, "STUDIO_INTERIOR" );
		use_interior = true;
	}
	else if( tr.materials[mat->gl_diffuse_id].gl_blendtex_id > 0 )
		GL_AddShaderDirective( options, "STUDIO_TEXTURE_BLEND" );

	if( tr.materials[mat->gl_diffuse_id].gl_colormask_id > 0 )
		GL_AddShaderDirective( options, "STUDIO_HAS_COLORMASK" );

	if( tr.materials[mat->gl_diffuse_id].FoliageSwayHeight != 0 )
		GL_AddShaderDirective( options, "STUDIO_SWAY_FOLIAGE" );

	if( shadows )
		GL_AddShaderDirective( options, "STUDIO_HAS_SHADOWS" );

	if( RI->currententity && RI->currententity->curstate.rendermode == kRenderTransTexture )
		GL_AddShaderDirective( options, "STUDIO_DEFAULTALPHATEST" ); // do not use 0.5 threshold

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitStudioDlightUniforms );
	if( !shader )
	{
		SetBits( mat->flags, STUDIO_NF_NODLIGHT );
		return 0; // something bad happens
	}

	word shaderNum = (shader - glsl_programs);

	// done
	ClearBits( mat->flags, STUDIO_NF_NODLIGHT );

	if( pl->pointlight )
	{
		mat->omniLightShaderNum[shadows] = shaderNum;
		mat->glsl_sequence_omni[shadows] = tr.glsl_valid_sequence;
	}
	else
	{
		mat->projLightShaderNum[shadows] = shaderNum;
		mat->glsl_sequence_proj[shadows] = tr.glsl_valid_sequence;
	}

	if( use_bump )
		shader->status |= SHADER_USE_BUMP;
	if( use_emboss )
		shader->status |= SHADER_USE_EMBOSS;
	if( use_interior )
		shader->status |= SHADER_USE_INTERIOR;
	if( use_specular )
		shader->status |= SHADER_USE_SPECULAR;

	return shaderNum;
}

word GL_UberShaderForStudioDecal( mstudiomaterial_t *mat )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	Q_strncpy( glname, "StudioDecal", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( FBitSet( mat->flags, STUDIO_NF_MASKED ))
		GL_AddShaderDirective( options, "DECAL_ALPHATEST" );

	if( FBitSet( mat->flags, STUDIO_NF_ADDITIVE ))
	{
		// mixed mode: solid & transparent controlled by alpha-channel
		if( FBitSet( mat->flags, STUDIO_NF_MASKED ))
			GL_AddShaderDirective( options, "STUDIO_ALPHA_GLASS" );
	}

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitStudioDecalUniforms );
	if( !shader ) return 0; // something bad happens

	return (shader - glsl_programs);
}

word GL_UberShaderForDlightGeneric( const plight_t *pl )
{
	bool shadows = (!FBitSet( pl->flags, CF_NOSHADOWS )) ? true : false;

	if( tr.shadows_notsupport || !CVAR_TO_BOOL( gl_shadows ) )
		shadows = false;

	if( pl->pointlight && tr.omni_shadows_notsupport )
		shadows = false;

	if( pl->pointlight )
	{
		if( tr.omniLightShaderNum[shadows] && tr.glsl_sequence_omni[shadows] == tr.glsl_valid_sequence )
			return tr.omniLightShaderNum[shadows]; // valid
	}
	else
	{
		if( tr.projLightShaderNum && tr.glsl_sequence_proj[shadows] == tr.glsl_valid_sequence )
			return tr.projLightShaderNum[shadows]; // valid
	}

	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	Q_strncpy( glname, "GenericDlight", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( pl->pointlight )
		GL_AddShaderDirective( options, "GENERIC_LIGHT_OMNIDIRECTIONAL" );
	else
		GL_AddShaderDirective( options, "GENERIC_LIGHT_PROJECTION" );

	if( shadows )
		GL_AddShaderDirective( options, "GENERIC_HAS_SHADOWS" );

	glsl_program_t *shader = GL_FindUberShader( glname, options, &GL_InitGenericDlightUniforms );
	if( !shader )
	{
		tr.nodlights = true;
		return 0; // something bad happens
	}

	word shaderNum = (shader - glsl_programs);

	// done
	tr.nodlights = false;

	if( pl->pointlight )
	{
		tr.omniLightShaderNum[shadows] = shaderNum;
		tr.glsl_sequence_omni[shadows] = tr.glsl_valid_sequence;
	}
	else
	{
		tr.projLightShaderNum[shadows] = shaderNum;
		tr.glsl_sequence_proj[shadows] = tr.glsl_valid_sequence;
	}

	return shaderNum;
}

void GL_ListGPUShaders( void )
{
	int count = 0;

	for( unsigned int i = 1; i < num_glsl_programs; i++ )
	{
		glsl_program_t *cur = &glsl_programs[i];
		if( !cur->name[0] ) continue;

		const char *options = GL_PretifyListOptions( cur->options );

		if( Q_stricmp( options, "" ))
			Msg( "#%i %s [%s]\n", i, cur->name, options );
		else Msg( "#%i %s\n", i, cur->name );
		count++;
	}

	Msg( "total %i shaders\n", count );
}

bool GL_PrecompileUberShaders( int &processed, int &total )
{
	if( bPrecompiled )
		return true;

	processed = shaders_processed_total;
	total = iTotalShaders;

	if( shaders_processed_total == iTotalShaders - 1 )
	{
		if( compiled_failed_total == 0 && compiled_outdated_total > 0 )
			Msg( "^2Loaded %d shaders:^7 ^3%d were recompiled.^7\n", compiled_count_total, compiled_outdated_total );
		else if( compiled_failed_total > 0 && compiled_outdated_total == 0 )
			Msg( "^2Loaded %d shaders:^7 ^1%d failed to preload.^7\n", compiled_count_total, compiled_failed_total );
		else if( compiled_failed_total > 0 && compiled_outdated_total > 0 )
			Msg( "^2Loaded %d shaders:^7 ^3%d were recompiled,^7 ^1%d failed to load.^7\n", compiled_count_total, compiled_outdated_total, compiled_failed_total );
		else
			Msg( "^2Loaded %d shaders.^7\n", compiled_count_total );

		bPrecompiled = true;
		compiled_outdated_total = 0;
		return true;
	}

	if( !bCompilingShaders )
	{
		Msg( "^2Loading shaders...^7\n" );
		shaders_processed_total = 0;
		bCompilingShaders = true;
		compiled_count_total = 0;
		compiled_failed_total = 0;
	}

	bPrecompiled = false;

	shaderlist_t curshader = shaders[shaders_processed_total];

	int compiled_shader_counter = 0; // allow to load up to 10 shaders at once if they are loaded from disk cache or from hash
	float mult = 0.0167f / g_fFrametime;
	int compiled_shader_max = 20 * mult;
	compiled_shader_max = bound( 3, compiled_shader_max, 50 );

	glsl_program_t *shader;
	while( compiled_shader_counter < compiled_shader_max )
	{
		if( curshader.glname == NULL )
			break; // end of the list - finalize next frame

		shader = GL_InitGPUShader( curshader.glname, curshader.glname, curshader.glname, curshader.defines );

		if( !shader )
		{
			// invalid shader
			compiled_shader_counter++;
			compiled_failed_total++;
			// advance shader
			shaders_processed_total++;
			curshader = shaders[shaders_processed_total];
		}
		else if( shader->status & SHADER_STATUS_HASHED )
		{
			// this shader has already been precached during map loading - skip it
			compiled_shader_counter++;
			compiled_count_total++;
			// advance shader
			shaders_processed_total++;
			curshader = shaders[shaders_processed_total];
		}
		else if( shader->status & SHADER_STATUS_FROMBINARY )
		{
			// this shader was loaded from disk - apply uniforms and load some more
			if( !Q_stricmp( curshader.glname, "bmodeldecal" ) )
				GL_InitBmodelDecalUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "bmodeldlight" ) )
				GL_InitBmodelDlightUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "bmodelsolid" ) )
				GL_InitSolidBmodelUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "studiodlight" ) )
				GL_InitStudioDlightUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "studiosolid" ) )
				GL_InitSolidStudioUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "grassdlight" ) )
				GL_InitGrassDlightUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "grasssolid" ) )
				GL_InitGrassSolidUniforms( shader );
			else
				ALERT( at_aiconsole, "^1GL_PrecompileUberShaders: unhandled uniforms for shader %s!^7\n", curshader.glname );

			compiled_shader_counter++;
			compiled_count_total++;
			// advance shader
			shaders_processed_total++;
			curshader = shaders[shaders_processed_total];
		}
		else
		{
			// uncompiled shader
			if( !Q_stricmp( curshader.glname, "bmodeldecal" ) )
				GL_InitBmodelDecalUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "bmodeldlight" ) )
				GL_InitBmodelDlightUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "bmodelsolid" ) )
				GL_InitSolidBmodelUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "studiodlight" ) )
				GL_InitStudioDlightUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "studiosolid" ) )
				GL_InitSolidStudioUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "grassdlight" ) )
				GL_InitGrassDlightUniforms( shader );
			else if( !Q_stricmp( curshader.glname, "grasssolid" ) )
				GL_InitGrassSolidUniforms( shader );
			else
				ALERT( at_aiconsole, "^1GL_PrecompileUberShaders: unhandled uniforms for shader %s!^7\n", curshader.glname );

			compiled_shader_counter++;
		//	compiled_shader_max -= 7; // decrease workload
			compiled_count_total++;
			// advance shader
			shaders_processed_total++;
			curshader = shaders[shaders_processed_total];
		}
	}

//	Msg( "This frame loaded shaders: %i\n", compiled_shader_counter );
	return false;
}

void GL_InitGPUShaders( void )
{
	plight_t pl;

	memset( &glsl_programs, 0, sizeof( glsl_programs ));
	memset( &glsl, 0, sizeof( glsl ));
	memset( &pl, 0, sizeof( pl ));
	memset( glsl_programsHashTable, 0, sizeof( glsl_programsHashTable ) );
	tr.glsl_valid_sequence = 1;
	num_glsl_programs = 1; // entry #0 isn't used
	compiled_outdated_total = 0; // this adds up in GL_InitGPUShader

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	ADD_COMMAND( "shaderlist", GL_ListGPUShaders );

	glsl_program_t *shader; // generic pointer. help to initialize uniforms

	// prepare sunshafts
	glsl.genSunShafts = shader = GL_InitGPUShader( "GenSunShafts", "generic", "genshafts" );
	GL_InitGenShaftsUniforms( shader );

	// render sunshafts
	glsl.drawSunShafts = shader = GL_InitGPUShader( "DrawSunShafts", "generic", "drawshafts" );
	GL_InitDrawShaftsUniforms( shader );

	// prepare SSAO
	glsl.genSSAO = shader = GL_InitGPUShader( "GenSSAO", "generic", "genssao" );
	GL_InitGenSSAOUniforms( shader );

	// prepare HBAO
	glsl.genHBAO = shader = GL_InitGPUShader( "GenHBAO", "generic", "genhbao" );
	GL_InitGenHBAOUniforms( shader );

	// render SSAO
	glsl.drawSSAO = shader = GL_InitGPUShader( "DrawSSAO", "generic", "drawssao" );
	GL_InitDrawSSAOUniforms( shader );

	// motion blur
	glsl.MotionBlur = shader = GL_InitGPUShader( "MotionBlur", "generic", "motionblur" );
	GL_InitMotionBlurUniforms( shader );

	// horizontal blur
	glsl.HorizontalBlur = shader = GL_InitGPUShader( "HorizontalBlur", "generic", "horizontalblur" );
	GL_InitHorizontalBlurUniforms( shader );

	// monochrome
	glsl.Monochrome = shader = GL_InitGPUShader( "Monochrome", "generic", "monochrome" );
	GL_InitMonochromeUniforms( shader );

	// various screen effects - sharpen, saturation...
	glsl.Enhance = shader = GL_InitGPUShader( "Enhance", "generic", "enhance" );
	GL_InitEnhanceUniforms( shader );

	glsl.FSR_easu = shader = GL_InitGPUShader( "FSR_easu", "generic", "fsr_easu" );
	GL_InitFSRUniforms( shader, true );
	glsl.FSR_rcas = shader = GL_InitGPUShader( "FSR_rcas", "generic", "fsr_rcas" );
	GL_InitFSRUniforms( shader, false );

	glsl.SMAAEdgeDetect = shader = GL_InitGPUShader( "SMAAEdgeDetect", "smaaedgedetect", "smaaedgedetect" );
	GL_InitSMAAEdgeDetectUniforms( shader );
	glsl.SMAABlendWeight = shader = GL_InitGPUShader( "SMAABlendWeight", "smaablendweight", "smaablendweight" );
	GL_InitSMAABlendWeightUniforms( shader );
	glsl.SMAANeighborBlend = shader = GL_InitGPUShader( "SMAANeighborBlend", "smaaneighborblend", "smaaneighborblend" );
	GL_InitSMAANeighborBlendUniforms( shader );

	// screen water
	glsl.ScreenWater = shader = GL_InitGPUShader( "ScreenWater", "generic", "screenwater" );
	GL_InitScreenWaterUniforms( shader );

	// glitch effect
	glsl.Glitch = shader = GL_InitGPUShader( "Glitch", "generic", "glitch" );
	GL_InitGlitchUniforms( shader );

	// drone screen effect
	glsl.DroneScreen = shader = GL_InitGPUShader( "DroneScreen", "generic", "dronescreen" );
	GL_InitDroneScreenUniforms( shader );

	// heat distortion effect
	glsl.HeatDistortion = shader = GL_InitGPUShader( "HeatDistortion", "generic", "heat" );
	GL_InitHeatDistortionUniforms( shader );

	// lens flare
	glsl.LensFlare = shader = GL_InitGPUShader( "LensFlare", "generic", "lensflare" );
	GL_InitLensFlareUniforms( shader );

	// water drops on screen
	glsl.WaterDrops = shader = GL_InitGPUShader( "WaterDrops", "generic", "waterdrops" );
	GL_InitWaterDropsUniforms( shader );

	// tonemap
	glsl.ToneMap = shader = GL_InitGPUShader( "ToneMap", "generic", "tonemap" );
	GL_InitTonemapUniforms( shader );

	glsl.GenExposure = shader = GL_InitGPUShader( "GenExposure", "generic", "generate_exposure" );
	GL_InitGenExposureUniforms( shader );

	glsl.Bloom = shader = GL_InitGPUShader( "Bloom", "generic", "bloom" );
	GL_InitBloomUniforms( shader );

	glsl.BlurMip = shader = GL_InitGPUShader( "BlurMip", "generic", "blurmip" );
	GL_InitBlurMipUniforms( shader );

	glsl.GenLuminance = shader = GL_InitGPUShader( "GenLuminance", "generic", "generate_luminance" );
	GL_InitGenLuminanceUniforms( shader );

	// gaussian blur
	glsl.blurShader = shader = GL_InitGPUShader( "GaussBlur", "generic", "gaussblur" );
	GL_InitPostProcessUniforms( shader );

	glsl.BilateralBlur = shader = GL_InitGPUShader( "BilateralBlur", "generic", "bilateralblur" );
	GL_InitBilateralBlurUniforms( shader );

	// draw sun & skybox
	glsl.skyboxEnv = shader = GL_InitGPUShader( "SkyBoxGeneric", "generic", "skybox" );
	GL_InitSkyBoxUniforms( shader );

	// studio shadowing
	glsl.studioDepthFill[0] = shader = GL_InitGPUShader( "StudioDepth", "StudioDepth", "generic", "#define NO_FOG\n" );
	GL_InitStudioDepthFillUniforms( shader );

	glsl.studioDepthFill[1] = shader = GL_InitGPUShader( "StudioDepth", "StudioDepth", "generic", "#define STUDIO_BONEWEIGHTING\n#define NO_FOG\n"  );
	GL_InitStudioDepthFillUniforms( shader );

	// bmodel shadowing
	glsl.bmodelDepthFill = shader = GL_InitGPUShader( "BrushDepth", "BmodelDepth", "generic", "#define NO_FOG\n" );
	GL_InitBmodelDepthFillUniforms( shader );

	// grass shadowing
	glsl.grassDepthFill = shader = GL_InitGPUShader( "GrassDepth", "GrassDepth", "generic", "#define NO_FOG\n" );
	GL_InitGrassSolidUniforms( shader );

	// fog processing
	glsl.genericFog = shader = GL_InitGPUShader( "GenericFog", "generic", "generic" );
	GL_InitGenericFogUniforms( shader );
	glsl.genericFogUseAlpha = shader = GL_InitGPUShader( "GenericFogUseAlpha", "generic", "generic", "#define FOG_USE_ALPHA\n" );
	GL_InitGenericFogUniforms( shader );

	// HACKHACK: precache generic light shaders
	pl.flags = 0;
	GL_UberShaderForDlightGeneric( &pl );
	pl.flags |= CF_NOSHADOWS;
	GL_UberShaderForDlightGeneric( &pl );
	pl.pointlight = true;
	pl.flags = 0;
	GL_UberShaderForDlightGeneric( &pl );
	pl.flags |= CF_NOSHADOWS;
	GL_UberShaderForDlightGeneric( &pl );
}

void GL_FreeUberShaders( void )
{
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	for( unsigned int i = 1; i < num_glsl_programs; i++ )
	{
		if( FBitSet( glsl_programs[i].status, SHADER_UBERSHADER )) 
			GL_FreeGPUShader( &glsl_programs[i] );
	}

	memset( glsl_programsHashTable, 0, sizeof( glsl_programsHashTable ) );

	GL_BindShader( GL_NONE );
}

void GL_FreeGPUShaders( void )
{
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	for( unsigned int i = 1; i < num_glsl_programs; i++ )
		GL_FreeGPUShader( &glsl_programs[i] );

	memset( glsl_programsHashTable, 0, sizeof( glsl_programsHashTable ) );

	GL_BindShader( GL_NONE );
}