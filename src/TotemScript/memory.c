//
//  memory.c
//  TotemScript
//
//  Created by Timothy Smale on 14/11/2015.
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <string.h>
#include <TotemScript/exec.h>

#define TOTEM_MEM_FREELIST_DIVISOR (sizeof(totemMemoryPageObject))
#define TOTEM_MEM_NUM_FREELISTS (512)
#define TOTEM_MEM_PAGESIZE (TOTEM_MEM_FREELIST_DIVISOR * TOTEM_MEM_NUM_FREELISTS)

#if TOTEM_DEBUGOPT_ASSERT_HASHMAP_LISTS
#define TOTEM_DEBUG_HASHMAP_ASSERT(x) TotemHashMap_Assert(x)
#else
#define TOTEM_DEBUG_HASHMAP_ASSERT(x)
#endif

typedef struct totemMemoryPageObject
{
    struct totemMemoryPageObject *Next;
}
totemMemoryPageObject;

typedef struct totemMemoryPage
{
    struct totemMemoryPage *Next;
    size_t NumAllocated;
    size_t NumTotal;
    char Data[TOTEM_MEM_PAGESIZE];
}
totemMemoryPage;

typedef struct
{
    totemLock Lock;
    totemMemoryPage *HeadPage;
    totemMemoryPageObject *HeadObject;
    size_t ObjectSize;
}
totemMemoryFreeList;

static totemMemoryFreeList s_FreeLists[TOTEM_MEM_NUM_FREELISTS];

static totemMallocCb s_mallocCb = NULL;
static totemFreeCb s_freeCb = NULL;
static totemBool s_initialized = totemBool_False;

void *totem_defaultMalloc(size_t size)
{
	return malloc(size);
}

void totem_defaultFree(void *ptr, size_t size)
{
	TOTEM_UNUSED(size);
	free(ptr);
}

void totem_InitMemory(void)
{
	if (!s_initialized)
	{
		for (size_t i = 0; i < TOTEM_MEM_NUM_FREELISTS; i++)
		{
			totemMemoryFreeList *freeList = &s_FreeLists[i];
			totemLock_Init(&freeList->Lock);
			freeList->ObjectSize = (i + 1) * sizeof(totemMemoryPageObject);
			freeList->HeadObject = NULL;
			freeList->HeadPage = NULL;
		}

		s_mallocCb = totem_defaultMalloc;
		s_freeCb = totem_defaultFree;

		s_initialized = totemBool_True;
	}
}

void totem_CleanupMemory(void)
{
	if (s_initialized)
	{
		for (size_t i = 0; i < TOTEM_MEM_NUM_FREELISTS; i++)
		{
			totemMemoryFreeList *freeList = &s_FreeLists[i];
			for (totemMemoryPage *page = freeList->HeadPage; page != NULL; /* nada */)
			{
				totemMemoryPage *next = page->Next;
				s_freeCb(page, sizeof(totemMemoryPage));
				page = next;
			}

			totemLock_Cleanup(&freeList->Lock);
			freeList->HeadObject = NULL;
			freeList->HeadPage = NULL;
		}

		s_mallocCb = NULL;
		s_freeCb = NULL;
		s_initialized = totemBool_False;
	}
}

void totem_SetMemoryCallbacks(totemMallocCb newMallocCb, totemFreeCb newFreeCb)
{
	if (newMallocCb && newFreeCb)
	{
		s_mallocCb = newMallocCb;
		s_freeCb = newFreeCb;
	}
	else
	{
		s_mallocCb = totem_defaultMalloc;
		s_freeCb = totem_defaultFree;
	}
}

static totemMemoryFreeList *totemMemoryFreeList_Get(size_t amount)
{
	size_t index = amount / TOTEM_MEM_FREELIST_DIVISOR;
	index += (amount % TOTEM_MEM_FREELIST_DIVISOR != 0);
    index -= (index != 0);
    
	TOTEM_DEBUG_ASSERT(index < TOTEM_MEM_NUM_FREELISTS);
    totemMemoryFreeList *list = &s_FreeLists[index];
    
    TOTEM_DEBUG_ASSERT(amount <= list->ObjectSize);
    TOTEM_DEBUG_ASSERT(list == &s_FreeLists[0] ? 1 : amount > (list - 1)->ObjectSize);
    
    return list;
}

void *totem_CacheMalloc(size_t amount)
{
    if(amount > TOTEM_MEM_PAGESIZE)
    {
        return s_mallocCb(amount);
    }
    
    void *ptr = NULL;
    totemMemoryFreeList *freeList = totemMemoryFreeList_Get(amount);
    if(freeList)
    {
        totemLock_Acquire(&freeList->Lock);
        totemMemoryPageObject *head = freeList->HeadObject;
        
        // check freelist first
        if(head != NULL)
        {
            totemMemoryPageObject *obj = head;
            freeList->HeadObject = obj->Next;
            ptr = obj;
        }
        else
        {
            // look for a new allocation in cached pages
            if (freeList->HeadPage)
            {
                totemMemoryPage *page = freeList->HeadPage;
                if (page->NumAllocated < page->NumTotal)
                {
                    totemMemoryPageObject *obj = (totemMemoryPageObject*)(page->Data + (page->NumAllocated * freeList->ObjectSize));
                    page->NumAllocated++;
                    ptr = obj;
                }
            }
            
            if (!ptr)
            {
                // grab a new page & allocate from that
				totemMemoryPage *newPage = s_mallocCb(sizeof(totemMemoryPage));
                if (newPage != NULL)
                {
                    newPage->NumTotal = TOTEM_MEM_PAGESIZE / freeList->ObjectSize;
                    newPage->Next = freeList->HeadPage;
                    freeList->HeadPage = newPage;
                    newPage->NumAllocated = 1;
                    ptr = newPage->Data;
                }
            }
        }
        
        totemLock_Release(&freeList->Lock);
    }
    
    return ptr;
}

void totem_CacheFree(void *ptr, size_t amount)
{
    if(!ptr)
    {
        return;
    }
    
    if(amount > TOTEM_MEM_PAGESIZE)
    {
        s_freeCb(ptr, amount);
        return;
    }
    
    totemMemoryFreeList *freeList = totemMemoryFreeList_Get(amount);
    totemMemoryPageObject *obj = ptr;
    
    totemLock_Acquire(&freeList->Lock);
    obj->Next = freeList->HeadObject;
    freeList->HeadObject = obj;
    totemLock_Release(&freeList->Lock);
}

void totemMemoryBlock_Cleanup(totemMemoryBlock **blockHead)
{
    totemMemoryBlock *block = *blockHead;
    
    while(block)
    {
        void *ptr = block;
        block = block->Prev;
        totem_CacheFree(ptr, sizeof(totemMemoryBlock));
    }
}

void *totemMemoryBlock_Alloc(totemMemoryBlock **blockHead, size_t objectSize)
{
    size_t allocSize = objectSize;
    size_t extra = allocSize % sizeof(uintptr_t);
    if(extra != 0)
    {
        allocSize += sizeof(uintptr_t) - extra;
    }
    
    totemMemoryBlock *chosenBlock = NULL;
    
    for(totemMemoryBlock *block = *blockHead; block != NULL; block = block->Prev)
    {
        if(block->Remaining > allocSize)
        {
            chosenBlock = block;
            break;
        }
    }
    
    if(chosenBlock == NULL)
    {
        // alloc new
        chosenBlock = (totemMemoryBlock*)totem_CacheMalloc(sizeof(totemMemoryBlock));
        if(chosenBlock == NULL)
        {
            return NULL;
        }
        chosenBlock->Remaining = TOTEM_MEMORYBLOCK_DATASIZE;
        chosenBlock->Prev = *blockHead;
        *blockHead = chosenBlock;
    }
    
    char *ptr = chosenBlock->Data + (TOTEM_MEMORYBLOCK_DATASIZE - chosenBlock->Remaining);
    TOTEM_ZERO_ARRAY(ptr, objectSize);
    chosenBlock->Remaining -= allocSize;
    return (void*)ptr;
}

void totemBufferBase_Init(totemBufferBase *buffer)
{
    buffer->Data = NULL;
    buffer->Length = 0;
    buffer->MaxLength = 0;
}

void totemBufferBase_Reset(totemBufferBase *buffer)
{
    buffer->Length = 0;
}

void totemBufferBase_Cleanup(totemBufferBase *buffer, size_t objectSize)
{
    if(buffer->Data)
    {
        totem_CacheFree(buffer->Data, buffer->MaxLength * objectSize);
        
        buffer->Data = NULL;
        buffer->Length = 0;
        buffer->MaxLength = 0;
    }
}

void *totemBufferBase_Top(totemBufferBase *buffer, size_t objectSize)
{
    if(buffer->Length)
    {
        size_t index = (buffer->Length - 1) * objectSize;
        return &buffer->Data[index];
    }
    
    return NULL;
}

void *totemBufferBase_Bottom(totemBufferBase *buffer)
{
    if(buffer->Length)
    {
        return buffer->Data;
    }
    
    return NULL;
}

void totemBufferBase_Pop(totemBufferBase *buffer, size_t amount)
{
    if(amount > buffer->Length)
    {
        amount = buffer->Length;
    }
    
    buffer->Length -= amount;
}

void *totemBufferBase_At(totemBufferBase *buffer, size_t index, size_t objectSize)
{
    if(index < buffer->MaxLength)
    {
        return (void*)&buffer->Data[index * objectSize];
    }
    
    return NULL;
}

size_t totemBufferBase_Num(totemBufferBase *buffer)
{
    return buffer->Length;
}

size_t totemBufferBase_Max(totemBufferBase *buffer)
{
    return buffer->MaxLength;
}

void *totemBufferBase_Secure(totemBufferBase *buffer, size_t amount, size_t objectSize, totemMallocCb alloc, totemFreeCb dealloc)
{
	if (buffer->Length + 1 >= buffer->MaxLength)
	{
		size_t toAlloc = 0;
		if (buffer->MaxLength == 0)
		{
			toAlloc = 8;
		}
		else
		{
			toAlloc = buffer->MaxLength * 2;
		}

		if (amount + buffer->Length > toAlloc)
		{
			toAlloc = amount + buffer->Length;
		}

		char *newMem = (char*)alloc(toAlloc * objectSize);

		if (newMem == NULL)
		{
			return NULL;
		}

		memcpy(newMem, buffer->Data, buffer->MaxLength * objectSize);
		TOTEM_ZERO_ARRAY(newMem + (buffer->MaxLength * objectSize), (toAlloc - buffer->MaxLength) * objectSize);

		dealloc(buffer->Data, buffer->MaxLength * objectSize);

		buffer->MaxLength = toAlloc;
		buffer->Data = newMem;
	}

	void *ptr = buffer->Data + (buffer->Length * objectSize);
	buffer->Length += amount;
	return ptr;
}

void *totemBufferBase_Insert(totemBufferBase *buffer, void *data, size_t amount, size_t objectSize, totemMallocCb alloc, totemFreeCb dealloc)
{
	void *ptr = totemBufferBase_Secure(buffer, amount, objectSize, alloc, dealloc);
	if (!ptr)
	{
		return NULL;
	}

	memcpy(ptr, data, objectSize * amount);
	return ptr;
}

totemBool totemBufferBase_Reserve(totemBufferBase *buffer, size_t amount, size_t objectSize, totemMallocCb alloc, totemFreeCb dealloc)
{
	if (buffer->MaxLength < amount)
	{
		size_t currentAmount = buffer->Length;

		if (totemBufferBase_Secure(buffer, amount - buffer->MaxLength, objectSize, alloc, dealloc) == NULL)
		{
			return totemBool_False;
		}

		buffer->Length = currentAmount;
	}

	return totemBool_True;
}

void TotemHashMap_Assert(totemHashMap *hashmap)
{
	for (totemInt i = 0; i < hashmap->NumBuckets; i++)
    {
        totemHashMapEntry *bucket = hashmap->Buckets[i];
        
        for (size_t k = 0; bucket; k++)
        {
            TOTEM_DEBUG_ASSERT(k < 50000);
            bucket = bucket->Next;
        }
    }
    
    totemHashMapEntry *bucket = hashmap->FreeList;
    
    for (size_t i = 0; bucket; i++)
    {
        TOTEM_DEBUG_ASSERT(i < 50000);
        bucket = bucket->Next;
    }
}

void totemHashMap_Init(totemHashMap *hashmap)
{
    TOTEM_ZERO(hashmap);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
}

void totemHashMap_FreeEntry(totemHashMap *hashmap, totemHashMapEntry *entry)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totem_CacheFree((void*)entry->Key, entry->KeyLen);
    
    entry->Next = hashmap->FreeList;
    hashmap->FreeList = entry;
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
}

totemHashMapEntry *totemHashMap_SecureEntry(totemHashMap *hashmap)
{
    totemHashMapEntry *entry = NULL;
    
    if (hashmap->FreeList)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        entry = hashmap->FreeList;
        hashmap->FreeList = entry->Next;
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    }
    else
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        entry = totem_CacheMalloc(sizeof(totemHashMapEntry));
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    }
    
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    return entry;
}

void totemHashMap_MoveKeysToFreeList(totemHashMap *hashmap)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    
    for(totemInt i = 0; i < hashmap->NumBuckets; i++)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        while(hashmap->Buckets[i])
        {
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            totemHashMapEntry *entry = hashmap->Buckets[i];
            hashmap->Buckets[i] = entry->Next;
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            totemHashMap_FreeEntry(hashmap, entry);
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        }
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    }
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    
    hashmap->NumKeys = 0;
}

void totemHashMap_InsertDirect(totemHashMapEntry **buckets, size_t numBuckets, totemHashMapEntry *entry)
{
    size_t index = entry->Hash % numBuckets;
    if(buckets[index] == NULL)
    {
        buckets[index] = entry;
    }
    else
    {
        for(totemHashMapEntry *bucket = buckets[index]; bucket != NULL; bucket = bucket->Next)
        {
            if(bucket->Next == NULL)
            {
                bucket->Next = entry;
                break;
            }
        }
    }
}

totemBool totemHashMap_InsertPrecomputedWithoutSearch(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashMapValue value, totemHashValue hash)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    if (hashmap->NumKeys >= hashmap->NumBuckets)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        // buckets realloc
		totemInt newNumBuckets = 0;
        if (hashmap->NumKeys == 0)
        {
            newNumBuckets = 32;
        }
        else
        {
            newNumBuckets = hashmap->NumKeys * 2;
        }
        
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        totemHashMapEntry **newBuckets = totem_CacheMalloc(sizeof(totemHashMapEntry**) * newNumBuckets);
        if (!newBuckets)
        {
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            return totemBool_False;
        }
        
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        // reassign
		TOTEM_ZERO_ARRAY(newBuckets, newNumBuckets);
		for (totemInt i = 0; i < hashmap->NumBuckets; i++)
        {
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            totemHashMapEntry *next = NULL;
            for (totemHashMapEntry *entry = hashmap->Buckets[i]; entry != NULL; entry = next)
            {
                TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                next = entry->Next;
                entry->Next = NULL;
                totemHashMap_InsertDirect(newBuckets, newNumBuckets, entry);
                TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            }
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        }
        
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        // replace buckets
        totem_CacheFree(hashmap->Buckets, sizeof(totemHashMapEntry**) * hashmap->NumBuckets);
        hashmap->Buckets = newBuckets;
        hashmap->NumBuckets = newNumBuckets;
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    }
    
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashMapEntry *entry = totemHashMap_SecureEntry(hashmap);
    if (!entry)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        return totemBool_False;
    }
    
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    void *persistKey = totem_CacheMalloc(keyLen);
    if (!persistKey)
    {
        return totemBool_False;
    }
    
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    memcpy(persistKey, key, keyLen);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    
    entry->Next = NULL;
    entry->Value = value;
    entry->Key = persistKey;
    entry->KeyLen = keyLen;
    entry->Hash = hash;
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashMap_InsertDirect(hashmap->Buckets, hashmap->NumBuckets, entry);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    
    hashmap->NumKeys++;
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    return totemBool_True;
}

totemBool totemHashMap_InsertPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashMapValue value, totemHashValue hash)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashMapEntry *existingEntry = totemHashMap_Find(hashmap, key, keyLen);
    if(existingEntry)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        existingEntry->Value = value;
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        return totemBool_True;
    }
    else
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        totemBool result = totemHashMap_InsertPrecomputedWithoutSearch(hashmap, key, keyLen, value, hash);
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        return result;
    }
}

totemBool totemHashMap_Insert(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashMapValue value)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashValue hash = totem_Hash(key, keyLen);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemBool result = totemHashMap_InsertPrecomputed(hashmap, key, keyLen, value, hash);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    return result;
}

totemBool totemHashMap_TakeFrom(totemHashMap *hashmap, totemHashMap *from)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    // todo: if this fails half-way through the list, free previously allocated entries
	for (totemInt i = 0; i < from->NumBuckets; i++)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        for(totemHashMapEntry *entry = from->Buckets[i]; entry != NULL; entry = entry->Next)
        {
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            if(!totemHashMap_InsertPrecomputed(hashmap, entry->Key, entry->KeyLen, entry->Value, entry->Hash))
            {
                TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                return totemBool_False;
            }
            
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        }
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    }
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    
    return totemBool_True;
}

totemHashMapEntry *totemHashMap_RemovePrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue hash)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    if (hashmap->NumBuckets > 0)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        int index = hash % hashmap->NumBuckets;
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        for (totemHashMapEntry *entry = hashmap->Buckets[index], *prev = NULL; entry != NULL; prev = entry, entry = entry->Next)
        {
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            if (entry->Hash == hash && memcmp(entry->Key, key, keyLen) == 0)
            {
                TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                if (prev)
                {
                    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                    prev->Next = entry->Next;
                    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                }
                else
                {
                    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                    hashmap->Buckets[index] = entry->Next;
                    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                }
                
                TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                totemHashMap_FreeEntry(hashmap, entry);
                TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                hashmap->NumKeys--;
                return entry;
            }
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        }
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    }
    
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    return NULL;
}

totemHashMapEntry *totemHashMap_Remove(totemHashMap *hashmap, const void *key, size_t keyLen)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashValue hash = totem_Hash(key, keyLen);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashMapEntry *result = totemHashMap_RemovePrecomputed(hashmap, key, keyLen, hash);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    return result;
}

totemHashMapEntry *totemHashMap_Find(totemHashMap *hashmap, const void *key, size_t keyLen)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashValue hash = totem_Hash(key, keyLen);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashMapEntry *result = totemHashMap_FindPrecomputed(hashmap, key, keyLen, hash);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    return result;
}

totemHashMapEntry *totemHashMap_FindPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue hash)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    if(hashmap->NumBuckets > 0)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        int index = hash % hashmap->NumBuckets;
        TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        for(totemHashMapEntry *entry = hashmap->Buckets[index]; entry != NULL; entry = entry->Next)
        {
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
            
            if(entry->Hash == hash && entry->KeyLen == keyLen && memcmp(entry->Key, key, entry->KeyLen) == 0)
            {
                TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
                return entry;
            }
            TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
        }
    }
    
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    return NULL;
}

void totemHashMap_Reset(totemHashMap *hashmap)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    totemHashMap_MoveKeysToFreeList(hashmap);
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
    hashmap->NumKeys = 0;
    TOTEM_DEBUG_HASHMAP_ASSERT(hashmap);
}

void totemHashMap_Cleanup(totemHashMap *map)
{
    TOTEM_DEBUG_HASHMAP_ASSERT(map);
    totemHashMap_MoveKeysToFreeList(map);
    TOTEM_DEBUG_HASHMAP_ASSERT(map);
    while(map->FreeList)
    {
        TOTEM_DEBUG_HASHMAP_ASSERT(map);
        totemHashMapEntry *entry = map->FreeList;
        map->FreeList = entry->Next;
        totem_CacheFree(entry, sizeof(totemHashMapEntry));
        TOTEM_DEBUG_HASHMAP_ASSERT(map);
    }
    
    TOTEM_DEBUG_HASHMAP_ASSERT(map);
    totem_CacheFree(map->Buckets, sizeof(totemHashMapEntry**) * map->NumBuckets);
    map->Buckets = NULL;
    map->NumBuckets = 0;
    TOTEM_DEBUG_HASHMAP_ASSERT(map);
}