//
//  hashmap_insertdef.h
//  TotemScript
//
//  Created by Timothy Smale on 26/10/2022
//  Copyright (c) 2022 Timothy Smale. All rights reserved.
//

#include <TotemScript/memory.h>

#ifndef TOTEM_HASHMAP
#define TOTEM_HASHMAP totemHashMap
TOTEM_HASHMAP_DECLARATION(totemHashMap);
TOTEM_STATIC_ASSERT(0, "Missing hashmap type declaration");
#endif

#ifndef TOTEM_HASHMAP_SUFFIX
#define TOTEM_HASHMAP_SUFFIX
TOTEM_STATIC_ASSERT(0, "Mising hashmap search suffix");
#endif

#ifndef TOTEM_HASHMAP_KEY
#define TOTEM_HASHMAP_KEY void
TOTEM_STATIC_ASSERT(0, "Missing hashmap key declaration");
#endif

#ifndef TOTEM_HASHMAP_KEYHASH_FUNC
totemHashValue TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultHash, TOTEM_HASHMAP_SUFFIX)(totemHashMapEntry* entry, TOTEM_HASHMAP_KEY* key)
{
    return 0;
}
#define TOTEM_HASHMAP_KEYHASH_FUNC TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultHash, TOTEM_HASHMAP_SUFFIX)
TOTEM_STATIC_ASSERT(0, "Missing hashmap key hash declaration");
#endif

#ifndef TOTEM_HASHMAP_KEYCOMPARE_FUNC
totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultCompare, TOTEM_HASHMAP_SUFFIX)(totemHashMapEntry* entry, TOTEM_HASHMAP_KEY* key)
{
    return totemBool_False;
}
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultCompare, TOTEM_HASHMAP_SUFFIX)
TOTEM_STATIC_ASSERT(0, "Missing hashmap key compare declaration");
#endif

#ifndef TOTEM_HASHMAP_KEYFETCH_FUNC
void TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultFetch, TOTEM_HASHMAP_SUFFIX)(totemHashMapEntry* entry, TOTEM_HASHMAP_KEY* key)
{

}
#define TOTEM_HASHMAP_KEYFETCH_FUNC TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultFetch, TOTEM_HASHMAP_SUFFIX)
TOTEM_STATIC_ASSERT(0, "Missing hashmap key fetch declaration");
#endif

#ifndef TOTEM_HASHMAP_KEYSTORE_FUNC
totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultStore, TOTEM_HASHMAP_SUFFIX)(totemHashMapEntry* entry, TOTEM_HASHMAP_KEY* key)
{

}
#define TOTEM_HASHMAP_KEYSTORE_FUNC TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, DefaultStore, TOTEM_HASHMAP_SUFFIX)
TOTEM_STATIC_ASSERT(0, "Missing hashmap key store declaration");
#endif

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertNoSearchPrecomputed, TOTEM_HASHMAP_SUFFIX) (TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key, totemHashValue hash)
{
    if (!TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, Secure)(hashmap, 1))
    {
        return totemBool_False;
    }

    totemHashMapEntry* entry = TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, AllocFreeEntry)(hashmap, hash);
    if (!entry)
    {
        return totemBool_False;
    }

    TOTEM_HASHMAP_KEYSTORE_FUNC(entry, key);
    totemHashMapEntry_Insert(hashmap->Buckets, hashmap->NumBuckets, entry);
    hashmap->NumKeys++;
    return totemBool_True;
}

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertNoSearch, TOTEM_HASHMAP_SUFFIX) (TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key)
{
    totemHashValue hash = TOTEM_HASHMAP_KEYHASH_FUNC(key);
    return TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertNoSearchPrecomputed, TOTEM_HASHMAP_SUFFIX)(hashmap, key, hash);
}

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertPrecomputed, TOTEM_HASHMAP_SUFFIX)(TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key, totemHashValue hash)
{
    if (hashmap->NumBuckets > 0)
    {
        int index = hash % hashmap->NumBuckets;
        for (totemHashMapEntry* entry = hashmap->Buckets[index]; entry != NULL; entry = entry->Next)
        {
            if (entry->Hash == hash && TOTEM_HASHMAP_KEYCOMPARE_FUNC(entry, key))
            {
                return totemBool_True;
            }
        }
    }

    return TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertNoSearchPrecomputed, TOTEM_HASHMAP_SUFFIX)(hashmap, key, hash);
}

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, Insert, TOTEM_HASHMAP_SUFFIX) (TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key)
{
    totemHashValue hash = TOTEM_HASHMAP_KEYHASH_FUNC(key);
    return TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertPrecomputed, TOTEM_HASHMAP_SUFFIX)(hashmap, key, hash);
}

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertGetPrecomputed, TOTEM_HASHMAP_SUFFIX)(TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key, totemHashValue hash)
{
    if (hashmap->NumBuckets > 0)
    {
        int index = hash % hashmap->NumBuckets;
        for (totemHashMapEntry* entry = hashmap->Buckets[index]; entry != NULL; entry = entry->Next)
        {
            if (entry->Hash == hash && TOTEM_HASHMAP_KEYCOMPARE_FUNC(entry, key))
            {
                TOTEM_HASHMAP_KEYFETCH_FUNC(entry, key);
                return totemBool_True;
            }
        }
    }

    return TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertNoSearchPrecomputed, TOTEM_HASHMAP_SUFFIX)(hashmap, key, hash);
}

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertGet, TOTEM_HASHMAP_SUFFIX) (TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key)
{
    totemHashValue hash = TOTEM_HASHMAP_KEYHASH_FUNC(key);
    return TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertGetPrecomputed, TOTEM_HASHMAP_SUFFIX)(hashmap, key, hash);
}

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertOverwritePrecomputed, TOTEM_HASHMAP_SUFFIX)(TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key, totemHashValue hash)
{
    if (hashmap->NumBuckets > 0)
    {
        int index = hash % hashmap->NumBuckets;
        for (totemHashMapEntry* entry = hashmap->Buckets[index]; entry != NULL; entry = entry->Next)
        {
            if (entry->Hash == hash && TOTEM_HASHMAP_KEYCOMPARE_FUNC(entry, key))
            {
                TOTEM_HASHMAP_KEYSTORE_FUNC(entry, key);
                return totemBool_True;
            }
        }
    }

    return TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertNoSearchPrecomputed, TOTEM_HASHMAP_SUFFIX)(hashmap, key, hash);
}

totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertOverwrite, TOTEM_HASHMAP_SUFFIX) (TOTEM_HASHMAP* hashmap, TOTEM_HASHMAP_KEY* key)
{
    totemHashValue hash = TOTEM_HASHMAP_KEYHASH_FUNC(key);
    return TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, InsertOverwritePrecomputed, TOTEM_HASHMAP_SUFFIX)(hashmap, key, hash);
}
