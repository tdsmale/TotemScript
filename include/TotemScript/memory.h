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

#ifdef __cplusplus
extern "C" {
#endif

	typedef void *(*totemMallocCb)(size_t);
	typedef void(*totemFreeCb)(void*, size_t);

	void *totem_CacheMalloc(size_t len);
	void totem_CacheFree(void *ptr, size_t len);
	void totem_SetMemoryCallbacks(totemMallocCb malloc, totemFreeCb free);

	void totem_printBits(FILE *file, uint64_t data, uint64_t numBits, uint64_t start);

	typedef struct totemBufferBase
	{
		char *Data;
		size_t MaxLength;
		size_t Length;
	}
	totemBufferBase;

	void totemBufferBase_Init(totemBufferBase *buffer);
	void totemBufferBase_Reset(totemBufferBase *buffer);
	void totemBufferBase_Cleanup(totemBufferBase *buffer, size_t objectSize);
	void *totemBufferBase_Top(totemBufferBase *buffer, size_t objectSize);
	void *totemBufferBase_Bottom(totemBufferBase *buffer);
	size_t totemBufferBase_Num(totemBufferBase *buffer);
	size_t totemBufferBase_Max(totemBufferBase *buffer);
	void totemBufferBase_Pop(totemBufferBase *buffer, size_t amount);
	void *totemBufferBase_At(totemBufferBase *buffer, size_t index, size_t objectSize);
	void *totemBufferBase_Secure(totemBufferBase *buffer, size_t amount, size_t objectSize, totemMallocCb alloc, totemFreeCb dealloc);
	void *totemBufferBase_Insert(totemBufferBase *buffer, void *data, size_t amount, size_t objectSize, totemMallocCb alloc, totemFreeCb dealloc);
	totemBool totemBufferBase_Reserve(totemBufferBase *buffer, size_t amount, size_t objectSize, totemMallocCb alloc, totemFreeCb dealloc);

#define TOTEM_BUFFER_NAME(type) type ## Buffer
#define TOTEM_BUFFER_METHOD_NAME(type, name) type ## Buffer_ ## name

#define TOTEM_BUFFER_DEFINE_METHOD(type, name) \
	TOTEM_INLINE void TOTEM_BUFFER_METHOD_NAME(type, name) (TOTEM_BUFFER_NAME(type) *buffer) \
	{ \
		totemBufferBase_##name (&buffer->Base); \
	}

#define TOTEM_BUFFER_DEFINE_METHOD_RET(type, ret, name)  \
	TOTEM_INLINE ret TOTEM_BUFFER_METHOD_NAME(type, name) (TOTEM_BUFFER_NAME(type) *buffer) \
	{ \
		return (ret) totemBufferBase_##name (&buffer->Base); \
	}

#define TOTEM_BUFFER_DEFINE_METHOD_ARG1(type, name, arg1Type, arg1Name) \
	TOTEM_INLINE void TOTEM_BUFFER_METHOD_NAME(type, name) (TOTEM_BUFFER_NAME(type) *buffer, arg1Type arg1Name) \
	{ \
		totemBufferBase_##name (&buffer->Base, arg1Name); \
	}

#define TOTEM_BUFFER_DEFINE_METHOD_ARGS_RET(type, ret, name, ...) \
	TOTEM_INLINE ret TOTEM_BUFFER_METHOD_NAME(type, name) (TOTEM_BUFFER_NAME(type) *buffer) \
	{ \
		return (ret) totemBufferBase_##name (&buffer->Base, __VA_ARGS__); \
	}

#define TOTEM_BUFFER_DEFINE_METHOD_ARGS1_RET(type, ret, name, arg1Type, arg1Name, ...) \
	TOTEM_INLINE ret TOTEM_BUFFER_METHOD_NAME(type, name) (TOTEM_BUFFER_NAME(type) *buffer, arg1Type arg1Name) \
	{ \
		return (ret) totemBufferBase_##name (&buffer->Base, arg1Name, __VA_ARGS__); \
	}

#define TOTEM_BUFFER_DEFINE_METHOD_ARGS(type, name, ...) \
	TOTEM_INLINE void TOTEM_BUFFER_METHOD_NAME(type, name) (TOTEM_BUFFER_NAME(type) *buffer) \
	{ \
		totemBufferBase_##name (&buffer->Base, __VA_ARGS__); \
	}

#define TOTEM_BUFFER_DEFINE_ALLOCATION(type, allocFunc, freeFunc) \
	TOTEM_INLINE type *TOTEM_BUFFER_METHOD_NAME(type, Secure) (TOTEM_BUFFER_NAME(type) *typeBuffer, size_t amount) \
	{ \
		return (type*)totemBufferBase_Secure(&typeBuffer->Base, amount, sizeof(type), allocFunc, freeFunc); \
	} \
	\
	TOTEM_INLINE type *TOTEM_BUFFER_METHOD_NAME(type, Insert) (TOTEM_BUFFER_NAME(type) *buffer, void *data, size_t numObjects) \
	{ \
		return (type*)totemBufferBase_Insert(&buffer->Base, data, numObjects, sizeof(type), allocFunc, freeFunc); \
	} \
	\
	TOTEM_INLINE totemBool TOTEM_BUFFER_METHOD_NAME(type, Reserve) (TOTEM_BUFFER_NAME(type) *buffer, size_t amount) \
	{ \
		return totemBufferBase_Reserve(&buffer->Base, amount, sizeof(type), allocFunc, freeFunc); \
	}

// define a buffer that stores stated type
#define TOTEM_BUFFER_DEFINITION(type, allocFunc, freeFunc) \
	typedef struct TOTEM_BUFFER_NAME(type) { totemBufferBase Base; } TOTEM_BUFFER_NAME(type); \
	TOTEM_BUFFER_DEFINE_METHOD(type, Init) \
	TOTEM_BUFFER_DEFINE_METHOD(type, Reset) \
	TOTEM_BUFFER_DEFINE_METHOD_ARGS(type, Cleanup, sizeof(type)) \
	TOTEM_BUFFER_DEFINE_METHOD_ARGS_RET(type, type*, Top, sizeof(type)) \
	TOTEM_BUFFER_DEFINE_METHOD_RET(type, type*, Bottom) \
	TOTEM_BUFFER_DEFINE_METHOD_RET(type, size_t, Num) \
	TOTEM_BUFFER_DEFINE_METHOD_RET(type, size_t, Max) \
	TOTEM_BUFFER_DEFINE_METHOD_ARG1(type, Pop, size_t, num) \
	TOTEM_BUFFER_DEFINE_METHOD_ARGS1_RET(type, type*, At, size_t, index, sizeof(type)) \
	TOTEM_BUFFER_DEFINE_ALLOCATION(type, allocFunc, freeFunc)

#define TOTEM_CACHEBUFFER_DEFINITION(type) TOTEM_BUFFER_DEFINITION(type, totem_CacheMalloc, totem_CacheFree)

	TOTEM_CACHEBUFFER_DEFINITION(totemOperandXUnsigned);
	TOTEM_CACHEBUFFER_DEFINITION(totemInstruction);
	TOTEM_CACHEBUFFER_DEFINITION(totemString);
	TOTEM_CACHEBUFFER_DEFINITION(totemChar);

#define TOTEM_MEMORYBLOCK_DATASIZE (512)

	typedef struct totemMemoryBlock
	{
		char Data[TOTEM_MEMORYBLOCK_DATASIZE];
		size_t Remaining;
		struct totemMemoryBlock *Prev;
	}
	totemMemoryBlock;

	void *totemMemoryBlock_Alloc(totemMemoryBlock **blockHead, size_t objectSize);
	void totemMemoryBlock_Cleanup(totemMemoryBlock **blockHead);

	typedef struct totemHashMapEntry
	{
		const void *Key;
		totemHashMapValue Value;
		struct totemHashMapEntry *Next;
		size_t KeyLen;
		totemHashValue Hash;
	}
	totemHashMapEntry;

	typedef struct totemHashMap
	{
		totemHashMapEntry **Buckets;
		totemHashMapEntry *FreeList;
		totemInt NumBuckets;
		totemInt NumKeys;
	}
	totemHashMap;

	void totemHashMap_Init(totemHashMap *hashmap);
	void totemHashMap_Reset(totemHashMap *hashmap);
	void totemHashMap_Cleanup(totemHashMap *hashmap);
	totemBool totemHashMap_TakeFrom(totemHashMap *hashmap, totemHashMap *from);
	totemBool totemHashMap_Insert(totemHashMap *hashmap, const void *key, size_t KeyLen, totemHashMapValue Value);
	totemBool totemHashMap_InsertPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashMapValue value, totemHashValue hash);
	totemBool totemHashMap_InsertPrecomputedWithoutSearch(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashMapValue value, totemHashValue hash);
	totemHashMapEntry *totemHashMap_Remove(totemHashMap *hashmap, const void *key, size_t KeyLen);
	totemHashMapEntry *totemHashMap_RemovePrecomputed(totemHashMap *hashmap, const void *key, size_t KeyLen, totemHashValue hash);
	totemHashMapEntry *totemHashMap_Find(totemHashMap *hashmap, const void *key, size_t keyLen);
	totemHashMapEntry *totemHashMap_FindPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue hash);

	void totem_InitMemory(void);
	void totem_CleanupMemory(void);

#ifdef __cplusplus
}
#endif

#endif
