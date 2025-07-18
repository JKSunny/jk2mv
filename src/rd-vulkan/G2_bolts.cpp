// leave this as first line for PCH reasons...
//



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

#if !defined(G2_H_INC)
	#include "../ghoul2/G2.h"
#endif
	#include "../ghoul2/G2_local.h"

#ifdef assert
#	undef assert
#	define assert(x)
#endif

//=====================================================================================================================
// Bolt List handling routines - so entities can attach themselves to any part of the model in question

// Given a bone number, see if that bone is already in our bone list
int G2_Find_Bolt_Bone_Num(boltInfo_v &bltlist, const int boneNum)
{
	// look through entire list
	for(size_t i = 0; i < bltlist.size(); i++)
	{
		// if this bone entry has no info in it, bounce over it
		if (bltlist[i].boneNumber == -1)
		{
			continue;
		}

		if (bltlist[i].boneNumber == boneNum)
		{
			return i;
		}
	}

	// didn't find it
	return -1;
}

// Given a bone number, see if that surface is already in our surfacelist list
int G2_Find_Bolt_Surface_Num(boltInfo_v &bltlist, const int surfaceNum, const int flags)
{
	// look through entire list
	for(size_t i = 0; i < bltlist.size(); i++)
	{
		// if this bone entry has no info in it, bounce over it
		if (bltlist[i].surfaceNumber == -1)
		{
			continue;
		}

		if ((bltlist[i].surfaceNumber == surfaceNum) && ((bltlist[i].surfaceType & flags) == flags))
		{
			return i;
		}
	}

	// didn't find it
	return -1;
}

//=========================================================================================
//// Public Bolt Routines
int G2_Add_Bolt_Surf_Num(const char *fileName, boltInfo_v &bltlist, surfaceInfo_v &slist, const int surfNum)
{
	boltInfo_t			tempBolt;

	// first up, make sure have a surface first
	if ((unsigned)surfNum >= slist.size())
	{
		return -1;
	}

	 // look through entire list - see if it's already there first
	for(size_t i = 0; i < bltlist.size(); i++)
	{
		// already there??
		if (bltlist[i].surfaceNumber == surfNum)
		{
			// increment the usage count
			bltlist[i].boltUsed++;
			return i;
		}
	}

	// we have a surface
	// look through entire list - see if it's already there first
	for(size_t i = 0; i < bltlist.size(); i++)
	{
		// if this surface entry has info in it, bounce over it
		if (bltlist[i].boneNumber == -1 && bltlist[i].surfaceNumber == -1)
		{
			// if we found an entry that had a -1 for the bone / surface number, then we hit a surface / bone slot that was empty
			bltlist[i].surfaceNumber = surfNum;
			bltlist[i].surfaceType = G2SURFACEFLAG_GENERATED;
			bltlist[i].boltUsed = 1;
			return i;
		}
	}

	// ok, we didn't find an existing surface of that name, or an empty slot. Lets add an entry
	tempBolt.surfaceNumber = surfNum;
	tempBolt.surfaceType = G2SURFACEFLAG_GENERATED;
	tempBolt.boneNumber = -1;
	tempBolt.boltUsed = 1;
	bltlist.push_back(tempBolt);
	return (int)bltlist.size()-1;

}

int G2_Add_Bolt(const char *fileName, boltInfo_v &bltlist, surfaceInfo_v &slist, const char *boneName)
{
	model_t		*mod_m = R_GetModelByHandle(RE_RegisterModel(fileName));
	mdxmHeader_t *mdxm = mod_m->data.glm->header;
	model_t		*mod_a = R_GetModelByHandle(mdxm->animIndex);
	int					x, surfNum = -1;
	mdxaSkel_t			*skel;
	mdxaSkelOffsets_t	*offsets;
	mdxmHierarchyOffsets_t	*surfOffsets;
	boltInfo_t			tempBolt;
	int					flags;

	surfOffsets = (mdxmHierarchyOffsets_t *)((byte*)mdxm + sizeof(mdxmHeader_t));
	// first up, we'll search for that which this bolt names in all the surfaces
	surfNum = G2_IsSurfaceLegal((void*)mod_m, boneName, &flags);

	// did we find it as a surface?
	if (surfNum != -1)
	{
		 // look through entire list - see if it's already there first
		for(size_t i = 0; i < bltlist.size(); i++)
		{
			// already there??
			if (bltlist[i].surfaceNumber == surfNum)
			{
				// increment the usage count
				bltlist[i].boltUsed++;
				return i;
			}
		}

		 // look through entire list - see if we can re-use one
		for(size_t i = 0; i < bltlist.size(); i++)
		{
			// if this surface entry has info in it, bounce over it
		  	if (bltlist[i].boneNumber == -1 && bltlist[i].surfaceNumber == -1)
			{
				// if we found an entry that had a -1 for the bone / surface number, then we hit a surface / bone slot that was empty
				bltlist[i].surfaceNumber = surfNum;
				bltlist[i].boltUsed = 1;
				bltlist[i].surfaceType = 0;
		 		return i;
			}
		}

		// ok, we didn't find an existing surface of that name, or an empty slot. Lets add an entry
		tempBolt.surfaceNumber = surfNum;
		tempBolt.boneNumber = -1;
		tempBolt.boltUsed = 1;
		tempBolt.surfaceType = 0;
		bltlist.push_back(tempBolt);
		return (int)bltlist.size()-1;
	}

	// no, check to see if it's a bone then

	mdxaHeader_t *mdxa = mod_a->data.gla;
   	offsets = (mdxaSkelOffsets_t *)((byte *)mdxa + sizeof(mdxaHeader_t));

 	// walk the entire list of bones in the gla file for this model and see if any match the name of the bone we want to find
 	for (x=0; x< mdxa->numBones; x++)
 	{
 		skel = (mdxaSkel_t *)((byte *)mdxa + sizeof(mdxaHeader_t) + offsets->offsets[x]);
 		// if name is the same, we found it
 		if (!Q_stricmp(skel->name, boneName))
		{
			break;
		}
	}

	// check to see we did actually make a match with a bone in the model
	if (x == mdxa->numBones)
	{
		// didn't find it? Error
		assert(0&&x == mod_a->mdxa->numBones);
		return -1;
	}

	// look through entire list - see if it's already there first
	for(size_t i = 0; i < bltlist.size(); i++)
	{
		// already there??
		if (bltlist[i].boneNumber == x)
		{
			// increment the usage count
			bltlist[i].boltUsed++;
			return i;
		}
	}

	// look through entire list - see if we can re-use it
	for(size_t i = 0; i < bltlist.size(); i++)
	{
		// if this bone entry has info in it, bounce over it
		if (bltlist[i].boneNumber == -1 && bltlist[i].surfaceNumber == -1)
		{
			// if we found an entry that had a -1 for the bonenumber, then we hit a bone slot that was empty
			bltlist[i].boneNumber = x;
			bltlist[i].boltUsed = 1;
			bltlist[i].surfaceType = 0;
	 		return i;
		}
	}

	// ok, we didn't find an existing bone of that name, or an empty slot. Lets add an entry
	tempBolt.boneNumber = x;
	tempBolt.surfaceNumber = -1;
	tempBolt.boltUsed = 1;
	tempBolt.surfaceType = 0;
	bltlist.push_back(tempBolt);
	return (int)bltlist.size()-1;

}

// Given a model handle, and a bone name, we want to remove this bone from the bone override list
qboolean G2_Remove_Bolt (boltInfo_v &bltlist, int index)
{
	// did we find it?
	if (index != -1)
	{
		bltlist[index].boltUsed--;
		if (!bltlist[index].boltUsed)
		{
			// set this bone to not used
			bltlist[index].boneNumber = -1;
			bltlist[index].surfaceNumber = -1;

			int newSize = (int)bltlist.size();
			// now look through the list from the back and see if there is a block of -1's we can resize off the end of the list
			for (int i=(int)bltlist.size()-1; i>-1; i--)
			{
				if ((bltlist[i].surfaceNumber == -1) && (bltlist[i].boneNumber == -1))
				{
					newSize = i;
				}
				// once we hit one that isn't a -1, we are done.
				else
				{
					break;
				}
			}
			// do we need to resize?
			if (newSize != (int)bltlist.size())
			{
				// yes, so lets do it
				bltlist.resize(newSize);
			}

		}
		return qtrue;
	}

	assert(0);

	// no
	return qfalse;
}

// set the bolt list to all unused so the bone transformation routine ignores it.
void G2_Init_Bolt_List(boltInfo_v &bltlist)
{
	bltlist.clear();
}

// remove any bolts that reference original surfaces, generated surfaces, or bones that aren't active anymore
void G2_RemoveRedundantBolts(boltInfo_v &bltlist, surfaceInfo_v &slist, int *activeSurfaces, int *activeBones)
{
	// walk the bolt list
	for (size_t i = 0; i < bltlist.size(); i++)
	{
		// are we using this bolt?
		if ((bltlist[i].surfaceNumber != -1) || (bltlist[i].boneNumber != -1))
		{
			// is this referenceing a surface?
			if (bltlist[i].surfaceNumber != -1)
			{
				// is this bolt looking at a generated surface?
				if (bltlist[i].surfaceType)
				{
					// yes, so look for it in the surface list
					if (!G2_FindOverrideSurface(bltlist[i].surfaceNumber, slist))
					{
						// no - we want to remove this bolt, regardless of how many people are using it
						bltlist[i].boltUsed = 1;
						G2_Remove_Bolt(bltlist, i);
					}
				}
				// no, it's an original, so look for it in the active surfaces list
				{
					if (!activeSurfaces[bltlist[i].surfaceNumber])
					{
						// no - we want to remove this bolt, regardless of how many people are using it
						bltlist[i].boltUsed = 1;
						G2_Remove_Bolt(bltlist, i);
					}
				}
			}
			// no, must be looking at a bone then
			else
			{
				// is that bone active then?
				if (!activeBones[bltlist[i].boneNumber])
				{
					// no - we want to remove this bolt, regardless of how many people are using it
					bltlist[i].boltUsed = 1;
					G2_Remove_Bolt(bltlist, i);
				}
			}
		}
	}
}
