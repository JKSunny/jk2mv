/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/
// tr_map.c

#include "tr_local.h"

static const imgFlags_t lightmapFlags = IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_LIGHTMAP | IMGFLAG_NOSCALE | IMGFLAG_CLAMPTOEDGE;

/*

Loads and prepares a map file for scene rendering.

A single entry point:

void RE_LoadWorldMap( const char *name );

*/

static	world_t		s_worldData;
static	byte		*fileBase;

static int			c_gridVerts;

//===============================================================================

static void HSVtoRGB( float h, float s, float v, float rgb[3] )
{
	int i;
	float f;
	float p, q, t;

	h *= 5;

	i = floor( h );
	f = h - i;

	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch ( i )
	{
	case 0:
		rgb[0] = v;
		rgb[1] = t;
		rgb[2] = p;
		break;
	case 1:
		rgb[0] = q;
		rgb[1] = v;
		rgb[2] = p;
		break;
	case 2:
		rgb[0] = p;
		rgb[1] = v;
		rgb[2] = t;
		break;
	case 3:
		rgb[0] = p;
		rgb[1] = q;
		rgb[2] = v;
		break;
	case 4:
		rgb[0] = t;
		rgb[1] = p;
		rgb[2] = v;
		break;
	case 5:
		rgb[0] = v;
		rgb[1] = p;
		rgb[2] = q;
		break;
	}
}

/*
===============
R_ClampDenorm

Clamp fp values that may result in denormalization after further multiplication
===============
*/
float R_ClampDenorm( float v ) {
	if ( fabsf( v ) > 0.0f && fabsf( v ) < 1e-9f ) {
		return 0.0f;
	} else {
		return v;
	}
}

/*
===============
R_ColorShiftLightingBytes

===============
*/
void R_ColorShiftLightingBytes( const byte in[4], byte out[4], qboolean hasAlpha ) {
	int shift, r, g, b;

	// shift the color data based on overbright range
	shift = Q_max( 0, r_mapOverBrightBits->integer - tr.overbrightBits );

	// shift the data based on overbright range
	r = in[0] << shift;
	g = in[1] << shift;
	b = in[2] << shift;

	// normalize by color instead of saturating to white
	if ( (r|g|b) > 255 ) {
		int		max;

		max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	if ( r_mapGreyScale->integer ) {
		const byte luma = LUMA( r, g, b );
		out[0] = luma;
		out[1] = luma;
		out[2] = luma;
	}
	else if ( r_mapGreyScale->value ) {
		const float scale = fabs( r_mapGreyScale->value );
		const float luma = LUMA( r, g, b );
		out[0] = LERP( r, luma, scale );
		out[1] = LERP( g, luma, scale );
		out[2] = LERP( b, luma, scale );
	}
	else {
		out[0] = r;
		out[1] = g;
		out[2] = b;
	}

	if ( hasAlpha ) {
		out[3] = in[3];
	}
}

/*
===============
R_LoadLightmaps

===============
*/
#define	DEFAULT_LIGHTMAP_SIZE	128
#define MAX_LIGHTMAP_PAGES 2

static void R_LoadLightmaps( lump_t *l, lump_t *surfs, world_t &worldData ) {
	byte		*buf, *buf_p;
	dsurface_t  *surf;
	int			len;
	byte		*image;
	int			imageSize;
	int			i, j, numLightmaps = 0;
	float maxIntensity = 0;
	int numColorComponents = 3;

	const int lightmapSize = DEFAULT_LIGHTMAP_SIZE;
	tr.worldInternalLightmapping = qfalse;

	len = l->filelen;
	// test for external lightmaps
	if ( !len ) {
		for ( i = 0, surf = (dsurface_t *)(fileBase + surfs->fileofs );
			i < surfs->filelen / sizeof(dsurface_t);
			i++, surf++) {
			for ( int j = 0; j < MAXLIGHTMAPS; j++ )
			{
				numLightmaps = MAX( numLightmaps, LittleLong(surf->lightmapNum[j]) + 1 );
			}
		}
		buf = NULL;
	}
	else
	{
		numLightmaps = len / (lightmapSize * lightmapSize * 3);
		buf = fileBase + l->fileofs;
		tr.worldInternalLightmapping = qtrue;
	}

	if ( numLightmaps == 0 )
		return;

	// we are about to upload textures
	//R_IssuePendingRenderCommands();

	imageSize = lightmapSize * lightmapSize * 4 * 2;
	image = (byte *)Z_Malloc( imageSize, TAG_BSP, qfalse );

	if ( tr.worldInternalLightmapping )
	{
		const int targetLightmapsPerX = (int)ceilf(sqrtf( numLightmaps ));

		int lightmapsPerX = 1;
		while ( lightmapsPerX < targetLightmapsPerX )
			lightmapsPerX *= 2;

		tr.lightmapsPerAtlasSide[0] = lightmapsPerX;
		tr.lightmapsPerAtlasSide[1] = (int)ceilf((float)numLightmaps / lightmapsPerX);

		tr.lightmapAtlasSize[0] = tr.lightmapsPerAtlasSide[0] * LIGHTMAP_WIDTH;
		tr.lightmapAtlasSize[1] = tr.lightmapsPerAtlasSide[1] * LIGHTMAP_HEIGHT;

		// FIXME: What happens if we need more?
		tr.numLightmaps = 1;
	}
	else
	{
		tr.numLightmaps = numLightmaps;
	}

#ifdef USE_JK2
	// interesting ..
	// Hunk_AllocDebug differs from Hunk_Alloc
	// Hunk_AllocDebug marks ** as valid, meaning [-1] is not NULL.
	// 
	// "tr.lightmaps[lightmapIndex[i]] == NULL" in "R_FindLightmap" returns false on Debug
	// but true on Release ..
	tr.lightmaps = (image_t **)Z_Malloc( tr.numLightmaps * sizeof(image_t *), TAG_BSP, qtrue );
#else
	tr.lightmaps = (image_t **)ri.Hunk_Alloc( tr.numLightmaps * sizeof(image_t *), h_low );
#endif

	if ( tr.worldInternalLightmapping )
	{
		for ( i = 0; i < tr.numLightmaps; i++ )
		{
			tr.lightmaps[i] = R_CreateImage(
				va("_lightmapatlas%d", i),
				NULL,
				tr.lightmapAtlasSize[0],
				tr.lightmapAtlasSize[1],
				lightmapFlags
				);
		}
	}

	for ( i = 0; i < numLightmaps; i++ )
	{
		int xoff = 0, yoff = 0;
		int lightmapnum = i;
		// expand the 24 bit on-disk to 32 bit

		if ( tr.worldInternalLightmapping )
		{
			xoff = (i % tr.lightmapsPerAtlasSide[0]) * lightmapSize;
			yoff = (i / tr.lightmapsPerAtlasSide[0]) * lightmapSize;
			lightmapnum = 0;
		}

		// if (tr.worldLightmapping)
		{
			char filename[MAX_QPATH];
			byte *externalLightmap = NULL;
			int lightmapWidth = lightmapSize;
			int lightmapHeight = lightmapSize;
			bool foundLightmap = true;

			
			if (!tr.worldInternalLightmapping)
			{
				Com_sprintf(filename, sizeof(filename), "maps/%s/lm_%04d.tga", worldData.baseName, i );

				R_LoadImage(filename, &externalLightmap, &lightmapWidth, &lightmapHeight);
			}
			
			if ( externalLightmap )
			{
				int newImageSize = lightmapWidth * lightmapHeight * 4 * 2;
				if ( tr.worldInternalLightmapping && (lightmapWidth != lightmapSize || lightmapHeight != lightmapSize) )
				{
					ri.Printf( PRINT_ALL, "Error loading %s: non %dx%d lightmaps\n", filename, lightmapSize, lightmapSize );
					Z_Free( externalLightmap );
					externalLightmap = NULL;
					continue;
				}
				else if ( newImageSize > imageSize )
				{
					Z_Free( image );
					imageSize = newImageSize;
					image = (byte *)Z_Malloc( imageSize, TAG_BSP, qfalse );
				}
				numColorComponents = 4;
			}
			if ( !externalLightmap )
			{
				lightmapWidth = lightmapSize;
				lightmapHeight = lightmapSize;
				numColorComponents = 3;
			}

			foundLightmap = true;
			if ( externalLightmap )
			{
				buf_p = externalLightmap;
			}
			else if ( buf )
			{
				buf_p = buf + i * lightmapSize * lightmapSize * 3;
			}
			else
			{
				buf_p = NULL;
				foundLightmap = false;
			}

			if ( foundLightmap )
			{
				for ( j = 0; j < lightmapWidth * lightmapHeight; j++ )
				{
					if ( buf_p )
					{
						if ( r_lightmap->integer == 2 )
						{	// color code by intensity as development tool	(FIXME: check range)
							float r = buf_p[j*numColorComponents + 0];
							float g = buf_p[j*numColorComponents + 1];
							float b = buf_p[j*numColorComponents + 2];
							float intensity;
							float out[3] = { 0.0, 0.0, 0.0 };

							intensity = 0.33f * r + 0.685f * g + 0.063f * b;

							if ( intensity > 255 )
								intensity = 1.0f;
							else
								intensity /= 255.0f;

							if ( intensity > maxIntensity )
								maxIntensity = intensity;

							HSVtoRGB( intensity, 1.00, 0.50, out );

							image[j * 4 + 0] = out[0] * 255;
							image[j * 4 + 1] = out[1] * 255;
							image[j * 4 + 2] = out[2] * 255;
							image[j * 4 + 3] = 255;
						}
						else
						{
							R_ColorShiftLightingBytes( &buf_p[j * numColorComponents], &image[j * 4], qfalse );
							image[j * 4 + 3] = 255;
						}
					}
				}

				if ( tr.worldInternalLightmapping )
					vk_upload_image_data( 
						tr.lightmaps[lightmapnum],
						xoff,
						yoff,
						lightmapWidth,
						lightmapHeight,
						1,
						image,
						lightmapWidth * lightmapHeight * 4, qtrue );
				else
					tr.lightmaps[i] = R_CreateImage(
						va("*lightmap%d", i),
						image,
						lightmapWidth,
						lightmapHeight,
						lightmapFlags );
			}

			if ( externalLightmap )
				Z_Free( externalLightmap );
		}
	}

	if ( r_lightmap->integer == 2 )	{
		ri.Printf( PRINT_ALL, "Brightest lightmap value: %d\n", ( int ) ( maxIntensity * 255 ) );
	}

	Z_Free( image );
}

static float FatPackU( float input, int lightmapnum )
{
	if ( lightmapnum < 0 )
		return input;

	if ( tr.lightmapAtlasSize[0] > 0 )
	{
		const int lightmapXOffset = lightmapnum % tr.lightmapsPerAtlasSide[0];
		const float invLightmapSide = 1.0f / tr.lightmapsPerAtlasSide[0];

		return ( lightmapXOffset * invLightmapSide ) + ( input * invLightmapSide );
	}

	return input;
}

static float FatPackV( float input, int lightmapnum )
{
	if ( lightmapnum < 0 )
		return input;

	if ( tr.lightmapAtlasSize[1] > 0 )
	{
		const int lightmapYOffset = lightmapnum / tr.lightmapsPerAtlasSide[0];
		const float invLightmapSide = 1.0f / tr.lightmapsPerAtlasSide[1];

		return ( lightmapYOffset * invLightmapSide ) + ( input * invLightmapSide );
	}

	return input;
}


static int FatLightmap(int lightmapnum)
{
	if (lightmapnum < 0)
		return lightmapnum;

	if (tr.lightmapAtlasSize[0] > 0)
		return 0;
	
	return lightmapnum;
}

/*
=================
RE_SetWorldVisData

This is called by the clipmodel subsystem so we can share the 1.8 megs of
space in big maps...
=================
*/
void		RE_SetWorldVisData( const byte *vis ) {
	tr.externalVisData = vis;
}

/*
=================
R_LoadVisibility
=================
*/
static	void R_LoadVisibility( const lump_t *l, world_t &worldData ) {
	int		len;
	byte	*buf;

	len = ( worldData.numClusters + 63 ) & ~63;
	worldData.novis = (unsigned char *)Hunk_Alloc( len, h_low );
	memset( worldData.novis, 0xff, len );

    len = l->filelen;
	if ( !len ) {
		return;
	}
	buf = fileBase + l->fileofs;

	worldData.numClusters = LittleLong( ((int *)buf)[0] );
	worldData.clusterBytes = LittleLong( ((int *)buf)[1] );

	// CM_Load should have given us the vis data to share, so
	// we don't need to allocate another copy
	if ( tr.externalVisData ) {
		worldData.vis = tr.externalVisData;
	} else {
		byte	*dest;

		dest = (unsigned char *)Hunk_Alloc( len - 8, h_low );
		memcpy( dest, buf + 8, len - 8 );
		worldData.vis = dest;
	}
}

//===============================================================================

/*
===============
ShaderForShaderNum
===============
*/
static shader_t *ShaderForShaderNum( int shaderNum, const int *lightmapNum, const byte *lightmapStyles, const byte *vertexStyles, world_t &worldData )
{
	shader_t	*shader;
	dshader_t	*dsh;
	const byte	*styles;

	styles = lightmapStyles;

	LL( shaderNum );
	if ( shaderNum < 0 || shaderNum >= worldData.numShaders ) {
		Com_Error( ERR_DROP, "ShaderForShaderNum: bad num %i", shaderNum );
	}
	dsh = &worldData.shaders[ shaderNum ];

	if (lightmapNum[0] == LIGHTMAP_BY_VERTEX)
	{
		styles = vertexStyles;
	}

	if ( r_vertexLight->integer )
	{
		lightmapNum = lightmapsVertex;
		styles = vertexStyles;
	}

	shader = R_FindShader( dsh->shader, lightmapNum, styles, qtrue );

#ifndef USE_JK2_SHADER_REMAP
	// if the shader had errors, just use default shader
	if ( shader->defaultShader ) {
		return tr.defaultShader;
	}
#endif

	return shader;
}

static void GenerateNormals( srfSurfaceFace_t *face )
{
	vec3_t ba, ca, cross;
	float* v1, * v2, * v3, * n1, * n2, * n3;
	int i, * indices, i0, i1, i2;

	indices = ((int*)((byte*)face + face->ofsIndices));

	// store as vec4_t so we can simply use memcpy() during tesselation
	face->normals = (float*)ri.Hunk_Alloc(face->numPoints * sizeof(tess.normal[0]), h_low);

	for (i = 0; i < face->numIndices; i += 3) {
		i0 = indices[i + 0];
		i1 = indices[i + 1];
		i2 = indices[i + 2];
		if (i0 >= face->numPoints || i1 >= face->numPoints || i2 >= face->numPoints)
			continue;
		v1 = face->points[i0];
		v2 = face->points[i1];
		v3 = face->points[i2];
		VectorSubtract(v3, v1, ca);
		VectorSubtract(v2, v1, ba);
		CrossProduct(ca, ba, cross);
		n1 = face->normals + indices[i + 0] * 4;
		n2 = face->normals + indices[i + 1] * 4;
		n3 = face->normals + indices[i + 2] * 4;
		VectorAdd(n1, cross, n1);
		VectorAdd(n2, cross, n2);
		VectorAdd(n3, cross, n3);
	}

	for (i = 0; i < face->numPoints; i++) {
		n1 = face->normals + i * 4;
		VectorNormalize2(n1, n1);
		for ( i0 = 0; i0 < 3; i0++ ) {
			n1[i0] = R_ClampDenorm( n1[i0] );
		}
	}
}

/*
===============
ParseFace
===============
*/
static void ParseFace( const dsurface_t *ds, const mapVert_t *verts, msurface_t *surf, int *indexes, world_t &worldData, int index ) {
	int					i, j;
	srfSurfaceFace_t	*cv;
	int					numPoints, numIndexes;
	int					lightmapNum[MAXLIGHTMAPS];
	int					sfaceSize, ofsIndexes;

	for(i = 0; i < MAXLIGHTMAPS; i++)
	{
		lightmapNum[i] = FatLightmap( LittleLong( ds->lightmapNum[i] ) );
	}

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = worldData.globalFog;
	}

	// get shader value
	surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum, ds->lightmapStyles, ds->vertexStyles, worldData );
	if ( r_singleShader->integer && !surf->shader->sky ) {
		surf->shader = tr.defaultShader;
	}

	numPoints = LittleLong( ds->numVerts );
	if (numPoints > MAX_FACE_POINTS) {
		vk_debug("WARNING: MAX_FACE_POINTS exceeded: %i\n", numPoints);
		numPoints = MAX_FACE_POINTS;
		surf->shader = tr.defaultShader;
	}

	numIndexes = LittleLong( ds->numIndexes );

	// create the srfSurfaceFace_t
	sfaceSize = ( size_t ) &((srfSurfaceFace_t *)0)->points[numPoints];
	ofsIndexes = sfaceSize;
	sfaceSize += sizeof( int ) * numIndexes;

	cv = (srfSurfaceFace_t *)Hunk_Alloc( sfaceSize, h_low );
	cv->surfaceType = SF_FACE;
	cv->numPoints = numPoints;
	cv->numIndices = numIndexes;
	cv->ofsIndices = ofsIndexes;

	
	verts += LittleLong( ds->firstVert );

	for ( i = 0 ; i < numPoints ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			cv->points[i][j] = LittleFloat( verts[i].xyz[j] );
		}

		for ( j = 0 ; j < 2 ; j++ ) {
			cv->points[i][3+j] = LittleFloat( verts[i].st[j] );
		}

		for ( j = 0; j < MAXLIGHTMAPS; j++ )
		{
			cv->points[i][VERTEX_LM+0+(j*2)] = FatPackU( 
				LittleFloat( verts[i].lightmap[j][0] ), ds->lightmapNum[j] );

			cv->points[i][VERTEX_LM+1+(j*2)] = FatPackV( 
				LittleFloat( verts[i].lightmap[j][1] ), ds->lightmapNum[j] );

			R_ColorShiftLightingBytes( verts[i].color[j], (byte *)&cv->points[i][VERTEX_COLOR+j], qtrue );
		}
	}


	indexes += LittleLong( ds->firstIndex );
	for ( i = 0 ; i < numIndexes ; i++ ) {
		((int *)((byte *)cv + cv->ofsIndices ))[i] = LittleLong( indexes[ i ] );
	}

	// take the plane information from the lightmap vector
	for ( i = 0 ; i < 3 ; i++ ) {
		cv->plane.normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}

#ifdef USE_PMLIGHT
	if (surf->shader->numUnfoggedPasses && surf->shader->lightingStage >= 0) {
		if (fabsf(cv->plane.normal[0]) < 0.01f && fabsf(cv->plane.normal[1]) < 0.01f && fabsf(cv->plane.normal[2]) < 0.01f) {
			// Zero-normals case:
			// might happen if surface contains multiple non-coplanar faces for terrain simulation
			// like in 'Pyramid of the Magician', 'tvy-bench' or 'terrast' maps
			// which results in non-working new per-pixel dynamic lighting.
			// So we will try to regenerate normals and apply smooth shading
			// for normals that is shared between multiple faces.
			// It is not a big problem for incorrectly (negative) generated normals
			// because it is unlikely for shared ones and will result in the same non-working lighting.
			// Also we will NOT update existing face->plane.normal to avoid potential surface culling issues
			GenerateNormals(cv);
		}
	}
#endif

	for ( i = 0; i < 3; i++ ) {
		cv->plane.normal[i] = R_ClampDenorm( cv->plane.normal[i] );
	}

	cv->plane.dist = DotProduct( cv->points[0], cv->plane.normal );
	SetPlaneSignbits( &cv->plane );
	cv->plane.type = PlaneTypeForNormal( cv->plane.normal );

	surf->data = (surfaceType_t *)cv;
}

/*
===============
ParseMesh
===============
*/
static void ParseMesh ( const dsurface_t *ds, const mapVert_t *verts, msurface_t *surf, world_t &worldData, int index ) {
	srfGridMesh_t			*grid;
	int						i, j;
	int						width, height, numPoints;
	drawVert_t				points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];
	int						lightmapNum[MAXLIGHTMAPS];
	vec3_t					bounds[2];
	vec3_t					tmpVec;
	static surfaceType_t	skipData = SF_SKIP;

	for(i=0;i<MAXLIGHTMAPS;i++)
	{
		lightmapNum[i] = FatLightmap( LittleLong( ds->lightmapNum[i] ) );
	}

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = worldData.globalFog;
	}

	// get shader value
	surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum, ds->lightmapStyles, ds->vertexStyles, worldData );
	if ( r_singleShader->integer && !surf->shader->sky ) {
		surf->shader = tr.defaultShader;
	}

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if ( worldData.shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & SURF_NODRAW ) {
		surf->data = &skipData;
		return;
	}

	width = LittleLong( ds->patchWidth );
	height = LittleLong( ds->patchHeight );

	verts += LittleLong( ds->firstVert );
	numPoints = width * height;
	for ( i = 0 ; i < numPoints ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			points[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			points[i].normal[j] = R_ClampDenorm( LittleFloat( verts[i].normal[j] ) );
		}
		for ( j = 0 ; j < 2 ; j++ ) {
			points[i].st[j] = LittleFloat( verts[i].st[j] );
		}
		for ( j = 0; j < MAXLIGHTMAPS; j++ )
		{
			points[i].lightmap[j][0] = FatPackU( 
				LittleFloat( verts[i].lightmap[j][0] ), ds->lightmapNum[j] );

			points[i].lightmap[j][1] = FatPackV( 
				LittleFloat( verts[i].lightmap[j][1] ), ds->lightmapNum[j] );

			R_ColorShiftLightingBytes( verts[i].color[j], points[i].color[j], qtrue );
		}
	}

	// pre-tesseleate
	grid = R_SubdividePatchToGrid( width, height, points );
	surf->data = (surfaceType_t *)grid;

	// copy the level of detail origin, which is the center
	// of the group of all curves that must subdivide the same
	// to avoid cracking
	for ( i = 0 ; i < 3 ; i++ ) {
		bounds[0][i] = LittleFloat( ds->lightmapVecs[0][i] );
		bounds[1][i] = LittleFloat( ds->lightmapVecs[1][i] );
	}
	VectorAdd( bounds[0], bounds[1], bounds[1] );
	VectorScale( bounds[1], 0.5f, grid->lodOrigin );
	VectorSubtract( bounds[0], grid->lodOrigin, tmpVec );
	grid->lodRadius = VectorLength( tmpVec );
}

/*
===============
ParseTriSurf
===============
*/
static void ParseTriSurf( const dsurface_t *ds, const mapVert_t *verts, msurface_t *surf, int *indexes, world_t &worldData, int index ) {
	srfTriangles_t	*tri;
	int				i, j;
	int				numVerts, numIndexes;

	int lightmapNum[MAXLIGHTMAPS];

	for ( j = 0; j < MAXLIGHTMAPS; j++ )
		lightmapNum[j] = FatLightmap(LittleLong (ds->lightmapNum[j]));

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = worldData.globalFog;
	}

	// get shader
	surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum, ds->lightmapStyles, ds->vertexStyles, worldData );
	if ( r_singleShader->integer && !surf->shader->sky ) {
		surf->shader = tr.defaultShader;
	}

	numVerts = LittleLong( ds->numVerts );
	numIndexes = LittleLong( ds->numIndexes );

	if ( numVerts >= SHADER_MAX_VERTEXES ) {
		Com_Error(ERR_DROP, "ParseTriSurf: verts > MAX (%d > %d) on misc_model %s", numVerts, SHADER_MAX_VERTEXES, surf->shader->name );
	}
	if ( numIndexes >= SHADER_MAX_INDEXES ) {
		Com_Error(ERR_DROP, "ParseTriSurf: indices > MAX (%d > %d) on misc_model %s", numIndexes, SHADER_MAX_INDEXES, surf->shader->name );
	}

	tri = (srfTriangles_t *)Hunk_Alloc( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] )
		+ numIndexes * sizeof( tri->indexes[0] ), h_low );
	tri->surfaceType = SF_TRIANGLES;
	tri->numVerts = numVerts;
	tri->numIndexes = numIndexes;
	tri->verts = (drawVert_t *)(tri + 1);
	tri->indexes = (int *)(tri->verts + tri->numVerts );

	surf->data = (surfaceType_t *)tri;

	// copy vertexes
	ClearBounds( tri->bounds[0], tri->bounds[1] );
	verts += LittleLong( ds->firstVert );
	for ( i = 0 ; i < numVerts ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			tri->verts[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			tri->verts[i].normal[j] = R_ClampDenorm( LittleFloat( verts[i].normal[j] ) );
		}
		AddPointToBounds( tri->verts[i].xyz, tri->bounds[0], tri->bounds[1] );
		for ( j = 0 ; j < 2 ; j++ ) {
			tri->verts[i].st[j] = LittleFloat( verts[i].st[j] );

		}
		for ( j = 0; j < MAXLIGHTMAPS; j++ )
		{
			tri->verts[i].lightmap[j][0] = FatPackU( 
				LittleFloat( verts[i].lightmap[j][0] ), ds->lightmapNum[j] );

			tri->verts[i].lightmap[j][1] = FatPackV( 
				LittleFloat( verts[i].lightmap[j][1] ), ds->lightmapNum[j] );

			R_ColorShiftLightingBytes( verts[i].color[j], tri->verts[i].color[j], qtrue );
		}
	}

	// copy indexes
	indexes += LittleLong( ds->firstIndex );
	for ( i = 0 ; i < numIndexes ; i++ ) {
		tri->indexes[i] = LittleLong( indexes[i] );
		if ( tri->indexes[i] < 0 || tri->indexes[i] >= numVerts ) {
			Com_Error( ERR_DROP, "Bad index in triangle surface" );
		}
	}
}

/*
===============
ParseFlare
===============
*/
static void ParseFlare( const dsurface_t *ds, const mapVert_t *verts, msurface_t *surf, int *indexes, world_t &worldData, int index ) {
	srfFlare_t		*flare;
	int				i;
	int				lightmaps[MAXLIGHTMAPS] = { LIGHTMAP_BY_VERTEX };

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = worldData.globalFog;
	}

	// get shader
	surf->shader = ShaderForShaderNum( ds->shaderNum, lightmaps, ds->lightmapStyles, ds->vertexStyles, worldData );
	if ( r_singleShader->integer && !surf->shader->sky ) {
		surf->shader = tr.defaultShader;
	}

	flare = (struct srfFlare_s *)Hunk_Alloc( sizeof( *flare ), h_low );
	flare->surfaceType = SF_FLARE;

	if ( surf->shader == tr.defaultShader )
		flare->shader = tr.flareShader;
	else
		flare->shader = surf->shader;

	surf->data = (surfaceType_t *)flare;

	for ( i = 0 ; i < 3 ; i++ ) {
		flare->origin[i] = LittleFloat( ds->lightmapOrigin[i] );
		flare->color[i] = LittleFloat( ds->lightmapVecs[0][i] );
		flare->normal[i] = R_ClampDenorm( LittleFloat( ds->lightmapVecs[2][i] ) );
	}
}

/*
=================
R_MergedWidthPoints

returns true if there are grid points merged on a width edge
=================
*/
static int R_MergedWidthPoints( const srfGridMesh_t *grid, int offset ) {
	int i, j;

	for (i = 1; i < grid->width-1; i++) {
		for (j = i + 1; j < grid->width-1; j++) {
			if ( fabs(grid->verts[i + offset].xyz[0] - grid->verts[j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[1] - grid->verts[j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[2] - grid->verts[j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_MergedHeightPoints

returns true if there are grid points merged on a height edge
=================
*/
static int R_MergedHeightPoints( const srfGridMesh_t *grid, int offset ) {
	int i, j;

	for (i = 1; i < grid->height-1; i++) {
		for (j = i + 1; j < grid->height-1; j++) {
			if ( fabs(grid->verts[grid->width * i + offset].xyz[0] - grid->verts[grid->width * j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[1] - grid->verts[grid->width * j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[2] - grid->verts[grid->width * j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_FixSharedVertexLodError_r

NOTE: never sync LoD through grid edges with merged points!

FIXME: write generalized version that also avoids cracks between a patch and one that meets half way?
=================
*/
static void R_FixSharedVertexLodError_r( int start, srfGridMesh_t *grid1, world_t &worldData ) {
	int j, k, l, m, n, offset1, offset2, touch;
	srfGridMesh_t *grid2;

	for ( j = start; j < worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// if the LOD errors are already fixed for this patch
		if ( grid2->lodFixed == 2 ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		touch = qfalse;
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = (grid1->height-1) * grid1->width;
			else offset1 = 0;
			if (R_MergedWidthPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->width-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = grid1->width-1;
			else offset1 = 0;
			if (R_MergedHeightPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->height-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		if (touch) {
			grid2->lodFixed = 2;
			R_FixSharedVertexLodError_r ( start, grid2, worldData );
			//NOTE: this would be correct but makes things really slow
			//grid2->lodFixed = 1;
		}
	}
}

/*
=================
R_FixSharedVertexLodError

This function assumes that all patches in one group are nicely stitched together for the highest LoD.
If this is not the case this function will still do its job but won't fix the highest LoD cracks.
=================
*/
static void R_FixSharedVertexLodError( world_t &worldData ) {
	int i;
	srfGridMesh_t *grid1;

	for ( i = 0; i < worldData.numsurfaces; i++ ) {
		//
		grid1 = (srfGridMesh_t *) worldData.surfaces[i].data;
		// if this surface is not a grid
		if ( grid1->surfaceType != SF_GRID )
			continue;
		//
		if ( grid1->lodFixed )
			continue;
		//
		grid1->lodFixed = 2;
		// recursively fix other patches in the same LOD group
		R_FixSharedVertexLodError_r( i + 1, grid1, worldData);
	}
}

/*
===============
R_StitchPatches
===============
*/
static int R_StitchPatches( int grid1num, int grid2num, world_t &worldData ) {
	int k, l, m, n, offset1, offset2, row, column;
	srfGridMesh_t *grid1, *grid2;
	float *v1, *v2;

	grid1 = (srfGridMesh_t *) worldData.surfaces[grid1num].data;
	grid2 = (srfGridMesh_t *) worldData.surfaces[grid2num].data;
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->width-2; k += 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->height-1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->height-2; k += 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = grid1->width-1; k > 1; k -= 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					if (!grid2)
						break;
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = grid1->height-1; k > 1; k -= 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//vk_debug("found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData.surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
===============
R_TryStitchPatch

This function will try to stitch patches in the same LoD group together for the highest LoD.

Only single missing vertice cracks will be fixed.

Vertices will be joined at the patch side a crack is first found, at the other side
of the patch (on the same row or column) the vertices will not be joined and cracks
might still appear at that side.
===============
*/
static int R_TryStitchingPatch( int grid1num, world_t &worldData ) {
	int j, numstitches;
	srfGridMesh_t *grid1, *grid2;

	numstitches = 0;
	grid1 = (srfGridMesh_t *) worldData.surfaces[grid1num].data;
	for ( j = 0; j < worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		while (R_StitchPatches(grid1num, j, worldData))
		{
			numstitches++;
		}
	}
	return numstitches;
}

/*
===============
R_StitchAllPatches
===============
*/
static void R_StitchAllPatches( world_t &worldData ) {
	int i, stitched, numstitches;
	srfGridMesh_t *grid1;

	numstitches = 0;
	do
	{
		stitched = qfalse;
		for ( i = 0; i < worldData.numsurfaces; i++ ) {
			//
			grid1 = (srfGridMesh_t *) worldData.surfaces[i].data;
			// if this surface is not a grid
			if ( grid1->surfaceType != SF_GRID )
				continue;
			//
			if ( grid1->lodStitched )
				continue;
			//
			grid1->lodStitched = qtrue;
			stitched = qtrue;
			//
			numstitches += R_TryStitchingPatch( i, worldData );
		}
	}
	while (stitched);
//	vk_debug("stitched %d LoD cracks\n", numstitches );
}

/*
===============
R_MovePatchSurfacesToHunk
===============
*/
static void R_MovePatchSurfacesToHunk( world_t &worldData ) {
	int i, j, n, k, size;
	srfGridMesh_t *grid, *hunkgrid;

	for ( i = 0; i < worldData.numsurfaces; i++ ) {
		//
		grid = (srfGridMesh_t *) worldData.surfaces[i].data;
		// if this surface is not a grid
		if ( grid->surfaceType != SF_GRID )
			continue;
		//
		n = grid->width * grid->height - 1;
		size = n * sizeof( drawVert_t ) + sizeof( *grid );

		for (j = 0; j < n; j++) {
			for (k = 0; k < 3; k++) {
				grid->verts[j].normal[k] = R_ClampDenorm( grid->verts[j].normal[k] );
			}
		}
		hunkgrid = (struct srfGridMesh_s *)Hunk_Alloc( size, h_low );
		memcpy(hunkgrid, grid, size);

		hunkgrid->widthLodError = (float *)Hunk_Alloc( grid->width * 4, h_low );
		memcpy( hunkgrid->widthLodError, grid->widthLodError, grid->width * 4 );

		hunkgrid->heightLodError = (float *)Hunk_Alloc( grid->height * 4, h_low );
		memcpy( grid->heightLodError, grid->heightLodError, grid->height * 4 );

		R_FreeSurfaceGridMesh( grid );

		worldData.surfaces[i].data = (surfaceType_t *) hunkgrid;
	}
}

/*
===============
R_LoadSurfaces
===============
*/
static	void R_LoadSurfaces( const lump_t *surfs, const lump_t *verts, const lump_t *indexLump, world_t &worldData, int index ) {
	dsurface_t	*in;
	msurface_t	*out;
	mapVert_t	*dv;
	int			*indexes;
	int			count;
	int			numFaces, numMeshes, numTriSurfs, numFlares;
	int			i;

	numFaces = 0;
	numMeshes = 0;
	numTriSurfs = 0;
	numFlares = 0;

	in = (dsurface_t *)(fileBase + surfs->fileofs);
	if (surfs->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	count = surfs->filelen / sizeof(*in);

	dv = (mapVert_t *)(fileBase + verts->fileofs);
	if (verts->filelen % sizeof(*dv))
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);

	indexes = (int *)(fileBase + indexLump->fileofs);
	if ( indexLump->filelen % sizeof(*indexes))
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);

	out = (struct msurface_s *)Hunk_Alloc ( count * sizeof(*out), h_low );

	worldData.surfaces = out;
	worldData.numsurfaces = count;

	for ( i = 0 ; i < count ; i++, in++, out++ ) {
		switch ( LittleLong( in->surfaceType ) ) {
		case MST_PATCH:
			ParseMesh ( in, dv, out, worldData, index );
			numMeshes++;
			break;
		case MST_TRIANGLE_SOUP:
			ParseTriSurf( in, dv, out, indexes, worldData, index );
			numTriSurfs++;
			break;
		case MST_PLANAR:
			ParseFace( in, dv, out, indexes, worldData, index );
			numFaces++;
			break;
		case MST_FLARE:
			ParseFlare( in, dv, out, indexes, worldData, index );
			numFlares++;
			break;
		default:
			Com_Error( ERR_DROP, "Bad surfaceType" );
		}
	}

	if ( r_patchStitching->integer ) {
		R_StitchAllPatches( worldData );
	}

	R_FixSharedVertexLodError(worldData);

	if ( r_patchStitching->integer ) {
		R_MovePatchSurfacesToHunk( worldData );
	}

	vk_debug("...loaded %d faces, %i meshes, %i trisurfs, %i flares\n", numFaces, numMeshes, numTriSurfs, numFlares );
}

/*
=================
R_LoadSubmodels
=================
*/
static	void R_LoadSubmodels( const lump_t *l, world_t &worldData, int index ) {
	dmodel_t	*in;
	bmodel_t	*out;
	int			i, j, count;

	in = (dmodel_t *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	count = l->filelen / sizeof(*in);

	worldData.bmodels = out = (bmodel_t *)Hunk_Alloc( count * sizeof(*out), h_low );

	for ( i=0 ; i<count ; i++, in++, out++ ) {
		model_t *model;

		model = R_AllocModel();

		assert( model != NULL );			// this should never happen
		if ( model == NULL ) {
			ri.Error(ERR_DROP, "R_LoadSubmodels: R_AllocModel() failed");
		}

		model->type = MOD_BRUSH;
		model->data.bmodel = out;
		if (index)
		{
			Com_sprintf( model->name, sizeof( model->name ), "*%d-%d", index, i );
			model->bspInstance = qtrue;
		}
		else
		{
			Com_sprintf( model->name, sizeof( model->name ), "*%d", i);
		}

		for (j=0 ; j<3 ; j++) {
			out->bounds[0][j] = LittleFloat (in->mins[j]);
			out->bounds[1][j] = LittleFloat (in->maxs[j]);
		}
/*
Ghoul2 Insert Start
*/

		RE_InsertModelIntoHash(model->name, model);
/*
Ghoul2 Insert End
*/
		out->firstSurface = worldData.surfaces + LittleLong( in->firstSurface );
		out->numSurfaces = LittleLong( in->numSurfaces );
	}
}

//==================================================================

/*
=================
R_SetParent
=================
*/
static	void R_SetParent ( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	R_SetParent (node->children[0], node);
	R_SetParent (node->children[1], node);
}

/*
=================
R_LoadNodesAndLeafs
=================
*/
static void R_LoadNodesAndLeafs ( const lump_t *nodeLump, const lump_t *leafLump, world_t &worldData ) {
	int				i, j, p;
	const dnode_t	*in;
	dleaf_t			*inLeaf;
	mnode_t 		*out;
	int				numNodes, numLeafs;

	in = (dnode_t *)(fileBase + nodeLump->fileofs);
	if (nodeLump->filelen % sizeof(dnode_t) ||
		leafLump->filelen % sizeof(dleaf_t) ) {
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	}
	numNodes = nodeLump->filelen / sizeof(dnode_t);
	numLeafs = leafLump->filelen / sizeof(dleaf_t);

	out = (struct mnode_s *)Hunk_Alloc ( (numNodes + numLeafs) * sizeof(*out), h_low);

	worldData.nodes = out;
	worldData.numnodes = numNodes + numLeafs;
	worldData.numDecisionNodes = numNodes;

	// load nodes
	for ( i=0 ; i<numNodes; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleLong (in->mins[j]);
			out->maxs[j] = LittleLong (in->maxs[j]);
		}

		p = LittleLong(in->planeNum);
		out->plane = worldData.planes + p;

		out->contents = CONTENTS_NODE;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = worldData.nodes + p;
			else
				out->children[j] = worldData.nodes + numNodes + (-1 - p);
		}
	}

	// load leafs
	inLeaf = (dleaf_t *)(fileBase + leafLump->fileofs);
	for ( i=0 ; i<numLeafs ; i++, inLeaf++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleLong (inLeaf->mins[j]);
			out->maxs[j] = LittleLong (inLeaf->maxs[j]);
		}

		out->cluster = LittleLong(inLeaf->cluster);
		out->area = LittleLong(inLeaf->area);

		if ( out->cluster >= worldData.numClusters ) {
			worldData.numClusters = out->cluster + 1;
		}

		out->firstmarksurface = worldData.marksurfaces +
			LittleLong(inLeaf->firstLeafSurface);
		out->nummarksurfaces = LittleLong(inLeaf->numLeafSurfaces);
	}

	// chain decendants
	R_SetParent (worldData.nodes, NULL);
}

//=============================================================================

/*
=================
R_LoadShaders
=================
*/
static	void R_LoadShaders( const lump_t *l, world_t &worldData ) {
	int		i, count;
	dshader_t	*in, *out;

	in = (dshader_t *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	count = l->filelen / sizeof(*in);
	out = (dshader_t *)Hunk_Alloc ( count*sizeof(*out), h_low );

	worldData.shaders = out;
	worldData.numShaders = count;

	memcpy( out, in, count*sizeof(*out) );

	for ( i=0 ; i<count ; i++ ) {
		out[i].surfaceFlags = LittleLong( out[i].surfaceFlags );
		out[i].contentFlags = LittleLong( out[i].contentFlags );
	}
}

/*
=================
R_LoadMarksurfaces
=================
*/
static	void R_LoadMarksurfaces ( const lump_t *l, world_t &worldData )
{
	int		i, j, count;
	int		*in;
	msurface_t **out;

	in = (int *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	count = l->filelen / sizeof(*in);
	out = (struct msurface_s **)Hunk_Alloc ( count*sizeof(*out), h_low);

	worldData.marksurfaces = out;
	worldData.nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong(in[i]);
		out[i] = worldData.surfaces + j;
	}
}

/*
=================
R_LoadPlanes
=================
*/
static	void R_LoadPlanes( const lump_t *l, world_t &worldData ) {
	int				i, j;
	cplane_t		*out;
	const dplane_t 	*in;
	int				count;
	int				bits;

	in = (dplane_t *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	count = l->filelen / sizeof(*in);
	out = (struct cplane_s *)Hunk_Alloc ( count*2*sizeof(*out), h_low);

	worldData.planes = out;
	worldData.numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++) {
		bits = 0;
		for (j=0 ; j<3 ; j++) {
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0) {
				bits |= 1<<j;
			}
		}

		out->dist = LittleFloat (in->dist);
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}

/*
=================
R_PreLoadFogs
=================
*/
static void R_PreLoadFogs( const lump_t *l ) {
	if ( l->filelen % sizeof( dfog_t ) ) {
		tr.numFogs = 0;
	} else {
		tr.numFogs = l->filelen / sizeof( dfog_t );
	}
}

/*
=================
R_LoadFogs

=================
*/
static	void R_LoadFogs( const lump_t *l, const lump_t *brushesLump, lump_t *sidesLump, world_t &worldData, int index ) {
	int			i, n;
	fog_t		*out;
	const dfog_t		*fogs;
	const dbrush_t 		*brushes, *brush;
	const dbrushside_t	*sides;
	int			count, brushesCount, sidesCount;
	int			sideNum;
	int			planeNum;
	shader_t	*shader;
	float		d;
	int			firstSide=0;
	int			lightmaps[MAXLIGHTMAPS] = { LIGHTMAP_NONE } ;

	fogs = (dfog_t *)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*fogs)) {
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	}
	count = l->filelen / sizeof(*fogs);

	// create fog strucutres for them
	worldData.numfogs = count + 1;
	worldData.fogs = (fog_t *)Hunk_Alloc ( worldData.numfogs*sizeof(*out), h_low);
	worldData.globalFog = -1;
	out = worldData.fogs + 1;

	// Copy the global fog from the main world into the bsp instance
	if(index)
	{
		if(tr.world && (tr.world->globalFog != -1))
		{
			// Use the nightvision fog slot
			worldData.fogs[worldData.numfogs] = tr.world->fogs[tr.world->globalFog];
			worldData.globalFog = worldData.numfogs;
			worldData.numfogs++;
		}
	}

	if ( !count ) {
		return;
	}

	brushes = (dbrush_t *)(fileBase + brushesLump->fileofs);
	if (brushesLump->filelen % sizeof(*brushes)) {
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	}
	brushesCount = brushesLump->filelen / sizeof(*brushes);

	sides = (dbrushside_t *)(fileBase + sidesLump->fileofs);
	if (sidesLump->filelen % sizeof(*sides)) {
		Com_Error (ERR_DROP, "LoadMap: funny lump size in %s",worldData.name);
	}
	sidesCount = sidesLump->filelen / sizeof(*sides);

	for ( i=0 ; i<count ; i++, fogs++) {
		out->originalBrushNumber = LittleLong( fogs->brushNum );

		if (out->originalBrushNumber == -1)
		{
			out->bounds[0][0] = out->bounds[0][1] = out->bounds[0][2] = MIN_WORLD_COORD;
			out->bounds[1][0] = out->bounds[1][1] = out->bounds[1][2] = MAX_WORLD_COORD;
			firstSide = -1;
			worldData.globalFog = i+1;
		}
		else
		{
			if ( (unsigned)out->originalBrushNumber >= (unsigned)brushesCount ) {
				Com_Error( ERR_DROP, "fog brushNumber out of range" );
			}
			brush = brushes + out->originalBrushNumber;

			firstSide = LittleLong( brush->firstSide );

				if ( (unsigned)firstSide > (unsigned)(sidesCount - 6) ) {
				Com_Error( ERR_DROP, "fog brush sideNumber out of range" );
			}

			// brushes are always sorted with the axial sides first
			sideNum = firstSide + 0;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[0][0] = -worldData.planes[ planeNum ].dist;

			sideNum = firstSide + 1;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[1][0] = worldData.planes[ planeNum ].dist;

			sideNum = firstSide + 2;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[0][1] = -worldData.planes[ planeNum ].dist;

			sideNum = firstSide + 3;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[1][1] = worldData.planes[ planeNum ].dist;

			sideNum = firstSide + 4;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[0][2] = -worldData.planes[ planeNum ].dist;

			sideNum = firstSide + 5;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[1][2] = worldData.planes[ planeNum ].dist;
		}

		// get information from the shader for fog parameters
		shader = R_FindShader( fogs->shader, lightmaps, stylesDefault, qtrue );


		if (!shader->fogParms)
		{//bad shader!!
			assert(shader->fogParms);
			out->parms.color[0] = 1.0f;
			out->parms.color[1] = 0.0f;
			out->parms.color[2] = 0.0f;

			out->parms.depthForOpaque = 250.0f;
		}
		else
		{
			if (r_mapGreyScale->value > 0) {
				float luminance;
				luminance = LUMA(out->parms.color[0], out->parms.color[1], out->parms.color[2]);
				out->parms.color[0] = LERP(out->parms.color[0], luminance, r_mapGreyScale->value);
				out->parms.color[1] = LERP(out->parms.color[1], luminance, r_mapGreyScale->value);
				out->parms.color[2] = LERP(out->parms.color[2], luminance, r_mapGreyScale->value);
			}

			out->parms = *shader->fogParms;
		}

		out->colorInt = ColorBytes4 (	out->parms.color[0] * tr.identityLight,
										out->parms.color[1] * tr.identityLight,
										out->parms.color[2] * tr.identityLight, 1.0 );

		for (n = 0; n < 4; n++)
			out->color[n] = ((out->colorInt >> (n * 8)) & 255) / 255.0f;

		d = out->parms.depthForOpaque < 1 ? 1 : out->parms.depthForOpaque;
		out->tcScale = 1.0f / ( d * 8 );

		// set the gradient vector
		sideNum = LittleLong( fogs->visibleSide );

		if ( sideNum == -1 ) {
			//rww - we need to set this to qtrue for global fog as well
			out->hasSurface = qtrue;
		} else {
			out->hasSurface = qtrue;
			planeNum = LittleLong( sides[ firstSide + sideNum ].planeNum );
			VectorSubtract( vec3_origin, worldData.planes[ planeNum ].normal, out->surface );
			out->surface[3] = -worldData.planes[ planeNum ].dist;
		}

		out++;
	}

}

/*
================
R_LoadLightGrid

================
*/
static void R_LoadLightGrid( const lump_t *l, world_t &worldData ) {
	int		i, j;
	vec3_t	maxs;
	world_t	*w;
	float	*wMins, *wMaxs;

	w = &worldData;

	w->lightGridInverseSize[0] = 1.0 / w->lightGridSize[0];
	w->lightGridInverseSize[1] = 1.0 / w->lightGridSize[1];
	w->lightGridInverseSize[2] = 1.0 / w->lightGridSize[2];

	wMins = w->bmodels[0].bounds[0];
	wMaxs = w->bmodels[0].bounds[1];

	for ( i = 0 ; i < 3 ; i++ ) {
		w->lightGridOrigin[i] = w->lightGridSize[i] * ceil( wMins[i] / w->lightGridSize[i] );
		maxs[i] = w->lightGridSize[i] * floor( wMaxs[i] / w->lightGridSize[i] );
		w->lightGridBounds[i] = (maxs[i] - w->lightGridOrigin[i])/w->lightGridSize[i] + 1;
	}

	int numGridDataElements = l->filelen / sizeof(*w->lightGridData);

	w->lightGridData = (mgrid_t *)Hunk_Alloc( l->filelen, h_low );
	memcpy( w->lightGridData, (void *)(fileBase + l->fileofs), l->filelen );

	// deal with overbright bits
	for ( i = 0 ; i < numGridDataElements ; i++ )
	{
		for(j=0;j<MAXLIGHTMAPS;j++)
		{
			R_ColorShiftLightingBytes(w->lightGridData[i].ambientLight[j], w->lightGridData[i].ambientLight[j], qfalse);
			R_ColorShiftLightingBytes(w->lightGridData[i].directLight[j], w->lightGridData[i].directLight[j], qfalse);
		}
	}
}

/*
================
R_LoadLightGridArray

================
*/
static void R_LoadLightGridArray( const lump_t *l, world_t &worldData ) {
	world_t	*w;
#ifdef Q3_BIG_ENDIAN
	int		i;
#endif

	w = &worldData;

	w->numGridArrayElements = w->lightGridBounds[0] * w->lightGridBounds[1] * w->lightGridBounds[2];

	if ( (unsigned)l->filelen != w->numGridArrayElements * sizeof(*w->lightGridArray) ) {
		vk_debug("WARNING: light grid array mismatch\n" );
		w->lightGridData = NULL;
		return;
	}

	w->lightGridArray = (unsigned short *)Hunk_Alloc( l->filelen, h_low );
	memcpy( w->lightGridArray, (void *)(fileBase + l->fileofs), l->filelen );
#ifdef Q3_BIG_ENDIAN
	for ( i = 0 ; i < w->numGridArrayElements ; i++ ) {
		w->lightGridArray[i] = LittleShort(w->lightGridArray[i]);
	}
#endif
}

/*
================
R_LoadEntities
================
*/
static void R_LoadEntities( const lump_t *l, world_t &worldData ) {
	const char *p, *token, *s;
	char keyname[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];
	world_t	*w;
	float ambient = 1;

	w = &worldData;
	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;

	VectorSet(tr.sunAmbient, 1, 1, 1);
	tr.distanceCull = 6000;//DEFAULT_DISTANCE_CULL;

	p = (char *)(fileBase + l->fileofs);

	// store for reference by the cgame
	w->entityString = (char *)Hunk_Alloc( l->filelen + 1, h_low );
	strcpy( w->entityString, p );
	w->entityParsePoint = w->entityString;

	COM_BeginParseSession ("R_LoadEntities");

	token = COM_ParseExt( &p, qtrue );
	if (!*token || *token != '{') {
		return;
	}

	// only parse the world spawn
	while ( 1 ) {
		// parse key
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(keyname, token, sizeof(keyname));

		// parse value
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(value, token, sizeof(value));

		// check for remapping of shaders for vertex lighting
		s = "vertexremapshader";
		if (!Q_strncmp(keyname, s, strlen(s)) ) {
			char *vs = strchr(value, ';');
			if (!vs) {
				vk_debug("WARNING: no semi colon in vertexshaderremap '%s'\n", value );
				break;
			}
			*vs++ = 0;
			if (r_vertexLight->integer) {
				R_RemapShader(value, s, "0");
			}
			continue;
		}
		// check for remapping of shaders
		s = "remapshader";
		if (!Q_strncmp(keyname, s, strlen(s)) ) {
			char *vs = strchr(value, ';');
			if (!vs) {
				vk_debug("WARNING: no semi colon in shaderremap '%s'\n", value );
				break;
			}
			*vs++ = 0;
			R_RemapShader(value, s, "0");
			continue;
		}
 		if (!Q_stricmp(keyname, "distanceCull")) {
			sscanf(value, "%f", &tr.distanceCull );
			if (r_distanceCull && r_distanceCull->value)
				tr.distanceCull = r_distanceCull->value;
			continue;
		}
		// check for a different grid size
		if (!Q_stricmp(keyname, "gridsize")) {
			sscanf(value, "%f %f %f", &w->lightGridSize[0], &w->lightGridSize[1], &w->lightGridSize[2] );
			continue;
		}
	// find the optional world ambient for arioche
		if (!Q_stricmp(keyname, "_color")) {
			sscanf(value, "%f %f %f", &tr.sunAmbient[0], &tr.sunAmbient[1], &tr.sunAmbient[2] );
			continue;
		}
		if (!Q_stricmp(keyname, "ambient")) {
			sscanf(value, "%f", &ambient);
			continue;
		}
	}
	//both default to 1 so no harm if not present.
	VectorScale( tr.sunAmbient, ambient, tr.sunAmbient);
}

/*
=================
R_GetEntityToken
=================
*/
qboolean R_GetEntityToken( char *buffer, int size ) {
	const char	*s;

	if (size == -1)
	{ //force reset
		s_worldData.entityParsePoint = s_worldData.entityString;
		return qtrue;
	}

	s = COM_Parse( (const char **) &s_worldData.entityParsePoint );
	Q_strncpyz( buffer, s, size );
	if ( !s_worldData.entityParsePoint || !s[0] ) {
		return qfalse;
	} else {
		return qtrue;
	}
}

/*
=================
RE_LoadWorldMap

Called directly from cgame
=================
*/
void RE_LoadWorldMap_Actual( const char *name, world_t &worldData, int index )
{
	dheader_t	*header;
	byte		*buffer;
	byte		*startMarker;

	if ( tr.worldMapLoaded && !index ) {
		Com_Error( ERR_DROP, "ERROR: attempted to redundantly load world map\n" );
	}

	if (!index)
	{
		skyboxportal = 0;

		// set default sun direction to be used if it isn't
		// overridden by a shader
		tr.sunDirection[0] = 0.45f;
		tr.sunDirection[1] = 0.3f;
		tr.sunDirection[2] = 0.9f;

		VectorNormalize( tr.sunDirection );

		tr.worldMapLoaded = qtrue;

		// clear tr.world so if the level fails to load, the next
		// try will not look at the partially loaded version
		tr.world = NULL;
		tr.mapLoading = qtrue;
	}

	// check for cached disk file from the server first...
	//
	if (ri.CM_GetCachedMapDiskImage())
	{
		buffer = (byte *)ri.CM_GetCachedMapDiskImage();
	}
	else
	{
		// still needs loading...
		//
		ri.FS_ReadFile( name, (void **)&buffer );
		if ( !buffer ) {
			Com_Error (ERR_DROP, "RE_LoadWorldMap: %s not found", name);
		}
	}

	memset( &worldData, 0, sizeof( worldData ) );
	Q_strncpyz( worldData.name, name, sizeof( worldData.name ) );
	Q_strncpyz( tr.worldDir, name, sizeof( tr.worldDir ) );
	Q_strncpyz( worldData.baseName, COM_SkipPath( worldData.name ), sizeof( worldData.name ) );

	COM_StripExtension( worldData.baseName, worldData.baseName, sizeof( worldData.baseName ) );
	COM_StripExtension( tr.worldDir, tr.worldDir, sizeof( tr.worldDir ) );

	startMarker = (byte *)Hunk_Alloc(0, h_low);
	c_gridVerts = 0;

	header = (dheader_t *)buffer;
	fileBase = (byte *)header;

	int i = LittleLong (header->version);
	if ( i != BSP_VERSION ) {
		Com_Error (ERR_DROP, "RE_LoadWorldMap: %s has wrong version number (%i should be %i)",
			name, i, BSP_VERSION);
	}

	// swap all the lumps
	for (size_t i=0 ; i<sizeof(dheader_t)/4 ; i++) {
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);
	}

	// load into heap
	R_LoadShaders( &header->lumps[LUMP_SHADERS], worldData );
	R_PreLoadFogs( &header->lumps[LUMP_FOGS] );
	R_LoadLightmaps( &header->lumps[LUMP_LIGHTMAPS], &header->lumps[LUMP_SURFACES], worldData );
	R_LoadPlanes (&header->lumps[LUMP_PLANES], worldData);
	R_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES], worldData, index );
	R_LoadSurfaces( &header->lumps[LUMP_SURFACES], &header->lumps[LUMP_DRAWVERTS], &header->lumps[LUMP_DRAWINDEXES], worldData, index );
	R_LoadMarksurfaces (&header->lumps[LUMP_LEAFSURFACES], worldData);
	R_LoadNodesAndLeafs (&header->lumps[LUMP_NODES], &header->lumps[LUMP_LEAFS], worldData);
	R_LoadSubmodels (&header->lumps[LUMP_MODELS], worldData, index);
	R_LoadVisibility( &header->lumps[LUMP_VISIBILITY], worldData );

#ifdef USE_VBO
	R_BuildWorldVBO(s_worldData.surfaces, s_worldData.numsurfaces);
#endif

	worldData.dataSize = (byte *)Hunk_Alloc(0, h_low) - startMarker;

	if (!index)
	{
		R_LoadEntities( &header->lumps[LUMP_ENTITIES], worldData );
		R_LoadLightGrid( &header->lumps[LUMP_LIGHTGRID], worldData );
		R_LoadLightGridArray( &header->lumps[LUMP_LIGHTARRAY], worldData );

		// only set tr.world now that we know the entire level has loaded properly
		tr.world = &worldData;

		tr.mapLoading = qfalse;
	}

	if (ri.CM_GetCachedMapDiskImage())
	{
		Z_Free( ri.CM_GetCachedMapDiskImage() );
		ri.CM_SetCachedMapDiskImage( NULL );
	}
	else
	{
		ri.FS_FreeFile( buffer );
	}
}

// new wrapper used for convenience to tell z_malloc()-fail recovery code whether it's safe to dump the cached-bsp or not.
//
void RE_LoadWorldMap( const char *name )
{
	ri.CM_SetUsingCache( qtrue );
	RE_LoadWorldMap_Actual( name, s_worldData, 0 );
	ri.CM_SetUsingCache( qfalse );

	vk_set_clearcolor();
}
