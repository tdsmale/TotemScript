//
//  exec_type.c
//  TotemScript
//
//  Created by Timothy Smale on 03/08/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

//static int gccount = 0;

totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type)
{
    totemGCObject *hdr = totemExecState_Alloc(state, sizeof(totemGCObject));
    if (!hdr)
    {
        return NULL;
    }
    
    hdr->RefCount = 0;
    hdr->Type = type;
    hdr->CycleDetectCount = 0;
    hdr->Array = NULL;
    hdr->ExecState = state;
    hdr->Prev = NULL;
    hdr->Next = NULL;
    
    if (state->GCStart)
    {
        state->GCStart->Prev = hdr;
    }
    else
    {
        state->GCTail = hdr;
    }
    
    hdr->Next = state->GCStart;
    state->GCStart = hdr;
    
    //gccount++;
    //printf("add %i %p %s %p %p\n", gccount, hdr, totemGCObjectType_Describe(type), state->GCStart, state->GCStart2);
    
    return hdr;
}

totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj)
{
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
            
        case totemGCObjectType_Channel:
            totemExecState_DestroyChannel(state, obj->Channel);
            break;
            
        case totemGCObjectType_Deleting:
            return NULL;
    }
    
    
    totemGCObject *next = obj->Next;
    
    //printf("%p %p %p %p\n", state->GCTail2, state->GCStart, state->GCTail, state->GCStart2);
    
    if (obj->Prev)
    {
        obj->Prev->Next = obj->Next;
    }
    
    if (obj->Next)
    {
        obj->Next->Prev = obj->Prev;
    }
    
    if (state->GCStart == obj)
    {
        state->GCStart = obj->Next;
    }
    
    if (state->GCTail == obj)
    {
        state->GCTail = obj->Prev;
    }
    
    if (state->GCStart2 == obj)
    {
        state->GCStart2 = obj->Next;
    }
    
    if (state->GCTail2 == obj)
    {
        state->GCTail2 = obj->Prev;
    }
    
    obj->Next = NULL;
    obj->Prev = NULL;
    
    //gccount--;
    //printf("kill %i %p %s %p %p\n", gccount, obj, totemGCObjectType_Describe(obj->Type), state->GCStart, state->GCStart2);
    
    totem_CacheFree(obj, sizeof(totemGCObject));
    return next;
}

totemExecStatus totemExecState_IncRefCount(totemExecState *state, totemGCObject *gc)
{
    if (totem_AtomicInc64(&gc->RefCount) < 0)
    {
        return totemExecStatus_Break(totemExecStatus_RefCountOverflow);
    }
    
    //printf("inc count %s %i\n", totemGCObjectType_Describe(gc->Type), gc->RefCount);
    
    return totemExecStatus_Continue;
}

void totemExecState_DecRefCount(totemExecState *state, totemGCObject *gc)
{
    if (gc->RefCount <= 0)
    {
        return;
    }
    
    if (totem_AtomicDec64(&gc->RefCount) == 0)
    {
        if (gc->ExecState == state)
        {
            totemExecState_DestroyGCObject(state, gc);
        }
    }
    
    //printf("dec count %s %i\n", totemGCObjectType_Describe(gc->Type), gc->RefCount);
}

void totemExecState_CycleDetect(totemExecState *state, totemGCObject *gc)
{
    totemRegister *regs = NULL;
    
    size_t numRegs = 0;
    
    switch (gc->Type)
    {
        case totemGCObjectType_Array:
            regs = gc->Array->Registers;
            numRegs = gc->Array->NumRegisters;
            break;
            
        case totemGCObjectType_Coroutine:
            regs = gc->Coroutine->FrameStart;
            numRegs = gc->Coroutine->NumRegisters;
            break;
            
        case totemGCObjectType_Object:
            regs = totemMemoryBuffer_Bottom(&gc->Object->Registers);
            numRegs = totemMemoryBuffer_GetNumObjects(&gc->Object->Registers);
            break;
            
        default:
            break;
    }
    
    if (regs)
    {
        for (size_t i = 0; i < numRegs; i++)
        {
            totemRegister *reg = &regs[i];
            
            if (TOTEM_REGISTER_ISGC(reg))
            {
                totemGCObject *childGC = reg->Value.GCObject;
                
                if (childGC->CycleDetectCount > 0 && childGC->ExecState == state)
                {
                    childGC->CycleDetectCount--;
                    //printf("cycle count %s %i\n", totemGCObjectType_Describe(childGC->Type), childGC->CycleDetectCount);
                }
            }
        }
    }
}

void totemExecState_CollectGarbageList(totemExecState *state, totemGCObject **listStart)
{
    if (!*listStart)
    {
        return;
    }
    
    for (totemGCObject *obj = *listStart; obj;)
    {
        if (obj->RefCount <= 0)
        {
            obj = totemExecState_DestroyGCObject(state, obj);
        }
        else
        {
            obj->CycleDetectCount = obj->RefCount;
            obj = obj->Next;
        }
    }
    
    for (totemGCObject *obj = *listStart; obj;)
    {
        totemExecState_CycleDetect(state, obj);
        obj = obj->Next;
    }
    
    for (totemGCObject *obj = *listStart; obj;)
    {
        if (obj->CycleDetectCount <= 0)
        {
            obj = totemExecState_DestroyGCObject(state, obj);
        }
        else
        {
            obj = obj->Next;
        }
    }
}

void totemExecState_MigrateGarbage(totemExecState *state)
{
    if (state->GCStart)
    {
        if (state->GCTail2)
        {
            state->GCTail2->Next = state->GCStart;
            state->GCStart->Prev = state->GCTail2;
        }
        else
        {
            state->GCStart2 = state->GCStart;
        }
        
        state->GCTail2 = state->GCTail;
        state->GCStart = NULL;
        state->GCTail = NULL;
    }
}

void totemExecState_CollectGarbage(totemExecState *state, totemBool full)
{
    totemGCObject **toCollect = &state->GCStart;
    
    if (full)
    {
        totemExecState_MigrateGarbage(state);
        toCollect = &state->GCStart2;
    }
    
    totemExecState_CollectGarbageList(state, toCollect);
    
    if (!full)
    {
        totemExecState_MigrateGarbage(state);
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
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Channel);
        default:return "UNKNOWN";
    }
}