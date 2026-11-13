//
//  memory.h
//  ColossusEngine
//
//  Created by Timothy Smale on 08/09/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_MEMORY_H
#define TOTEMSCRIPT_MEMORY_H

#include <TotemScript/base.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef void *(*totemMallocCb)(size_t);
	typedef void(*totemFreeCb)(void*, size_t);

	void *totem_CacheMalloc(size_t len, size_t line, const char *file);
    void *totem_CacheRealloc(void *ptr, size_t oldLen, size_t newLen, size_t line, const char *file);
    void *totem_CacheCopy(void *ptr, size_t len, size_t line, const char *file);
	void totem_CacheFree(void *ptr, size_t len, size_t line, const char *file);
	void totem_SetMemoryCallbacks(totemMallocCb malloc, totemFreeCb free);

#define TOTEM_MALLOC(size) totem_CacheMalloc(size, __LINE__, __FILE__)
#define TOTEM_REALLOC(from, oldSize, newSize) totem_CacheRealloc(from, oldSize, newSize, __LINE__, __FILE__)
#define TOTEM_COPY(from, size) totem_CacheCopy(from, size, __LINE__, __FILE__)
#define TOTEM_FREE(ptr, size) totem_CacheFree(ptr, size, __LINE__, __FILE__)

	void totem_PrintBits(totemLog *file, uint64_t data, uint64_t numBits, uint64_t start);

	void totem_InitMemory(void);
	void totem_CleanupMemory(void);

    typedef struct totemBuffer
    {
        char *Ptr;
        size_t Len;
    }
    totemBuffer;

    void totemBuffer_Init(totemBuffer *buff);
    void totemBuffer_Reset(totemBuffer *buff, size_t elemSize);
    void totemBuffer_Cleanup(totemBuffer *buff, size_t elemSize);
    char *totemBuffer_Top(totemBuffer *buffer, size_t elemSize);
    char *totemBuffer_Bottom(totemBuffer *buffer);
    char *totemBuffer_At(totemBuffer *buffer, size_t at, size_t elemSize);
    totemBool totemBuffer_Reserve(totemBuffer *buffer, size_t len, size_t elemSize);
    size_t totemBuffer_Length(totemBuffer *buffer);

#define TOTEM_BUFFER_DECLARATION(type) \
    typedef struct type ## Buffer \
    { \
        totemBuffer Buf;\
    } \
    type ## Buffer; \
    void type ## Buffer_Init(type ## Buffer *buff) { totemBuffer_Init(&buff->Buf); } \
    void type ## Buffer_Reset(type ## Buffer *buff) { totemBuffer_Reset(&buff->Buf, sizeof(type)); } \
    void type ## Buffer_Cleanup(type ## Buffer *buff) { totemBuffer_Cleanup(&buff->Buf, sizeof(type)); } \
    type * type ## Buffer_Top(type ## Buffer *buff) { return (type*)totemBuffer_Top(&buff->Buf, sizeof(type)); } \
    type * type ## Buffer_Bottom(type ## Buffer *buff) { return (type*)totemBuffer_Bottom(&buff->Buf); } \
    type * type ## Buffer_At(type ## Buffer *buff, size_t at) { return (type*)totemBuffer_At(&buff->Buf, at, sizeof(type)); } \
    void type ## Buffer_Secure(type ## Buffer _buff, size_t len) { return (type*)totemBuffer_Secure(&buff->Buf, len, sizeof(type)); } \
    size_t type ## Buffer_Length (totemBuffer *buffer);

    typedef struct totemDynBuffer
    {
        char *Ptr;
        size_t Len;
        size_t Max;
    }
    totemDynBuffer;

    void totemDynBuffer_Init(totemDynBuffer *buf);
    void totemDynBuffer_Reset(totemDynBuffer *buf);
    void totemDynBuffer_Cleanup(totemDynBuffer *buf, size_t elemSize);
    char *totemDynBuffer_Top(totemDynBuffer *buf, size_t elemSize);
    char *totemDynBuffer_Bottom(totemDynBuffer *buf);
    char *totemDynBuffer_Push(totemDynBuffer *buf, char *data, size_t len, size_t elemSize);
    totemBool totemDynBuffer_Pop(totemDynBuffer *buf, size_t len);
    char *totemDynBuffer_At(totemDynBuffer *buf, size_t at, size_t elemSize);
    char *totemDynBuffer_Secure(totemDynBuffer *buf, size_t len, size_t elemSize);
    totemBool totemDynBuffer_Reserve(totemDynBuffer *buf, size_t len, size_t elemSize);
    size_t totemDynBuffer_Length(totemDynBuffer *buf);
    size_t totemDynBuffer_Max(totemDynBuffer *buf);

#define TOTEM_DYNBUFFER_DECLARATION(type) \
    typedef struct type ## DynBuffer \
    { \
        totemDynBuffer Buf;\
    } \
    type ## DynBuffer; \
    static TOTEM_INLINE void type ## DynBuffer_Init(type ## DynBuffer *buf) { totemDynBuffer_Init(&buf->Buf); } \
    static TOTEM_INLINE void type ## DynBuffer_Reset(type ## DynBuffer *buf) { totemDynBuffer_Reset(&buf->Buf); } \
    static TOTEM_INLINE void type ## DynBuffer_Cleanup(type ## DynBuffer *buf) { totemDynBuffer_Cleanup(&buf->Buf, sizeof(type)); } \
    static TOTEM_INLINE type *type ## DynBuffer_Top(type ## DynBuffer *buf) { return (type*)totemDynBuffer_Top(&buf->Buf, sizeof(type)); } \
    static TOTEM_INLINE type *type ## DynBuffer_Bottom(type ## DynBuffer *buf) { return (type*)totemDynBuffer_Bottom(&buf->Buf); } \
    static TOTEM_INLINE type *type ## DynBuffer_Push(type ## DynBuffer *buf, type *data, size_t len) \
        { \
            return (type*)totemDynBuffer_Push(&buf->Buf, (char*)data, len, sizeof(type)); \
        } \
    static TOTEM_INLINE totemBool type ## DynBuffer_Pop(type ## DynBuffer *buf, size_t size) \
        { \
            return totemDynBuffer_Pop(&buf->Buf, size); \
        } \
    static TOTEM_INLINE type *type ## DynBuffer_At(type ## DynBuffer *buf, size_t at) \
        { \
            return (type*)totemDynBuffer_At(&buf->Buf, at, sizeof(type)); \
        } \
    static TOTEM_INLINE type *type ## DynBuffer_Secure(type ## DynBuffer *buf, size_t len) \
        { \
            return (type*)totemDynBuffer_Secure(&buf->Buf, len, sizeof(type)); \
        } \
    static TOTEM_INLINE totemBool type ## DynBuffer_Reserve(type ## DynBuffer *buf, size_t len) \
        { \
            return totemDynBuffer_Reserve(&buf->Buf, len, sizeof(type)); \
        } \
    static TOTEM_INLINE size_t type ## DynBuffer_Length(type ## DynBuffer *buf) { return totemDynBuffer_Length(&buf->Buf); } \
    static TOTEM_INLINE size_t type ## DynBuffer_Max(type ## DynBuffer *buf) { return totemDynBuffer_Max(&buf->Buf); }

	TOTEM_DYNBUFFER_DECLARATION(totemOperandXu);
	TOTEM_DYNBUFFER_DECLARATION(totemInstruction);

	typedef struct totemHashMapEntry
	{
		struct totemHashMapEntry* Next;
		totemHashValue Hash;
    }
	totemHashMapEntry;

    static TOTEM_INLINE void totemHashMapEntry_Insert(totemHashMapEntry** buckets, size_t numBuckets, totemHashMapEntry* entry)
	{
		size_t index = entry->Hash % numBuckets;
		entry->Next = buckets[index];
		buckets[index] = entry;
	}

#define TOTEM_HASHMAP_M_CONCAT(x, y) x ## _ ## y
#define TOTEM_HASHMAP_M_CONCAT_2(x, y, z) x ## _ ## y ## z
#define TOTEM_HASHMAP_M_NAME(type, name) TOTEM_HASHMAP_M_CONCAT(type, name)
#define TOTEM_HASHMAP_M_NAME_SUFFIX(hashmap, name, suffix) TOTEM_HASHMAP_M_CONCAT_2(hashmap, name, suffix)
//#define TOTEM_HASHMAP_M_NAME_SUFFIX(TOTEM_HASHMAP, type, suffix) TOTEM_CONCAT(type, _) ## name ## suffix //TOTEM_X(type) ## _ ## TOTEM_X(name) ## TOTEM_X(suffix)

#define TOTEM_HASHMAP_DECLARATION(name) \
    typedef struct name \
    { \
        totemHashMapEntry **Buckets; \
        totemHashMapEntry *FreeList; \
        uint32_t NumBuckets; \
        uint32_t NumKeys; \
    }  \
    name; \
    \
    void TOTEM_HASHMAP_M_NAME(name, Init) (name* map); \
    void TOTEM_HASHMAP_M_NAME(name, Reset) (name* map); \
    void TOTEM_HASHMAP_M_NAME(name, Cleanup) (name* map); \
    totemBool TOTEM_HASHMAP_M_NAME(name, Secure)(name* map, size_t num); \
    void TOTEM_HASHMAP_M_NAME(name, AddEntryToFreeList)(name* map, totemHashMapEntry *entry); \
    totemHashMapEntry *TOTEM_HASHMAP_M_NAME(name, AllocFreeEntry)(name* map, totemHashValue hash); \

#define TOTEM_HASHMAP_INSERT_DECLARATION_SUFFIX(name, keytype, suffix) \
	totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, InsertNoSearchPrecomputed, suffix)(name* hashmap, keytype* key, totemHashValue hash); \
	totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, InsertNoSearch, suffix)(name* hashmap, keytype* key); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, InsertPrecomputed, suffix)(name* hashmap, keytype* key, totemHashValue hash); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, Insert, suffix)(name* hashmap, keytype* key); \
	totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, InsertGetPrecomputed, suffix)(name* hashmap, keytype* key, totemHashValue hash); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, InsertGet, suffix)(name* hashmap, keytype* key); \
	totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, InsertOverwritePrecomputed, suffix)(name* hashmap, keytype* key, totemHashValue hash); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, InsertOverwrite, suffix)(name* hashmap, keytype* key); \

#define TOTEM_HASHMAP_SEARCH_DECLARATION_SUFFIX(name, keytype, suffix) \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, ExistsPrecomputed, suffix) (name* hashmap, keytype* key, totemHashValue hash); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, Exists, suffix) (name* hashmap, keytype* key); \
	totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, GetPrecomputed, suffix) (name* hashmap, keytype* key, totemHashValue hash); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, Get, suffix) (name* hashmap, keytype* key); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, RemovePrecomputed, suffix)(name* hashmap, keytype* key, totemHashValue hash); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, Remove, suffix)(name* hashmap, keytype* key); \
	totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, RemoveGetPrecomputed, suffix)(name* hashmap, keytype* key, totemHashValue hash); \
    totemBool TOTEM_HASHMAP_M_NAME_SUFFIX(name, RemoveGet, suffix)(name* hashmap, keytype* key); \

#define TOTEM_HASHMAP_SUFFIX TOTEM_NONE
#define TOTEM_HASHMAP_SEARCH_DECLARATION(name, keytype) TOTEM_HASHMAP_SEARCH_DECLARATION_SUFFIX(name, keytype, TOTEM_NONE)
#define TOTEM_HASHMAP_INSERT_DECLARATION(name, keytype) TOTEM_HASHMAP_INSERT_DECLARATION_SUFFIX(name, keytype, TOTEM_NONE)
#undef TOTEM_HASHMAP_SUFFIX

#ifdef __cplusplus
}
#endif

#endif
