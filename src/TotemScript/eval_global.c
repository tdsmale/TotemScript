//
//  eval_global.c
//  TotemScript
//
//  Created by Timothy Smale on 10/08/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <string.h>

totemEvalStatus totemBuildPrototype_RotateGlobalCache(totemBuildPrototype *build, totemRegisterListPrototype *list, totemRegisterListPrototypeScope *scope, totemBool toLocal)
{
    for (uint32_t i = 0; i < scope->MoveToLocalVars.NumBuckets; i++)
    {
        for (totemHashMapEntry *entry = scope->MoveToLocalVars.Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemEvalOperandHashMapEntry* e = (totemEvalOperandHashMapEntry*)entry;
            
            totemOperandXu global = e->Data.Key;
            totemOperandXu local = e->Data.Value;
            
            // only need to rotate what can be mutated
            totemRegisterPrototypeFlag globalFlags;
            if (totemRegisterListPrototype_GetRegisterFlags(&build->GlobalRegisters, global, &globalFlags)
                && !TOTEM_HASANYBITS(globalFlags, totemRegisterPrototypeFlag_IsValue | totemRegisterPrototypeFlag_IsConst))
            {
                totemRegisterPrototypeFlag localFlags;
                if (totemRegisterListPrototype_GetRegisterFlags(list, local, &localFlags))
                {
                    if (!TOTEM_HASBITS(globalFlags, totemRegisterPrototypeFlag_IsAssigned)
                        && !TOTEM_HASBITS(localFlags, totemRegisterPrototypeFlag_IsAssigned))
                    {
                        continue;
                    }
                }
                    
                totemOperandPrototype dummy;
                dummy.RegisterIndex = local;
                
                if (toLocal)
                {
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_MoveToLocal(build, &dummy, global));
                }
                else
                {
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_MoveToGlobal(build, &dummy, global));
                }
            }
        }
    }

    return totemEvalStatus_Success;
}

// write instructions to free global associations from ALL local scopes
totemEvalStatus totemBuildPrototype_RotateAllGlobalCaches(totemBuildPrototype *build, totemRegisterListPrototype *list, totemBool toLocal)
{
    for (totemRegisterListPrototypeScope *scope = list->Scope; scope; scope = scope->Prev)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateGlobalCache(build, list, scope, toLocal));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_GlobalCacheCheck(totemBuildPrototype *build, totemOperandPrototype *opOut, totemOperandPrototype *hint)
{
    // if we can't access this in a normal instruction, move to local-scope
    if (TOTEM_HASBITS(build->Flags, totemBuildPrototypeFlag_EvalGlobalCaches))
    {
        totemRegisterListPrototype *localRegList = totemBuildPrototype_GetLocalRegisters(build);
        if (localRegList->ScopeType == totemOperandType_GlobalRegister)
        {
            // already in global scope, so nothing to do
            TOTEM_ASSERT(opOut->RegisterIndex < TOTEM_MAX_LOCAL_REGISTERS);
            return totemEvalStatus_Success;
        }

        totemEvalOperandHashMapKey key;
        key.Key = opOut->RegisterIndex;

        totemRegisterListPrototypeScope *localScope = localRegList->Scope;
        while (localScope)
        {
            // is this register already in local scope?
            if (totemEvalOperandHashMap_Get(&localScope->MoveToLocalVars, &key))
            {
                opOut->RegisterIndex = key.Value;
                TOTEM_ASSERT(opOut->RegisterIndex < TOTEM_MAX_LOCAL_REGISTERS);
                return totemEvalStatus_Success;
            }
                
            localScope = localScope->Prev;
        }
            
        // need to add a new one to current scope
        localScope = localRegList->Scope;

        totemOperandPrototype original = *opOut;
            
        if (hint)
        {
			*opOut = *hint;
        }
        else
        {
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(localRegList, opOut));
        }
            
        //totemRegisterPrototypeFlag srcFlags;
        //totemRegisterListPrototype_GetRegisterFlags(&build->GlobalRegisters, original.RegisterIndex, &srcFlags);
        //if (TOTEM_HASANYBITS(srcFlags, totemRegisterPrototypeFlag_IsAssigned | totemRegisterPrototypeFlag_IsValue))
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_MoveToLocal(build, opOut, original.RegisterIndex));
        }
            
        totemRegisterPrototypeFlag dstFlags;
        totemRegisterListPrototype_GetRegisterFlags(localRegList, opOut->RegisterIndex, &dstFlags);
        if (!TOTEM_HASBITS(dstFlags, totemRegisterPrototypeFlag_IsVariable))
        {
            // add to lookup
            key.Value = opOut->RegisterIndex;
            if(!totemEvalOperandHashMap_InsertNoSearch(&localScope->MoveToLocalVars, &key))
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
            
            totemRegisterPrototype *globalReg = totemRegisterPrototypeDynBuffer_At(&build->GlobalRegisters.Registers, original.RegisterIndex);
            totemRegisterPrototype *localReg = totemRegisterPrototypeDynBuffer_At(&localRegList->Registers, opOut->RegisterIndex);
            
            *localReg = *globalReg;
            
            /*
            totemRegisterListPrototype_UnsetRegisterFlags(localRegList, opOut->RegisterIndex, totemRegisterPrototypeFlag_IsTemporary);
            
            totemPublicDataType type;
            totemRegisterPrototypeValue val;
            totemRegisterPrototypeFlag flag;
            totemRegisterListPrototype_GetRegisterType(&build->GlobalRegisters, original.RegisterIndex, &type);
            totemRegisterListPrototype_GetRegisterValue(&build->GlobalRegisters, original.RegisterIndex, &val);
            totemRegisterListPrototype_GetRegisterFlags(localRegList, original.RegisterIndex, &flag);
            totemRegisterListPrototype_SetRegisterType(localRegList, opOut->RegisterIndex, type);
            totemRegisterListPrototype_SetRegisterValue(localRegList, opOut->RegisterIndex, &val);
            totemRegisterListPrototype_SetRegisterFlags(localRegList, opOut->RegisterIndex, flag);*/
        }
    }
    
    TOTEM_ASSERT(opOut->RegisterIndex < TOTEM_MAX_LOCAL_REGISTERS);
    return totemEvalStatus_Success;
}
