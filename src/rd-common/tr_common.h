#ifndef DEDICATED
#ifndef TR_COMMON_H
#define TR_COMMON_H

#include "../rd-common/tr_public.h"
#include "../rd-common/tr_font.h"

extern refimport_t ri;

/*
================================================================================
 Noise Generation
================================================================================
*/
// Initialize the noise generator.
void R_NoiseInit( void );

// Get random 4-component vector.
float R_NoiseGet4f( float x, float y, float z, double t );


// Get the noise time.
float GetNoiseTime( int t );

/*
================================================================================
 Image Loading
================================================================================
*/
// Initialize the image loader.
void R_ImageLoader_Init();

typedef void (*ImageLoaderFn)( const char *filename, byte **pic, int *width, int *height );

// Adds a new image loader to handle a new image type. The extension should not
// begin with a period (a full stop).
qboolean R_ImageLoader_Add( const char *extension, ImageLoaderFn imageLoader );

// Load an image from file.
void R_LoadImage( const char *shortname, byte **pic, int *width, int *height );

// Load raw image data from TGA image.
void LoadTGA( const char *name, byte **pic, int *width, int *height );

// Load raw image data from JPEG image.
void LoadJPG( const char *filename, byte **pic, int *width, int *height );

// Load raw image data from PNG image.
void LoadPNG( const char *filename, byte **data, int *width, int *height );

#endif
#endif
