//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATAMAP_H
#define DATAMAP_H
#ifdef _WIN32
#pragma once
#endif

#include <string.h>		// strlen, strcat
#include <stdio.h>		// sprintf
#include <stddef.h>		// size_t, offsetof
#include <type_traits>	// compile-time field checks
#include "utlarray.h"
#include "ehandle.h"

// SINGLE_INHERITANCE restricts the size of CBaseEntity pointers-to-member-functions to 4 bytes
#ifdef _MSC_VER
class __single_inheritance CBaseEntity;
#else
class CBaseEntity;
#endif

//-----------------------------------------------------------------------------
// Field sizes... 
//-----------------------------------------------------------------------------
template <int FIELD_TYPE>
class CDatamapFieldSizeDeducer
{
public:
	enum
	{
		SIZE = 0,
		INPUT_SIZE = 1
	};

	static int FieldSize( )
	{
		return 0;
	}

	static int FieldInputSize()
	{
		return 0;
	}
};

#define DECLARE_FIELD_SIZE( _fieldType, _fieldSize, _fieldInputSize ) \
	template< > class CDatamapFieldSizeDeducer<_fieldType> { \
		public: \
		enum { SIZE = _fieldSize, INPUT_SIZE = _fieldInputSize }; \
		static int FieldSize() { return _fieldSize; } \
		static int FieldInputSize() { return _fieldInputSize; } \
	};

#define FIELD_SIZE( _fieldType )		CDatamapFieldSizeDeducer<_fieldType>::SIZE
#define FIELD_INPUT_SIZE( _fieldType )	CDatamapFieldSizeDeducer<_fieldType>::INPUT_SIZE

DECLARE_FIELD_SIZE( FIELD_FLOAT, sizeof( float ), sizeof( float ) )
DECLARE_FIELD_SIZE( FIELD_STRING, sizeof( int ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_ENTITY, sizeof( void * ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_CLASSPTR, sizeof( void * ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_EHANDLE, sizeof( EHANDLE ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_EVARS, sizeof( void * ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_EDICT, sizeof( void * ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_VECTOR, 3 * sizeof( float ), 3 * sizeof( float ) )
DECLARE_FIELD_SIZE( FIELD_POSITION_VECTOR, 3 * sizeof( float ), 3 * sizeof( float ) )
DECLARE_FIELD_SIZE( FIELD_POINTER, sizeof( void * ), sizeof( void * ) )
DECLARE_FIELD_SIZE( FIELD_INTEGER, sizeof( int ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_FUNCTION, sizeof( void * ), sizeof( void * ) )
DECLARE_FIELD_SIZE( FIELD_BOOLEAN, sizeof( bool ), sizeof( bool ) )
DECLARE_FIELD_SIZE( FIELD_SHORT, sizeof( short ), sizeof( short ) )
DECLARE_FIELD_SIZE( FIELD_CHARACTER, sizeof( char ), sizeof( char ) )
DECLARE_FIELD_SIZE( FIELD_TIME, sizeof( float ), sizeof( float ) )
DECLARE_FIELD_SIZE( FIELD_MODELNAME, sizeof( int ), sizeof( int ) )
DECLARE_FIELD_SIZE( FIELD_SOUNDNAME, sizeof( int ), sizeof( int ) )

//-----------------------------------------------------------------------------
// Compile-time validation of the DEFINE_* macros
//-----------------------------------------------------------------------------

// Per-FIELD-type rules for the element type E. Types without a rule are only size-checked.
template< int FT, typename E >
struct CDatamapKindCheck { enum { OK = 1 }; };

#define DATAMAP_KIND_RULE( _fieldType, _condition, _message ) \
	template< typename E > \
	struct CDatamapKindCheck< _fieldType, E > \
	{ \
		static_assert( _condition, _message ); \
		enum { OK = 1 }; \
	};

#define DM_PLAIN_INT( E )	( std::is_integral<E>::value && !std::is_same<E, bool>::value )
#define DM_STRING_T( E )	( DM_PLAIN_INT( E ) && sizeof( E ) == sizeof( int ))

DATAMAP_KIND_RULE( FIELD_FLOAT, (std::is_same<E, float>::value), "FIELD_FLOAT member must be a float" )
DATAMAP_KIND_RULE( FIELD_TIME, (std::is_same<E, float>::value), "FIELD_TIME member must be a float" )
DATAMAP_KIND_RULE( FIELD_INTEGER, ((DM_PLAIN_INT( E ) || std::is_enum<E>::value) && sizeof( E ) == sizeof( int )), "FIELD_INTEGER member must be a 4-byte int, BOOL or enum" )
DATAMAP_KIND_RULE( FIELD_BOOLEAN, (std::is_same<E, bool>::value), "FIELD_BOOLEAN member must be a bool (use FIELD_INTEGER for BOOL)" )
DATAMAP_KIND_RULE( FIELD_SHORT, (std::is_integral<E>::value && sizeof( E ) == sizeof( short )), "FIELD_SHORT member must be a 2-byte integer" )
DATAMAP_KIND_RULE( FIELD_CHARACTER, (std::is_integral<E>::value && sizeof( E ) == 1), "FIELD_CHARACTER member must be a 1-byte integer (char/byte)" )
DATAMAP_KIND_RULE( FIELD_STRING, DM_STRING_T( E ), "FIELD_STRING member must be a string_t" )
DATAMAP_KIND_RULE( FIELD_MODELNAME, DM_STRING_T( E ), "FIELD_MODELNAME member must be a string_t" )
DATAMAP_KIND_RULE( FIELD_SOUNDNAME, DM_STRING_T( E ), "FIELD_SOUNDNAME member must be a string_t" )
DATAMAP_KIND_RULE( FIELD_VECTOR, (sizeof( E ) == 3 * sizeof( float )), "FIELD_VECTOR member must be a Vector / vec3_t" )
DATAMAP_KIND_RULE( FIELD_POSITION_VECTOR, (sizeof( E ) == 3 * sizeof( float )), "FIELD_POSITION_VECTOR member must be a Vector / vec3_t" )
DATAMAP_KIND_RULE( FIELD_EHANDLE, (sizeof( E ) == sizeof( EHANDLE )), "FIELD_EHANDLE member must be an EHANDLE" )
DATAMAP_KIND_RULE( FIELD_CLASSPTR, (std::is_pointer<E>::value &&std::is_base_of< CBaseEntity, typename std::remove_pointer<E>::type >::value), "FIELD_CLASSPTR member must be a pointer to a CBaseEntity-derived class" )
DATAMAP_KIND_RULE( FIELD_EVARS, (std::is_same<E, entvars_t *>::value), "FIELD_EVARS member must be an entvars_t *" )
DATAMAP_KIND_RULE( FIELD_EDICT, (std::is_same<E, edict_t *>::value), "FIELD_EDICT member must be an edict_t *" )
DATAMAP_KIND_RULE( FIELD_ENTITY, (std::is_integral<E>::value && sizeof( E ) == sizeof( int )), "FIELD_ENTITY member must be an EOFFSET (int)" )
DATAMAP_KIND_RULE( FIELD_POINTER, (std::is_pointer<E>::value), "FIELD_POINTER member must be a pointer" )
DATAMAP_KIND_RULE( FIELD_FUNCTION, (std::is_member_function_pointer<E>::value || (std::is_pointer<E>::value && std::is_function< typename std::remove_pointer<E>::type >::value)), "FIELD_FUNCTION member must be a function pointer" )

#undef DATAMAP_KIND_RULE

// Raw-byte members (single field over an array: vec3_t, byte[4]...) skip the kind check
template< int FT, typename E, bool SKIP > struct CDatamapKindGate { enum { OK = 1 }; };
template< int FT, typename E > struct CDatamapKindGate< FT, E, false > { enum { OK = CDatamapKindCheck< FT, E >::OK }; };

template< int FT, typename T, int N >
struct CDatamapMemberCheck
{
	// remove_all_extents also handles 2D arrays like float m[4][4]
	typedef typename std::remove_cv< typename std::remove_all_extents< T >::type >::type E;

	static_assert(N > 0 && N <= 0xFFFF, "field count must fit the typedesc's unsigned short");

	// FIELD_FUNCTION is exempt: a member function pointer is 16 bytes on GCC/Clang, only the first word is saved
	static_assert(FT == FIELD_FUNCTION || sizeof( T ) == static_cast<size_t>(N) * static_cast<size_t>(CDatamapFieldSizeDeducer<FT>::SIZE),
		"member size does not match what save/restore reads and writes for this FIELD type");

	// restore writes raw bytes over the member, so a vtable pointer would be overwritten
	static_assert(!std::is_polymorphic<E>::value, "member has a vtable, it can't be saved as raw bytes");

	// the element type can't be verified for:
	//  - a single field over a raw array (vec3_t, byte controller[4])
	//  - a class/struct holding N elements (matrix4x4, Vector4D...): saved as raw bytes,
	//    the size check above is what protects it
	enum { RAW_BLOCK = (std::is_array<T>::value && N == 1) || std::is_class<E>::value };
	enum { KIND_OK = CDatamapKindGate< FT, E, (RAW_BLOCK != 0) >::OK };
	enum { COUNT = N };
};

#define _FIELD_CHECKED( name, fieldtype, count )	CDatamapMemberCheck< fieldtype, decltype( ((classNameTypedef *)0)->name ), count >::COUNT
#define _EFIELD_CHECKED( name, fieldtype, count )	CDatamapMemberCheck< fieldtype, decltype( ((entvars_t *)0)->name ), count >::COUNT

#define _FIELD(name, fieldtype, count, flags, mapname )		{ fieldtype, #name, offsetof(classNameTypedef, name), count, flags, mapname, NULL }
#define _EFIELD(name, fieldtype, count, flags, mapname )		{ fieldtype, #name, offsetof(entvars_t, name), count, flags, mapname, NULL }
#define DEFINE_FIELD_NULL					{ FIELD_VOID, 0, 0, 0, 0, 0, 0 }
#define DEFINE_FIELD(name, fieldtype)			_FIELD(name, fieldtype, _FIELD_CHECKED( name, fieldtype, 1 ), FTYPEDESC_SAVE, NULL )
#define DEFINE_KEYFIELD(name, fieldtype, mapname)	_FIELD(name, fieldtype, _FIELD_CHECKED( name, fieldtype, 1 ), FTYPEDESC_KEY|FTYPEDESC_SAVE, mapname )
#define DEFINE_ARRAY(name, fieldtype, count)		_FIELD(name, fieldtype, _FIELD_CHECKED( name, fieldtype, count ), FTYPEDESC_SAVE, NULL )
#define DEFINE_GLOBAL_FIELD(name, fieldtype)		_FIELD(name, fieldtype, _FIELD_CHECKED( name, fieldtype, 1 ), FTYPEDESC_GLOBAL|FTYPEDESC_SAVE, NULL )
#define DEFINE_GLOBAL_KEYFIELD(name, fieldtype, mapname)	_FIELD(name, fieldtype, _FIELD_CHECKED( name, fieldtype, 1 ), FTYPEDESC_GLOBAL|FTYPEDESC_KEY|FTYPEDESC_SAVE, mapname )
#define DEFINE_ENTITY_FIELD(name, fieldtype)		_EFIELD(name, fieldtype, _EFIELD_CHECKED( name, fieldtype, 1 ), FTYPEDESC_KEY|FTYPEDESC_SAVE, #name )
#define DEFINE_ENTITY_GLOBAL_FIELD(name, fieldtype)	_EFIELD(name, fieldtype, _EFIELD_CHECKED( name, fieldtype, 1 ), FTYPEDESC_KEY|FTYPEDESC_SAVE|FTYPEDESC_GLOBAL, #name )

// opt-out for deliberate exceptions
#define DEFINE_FIELD_UNCHECKED(name, fieldtype)		_FIELD(name, fieldtype, 1, FTYPEDESC_SAVE, NULL )
#define DEFINE_ENTITY_FIELD_UNCHECKED(name, fieldtype)	_EFIELD(name, fieldtype, 1, FTYPEDESC_KEY|FTYPEDESC_SAVE, #name )

// replaces EXPORT table for portability and non-DLL based systems (xbox)
#define DEFINE_FUNCTION_RAW( function, func_type )		{ FIELD_VOID, nameHolder.GenerateName(#function), 0, 1, FTYPEDESC_FUNCTIONTABLE, NULL, (func_t)((func_type)(&classNameTypedef::function)) }
#define DEFINE_FUNCTION( function )				DEFINE_FUNCTION_RAW( function, func_t )

//
// Generic function prototype.
//
typedef void (CBaseEntity::*func_t)(void);

typedef struct typedescription_s
{
	FIELDTYPE		fieldType;
	const char	*fieldName;
	int		fieldOffset;
	unsigned short	fieldSize;
	short		flags;
	const char	*mapName;				// a name of keyfield on a map (e.g. "origin", "spawnflags" etc)	
	func_t		func;				// pointer to a function 
} TYPEDESCRIPTION;


//-----------------------------------------------------------------------------
// Purpose: stores the list of objects in the hierarchy
//          used to iterate through an object's data descriptions
//-----------------------------------------------------------------------------
typedef struct datamap_s
{
	TYPEDESCRIPTION	*dataDesc;
	int		dataNumFields;
	char const	*dataClassName;
	datamap_s		*baseMap;
} DATAMAP;

//-----------------------------------------------------------------------------
//
// Macros used to implement datadescs
//
#define DECLARE_SIMPLE_DATADESC() \
	static DATAMAP m_DataMap; \
	static DATAMAP *GetBaseMap(); \
	template <typename T> friend void DataMapAccess(T *, DATAMAP **p); \
	template <typename T> friend DATAMAP *DataMapInit(T *);

#define DECLARE_DATADESC() \
	DECLARE_SIMPLE_DATADESC() \
	virtual DATAMAP *GetDataDescMap( void );

#define BEGIN_DATADESC( className ) \
	DATAMAP className::m_DataMap = { 0, 0, #className, NULL }; \
	DATAMAP *className::GetDataDescMap( void ) { return &m_DataMap; } \
	DATAMAP *className::GetBaseMap() { DATAMAP *pResult; DataMapAccess((BaseClass *)NULL, &pResult); return pResult; } \
	BEGIN_DATADESC_GUTS( className )

#define BEGIN_DATADESC_NO_BASE( className ) \
	DATAMAP className::m_DataMap = { 0, 0, #className, NULL }; \
	DATAMAP *className::GetDataDescMap( void ) { return &m_DataMap; } \
	DATAMAP *className::GetBaseMap() { return NULL; } \
	BEGIN_DATADESC_GUTS( className )

#define BEGIN_SIMPLE_DATADESC( className ) \
	DATAMAP className::m_DataMap = { 0, 0, #className, NULL }; \
	DATAMAP *className::GetBaseMap() { return NULL; } \
	BEGIN_DATADESC_GUTS( className )

#define BEGIN_DATADESC_GUTS( className ) \
	template <typename T> DATAMAP *DataMapInit(T *); \
	template <> DATAMAP *DataMapInit<className>( className * ); \
	namespace className##_DataDescInit \
	{ \
		DATAMAP *g_DataMapHolder = DataMapInit( (className *)NULL ); /* This can/will be used for some clean up duties later */ \
	} \
	\
	template <> DATAMAP *DataMapInit<className>( className * ) \
	{ \
		typedef className classNameTypedef; \
		static CDatadescGeneratedNameHolder nameHolder(#className); \
		className::m_DataMap.baseMap = className::GetBaseMap(); \
		static TYPEDESCRIPTION dataDesc[] = \
		{ \
		{ FIELD_VOID,0,0,0,0,0,0}, /* so you can define "empty" tables */

#define END_DATADESC() \
		}; \
		\
		if ( sizeof( dataDesc ) > sizeof( dataDesc[0] ) ) \
		{ \
			classNameTypedef::m_DataMap.dataNumFields = SIZEOFARRAY( dataDesc ) - 1; \
			classNameTypedef::m_DataMap.dataDesc 	  = &dataDesc[1]; \
		} \
		else \
		{ \
			classNameTypedef::m_DataMap.dataNumFields = 1; \
			classNameTypedef::m_DataMap.dataDesc 	  = dataDesc; \
		} \
		return &classNameTypedef::m_DataMap; \
	}

//-----------------------------------------------------------------------------

#define DECLARE_CLASS( className, baseClassName ) \
	typedef baseClassName BaseClass; \
	typedef className ThisClass;

#define DECLARE_CLASS_NOBASE( className ) \
	typedef className ThisClass;

//-----------------------------------------------------------------------------

template <typename T> 
inline void DataMapAccess(T *ignored, DATAMAP **p)
{
	*p = &T::m_DataMap;
}

//-----------------------------------------------------------------------------

class CDatadescGeneratedNameHolder
{
public:
	CDatadescGeneratedNameHolder( const char *pszBase ) : m_pszBase(pszBase)
	{
		m_nLenBase = strlen( m_pszBase ) + 2;
	}
	
	~CDatadescGeneratedNameHolder()
	{
		for( int i = 0; i < m_Names.Count(); i++ )
		{
			delete m_Names[i];
		}
	}
	
	const char *GenerateName( const char *pszIdentifier )
	{
		char *pBuf = new char[m_nLenBase + strlen(pszIdentifier) + 1];
		sprintf( pBuf, "%s::", m_pszBase );
		strcat( pBuf, pszIdentifier );
		m_Names.AddToTail( pBuf );
		return pBuf;
	}
	
private:
	const char *m_pszBase;
	size_t m_nLenBase;
	CUtlArray<char *> m_Names;
};

#endif // DATAMAP_H
