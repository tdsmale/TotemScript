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
    //gccount++;
    //printf("add %i %s\n", gccount, totemGCObjectType_Describe(type));
    
    totemGCObject *hdr = NULL;
    
    if (state->GCFreeList)
    {
        hdr = state->GCFreeList;
        state->GCFreeList = hdr->Next;
    }
    else
    {
        hdr = totem_CacheMalloc(sizeof(totemGCObject));
        if (!hdr)
        {
            return NULL;
        }
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
    
    hdr->Next = state->GCStart;
    state->GCStart = hdr;
    
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
    
    //gccount--;
    //printf("kill %i %p %s\n", gccount, obj, totemGCObjectType_Describe(obj->Type));
    totemGCObject *next = obj->Next;
    
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
    
    obj->Next = NULL;
    obj->Prev = NULL;
    
    if (state->GCFreeList)
    {
        obj->Next = state->GCFreeList;
    }
    
    state->GCFreeList = obj;
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

void totemExecState_CollectGarbage(totemExecState *state)
{
    //int count = 0;
    
    for (totemGCObject *obj = state->GCStart; obj;)
    {
        //printf("%p %p\n", obj, obj->Next);
        
        //count++;
        
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
    
    //printf("%i\n", count);
    //count = 0;
    
    for (totemGCObject *obj = state->GCStart; obj;)
    {
        //printf("%p %p\n", obj, obj->Next);
        //count++;
        totemExecState_CycleDetect(state, obj);
        obj = obj->Next;
    }
    
    //printf("%i\n", count);
    //count = 0;
    
    for (totemGCObject *obj = state->GCStart; obj;)
    {
        //printf("%p %p\n", obj, obj->Next);
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