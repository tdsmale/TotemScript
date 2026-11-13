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
    
	TOTEM_ASSERT(index < TOTEM_MEM_NUM_FREELISTS);
    totemMemoryFreeList *list = &s_FreeLists[index];
    
    TOTEM_ASSERT(amount <= list->ObjectSize);
    TOTEM_ASSERT(list == &s_FreeLists[0] ? 1 : amount > (list - 1)->ObjectSize);
    
    return list;
}

void *totem_CacheMalloc(size_t amount, size_t line, const char *file)
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
                    totemMemoryPageObject* obj = (totemMemoryPageObject*)(page->Data + (page->NumAllocated * freeList->ObjectSize));
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
        TOTEM_LOG_V(Memory, Free, Debug, "Allocated pointer %p %i %s:%i", ptr, freeList->ObjectSize, file, line);
    }
    
    return ptr;
}

void totem_CacheFree(void *ptr, size_t amount, size_t line, const char *file)
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
    
    TOTEM_LOG_BEGIN(Memory, Free, Debug, logger);
    {
        totemLog_Write(logger, "Freeing pointer %p %i %s:%i\n", ptr, freeList->ObjectSize, file, line);

        totemBool found = totemBool_False;
        for(totemMemoryPage *page = freeList->HeadPage; page; page = page->Next)
        {
            char *start = page->Data;
            char *end = start + sizeof(page->Data) - freeList->ObjectSize;
            totemBool isBetween = ptr >= (void*)start && ptr <= (void*)end;
            
            totemLog_Write(logger, "Found page page(%p) start(%p) end(%p) ptr(%p) isBetween(%i)\n", page, start, end, ptr, isBetween);
            
            if(isBetween)
            {
                TOTEM_ASSERT(!found);
                found = totemBool_True;
            }
        }
    
        TOTEM_ASSERT(found);
    }
    TOTEM_LOG_END();
    
    totemMemoryPageObject *obj = ptr;
    memset(obj, 0xDEADBEEF, amount);
    totemLock_Acquire(&freeList->Lock);
    obj->Next = freeList->HeadObject;
    freeList->HeadObject = obj;
    totemLock_Release(&freeList->Lock);
}

void *totem_CacheRealloc(void *ptr, size_t oldLen, size_t newLen, size_t line, const char *file)
{
    char *newMem = (char*)totem_CacheMalloc(newLen, line, file);
    if (newMem == NULL)
    {
        return NULL;
    }
    
    if(oldLen < newLen)
    {
        memcpy(newMem, ptr, oldLen);
        TOTEM_ZERO_N(newMem + oldLen, newLen - oldLen);
    }
    else
    {
        memcpy(newMem, ptr, newLen);
    }
    
    totem_CacheFree(ptr, oldLen, line, file);
    return newMem;
}

void *totem_CacheCopy(void *ptr, size_t len, size_t line, const char *file)
{
    char *newMem = (char*)totem_CacheMalloc(len, line, file);
    if(newMem == NULL)
    {
        return NULL;
    }
    
    memcpy(newMem, ptr, len);
    return newMem;
}

void totemBuffer_Init(totemBuffer *buff)
{
    buff->Len = 0;
    buff->Ptr = NULL;
}

void totemBuffer_Reset(totemBuffer *buff, size_t elemSize)
{
    totemBuffer_Cleanup(buff, elemSize);
}

void totemBuffer_Cleanup(totemBuffer *buff, size_t elemSize)
{
    TOTEM_FREE(buff->Ptr, buff->Len * elemSize);
    buff->Ptr = NULL;
    buff->Len = 0;
}

char *totemBuffer_Top(totemBuffer *buffer, size_t elemSize)
{
    if(buffer->Len)
    {
        return buffer->Ptr + (elemSize * (buffer->Len - 1));
    }
    
    return NULL;
}

char *totemBuffer_Bottom(totemBuffer *buffer)
{
    if(buffer->Len)
    {
        return buffer->Ptr;
    }
    
    return NULL;
}

char *totemBuffer_At(totemBuffer *buffer, size_t at, size_t elemSize)
{
    if(at < buffer->Len)
    {
        return buffer->Ptr + (elemSize * at);
    }
    
    return NULL;
}

totemBool totemBuffer_Reserve(totemBuffer *buffer, size_t len, size_t elemSize)
{
    if(buffer->Len < len)
    {
        char *newPtr = TOTEM_REALLOC(buffer->Ptr, buffer->Len * elemSize, len * elemSize);
        if(!newPtr)
        {
            buffer->Len = len;
            buffer->Ptr = newPtr;
            return totemBool_True;
        }
    }
    
    return totemBool_False;
}

size_t totemBuffer_Length(totemBuffer *buffer)
{
    return buffer->Len;
}

void totemDynBuffer_Init(totemDynBuffer *buf)
{
    buf->Max = 0;
    buf->Ptr = NULL;
    totemDynBuffer_Reset(buf);
}

void totemDynBuffer_Reset(totemDynBuffer *buf)
{
    buf->Len = 0;
}

void totemDynBuffer_Cleanup(totemDynBuffer *buf, size_t elemSize)
{
    if (buf->Ptr)
    {
        TOTEM_FREE(buf->Ptr, buf->Max * elemSize);
        buf->Ptr = NULL;
        buf->Len = 0;
        buf->Max = 0;
    }
}

char *totemDynBuffer_Top(totemDynBuffer *buf, size_t elemSize)
{
    if(buf->Len)
    {
        return &buf->Ptr[(buf->Len * elemSize) - elemSize];
    }
    
    return NULL;
}

char *totemDynBuffer_Bottom(totemDynBuffer *buf)
{
    if(buf->Len)
    {
        return buf->Ptr;
    }
    
    return NULL;
}

char *totemDynBuffer_Push(totemDynBuffer *buf, char *data, size_t len, size_t elemSize)
{
    char* ptr = totemDynBuffer_Secure(buf, len, elemSize);
    if (!ptr)
    {
        return NULL;
    }

    memcpy(ptr, data, len * elemSize);
    return ptr;
}

totemBool totemDynBuffer_Pop(totemDynBuffer *buf, size_t size)
{
    if(buf->Len >= size)
    {
        buf->Len -= size;
        return totemBool_True;
    }
    
    return totemBool_False;
}

char *totemDynBuffer_At(totemDynBuffer *buf, size_t at, size_t elemSize)
{
    if(buf->Len > at)
    {
        return buf->Ptr + (at * elemSize);
    }
    
    return NULL;
}

totemBool totemDynBuffer_Reserve(totemDynBuffer *buf, size_t needed, size_t elemSize)
{
    // allocate space for x elements
    if (needed >= buf->Max)
    {
        size_t toAlloc = 0;
        if (buf->Max == 0)
        {
            toAlloc = 8;
        }
        else
        {
            toAlloc = buf->Max * 2;
        }
        if (needed > toAlloc)
        {
            toAlloc = needed;
        }
        char *newMem = TOTEM_REALLOC(buf->Ptr, buf->Max * elemSize, toAlloc * elemSize);
        if (newMem == NULL)
        {
            return totemBool_False;
        }
        buf->Max = toAlloc;
        buf->Ptr = newMem;
    }
    
    return totemBool_True;
}

char *totemDynBuffer_Secure(totemDynBuffer *buf, size_t len, size_t elemSize)
{
    // add x elements
    size_t needed = buf->Len + len;
    
    if(!totemDynBuffer_Reserve(buf, needed, elemSize))
    {
        return NULL;
    }
    
    char *ptr = buf->Ptr + (buf->Len * elemSize);
    buf->Len += len;
    return ptr;
}

size_t totemDynBuffer_Length(totemDynBuffer *buf)
{
    return buf->Len;
}

size_t totemDynBuffer_Max(totemDynBuffer *buf)
{
    return buf->Max;
}
