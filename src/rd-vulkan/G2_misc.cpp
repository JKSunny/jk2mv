// leave this as first line for PCH reasons...
//

#ifdef assert
#	undef assert
#	define assert
#endif


#ifndef __Q_SHARED_H
	#include "../qcommon/q_shared.h"
#endif

#if !defined(TR_LOCAL_H)
#ifdef DEDICATED
	#include "../renderer/tr_local.h"
#else
	#include "../rd-vulkan/tr_local.h"
#endif
#endif
#include "../rd-common/matcomp.h"

#if !defined(G2_H_INC)
	#include "../ghoul2/G2.h"
#endif

#if !defined (MINIHEAP_H_INC)
	#include "../qcommon/MiniHeap.h"
#endif

#include "../server/server.h"
#include "../ghoul2/G2_local.h"

extern mdxaBone_t		worldMatrix;
extern mdxaBone_t		worldMatrixInv;

class CTraceSurface
{
public:
	int					surfaceNum;
	surfaceInfo_v		&rootSList;
	model_t				*currentModel;
	int					lod;
	vec3_t				rayStart;
	vec3_t				rayEnd;
	CollisionRecord_t	*collRecMap;
	int					entNum;
	int					modelIndex;
	skin_t				*skin;
	shader_t			*cust_shader;
	size_t				*TransformedVertsArray;
	int					traceFlags;
	bool				hitOne;
	float				m_fRadius;


	CTraceSurface(
	int					initsurfaceNum,
	surfaceInfo_v		&initrootSList,
	model_t				*initcurrentModel,
	int					initlod,
	const vec3_t		initrayStart,
	const vec3_t		initrayEnd,
	CollisionRecord_t	*initcollRecMap,
	int					initentNum,
	int					initmodelIndex,
	skin_t				*initskin,
	shader_t			*initcust_shader,
	size_t				*initTransformedVertsArray,
	int					inittraceFlags,
	float				fRadius):

	surfaceNum(initsurfaceNum),
	rootSList(initrootSList),
	currentModel(initcurrentModel),
	lod(initlod),
	collRecMap(initcollRecMap),
	entNum(initentNum),
	modelIndex(initmodelIndex),
	skin(initskin),
	cust_shader(initcust_shader),
	TransformedVertsArray(initTransformedVertsArray),
	traceFlags(inittraceFlags),
	m_fRadius(fRadius)
	{
		VectorCopy(initrayStart, rayStart);
		VectorCopy(initrayEnd, rayEnd);
		hitOne = false;
	}

};

// assorted Ghoul 2 functions.
// list all surfaces associated with a model
void G2_List_Model_Surfaces(const char *fileName)
{
	int			i, x;
	model_t		*mod_m = R_GetModelByHandle(RE_RegisterModel(fileName));
	mdxmSurfHierarchy_t	*surf;
	mdxmHeader_t *mdxm = mod_m->data.glm->header;

	surf = (mdxmSurfHierarchy_t *) ( (byte *)mdxm + mdxm->ofsSurfHierarchy );
	mdxmSurface_t *surface = (mdxmSurface_t *)((byte *)mdxm + mdxm->ofsLODs + sizeof(mdxmLOD_t));

	for ( x = 0 ; x < mdxm->numSurfaces ; x++)
	{
		Com_Printf("Surface %i Name %s\n", x, surf->name);
		if (r_verbose->value)
		{
			Com_Printf("Num Descendants %i\n",  surf->numChildren);
			for (i=0; i<surf->numChildren; i++)
			{
				Com_Printf("Descendant %i\n", surf->childIndexes[i]);
			}
		}
		// find the next surface
		surf = (mdxmSurfHierarchy_t *)( (byte *)surf + (size_t)( &((mdxmSurfHierarchy_t *)0)->childIndexes[ surf->numChildren ] ));
		surface =(mdxmSurface_t *)( (byte *)surface + surface->ofsEnd );
	}

}

// list all bones associated with a model
void G2_List_Model_Bones(const char *fileName, int frame)
{
	int				x, i;
	mdxaSkel_t		*skel;
	mdxaSkelOffsets_t	*offsets;
	model_t			*mod_m = R_GetModelByHandle(RE_RegisterModel(fileName));
	model_t			*mod_a = R_GetModelByHandle(mod_m->data.glm->header->animIndex);
//	mdxaFrame_t		*aframe=0;
//	int				frameSize;
	mdxaHeader_t	*header = mod_a->data.gla;

	// figure out where the offset list is
	offsets = (mdxaSkelOffsets_t *)((byte *)header + sizeof(mdxaHeader_t));

//	frameSize = (int)( &((mdxaFrame_t *)0)->boneIndexes[ header->numBones ] );

//	aframe = (mdxaFrame_t *)((byte *)header + header->ofsFrames + (frame * frameSize));
	// walk each bone and list it's name
	for (x=0; x< header->numBones; x++)
	{
		skel = (mdxaSkel_t *)((byte *)header + sizeof(mdxaHeader_t) + offsets->offsets[x]);
		Com_Printf("Bone %i Name %s\n", x, skel->name);

		Com_Printf("X pos %f, Y pos %f, Z pos %f\n", skel->BasePoseMat.matrix[0][3], skel->BasePoseMat.matrix[1][3], skel->BasePoseMat.matrix[2][3]);

		// if we are in verbose mode give us more details
		if (r_verbose->value)
		{
			Com_Printf("Num Descendants %i\n",  skel->numChildren);
			for (i=0; i<skel->numChildren; i++)
			{
				Com_Printf("Num Descendants %i\n",  skel->numChildren);
			}
		}
	}
}


/************************************************************************************************
 * G2_GetAnimFileName
 *    obtain the .gla filename for a model
 *
 * Input
 *    filename of model
 *
 * Output
 *    true if we successfully obtained a filename, false otherwise
 *
 ************************************************************************************************/
qboolean G2_GetAnimFileName(const char *fileName, char **filename)
{
	// find the model we want
	model_t				*mod = R_GetModelByHandle(RE_RegisterModel(fileName));

	if (mod)
	{
		mdxmHeader_t *mdxm = mod->data.glm->header;
		if (mdxm && mdxm->animName[0] != 0)
		{
			*filename = mdxm->animName;
			return qtrue;
		}
	}
	return qfalse;
}


/////////////////////////////////////////////////////////////////////
//
//	Code for collision detection for models gameside
//
/////////////////////////////////////////////////////////////////////

int G2_DecideTraceLod(CGhoul2Info &ghoul2, int useLod, model_t *mod)
{
	int returnLod = useLod;

   	// if we are overriding the LOD at top level, then we can afford to only check this level of model
   	if (ghoul2.mLodBias > returnLod)
   	{
   		returnLod =  ghoul2.mLodBias;
   	}

	//what about r_lodBias?

	// now ensure that we haven't selected a lod that doesn't exist for this model
	if ( returnLod >= mod->numLods )
 	{
 		returnLod = mod->numLods - 1;
 	}

	return returnLod;
}

#ifdef G2_COLLISION_ENABLED
void R_TransformEachSurface( mdxmSurface_t	*surface, vec3_t scale, CMiniHeap *G2VertSpace, size_t *TransformedVertsArray, mdxaBone_v &bonePtr) {
	int				j, k, pos;
	int				numVerts;
	mdxmVertex_t 	*v;
	float			*TransformedVerts;

	//
	// deform the vertexes by the lerped bones
	//

	// alloc some space for the transformed verts to get put in
	v = (mdxmVertex_t *) ((byte *)surface + surface->ofsVerts);
	numVerts = surface->numVerts;
	mdxmVertexTexCoord_t *pTexCoords = (mdxmVertexTexCoord_t *) &v[numVerts];

	TransformedVerts = (float *)G2VertSpace->MiniHeapAlloc(numVerts * 5 * 4);
	TransformedVertsArray[surface->thisSurfaceIndex] = (size_t)TransformedVerts;
	if (!TransformedVerts)
	{
		Com_Error(ERR_DROP, "Ran out of transform space gameside for Ghoul2 Models.");
	}

	// whip through and actually transform each vertex
	int *piBoneRefs = (int*) ((byte*)surface + surface->ofsBoneReferences);

	// optimisation issue
	if ((scale[0] != 1.0f) || (scale[1] != 1.0f) || (scale[2] != 1.0f))
	{
		for ( j = pos = 0; j < numVerts; j++ )
		{
			vec3_t			tempVert;

			VectorClear( tempVert );

			const int iNumWeights = G2_GetVertWeights( v );
			float fTotalWeight = 0.0f;
			for ( k = 0 ; k < iNumWeights ; k++ )
			{
				int		iBoneIndex	= G2_GetVertBoneIndex( v, k );
				float	fBoneWeight	= G2_GetVertBoneWeight( v, k, fTotalWeight, iNumWeights );

				const mdxaBone_t &bone=bonePtr[piBoneRefs[iBoneIndex]].second;

				tempVert[0] += fBoneWeight * ( DotProduct( bone.matrix[0], v->vertCoords ) + bone.matrix[0][3] );
				tempVert[1] += fBoneWeight * ( DotProduct( bone.matrix[1], v->vertCoords ) + bone.matrix[1][3] );
				tempVert[2] += fBoneWeight * ( DotProduct( bone.matrix[2], v->vertCoords ) + bone.matrix[2][3] );
			}
			// copy tranformed verts into temp space
			TransformedVerts[pos++] = tempVert[0] * scale[0];
			TransformedVerts[pos++] = tempVert[1] * scale[1];
			TransformedVerts[pos++] = tempVert[2] * scale[2];
			// we will need the S & T coors too for hitlocation and hitmaterial stuff
			TransformedVerts[pos++] = pTexCoords[j].texCoords[0];
			TransformedVerts[pos++] = pTexCoords[j].texCoords[1];

			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
		}
	}
	else
	{
	  	for ( j = pos = 0; j < numVerts; j++ )
		{
			vec3_t			tempVert;

			VectorClear( tempVert );

			const int iNumWeights = G2_GetVertWeights( v );
			float fTotalWeight = 0.0f;
			for ( k = 0 ; k < iNumWeights ; k++ )
			{
				int		iBoneIndex	= G2_GetVertBoneIndex( v, k );
				float	fBoneWeight	= G2_GetVertBoneWeight( v, k, fTotalWeight, iNumWeights );

				const mdxaBone_t &bone=bonePtr[piBoneRefs[iBoneIndex]].second;

				tempVert[0] += fBoneWeight * ( DotProduct( bone.matrix[0], v->vertCoords ) + bone.matrix[0][3] );
				tempVert[1] += fBoneWeight * ( DotProduct( bone.matrix[1], v->vertCoords ) + bone.matrix[1][3] );
				tempVert[2] += fBoneWeight * ( DotProduct( bone.matrix[2], v->vertCoords ) + bone.matrix[2][3] );
			}
			// copy tranformed verts into temp space
			TransformedVerts[pos++] = tempVert[0];
			TransformedVerts[pos++] = tempVert[1];
			TransformedVerts[pos++] = tempVert[2];
			// we will need the S & T coors too for hitlocation and hitmaterial stuff
			TransformedVerts[pos++] = pTexCoords[j].texCoords[0];
			TransformedVerts[pos++] = pTexCoords[j].texCoords[1];

			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
		}
	}
}
#else
void R_TransformEachSurface( mdxmSurface_t	*surface, vec3_t scale, CMiniHeap *G2VertSpace, int *TransformedVertsArray, mdxaBone_v &bonePtr) {
	int				 j, k;
	int				numVerts;
	mdxmVertex_t 	*v;
	float			*TransformedVerts;

	//
	// deform the vertexes by the lerped bones
	//

	// alloc some space for the transformed verts to get put in
	TransformedVerts = (float *)G2VertSpace->MiniHeapAlloc(surface->numVerts * 5 * 4);
	TransformedVertsArray[surface->thisSurfaceIndex] = (int)TransformedVerts;
	if (!TransformedVerts)
	{
		Com_Error(ERR_DROP, "Ran out of transform space gameside for Ghoul2 Models. Please See Jake to Make space larger");
	}

	// whip through and actually transform each vertex
	int *piBoneRefs = (int*) ((byte*)surface + surface->ofsBoneReferences);
	numVerts = surface->numVerts;
	v = (mdxmVertex_t *) ((byte *)surface + surface->ofsVerts);
	mdxmVertexTexCoord_t *pTexCoords = (mdxmVertexTexCoord_t *) &v[numVerts];
	// optimisation issue
	if ((scale[0] != 1.0) || (scale[1] != 1.0) || (scale[2] != 1.0))
	{
		for ( j = 0; j < numVerts; j++ )
		{
			vec3_t			tempVert, tempNormal;
//			mdxmWeight_t	*w;

			VectorClear( tempVert );
			VectorClear( tempNormal );
//			w = v->weights;

			const int iNumWeights = G2_GetVertWeights( v );
			float fTotalWeight = 0.0f;
			for ( k = 0 ; k < iNumWeights ; k++ )
			{
				int		iBoneIndex	= G2_GetVertBoneIndex( v, k );
				float	fBoneWeight	= G2_GetVertBoneWeight( v, k, fTotalWeight, iNumWeights );

				//bone = bonePtr + piBoneRefs[w->boneIndex];

				tempVert[0] += fBoneWeight * ( DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[0], v->vertCoords ) + bonePtr[piBoneRefs[iBoneIndex]].second.matrix[0][3] );
				tempVert[1] += fBoneWeight * ( DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[1], v->vertCoords ) + bonePtr[piBoneRefs[iBoneIndex]].second.matrix[1][3] );
				tempVert[2] += fBoneWeight * ( DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[2], v->vertCoords ) + bonePtr[piBoneRefs[iBoneIndex]].second.matrix[2][3] );

				tempNormal[0] += fBoneWeight * DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[0], v->normal );
				tempNormal[1] += fBoneWeight * DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[1], v->normal );
				tempNormal[2] += fBoneWeight * DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[2], v->normal );
			}
			int pos = j * 5;

			// copy tranformed verts into temp space
			TransformedVerts[pos++] = tempVert[0] * scale[0];
			TransformedVerts[pos++] = tempVert[1] * scale[1];
			TransformedVerts[pos++] = tempVert[2] * scale[2];
			// we will need the S & T coors too for hitlocation and hitmaterial stuff
			TransformedVerts[pos++] = pTexCoords[j].texCoords[0];
			TransformedVerts[pos] = pTexCoords[j].texCoords[1];

			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
		}
	}
	else
	{
	  	for ( j = 0; j < numVerts; j++ )
		{
			vec3_t			tempVert, tempNormal;
//			mdxmWeight_t	*w;

			VectorClear( tempVert );
			VectorClear( tempNormal );
//			w = v->weights;

			const int iNumWeights = G2_GetVertWeights( v );
			float fTotalWeight = 0.0f;

			for ( k = 0 ; k < iNumWeights ; k++ )
			{
				int		iBoneIndex	= G2_GetVertBoneIndex( v, k );
				float	fBoneWeight	= G2_GetVertBoneWeight( v, k, fTotalWeight, iNumWeights );

				//bone = bonePtr + piBoneRefs[w->boneIndex];

				tempVert[0] += fBoneWeight * ( DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[0], v->vertCoords ) + bonePtr[piBoneRefs[iBoneIndex]].second.matrix[0][3] );
				tempVert[1] += fBoneWeight * ( DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[1], v->vertCoords ) + bonePtr[piBoneRefs[iBoneIndex]].second.matrix[1][3] );
				tempVert[2] += fBoneWeight * ( DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[2], v->vertCoords ) + bonePtr[piBoneRefs[iBoneIndex]].second.matrix[2][3] );

				tempNormal[0] += fBoneWeight * DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[0], v->normal );
				tempNormal[1] += fBoneWeight * DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[1], v->normal );
				tempNormal[2] += fBoneWeight * DotProduct( bonePtr[piBoneRefs[iBoneIndex]].second.matrix[2], v->normal );
			}
			int pos = j * 5;

			// copy tranformed verts into temp space
			TransformedVerts[pos++] = tempVert[0];
			TransformedVerts[pos++] = tempVert[1];
			TransformedVerts[pos++] = tempVert[2];
			// we will need the S & T coors too for hitlocation and hitmaterial stuff
			TransformedVerts[pos++] = pTexCoords[j].texCoords[0];
			TransformedVerts[pos] = pTexCoords[j].texCoords[1];

			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
		}
	}
}
#endif

void G2_TransformSurfaces(int surfaceNum, surfaceInfo_v &rootSList,
					mdxaBone_v &bonePtr, model_t *currentModel, int lod, vec3_t scale, CMiniHeap *G2VertSpace, size_t *TransformedVertArray, bool secondTimeAround)
{
	int	i;
	// back track and get the surfinfo struct for this surface
	mdxmSurface_t			*surface = (mdxmSurface_t *)G2_FindSurface((void *)currentModel, surfaceNum, lod);
	mdxmHierarchyOffsets_t	*surfIndexes = (mdxmHierarchyOffsets_t *)((byte *)currentModel->data.glm->header + sizeof(mdxmHeader_t));
	mdxmSurfHierarchy_t		*surfInfo = (mdxmSurfHierarchy_t *)((byte *)surfIndexes + surfIndexes->offsets[surface->thisSurfaceIndex]);

	// see if we have an override surface in the surface list
	surfaceInfo_t	*surfOverride = G2_FindOverrideSurface(surfaceNum, rootSList);

	// really, we should use the default flags for this surface unless it's been overriden
	int offFlags = surfInfo->flags;

	if (surfOverride)
	{
		offFlags = surfOverride->offFlags;
	}
	// if this surface is not off, add it to the shader render list
	if (!offFlags)
	{
		// have we already transformed this group of ghoul2 skeletons this frame?
		if (secondTimeAround && (TransformedVertArray[surface->thisSurfaceIndex]))
		{
			return;
		}

		R_TransformEachSurface(surface, scale, G2VertSpace, TransformedVertArray, bonePtr);
	}

	// if we are turning off all descendants, then stop this recursion now
	if (offFlags & G2SURFACEFLAG_NODESCENDANTS)
	{
		return;
	}

	// now recursively call for the children
	for (i=0; i< surfInfo->numChildren; i++)
	{
		G2_TransformSurfaces(surfInfo->childIndexes[i], rootSList, bonePtr, currentModel, lod, scale, G2VertSpace, TransformedVertArray, secondTimeAround);
	}
}

// main calling point for the model transform for collision detection. At this point all of the skeleton has been transformed.
void G2_TransformModel(CGhoul2Info_v &ghoul2, const int frameNum, const vec3_t scale, CMiniHeap *G2VertSpace, int useLod)
{
	int				lod;
	model_t			*currentModel;
	model_t			*animModel;
	mdxmHeader_t	*mdxm;
	mdxaHeader_t	*aHeader;
	vec3_t			correctScale;
	bool			secondTimeAround = false;

	// if we have already done this once, lets go again, and only do those surfaces that might have changed
	if (ghoul2[0].mMeshFrameNum == frameNum)
	{
		secondTimeAround = true;
	}

	VectorCopy(scale, correctScale);
	// check for scales of 0 - that's the default I believe
	if (!scale[0])
	{
		correctScale[0] = 1.0;
	}
	if (!scale[1])
	{
		correctScale[1] = 1.0;
	}
	if (!scale[2])
	{
		correctScale[2] = 1.0;
	}

	// walk each possible model for this entity and try rendering it out
	for (size_t i = 0; i < ghoul2.size(); i++)
	{
		// don't bother with models that we don't care about.
		if (ghoul2[i].mModelindex == -1)
		{
			continue;
		}
		// get the sorted model to play with
		currentModel = R_GetModelByHandle(RE_RegisterModel(ghoul2[i].mFileName));
		animModel =  R_GetModelByHandle(currentModel->data.glm->header->animIndex);
		aHeader = animModel->data.gla;

		// stop us building this model more than once per frame
		ghoul2[i].mMeshFrameNum = frameNum;

		// decide the LOD
		lod = G2_DecideTraceLod(ghoul2[i], useLod, currentModel);

		mdxm = currentModel->data.glm->header;

		// give us space for the transformed vertex array to be put in
		ghoul2[i].mTransformedVertsArray = (size_t*)G2VertSpace->MiniHeapAlloc(mdxm->numSurfaces * sizeof(size_t));
		memset(ghoul2[i].mTransformedVertsArray, 0, (mdxm->numSurfaces * sizeof(size_t)));

		// did we get enough space?
		assert(ghoul2[i].mTransformedVertsArray);

		// recursively call the model surface transform
		G2_TransformSurfaces(ghoul2[i].mSurfaceRoot, ghoul2[i].mSlist, ghoul2[i].mTempBoneList,  currentModel, lod, correctScale, G2VertSpace, ghoul2[i].mTransformedVertsArray, secondTimeAround);
	}
}


// work out how much space a triangle takes
static float	G2_AreaOfTri(const vec3_t A, const vec3_t B, const vec3_t C)
{
	vec3_t	cross, ab, cb;
	VectorSubtract(A, B, ab);
	VectorSubtract(C, B, cb);

	CrossProduct(ab, cb, cross);

	return VectorLength(cross);
}

// actually determine the S and T of the coordinate we hit in a given poly
static void G2_BuildHitPointST( const vec3_t A, const float SA, const float TA,
						 const vec3_t B, const float SB, const float TB,
						 const vec3_t C, const float SC, const float TC,
						 const vec3_t P, float *s, float *t,float &bary_i,float &bary_j)
{
	float	areaABC = G2_AreaOfTri(A, B, C);

	float i = G2_AreaOfTri(P, B, C) / areaABC;
	bary_i=i;
	float j = G2_AreaOfTri(A, P, C) / areaABC;
	bary_j=j;
	float k = G2_AreaOfTri(A, B, P) / areaABC;

	*s = SA * i + SB * j + SC * k;
	*t = TA * i + TB * j + TC * k;

	*s=fmodf(*s, 1);
	if (*s< 0)
	{
		*s+= 1.0f;
	}

	*t=fmodf(*t, 1);
	if (*t< 0)
	{
		*t+= 1.0f;
	}

}


// routine that works out given a ray whether or not it hits a poly
static qboolean G2_SegmentTriangleTest( const vec3_t start, const vec3_t end,
	const vec3_t A, const vec3_t B, const vec3_t C,
	qboolean backFaces,qboolean frontFaces,vec3_t returnedPoint,vec3_t returnedNormal, float *denom)
{
	static const float tiny=1E-10f;
	vec3_t returnedNormalT;

	vec3_t edgeAC;

	VectorSubtract(C, A, edgeAC);
	VectorSubtract(B, A, returnedNormalT);

	CrossProduct(returnedNormalT, edgeAC, returnedNormal);

	vec3_t ray;
	VectorSubtract(end, start, ray);

	*denom=DotProduct(ray, returnedNormal);

	if (fabsf(*denom)<tiny||        // triangle parallel to ray
		(!backFaces && *denom>0)||		// not accepting back faces
		(!frontFaces && *denom<0))		//not accepting front faces
	{
		return qfalse;
	}

	vec3_t toPlane;
	VectorSubtract(A, start, toPlane);

	const float t=DotProduct(toPlane, returnedNormal)/ *denom;

	if (t<0.0f||t>1.0f)
	{
		return qfalse; // off segment
	}

	VectorScale(ray, t, ray);
	VectorAdd(ray, start, returnedPoint);

	vec3_t edgePA;
	VectorSubtract(A, returnedPoint, edgePA);

	vec3_t edgePB;
	VectorSubtract(B, returnedPoint, edgePB);

	vec3_t edgePC;
	VectorSubtract(C, returnedPoint, edgePC);

	vec3_t temp;

	CrossProduct(edgePA, edgePB, temp);
	if (DotProduct(temp, returnedNormal)<0.0f)
	{
		return qfalse; // off triangle
	}

	CrossProduct(edgePC, edgePA, temp);
	if (DotProduct(temp,returnedNormal)<0.0f)
	{
		return qfalse; // off triangle
	}

	CrossProduct(edgePB, edgePC, temp);
	if (DotProduct(temp, returnedNormal)<0.0f)
	{
		return qfalse; // off triangle
	}
	return qtrue;
}

//Sorry for the sloppiness here, this stuff is just hacked together to work from SP
#ifdef G2_COLLISION_ENABLED
struct SVertexTemp
{
	int flags;
	int touch;
	int newindex;
	float tex[2];
	SVertexTemp()
	{
		touch=0;
	}
};
#define MAX_GORE_VERTS (3000)
static SVertexTemp GoreVerts[MAX_GORE_VERTS];
#endif

void TransformAndTranslatePoint_SP (const vec3_t in, vec3_t out, mdxaBone_t *mat)
{

	for (int i=0;i<3;i++)
	{
		out[i]= in[0]*mat->matrix[i][0] + in[1]*mat->matrix[i][1] + in[2]*mat->matrix[i][2] + mat->matrix[i][3];
	}
}

// now we're at poly level, check each model space transformed poly against the model world transfomed ray
static bool G2_RadiusTracePolys( const mdxmSurface_t *surface, const vec3_t rayStart, const vec3_t rayEnd, CollisionRecord_t *collRecMap, int entNum, int modelIndex, const skin_t *skin, const shader_t *cust_shader, const mdxmSurfHierarchy_t *surfInfo, size_t *TransformedVertsArray, int traceFlags, float fRadius)
{
	int		j;
	vec3_t basis1;
	vec3_t basis2;
	vec3_t taxis;
	vec3_t saxis;

	basis2[0]=0.0f;
	basis2[1]=0.0f;
	basis2[2]=1.0f;

	vec3_t v3RayDir;
	VectorSubtract(rayEnd, rayStart, v3RayDir);

	CrossProduct(v3RayDir,basis2,basis1);

	if (DotProduct(basis1,basis1)<.1f)
	{
		basis2[0]=0.0f;
		basis2[1]=1.0f;
		basis2[2]=0.0f;
		CrossProduct(v3RayDir,basis2,basis1);
	}

	CrossProduct(v3RayDir,basis1,basis2);
	// Give me a shot direction not a bunch of zeros :) -Gil
//	assert(DotProduct(basis1,basis1)>.0001f);
//	assert(DotProduct(basis2,basis2)>.0001f);

	VectorNormalize(basis1);
	VectorNormalize(basis2);

	const float c=cosf(0.0f);//theta
	const float s=sinf(0.0f);//theta

	VectorScale(basis1, 0.5f * c / fRadius,taxis);
	VectorMA(taxis,     0.5f * s / fRadius,basis2,taxis);

	VectorScale(basis1,-0.5f * s /fRadius,saxis);
	VectorMA(    saxis, 0.5f * c /fRadius,basis2,saxis);

	const float * const verts = (float *)TransformedVertsArray[surface->thisSurfaceIndex];
	const int numVerts = surface->numVerts;

	int flags=63;
	//rayDir/=lengthSquared(raydir);
	const float f = VectorLengthSquared(v3RayDir);
	v3RayDir[0]/=f;
	v3RayDir[1]/=f;
	v3RayDir[2]/=f;

	for ( j = 0; j < numVerts; j++ )
	{
		const int pos=j*5;
		vec3_t delta;
		delta[0]=verts[pos+0]-rayStart[0];
		delta[1]=verts[pos+1]-rayStart[1];
		delta[2]=verts[pos+2]-rayStart[2];
		const float s=DotProduct(delta,saxis)+0.5f;
		const float t=DotProduct(delta,taxis)+0.5f;
		const float u=DotProduct(delta,v3RayDir);
		int vflags=0;

		if (s>0)
		{
			vflags|=1;
		}
		if (s<1)
		{
			vflags|=2;
		}
		if (t>0)
		{
			vflags|=4;
		}
		if (t<1)
		{
			vflags|=8;
		}
		if (u>0)
		{
			vflags|=16;
		}
		if (u<1)
		{
			vflags|=32;
		}

		vflags=(~vflags);
		flags&=vflags;
		GoreVerts[j].flags=vflags;
	}

	if (flags)
	{
		return false; // completely off the gore splotch  (so presumably hit nothing? -Ste)
	}
	const int numTris = surface->numTriangles;
	const mdxmTriangle_t * const tris = (const mdxmTriangle_t *) ((const byte *)surface + surface->ofsTriangles);

	for ( j = 0; j < numTris; j++ )
	{
		assert(tris[j].indexes[0]>=0&&tris[j].indexes[0]<numVerts);
		assert(tris[j].indexes[1]>=0&&tris[j].indexes[1]<numVerts);
		assert(tris[j].indexes[2]>=0&&tris[j].indexes[2]<numVerts);
		flags=63&
			GoreVerts[tris[j].indexes[0]].flags&
			GoreVerts[tris[j].indexes[1]].flags&
			GoreVerts[tris[j].indexes[2]].flags;
		if (flags)
		{
			continue;
		}
		else
		{
			// we hit a triangle, so init a collision record...
			//
			int i=0;
			for (i=0; i<MAX_G2_COLLISIONS;i++)
			{
				if (collRecMap[i].mEntityNum == -1)
				{
					CollisionRecord_t  	&newCol = collRecMap[i];

					newCol.mPolyIndex = j;
					newCol.mEntityNum = entNum;
					newCol.mSurfaceIndex = surface->thisSurfaceIndex;
					newCol.mModelIndex = modelIndex;
//					if (face>0)
//					{
						newCol.mFlags = G2_FRONTFACE;
//					}
//					else
//					{
//						newCol.mFlags = G2_BACKFACE;
//					}

					//get normal from triangle
					const float *A = &verts[(tris[j].indexes[0] * 5)];
					const float *B = &verts[(tris[j].indexes[1] * 5)];
					const float *C = &verts[(tris[j].indexes[2] * 5)];
					vec3_t normal;
					vec3_t edgeAC, edgeBA;

					VectorSubtract(C, A, edgeAC);
					VectorSubtract(B, A, edgeBA);
					CrossProduct(edgeBA, edgeAC, normal);

					// transform normal (but don't translate) into world angles
					TransformPoint(normal, newCol.mCollisionNormal, &worldMatrix);
					VectorNormalize(newCol.mCollisionNormal);

					newCol.mMaterial = newCol.mLocation = 0;
					// exit now if we should
					if (traceFlags & G2_RETURNONHIT)
					{
						//hitOne = true;
						return true;
					}

					//i don't know the hitPoint, but let's just assume it's the first vert for now...
					const float *hitPoint = A;
					vec3_t			  distVect;

					VectorSubtract(hitPoint, rayStart, distVect);
					newCol.mDistance = VectorLength(distVect);

					// put the hit point back into world space
					TransformAndTranslatePoint_SP(hitPoint, newCol.mCollisionPosition, &worldMatrix);
					newCol.mBarycentricI = newCol.mBarycentricJ = 0.0f;

					break;
				}
			}
			if (i==MAX_G2_COLLISIONS)
			{
				//assert(i!=MAX_G2_COLLISIONS);		// run out of collision record space - happens OFTEN
				//hitOne = true;	//force stop recursion
				return true;	// return true to avoid wasting further time, but no hit will result without a record
			}
		}
	}

	return false;
}

// now we're at poly level, check each model space transformed poly against the model world transfomed ray
bool G2_TracePolys( const mdxmSurface_t *surface, const vec3_t rayStart, const vec3_t rayEnd, CollisionRecord_t *collRecMap, int entNum, int modelIndex, const skin_t *skin, const shader_t *cust_shader, const mdxmSurfHierarchy_t *surfInfo, size_t *TransformedVertsArray, int traceFlags)
{
	int		j, numTris;

	// whip through and actually transform each vertex
	const mdxmTriangle_t *tris = (const mdxmTriangle_t *) ((const byte *)surface + surface->ofsTriangles);
	const float *verts = (float *)TransformedVertsArray[surface->thisSurfaceIndex];
	numTris = surface->numTriangles;

	for ( j = 0; j < numTris; j++ )
	{
		float			face;
		vec3_t	hitPoint, normal;
		// determine actual coords for this triangle
		const float *point1 = &verts[(tris[j].indexes[0] * 5)];
		const float *point2 = &verts[(tris[j].indexes[1] * 5)];
		const float *point3 = &verts[(tris[j].indexes[2] * 5)];
		// did we hit it?
		if (G2_SegmentTriangleTest(rayStart, rayEnd, point1, point2, point3, qtrue, qtrue, hitPoint, normal, &face))
		{
			assert(collRecMap);
			// find space in the collision records for this record
			for (int i=0; i<MAX_G2_COLLISIONS;i++)
			{
				if (collRecMap[i].mEntityNum == -1)
				{
					CollisionRecord_t	&newCol = collRecMap[i];
					float				distance;
					vec3_t				distVect;
					float				x_pos = 0, y_pos = 0;

					newCol.mPolyIndex = j;
					newCol.mEntityNum = entNum;
					newCol.mSurfaceIndex = surface->thisSurfaceIndex;
					newCol.mModelIndex = modelIndex;
					if (face>0)
					{
						newCol.mFlags = G2_FRONTFACE;
					}
					else
					{
						newCol.mFlags = G2_BACKFACE;
					}

					VectorSubtract(hitPoint, rayStart, distVect);
					distance = VectorLength(distVect);

					// put the hit point back into world space
					TransformAndTranslatePoint(hitPoint, newCol.mCollisionPosition, &worldMatrix);

					// transform normal (but don't translate) into world angles
					TransformPoint(normal, newCol.mCollisionNormal, &worldMatrix);
					VectorNormalize(newCol.mCollisionNormal);

					newCol.mMaterial = newCol.mLocation = 0;

					// Determine our location within the texture, and barycentric coordinates
					G2_BuildHitPointST(point1, point1[3], point1[4],
									   point2, point2[3], point2[4],
									   point3, point3[3], point3[4],
									   hitPoint, &x_pos, &y_pos,newCol.mBarycentricI,newCol.mBarycentricJ);


					const shader_t	*shader = 0;
					// now, we know what surface this hit belongs to, we need to go get the shader handle so we can get the correct hit location and hit material info
					if ( cust_shader )
					{
						shader = cust_shader;
					}
					else if ( skin )
					{
						int		j;

						// match the surface name to something in the skin file
						shader = tr.defaultShader;
						for ( j = 0 ; j < skin->numSurfaces ; j++ )
						{
							// the names have both been lowercased
							if ( !strcmp( skin->surfaces[j]->name, surfInfo->name ) )
							{
								shader = skin->surfaces[j]->shader;
								break;
							}
						}
					}
					else
					{
						shader = R_GetShaderByHandle( surfInfo->shaderIndex );
					}

					// do we even care to decide what the hit or location area's are? If we don't have them in the shader there is little point
					if ((shader->hitLocation) || (shader->hitMaterial))
					{
 						// ok, we have a floating point position. - determine location in data we need to look at
						if (shader->hitLocation)
						{
							newCol.mLocation = *(hitMatReg[shader->hitLocation].loc +
												((int)(y_pos * hitMatReg[shader->hitLocation].height) * hitMatReg[shader->hitLocation].width) +
												((int)(x_pos * hitMatReg[shader->hitLocation].width)));
							Com_Printf("G2_TracePolys hit location: %d\n", newCol.mLocation);
						}

						if (shader->hitMaterial)
						{
							newCol.mMaterial = *(hitMatReg[shader->hitMaterial].loc +
												((int)(y_pos * hitMatReg[shader->hitMaterial].height) * hitMatReg[shader->hitMaterial].width) +
												((int)(x_pos * hitMatReg[shader->hitMaterial].width)));
						}
					}
					// now we have constructed that new hit record, store it.
					newCol.mDistance = distance;

					// exit now if we should
					if (traceFlags & G2_RETURNONHIT)
					{
						return true;
					}

					break;
				}
			}
		}
	}
	return false;
}


// look at a surface and then do the trace on each poly
void G2_TraceSurfaces(CTraceSurface &TS)
{
	int	i;
	// back track and get the surfinfo struct for this surface
	const mdxmSurface_t				*surface = (const mdxmSurface_t *)G2_FindSurface((void *)TS.currentModel, TS.surfaceNum, TS.lod);
	const mdxmHierarchyOffsets_t	*surfIndexes = (const mdxmHierarchyOffsets_t *)((byte *)TS.currentModel->data.glm->header + sizeof(mdxmHeader_t));
	const mdxmSurfHierarchy_t		*surfInfo = (const mdxmSurfHierarchy_t *)((const byte *)surfIndexes + surfIndexes->offsets[surface->thisSurfaceIndex]);

	// see if we have an override surface in the surface list
	const surfaceInfo_t	*surfOverride = G2_FindOverrideSurface(TS.surfaceNum, TS.rootSList);

	// don't allow recursion if we've already hit a polygon
	if (TS.hitOne)
	{
		return;
	}

	// really, we should use the default flags for this surface unless it's been overriden
	int offFlags = surfInfo->flags;

	// set the off flags if we have some
	if (surfOverride)
	{
		offFlags = surfOverride->offFlags;
	}

	// if this surface is not off, add it to the shader render list
	if (!offFlags)
	{
#ifdef G2_COLLISION_ENABLED
		if (!(fabsf(TS.m_fRadius) < 0.1f))	// if not a point-trace
		{
			// .. then use radius check
			//
			if (G2_RadiusTracePolys(surface, TS.rayStart, TS.rayEnd, TS.collRecMap, TS.entNum, TS.modelIndex, TS.skin, TS.cust_shader, surfInfo, TS.TransformedVertsArray, TS.traceFlags, TS.m_fRadius) && (TS.traceFlags & G2_RETURNONHIT))
			{
				// ok, we hit one, *and* we want to return instantly because the returnOnHit is set
				// so indicate we've hit one, so other surfaces don't get hit and return
				TS.hitOne = true;
				return;
			}
		}
		else
#endif
		{
			// go away and trace the polys in this surface
			if (G2_TracePolys(surface, TS.rayStart, TS.rayEnd, TS.collRecMap, TS.entNum, TS.modelIndex, TS.skin, TS.cust_shader, surfInfo, TS.TransformedVertsArray, TS.traceFlags) && (TS.traceFlags & G2_RETURNONHIT))
			{
				// ok, we hit one, *and* we want to return instantly because the returnOnHit is set
				// so indicate we've hit one, so other surfaces don't get hit and return
				TS.hitOne = true;
				return;
			}
		}
	}

	// if we are turning off all descendants, then stop this recursion now
	if (offFlags & G2SURFACEFLAG_NODESCENDANTS)
	{
		return;
	}

	// now recursively call for the children
	for (i=0; i< surfInfo->numChildren; i++)
	{
		TS.surfaceNum = surfInfo->childIndexes[i];
		G2_TraceSurfaces(TS);
	}

}

void G2_TraceModels(CGhoul2Info_v &ghoul2, const vec3_t rayStart, const vec3_t rayEnd, CollisionRecord_t *collRecMap, int entNum, int traceFlags, int useLod, float fRadius)
{
	int				lod;
	model_t			*currentModel;
	model_t			*animModel;
	mdxaHeader_t	*aHeader;
	skin_t			*skin;
	shader_t		*cust_shader;

	// walk each possible model for this entity and try tracing against it
	for (size_t i = 0; i < ghoul2.size(); i++)
	{
		// don't bother with models that we don't care about.
		if (ghoul2[i].mModelindex == -1)
		{
			continue;
		}
		// do we really want to collide with this object?
		if (ghoul2[i].mFlags & GHOUL2_NOCOLLIDE)
		{
			continue;
		}

		currentModel = R_GetModelByHandle(RE_RegisterModel(ghoul2[i].mFileName));
		animModel =  R_GetModelByHandle(currentModel->data.glm->header->animIndex);
		aHeader = animModel->data.gla;

				//
		// figure out whether we should be using a custom shader for this model
		//
		if (ghoul2[i].mCustomShader)
		{
			cust_shader = R_GetShaderByHandle(ghoul2[i].mCustomShader );
		}
		else
		{
			cust_shader = NULL;
		}

		// figure out the custom skin thing
		if ( ghoul2[i].mSkin > 0 && ghoul2[i].mSkin < tr.numSkins )
		{
			skin = R_GetSkinByHandle( ghoul2[i].mSkin );
		}
		else
		{
			skin = NULL;
		}
#ifdef G2_COLLISION_ENABLED
		if (collRecMap)
		{
			lod = G2_DecideTraceLod(ghoul2[i],useLod, currentModel);
		}
		else
		{
			lod=useLod;
			if (lod>=currentModel->numLods)
			{
				return;
			}
		}
#else
		lod = G2_DecideTraceLod(ghoul2[i],useLod, currentModel);
#endif

		CTraceSurface TS(ghoul2[i].mSurfaceRoot, ghoul2[i].mSlist,  currentModel, lod, rayStart, rayEnd, collRecMap, entNum, i, skin, cust_shader, ghoul2[i].mTransformedVertsArray, traceFlags, fRadius);
		// start the surface recursion loop
		G2_TraceSurfaces(TS);

		// if we've hit one surface on one model, don't bother doing the rest
		if ((traceFlags & G2_RETURNONHIT) && TS.hitOne)
		{
			break;
		}
	}
}

void TransformPoint (const vec3_t in, vec3_t out, const mdxaBone_t *mat) {
	for (int i=0;i<3;i++)
	{
		out[i]= in[0]*mat->matrix[i][0] + in[1]*mat->matrix[i][1] + in[2]*mat->matrix[i][2];
	}
}

void TransformAndTranslatePoint (const vec3_t in, vec3_t out, const mdxaBone_t *mat) {

	for (int i=0;i<3;i++)
	{
		out[i]= in[0]*mat->matrix[i][0] + in[1]*mat->matrix[i][1] + in[2]*mat->matrix[i][2] + mat->matrix[i][3];
	}
}


// create a matrix using a set of angles
void Create_Matrix(const float *angle, mdxaBone_t *matrix)
{
	vec3_t		axis[3];

	// convert angles to axis
	AnglesToAxis( angle, axis );
	matrix->matrix[0][0] = axis[0][0];
	matrix->matrix[1][0] = axis[0][1];
	matrix->matrix[2][0] = axis[0][2];

	matrix->matrix[0][1] = axis[1][0];
	matrix->matrix[1][1] = axis[1][1];
	matrix->matrix[2][1] = axis[1][2];

	matrix->matrix[0][2] = axis[2][0];
	matrix->matrix[1][2] = axis[2][1];
	matrix->matrix[2][2] = axis[2][2];

	matrix->matrix[0][3] = 0;
	matrix->matrix[1][3] = 0;
	matrix->matrix[2][3] = 0;


}

// given a matrix, generate the inverse of that matrix
void Inverse_Matrix(const mdxaBone_t *src, mdxaBone_t *dest)
{
	int i, j;

	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			dest->matrix[i][j]=src->matrix[j][i];
		}
	}
	for (i = 0; i < 3; i++)
	{
		dest->matrix[i][3]=0;
		for (j = 0; j < 3; j++)
		{
			dest->matrix[i][3]-=dest->matrix[i][j]*src->matrix[j][3];
		}
	}
}

// generate the world matrix for a given set of angles and origin - called from lots of places
void G2_GenerateWorldMatrix(const vec3_t angles, const vec3_t origin)
{
	Create_Matrix(angles, &worldMatrix);
	worldMatrix.matrix[0][3] = origin[0];
	worldMatrix.matrix[1][3] = origin[1];
	worldMatrix.matrix[2][3] = origin[2];

	Inverse_Matrix(&worldMatrix, &worldMatrixInv);
}

// go away and determine what the pointer for a specific surface definition within the model definition is
void *G2_FindSurface(void *mod_t, int index, int lod)
{
	// damn include file dependancies
	model_t	*mod = (model_t *)mod_t;
	mdxmHeader_t *mdxm = mod->data.glm->header;

	// point at first lod list
	byte	*current = (byte*)((size_t)mdxm + (size_t)mdxm->ofsLODs);
	int i;

	//walk the lods
	for (i=0; i<lod; i++)
	{
		mdxmLOD_t *lodData = (mdxmLOD_t *)current;
		current += lodData->ofsEnd;
	}

	// avoid the lod pointer data structure
	current += sizeof(mdxmLOD_t);

	mdxmLODSurfOffset_t *indexes = (mdxmLODSurfOffset_t *)current;
	// we are now looking at the offset array
	current += indexes->offsets[index];

	return (void *)current;
}

#if 0 // todo is this needed?

#define SURFACE_SAVE_BLOCK_SIZE	sizeof(surfaceInfo_t)
#define BOLT_SAVE_BLOCK_SIZE (sizeof(boltInfo_t) - sizeof(mdxaBone_t))
#define BONE_SAVE_BLOCK_SIZE sizeof(boneInfo_t)

qboolean G2_SaveGhoul2Models(CGhoul2Info_v &ghoul2, char **buffer, int *size)
{

	// is there anything to save?
	if (!ghoul2.size())
	{
		*buffer = (char *)Z_Malloc(4, TAG_GHOUL2, qtrue);
		int *tempBuffer = (int *)*buffer;
		*tempBuffer = 0;
		*size = 4;
		return qtrue;
	}

	// yeah, lets get busy
	*size = 0;

	// this one isn't a define since I couldn't work out how to figure it out at compile time
	int ghoul2BlockSize = (int)((size_t)&ghoul2[0].mTransformedVertsArray - (size_t)&ghoul2[0].mModelindex);

	// add in count for number of ghoul2 models
	*size += 4;
	// start out working out the total size of the buffer we need to allocate
	for (size_t i = 0; i < ghoul2.size(); i++)
	{
		*size += ghoul2BlockSize;
		// add in count for number of surfaces
		*size += 4;
		*size += ((int)ghoul2[i].mSlist.size() * SURFACE_SAVE_BLOCK_SIZE);
		// add in count for number of bones
		*size += 4;
		*size += ((int)ghoul2[i].mBlist.size() * BONE_SAVE_BLOCK_SIZE);
		// add in count for number of bolts
		*size += 4;
		*size += ((int)ghoul2[i].mBltlist.size() * BOLT_SAVE_BLOCK_SIZE);
	}

	// ok, we should know how much space we need now
	*buffer = (char*)Z_Malloc(*size, TAG_GHOUL2, qtrue);

	// now lets start putting the data we care about into the buffer
	char *tempBuffer = *buffer;

	// save out how many ghoul2 models we have
	*(int *)tempBuffer = (int)ghoul2.size();
	tempBuffer +=4;

	for (size_t i = 0; i < ghoul2.size(); i++)
	{
		// first save out the ghoul2 details themselves
//		OutputDebugString(va("G2_SaveGhoul2Models(): ghoul2[%d].mModelindex = %d\n",i,ghoul2[i].mModelindex));
		memcpy(tempBuffer, &ghoul2[i].mModelindex, ghoul2BlockSize);
		tempBuffer += ghoul2BlockSize;

		// save out how many surfaces we have
		*(int*)tempBuffer = (int)ghoul2[i].mSlist.size();
		tempBuffer +=4;

		// now save the all the surface list info
		for (size_t x = 0; x < ghoul2[i].mSlist.size(); x++)
		{
			memcpy(tempBuffer, &ghoul2[i].mSlist[x], SURFACE_SAVE_BLOCK_SIZE);
			tempBuffer += SURFACE_SAVE_BLOCK_SIZE;
		}

		// save out how many bones we have
		*(int*)tempBuffer = (int)ghoul2[i].mBlist.size();
		tempBuffer +=4;

		// now save the all the bone list info
		for (size_t x = 0; x < ghoul2[i].mBlist.size(); x++)
		{
			memcpy(tempBuffer, &ghoul2[i].mBlist[x], BONE_SAVE_BLOCK_SIZE);
			tempBuffer += BONE_SAVE_BLOCK_SIZE;
		}

		// save out how many bolts we have
		*(int*)tempBuffer = (int)ghoul2[i].mBltlist.size();
		tempBuffer +=4;

		// lastly save the all the bolt list info
		for (size_t x = 0; x < ghoul2[i].mBltlist.size(); x++)
		{
			memcpy(tempBuffer, &ghoul2[i].mBltlist[x], BOLT_SAVE_BLOCK_SIZE);
			tempBuffer += BOLT_SAVE_BLOCK_SIZE;
		}
	}

	return qtrue;
}

// have to free space malloced in the save system here because the game DLL can't.
void G2_FreeSaveBuffer(char *buffer)
{
	Z_Free(buffer);
}

int G2_FindConfigStringSpace(char *name, int start, int max)
{
	char	s[MAX_STRING_CHARS];
	int i;
	for ( i=1 ; i<max ; i++ )
	{
		SV_GetConfigstring( start + i, s, sizeof( s ) );
		if ( !s[0] )
		{
			break;
		}
		if ( !Q_stricmp( s, name ) )
		{
			return i;
		}
	}

	SV_SetConfigstring(start + i, name);
	return i;
}

void G2_LoadGhoul2Model(CGhoul2Info_v &ghoul2, char *buffer)
{
	// first thing, lets see how many ghoul2 models we have, and resize our buffers accordingly
	int newSize = *(int*)buffer;
	ghoul2.resize(newSize);
	buffer += 4;

	// did we actually resize to a value?
	if (!newSize)
	{
		// no, ok, well, done then.
		return;
	}

	// this one isn't a define since I couldn't work out how to figure it out at compile time
	int ghoul2BlockSize = (int)((size_t)&ghoul2[0].mTransformedVertsArray - (size_t)&ghoul2[0].mModelindex);

	// now we have enough instances, lets go through each one and load up the relevant details
	for (size_t i = 0; i < ghoul2.size(); i++)
	{
		// load the ghoul2 info from the buffer
		memcpy(&ghoul2[i].mModelindex, buffer, ghoul2BlockSize);
//		OutputDebugString(va("G2_LoadGhoul2Model(): ghoul2[%d].mModelindex = %d\n",i,ghoul2[i].mModelindex));
		buffer +=ghoul2BlockSize;

		// now we have to do some building up of stuff so we can register these new models.
		ghoul2[i].mModel = RE_RegisterModel(ghoul2[i].mFileName);
		// we are going to go right ahead and stuff the modelIndex in here, since we know we are only loading and saving on the server. It's a bit naught,
		// but so what?:)
		ghoul2[i].mModelindex = G2_FindConfigStringSpace(ghoul2[i].mFileName, CS_MODELS, MAX_MODELS);
//		OutputDebugString(va("(further on): ghoul2[%d].mModelindex = %d\n",i,ghoul2[i].mModelindex));

		// give us enough surfaces to load up the data
		ghoul2[i].mSlist.resize(*(int*)buffer);
		buffer +=4;

		// now load all the surfaces
		for (size_t x = 0; x < ghoul2[i].mSlist.size(); x++)
		{
			memcpy(&ghoul2[i].mSlist[x], buffer, SURFACE_SAVE_BLOCK_SIZE);
			buffer += SURFACE_SAVE_BLOCK_SIZE;
		}

		// give us enough bones to load up the data
		ghoul2[i].mBlist.resize(*(int*)buffer);
		buffer +=4;

		// now load all the bones
		for (size_t x = 0; x < ghoul2[i].mBlist.size(); x++)
		{
			memcpy(&ghoul2[i].mBlist[x], buffer, BONE_SAVE_BLOCK_SIZE);
			buffer += BONE_SAVE_BLOCK_SIZE;
		}

		// give us enough bolts to load up the data
		ghoul2[i].mBltlist.resize(*(int*)buffer);
		buffer +=4;

		// now load all the bolts
		for (size_t x = 0; x < ghoul2[i].mBltlist.size(); x++)
		{
			memcpy(&ghoul2[i].mBltlist[x], buffer, BOLT_SAVE_BLOCK_SIZE);
			buffer += BOLT_SAVE_BLOCK_SIZE;
		}
	}
}

#endif // 0

void G2_LerpAngles(CGhoul2Info_v &ghoul2,CGhoul2Info_v &nextGhoul2, float interpolation)
{
	// loop each model
	for (size_t i = 0; i < ghoul2.size(); i++)
	{
		if (ghoul2[i].mModelindex != -1)
		{
			// now walk the bone list
			for (size_t x = 0; x < ghoul2[i].mBlist.size(); x++)
			{
				boneInfo_t	&bone = ghoul2[i].mBlist[x];
				// sure we have one to lerp to?
				if ((nextGhoul2.size() > i) &&
					(nextGhoul2[i].mModelindex != -1) &&
					(nextGhoul2[i].mBlist.size() > x) &&
					(nextGhoul2[i].mBlist[x].boneNumber != -1))
				{
					boneInfo_t	&nextBone = nextGhoul2[i].mBlist[x];
					// does this bone override actually have anything in it, and if it does, is it a bone angles override?
					if ((bone.boneNumber != -1) && ((bone.flags) & (BONE_ANGLES_TOTAL)))
					{
						float *nowMatrix = (float*) &bone.matrix;
						float *nextMatrix = (float*) &nextBone.matrix;
						float *newMatrix = (float*) &bone.newMatrix;
						// now interpolate the matrix
						for (int z=0; z < 12; z++)
						{
							newMatrix[z] = nowMatrix[z] + interpolation * ( nextMatrix[z] - nowMatrix[z] );
						}
					}
				}
				else
				{
					memcpy(&ghoul2[i].mBlist[x].newMatrix, &ghoul2[i].mBlist[x].matrix, sizeof(mdxaBone_t));
				}
			}
		}
	}
}
