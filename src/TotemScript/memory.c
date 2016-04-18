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
#define TOTEM_MEM_BLOCKSIZE (TOTEM_MEM_PAGESIZE * TOTEM_MEM_PAGESIZE)
#define TOTEM_MEM_NUM_FREELISTS (TOTEM_MEM_PAGESIZE / TOTEM_MEM_FREELIST_DIVISOR)

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
    totemMemoryPage *HeadPage;
    totemMemoryPageObject *HeadObject;
}
totemMemoryFreeList;

static totemMemoryFreeList s_FreeLists[TOTEM_MEM_NUM_FREELISTS];

static totemMallocCb mallocCb = NULL;
static totemFreeCb freeCb = NULL;

void totem_SetMemoryCallbacks(totemMallocCb newMallocCb, totemFreeCb newFreeCb)
{
    mallocCb = newMallocCb;
    freeCb = newFreeCb;
}

void *totem_Malloc(size_t len)
{
    //printf("allocating %zu\n", len);
    if(mallocCb)
    {
        return mallocCb(len);
    }
    
    return malloc(len);
}

void totem_Free(void *mem)
{
    if(freeCb)
    {
        return freeCb(mem);
    }
    
    return free(mem);
}

totemMemoryFreeList *totemMemoryFreeList_Get(size_t amount, size_t *actualAmount)
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
    if(amount % TOTEM_MEM_FREELIST_DIVISOR != 0)
    {
        index++;
    }
    
    *actualAmount = index * TOTEM_MEM_FREELIST_DIVISOR;
    index -= (sizeof(totemMemoryPageObject) / TOTEM_MEM_FREELIST_DIVISOR);
    
    return &s_FreeLists[index];
}

void *totem_CacheMalloc(size_t amount)
{
    if(amount > TOTEM_MEM_PAGESIZE)
    {
        return totem_Malloc(amount);
    }
    
    size_t actualAmount = amount;
    totemMemoryFreeList *freeList = totemMemoryFreeList_Get(amount, &actualAmount);
    totemMemoryPageObject *head = freeList->HeadObject;
    
    //printf("cache alloc %zu, actual %zu\n", amount, actualAmount);
    
    // check freelist first
    if(head != NULL)
    {
        totemMemoryPageObject *obj = head;
        freeList->HeadObject = obj->Next;
        return obj;
    }
    else
    {
        // look for a new allocation in cached pages
        for(totemMemoryPage *page = freeList->HeadPage; page != NULL; page = page->Next)
        {
            if(page->NumAllocated < page->NumTotal)
            {
                totemMemoryPageObject *obj = (totemMemoryPageObject*)(page->Data + (page->NumAllocated * actualAmount));
                page->NumAllocated++;
                return obj;
            }
        }
        
        // grab a new page & allocate from that
        totemMemoryPage *newPage = totem_Malloc(sizeof(totemMemoryPage));
        if(newPage != NULL)
        {
            newPage->Next = freeList->HeadPage;
            freeList->HeadPage = newPage;
            newPage->NumAllocated = 1;
            return newPage->Data;
        }
    }
    
    // no more memory!
    return NULL;
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
    
    size_t actualAmount = amount;
    totemMemoryFreeList *freeList = totemMemoryFreeList_Get(amount, &actualAmount);
    totemMemoryPageObject *obj = ptr;
    
    obj->Next = freeList->HeadObject;
    freeList->HeadObject = obj;
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

void totemMemoryBuffer_Reset(totemMemoryBuffer *buffer, size_t objectSize)
{
    buffer->Length = 0;
    buffer->ObjectSize = objectSize;
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
        return buffer->Data + ((buffer->Length - 1) * buffer->ObjectSize);
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

size_t totemMemoryBuffer_Pop(totemMemoryBuffer *buffer, size_t amount)
{
    if(amount > buffer->Length)
    {
        amount = buffer->Length;
    }
    
    buffer->Length -= amount;
    return amount;
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

void *totemMemoryBuffer_Secure(totemMemoryBuffer *buffer, size_t amount)
{
    char *memEnd = buffer->Data + buffer->MaxLength;
    char *memCurrent = buffer->Data + buffer->Length;
    
    amount *= buffer->ObjectSize;
    
    if(memCurrent + amount > memEnd)
    {
        size_t toAlloc = buffer->MaxLength;
        
        if(buffer->MaxLength == 0)
        {
            toAlloc = buffer->ObjectSize * 32;
        }
        else
        {
            toAlloc *= 1.5;
        }
        
        if(amount > toAlloc)
        {
            toAlloc += amount;
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

totemBool totemHashMap_InsertPrecomputed(totemHashMap *hashmap, const char *key, size_t keyLen, totemHashValue value, totemHash hash)
{
    totemHashMapEntry *existingEntry = totemHashMap_Find(hashmap, key, keyLen);
    if(existingEntry)
    {
        existingEntry->Value = value;
        return totemBool_True;
    }
    else
    {
        if(hashmap->NumKeys >= hashmap->NumBuckets)
        {
            // buckets realloc
            size_t newNumBuckets = 0;
            if(hashmap->NumKeys == 0)
            {
                newNumBuckets = 32;
            }
            else
            {
                newNumBuckets = hashmap->NumKeys * 1.5;
            }
            
            totemHashMapEntry **newBuckets = totem_CacheMalloc(sizeof(totemHashMapEntry**) * newNumBuckets);
            if(!newBuckets)
            {
                return totemBool_False;
            }
            
            // reassign
            memset(newBuckets, 0, newNumBuckets * sizeof(totemHashMapEntry**));
            for(size_t i = 0; i < hashmap->NumBuckets; i++)
            {
                totemHashMapEntry *next = NULL;
                for(totemHashMapEntry *entry = hashmap->Buckets[i]; entry != NULL; entry = next)
                {
                    next = entry->Next;
                    entry->Next = NULL;
                    totemHashMap_InsertDirect(newBuckets, newNumBuckets, entry);
                }
            }
            
            // replace buckets
            totem_CacheFree(hashmap->Buckets, sizeof(totemHashMapEntry**) * hashmap->NumBuckets);
            hashmap->Buckets = newBuckets;
            hashmap->NumBuckets = newNumBuckets;
        }
        
        // hashmap insert
        totemHashMapEntry *entry = NULL;
        if (hashmap->FreeList)
        {
            entry = hashmap->FreeList;
            hashmap->FreeList = entry->Next;
        }
        else
        {
            entry = totem_CacheMalloc(sizeof(totemHashMapEntry));
            if(entry == NULL)
            {
                return totemBool_False;
            }
        }
        
        entry->Next = NULL;
        entry->Value = value;
        entry->Key = key;
        entry->KeyLen = keyLen;
        entry->Hash = hash == 0 ? totem_Hash(key, keyLen) : hash;
        totemHashMap_InsertDirect(hashmap->Buckets, hashmap->NumBuckets, entry);
        return totemBool_True;
    }
}

totemBool totemHashMap_Insert(totemHashMap *hashmap, const char *key, size_t keyLen, totemHashValue value)
{
    return totemHashMap_InsertPrecomputed(hashmap, key, keyLen, value, 0);
}

totemHashMapEntry *totemHashMap_Find(totemHashMap *hashmap, const char *key, size_t keyLen)
{
    if(hashmap->NumBuckets > 0)
    {
        uint32_t hash = totem_Hash((char*)key, keyLen);
        int index = hash % hashmap->NumBuckets;
        
        for(totemHashMapEntry *entry = hashmap->Buckets[index]; entry != NULL; entry = entry->Next)
        {
            if(strncmp(entry->Key, key, entry->KeyLen) == 0)
            {
                return entry;
            }
        }
    }
    
    return NULL;
}

void totemHashMap_MoveKeysToFreeList(totemHashMap *hashmap)
{
    for(size_t i = 0; i < hashmap->NumBuckets; i++)
    {
        while(hashmap->Buckets[i])
        {
            totemHashMapEntry *entry = hashmap->Buckets[i];
            hashmap->Buckets[i] = entry->Next;
            entry->Next = hashmap->FreeList;
            hashmap->FreeList = entry;
        }
    }
}

void totemHashMap_Reset(totemHashMap *hashmap)
{
    totemHashMap_MoveKeysToFreeList(hashmap);
    hashmap->NumKeys = 0;
}

void totemHashMap_Cleanup(totemHashMap *map)
{
    totemHashMap_MoveKeysToFreeList(map);
    while(map->FreeList)
    {
        totemHashMapEntry *entry = map->FreeList;
        map->FreeList = entry->Next;
        totem_CacheFree(entry, sizeof(totemHashMapEntry));
    }
    
    totem_CacheFree(map->Buckets, sizeof(totemHashMapEntry) * map->NumBuckets);
    map->Buckets = NULL;
}