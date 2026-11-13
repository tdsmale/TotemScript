//
//  eval_memory.c
//  TotemScript
//
//  Created by Timothy Smale on 26/09/2022
//  Copyright (c) 2022 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>

totemHashMapEntry* totemEvalStrHashMap_AllocEntry(totemHashValue hash)
{
    totemHashMapEntry *entry = (totemHashMapEntry*)TOTEM_MALLOC(sizeof(totemEvalStrHashMapEntry));
    if (entry)
    {
        entry->Hash = hash;
    }

    return entry;
}

void totemEvalStrHashMap_CopyEntry(totemHashMapEntry* from, totemHashMapEntry* to)
{
    totemEvalStrHashMapEntry* e1 = (totemEvalStrHashMapEntry*)from;
    totemEvalStrHashMapEntry* e2 = (totemEvalStrHashMapEntry*)to;
    e2->Data = e1->Data;
}

void totemEvalStrHashMap_FreeEntry(totemHashMapEntry* entry)
{
    TOTEM_FREE(entry, sizeof(totemEvalStrHashMapEntry));
}

void totemEvalStrHashMap_FreeKey(totemHashMapEntry* entry)
{

}

totemHashValue totemEvalStrHashMap_HashKey(totemEvalStrHashMapKey* key)
{
    return totemParseString_Hash(key->Key);
}

void totemEvalStrHashMap_StoreKey(totemHashMapEntry* entry, totemEvalStrHashMapKey* key)
{
    totemEvalStrHashMapEntry* e = (totemEvalStrHashMapEntry*)entry;
    e->Data = *key;
}

void totemEvalStrHashMap_FetchKey(totemHashMapEntry* entry, totemEvalStrHashMapKey* key)
{
    totemEvalStrHashMapEntry* e = (totemEvalStrHashMapEntry*)entry;
    key->Value = e->Data.Value;
}

totemBool totemEvalStrHashMap_CompareKey(totemHashMapEntry* entry, totemEvalStrHashMapKey* key)
{
    totemEvalStrHashMapEntry* e = (totemEvalStrHashMapEntry*)entry;
    return totemParseString_Compare(e->Data.Key, key->Key);
}

#define TOTEM_HASHMAP totemEvalStrHashMap
#define TOTEM_HASHMAP_FREEKEY_FUNC totemEvalStrHashMap_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemEvalStrHashMap_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemEvalStrHashMap_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemEvalStrHashMap_FreeEntry

#define TOTEM_HASHMAP_SUFFIX TOTEM_NONE
#define TOTEM_HASHMAP_KEY totemEvalStrHashMapKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemEvalStrHashMap_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemEvalStrHashMap_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemEvalStrHashMap_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemEvalStrHashMap_StoreKey

#include <TotemScript/hashmap_def.h>
#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

#undef TOTEM_HASHMAP_ENTRYFREE_FUNC
#undef TOTEM_HASHMAP_ENTRYCOPY_FUNC
#undef TOTEM_HASHMAP_ENTRYALLOC_FUNC
#undef TOTEM_HASHMAP_FREEKEY_FUNC
#undef TOTEM_HASHMAP

void totemEvalOperandHashMap_FreeKey(totemHashMapEntry *entry)
{

}

totemHashMapEntry* totemEvalOperandHashMap_AllocEntry(totemHashValue hash)
{
    totemHashMapEntry* entry = (totemHashMapEntry*)TOTEM_MALLOC(sizeof(totemEvalOperandHashMapEntry));
    if (entry)
    {
        entry->Hash = hash;
    }

    return entry;
}

void totemEvalOperandHashMap_CopyEntry(totemHashMapEntry* from, totemHashMapEntry* to)
{
    totemEvalOperandHashMapEntry* e1 = (totemEvalOperandHashMapEntry*)from;
    totemEvalOperandHashMapEntry* e2 = (totemEvalOperandHashMapEntry*)to;
    e2->Data = e1->Data;
}

void totemEvalOperandHashMap_FreeEntry(totemHashMapEntry* entry)
{
    TOTEM_FREE(entry, sizeof(totemEvalOperandHashMapEntry));
}

totemHashValue totemEvalOperandHashMap_HashKey(totemEvalOperandHashMapKey* key)
{
    return totem_Hash(&key->Key, sizeof(key->Key));
}

totemBool totemEvalOperandHashMap_CompareKey(totemHashMapEntry* entry, totemEvalOperandHashMapKey *key)
{
    totemEvalOperandHashMapEntry* e = (totemEvalOperandHashMapEntry*)entry;
    return e->Data.Key == key->Key;
}

void totemEvalOperandHashMap_FetchKey(totemHashMapEntry* entry, totemEvalOperandHashMapKey* valueOut)
{
    totemEvalOperandHashMapEntry* e = (totemEvalOperandHashMapEntry*)entry;
    *valueOut = e->Data;
}

void totemEvalOperandHashMap_StoreKey(totemHashMapEntry* entry, totemEvalOperandHashMapKey* value)
{
    totemEvalOperandHashMapEntry* e = (totemEvalOperandHashMapEntry*)entry;
    e->Data = *value;
}

#define TOTEM_HASHMAP totemEvalOperandHashMap
#define TOTEM_HASHMAP_FREEKEY_FUNC totemEvalOperandHashMap_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemEvalOperandHashMap_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemEvalOperandHashMap_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemEvalOperandHashMap_FreeEntry

#define TOTEM_HASHMAP_SUFFIX TOTEM_NONE
#define TOTEM_HASHMAP_KEY totemEvalOperandHashMapKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemEvalOperandHashMap_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemEvalOperandHashMap_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemEvalOperandHashMap_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemEvalOperandHashMap_StoreKey

#include <TotemScript/hashmap_def.h>
#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

#undef TOTEM_HASHMAP_ENTRYFREE_FUNC
#undef TOTEM_HASHMAP_ENTRYCOPY_FUNC
#undef TOTEM_HASHMAP_ENTRYALLOC_FUNC
#undef TOTEM_HASHMAP_FREEKEY_FUNC
#undef TOTEM_HASHMAP

void totemEvalFloatHashMap_FreeKey(totemHashMapEntry *entry)
{

}

totemHashMapEntry* totemEvalFloatHashMap_AllocEntry(totemHashValue hash)
{
    totemHashMapEntry* entry = (totemHashMapEntry*)TOTEM_MALLOC(sizeof(totemEvalFloatHashMapEntry));
    if (entry)
    {
        entry->Hash = hash;
    }

    return entry;
}

void totemEvalFloatHashMap_CopyEntry(totemHashMapEntry* from, totemHashMapEntry* to)
{
    totemEvalFloatHashMapEntry* e1 = (totemEvalFloatHashMapEntry*)from;
    totemEvalFloatHashMapEntry* e2 = (totemEvalFloatHashMapEntry*)to;
    e2->Data = e1->Data;
}

void totemEvalFloatHashMap_FreeEntry(totemHashMapEntry* entry)
{
    TOTEM_FREE(entry, sizeof(totemEvalFloatHashMapEntry));
}

totemHashValue totemEvalFloatHashMap_HashKey(totemEvalFloatHashMapKey* key)
{
    return totem_Hash(&key->Key, sizeof(key->Key));
}

totemBool totemEvalFloatHashMap_CompareKey(totemHashMapEntry* entry, totemEvalFloatHashMapKey *key)
{
    totemEvalFloatHashMapEntry* e = (totemEvalFloatHashMapEntry*)entry;
    return e->Data.Key == key->Key;
}

void totemEvalFloatHashMap_FetchKey(totemHashMapEntry* entry, totemEvalFloatHashMapKey* valueOut)
{
    totemEvalFloatHashMapEntry* e = (totemEvalFloatHashMapEntry*)entry;
    *valueOut = e->Data;
}

void totemEvalFloatHashMap_StoreKey(totemHashMapEntry* entry, totemEvalFloatHashMapKey* value)
{
    totemEvalFloatHashMapEntry* e = (totemEvalFloatHashMapEntry*)entry;
    e->Data = *value;
}

#define TOTEM_HASHMAP totemEvalFloatHashMap
#define TOTEM_HASHMAP_FREEKEY_FUNC totemEvalFloatHashMap_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemEvalFloatHashMap_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemEvalFloatHashMap_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemEvalFloatHashMap_FreeEntry

#define TOTEM_HASHMAP_SUFFIX TOTEM_NONE
#define TOTEM_HASHMAP_KEY totemEvalFloatHashMapKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemEvalFloatHashMap_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemEvalFloatHashMap_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemEvalFloatHashMap_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemEvalFloatHashMap_StoreKey

#include <TotemScript/hashmap_def.h>
#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

#undef TOTEM_HASHMAP_ENTRYFREE_FUNC
#undef TOTEM_HASHMAP_ENTRYCOPY_FUNC
#undef TOTEM_HASHMAP_ENTRYALLOC_FUNC
#undef TOTEM_HASHMAP_FREEKEY_FUNC
#undef TOTEM_HASHMAP



void totemEvalIntHashMap_FreeKey(totemHashMapEntry *entry)
{

}

totemHashMapEntry* totemEvalIntHashMap_AllocEntry(totemHashValue hash)
{
    totemHashMapEntry* entry = (totemHashMapEntry*)TOTEM_MALLOC(sizeof(totemEvalIntHashMapEntry));
    if (entry)
    {
        entry->Hash = hash;
    }

    return entry;
}

void totemEvalIntHashMap_CopyEntry(totemHashMapEntry* from, totemHashMapEntry* to)
{
    totemEvalIntHashMapEntry* e1 = (totemEvalIntHashMapEntry*)from;
    totemEvalIntHashMapEntry* e2 = (totemEvalIntHashMapEntry*)to;
    e2->Data = e1->Data;
}

void totemEvalIntHashMap_FreeEntry(totemHashMapEntry* entry)
{
    TOTEM_FREE(entry, sizeof(totemEvalIntHashMapEntry));
}

totemHashValue totemEvalIntHashMap_HashKey(totemEvalIntHashMapKey* key)
{
    return totem_Hash(&key->Key, sizeof(key->Key));
}

totemBool totemEvalIntHashMap_CompareKey(totemHashMapEntry* entry, totemEvalIntHashMapKey *key)
{
    totemEvalIntHashMapEntry* e = (totemEvalIntHashMapEntry*)entry;
    return e->Data.Key == key->Key;
}

void totemEvalIntHashMap_FetchKey(totemHashMapEntry* entry, totemEvalIntHashMapKey* valueOut)
{
    totemEvalIntHashMapEntry* e = (totemEvalIntHashMapEntry*)entry;
    *valueOut = e->Data;
}

void totemEvalIntHashMap_StoreKey(totemHashMapEntry* entry, totemEvalIntHashMapKey* value)
{
    totemEvalIntHashMapEntry* e = (totemEvalIntHashMapEntry*)entry;
    e->Data = *value;
}

#define TOTEM_HASHMAP totemEvalIntHashMap
#define TOTEM_HASHMAP_FREEKEY_FUNC totemEvalIntHashMap_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemEvalIntHashMap_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemEvalIntHashMap_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemEvalIntHashMap_FreeEntry

#define TOTEM_HASHMAP_SUFFIX TOTEM_NONE
#define TOTEM_HASHMAP_KEY totemEvalIntHashMapKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemEvalIntHashMap_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemEvalIntHashMap_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemEvalIntHashMap_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemEvalIntHashMap_StoreKey

#include <TotemScript/hashmap_def.h>
#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

#undef TOTEM_HASHMAP_ENTRYFREE_FUNC
#undef TOTEM_HASHMAP_ENTRYCOPY_FUNC
#undef TOTEM_HASHMAP_ENTRYALLOC_FUNC
#undef TOTEM_HASHMAP_FREEKEY_FUNC
#undef TOTEM_HASHMAP
