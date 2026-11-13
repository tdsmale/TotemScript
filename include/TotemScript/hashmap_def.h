//
//  hashmap_def.h
//  TotemScript
//
//  Created by Timothy Smale on 26/09/2022
//  Copyright (c) 2022 Timothy Smale. All rights reserved.
//

#include <TotemScript/memory.h>

#ifndef TOTEM_HASHMAP
#define TOTEM_HASHMAP totemHashMap
TOTEM_HASHMAP_DECLARATION(totemHashMap)
TOTEM_STATIC_ASSERT(0, "Missing hashmap type declaration");
#endif

#ifndef TOTEM_HASHMAP_FREEKEY_FUNC
void TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultFreeKey)(totemHashMapEntry *entry)
{
    
}
#define TOTEM_HASHMAP_FREEKEY_FUNC TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultFreeKey)
TOTEM_STATIC_ASSERT(0, "Missing hashmap free key declaration");
#endif

#ifndef TOTEM_HASHMAP_ENTRYALLOC_FUNC
totemHashMapEntry *TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultAllocEntry)(totemHashValue hash)
{
    return NULL;
}
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultAllocEntry)
TOTEM_STATIC_ASSERT(0, "Missing hashmap alloc entry declaration");
#endif

#ifndef TOTEM_HASHMAP_ENTRYCOPY_FUNC
totemHashMapEntry* TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultCopyEntry)(totemHashValue hash)
{
    return NULL;
}
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultAllocEntry)
TOTEM_STATIC_ASSERT(0, "Missing hashmap alloc copy declaration");
#endif

#ifndef TOTEM_HASHMAP_ENTRYFREE_FUNC
void TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultFreeEntry)(totemHashMapEntry* entry)
{

}
#define TOTEM_HASHMAP_ENTRYFREE_FUNC TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, DefaultFreeEntry)
TOTEM_STATIC_ASSERT(0, "Missing hashmap free entry declaration");
#endif

void TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, Init) (TOTEM_HASHMAP* map)
{
    TOTEM_ZERO(*map);
}

void TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, AddEntryToFreeList)(TOTEM_HASHMAP* hashmap, totemHashMapEntry* entry)
{
    TOTEM_HASHMAP_FREEKEY_FUNC(entry);
    entry->Next = hashmap->FreeList;
    hashmap->FreeList = entry;
}

totemHashMapEntry* TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, AllocFreeEntry)(TOTEM_HASHMAP* map, totemHashValue hash)
{
    totemHashMapEntry* entry = NULL;
    if (map->FreeList)
    {
        entry = map->FreeList;
        map->FreeList = entry->Next;
    }
    else
    {
        entry = TOTEM_HASHMAP_ENTRYALLOC_FUNC(hash);
    }
    
    return entry;
}

totemBool TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, Secure)(TOTEM_HASHMAP* hashmap, size_t num)
{
    size_t needed = hashmap->NumKeys + num;
    if (needed >= hashmap->NumBuckets)
    {
        uint32_t newNumBuckets = 0;
        if (hashmap->NumBuckets == 0)
        {
            newNumBuckets = 32;
        }
        else
        {
            newNumBuckets = hashmap->NumBuckets * 2;
        }

        totemHashMapEntry** newBuckets;
        newBuckets = (totemHashMapEntry**)TOTEM_MALLOC(newNumBuckets * sizeof(totemHashMapEntry*));
        if (newBuckets == NULL)
        {
            return totemBool_False;
        }

        TOTEM_ZERO_N(newBuckets, newNumBuckets);

        for (uint32_t i = 0; i < hashmap->NumBuckets; i++)
        {
            totemHashMapEntry* next = NULL;
            for (totemHashMapEntry* entry = hashmap->Buckets[i]; entry != NULL; entry = next)
            {
                next = entry->Next;
                entry->Next = NULL;
                totemHashMapEntry_Insert(newBuckets, newNumBuckets, entry);
            }
        }
        
        TOTEM_FREE(hashmap->Buckets, hashmap->NumBuckets * sizeof(totemHashMapEntry*));

        hashmap->NumBuckets = newNumBuckets;
        hashmap->Buckets = newBuckets;
    }

    return totemBool_True;
}

void TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, Reset) (TOTEM_HASHMAP* map)
{
    for (uint32_t i = 0; i < map->NumBuckets; i++)
    {
        while (map->Buckets[i])
        {
            totemHashMapEntry* entry = map->Buckets[i];
            map->Buckets[i] = entry->Next;
            TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, AddEntryToFreeList)(map, entry);
        }
    }

    map->NumKeys = 0;
}

void TOTEM_HASHMAP_M_NAME(TOTEM_HASHMAP, Cleanup)(TOTEM_HASHMAP* map)
{
    for (uint32_t i = 0; i < map->NumBuckets; i++)
    {
        while (map->Buckets[i])
        {
            totemHashMapEntry* entry = map->Buckets[i];
            map->Buckets[i] = entry->Next;
            TOTEM_HASHMAP_FREEKEY_FUNC(entry);
            TOTEM_HASHMAP_ENTRYFREE_FUNC(entry);
        }
    }

    while (map->FreeList)
    {
        totemHashMapEntry* entry = map->FreeList;
        map->FreeList = entry->Next;
        TOTEM_HASHMAP_ENTRYFREE_FUNC(entry);
    }

    TOTEM_FREE(map->Buckets, map->NumBuckets * sizeof(*map->Buckets));
    map->Buckets = NULL;
    map->NumBuckets = 0;
    map->NumKeys = 0;
}
