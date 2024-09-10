/*
tnbasis.h - compute TBN matrix
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef TNBASIS_H
#define TNBASIS_H

attribute vec3		attr_Normal;
attribute vec3		attr_Tangent;
attribute vec3		attr_Binormal;

mat3 ComputeTBN( const in mat4 modelMatrix )
{
	mat3 tbn;

	tbn[0] = mat3( modelMatrix ) * normalize( attr_Tangent );
	tbn[1] = mat3( modelMatrix ) * normalize( attr_Binormal );
	tbn[2] = mat3( modelMatrix ) * normalize( attr_Normal );

	return tbn;
}

#endif//TNBASIS_H