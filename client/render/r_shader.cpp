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

#define SHADERS_HASH_SIZE	(MAX_GLSL_PROGRAMS >> 2)
#define MAX_FILE_STACK	64

static char	filenames_stack[MAX_FILE_STACK][256];
glsl_program_t	glsl_programs[MAX_GLSL_PROGRAMS];
glsl_program_t	*glsl_programsHashTable[SHADERS_HASH_SIZE];
unsigned int	num_glsl_programs;
static int	file_stack_pos;
ref_shaders_t	glsl;

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
		if( !Q_strcmp( prog->name, glname ) && !Q_strcmp( prog->options, defines ))
			return prog;
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
	num_glsl_programs++;

	Q_strncpy( shader->name, glname, sizeof( shader->name ));
	Q_strncpy( shader->options, defines, sizeof( shader->options ));
	shader->handle = pglCreateProgramObjectARB();

	if( vpname ) GL_LoadGPUShader( shader, vpname, GL_VERTEX_SHADER_ARB, false, defines );
	else shader->status |= SHADER_VERTEX_COMPILED;
	if( fpname ) GL_LoadGPUShader( shader, fpname, GL_FRAGMENT_SHADER_ARB, false, defines );
	else shader->status |= SHADER_FRAGMENT_COMPILED;

	if( vpname && FBitSet( shader->status, SHADER_VERTEX_COMPILED ))
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

	if( shader != NULL )
	{
		// add to hash table
		shader->nextHash = glsl_programsHashTable[hash];
		glsl_programsHashTable[hash] = shader;
	}

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

static glsl_program_t *GL_CreateUberShader( GLuint slot, const char *glname, const char *options = NULL, pfnShaderCallback pfnInitUniforms = NULL )
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

	shader->handle = pglCreateProgramObjectARB();
	if( !shader->handle ) return NULL; // some bad happens

	Q_strncpy( shader->name, glname, sizeof( shader->name ));
	Q_strncpy( shader->options, options, sizeof( shader->options ));

	GL_LoadGPUShader( shader, glname, GL_VERTEX_SHADER_ARB, true, options );
	GL_LoadGPUShader( shader, glname, GL_FRAGMENT_SHADER_ARB, true, options );

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

	ALERT( at_aiconsole, "CompileUberShader #%i: %s\n%s\n", slot, glname, options );

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
		if( !Q_strcmp( prog->name, glname ) && !Q_strcmp( prog->options, options ))
			return prog;
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
	if( GL_FindShaderDirective( shader, "BMODEL_WATER" ) && GL_FindShaderDirective( shader, "BMODEL_WATER_REFRACTION" ) )
	{
		shader->u_ScreenSizeInv = pglGetUniformLocationARB( shader->handle, "u_ScreenSizeInv" );
	//	shader->u_zFar = pglGetUniformLocationARB( shader->handle, "u_zFar" );
	}
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
	if( !GL_FindShaderDirective( shader, "BMODEL_WATER" ) )
		pglUniform1iARB( shader->u_DeluxeMap, GL_TEXTURE2 );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE3 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE4 );

	if( GL_FindShaderDirective( shader, "BMODEL_MULTI_LAYERS" ) )
		pglUniform1iARB( shader->u_LayerMap, GL_TEXTURE5 );
	else if( GL_FindShaderDirective( shader, "BMODEL_INTERIOR" ) )
		pglUniform1iARB( shader->u_InteriorMap, GL_TEXTURE5 );
	else if( GL_FindShaderDirective( shader, "BMODEL_WATER_PLANAR" ) )
		pglUniform1iARB( shader->u_WaterTex, GL_TEXTURE5 );

	if( GL_FindShaderDirective( shader, "BMODEL_WATER_REFRACTION" ) )
		pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE6 );

	if( GL_FindShaderDirective( shader, "REFLECTION_CUBEMAP" ) )  // diffusioncubemaps
	{
		if( GL_FindShaderDirective( shader, "BMODEL_WATER" ) )
		{
			pglUniform1iARB( shader->u_CubemapBox, GL_TEXTURE2 ); // water won't use deluxmap so I'll use it here
		}
		else
		{
			pglUniform1iARB( shader->u_CubemapBox, GL_TEXTURE6 ); // other brushes won't use depthmap so I'll use it here
		}
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
				GL_AddShaderDirective( options, "BMODEL_WATER_REFRACTION" );
				GL_EncodeNormal( options, tr.waterTextures[0] );
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
			GL_AddShaderDirective( options, "BMODEL_WATER_REFRACTION" );
			GL_EncodeNormal( options, tr.waterTextures[0] );
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

void GL_InitGPUShaders( void )
{
	plight_t pl;

	memset( &glsl_programs, 0, sizeof( glsl_programs ));
	memset( &glsl, 0, sizeof( glsl ));
	memset( &pl, 0, sizeof( pl ));
	tr.glsl_valid_sequence = 1;
	num_glsl_programs = 1; // entry #0 isn't used

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
	GL_UberShaderForDlightGeneric( &pl );
//	SetBits( pl.flags, CF_NOSHADOWS );
//	GL_UberShaderForDlightGeneric( &pl );
	pl.pointlight = true;
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

	GL_BindShader( GL_NONE );
}

void GL_FreeGPUShaders( void )
{
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	for( unsigned int i = 1; i < num_glsl_programs; i++ )
		GL_FreeGPUShader( &glsl_programs[i] );

	GL_BindShader( GL_NONE );
}