//
//  exec.c
//  TotemScript
//
//  Created by Timothy Smale on 17/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <TotemScript/eval.h>
#include <TotemScript/memory.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

totemLinkStatus totemLinkStatus_Break(totemLinkStatus status)
{
    return status;
}

void totemInstance_Init(totemInstance *instance)
{
    instance->LocalFunctions = NULL;
    instance->NumLocalFunctions = 0;
    instance->Script = NULL;
}

void totemInstance_Reset(totemInstance *instance)
{
    totemInstance_Cleanup(instance);
}

void totemInstance_Cleanup(totemInstance *instance)
{
    TOTEM_FREE(instance->LocalFunctions, sizeof(totemInstanceFunction) * instance->NumLocalFunctions);
    totemInstance_Init(instance);
}

void totemScript_Init(totemScript *script)
{
    script->Functions = NULL;
    script->FunctionNames = NULL;
    script->GlobalRegisters = NULL;
    script->Instructions = NULL;
    script->NumFunctions = 0;
    script->NumGlobalRegisters = 0;
    script->NumInstructions = 0;
    totemRuntimeStringMap_Init(&script->FunctionNameLookup);
    totemScriptFunctionAddrDynBuffer_Init(&script->ScriptFunctionAddrs);
}

static void totemScript_ResetCleanup(totemScript *script)
{
#ifdef TOTEM_JIT
    for (size_t i = 0; i < script->NumFunctions; i++)
    {
        if (jitted())
        {
            freeJitFunc();
        }
    }
#endif

    TOTEM_FREE(script->Functions, sizeof(totemScriptFunction) * script->NumFunctions);
    TOTEM_FREE(script->FunctionNames, sizeof(totemRegister) * script->NumFunctions);
    TOTEM_FREE(script->GlobalRegisters, sizeof(totemRegister) * script->NumGlobalRegisters);
    TOTEM_FREE(script->Instructions, sizeof(totemInstruction) * script->NumInstructions);

    script->FunctionNames = NULL;
    script->Functions = NULL;
    script->GlobalRegisters = NULL;
    script->Instructions = NULL;
    script->NumFunctions = 0;
    script->NumInstructions = 0;
    script->NumGlobalRegisters = 0;
}

void totemScript_Reset(totemScript *script)
{
    totemRuntimeStringMap_Reset(&script->FunctionNameLookup);
    totemScriptFunctionAddrDynBuffer_Reset(&script->ScriptFunctionAddrs);
    totemScript_ResetCleanup(script);
}

void totemScript_Cleanup(totemScript *script)
{
    totemRuntimeStringMap_Cleanup(&script->FunctionNameLookup);
    totemScriptFunctionAddrDynBuffer_Cleanup(&script->ScriptFunctionAddrs);
    totemScript_ResetCleanup(script);
}

totemBool totemScript_GetFunctionName(totemScript *script, totemOperandXu addr, totemRuntimeStringValue *valOut)
{
    if(addr >= script->NumFunctions)
    {
        return totemBool_False;
    }
    
    totemRegister *name = &script->FunctionNames[addr];
    return totemRegister_GetString(name, valOut);
}

void totemRuntime_Init(totemRuntime *runtime)
{
	totemNativeFunctionDynBuffer_Init(&runtime->NativeFunctions);
    totemRuntimeStringValueDynBuffer_Init(&runtime->NativeFunctionNames);
    totemRuntimeStringMap_Init(&runtime->NativeFunctionsLookup);
    totemRuntimeStringSet_Init(&runtime->InternedStrings);
    totemScriptCache_Init(&runtime->ScriptCache);
    totemLock_Init(&runtime->InternedStringsLock);
    totemLock_Init(&runtime->ScriptCacheLock);
}

void totemRuntime_Reset(totemRuntime *runtime)
{
	totemNativeFunctionDynBuffer_Reset(&runtime->NativeFunctions);
	totemRuntimeStringValueDynBuffer_Reset(&runtime->NativeFunctionNames);
    totemRuntimeStringMap_Reset(&runtime->NativeFunctionsLookup);
    totemRuntimeStringSet_Reset(&runtime->InternedStrings);
    totemScriptCache_Reset(&runtime->ScriptCache);
}

void totemRuntime_Cleanup(totemRuntime *runtime)
{
	totemNativeFunctionDynBuffer_Cleanup(&runtime->NativeFunctions);
	totemRuntimeStringValueDynBuffer_Cleanup(&runtime->NativeFunctionNames);
    totemRuntimeStringMap_Cleanup(&runtime->NativeFunctionsLookup);
    totemRuntimeStringSet_Cleanup(&runtime->InternedStrings);
    totemScriptCache_Cleanup(&runtime->ScriptCache);
	totemLock_Cleanup(&runtime->InternedStringsLock);
    totemLock_Cleanup(&runtime->ScriptCacheLock);
}

totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *script)
{
	totemScript_Reset(script);

	// instructions
    script->Instructions = TOTEM_COPY(totemInstructionDynBuffer_Bottom(&build->Instructions), totemInstructionDynBuffer_Length(&build->Instructions) * sizeof(totemInstruction));
	if(!script->Instructions)
	{
		return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
	}
    script->NumInstructions = totemInstructionDynBuffer_Length(&build->Instructions);

	// functions
    size_t numFunctions = totemScriptFunctionPrototypeDynBuffer_Length(&build->Functions);
    script->Functions = TOTEM_MALLOC(sizeof(totemScriptFunction) * numFunctions);
	if (!script->Functions)
	{
		return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
	}
    script->NumFunctions = numFunctions;
    
    script->FunctionNames = TOTEM_MALLOC(sizeof(totemRegister) * numFunctions);
	if (!script->FunctionNames)
	{
		return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
	}
    
    totemScriptFunction *funcs = &script->Functions[0];
	totemScriptFunctionPrototype *funcProts = totemScriptFunctionPrototypeDynBuffer_At(&build->Functions, 0);
    
    for (size_t i = 0; i < numFunctions; i++)
    {
        totemRegister *str = &script->FunctionNames[i];
        totemScriptFunctionPrototype *funcProt = &funcProts[i];
        totemScriptFunction *func = &funcs[i];
        
        func->Address = (totemOperandXu)i;
        func->InstructionsStart = &script->Instructions[funcProt->InstructionsStart];
		func->NumInstructions = funcProt->NumInstructions;
        func->RegistersNeeded = funcProt->RegistersNeeded;
        
        if(funcProt->Name)
        {
            totemRuntimeStringValue newVal;
            if (totemRuntime_InternParseString(runtime, funcProt->Name, &newVal) != totemLinkStatus_Success)
            {
                return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
            }
            
            totemRegister_SetString(str, &newVal);
        }
        else
        {
            totemRegister_SetMiniString(str, "", 0);
        }
        
#ifdef TOTEM_JIT
		if (!jitterize(func))
#endif
		{
			func->Invoke = totemExecState_ExecuteInstructions;
		}
    }
    
    // check script function names against existing native functions
    numFunctions = totemScriptFunctionPrototypeDynBuffer_Length(&build->Functions);
	for (size_t i = 0; i < numFunctions; i++)
    {
        totemScriptFunctionPrototype *func = totemScriptFunctionPrototypeDynBuffer_At(&build->Functions, i);
        
        if (func->Name != NULL)
        {
            totemRuntimeStringMapParseKey key;
            key.Key = func->Name;
            if(totemRuntimeStringMap_GetParse(&runtime->NativeFunctionsLookup, &key) == totemBool_True)
            {
                return totemLinkStatus_Break(totemLinkStatus_FunctionAlreadyDeclared);
            }
        }
    }
    
    // copy function name lookup
    for (size_t i = 0; i < build->FunctionLookup.NumBuckets; i++)
    {
        for (totemEvalStrHashMapEntry* entry = (totemEvalStrHashMapEntry*)build->FunctionLookup.Buckets[i]; entry; entry = (totemEvalStrHashMapEntry*)entry->Entry.Next)
        {
            totemRuntimeStringMapKey key;
            key.Value = entry->Data.Value;
            if(totemRuntime_InternParseString(runtime, entry->Data.Key, &key.Key) != totemLinkStatus_Success)
            {
                return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
            }
            
            if (!totemRuntimeStringMap_Insert(&script->FunctionNameLookup, &key))
            {
                return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
            }
        }
    }
    
    // global register values
    size_t numGlobalRegs = totemRegisterPrototypeDynBuffer_Length(&build->GlobalRegisters.Registers);
    script->GlobalRegisters = TOTEM_MALLOC(sizeof(totemRegister) * numGlobalRegs);
    if(!script->GlobalRegisters)
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    script->NumGlobalRegisters = numGlobalRegs;
    
    for(size_t i = 0; i < numGlobalRegs; i++)
    {
        totemRegisterPrototype *prototype = totemRegisterPrototypeDynBuffer_At(&build->GlobalRegisters.Registers, i);
        totemRegister *reg = &script->GlobalRegisters[i];
        
        switch(prototype->DataType)
        {
                // intern string values
            case totemPublicDataType_String:
            {
                totemRuntimeStringValue newVal;
				if (totemRuntime_InternParseString(runtime, prototype->Value.String, &newVal) != totemLinkStatus_Success)
                {
                    return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
                }
                
                totemRegister_SetString(reg, &newVal);
                break;
            }
                
                // fix function pointers
            case totemPublicDataType_Function:
            {
				if (prototype->Value.FunctionPointer.Type == totemFunctionType_Native)
                {
					totemParseString *funcName = totemParseStringDynBuffer_At(&build->NativeFunctionNames, prototype->Value.FunctionPointer.Address);
                    if(!funcName)
                    {
                        return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionName);
                    }
                    
                    totemRuntimeStringMapParseKey key;
                    key.Key = funcName;
                    
                    TOTEM_LOG_BEGIN(Exec, String, Debug, dst)
                    {
                        totemLog_Write(dst, "Attempting to fix native function pointer: ");
                        totemParseString_Print(key.Key, dst);
                    }
                    TOTEM_LOG_END()
                    
                    if(totemRuntimeStringMap_GetParse(&runtime->NativeFunctionsLookup, &key) != totemBool_True)
                    {
                        return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                    }
                    
					totemNativeFunction *func = totemNativeFunctionDynBuffer_At(&runtime->NativeFunctions, key.Value);
                    if (!func)
                    {
                        return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                    }
                    
                    totemRegister_SetNativeFunction(reg, func);
                }
                else
                {
                    // fix these up later...
                    totemScriptFunctionAddr addr;
                    addr.RegisterIndex = i;
                    addr.FunctionAddr = prototype->Value.FunctionPointer.Address;
                    totemScriptFunctionAddrDynBuffer_Push(&script->ScriptFunctionAddrs, &addr, 1);
                    
                    // empty for now...
                    totemRegister_SetInstanceFunction(reg, NULL);
                }
                
                break;
            }
                
            case totemPublicDataType_Int:
				totemRegister_SetInt(reg, prototype->Value.Int);
                break;
                
            case totemPublicDataType_Float:
				totemRegister_SetFloat(reg, prototype->Value.Float);
                break;
                
            case totemPublicDataType_Type:
				totemRegister_SetTypeValue(reg, prototype->Value.Type);
                break;
                
            case totemPublicDataType_Boolean:
				totemRegister_SetBool(reg, prototype->Value.Bool);
                break;
                
            case totemPublicDataType_Null:
            case totemPublicDataType_Max:
                totemRegister_SetNull(reg);
                break;
                
            default:
                return totemLinkStatus_Break(totemLinkStatus_UnexpectedValueType);
        }
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunctions(totemRuntime *runtime, totemNativeFunctionPrototype *funcs, totemOperandXu num)
{
    size_t maxNative = totemNativeFunctionDynBuffer_Max(&runtime->NativeFunctions);
	if (maxNative < num)
    {
		if (!totemNativeFunctionDynBuffer_Reserve(&runtime->NativeFunctions, num))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    for (totemOperandXu i = 0; i < num; i++)
    {
        totemOperandXu addr;
        totemLinkStatus status = totemRuntime_LinkNativeFunction(runtime, funcs[i].Callback, &funcs[i].Name, &addr);
        if (status != totemLinkStatus_Success)
        {
            return status;
        }
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemRuntimeFunctionCb cb, totemString *name, totemOperandXu *addressOut)
{
    totemRuntimeStringMapLiteralKey key;
    key.Key = *name;

    totemNativeFunction *func = NULL;
    if(totemRuntimeStringMap_GetLiteral(&runtime->NativeFunctionsLookup, &key))
    {
        func = totemNativeFunctionDynBuffer_At(&runtime->NativeFunctions, key.Value);
        *addressOut = key.Value;
    }
    else
    {
        size_t numNative = totemNativeFunctionDynBuffer_Length(&runtime->NativeFunctions);
		if (numNative >= TOTEM_MAX_NATIVEFUNCTIONS - 1)
        {
            return totemLinkStatus_Break(totemLinkStatus_TooManyNativeFunctions);
        }
        
		*addressOut = (totemOperandXu)numNative;
		func = totemNativeFunctionDynBuffer_Secure(&runtime->NativeFunctions, 1);
        if(!func)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        func->Address = *addressOut;
        
        totemRuntimeStringMapKey key;
        key.Value = func->Address;
        
        if(totemRuntime_InternString(runtime, name, &key.Key) != totemLinkStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        TOTEM_LOG_BEGIN(Exec, String, Debug, dst)
        {
            totemLog_Write(dst, "NEWSTRING: ");
            totemRuntimeStringValue_Print(&key.Key, dst);
        }
        TOTEM_LOG_END()
        
		if (!totemRuntimeStringValueDynBuffer_Push(&runtime->NativeFunctionNames, &key.Key, 1))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        if(!totemRuntimeStringMap_InsertNoSearch(&runtime->NativeFunctionsLookup, &key))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    func->Callback = cb;
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters)
{
    state->Runtime = runtime;
    
    if (numRegisters > 0)
    {
        totemRegister *regs = TOTEM_MALLOC(sizeof(totemRegister) * numRegisters);
        
        if (!regs)
        {
            return totemLinkStatus_OutOfMemory;
        }
        
        state->LocalRegisters = regs;
        state->NextFreeRegister = regs;
		state->LastFreeRegister = regs + numRegisters;
    }
    else
    {
        state->LocalRegisters = NULL;
        state->NextFreeRegister = NULL;
		state->LastFreeRegister = NULL;
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LoadAndCacheScript(totemRuntime *runtime, totemRuntimeStringValue *name, totemScript **scriptOut)
{
    totemLinkStatus status = totemLinkStatus_Success;
    totemScriptCacheKey key;
    key.Key = *name;
    
    totemLock_Acquire(&runtime->ScriptCacheLock);
    if(totemScriptCache_Get(&runtime->ScriptCache, &key))
    {
        *scriptOut = key.Value;
    }
    else
    {
        const char *filename = totemRuntimeStringValue_GetVal(name);
        FILE *f = totem_fopen(filename, "rb");
        if(!f)
        {
            status = totemLinkStatus_CannotFindFile;
        }
        else
        {
            totemInterpreter interpreter;
            totemInterpreter_InitWithFile(&interpreter, f);
            
            if(!totemInterpreter_Interpret(&interpreter))
            {
                TOTEM_LOG_DO(Exec, Runtime, Error, logger, totemInterpreter_PrintResult(&interpreter, logger));
            }
            else
            {
                totemScript *script = TOTEM_MALLOC(sizeof(totemScript));
                if(!script)
                {
                    TOTEM_LOG_V(Exec, Runtime, Error, "Could not create script %s", totemRuntimeStringValue_GetVal(name));
                    status = totemLinkStatus_OutOfMemory;
                }
                else
                {
                    totemScript_Init(script);
                    key.Value = script;

                    status = totemRuntime_LinkBuild(runtime, &interpreter.Build, script);
                    if(status != totemLinkStatus_Success)
                    {
                        TOTEM_LOG_V(Exec, Runtime, Error, "Could not link build script %s", totemRuntimeStringValue_GetVal(name));
                    }
                    // script cache takes responsibility for freeing the script object if the insert is successful
                    else if(!totemScriptCache_InsertNoSearch(&runtime->ScriptCache, &key))
                    {
                        TOTEM_LOG_V(Exec, Runtime, Error, "Could not insert build script %s into cache", totemRuntimeStringValue_GetVal(name));
                        status = totemLinkStatus_OutOfMemory;
                    }
                    
                    if(status != totemLinkStatus_Success)
                    {
                        totemScript_Cleanup(script);
                        TOTEM_FREE(script, sizeof(*script));
                        *scriptOut = NULL;
                    }
                    else
                    {
                        *scriptOut = script;
                    }
                }
            }
            
            totemInterpreter_Cleanup(&interpreter);
        }
        
        fclose(f);
    }
    
    totemLock_Release(&runtime->ScriptCacheLock);
    return status;
}

totemExecStatus totemExecStatus_Break(totemExecStatus status)
{
    return status;
}

void *totemExecState_Alloc(totemExecState *state, size_t size)
{
    void *ptr = TOTEM_MALLOC(size);
    if (!ptr)
    {
        totemExecState_CollectGarbage(state, totemBool_True);
        ptr = TOTEM_MALLOC(size);
    }
    
    return ptr;
}

totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state)
{
    totemFunctionCall *call = NULL;
    
    if (state->CallStackFreeList)
    {
        call = state->CallStackFreeList;
        state->CallStackFreeList = call->Prev;
    }
    else
    {
        call = totemExecState_Alloc(state, sizeof(totemFunctionCall));
    }
    
    if (!call)
    {
        return NULL;
    }
    
	TOTEM_ZERO(*call);
    return call;
}

void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call)
{
    call->Prev = state->CallStackFreeList;
    state->CallStackFreeList = call;
}

totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRuntimeStringValue *strOut)
{
    if (totemRuntime_InternString(state->Runtime, str, strOut) != totemLinkStatus_Success)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_LoadAndCacheInstance(totemExecState *state, totemRuntimeStringValue *name, totemGCObject **instanceOut)
{
    totemInstanceCacheKey key;
    totemExecStatus execStatus = totemExecStatus_Continue;
    key.Key = *name;
    
    totemLock_Acquire(&state->InstanceCacheLock);
    if(totemInstanceCache_Get(&state->InstanceCache, &key))
    {
        *instanceOut = key.Value;
    }
    else
    {
        totemScript *script = NULL;
        totemLinkStatus status = totemRuntime_LoadAndCacheScript(state->Runtime, name, &script);
        if(status != totemLinkStatus_Success)
        {
            execStatus = totemExecStatus_Stop;
        }
        else
        {
            execStatus = totemExecState_CreateInstance(state, script, totemBool_True, instanceOut);
            if(execStatus != totemExecStatus_Continue)
            {
                TOTEM_LOG_V(Exec, Runtime, Error, "Could not create instance for script %s", totemRuntimeStringValue_GetVal(name));
            }
            else
            {
                key.Value = *instanceOut;
                if(!totemInstanceCache_InsertNoSearch(&state->InstanceCache, &key))
                {
                    TOTEM_LOG_V(Exec, Runtime, Error, "Could not insert instance into map for script %s", totemRuntimeStringValue_GetVal(name));
                    execStatus = totemExecStatus_Stop;
                    totemExecState_DestroyGCObject(state, *instanceOut);
                    *instanceOut = NULL;
                }
            }
        }
    }
    
    totemLock_Release(&state->InstanceCacheLock);
    return execStatus;
}

void totemExecState_Init(totemExecState *state)
{
	TOTEM_ZERO(*state);
    totemExecState_InitGC(state);
    state->GCByteThreshold = 1024 * 1024;
    totemInstanceCache_Init(&state->InstanceCache);
    totemLock_Init(&state->InstanceCacheLock);
}

void totemExecState_SetArgV(totemExecState *state, const char **argv, int num)
{
    state->ArgV = argv;
    state->ArgC = num;
}

void totemExecState_Cleanup(totemExecState *state)
{
	// something went wrong - either memory is corrupted, or a routine hasn't been cleaned up
	// all registers should be clean by this point
	TOTEM_ASSERT(state->LocalRegisters == state->NextFreeRegister);
    
    // cleanup remaining gc objects
    totemExecState_CleanupGC(state);
    
    // function call free-list
    for (totemFunctionCall *call = state->CallStackFreeList; call; )
    {
        totemFunctionCall *next = call->Prev;
        TOTEM_FREE(call, sizeof(totemFunctionCall));
        call = next;
    }
    
    state->CallStackFreeList = NULL;
    
    // gc free-list
    for (totemGCObject *gc = state->GCFreeList; gc;)
    {
        totemGCObject *next = gc->Header.Next.Obj;
        TOTEM_FREE(gc, sizeof(totemGCObject));
        gc = next;
    }
    
    state->GCFreeList = NULL;
    
    // cleanup local stack
	if (state->LocalRegisters)
	{
        TOTEM_FREE(state->LocalRegisters, sizeof(totemRegister) * (state->LastFreeRegister - state->LocalRegisters));
		state->LocalRegisters = NULL;
	}
    
    totemInstanceCache_Cleanup(&state->InstanceCache);
    totemLock_Cleanup(&state->InstanceCacheLock);
}

const char *totemExecStatus_Describe(totemExecStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemExecStatus_Continue);
            TOTEM_STRINGIFY_CASE(totemExecStatus_FunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RegisterOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnexpectedDataType);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnrecognisedOperation);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Stop);
            TOTEM_STRINGIFY_CASE(totemExecStatus_IndexOutOfBounds);
            TOTEM_STRINGIFY_CASE(totemExecStatus_DivideByZero);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InternalBufferOverrun);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InvalidDispatch);
            TOTEM_STRINGIFY_CASE(totemExecStatus_NullPointer);
    }
    
    return "UNKNOWN";
}

const char *totemLinkStatus_Describe(totemLinkStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemLinkStatus_UnexpectedValueType);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_TooManyNativeFunctions);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_FunctionAlreadyDeclared);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_FunctionNotDeclared);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_InvalidNativeFunctionAddress);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_InvalidNativeFunctionName);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_Success);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_CannotFindFile);
    }
    
    return "UNKNOWN";
}
