//
//  memory.c
//  TotemScript
//
//  Created by Timothy Smale on 14/11/2015.
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <string.h>

#define TOTEM_MEM_FREELIST_DIVISOR (sizeof(totemMemoryPageObject))
#define TOTEM_MEM_PAGESIZE (TOTEM_MEM_FREELIST_DIVISOR * 512)
#define TOTEM_MEM_NUM_FREELISTS (TOTEM_MEM_PAGESIZE / TOTEM_MEM_FREELIST_DIVISOR)

#if 0
#define totemHashMap_Assert(x) totemHashMap_AssertList(x)
#else
#define totemHashMap_Assert(x)
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

static totemMallocCb mallocCb = NULL;
static totemFreeCb freeCb = NULL;

//size_t allocs = 0;

void *totem_Malloc(size_t len)
{
    //printf("allocating %zu %zu\n", allocs++, len);
    if (mallocCb)
    {
        return mallocCb(len);
    }
    
    return malloc(len);
}

void totem_Free(void *mem)
{
    //printf("freeing %zu\n", --allocs);
    if (freeCb)
    {
        freeCb(mem);
    }
    else
    {
        free(mem);
    }
}

void totem_InitMemory()
{
    memset(&s_FreeLists, 0, sizeof(s_FreeLists));
    for (size_t i = 0; i < TOTEM_MEM_NUM_FREELISTS; i++)
    {
        totemMemoryFreeList *freeList = &s_FreeLists[i];
        totemLock_Init(&freeList->Lock);
        freeList->ObjectSize = (i + 1) * sizeof(totemMemoryPageObject);
    }
    
    mallocCb = NULL;
    freeCb = NULL;
}

void totem_CleanupMemory()
{
    for (size_t i = 0; i < TOTEM_MEM_NUM_FREELISTS; i++)
    {
        totemMemoryFreeList *freeList = &s_FreeLists[i];
        for (totemMemoryPage *page = freeList->HeadPage; page != NULL; /* nada */)
        {
            totemMemoryPage *next = page->Next;
            totem_Free(page);
            page = next;
        }
        
        totemLock_Cleanup(&freeList->Lock);
    }
}

void totem_SetMemoryCallbacks(totemMallocCb newMallocCb, totemFreeCb newFreeCb)
{
    mallocCb = newMallocCb;
    freeCb = newFreeCb;
}

totemMemoryFreeList *totemMemoryFreeList_Get(size_t amount)
{
    if(amount > TOTEM_MEM_PAGESIZE)
    {
        return NULL;
    }
    
    if(amount < sizeof(totemMemoryPageObject))
    {
        amount = sizeof(totemMemoryPageObject);
    }
    
    size_t index = amount / TOTEM_MEM_FREELIST_DIVISOR;
    index += (amount % TOTEM_MEM_FREELIST_DIVISOR != 0);
    index--;
    
    totemMemoryFreeList *list = &s_FreeLists[index];
    
    totem_assert(amount <= list->ObjectSize);
    totem_assert(list == &s_FreeLists[0] ? 1 : amount > (list - 1)->ObjectSize);
    
    return list;
}

void *totem_CacheMalloc(size_t amount)
{
    if(amount > TOTEM_MEM_PAGESIZE)
    {
        return totem_Malloc(amount);
    }
    
    void *ptr = NULL;
    totemMemoryFreeList *freeList = totemMemoryFreeList_Get(amount);
    if(freeList)
    {
        totemLock_Acquire(&freeList->Lock);
        totemMemoryPageObject *head = freeList->HeadObject;
        
        //printf("cache alloc %zu, actual %zu\n", amount, actualAmount);
        
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
                totemMemoryPage *newPage = totem_Malloc(sizeof(totemMemoryPage));
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
        totem_Free(ptr);
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
    
    void *ptr = chosenBlock->Data + (TOTEM_MEMORYBLOCK_DATASIZE - chosenBlock->Remaining);
    memset(ptr, 0, objectSize);
    chosenBlock->Remaining -= allocSize;
    return ptr;
}

void totemMemoryBuffer_Init(totemMemoryBuffer *buffer, size_t objectSize)
{
    buffer->ObjectSize = objectSize;
    buffer->Data = NULL;
    buffer->Length = 0;
    buffer->MaxLength = 0;
}

void totemMemoryBuffer_Reset(totemMemoryBuffer *buffer)
{
    buffer->Length = 0;
}

void totemMemoryBuffer_Cleanup(totemMemoryBuffer *buffer)
{
    if(buffer->Data)
    {
        totem_CacheFree(buffer->Data, buffer->MaxLength);
        
        buffer->Data = NULL;
        buffer->Length = 0;
        buffer->MaxLength = 0;
        buffer->ObjectSize = 0;
    }
}

void *totemMemoryBuffer_Top(totemMemoryBuffer *buffer)
{
    if(buffer->Length)
    {
        size_t index = buffer->Length - buffer->ObjectSize;
        return &buffer->Data[index];
    }
    
    return NULL;
}

void *totemMemoryBuffer_Bottom(totemMemoryBuffer *buffer)
{
    if(buffer->Length)
    {
        return buffer->Data;
    }
    
    return NULL;
}

void totemMemoryBuffer_Pop(totemMemoryBuffer *buffer, size_t amount)
{
    amount *= buffer->ObjectSize;
    
    if(amount > buffer->Length)
    {
        amount = buffer->Length;
    }
    
    buffer->Length -= amount;
}

void *totemMemoryBuffer_Insert(totemMemoryBuffer *buffer, void *data, size_t numObjects)
{
    void *ptr = totemMemoryBuffer_Secure(buffer, numObjects);
    if(!ptr)
    {
        return NULL;
    }
    
    memcpy(ptr, data, buffer->ObjectSize * numObjects);
    return ptr;
}

void *totemMemoryBuffer_SecureDirect(totemMemoryBuffer *buffer, size_t amount)
{
    char *memEnd = buffer->Data + buffer->MaxLength;
    char *memCurrent = buffer->Data + buffer->Length;
    
    if(memCurrent + amount >= memEnd)
    {
        size_t toAlloc = buffer->MaxLength;
        
        if(buffer->MaxLength == 0)
        {
            toAlloc = buffer->ObjectSize * 32;
        }
        else
        {
            toAlloc *= 2;
        }
        
        if(amount + buffer->Length > toAlloc)
        {
            toAlloc = amount + buffer->Length;
        }
        
        char *newMem = totem_CacheMalloc(toAlloc);
        
        if(newMem == NULL)
        {
            return NULL;
        }
        
        memcpy(newMem, buffer->Data, buffer->MaxLength);
        memset(newMem + buffer->MaxLength, 0, toAlloc - buffer->MaxLength);
        
        totem_CacheFree(buffer->Data, buffer->MaxLength);
        
        buffer->MaxLength = toAlloc;
        buffer->Data = newMem;
    }
    
    void *ptr = buffer->Data + buffer->Length;
    buffer->Length += amount;
    return ptr;
}

totemBool totemMemoryBuffer_Reserve(totemMemoryBuffer *buffer, size_t amount)
{
    amount *= buffer->ObjectSize;
    
    if (buffer->MaxLength < amount)
    {
        size_t currentAmount = buffer->Length;
        
        if (totemMemoryBuffer_SecureDirect(buffer, amount - buffer->MaxLength) == NULL)
        {
            return totemBool_False;
        }
        
        buffer->Length = currentAmount;
    }
    
    return totemBool_True;
}

void *totemMemoryBuffer_Secure(totemMemoryBuffer *buffer, size_t amount)
{
    amount *= buffer->ObjectSize;
    return totemMemoryBuffer_SecureDirect(buffer, amount);
}

void *totemMemoryBuffer_TakeFrom(totemMemoryBuffer *buffer, totemMemoryBuffer *takeFrom)
{
    void *ptr = totemMemoryBuffer_SecureDirect(buffer, takeFrom->Length);
    if(!ptr)
    {
        return NULL;
    }
    
    memcpy(ptr, takeFrom->Data, takeFrom->Length);
    return ptr;
}

void *totemMemoryBuffer_Get(totemMemoryBuffer *buffer, size_t index)
{
    index *= buffer->ObjectSize;
    if(index < buffer->MaxLength)
    {
        return (void*)&buffer->Data[index];
    }
    
    return NULL;
}

size_t totemMemoryBuffer_GetNumObjects(totemMemoryBuffer *buffer)
{
    return buffer->Length / buffer->ObjectSize;
}

size_t totemMemoryBuffer_GetMaxObjects(totemMemoryBuffer *buffer)
{
    return buffer->MaxLength / buffer->ObjectSize;
}

void totemHashMap_AssertList(totemHashMap *hashmap)
{
    for (size_t i = 0; i < hashmap->NumBuckets; i++)
    {
        totemHashMapEntry *bucket = hashmap->Buckets[i];
        
        for (size_t k = 0; bucket; k++)
        {
            totem_assert(k < 50000);
            bucket = bucket->Next;
        }
    }
    
    totemHashMapEntry *bucket = hashmap->FreeList;
    
    for (size_t i = 0; bucket; i++)
    {
        totem_assert(i < 50000);
        bucket = bucket->Next;
    }
}

void totemHashMap_Init(totemHashMap *hashmap)
{
    memset(hashmap, 0, sizeof(totemHashMap));
    totemHashMap_Assert(hashmap);
}

void totemHashMap_FreeEntry(totemHashMap *hashmap, totemHashMapEntry *entry)
{
    totemHashMap_Assert(hashmap);
    totem_CacheFree((void*)entry->Key, entry->KeyLen);
    
    entry->Next = hashmap->FreeList;
    hashmap->FreeList = entry;
    totemHashMap_Assert(hashmap);
}

totemHashMapEntry *totemHashMap_SecureEntry(totemHashMap *hashmap)
{
    totemHashMapEntry *entry = NULL;
    
    if (hashmap->FreeList)
    {
        totemHashMap_Assert(hashmap);
        entry = hashmap->FreeList;
        hashmap->FreeList = entry->Next;
        totemHashMap_Assert(hashmap);
    }
    else
    {
        totemHashMap_Assert(hashmap);
        entry = totem_CacheMalloc(sizeof(totemHashMapEntry));
        totemHashMap_Assert(hashmap);
    }
    
    totemHashMap_Assert(hashmap);
    return entry;
}

void totemHashMap_MoveKeysToFreeList(totemHashMap *hashmap)
{
    totemHashMap_Assert(hashmap);
    
    for(size_t i = 0; i < hashmap->NumBuckets; i++)
    {
        totemHashMap_Assert(hashmap);
        while(hashmap->Buckets[i])
        {
            totemHashMap_Assert(hashmap);
            totemHashMapEntry *entry = hashmap->Buckets[i];
            hashmap->Buckets[i] = entry->Next;
            totemHashMap_Assert(hashmap);
            totemHashMap_FreeEntry(hashmap, entry);
            totemHashMap_Assert(hashmap);
        }
        totemHashMap_Assert(hashmap);
    }
    totemHashMap_Assert(hashmap);
    
    totemHashMap_Assert(hashmap);
    
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

totemBool totemHashMap_InsertPrecomputedWithoutSearch(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue value, totemHash hash)
{
    totemHashMap_Assert(hashmap);
    if (hashmap->NumKeys >= hashmap->NumBuckets)
    {
        totemHashMap_Assert(hashmap);
        // buckets realloc
        size_t newNumBuckets = 0;
        if (hashmap->NumKeys == 0)
        {
            newNumBuckets = 32;
        }
        else
        {
            newNumBuckets = hashmap->NumKeys * 2;
        }
        
        totemHashMap_Assert(hashmap);
        totemHashMapEntry **newBuckets = totem_CacheMalloc(sizeof(totemHashMapEntry**) * newNumBuckets);
        if (!newBuckets)
        {
            totemHashMap_Assert(hashmap);
            return totemBool_False;
        }
        
        totemHashMap_Assert(hashmap);
        // reassign
        memset(newBuckets, 0, newNumBuckets * sizeof(totemHashMapEntry**));
        for (size_t i = 0; i < hashmap->NumBuckets; i++)
        {
            totemHashMap_Assert(hashmap);
            totemHashMapEntry *next = NULL;
            for (totemHashMapEntry *entry = hashmap->Buckets[i]; entry != NULL; entry = next)
            {
                totemHashMap_Assert(hashmap);
                next = entry->Next;
                entry->Next = NULL;
                totemHashMap_InsertDirect(newBuckets, newNumBuckets, entry);
                totemHashMap_Assert(hashmap);
            }
            totemHashMap_Assert(hashmap);
        }
        
        totemHashMap_Assert(hashmap);
        // replace buckets
        totem_CacheFree(hashmap->Buckets, sizeof(totemHashMapEntry**) * hashmap->NumBuckets);
        hashmap->Buckets = newBuckets;
        hashmap->NumBuckets = newNumBuckets;
        totemHashMap_Assert(hashmap);
    }
    
    totemHashMap_Assert(hashmap);
    totemHashMapEntry *entry = totemHashMap_SecureEntry(hashmap);
    if (!entry)
    {
        totemHashMap_Assert(hashmap);
        return totemBool_False;
    }
    
    totemHashMap_Assert(hashmap);
    void *persistKey = totem_CacheMalloc(keyLen);
    if (!persistKey)
    {
        return totemBool_False;
    }
    
    totemHashMap_Assert(hashmap);
    memcpy(persistKey, key, keyLen);
    totemHashMap_Assert(hashmap);
    
    entry->Next = NULL;
    entry->Value = value;
    entry->Key = persistKey;
    entry->KeyLen = keyLen;
    entry->Hash = hash;
    totemHashMap_Assert(hashmap);
    totemHashMap_InsertDirect(hashmap->Buckets, hashmap->NumBuckets, entry);
    totemHashMap_Assert(hashmap);
    
    hashmap->NumKeys++;
    totemHashMap_Assert(hashmap);
    return totemBool_True;
}

totemBool totemHashMap_InsertPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue value, totemHash hash)
{
    totemHashMap_Assert(hashmap);
    totemHashMapEntry *existingEntry = totemHashMap_Find(hashmap, key, keyLen);
    if(existingEntry)
    {
        totemHashMap_Assert(hashmap);
        existingEntry->Value = value;
        totemHashMap_Assert(hashmap);
        return totemBool_True;
    }
    else
    {
        totemHashMap_Assert(hashmap);
        totemBool result = totemHashMap_InsertPrecomputedWithoutSearch(hashmap, key, keyLen, value, hash);
        totemHashMap_Assert(hashmap);
        return result;
    }
}

totemBool totemHashMap_Insert(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue value)
{
    totemHashMap_Assert(hashmap);
    totemHash hash = totem_Hash(key, keyLen);
    totemHashMap_Assert(hashmap);
    totemBool result = totemHashMap_InsertPrecomputed(hashmap, key, keyLen, value, hash);
    totemHashMap_Assert(hashmap);
    return result;
}

totemBool totemHashMap_TakeFrom(totemHashMap *hashmap, totemHashMap *from)
{
    totemHashMap_Assert(hashmap);
    // todo: if this fails half-way through the list, free previously allocated entries
    for(size_t i = 0; i < from->NumBuckets; i++)
    {
        totemHashMap_Assert(hashmap);
        for(totemHashMapEntry *entry = from->Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemHashMap_Assert(hashmap);
            if(!totemHashMap_InsertPrecomputed(hashmap, entry->Key, entry->KeyLen, entry->Value, entry->Hash))
            {
                totemHashMap_Assert(hashmap);
                return totemBool_False;
            }
            
            totemHashMap_Assert(hashmap);
        }
        totemHashMap_Assert(hashmap);
    }
    totemHashMap_Assert(hashmap);
    
    return totemBool_True;
}

totemHashMapEntry *totemHashMap_RemovePrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHash hash)
{
    totemHashMap_Assert(hashmap);
    if (hashmap->NumBuckets > 0)
    {
        totemHashMap_Assert(hashmap);
        int index = hash % hashmap->NumBuckets;
        totemHashMap_Assert(hashmap);
        for (totemHashMapEntry *entry = hashmap->Buckets[index], *prev = NULL; entry != NULL; prev = entry, entry = entry->Next)
        {
            totemHashMap_Assert(hashmap);
            if (entry->Hash == hash && memcmp(entry->Key, key, keyLen) == 0)
            {
                totemHashMap_Assert(hashmap);
                if (prev)
                {
                    totemHashMap_Assert(hashmap);
                    prev->Next = entry->Next;
                    totemHashMap_Assert(hashmap);
                }
                else
                {
                    totemHashMap_Assert(hashmap);
                    hashmap->Buckets[index] = entry->Next;
                    totemHashMap_Assert(hashmap);
                }
                
                totemHashMap_Assert(hashmap);
                totemHashMap_FreeEntry(hashmap, entry);
                totemHashMap_Assert(hashmap);
                hashmap->NumKeys--;
                return entry;
            }
            totemHashMap_Assert(hashmap);
        }
        totemHashMap_Assert(hashmap);
    }
    
    totemHashMap_Assert(hashmap);
    return NULL;
}

totemHashMapEntry *totemHashMap_Remove(totemHashMap *hashmap, const void *key, size_t keyLen)
{
    totemHashMap_Assert(hashmap);
    totemHash hash = totem_Hash(key, keyLen);
    totemHashMap_Assert(hashmap);
    totemHashMapEntry *result = totemHashMap_RemovePrecomputed(hashmap, key, keyLen, hash);
    totemHashMap_Assert(hashmap);
    return result;
}

totemHashMapEntry *totemHashMap_Find(totemHashMap *hashmap, const void *key, size_t keyLen)
{
    totemHashMap_Assert(hashmap);
    totemHash hash = totem_Hash(key, keyLen);
    totemHashMap_Assert(hashmap);
    totemHashMapEntry *result = totemHashMap_FindPrecomputed(hashmap, key, keyLen, hash);
    totemHashMap_Assert(hashmap);
    return result;
}

totemHashMapEntry *totemHashMap_FindPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHash hash)
{
    totemHashMap_Assert(hashmap);
    if(hashmap->NumBuckets > 0)
    {
        totemHashMap_Assert(hashmap);
        int index = hash % hashmap->NumBuckets;
        totemHashMap_Assert(hashmap);
        for(totemHashMapEntry *entry = hashmap->Buckets[index]; entry != NULL; entry = entry->Next)
        {
            totemHashMap_Assert(hashmap);
            if(entry->Hash == hash && memcmp(entry->Key, key, entry->KeyLen) == 0)
            {
                totemHashMap_Assert(hashmap);
                return entry;
            }
            totemHashMap_Assert(hashmap);
        }
    }
    
    totemHashMap_Assert(hashmap);
    return NULL;
}

void totemHashMap_Reset(totemHashMap *hashmap)
{
    totemHashMap_Assert(hashmap);
    totemHashMap_MoveKeysToFreeList(hashmap);
    totemHashMap_Assert(hashmap);
    hashmap->NumKeys = 0;
    totemHashMap_Assert(hashmap);
}

void totemHashMap_Cleanup(totemHashMap *map)
{
    totemHashMap_Assert(map);
    totemHashMap_MoveKeysToFreeList(map);
    totemHashMap_Assert(map);
    while(map->FreeList)
    {
        totemHashMap_Assert(map);
        totemHashMapEntry *entry = map->FreeList;
        map->FreeList = entry->Next;
        totem_CacheFree(entry, sizeof(totemHashMapEntry));
        totemHashMap_Assert(map);
    }
    
    totemHashMap_Assert(map);
    totem_CacheFree(map->Buckets, sizeof(totemHashMapEntry**) * map->NumBuckets);
    map->Buckets = NULL;
    map->NumBuckets = 0;
    totemHashMap_Assert(map);
}