//
//  exec_type.c
//  TotemScript
//
//  Created by Timothy Smale on 03/08/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

//static int gc = 0;

totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type)
{
    //gc++;
    //printf("add %i\n", gc);
    
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
    //gc--;
    //printf("kill %i\n", gc);
    totem_CacheFree(obj, sizeof(totemGCObject));
}

void totemExecState_DecRefCount(totemExecState *state, totemGCObject *gc)
{
    switch (gc->RefCount)
    {
        case 0:
            break;
            
        case 1:
            gc->RefCount = 0;
            
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
        }
            
            totemExecState_DestroyGCObject(state, gc);
            break;
            
        default:
            gc->RefCount--;
            break;
    }
}