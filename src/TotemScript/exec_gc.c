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
    //printf("add %i\n", gccount);
    
    totemGCObject *hdr = totem_CacheMalloc(sizeof(totemGCObject));
    if (!hdr)
    {
        return NULL;
    }
    
    hdr->RefCount = 0;
    hdr->Type = type;
    hdr->CycleDetectCount = 0;
    hdr->Array = NULL;
    return hdr;
}

void totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj)
{
    //gccount--;
    //printf("kill %i\n", gccount);
    totem_CacheFree(obj, sizeof(totemGCObject));
}

totemExecStatus totemExecState_IncRefCount(totemExecState *state, totemGCObject *gc)
{
    if (totem_AtomicInc64(&gc->RefCount) < 0)
    {
        return totemExecStatus_Break(totemExecStatus_RefCountOverflow);
    }
    
    //printf("ref count %s %i\n", totemGCObjectType_Describe(gc->Type), gc->RefCount);
    
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
        switch (gc->Type)
        {
            case totemGCObjectType_Array:
                totemExecState_DestroyArray(state, gc->Array);
                break;
                
            case totemGCObjectType_Coroutine:
                totemExecState_DestroyCoroutine(state, gc->Coroutine);
                break;
                
            case totemGCObjectType_Object:
                totemExecState_DestroyObject(state, gc->Object);
                break;
                
            case totemGCObjectType_Userdata:
                totemExecState_DestroyUserdata(state, gc->Userdata);
                break;
        }
        
        totemExecState_DestroyGCObject(state, gc);
    }
    
    //printf("ref count %s %i\n", totemGCObjectType_Describe(gc->Type), gc->RefCount);
}

const char *totemGCObjectType_Describe(totemGCObjectType type)
{
    switch (type)
    {
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Array);
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Coroutine);
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Object);
            TOTEM_STRINGIFY_CASE(totemGCObjectType_Userdata);
        default:return "UNKNOWN";
    }
}