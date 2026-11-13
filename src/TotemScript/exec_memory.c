//
//  exec_memory.c
//  TotemScript
//
//  Created by Timothy Smale on 26/09/2022
//  Copyright (c) 2022 Timothy Smale. All rights reserved.
//

#include <TotemScript/exec.h>
#include <TotemScript/parse.h>

void totemRuntimeStringSet_FreeKey(totemHashMapEntry* entry)
{

}

totemHashMapEntry* totemRuntimeStringSet_AllocEntry(totemHashValue value)
{
	totemHashMapEntry* entry = (totemHashMapEntry*)TOTEM_MALLOC(sizeof(totemRuntimeStringSetEntry));
	if (entry)
	{
		entry->Hash = value;
	}

	return entry;
}

void totemRuntimeStringSet_CopyEntry(totemHashMapEntry* from, totemHashMapEntry* to)
{
	totemRuntimeStringSetEntry* e1 = (totemRuntimeStringSetEntry*)from;
	totemRuntimeStringSetEntry* e2 = (totemRuntimeStringSetEntry*)to;
	e2->Data = e1->Data;
}

void totemRuntimeStringSet_FreeEntry(totemHashMapEntry* entry)
{
    TOTEM_FREE(entry, sizeof(totemRuntimeStringSetEntry));
}

#define TOTEM_HASHMAP totemRuntimeStringSet
#define TOTEM_HASHMAP_FREEKEY_FUNC totemRuntimeStringSet_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemRuntimeStringSet_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemRuntimeStringSet_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemRuntimeStringSet_FreeEntry

#include <TotemScript/hashmap_def.h>

totemHashValue totemRuntimeStringSet_HashKey(totemRuntimeStringValue* key)
{
	return key->Hash;
}

totemBool totemRuntimeStringSet_CompareKey(totemHashMapEntry* entry, totemRuntimeStringValue* key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
    return totemRuntimeStringValue_Compare(key, &e->Data);
}

void totemRuntimeStringSet_FetchKey(totemHashMapEntry* entry, totemRuntimeStringValue* key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
	*key = e->Data;
}

void totemRuntimeStringSet_StoreKey(totemHashMapEntry* entry, totemRuntimeStringValue* key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
	e->Data = *key;
}

#define TOTEM_HASHMAP_SUFFIX TOTEM_NONE
#define TOTEM_HASHMAP_KEY totemRuntimeStringValue
#define TOTEM_HASHMAP_KEYHASH_FUNC totemRuntimeStringSet_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemRuntimeStringSet_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemRuntimeStringSet_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemRuntimeStringSet_StoreKey

#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

totemHashValue totemRuntimeStringSet_HashKeyLiteral(totemRuntimeStringSetLiteralKey* key)
{
	return totem_Hash(key->Key.Value, key->Key.Length);
}

totemBool totemRuntimeStringSet_CompareKeyLiteral(totemHashMapEntry* entry, totemRuntimeStringSetLiteralKey *key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
    return totemRuntimeStringValue_CompareWithString(&e->Data, &key->Key);
}

void totemRuntimeStringSet_FetchKeyLiteral(totemHashMapEntry* entry, totemRuntimeStringSetLiteralKey* key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
	key->Value = &e->Data;
}

#define TOTEM_HASHMAP_SUFFIX Literal
#define TOTEM_HASHMAP_KEY totemRuntimeStringSetLiteralKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemRuntimeStringSet_HashKeyLiteral
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemRuntimeStringSet_CompareKeyLiteral
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemRuntimeStringSet_FetchKeyLiteral

#include <TotemScript/hashmap_searchdef.h>

#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

totemHashValue totemRuntimeStringSet_HashKeyParse(totemRuntimeStringSetParseKey* key)
{
	return totemParseString_Hash(key->Key);
}

totemBool totemRuntimeStringSet_CompareKeyParse(totemHashMapEntry* entry, totemRuntimeStringSetParseKey* key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
    const char *entryStrPtr = totemRuntimeStringValue_GetVal(&e->Data);
	return totemParseString_Equals(key->Key, entryStrPtr, e->Data.Length);
}

void totemRuntimeStringSet_FetchKeyParse(totemHashMapEntry* entry, totemRuntimeStringSetParseKey* key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
	key->Value = &e->Data;
}

#define TOTEM_HASHMAP_SUFFIX Parse
#define TOTEM_HASHMAP_KEY totemRuntimeStringSetParseKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemRuntimeStringSet_HashKeyParse
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemRuntimeStringSet_CompareKeyParse
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemRuntimeStringSet_FetchKeyParse

#include <TotemScript/hashmap_searchdef.h>

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

void totemRuntimeStringMap_FreeKey(totemHashMapEntry* entry)
{
	totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
	if (e->Data.Key.Type == totemPrivateDataType_InternedString)
	{
		totemInternedStringHeader_Free(e->Data.Key.Src.InternedString);
	}
}

totemHashMapEntry* totemRuntimeStringMap_AllocEntry(totemHashValue value)
{
	totemHashMapEntry* entry = (totemHashMapEntry*)TOTEM_MALLOC(sizeof(totemRuntimeStringMapEntry));
	if (entry)
	{
		entry->Hash = value;
	}

	return entry;
}

void totemRuntimeStringMap_CopyEntry(totemHashMapEntry *from, totemHashMapEntry *to)
{
	totemRuntimeStringMapEntry* e1 = (totemRuntimeStringMapEntry*)from;
	totemRuntimeStringMapEntry* e2 = (totemRuntimeStringMapEntry*)to;
	e2->Data = e1->Data;
}

void totemRuntimeStringMap_FreeEntry(totemHashMapEntry* entry)
{
    TOTEM_FREE(entry, sizeof(totemRuntimeStringMapEntry));
}

#define TOTEM_HASHMAP totemRuntimeStringMap
#define TOTEM_HASHMAP_FREEKEY_FUNC totemRuntimeStringMap_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemRuntimeStringMap_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemRuntimeStringMap_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemRuntimeStringMap_FreeEntry

#include <TotemScript/hashmap_def.h>

totemHashValue totemRuntimeStringMap_HashKey(totemRuntimeStringMapKey* key)
{
	return key->Key.Hash;
}

totemBool totemRuntimeStringMap_CompareKey(totemHashMapEntry* entry, totemRuntimeStringMapKey* key)
{
	totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
    return totemRuntimeStringValue_Compare(&e->Data.Key, &key->Key);
}

void totemRuntimeStringMap_FetchKey(totemHashMapEntry* entry, totemRuntimeStringMapKey* key)
{
	totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
	*key = e->Data;
}

void totemRuntimeStringMap_StoreKey(totemHashMapEntry* entry, totemRuntimeStringMapKey* key)
{
	totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
	e->Data = *key;
}

#define TOTEM_HASHMAP_SUFFIX TOTEM_NONE
#define TOTEM_HASHMAP_KEY totemRuntimeStringMapKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemRuntimeStringMap_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemRuntimeStringMap_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemRuntimeStringMap_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemRuntimeStringMap_StoreKey

#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

totemHashValue totemRuntimeStringMap_HashKeyLiteral(totemRuntimeStringMapLiteralKey* key)
{
	return totem_Hash(key->Key.Value, key->Key.Length);
}

totemBool totemRuntimeStringMap_CompareKeyLiteral(totemHashMapEntry* entry, totemRuntimeStringMapLiteralKey* key)
{
	totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
    return totemRuntimeStringValue_CompareWithString(&e->Data.Key, &key->Key);
}

void totemRuntimeStringMap_FetchKeyLiteral(totemHashMapEntry* entry, totemRuntimeStringMapLiteralKey* key)
{
	totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
	key->Value = e->Data.Value;
}

#define TOTEM_HASHMAP_SUFFIX Literal
#define TOTEM_HASHMAP_KEY totemRuntimeStringMapLiteralKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemRuntimeStringMap_HashKeyLiteral
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemRuntimeStringMap_CompareKeyLiteral
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemRuntimeStringMap_FetchKeyLiteral

#include <TotemScript/hashmap_searchdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

totemHashValue totemRuntimeStringMap_HashKeyParse(totemRuntimeStringMapParseKey* key)
{
	return totemParseString_Hash(key->Key);
}

totemBool totemRuntimeStringMap_CompareKeyParse(totemHashMapEntry* entry, totemRuntimeStringMapParseKey* key)
{
	totemRuntimeStringSetEntry* e = (totemRuntimeStringSetEntry*)entry;
    const char *entryStrPtr = totemRuntimeStringValue_GetVal(&e->Data);
	return totemParseString_Equals(key->Key, entryStrPtr, e->Data.Length);
}

void totemRuntimeStringMap_FetchKeyParse(totemHashMapEntry* entry, totemRuntimeStringMapParseKey* key)
{;
	totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
	key->Value = e->Data.Value;
}

#define TOTEM_HASHMAP_SUFFIX Parse
#define TOTEM_HASHMAP_KEY totemRuntimeStringMapParseKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemRuntimeStringMap_HashKeyParse
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemRuntimeStringMap_CompareKeyParse
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemRuntimeStringMap_FetchKeyParse

#include <TotemScript/hashmap_searchdef.h>

#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

#undef TOTEM_HASHMAP_ENTRYCOPY_FUNC
#undef TOTEM_HASHMAP_ENTRYFREE_FUNC
#undef TOTEM_HASHMAP_ENTRYALLOC_FUNC
#undef TOTEM_HASHMAP_FREEKEY_FUNC
#undef TOTEM_HASHMAP

totemHashMapEntry* totemScriptCache_AllocEntry(totemHashValue value)
{
	totemScriptCacheEntry* entry = (totemScriptCacheEntry*)TOTEM_MALLOC(sizeof(totemScriptCacheEntry));
    if (entry)
    {
		entry->Data.Value = NULL;
        entry->Entry.Hash = value;
    }

    return (totemHashMapEntry*)entry;
}

void totemScriptCache_CopyEntry(totemHashMapEntry* from, totemHashMapEntry* to)
{
    totemScriptCacheEntry* e1 = (totemScriptCacheEntry*)from;
    totemScriptCacheEntry* e2 = (totemScriptCacheEntry*)to;
    e2->Data = e1->Data;
}

void totemScriptCache_FreeEntry(totemHashMapEntry* entry)
{
	totemScriptCacheEntry* e = (totemScriptCacheEntry*)entry;
    TOTEM_FREE(e, sizeof(totemScriptCacheEntry));
}

void totemScriptCache_FreeKey(totemHashMapEntry* entry)
{
	totemScriptCacheEntry* e = (totemScriptCacheEntry*)entry;
	if (e->Data.Value)
	{
		TOTEM_FREE(e->Data.Value, sizeof(*e->Data.Value));
		e->Data.Value = NULL;
	}
}

#define TOTEM_HASHMAP totemScriptCache
#define TOTEM_HASHMAP_FREEKEY_FUNC totemScriptCache_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemScriptCache_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemScriptCache_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemScriptCache_FreeEntry

#include <TotemScript/hashmap_def.h>

totemHashValue totemScriptCache_HashKey(totemScriptCacheKey* key)
{
    return key->Key.Hash;
}

totemBool totemScriptCache_CompareKey(totemHashMapEntry* entry, totemScriptCacheKey* key)
{
    totemScriptCacheKey* e = (totemScriptCacheKey*)entry;
    return totemRuntimeStringValue_Compare(&key->Key, &e->Key);
}

void totemScriptCache_FetchKey(totemHashMapEntry* entry, totemScriptCacheKey* key)
{;
    totemScriptCacheEntry* e = (totemScriptCacheEntry*)entry;
    *key = e->Data;
}

void totemScriptCache_StoreKey(totemHashMapEntry* entry, totemScriptCacheKey* key)
{
    totemScriptCacheEntry* e = (totemScriptCacheEntry*)entry;
    e->Data = *key;
}

#define TOTEM_HASHMAP_SUFFIX
#define TOTEM_HASHMAP_KEY totemScriptCacheKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemScriptCache_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemScriptCache_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemScriptCache_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemScriptCache_StoreKey

#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYSTORE_FUNC
#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

#undef TOTEM_HASHMAP_ENTRYCOPY_FUNC
#undef TOTEM_HASHMAP_ENTRYFREE_FUNC
#undef TOTEM_HASHMAP_ENTRYALLOC_FUNC
#undef TOTEM_HASHMAP_FREEKEY_FUNC
#undef TOTEM_HASHMAP

void totemInstanceCache_FreeKey(totemHashMapEntry* entry)
{

}

totemHashMapEntry* totemInstanceCache_AllocEntry(totemHashValue value)
{
    totemHashMapEntry* entry = (totemHashMapEntry*)TOTEM_MALLOC(sizeof(totemInstanceCacheEntry));
    if (entry)
    {
        entry->Hash = value;
    }

    return entry;
}

void totemInstanceCache_CopyEntry(totemHashMapEntry* from, totemHashMapEntry* to)
{
    totemInstanceCacheEntry* e1 = (totemInstanceCacheEntry*)from;
    totemInstanceCacheEntry* e2 = (totemInstanceCacheEntry*)to;
    e2->Data = e1->Data;
}

void totemInstanceCache_FreeEntry(totemHashMapEntry* entry)
{
    TOTEM_FREE(entry, sizeof(totemInstanceCacheEntry));
}

#define TOTEM_HASHMAP totemInstanceCache
#define TOTEM_HASHMAP_FREEKEY_FUNC totemInstanceCache_FreeKey
#define TOTEM_HASHMAP_ENTRYALLOC_FUNC totemInstanceCache_AllocEntry
#define TOTEM_HASHMAP_ENTRYCOPY_FUNC totemInstanceCache_CopyEntry
#define TOTEM_HASHMAP_ENTRYFREE_FUNC totemInstanceCache_FreeEntry

#include <TotemScript/hashmap_def.h>

totemHashValue totemInstanceCache_HashKey(totemInstanceCacheKey* key)
{
    return key->Key.Hash;
}

totemBool totemInstanceCache_CompareKey(totemHashMapEntry* entry, totemInstanceCacheKey* key)
{
    totemInstanceCacheEntry* e = (totemInstanceCacheEntry*)entry;
    return totemRuntimeStringValue_Compare(&key->Key, &e->Data.Key);
}

void totemInstanceCache_FetchKey(totemHashMapEntry* entry, totemInstanceCacheKey* key)
{;
    totemInstanceCacheEntry* e = (totemInstanceCacheEntry*)entry;
    *key = e->Data;
}

void totemInstanceCache_StoreKey(totemHashMapEntry* entry, totemInstanceCacheKey* key)
{
    totemInstanceCacheEntry* e = (totemInstanceCacheEntry*)entry;
    e->Data = *key;
}

#define TOTEM_HASHMAP_SUFFIX
#define TOTEM_HASHMAP_KEY totemInstanceCacheKey
#define TOTEM_HASHMAP_KEYHASH_FUNC totemInstanceCache_HashKey
#define TOTEM_HASHMAP_KEYCOMPARE_FUNC totemInstanceCache_CompareKey
#define TOTEM_HASHMAP_KEYFETCH_FUNC totemInstanceCache_FetchKey
#define TOTEM_HASHMAP_KEYSTORE_FUNC totemInstanceCache_StoreKey

#include <TotemScript/hashmap_searchdef.h>
#include <TotemScript/hashmap_insertdef.h>

#undef TOTEM_HASHMAP_KEYFETCH_FUNC
#undef TOTEM_HASHMAP_KEYCOMPARE_FUNC
#undef TOTEM_HASHMAP_KEYHASH_FUNC
#undef TOTEM_HASHMAP_KEY
#undef TOTEM_HASHMAP_SUFFIX

#undef TOTEM_HASHMAP_ENTRYFREE_FUNC
#undef TOTEM_HASHMAP_ENTRYALLOC_FUNC
#undef TOTEM_HASHMAP_FREEKEY_FUNC
#undef TOTEM_HASHMAP
