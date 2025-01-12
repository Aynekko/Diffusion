#ifndef EMBOSS_H
#define EMBOSS_H

#define KERNEL_SIZE 9

const float kernel[KERNEL_SIZE] = float[KERNEL_SIZE](
    2.0, 0.0, 0.0,
    0.0, -1.0, 0.0,
    0.0, 0.0, -1.0
    );

const vec2 offset[KERNEL_SIZE] = vec2[KERNEL_SIZE](
    vec2( -0.5, -0.5 ), vec2( 0.0, -0.5 ), vec2( 0.5, -0.5 ),
    vec2( -0.5, 0.0 ), vec2( 0.0, 0.0 ), vec2( 0.5, 0.0 ),
    vec2( -0.5, 0.5 ), vec2( 0.0, 0.5 ), vec2( 0.5, 0.5 )
    );

const vec3 factor = vec3( 0.27, 0.67, 0.06 );

vec3 EmbossFilter( sampler2D ColorMap, vec2 texcoord, float EmbossScale )
{
    vec2 pstep = vec2( EmbossScale ) / vec2( textureSize( ColorMap, 0 ) );
    vec4 res = vec4( 0.5 );

    for( int i = 0; i < KERNEL_SIZE; ++i )
        res += texture2D( ColorMap, texcoord + offset[i] * pstep ) * kernel[i];
    vec3 emboss = vec3( dot( factor, vec3( res ) ) );

    emboss *= 2.0;

    return emboss;
}

#if defined( BMODEL_MULTI_LAYERS )
vec3 EmbossFilterTerrain( sampler2DArray ColorMap, vec2 texcoord, vec4 mask0, vec4 mask1, vec4 mask2, vec4 mask3, float EmbossScale )
{
    vec2 pstep = vec2( EmbossScale ) / vec2( textureSize( ColorMap, 0 ) );
    vec4 res = vec4( 0.5 );
    vec3 emboss = vec3( 0.0 );

#if (TERRAIN_NUM_LAYERS >= 1)
    if( mask0.r > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 0.0 ) ) * kernel[i] * mask0.r;

    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 2
    if( mask0.g > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 1.0 ) ) * kernel[i] * mask0.g;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 3
    if( mask0.b > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 2.0 ) ) * kernel[i] * mask0.b;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 4
    if( mask0.a > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 3.0 ) ) * kernel[i] * mask0.a;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 5
    if( mask1.r > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 4.0 ) ) * kernel[i] * mask1.r;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 6
    if( mask1.g > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 5.0 ) ) * kernel[i] * mask1.g;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 7
    if( mask1.b > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 6.0 ) ) * kernel[i] * mask1.b;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 8
    if( mask1.a > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 7.0 ) ) * kernel[i] * mask1.a;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 9
    if( mask0.r > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 8.0 ) ) * kernel[i] * mask2.r;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 10
    if( mask0.g > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 9.0 ) ) * kernel[i] * mask2.g;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 11
    if( mask0.b > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 10.0 ) ) * kernel[i] * mask2.b;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 12
    if( mask0.a > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 11.0 ) ) * kernel[i] * mask2.a;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 13
    if( mask1.r > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 12.0 ) ) * kernel[i] * mask3.r;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 14
    if( mask1.g > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 13.0 ) ) * kernel[i] * mask3.g;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 15
    if( mask1.b > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 14.0 ) ) * kernel[i] * mask3.b;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

#if TERRAIN_NUM_LAYERS >= 16
    if( mask1.a > 0.0 )

        for( int i = 0; i < KERNEL_SIZE; ++i )
            res += texture2DArray( ColorMap, vec3( texcoord + offset[i] * pstep, 15.0 ) ) * kernel[i] * mask3.a;
    emboss = vec3( dot( factor, vec3( res ) ) );
#endif

    emboss *= 2.0;

    return emboss;
}
#endif // /BMODEL_MULTI_LAYERS

#endif//EMBOSS_H