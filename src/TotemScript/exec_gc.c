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
#define totemGCHeader_Assert totemGCHeader_AssertList
#define totemGCObject_Assert totemGCObject_AssertLiveliness
#else
#define TOTEM_GC_LOG(e)
#define totemGCHeader_Assert(x, y)
#define totemGCObject_Assert(x)
#endif

#define TOTEM_GC_MAYBE_UNREACHABLE (~((totemRefCount)0))

void totemGCHeader_AssertList(totemGCHeader *head, totemGCHeader *no)
{
    totem_assert(head->PrevHdr && head->NextHdr);
    
    size_t i = 0;
    for (totemGCHeader *hdr = head->NextHdr; hdr != head; hdr = hdr->NextHdr)
    {
        totem_assert(hdr->PrevHdr && hdr->NextHdr);
        totem_assert(no == NULL || hdr != no);
        totem_assert(i < 5000);
        totem_assert(hdr != NULL);
        i++;
    }
    
    if (!i)
    {
        totem_assert(head->PrevHdr == head && head->NextHdr == head);
    }
    
    size_t j = 0;
    for (totemGCHeader *hdr = head->PrevHdr; hdr != head; hdr = hdr->PrevHdr)
    {
        totem_assert(hdr->PrevHdr && hdr->NextHdr);
        totem_assert(no == NULL || hdr != no);
        totem_assert(j < 5000);
        totem_assert(hdr != NULL);
        j++;
    }
    
    if (!j)
    {
        totem_assert(head->PrevHdr == head && head->NextHdr == head);
    }
    
    totem_assert(i == j);
}

void totemGCObject_AssertLiveliness(totemGCObject *obj)
{
#if TOTEM_GCTYPE_ISMARKANDSWEEP
    totem_assert(TOTEM_HASBITS(obj->MarkFlags, totemGCObjectMarkSweepFlag_IsUsed));
#endif
    totemGCHeader_Assert(&obj->Header, NULL);
    totem_assert(obj->Header.PrevHdr && obj->Header.NextHdr);
    totemGCHeader_Assert(obj->Header.NextHdr, NULL);
    totemGCHeader_Assert(obj->Header.PrevHdr, NULL);
}

void totemGCHeader_Reset(totemGCHeader *obj)
{
    obj->NextHdr = obj;
    obj->PrevHdr = obj;
    
    totemGCHeader_Assert(obj, NULL);
}

totemBool totemGCHeader_IsEmpty(totemGCHeader *hdr)
{
    totem_assert(hdr->NextHdr && hdr->PrevHdr);
    totemGCHeader_Assert(hdr, NULL);
    return hdr->NextHdr == hdr;
}

void totemGCHeader_Unlink(totemGCHeader *obj)
{
    totem_assert(obj->NextHdr && obj->PrevHdr);
    totemGCHeader_Assert(obj, NULL);
    
    totemGCHeader *prev = obj->PrevHdr;
    prev->NextHdr = obj->NextHdr;
    obj->NextHdr->PrevHdr = prev;
    
    totemGCHeader_Assert(prev, obj);
    totemGCHeader_Assert(obj->NextHdr, obj);
    totemGCHeader_Assert(obj->PrevHdr, obj);
    totemGCHeader_Assert(obj->NextHdr, obj);
}

void totemGCHeader_Push(totemGCHeader *obj, totemGCHeader *to)
{
    totem_assert(obj != to);
    totemGCHeader_Assert(to, obj);
    
    obj->PrevHdr = to->PrevHdr;
    to->PrevHdr = obj;
    
    obj->PrevHdr->NextHdr = obj;
    obj->NextHdr = to;
    
    totemGCHeader_Assert(obj, NULL);
    totemGCHeader_Assert(to, NULL);
    totem_assert(obj->PrevHdr && obj->NextHdr);
    totem_assert(to->PrevHdr && to->NextHdr);
}

void totemGCHeader_Move(totemGCHeader *obj, totemGCHeader *to)
{
    totem_assert(obj != to);
    totemGCHeader_Unlink(obj);
    totemGCHeader_Push(obj, to);
    totem_assert(obj->PrevHdr && obj->NextHdr);
    totem_assert(to->PrevHdr && to->NextHdr);
}

void totemGCHeader_Migrate(totemGCHeader *from, totemGCHeader *to)
{
    totem_assert(from != to);
    totemGCHeader_Assert(from, to);
    totemGCHeader_Assert(to, from);
    
    if (!totemGCHeader_IsEmpty(from))
    {
        to->PrevHdr->NextHdr = from->NextHdr;
        from->NextHdr->PrevHdr = to->PrevHdr;
        to->PrevHdr = from->PrevHdr;
        to->PrevHdr->NextHdr = to;
        totemGCHeader_Reset(from);
    }
    else
    {
        totem_assert(from->PrevHdr == from);
    }
    
    totem_assert(from->PrevHdr && from->NextHdr);
    totem_assert(to->PrevHdr && to->NextHdr);
    totemGCHeader_Assert(from, to);
    totemGCHeader_Assert(to, from);
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
    
    totemGCHeader_Reset(hdr);
}

#if TOTEM_GCTYPE_ISREFCOUNTING

void totemExecState_InitGC(totemExecState *state)
{
    totemGCHeader_Reset(&state->GC);
    totemGCHeader_Reset(&state->GC2);
}

void totemExecState_CleanupGC(totemExecState *state)
{
    totemExecState_CleanupGCList(state, &state->GC);
    totemExecState_CleanupGCList(state, &state->GC2);
}

void totemExecState_AppendNewGCObject(totemExecState *state, totemGCObject *gc)
{
    hdr->RefCount = 1; // so it doesn't get collected prematurely - consequently, the first assignment to a register should not use the generic assign function but a type-specific one
    hdr->CycleDetectCount = 0;
    totemGCHeader_Push(&hdr->Header, &state->GC);
    state->GCNum++;
}

void totemExecState_WriteBarrier(totemExecState *state, totemGCObject *gc)
{
    // nada
}

void totemExecState_IncRefCount(totemExecState *state, totemRegister *gc)
{
    if (totemRegister_IsGarbageCollected(gc))
    {
        gc->Value.GCObject->RefCount++;
        TOTEM_GC_LOG(printf("inc count %s %i %p\n", totemGCObjectType_Describe(gc->Value.GCObject->Type), gc->Value.GCObject->RefCount, gc->Value.GCObject));
    }
}

void totemExecState_DecRefCount(totemExecState *state, totemRegister *gc)
{
    if (totemRegister_IsGarbageCollected(gc))
    {
        gc->Value.GCObject->RefCount--;
        if (gc->Value.GCObject->RefCount <= 0)
        {
            totemExecState_DestroyGCObject(state, gc->Value.GCObject);
        }
        
        TOTEM_GC_LOG(printf("dec count %s %i %p\n", totemGCObjectType_Describe(gc->Value.GCObject->Type), gc->Value.GCObject->RefCount, gc->Value.GCObject));
    }
}

void totemExecState_CycleDoubleCheck(totemExecState *state, totemGCObject *gc, totemGCHeader *reachable)
{
    totemRegister *regs = gc->Registers;
    if (regs)
    {
        size_t numRegs = gc->NumRegisters;
        
        for (size_t i = 0; i < numRegs; i++)
        {
            totemRegister *reg = &regs[i];
            
            if (totemRegister_IsGarbageCollected(reg))
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
    totemRegister *regs = gc->Registers;
    if (regs)
    {
        size_t numRegs = gc->NumRegisters;
        
        for (size_t i = 0; i < numRegs; i++)
        {
            totemRegister *reg = &regs[i];
            
            if (totemRegister_IsGarbageCollected(reg))
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

void totemExecState_CollectReferenceCycles(totemExecState *state, totemGCHeader *listHead)
{
    totemGCHeader unreachable;
    totemGCHeader_Reset(&unreachable);
    
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
            // object is reachable from outside this set, ensure all its members are considered reachable as well
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
    totemGCHeader *toCollect;
    
    if (full)
    {
        totemGCHeader_Migrate(&state->GC, &state->GC2);
        toCollect = &state->GC2;
    }
    else if (state->GCNumBytes < state->GCByteThreshold)
    {
        return;
    }
    else
    {
        toCollect = &state->GC;
    }
    
    totemExecState_CollectReferenceCycles(state, toCollect);
    
    if (!full)
    {
        totemGCHeader_Migrate(&state->GC, &state->GC2);
    }
}

#elif TOTEM_GCTYPE_ISMARKANDSWEEP

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
    TOTEM_GC_LOG(printf("Moving object %p to root\n", gc));
    TOTEM_UNSETBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_IsGrey);
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
    TOTEM_GC_LOG(printf("Moving object %p to grey\n", gc));
    TOTEM_SETBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_IsGrey);
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
    TOTEM_GC_LOG(printf("Moving object %p to black\n", gc));
    TOTEM_UNSETBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_IsGrey);
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
    totem_assert(state->GCCurrentBit == 1 || state->GCCurrentBit == 0);
    TOTEM_FORCEBIT(gc->MarkFlags, state->GCCurrentBit, totemGCObjectMarkSweepFlag_Mark >> 1);
    TOTEM_GC_LOG(printf("Marking object %p %i %i\n", gc, state->GCCurrentBit, TOTEM_GETBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_Mark)));
}

void totemExecState_UnsetMark(totemExecState *state, totemGCObject *gc)
{
    totem_assert(state->GCCurrentBit == 1 || state->GCCurrentBit == 0);
    TOTEM_FORCEBIT(gc->MarkFlags, !state->GCCurrentBit, totemGCObjectMarkSweepFlag_Mark >> 1);
    TOTEM_GC_LOG(printf("Unmarking object %p %i %i\n", gc, state->GCCurrentBit, TOTEM_GETBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_Mark)));
}

totemBool totemExecState_HasMark(totemExecState *state, totemGCObject *gc)
{
    totem_assert(state->GCCurrentBit == 1 || state->GCCurrentBit == 0);
    totemBool result = TOTEM_GETBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_Mark) == state->GCCurrentBit;
    TOTEM_GC_LOG(printf("Marked? %p %i %i %i\n", gc, state->GCCurrentBit, TOTEM_GETBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_Mark), result));
    
    return result;
}

void totemExecState_AppendNewGCObject(totemExecState *state, totemGCObject *gc)
{
    totemExecState_UnsetMark(state, gc);
    
    if (gc->Type == totemGCObjectType_Instance)
    {
        TOTEM_GC_LOG(printf("Moving new object %p to root\n", gc));
        totemGCHeader_Push(&gc->Header, &state->GCRoots);
        totemGCHeader_Assert(&state->GCRoots, NULL);
        totemGCHeader_Assert(&state->GCWhite, &gc->Header);
        totemGCHeader_Assert(&state->GCGrey, &gc->Header);
        totemGCHeader_Assert(&state->GCBlack, &gc->Header);
        totemGCHeader_Assert(&state->GCSweep, &gc->Header);
    }
    else
    {
        TOTEM_GC_LOG(printf("Moving new object %p to white\n", gc));
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
    if (!TOTEM_HASBITS(gc->MarkFlags, totemGCObjectMarkSweepFlag_IsGrey))
    {
        TOTEM_GC_LOG(printf("Write barrier %p\n", gc));
        totemGCObject_Assert(gc);
        totemExecState_SetMark(state, gc);
        totemGCObject_Assert(gc);
        totemExecState_MoveGrey(state, gc);
        totemGCObject_Assert(gc);
    }
}

void totemExecState_IncRefCount(totemExecState *state, totemRegister *gc)
{
    // nada
}

void totemExecState_DecRefCount(totemExecState *state, totemRegister *gc)
{
    // nada
}

void totemExecState_TraverseRegisterList(totemExecState *state, totemRegister *regs, size_t num)
{
    totemGCObject *obj;
    
    for (size_t i = 0; i < num; i++)
    {
        totemRegister *reg = &regs[i];
        if (totemRegister_IsGarbageCollected(reg))
        {
            obj = reg->Value.GCObject;
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
                else if (obj->Type == totemGCObjectType_Instance)
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
    totemExecState_TraverseRegisterList(state, obj->Registers, obj->NumRegisters);
    
    totemGCObject *next = obj->Header.NextObj;
    
    if (obj->Type == totemGCObjectType_Instance)
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
    
    for (totemGCObject *obj = state->GCRoots.NextObj; &obj->Header != &state->GCRoots; obj = obj->Header.NextObj)
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
            //which objects are roots?
            //- stack
            //- instances
        case totemMarkSweepState_Reset:
            TOTEM_GC_LOG(printf("MARK AND SWEEP STATE RESET\n"));
            
            // mark roots grey
            amount += totemExecState_MoveRootsGrey(state);
            state->GCState = totemMarkSweepState_Mark;
            break;
            
        case totemMarkSweepState_Mark:
            TOTEM_GC_LOG(printf("MARK AND SWEEP STATE MARK\n"));
            if (!totemGCHeader_IsEmpty(&state->GCGrey))
            {
                totemGCObject *obj = state->GCGrey.NextObj;
                amount += obj->NumRegisters;
                totemGCObject_Assert(obj);
                totemExecState_TraverseGCObject(state, obj);
                totemGCObject_Assert(obj);
            }
            
            if (totemGCHeader_IsEmpty(&state->GCGrey))
            {
                TOTEM_GC_LOG(printf("MARK AND SWEEP STATE MARK FINISH\n"));
                
                // check stack right before we start sweeping so we don't need to use a write-barrier for it
                for (totemFunctionCall *call = state->CallStack; call; call = call->Prev)
                {
                    amount += call->NumRegisters;
                    totemExecState_TraverseRegisterList(state, call->FrameStart, call->NumRegisters);
                }
                
                // double-check roots if we have global operands enabled, for the same reason
#if TOTEM_VMOPT_GLOBAL_OPERANDS
                amount += totemExecState_MoveRootsGrey(state);
#endif
                // extinguish grey list
                for (totemGCObject *obj = state->GCGrey.NextObj; &obj->Header != &state->GCGrey;)
                {
                    totemGCObject_Assert(obj);
                    amount += obj->NumRegisters;
                    obj = totemExecState_TraverseGCObject(state, obj);
                }
                
                // anything in black or roots is now assumed to be accessible, so we must keep them
                // anything left in white is now assumed to be inaccessible and needs freeing
                totemGCHeader_Migrate(&state->GCWhite, &state->GCSweep);
                totemGCHeader_Migrate(&state->GCBlack, &state->GCWhite);
                //TOTEM_GC_LOG(printf("Flipping bit: %i\n", state->GCCurrentBit));
                state->GCCurrentBit = !state->GCCurrentBit;
                TOTEM_GC_LOG(printf("Flipping bit: %i\n", state->GCCurrentBit));
                state->GCState = totemMarkSweepState_Sweep;
            }
            break;
            
        case totemMarkSweepState_Sweep:
            TOTEM_GC_LOG(printf("MARK AND SWEEP STATE SWEEP\n"));
            if (!totemGCHeader_IsEmpty(&state->GCSweep))
            {
                totemGCObject *obj = state->GCSweep.NextObj;
                totemGCObject_Assert(obj);
                totemExecState_DestroyGCObject(state, obj);
                amount++;
            }
            
            if (totemGCHeader_IsEmpty(&state->GCSweep))
            {
                TOTEM_GC_LOG(printf("MARK AND SWEEP STATE SWEEP FINISH\n"));
                
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

#endif

totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type, size_t numRegisters)
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
    
    hdr->Type = type;
    hdr->Coroutine = NULL;
    hdr->Userdata = NULL;
    hdr->NumRegisters = numRegisters;
    hdr->MarkFlags = totemGCObjectMarkSweepFlag_None;
    hdr->Header.NextHdr = NULL;
    hdr->Header.PrevHdr = NULL;
    TOTEM_SETBITS(hdr->MarkFlags, totemGCObjectMarkSweepFlag_IsUsed);
    
    if (numRegisters)
    {
        hdr->Registers = totemExecState_Alloc(state, sizeof(totemRegister) * numRegisters);
        if (!hdr->Registers)
        {
            totem_CacheFree(hdr, sizeof(totemGCObject));
            return NULL;
        }
        
        memset(hdr->Registers, 0, sizeof(totemRegister) * numRegisters);
    }
    else
    {
        hdr->Registers = NULL;
    }
    
    state->GCNumBytes += sizeof(totemGCObject) + (sizeof(totemRegister) * numRegisters);
    
    totemExecState_AppendNewGCObject(state, hdr);
    
    TOTEM_GC_LOG(printf("add %i %p %s %p %p\n", state->GCNum, hdr, totemGCObjectType_Describe(type), hdr->Header.NextHdr, hdr->Header.NextHdr));
    
    return hdr;
}

totemBool totemExecState_ExpandGCObject(totemExecState *state, totemGCObject *obj)
{
    size_t newNumRegisters = obj->NumRegisters;
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
    
    memcpy(newRegs, obj->Registers, sizeof(totemRegister) * obj->NumRegisters);
    memset(newRegs + obj->NumRegisters, 0, sizeof(totemRegister) * obj->NumRegisters);
    
    totem_CacheFree(obj->Registers, sizeof(totemRegister) * obj->NumRegisters);
    
    state->GCNumBytes -= (sizeof(totemRegister) * obj->NumRegisters);
    state->GCNumBytes += (sizeof(totemRegister) * newNumRegisters);
    
    obj->Registers = newRegs;
    obj->NumRegisters = newNumRegisters;
    
    return totemBool_True;
}

totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj)
{
    //TOTEM_GC_LOG(printf("destroying %i %p %s %p %p\n", state->GCNum, obj, totemGCObjectType_Describe(obj->Type), obj->Header.NextHdr, obj->Header.PrevHdr));
    
    totemGCObjectType type = obj->Type;
    obj->Type = totemGCObjectType_Deleting;
    
    switch (type)
    {
        case totemGCObjectType_Coroutine:
            totemExecState_DestroyCoroutine(state, obj->Coroutine);
            break;
            
        case totemGCObjectType_Object:
            totemExecState_DestroyObject(state, obj->Object);
            break;
            
        case totemGCObjectType_Userdata:
            obj->UserdataDestructor(state, obj->Userdata);
            break;
            
        case totemGCObjectType_Instance:
            totemExecState_DestroyInstance(state, obj->Instance);
            break;
            
        case totemGCObjectType_Deleting:
            return NULL;
            
        default:
            break;
    }
    
    if (obj->Registers)
    {
        totemExecState_CleanupRegisterList(state, obj->Registers, obj->NumRegisters);
    }
    
    //TOTEM_GC_LOG(printf("unlinking %i %p %s %p %p\n", state->GCNum, obj, totemGCObjectType_Describe(obj->Type), obj->Header.NextHdr, obj->Header.PrevHdr));
    
    state->GCNumBytes -= sizeof(totemGCObject) + (sizeof(totemRegister) * obj->NumRegisters);
    
    totemGCObject *next = obj->Header.NextObj;
    totemGCHeader_Unlink(&obj->Header);
    
    totemGCHeader_Assert(&state->GCRoots, &obj->Header);
    totemGCHeader_Assert(&state->GCWhite, &obj->Header);
    totemGCHeader_Assert(&state->GCGrey, &obj->Header);
    totemGCHeader_Assert(&state->GCBlack, &obj->Header);
    totemGCHeader_Assert(&state->GCSweep, &obj->Header);
    
    TOTEM_GC_LOG(printf("killing %i %p %s %p %p\n", state->GCNum, obj, totemGCObjectType_Describe(obj->Type), obj->Header.NextHdr, obj->Header.PrevHdr));
    
    TOTEM_UNSETBITS(obj->MarkFlags, totemGCObjectMarkSweepFlag_IsUsed);
    obj->Header.NextObj = state->GCFreeList;
    state->GCFreeList = obj;
    state->GCNum--;
    return next;
}

totemExecStatus totemExecState_CreateArray(totemExecState *state, totemInt numRegisters, totemGCObject **gcOut)
{
    if (numRegisters < 0 || numRegisters >= UINT32_MAX)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemGCObject *obj = totemExecState_CreateGCObject(state, totemGCObjectType_Array, (size_t)numRegisters);
    if (!obj)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    *gcOut = obj;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, size_t numRegisters, totemGCObject **gcOut)
{
    totemExecStatus status = totemExecState_CreateArray(state, numRegisters, gcOut);
    if (status != totemExecStatus_Continue)
    {
        return status;
    }
    
    for (uint32_t i = 0; i < numRegisters; i++)
    {
        totemExecState_Assign(state, &(*gcOut)->Registers[i], &registers[i]);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateObject(totemExecState *state, totemInt size, totemGCObject **gcOut)
{
    totemHashMap *obj = totemExecState_Alloc(state, sizeof(totemHashMap));
    if (!obj)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemHashMap_Init(obj);
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Object, (size_t)size);
    if (!gc)
    {
        totem_CacheFree(obj, sizeof(*obj));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    state->GCNumBytes += sizeof(totemHashMap);
    gc->Object = obj;
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
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Coroutine, function->Function->RegistersNeeded);
    if (!gc)
    {
        totemExecState_FreeFunctionCall(state, coroutine);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memset(coroutine, 0, sizeof(totemFunctionCall));
    coroutine->Function = function;
    coroutine->Type = totemFunctionType_Script;
    coroutine->Instance = state->CallStack->Instance;
    coroutine->Flags = totemFunctionCallFlag_IsCoroutine | totemFunctionCallFlag_FreeStack;
    coroutine->NumRegisters = function->Function->RegistersNeeded;
    coroutine->FrameStart = gc->Registers;
    
    gc->Coroutine = coroutine;
    state->GCNumBytes += sizeof(totemFunctionCall);
    
    *gcOut = gc;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateUserdata(totemExecState *state, void *data, totemUserdataDestructor destructor, totemGCObject **gcOut)
{
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Userdata, 0);
    if (!gc)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    gc->Userdata = data;
    gc->UserdataDestructor = destructor;
    
    *gcOut = gc;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateInstance(totemExecState *state, totemScript *script, totemGCObject **gcOut)
{
    totemInstance *instance = totemExecState_Alloc(state, sizeof(totemInstance));
    if (!instance)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemInstance_Init(instance);
    
    instance->Script = script;
    
    // setup instance functions
    size_t numFunctions = totemMemoryBuffer_GetNumObjects(&script->Functions);
    if (!totemMemoryBuffer_Secure(&instance->LocalFunctions, numFunctions))
    {
        totem_CacheFree(instance, sizeof(totemInstance));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Instance, totemMemoryBuffer_GetNumObjects(&script->GlobalRegisters));
    if (!gc)
    {
        totem_CacheFree(instance, sizeof(totemInstance));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    for (size_t i = 0; i < numFunctions; i++)
    {
        totemInstanceFunction *instanceFunc = totemMemoryBuffer_Get(&instance->LocalFunctions, i);
        totemScriptFunction *scriptFunc = totemMemoryBuffer_Get(&script->Functions, i);
        
        instanceFunc->Function = scriptFunc;
        instanceFunc->Instance = gc;
    }
    
    // fix instance function pointers
    for (size_t i = 0; i < gc->NumRegisters; i++)
    {
        totemRegister *reg = &gc->Registers[i];
        totemRegister *from = totemMemoryBuffer_Get(&script->GlobalRegisters, i);
        memcpy(reg, from, sizeof(totemRegister));
        
        if (reg->DataType == totemPrivateDataType_InstanceFunction)
        {
            reg->Value.InstanceFunction = totemMemoryBuffer_Get(&instance->LocalFunctions, (size_t)reg->Value.Data);
        }
    }
    
    state->GCNumBytes += sizeof(totemInstance) + instance->LocalFunctions.MaxLength;
    gc->Instance = instance;
    *gcOut = gc;
    
    return totemExecStatus_Continue;
}

void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co)
{
    state->GCNumBytes -= sizeof(totemFunctionCall);
    totemExecState_FreeFunctionCall(state, co);
}

void totemExecState_DestroyObject(totemExecState *state, totemHashMap *obj)
{
    state->GCNumBytes -= sizeof(totemHashMap) + (sizeof(totemRegister) * obj->NumKeys) + (sizeof(totemHashMapEntry) * obj->NumKeys);
    totemHashMap_Cleanup(obj);
    totem_CacheFree(obj, sizeof(totemHashMap));
}

void totemExecState_DestroyInstance(totemExecState *state, totemInstance *obj)
{
    state->GCNumBytes -= sizeof(totemInstance) + obj->LocalFunctions.MaxLength;
    totemInstance_Cleanup(obj);
    totem_CacheFree(obj, sizeof(totemInstance));
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
