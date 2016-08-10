//
//  exec_gc.c
//  TotemScript
//
//  Created by Timothy Smale on 08/03/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

#if 0
#define TOTEM_GC_LOG(e) e
static int gccount = 0;
#else
#define TOTEM_GC_LOG(e)
#endif

#if 0
#define totemGCHeader_Assert(x, y) totemGCHeader_AssertList(x, y)
#else
#define totemGCHeader_Assert(x, y)
#endif

#define TOTEM_GC_MAYBE_UNREACHABLE (~((totemRefCount)0))

void totemGCHeader_AssertList(totemGCHeader *head, totemGCHeader *no)
{
    size_t i = 0;
    for (totemGCHeader *hdr = head->NextHdr; hdr != head; hdr = hdr->NextHdr)
    {
        totem_assert(no == NULL || hdr != no);
        totem_assert(i < 5000);
        totem_assert(hdr != NULL);
        i++;
    }
    
    size_t j = 0;
    for (totemGCHeader *hdr = head->PrevHdr; hdr != head; hdr = hdr->PrevHdr)
    {
        totem_assert(no == NULL || hdr != no);
        totem_assert(j < 5000);
        totem_assert(hdr != NULL);
        j++;
    }
    
    totem_assert(i == j);
}

void totemGCHeader_Init(totemGCHeader *obj)
{
    obj->NextHdr = obj;
    obj->PrevHdr = obj;
    
    totemGCHeader_Assert(obj, NULL);
}

void totemGCHeader_Unlink(totemGCHeader *obj)
{
    totemGCHeader_Assert(obj, NULL);
    
    totemGCHeader *prev = obj->PrevHdr;
    prev->NextHdr = obj->NextHdr;
    obj->NextHdr->PrevHdr = prev;
    
    totemGCHeader_Assert(prev, obj);
    totemGCHeader_Assert(obj->NextHdr, obj);
}

void totemGCHeader_Prepend(totemGCHeader *obj, totemGCHeader *to)
{
    totem_assert(obj != to);
    
    obj->PrevHdr = to->PrevHdr;
    to->PrevHdr = obj;
    
    obj->PrevHdr->NextHdr = obj;
    obj->NextHdr = to;
    
    totemGCHeader_Assert(obj, NULL);
    totemGCHeader_Assert(to, NULL);
}

void totemGCHeader_Move(totemGCHeader *obj, totemGCHeader *to)
{
    totem_assert(obj != to);
    totemGCHeader_Unlink(obj);
    totemGCHeader_Prepend(obj, to);
}

void totemGCHeader_Migrate(totemGCHeader *from, totemGCHeader *to)
{
    totem_assert(from != to);
    
    if (from->NextHdr != from)
    {
        to->PrevHdr->NextHdr = from->NextHdr;
        from->NextHdr->PrevHdr = to->PrevHdr;
        to->PrevHdr = from->PrevHdr;
        to->PrevHdr->NextHdr = to;
        totemGCHeader_Init(from);
    }
    else
    {
        totem_assert(from->PrevHdr == from);
    }
    
    totemGCHeader_Assert(from, to);
    totemGCHeader_Assert(to, from);
}

totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type)
{
    totemGCObject *hdr = NULL;
    
    if (state->GCFreeList)
    {
        hdr = state->GCFreeList;
        state->GCFreeList = hdr->Header.NextObj;
    }
    else
    {
        hdr = totemExecState_Alloc(state, sizeof(totemGCObject));
    }
    
    if (!hdr)
    {
        return NULL;
    }
    
    hdr->RefCount = 0;
    hdr->Type = type;
    hdr->CycleDetectCount = 0;
    hdr->Array = NULL;
    
    totemGCHeader_Prepend(&hdr->Header, &state->GC);
    state->NumGC++;
    
    TOTEM_GC_LOG(gccount++);
    TOTEM_GC_LOG(printf("add %i %p %s %p %p\n", gccount, hdr, totemGCObjectType_Describe(type), state->GC.NextHdr, state->GC2.NextHdr));
    
    return hdr;
}

totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj)
{
    TOTEM_GC_LOG(printf("destroying %i %p %s %p %p\n", gccount, obj, totemGCObjectType_Describe(obj->Type), obj->Header.NextHdr, obj->Header.PrevHdr));
    
    totemGCObjectType type = obj->Type;
    obj->Type = totemGCObjectType_Deleting;
    
    switch (type)
    {
        case totemGCObjectType_Array:
            totemExecState_DestroyArray(state, obj->Array);
            break;
            
        case totemGCObjectType_Coroutine:
            totemExecState_DestroyCoroutine(state, obj->Coroutine);
            break;
            
        case totemGCObjectType_Object:
            totemExecState_DestroyObject(state, obj->Object);
            break;
            
        case totemGCObjectType_Userdata:
            totemExecState_DestroyUserdata(state, obj->Userdata);
            break;
            
        case totemGCObjectType_Deleting:
            return NULL;
    }
    
    TOTEM_GC_LOG(printf("unlinking %i %p %s %p %p\n", gccount, obj, totemGCObjectType_Describe(obj->Type), obj->Header.NextHdr, obj->Header.PrevHdr));
    
    totemGCObject *next = obj->Header.NextObj;
    totemGCHeader_Unlink(&obj->Header);
    
    TOTEM_GC_LOG(gccount--);
    TOTEM_GC_LOG(printf("killing %i %p %s %p %p\n", gccount, obj, totemGCObjectType_Describe(obj->Type), obj->Header.NextHdr, obj->Header.PrevHdr));
    
    obj->Header.NextObj = state->GCFreeList;
    state->GCFreeList = obj;
    state->NumGC--;
    return next;
}

void totemExecState_CleanupGCList(totemExecState *state, totemGCHeader *hdr)
{
    totemGCHeader_Assert(hdr, NULL);
    
    for (totemGCObject *obj = hdr->NextObj; obj != (totemGCObject*)hdr;)
    {
        totemGCHeader_Assert(&obj->Header, NULL);
        obj = totemExecState_DestroyGCObject(state, obj);
        totemGCHeader_Assert(&obj->Header, NULL);
    }
    
    totemGCHeader_Init(hdr);
}

void totemExecState_IncRefCount(totemExecState *state, totemGCObject *gc)
{
    gc->RefCount++;
    
    TOTEM_GC_LOG(printf("inc count %s %i %p\n", totemGCObjectType_Describe(gc->Type), gc->RefCount, gc));
}

void totemExecState_DecRefCount(totemExecState *state, totemGCObject *gc)
{
    gc->RefCount--;
    if (gc->RefCount <= 0)
    {
        totemExecState_DestroyGCObject(state, gc);
    }
    
    TOTEM_GC_LOG(printf("dec count %s %i %p\n", totemGCObjectType_Describe(gc->Type), gc->RefCount, gc));
}

void totemExecState_GetGCRegisterList(totemExecState *state, totemGCObject *gc, totemRegister **regs,  size_t *numRegs)
{
    switch (gc->Type)
    {
        case totemGCObjectType_Array:
            *regs = gc->Array->Registers;
            *numRegs = gc->Array->NumRegisters;
            break;
            
        case totemGCObjectType_Coroutine:
            *regs = gc->Coroutine->FrameStart;
            *numRegs = gc->Coroutine->NumRegisters;
            break;
            
        case totemGCObjectType_Object:
            *regs = totemMemoryBuffer_Bottom(&gc->Object->Registers);
            *numRegs = totemMemoryBuffer_GetNumObjects(&gc->Object->Registers);
            break;
            
        default:
            return;
    }
}

void totemExecState_CycleDoubleCheck(totemExecState *state, totemGCObject *gc, totemGCHeader *reachable)
{
    totemRegister *regs = NULL;
    size_t numRegs = 0;
    
    totemExecState_GetGCRegisterList(state, gc, &regs, &numRegs);
    
    if (regs)
    {
        for (size_t i = 0; i < numRegs; i++)
        {
            totemRegister *reg = &regs[i];
            
            if (TOTEM_REGISTER_ISGC(reg))
            {
                totemGCObject *childGC = reg->Value.GCObject;
                
                if (childGC->CycleDetectCount == TOTEM_GC_MAYBE_UNREACHABLE)
                {
                    // this is reachable but was prematurely marked as unreachable, ensure it isn't collected
                    totemGCHeader_Move(&childGC->Header, reachable);
                }
                
                // reachable, ensure it isn't marked otherwise in the near future
                childGC->CycleDetectCount = 1;
            }
        }
    }
}

void totemExecState_CycleDetect(totemExecState *state, totemGCObject *gc)
{
    totemRegister *regs = NULL;
    size_t numRegs = 0;
    
    totemExecState_GetGCRegisterList(state, gc, &regs, &numRegs);
    
    if (regs)
    {
        for (size_t i = 0; i < numRegs; i++)
        {
            totemRegister *reg = &regs[i];
            
            if (TOTEM_REGISTER_ISGC(reg))
            {
                totemGCObject *childGC = reg->Value.GCObject;
                
                if (childGC->CycleDetectCount > 0)
                {
                    childGC->CycleDetectCount--;
                    TOTEM_GC_LOG(printf("cycle count %s %i\n", totemGCObjectType_Describe(childGC->Type), childGC->CycleDetectCount));
                }
            }
        }
    }
}

void totemExecState_CollectGarbageList(totemExecState *state, totemGCHeader *listHead)
{
    totemGCHeader unreachable;
    totemGCHeader_Init(&unreachable);
    
    for (totemGCObject *obj = listHead->NextObj; obj != (totemGCObject*)listHead;)
    {
        if (obj->RefCount <= 0)
        {
            obj = totemExecState_DestroyGCObject(state, obj);
        }
        else
        {
            obj->CycleDetectCount = obj->RefCount;
            obj = obj->Header.NextObj;
        }
    }
    
    totemGCHeader_Assert(listHead, NULL);
    
    for (totemGCObject *obj = listHead->NextObj; obj != (totemGCObject*)listHead;)
    {
        totemGCHeader_Assert(obj, NULL);
        totemExecState_CycleDetect(state, obj);
        obj = obj->Header.NextObj;
    }
    
    for (totemGCObject *obj = listHead->NextObj; obj != (totemGCObject*)listHead;)
    {
        totemGCObject *next = obj->Header.NextObj;
        
        totemGCHeader_Assert(obj, NULL);
        if (obj->CycleDetectCount)
        {
            // object is reachable from outside this set, nsure all its members are considered reachable as well
            totemExecState_CycleDoubleCheck(state, obj, listHead);
            totemGCHeader_Assert(obj, NULL);
        }
        else
        {
            // assume this is unreachable
            obj->CycleDetectCount = TOTEM_GC_MAYBE_UNREACHABLE;
            totemGCHeader_Move(&obj->Header, &unreachable);
            totemGCHeader_Assert(listHead, NULL);
            totemGCHeader_Assert(obj, NULL);
        }
        
        obj = next;
    }
    
    totemGCHeader_Assert(listHead, NULL);
    totemGCHeader_Assert(&unreachable, NULL);
    
    // free unreachable objects
    totemExecState_CleanupGCList(state, &unreachable);
    totemGCHeader_Assert(listHead, NULL);
}

/**
 * Detects reference-count cycles
 * Pretty much the same solution found in cpython
 *
 * For a more in-depth explanation:
 * http://www.arctrix.com/nas/python/gc/
 */
void totemExecState_CollectGarbage(totemExecState *state, totemBool full)
{
    totemGCHeader_Assert(&state->GC, &state->GC2);
    totemGCHeader_Assert(&state->GC2, &state->GC);
    
    totemGCHeader *toCollect = &state->GC;
    
    if (full)
    {
        totemGCHeader_Migrate(&state->GC, &state->GC2);
        totemGCHeader_Assert(&state->GC, &state->GC2);
        totemGCHeader_Assert(&state->GC2, &state->GC);
        toCollect = &state->GC2;
    }
    
    totemExecState_CollectGarbageList(state, toCollect);
    
    totemGCHeader_Assert(&state->GC, &state->GC2);
    totemGCHeader_Assert(&state->GC2, &state->GC);
    
    if (!full)
    {
        totemGCHeader_Migrate(&state->GC, &state->GC2);
        totemGCHeader_Assert(&state->GC, &state->GC2);
        totemGCHeader_Assert(&state->GC2, &state->GC);
    }
}

const char *totemGCObjectType_Describe(totemGCObjectType type)
{
    switch (type)
    {
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Array);
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Coroutine);
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Object);
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Userdata);
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Deleting);
        default:return "UNKNOWN";
    }
}