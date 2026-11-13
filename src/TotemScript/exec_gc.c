//
//  exec_gc.c
//  TotemScript
//
//  Created by Timothy Smale on 08/03/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

#define TOTEM_GC_DEBUG_LOG_V(format, ...) TOTEM_LOG_V(Exec, GC, Debug, format, __VA_ARGS__)
#define TOTEM_GC_DEBUG_LOG(format) TOTEM_LOG(Exec, GC, Debug, format)

#define totemGCHeader_Assert(head, no) \
    if(TOTEM_LOG_IS_COMPILED_AND_ACTIVE(Exec, GC, Debug)) \
    { \
        totemGCHeader_AssertList(head, no); \
    }

#define totemGCObject_Assert(obj) \
    if(TOTEM_LOG_IS_COMPILED_AND_ACTIVE(Exec, GC, Debug)) \
    { \
        totemGCObject_AssertLiveliness(obj); \
    }

void totemGCHeader_AssertList(totemGCHeader *head, totemGCHeader *no)
{
    TOTEM_ASSERT(head->Prev.Hdr && head->Next.Hdr);
    
    size_t i = 0;
    for (totemGCHeader *hdr = head->Next.Hdr; hdr != head; hdr = hdr->Next.Hdr)
    {
        TOTEM_ASSERT(hdr->Prev.Hdr && hdr->Next.Hdr);
        TOTEM_ASSERT(no == NULL || hdr != no);
        TOTEM_ASSERT(i < 5000);
        TOTEM_ASSERT(hdr != NULL);
        i++;
    }
    
    if (!i)
    {
        TOTEM_ASSERT(head->Prev.Hdr == head && head->Next.Hdr == head);
    }
    
    size_t j = 0;
    for (totemGCHeader *hdr = head->Prev.Hdr; hdr != head; hdr = hdr->Prev.Hdr)
    {
        TOTEM_ASSERT(hdr->Prev.Hdr && hdr->Next.Hdr);
        TOTEM_ASSERT(no == NULL || hdr != no);
        TOTEM_ASSERT(j < 5000);
        TOTEM_ASSERT(hdr != NULL);
        j++;
    }
    
    if (!j)
    {
        TOTEM_ASSERT(head->Prev.Hdr == head && head->Next.Hdr == head);
    }
    
    TOTEM_ASSERT(i == j);
}

void totemGCObject_AssertLiveliness(totemGCObject *obj)
{
    TOTEM_ASSERT(TOTEM_HASBITS(obj->Flags, totemGCObjectFlag_IsUsed));
    totemGCHeader_Assert(&obj->Header, NULL);
    TOTEM_ASSERT(obj->Header.Prev.Hdr && obj->Header.Next.Hdr);
    totemGCHeader_Assert(obj->Header.Next.Hdr, NULL);
    totemGCHeader_Assert(obj->Header.Prev.Hdr, NULL);
}

void totemGCHeader_Reset(totemGCHeader *obj)
{
    obj->Next.Hdr = obj;
    obj->Prev.Hdr = obj;
    
    totemGCHeader_Assert(obj, NULL);
}

totemBool totemGCHeader_IsEmpty(totemGCHeader *hdr)
{
    TOTEM_ASSERT(hdr->Next.Hdr && hdr->Prev.Hdr);
    totemGCHeader_Assert(hdr, NULL);
    return hdr->Next.Hdr == hdr;
}

void totemGCHeader_Unlink(totemGCHeader *obj)
{
    TOTEM_ASSERT(obj->Next.Hdr && obj->Prev.Hdr);
    totemGCHeader_Assert(obj, NULL);
    
    totemGCHeader *prev = obj->Prev.Hdr;
    prev->Next.Hdr = obj->Next.Hdr;
    obj->Next.Hdr->Prev.Hdr = prev;
    
    totemGCHeader_Assert(prev, obj);
    totemGCHeader_Assert(obj->Next.Hdr, obj);
    totemGCHeader_Assert(obj->Prev.Hdr, obj);
    totemGCHeader_Assert(obj->Next.Hdr, obj);
}

void totemGCHeader_Push(totemGCHeader *obj, totemGCHeader *to)
{
    TOTEM_ASSERT(obj != to);
    totemGCHeader_Assert(to, obj);
    
    obj->Prev.Hdr = to->Prev.Hdr;
    to->Prev.Hdr = obj;
    
    obj->Prev.Hdr->Next.Hdr = obj;
    obj->Next.Hdr = to;
    
    totemGCHeader_Assert(obj, NULL);
    totemGCHeader_Assert(to, NULL);
    TOTEM_ASSERT(obj->Prev.Hdr && obj->Next.Hdr);
    TOTEM_ASSERT(to->Prev.Hdr && to->Next.Hdr);
}

void totemGCHeader_Move(totemGCHeader *obj, totemGCHeader *to)
{
    TOTEM_ASSERT(obj != to);
    totemGCHeader_Unlink(obj);
    totemGCHeader_Push(obj, to);
    TOTEM_ASSERT(obj->Prev.Hdr && obj->Next.Hdr);
    TOTEM_ASSERT(to->Prev.Hdr && to->Next.Hdr);
}

void totemGCHeader_Migrate(totemGCHeader *from, totemGCHeader *to)
{
    TOTEM_ASSERT(from != to);
    totemGCHeader_Assert(from, to);
    totemGCHeader_Assert(to, from);
    
    if (!totemGCHeader_IsEmpty(from))
    {
        to->Prev.Hdr->Next.Hdr = from->Next.Hdr;
        from->Next.Hdr->Prev.Hdr = to->Prev.Hdr;
        to->Prev.Hdr = from->Prev.Hdr;
        to->Prev.Hdr->Next.Hdr = to;
        totemGCHeader_Reset(from);
    }
    else
    {
        TOTEM_ASSERT(from->Prev.Hdr == from);
    }
    
    TOTEM_ASSERT(from->Prev.Hdr && from->Next.Hdr);
    TOTEM_ASSERT(to->Prev.Hdr && to->Next.Hdr);
    totemGCHeader_Assert(from, to);
    totemGCHeader_Assert(to, from);
}

void totemExecState_CleanupGCList(totemExecState *state, totemGCHeader *hdr)
{
    totemGCHeader_Assert(hdr, NULL);
    
    for (totemGCObject *obj = hdr->Next.Obj; obj != (totemGCObject*)hdr;)
    {
        totemGCHeader_Assert(&obj->Header, NULL);
        obj = totemExecState_DestroyGCObject(state, obj);
        totemGCHeader_Assert(&obj->Header, NULL);
    }
    
    totemGCHeader_Reset(hdr);
}

void totemExecState_InitGC(totemExecState *state)
{
    state->GCState = totemMarkSweepState_Reset;
    state->GCCurrentBit = totemBool_True;
    totemGCHeader_Reset(&state->GCWhite);
    totemGCHeader_Reset(&state->GCGrey);
    totemGCHeader_Reset(&state->GCBlack);
    totemGCHeader_Reset(&state->GCRoots);
    totemGCHeader_Reset(&state->GCSweep);
}

void totemExecState_CleanupGC(totemExecState *state)
{
    totemExecState_CleanupGCList(state, &state->GCWhite);
    totemExecState_CleanupGCList(state, &state->GCGrey);
    totemExecState_CleanupGCList(state, &state->GCBlack);
    totemExecState_CleanupGCList(state, &state->GCRoots);
    totemExecState_CleanupGCList(state, &state->GCSweep);
}

void totemExecState_MoveRoot(totemExecState *state, totemGCObject *gc)
{
    TOTEM_GC_DEBUG_LOG_V("Moving object %p to root", gc);
    TOTEM_UNSETBITS(gc->Flags, totemGCObjectFlag_IsGrey);
    totemGCObject_Assert(gc);
    totemGCHeader_Move(&gc->Header, &state->GCRoots);
    totemGCHeader_Assert(&state->GCRoots, NULL);
    totemGCHeader_Assert(&state->GCWhite, &gc->Header);
    totemGCHeader_Assert(&state->GCGrey, &gc->Header);
    totemGCHeader_Assert(&state->GCBlack, &gc->Header);
    totemGCHeader_Assert(&state->GCSweep, &gc->Header);
    totemGCObject_Assert(gc);
}

void totemExecState_MoveGrey(totemExecState *state, totemGCObject *gc)
{
    TOTEM_GC_DEBUG_LOG_V("Moving object %p to grey", gc);
    TOTEM_SETBITS(gc->Flags, totemGCObjectFlag_IsGrey);
    totemGCObject_Assert(gc);
    totemGCHeader_Move(&gc->Header, &state->GCGrey);
    totemGCHeader_Assert(&state->GCRoots, &gc->Header);
    totemGCHeader_Assert(&state->GCWhite, &gc->Header);
    totemGCHeader_Assert(&state->GCGrey, NULL);
    totemGCHeader_Assert(&state->GCBlack, &gc->Header);
    totemGCHeader_Assert(&state->GCSweep, &gc->Header);
    totemGCObject_Assert(gc);
}

void totemExecState_MoveBlack(totemExecState *state, totemGCObject *gc)
{
    TOTEM_GC_DEBUG_LOG_V("Moving object %p to black", gc);
    TOTEM_UNSETBITS(gc->Flags, totemGCObjectFlag_IsGrey);
    totemGCObject_Assert(gc);
    totemGCHeader_Move(&gc->Header, &state->GCBlack);
    totemGCHeader_Assert(&state->GCRoots, &gc->Header);
    totemGCHeader_Assert(&state->GCWhite, &gc->Header);
    totemGCHeader_Assert(&state->GCGrey, &gc->Header);
    totemGCHeader_Assert(&state->GCBlack, NULL);
    totemGCHeader_Assert(&state->GCSweep, &gc->Header);
    totemGCObject_Assert(gc);
}

void totemExecState_SetMark(totemExecState *state, totemGCObject *gc)
{
    TOTEM_ASSERT(state->GCCurrentBit == 1 || state->GCCurrentBit == 0);
    TOTEM_FORCEBITS_OFFSET(gc->Flags, state->GCCurrentBit, totemGCObjectFlag_Mark >> 1);
    TOTEM_GC_DEBUG_LOG_V("Marking object %p %i %i", gc, state->GCCurrentBit, TOTEM_GETBITS(gc->Flags, totemGCObjectFlag_Mark));
}

void totemExecState_UnsetMark(totemExecState *state, totemGCObject *gc)
{
    TOTEM_ASSERT(state->GCCurrentBit == 1 || state->GCCurrentBit == 0);
    TOTEM_FORCEBITS_OFFSET(gc->Flags, !state->GCCurrentBit, totemGCObjectFlag_Mark >> 1);
    TOTEM_GC_DEBUG_LOG_V("Unmarking object %p %i %i", gc, state->GCCurrentBit, TOTEM_GETBITS(gc->Flags, totemGCObjectFlag_Mark));
}

totemBool totemExecState_HasMark(totemExecState *state, totemGCObject *gc)
{
    TOTEM_ASSERT(state->GCCurrentBit == 1 || state->GCCurrentBit == 0);
    totemBool result = TOTEM_GETBITS(gc->Flags, totemGCObjectFlag_Mark) == state->GCCurrentBit;
    TOTEM_GC_DEBUG_LOG_V("Marked? %p %i %i %i", gc, state->GCCurrentBit, TOTEM_GETBITS(gc->Flags, totemGCObjectFlag_Mark), result);
    
    return result;
}

void totemExecState_AppendNewGCObject(totemExecState *state, totemGCObject *gc)
{
    totemExecState_UnsetMark(state, gc);
    
    if(TOTEM_HASBITS(gc->Flags, totemGCObjectFlag_IsRoot))
    {
        TOTEM_GC_DEBUG_LOG_V("Moving new object %p to root", gc);
        totemGCHeader_Push(&gc->Header, &state->GCRoots);
        totemGCHeader_Assert(&state->GCRoots, NULL);
        totemGCHeader_Assert(&state->GCWhite, &gc->Header);
        totemGCHeader_Assert(&state->GCGrey, &gc->Header);
        totemGCHeader_Assert(&state->GCBlack, &gc->Header);
        totemGCHeader_Assert(&state->GCSweep, &gc->Header);
    }
    else
    {
        TOTEM_GC_DEBUG_LOG_V("Moving new object %p to white", gc);
        totemGCHeader_Push(&gc->Header, &state->GCWhite);
        totemGCHeader_Assert(&state->GCRoots, &gc->Header);
        totemGCHeader_Assert(&state->GCWhite, NULL);
        totemGCHeader_Assert(&state->GCGrey, &gc->Header);
        totemGCHeader_Assert(&state->GCBlack, &gc->Header);
        totemGCHeader_Assert(&state->GCSweep, &gc->Header);
    }
    
    totemGCObject_Assert(gc);
    state->GCNum++;
}

void totemExecState_WriteBarrier(totemExecState *state, totemGCObject *gc)
{
    if (!TOTEM_HASBITS(gc->Flags, totemGCObjectFlag_IsGrey))
    {
        TOTEM_GC_DEBUG_LOG_V("Write barrier %p", gc);
        totemGCObject_Assert(gc);
        totemExecState_SetMark(state, gc);
        totemGCObject_Assert(gc);
        totemExecState_MoveGrey(state, gc);
        totemGCObject_Assert(gc);
    }
}

void totemExecState_TraverseRegisterList(totemExecState *state, totemRegister *regs, uint32_t num)
{
	for (uint32_t i = 0; i < num; i++)
    {
        totemRegister *reg = &regs[i];
        totemGCObject *obj = NULL;
        
        if (totemRegister_IsGarbageCollected(reg))
        {
            obj = totemRegister_GetGCObject(reg);
        }
        else if (totemRegister_IsInstanceFunction(reg))
        {
            obj = totemRegister_GetInstanceFunction(reg)->Instance;
        }
        
        if(obj)
        {
            totemGCObject_Assert(obj);
            if (!totemExecState_HasMark(state, obj))
            {
                totemExecState_SetMark(state, obj);
                
                if (obj->NumRegisters)
                {
                    totemGCObject_Assert(obj);
                    totemExecState_MoveGrey(state, obj);
                    totemGCObject_Assert(obj);
                }
                else if (TOTEM_HASBITS(obj->Flags, totemGCObjectFlag_IsRoot))
                {
                    totemGCObject_Assert(obj);
                    totemExecState_MoveRoot(state, obj);
                    totemGCObject_Assert(obj);
                }
                else
                {
                    totemGCObject_Assert(obj);
                    totemExecState_MoveBlack(state, obj);
                    totemGCObject_Assert(obj);
                }
            }
        }
    }
}

totemGCObject *totemExecState_TraverseGCObject(totemExecState *state, totemGCObject *obj)
{
    totemExecState_TraverseRegisterList(state, obj->Data.Registers, obj->NumRegisters);
    
    totemGCObject *next = obj->Header.Next.Obj;
    
    if (TOTEM_HASBITS(obj->Flags, totemGCObjectFlag_IsRoot))
    {
        totemGCObject_Assert(obj);
        totemExecState_MoveRoot(state, obj);
        totemGCObject_Assert(obj);
    }
    else
    {
        totemGCObject_Assert(obj);
        totemExecState_MoveBlack(state, obj);
        totemGCObject_Assert(obj);
    }
    
    return next;
}

size_t totemExecState_MoveRootsGrey(totemExecState *state)
{
    size_t amount = 0;
    
    for (totemGCObject *obj = state->GCRoots.Next.Obj; &obj->Header != &state->GCRoots; obj = obj->Header.Next.Obj)
    {
        totemGCObject_Assert(obj);
        totemExecState_SetMark(state, obj);
        totemGCObject_Assert(obj);
        amount++;
    }
    
    totemGCHeader_Migrate(&state->GCRoots, &state->GCGrey);
    return amount;
}

size_t totemExecState_MarkSweepStep(totemExecState *state)
{
    size_t amount = 0;
    
    switch (state->GCState)
    {
        case totemMarkSweepState_Reset:
            TOTEM_GC_DEBUG_LOG("MARK AND SWEEP STATE RESET");
            
            // mark roots grey
            amount += totemExecState_MoveRootsGrey(state);
            state->GCState = totemMarkSweepState_Mark;
            break;
            
        case totemMarkSweepState_Mark:
            TOTEM_GC_DEBUG_LOG("MARK AND SWEEP STATE MARK");
            if (!totemGCHeader_IsEmpty(&state->GCGrey))
            {
                totemGCObject *obj = state->GCGrey.Next.Obj;
                amount += obj->NumRegisters;
                totemGCObject_Assert(obj);
                totemExecState_TraverseGCObject(state, obj);
                totemGCObject_Assert(obj);
            }
            
            if (totemGCHeader_IsEmpty(&state->GCGrey))
            {
                TOTEM_GC_DEBUG_LOG("MARK AND SWEEP STATE MARK FINISH");
                
                // check stack right before we start sweeping so we don't need to use a write-barrier for it
                for (totemFunctionCall *call = state->CallStack; call; call = call->Prev)
                {
                    amount += call->NumRegisters;
                    totemExecState_TraverseRegisterList(state, call->FrameStart, call->NumRegisters);
                }

                // extinguish grey list
                for (totemGCObject *obj = state->GCGrey.Next.Obj; &obj->Header != &state->GCGrey;)
                {
                    totemGCObject_Assert(obj);
                    amount += obj->NumRegisters;
                    obj = totemExecState_TraverseGCObject(state, obj);
                }
                
                // anything in black or roots is now assumed to be accessible, so we must keep them
                // anything left in white is now assumed to be inaccessible and needs freeing
                totemGCHeader_Migrate(&state->GCWhite, &state->GCSweep);
                totemGCHeader_Migrate(&state->GCBlack, &state->GCWhite);
                //TOTEM_GC_DEBUG_LOG_V("Flipping bit: %i", state->GCCurrentBit));
                state->GCCurrentBit = !state->GCCurrentBit;
                TOTEM_GC_DEBUG_LOG_V("Flipping bit: %i", state->GCCurrentBit);
                state->GCState = totemMarkSweepState_Sweep;
            }
            break;
            
        case totemMarkSweepState_Sweep:
            TOTEM_GC_DEBUG_LOG("MARK AND SWEEP STATE SWEEP");
            if (!totemGCHeader_IsEmpty(&state->GCSweep))
            {
                totemGCObject *obj = state->GCSweep.Next.Obj;
                totemGCObject_Assert(obj);
                totemExecState_DestroyGCObject(state, obj);
                amount++;
            }
            
            if (totemGCHeader_IsEmpty(&state->GCSweep))
            {
                TOTEM_GC_DEBUG_LOG("MARK AND SWEEP STATE SWEEP FINISH");
                
                state->GCState = totemMarkSweepState_Reset;
            }
            break;
    }
    
    return amount;
}

// tri-colour, incremental mark-and-sweep
void totemExecState_CollectGarbage(totemExecState *state, totemBool full)
{
    totemGCHeader_Assert(&state->GCBlack, NULL);
    totemGCHeader_Assert(&state->GCWhite, NULL);
    totemGCHeader_Assert(&state->GCGrey, NULL);
    totemGCHeader_Assert(&state->GCRoots, NULL);
    totemGCHeader_Assert(&state->GCSweep, NULL);
    
    if (full)
    {
        // finish previous cycle
        while (state->GCState != totemMarkSweepState_Reset)
        {
            totemExecState_MarkSweepStep(state);
        }
        
        // do another one
        totemExecState_MarkSweepStep(state);
        while (state->GCState != totemMarkSweepState_Reset)
        {
            totemExecState_MarkSweepStep(state);
        }
    }
    else if (state->GCNumBytes > state->GCByteThreshold)
    {
        size_t expected = 8; // BS magic heuristic
        size_t done = 0;
        size_t step = 0;
        do
        {
            step = totemExecState_MarkSweepStep(state);
            done += step;
        }
        while (done < expected && step && state->GCState != totemMarkSweepState_Reset);
        
        state->GCByteThreshold += (sizeof(totemGCObject) * (expected - done));
    }
    
    totemGCHeader_Assert(&state->GCBlack, NULL);
    totemGCHeader_Assert(&state->GCWhite, NULL);
    totemGCHeader_Assert(&state->GCGrey, NULL);
    totemGCHeader_Assert(&state->GCRoots, NULL);
    totemGCHeader_Assert(&state->GCSweep, NULL);
}

totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type, uint32_t numRegisters, totemBool isRoot)
{
    totemGCObject *hdr = NULL;
    
    if (state->GCFreeList)
    {
        hdr = state->GCFreeList;
        state->GCFreeList = hdr->Header.Next.Obj;
    }
    else
    {
        hdr = totemExecState_Alloc(state, sizeof(totemGCObject));
    }
    
    if (!hdr)
    {
        return NULL;
    }
    
    hdr->Type = type;
	hdr->Ptr.Coroutine = NULL;
    hdr->Data.Registers = NULL;
    hdr->NumRegisters = numRegisters;
    hdr->Flags = totemGCObjectFlag_None;
    hdr->Header.Next.Hdr = NULL;
    hdr->Header.Prev.Hdr = NULL;
    TOTEM_SETBITS(hdr->Flags, totemGCObjectFlag_IsUsed);
    
    if(isRoot)
    {
        TOTEM_SETBITS(hdr->Flags, totemGCObjectFlag_IsRoot);
    }
    
    if (numRegisters)
    {
        hdr->Data.Registers = totemExecState_Alloc(state, sizeof(totemRegister) * numRegisters);
        if (!hdr->Data.Registers)
        {
            TOTEM_FREE(hdr, sizeof(totemGCObject));
            return NULL;
        }
        
        totemRegister_InitList(hdr->Data.Registers, numRegisters);
    }
    else
    {
        hdr->Data.Registers = NULL;
    }
    
    state->GCNumBytes += sizeof(totemGCObject) + (sizeof(totemRegister) * numRegisters);
    
    totemExecState_AppendNewGCObject(state, hdr);
    
    TOTEM_GC_DEBUG_LOG_V("add %i %p %s %p %p", state->GCNum, hdr, totemGCObjectType_Describe(type), hdr->Header.Next.Hdr, hdr->Header.Next.Hdr);
    
    return hdr;
}

totemBool totemExecState_ExpandGCObject(totemExecState *state, totemGCObject *obj)
{
    uint32_t newNumRegisters = obj->NumRegisters;
    if (!newNumRegisters)
    {
        newNumRegisters = 8;
    }
    else
    {
        newNumRegisters *= 2;
    }
    
    totemRegister *newRegs = totemExecState_Alloc(state, sizeof(totemRegister) * newNumRegisters);
    if (!newRegs)
    {
        return totemBool_False;
    }
    
    memcpy(newRegs, obj->Data.Registers, sizeof(totemRegister) * obj->NumRegisters);
    totemRegister_InitList(newRegs + obj->NumRegisters, newNumRegisters - obj->NumRegisters);
    
    TOTEM_FREE(obj->Data.Registers, sizeof(totemRegister) * obj->NumRegisters);
    
    state->GCNumBytes -= (sizeof(totemRegister) * obj->NumRegisters);
    state->GCNumBytes += (sizeof(totemRegister) * newNumRegisters);
    
    obj->Data.Registers = newRegs;
    obj->NumRegisters = newNumRegisters;
    
    return totemBool_True;
}

totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj)
{
    //TOTEM_GC_DEBUG_LOG_V("destroying %i %p %s %p %p", state->GCNum, obj, totemGCObjectType_Describe(obj->Type), obj->Header.Next.Hdr, obj->Header.Prev.Hdr));
    
    totemGCObjectType type = obj->Type;
    obj->Type = totemGCObjectType_Deleting;
    
    switch (type)
    {
        case totemGCObjectType_Coroutine:
			totemExecState_DestroyCoroutine(state, obj->Ptr.Coroutine);
            break;
            
        case totemGCObjectType_Object:
			totemExecState_DestroyObject(state, obj->Ptr.Object);
            break;
            
        case totemGCObjectType_Userdata:
			obj->Ptr.UserdataDestructor(state, obj->Data.Userdata);
            break;
            
        case totemGCObjectType_Instance:
			totemExecState_DestroyInstance(state, obj->Ptr.Instance);
            break;
            
        case totemGCObjectType_Deleting:
            return NULL;
            
        default:
            break;
    }
    
    //TOTEM_GC_DEBUG_LOG_V("unlinking %i %p %s %p %p", state->GCNum, obj, totemGCObjectType_Describe(obj->Type), obj->Header.Next.Hdr, obj->Header.Prev.Hdr));
    
    state->GCNumBytes -= sizeof(totemGCObject) + (sizeof(totemRegister) * obj->NumRegisters);
    
    totemGCObject *next = obj->Header.Next.Obj;
    totemGCHeader_Unlink(&obj->Header);
    
    totemGCHeader_Assert(&state->GCRoots, &obj->Header);
    totemGCHeader_Assert(&state->GCWhite, &obj->Header);
    totemGCHeader_Assert(&state->GCGrey, &obj->Header);
    totemGCHeader_Assert(&state->GCBlack, &obj->Header);
    totemGCHeader_Assert(&state->GCSweep, &obj->Header);
    
    TOTEM_GC_DEBUG_LOG_V("killing %i %p %s %p %p", state->GCNum, obj, totemGCObjectType_Describe(obj->Type), obj->Header.Next.Hdr, obj->Header.Prev.Hdr);
    
    TOTEM_UNSETBITS(obj->Flags, totemGCObjectFlag_IsUsed);
    obj->Header.Next.Obj = state->GCFreeList;
    state->GCFreeList = obj;
    state->GCNum--;
    return next;
}

totemExecStatus totemExecState_CloneArray(totemExecState *state, totemGCObject *src, totemGCObject **dst)
{
	return totemExecState_CreateArrayFromExisting(state, src->Data.Registers, src->NumRegisters, dst);
}

totemExecStatus totemExecState_CloneObject(totemExecState *state, totemGCObject *src, totemGCObject **dst)
{
	TOTEM_EXEC_CHECKRETURN(totemExecState_CreateObject(state, src->Ptr.Object->NumKeys, dst));
    
    for (size_t i = 0; i < src->Ptr.Object->NumBuckets; i++)
    {
        for (totemRuntimeStringMapEntry* entry = (totemRuntimeStringMapEntry*)src->Ptr.Object->Buckets[i]; entry; entry = (totemRuntimeStringMapEntry*)entry->Entry.Next)
        {
            if (!totemRuntimeStringMap_Insert((*dst)->Ptr.Object, &entry->Data))
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
        }
    }

	for (uint32_t i = 0; i < src->NumRegisters; i++)
	{
		totemRegister_Set(&(*dst)->Data.Registers[i], &src->Data.Registers[i]);
	}

	return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CloneInstance(totemExecState *state, totemGCObject *src, totemBool isRoot, totemGCObject **dst)
{
	TOTEM_EXEC_CHECKRETURN(totemExecState_CreateInstance(state, src->Ptr.Instance->Script, isRoot, dst));

	for (uint32_t i = 0; i < src->NumRegisters; i++)
	{
		totemRegister_Set(&(*dst)->Data.Registers[i], &src->Data.Registers[i]);
	}

	return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CloneUserdata(totemExecState *state, totemGCObject *src, totemGCObject **dst)
{
	return totemExecState_CreateUserdata(state, src->Data.Userdata, src->Ptr.UserdataDestructor, dst);
}

totemExecStatus totemExecState_CloneCoroutine(totemExecState *state, totemGCObject *src, totemGCObject **dst)
{
	TOTEM_EXEC_CHECKRETURN(totemExecState_CreateCoroutine(state, src->Ptr.Coroutine->Function.Instance, dst));

	for (uint32_t i = 0; i < src->NumRegisters; i++)
	{
		totemRegister_Set(&(*dst)->Data.Registers[i], &src->Data.Registers[i]);
	}

	return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CloneGCObject(totemExecState *state, totemGCObject *src, totemGCObject **dst)
{
	switch (src->Type)
	{
		case totemGCObjectType_Array:
			return totemExecState_CloneArray(state, src, dst);
		
		case totemGCObjectType_Object:
			return totemExecState_CloneObject(state, src, dst);

		case totemGCObjectType_Instance:
			return totemExecState_CloneInstance(state, src, TOTEM_HASBITS(src->Flags, totemGCObjectFlag_IsRoot), dst);

		case totemGCObjectType_Userdata:
			return totemExecState_CloneUserdata(state, src, dst);

		case totemGCObjectType_Coroutine:
			return totemExecState_CloneCoroutine(state, src, dst);

		default:
			return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
	}
}

totemExecStatus totemExecState_CreateArray(totemExecState *state, uint32_t numRegisters, totemGCObject **gcOut)
{
    totemGCObject *obj = totemExecState_CreateGCObject(state, totemGCObjectType_Array, numRegisters, totemBool_False);
    if (!obj)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    *gcOut = obj;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, uint32_t numRegisters, totemGCObject **gcOut)
{
    totemExecStatus status = totemExecState_CreateArray(state, numRegisters, gcOut);
    if (status != totemExecStatus_Continue)
    {
        return status;
    }
    
    for (uint32_t i = 0; i < numRegisters; i++)
    {
        totemRegister_Set(&(*gcOut)->Data.Registers[i], &registers[i]);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateObject(totemExecState *state, uint32_t size, totemGCObject **gcOut)
{
    totemRuntimeStringMap *obj = totemExecState_Alloc(state, sizeof(totemRuntimeStringMap));
    if (!obj)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemRuntimeStringMap_Init(obj);
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Object, size, totemBool_False);
    if (!gc)
    {
        TOTEM_FREE(obj, sizeof(*obj));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    state->GCNumBytes += sizeof(totemRuntimeStringMap);
	gc->Ptr.Object = obj;
    *gcOut = gc;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemInstanceFunction *function, totemGCObject **gcOut)
{
    totemFunctionCall *coroutine = totemExecState_SecureFunctionCall(state);
    if (!coroutine)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Coroutine, function->Function->RegistersNeeded, totemBool_True);
    if (!gc)
    {
        totemExecState_FreeFunctionCall(state, coroutine);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
	TOTEM_ZERO(*coroutine);
    coroutine->Function.Void = function;
    coroutine->Instance = state->CallStack->Instance;
	coroutine->Flags = totemFunctionCallFlag_IsCoroutine;// | totemFunctionCallFlag_FreeStack;
    coroutine->NumRegisters = function->Function->RegistersNeeded;
	coroutine->FrameStart = gc->Data.Registers;
    coroutine->Invoke = totemExecState_ExecuteInstructions;
    
	gc->Ptr.Coroutine = coroutine;
    state->GCNumBytes += sizeof(totemFunctionCall);
    
    *gcOut = gc;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateUserdata(totemExecState *state, void *data, totemUserdataDestructor destructor, totemGCObject **gcOut)
{
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Userdata, 0, totemBool_False);
    if (!gc)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    gc->Data.Userdata = data;
	gc->Ptr.UserdataDestructor = destructor;
    
    *gcOut = gc;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateInstance(totemExecState *state, totemScript *script, totemBool isRoot, totemGCObject **gcOut)
{
    totemInstance *instance = totemExecState_Alloc(state, sizeof(totemInstance));
    if (!instance)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemInstance_Init(instance);
    
    instance->Script = script;
    
    // setup instance functions
    size_t numFunctions = script->NumFunctions;
    instance->LocalFunctions = totemExecState_Alloc(state, sizeof(totemInstanceFunction) * numFunctions);
    if(!instance->LocalFunctions)
    {
        TOTEM_FREE(instance, sizeof(totemInstance));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    instance->NumLocalFunctions = numFunctions;
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Instance, (uint32_t)script->NumGlobalRegisters, isRoot);
    if (!gc)
    {
        TOTEM_FREE(instance, sizeof(totemInstance));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    // copy global registers over
    for(size_t i = 0; i < script->NumGlobalRegisters; i++)
    {
        totemRegister_Set(&gc->Data.Registers[i], &script->GlobalRegisters[i]);
    }
    
    for (size_t i = 0; i < numFunctions; i++)
    {
        totemInstanceFunction *instanceFunc = &instance->LocalFunctions[i];
		totemScriptFunction *scriptFunc = &script->Functions[i];
        
        instanceFunc->Function = scriptFunc;
        instanceFunc->Instance = gc;
    }
    
    // fix instance function pointers
    for(size_t i = 0; i < totemScriptFunctionAddrDynBuffer_Length(&script->ScriptFunctionAddrs); i++)
    {
        totemScriptFunctionAddr *addr = totemScriptFunctionAddrDynBuffer_At(&script->ScriptFunctionAddrs, i);
        totemRegister *reg = &gc->Data.Registers[addr->RegisterIndex];
        totemInstanceFunction *func = &instance->LocalFunctions[addr->FunctionAddr];
        totemRegister_SetInstanceFunction(reg, func);
    }
    
    state->GCNumBytes += sizeof(totemInstance) + (instance->NumLocalFunctions * sizeof(totemInstanceFunction));
    gc->Ptr.Instance = instance;
    *gcOut = gc;
    
    // perform global function
    totemInstanceFunction *func = &instance->LocalFunctions[0];
    return totemExecState_StartInstance(state, func);
}

totemExecStatus totemExecState_ArraySet(totemExecState* state, totemGCObject* obj, uint32_t index, totemRegister* src)
{
    uint32_t len = obj->NumRegisters;

    if (index >= len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRegister* arr = obj->Data.Registers;

    totemRegister_Set(&arr[index], src);
    totemExecState_WriteBarrier(state, obj);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArrayGet(totemExecState* state, totemGCObject* obj, uint32_t index, totemRegister* dst)
{
    uint32_t len = obj->NumRegisters;
    totemRegister* arr = obj->Data.Registers;

    if (index >= len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }

    totemRegister_Set(dst, &arr[index]);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ObjectGet(totemExecState* state, totemGCObject* obj, totemRuntimeStringValue* key, totemRegister* dst)
{
    totemRuntimeStringMapKey actualKey;
    actualKey.Key = *key;

    if (!totemRuntimeStringMap_GetPrecomputed(obj->Ptr.Object, &actualKey, key->Hash))
    {
        totemRegister_SetNull(dst);
    }
    else
    {
        totemRegister_Set(dst, obj->Data.Registers + actualKey.Value);
    }

    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ObjectSet(totemExecState* state, totemGCObject* obj, totemRuntimeStringValue* key, totemRegister* src)
{
    totemRuntimeStringMapKey actualKey;
    actualKey.Key = *key;

    if (totemRegister_IsNull(src))
    {
        // just remove the entry
        totemRuntimeStringMap_Remove(obj->Ptr.Object, &actualKey);
        return totemExecStatus_Continue;
    }

    if (!totemRuntimeStringMap_GetPrecomputed(obj->Ptr.Object, &actualKey, key->Hash))
    {
        totemBool expanded = totemBool_False;

        // we need to insert a new lookup value
        if (obj->Ptr.Object->FreeList)
        {
            // we can reuse the register index sitting on top of the freelist
            totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)obj->Ptr.Object->FreeList;
            actualKey.Value = e->Data.Value;
        }
        else
        {
            expanded = totemBool_True;

            // otherwise we need a new one
            actualKey.Value = obj->NumRegisters;

            if (!totemExecState_ExpandGCObject(state, obj))
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
        }

        if (!totemRuntimeStringMap_InsertNoSearchPrecomputed(obj->Ptr.Object, &actualKey, key->Hash))
        {
            return totemExecStatus_Break(totemExecStatus_OutOfMemory);
        }

        if (expanded)
        {
            state->GCNumBytes += sizeof(totemRegister) + sizeof(totemHashMapEntry);
        }
    }

    totemRegister* newReg = obj->Data.Registers + actualKey.Value;
    totemRegister_Set(newReg, src);
    totemExecState_WriteBarrier(state, obj);
    return totemExecStatus_Continue;
}

void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co)
{
    state->GCNumBytes -= sizeof(totemFunctionCall);
    totemExecState_FreeFunctionCall(state, co);
}

void totemExecState_DestroyObject(totemExecState *state, totemRuntimeStringMap *obj)
{
    state->GCNumBytes -= sizeof(totemRuntimeStringMap) + (sizeof(totemRegister) * obj->NumKeys) + (sizeof(totemHashMapEntry) * obj->NumKeys);
    totemRuntimeStringMap_Cleanup(obj);
    TOTEM_FREE(obj, sizeof(totemRuntimeStringMap));
}

void totemExecState_DestroyInstance(totemExecState *state, totemInstance *obj)
{
    state->GCNumBytes -= sizeof(totemInstance) + (obj->NumLocalFunctions * sizeof(totemInstanceFunction));
    totemInstance_Cleanup(obj);
    TOTEM_FREE(obj, sizeof(totemInstance));
}

const char *totemGCObjectType_Describe(totemGCObjectType type)
{
    switch (type)
    {
        TOTEM_STRINGIFY_CASE(totemGCObjectType_Array);
        TOTEM_STRINGIFY_CASE(totemGCObjectType_Coroutine);
        TOTEM_STRINGIFY_CASE(totemGCObjectType_Object);
        TOTEM_STRINGIFY_CASE(totemGCObjectType_Userdata);
        TOTEM_STRINGIFY_CASE(totemGCObjectType_Instance);
        TOTEM_STRINGIFY_CASE(totemGCObjectType_Deleting);
        default:return "UNKNOWN";
    }
}
