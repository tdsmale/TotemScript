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
#if TOTEM_EVALOPT_GLOBAL_CACHE
    for (size_t i = 0; i < scope->MoveToLocalVars.NumBuckets; i++)
    {
        for (totemHashMapEntry *entry = scope->MoveToLocalVars.Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemOperandXUnsigned globalCache = 0;
            
            if (totemRegisterListPrototype_GetRegisterGlobalCache(list, (totemOperandXUnsigned)entry->Value, &globalCache))
            {
                totemRegisterPrototypeFlag globalFlags;
                if (totemRegisterListPrototype_GetRegisterFlags(&build->GlobalRegisters, globalCache, &globalFlags)
                    && !TOTEM_HASANYBITS(globalFlags, totemRegisterPrototypeFlag_IsValue | totemRegisterPrototypeFlag_IsConst))
                {
                    totemRegisterPrototypeFlag localFlags;
                    if (totemRegisterListPrototype_GetRegisterFlags(list, (totemOperandXUnsigned)entry->Value, &localFlags))
                    {
                        if (!TOTEM_HASBITS(globalFlags, totemRegisterPrototypeFlag_IsAssigned) && !TOTEM_HASBITS(localFlags, totemRegisterPrototypeFlag_IsAssigned))
                        {
                            continue;
                        }
                    }
                    
                    totemOperandRegisterPrototype dummy;
                    dummy.RegisterIndex = (totemOperandXUnsigned)entry->Value;
                    dummy.RegisterScopeType = totemOperandType_LocalRegister;
                    
                    if (toLocal)
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &dummy, globalCache, totemOperationType_MoveToLocal));
                    }
                    else
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &dummy, globalCache, totemOperationType_MoveToGlobal));
                    }
                }
            }
        }
    }
#endif
    return totemEvalStatus_Success;
}

// write instructions to free global associations from ALL local scopes
totemEvalStatus totemBuildPrototype_RotateAllGlobalCaches(totemBuildPrototype *build, totemRegisterListPrototype *list, totemBool toLocal)
{
#if TOTEM_EVALOPT_GLOBAL_CACHE
    for (totemRegisterListPrototypeScope *scope = list->Scope; scope; scope = scope->Prev)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateGlobalCache(build, list, scope, toLocal));
    }
#endif
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_GlobalCacheCheck(totemBuildPrototype *build, totemOperandRegisterPrototype *opOut, totemOperandRegisterPrototype *hint)
{
#if TOTEM_EVALOPT_GLOBAL_CACHE
    // if we can't access this in a normal instruction, move to local-scope
    if (opOut->RegisterScopeType == totemOperandType_GlobalRegister && TOTEM_HASBITS(build->Flags, totemBuildPrototypeFlag_EvalGlobalCaches))
    {
#if TOTEM_VMOPT_GLOBAL_OPERANDS
        totemBool doIt = opOut->RegisterIndex >= TOTEM_MAX_LOCAL_REGISTERS;
#else
        totemBool doIt = totemBool_True;
#endif
        if (doIt)
        {
            totemRegisterListPrototype *localScopeList = totemBuildPrototype_GetLocalScope(build);
            totemRegisterListPrototypeScope *localScope = localScopeList->Scope;
            
            while (localScope)
            {
                // is this register already in local scope?
                totemHashMapEntry *entry = totemHashMap_Find(&localScope->MoveToLocalVars, (const char*)opOut, sizeof(totemOperandRegisterPrototype));
                if (entry != NULL)
                {
                    opOut->RegisterIndex = (totemOperandXUnsigned)entry->Value;
                    opOut->RegisterScopeType = totemOperandType_LocalRegister;
                    totemRegisterListPrototype_IncRegisterRefCount(localScopeList, opOut->RegisterIndex, NULL);
                    return totemEvalStatus_Success;
                }
                
                localScope = localScope->Prev;
            }
            
            // need to add a new one to current scope
            localScope = localScopeList->Scope;
            
            totemOperandRegisterPrototype original;
            memcpy(&original, opOut, sizeof(totemOperandRegisterPrototype));
            
            if (hint)
            {
                memcpy(opOut, hint, sizeof(totemOperandRegisterPrototype));
            }
            else
            {
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(localScopeList, opOut));
            }
            
            totemRegisterPrototypeFlag srcFlags;
            totemRegisterListPrototype_GetRegisterFlags(&build->GlobalRegisters, original.RegisterIndex, &srcFlags);
            if (TOTEM_HASANYBITS(srcFlags, totemRegisterPrototypeFlag_IsAssigned | totemRegisterPrototypeFlag_IsValue))
            {
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, opOut, original.RegisterIndex, totemOperationType_MoveToLocal));
            }
            
            totemRegisterPrototypeFlag dstFlags;
            totemRegisterListPrototype_GetRegisterFlags(localScopeList, opOut->RegisterIndex, &dstFlags);
            if (!TOTEM_HASBITS(dstFlags, totemRegisterPrototypeFlag_IsVariable))
            {
                totemRegisterListPrototype_SetRegisterGlobalCache(localScopeList, opOut->RegisterIndex, original.RegisterIndex);
                
                // add to lookup
                if (!totemHashMap_Insert(&localScope->MoveToLocalVars, (const char*)&original, sizeof(totemOperandRegisterPrototype), opOut->RegisterIndex))
                {
                    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
                }
                
                totemRegisterListPrototype_UnsetRegisterFlags(localScopeList, opOut->RegisterIndex, totemRegisterPrototypeFlag_IsTemporary);
                
                if (hint)
                {
                    totemRegisterListPrototype_SetRegisterFlags(localScopeList, opOut->RegisterIndex, totemRegisterPrototypeFlag_IsGlobalCache);
                }
                else
                {
                    totemPublicDataType type;
                    totemRegisterListPrototype_GetRegisterType(&build->GlobalRegisters, original.RegisterIndex, &type);
                    totemRegisterListPrototype_SetRegisterFlags(localScopeList, opOut->RegisterIndex, srcFlags | totemRegisterPrototypeFlag_IsGlobalCache);
                    totemRegisterListPrototype_SetRegisterType(localScopeList, opOut->RegisterIndex, type);
                }
            }
        }
    }
#endif
    
    return totemEvalStatus_Success;
}