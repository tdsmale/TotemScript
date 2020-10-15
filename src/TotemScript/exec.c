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
#include <string.h>
#include <stdarg.h>
#include <math.h>

totemLinkStatus totemLinkStatus_Break(totemLinkStatus status)
{
    return status;
}

size_t totemInternedStringHeader_Size(size_t len)
{
	return len + sizeof(totemInternedStringHeader) - TOTEM_SIZEOF_STATIC(totemInternedStringHeader, Data);
}

totemInternedStringHeader *totemInternedStringHeader_Alloc(size_t len)
{
	size_t toAllocate = totemInternedStringHeader_Size(len);
	return (totemInternedStringHeader*)totem_CacheMalloc(toAllocate);
}

void totemInternedStringHeader_Free(totemInternedStringHeader *str)
{
	totem_CacheFree(str, totemInternedStringHeader_Size(str->Length));
}

totemLinkStatus totemRuntimeStringValue_LinkToRegister(totemRuntimeStringValue *val, totemRegister *dst)
{
    switch (val->Type)
    {
        case totemPrivateDataType_InternedString:
			totemRegister_SetInternedString(dst, val->Src.InternedString);
            break;
            
        case totemPrivateDataType_MiniString:
			totemRegister_SetMiniString(dst, val->Src.MiniString);
            break;
            
        default:
            return totemLinkStatus_Break(totemLinkStatus_UnexpectedValueType);
    }
    
    return totemLinkStatus_Success;
}

void totemInstance_Init(totemInstance *instance)
{
	totemInstanceFunctionBuffer_Init(&instance->LocalFunctions);
    instance->Script = NULL;
}

void totemInstance_Reset(totemInstance *instance)
{
	totemInstanceFunctionBuffer_Reset(&instance->LocalFunctions);
}

void totemInstance_Cleanup(totemInstance *instance)
{
	totemInstanceFunctionBuffer_Cleanup(&instance->LocalFunctions);
}

void totemScript_Init(totemScript *script)
{
	totemScriptFunctionBuffer_Init(&script->Functions);
    totemRegisterBuffer_Init(&script->FunctionNames);
    totemRegisterBuffer_Init(&script->GlobalRegisters);
    totemInstructionBuffer_Init(&script->Instructions);
    totemHashMap_Init(&script->FunctionNameLookup);
}

void totemScript_Reset(totemScript *script)
{
#ifdef TOTEM_JIT
	for (size_t i = 0; i < totemScriptFunctionBuffer_Num(&script->Functions); i++)
	{
		if (jitted())
		{
			freeJitFunc();
		}
	}
#endif

	totemScriptFunctionBuffer_Reset(&script->Functions);
	totemRegisterBuffer_Reset(&script->FunctionNames);
	totemRegisterBuffer_Reset(&script->GlobalRegisters);
	totemInstructionBuffer_Reset(&script->Instructions);
    totemHashMap_Reset(&script->FunctionNameLookup);
}

void totemScript_Cleanup(totemScript *script)
{
	totemScriptFunctionBuffer_Cleanup(&script->Functions);
	totemRegisterBuffer_Cleanup(&script->FunctionNames);
	totemRegisterBuffer_Cleanup(&script->GlobalRegisters);
	totemInstructionBuffer_Cleanup(&script->Instructions);
    totemHashMap_Cleanup(&script->FunctionNameLookup);
}

totemBool totemScript_GetFunctionName(totemScript *script, totemOperandXUnsigned addr, totemRuntimeStringValue *valOut)
{
    totemRegister *name = totemRegisterBuffer_At(&script->FunctionNames, addr);
    if (!name)
    {
        return totemBool_False;
    }
    
    totemRegister_GetStringValue(name, valOut);
    return totemBool_True;
}

void totemRuntime_Init(totemRuntime *runtime)
{
	totemNativeFunctionBuffer_Init(&runtime->NativeFunctions);
    totemRegisterBuffer_Init(&runtime->NativeFunctionNames);
    totemHashMap_Init(&runtime->NativeFunctionsLookup);
    totemHashMap_Init(&runtime->InternedStrings);
    totemLock_Init(&runtime->InternedStringsLock);
}

void totemRuntime_Reset(totemRuntime *runtime)
{
	totemNativeFunctionBuffer_Reset(&runtime->NativeFunctions);
	totemRegisterBuffer_Reset(&runtime->NativeFunctionNames);
    totemHashMap_Reset(&runtime->NativeFunctionsLookup);
    totemHashMap_Reset(&runtime->InternedStrings);
    totemLock_Release(&runtime->InternedStringsLock);
}

void totemRuntime_Cleanup(totemRuntime *runtime)
{
	totemNativeFunctionBuffer_Cleanup(&runtime->NativeFunctions);
	totemRegisterBuffer_Cleanup(&runtime->NativeFunctionNames);
	totemHashMap_Cleanup(&runtime->NativeFunctionsLookup);

	// clean up interned strings
	for (totemInt i = 0; i < runtime->InternedStrings.NumBuckets; i++)
	{
		for (totemHashMapEntry *entry = runtime->InternedStrings.Buckets[i]; entry != NULL; entry = entry->Next)
		{
			totemInternedStringHeader *hdr = (totemInternedStringHeader*)entry->Value;
			if (hdr)
			{
				totemInternedStringHeader_Free(hdr);
			}
		}
	}

	totemHashMap_Cleanup(&runtime->InternedStrings);
	totemLock_Cleanup(&runtime->InternedStringsLock);
}

totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRuntimeStringValue *valOut)
{
	if (str->Length <= TOTEM_MINISTRING_MAXLENGTH)
	{
		valOut->Type = totemPrivateDataType_MiniString;
		TOTEM_ZERO(valOut->Src.MiniString);
		memcpy(valOut->Src.MiniString, str->Value, str->Length);

		return totemLinkStatus_Success;
	}

	totemLinkStatus status = totemLinkStatus_Success;
	totemLock_Acquire(&runtime->InternedStringsLock);

	totemHashMapEntry *result = totemHashMap_Find(&runtime->InternedStrings, str->Value, str->Length);
	if (result)
	{
		valOut->Type = totemPrivateDataType_InternedString;
		valOut->Src.InternedString = (totemInternedStringHeader*)result->Value;
	}
	else
	{
		totemInternedStringHeader *newStr = totemInternedStringHeader_Alloc(str->Length);
		if (!newStr)
		{
			status = totemLinkStatus_OutOfMemory;
		}
		else
		{
			newStr->Length = str->Length;

			newStr->Hash = totem_Hash(str->Value, newStr->Length);
			memcpy(newStr->Data, str->Value, newStr->Length);
			newStr->Data[newStr->Length] = 0;

			if (!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, newStr->Data, newStr->Length, (uintptr_t)newStr, newStr->Hash))
			{
				totemInternedStringHeader_Free(newStr);
				status = totemLinkStatus_OutOfMemory;
			}
			else
			{
				valOut->Type = totemPrivateDataType_InternedString;
				valOut->Src.InternedString = newStr;
			}
		}
	}

	totemLock_Release(&runtime->InternedStringsLock);

	if (status != totemLinkStatus_Success)
	{
		return totemLinkStatus_Break(status);
	}

	return status;
}

totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *script)
{
	totemScript_Reset(script);

	// instructions
	if (!totemInstructionBuffer_Insert(&script->Instructions, totemInstructionBuffer_Bottom(&build->Instructions), totemInstructionBuffer_Num(&build->Instructions)))
	{
		return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
	}

	// functions
	size_t numFunctions = totemScriptFunctionPrototypeBuffer_Num(&build->Functions);
	if (!totemScriptFunctionBuffer_Secure(&script->Functions, numFunctions))
	{
		return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
	}

	if (!totemRegisterBuffer_Secure(&script->FunctionNames, numFunctions))
	{
		return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
	}
    
    totemInstruction *instructions = totemInstructionBuffer_Bottom(&script->Instructions);
    totemScriptFunction *funcs = totemScriptFunctionBuffer_At(&script->Functions, 0);
    totemRegister *strs = totemRegisterBuffer_At(&script->FunctionNames, 0);
	totemScriptFunctionPrototype *funcProts = totemScriptFunctionPrototypeBuffer_At(&build->Functions, 0);
    
    for (size_t i = 0; i < numFunctions; i++)
    {
        totemRegister *str = &strs[i];
        totemScriptFunctionPrototype *funcProt = &funcProts[i];
        totemScriptFunction *func = &funcs[i];
        
        func->Address = (totemOperandXUnsigned)i;
        func->InstructionsStart = instructions + (funcProt->InstructionsStart);
		func->NumInstructions = funcProt->NumInstructions;
        func->RegistersNeeded = funcProt->RegistersNeeded;
        
        totemRuntimeStringValue newVal;
        if (totemRuntime_InternString(runtime, &funcProt->Name, &newVal) != totemLinkStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        totemLinkStatus status = totemRuntimeStringValue_LinkToRegister(&newVal, str);
        if (status != totemLinkStatus_Success)
        {
            return status;
        }

#ifdef TOTEM_JIT
		if (!jitterize(func))
#endif
		{
			func->Invoke = totemExecState_ExecuteInstructions;
		}
    }
    
    // function name lookup
    if (!totemHashMap_TakeFrom(&script->FunctionNameLookup, &build->FunctionLookup))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    // check script function names against existing native functions
	for (size_t i = 0; i < totemScriptFunctionPrototypeBuffer_Num(&build->Functions); i++)
    {
        totemOperandXUnsigned dummyAddr;
        totemScriptFunctionPrototype *func = totemScriptFunctionPrototypeBuffer_At(&build->Functions, i);
        
        if (func->Name.Value != NULL)
        {
            if (totemRuntime_GetNativeFunctionAddress(runtime, &func->Name, &dummyAddr) == totemBool_True)
            {
                return totemLinkStatus_Break(totemLinkStatus_FunctionAlreadyDeclared);
            }
        }
    }
    
    // global register values
    size_t numGlobalRegs = totemRegisterPrototypeBuffer_Num(&build->GlobalRegisters.Registers);
    
    if(!totemRegisterBuffer_Secure(&script->GlobalRegisters, numGlobalRegs))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    for(size_t i = 0; i < numGlobalRegs; i++)
    {
        totemRegisterPrototype *prototype = totemRegisterPrototypeBuffer_At(&build->GlobalRegisters.Registers, i);
        totemRegister *reg = totemRegisterBuffer_At(&script->GlobalRegisters, i);
        
        switch(prototype->DataType)
        {
                // intern string values
            case totemPublicDataType_String:
            {
                totemRuntimeStringValue newVal;
				if (totemRuntime_InternString(runtime, prototype->Value.String, &newVal) != totemLinkStatus_Success)
                {
                    return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
                }
                
                totemLinkStatus status = totemRuntimeStringValue_LinkToRegister(&newVal, reg);
                if (status != totemLinkStatus_Success)
                {
                    return status;
                }
                
                break;
            }
                
                // fix function pointers
            case totemPublicDataType_Function:
            {
				if (prototype->Value.FunctionPointer.Type == totemFunctionType_Native)
                {
					totemString *funcName = totemStringBuffer_At(&build->NativeFunctionNames, prototype->Value.FunctionPointer.Address);
                    if(!funcName)
                    {
                        return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionName);
                    }
                    
                    totemOperandXUnsigned funcAddr = 0;
                    if(!totemRuntime_GetNativeFunctionAddress(runtime, funcName, &funcAddr))
                    {
                        return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                    }
                    
					totemNativeFunction *func = totemNativeFunctionBuffer_At(&runtime->NativeFunctions, funcAddr);
                    if (!func)
                    {
                        return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                    }
                    
                    totemRegister_SetNativeFunction(reg, func);
                }
                else
                {
					totemRegister_SetInstanceFunction(reg, (totemInstanceFunction*)/*all apologies*/prototype->Value.FunctionPointer.Address);
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
				totemRegister_SetBoolean(reg, prototype->Value.Boolean);
                break;
                
            case totemPublicDataType_Null:
                totemRegister_SetNull(reg);
                break;
                
            default:
                return totemLinkStatus_Break(totemLinkStatus_UnexpectedValueType);
        }
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunctions(totemRuntime *runtime, totemNativeFunctionPrototype *funcs, totemOperandXUnsigned num)
{
	if (totemNativeFunctionBuffer_Max(&runtime->NativeFunctions) < num)
    {
		if (!totemNativeFunctionBuffer_Reserve(&runtime->NativeFunctions, num))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    for (totemOperandXUnsigned i = 0; i < num; i++)
    {
        totemOperandXUnsigned addr;
        totemLinkStatus status = totemRuntime_LinkNativeFunction(runtime, funcs[i].Callback, &funcs[i].Name, &addr);
        if (status != totemLinkStatus_Success)
        {
            return status;
        }
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemRuntimeFunctionCb cb, totemString *name, totemOperandXUnsigned *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    totemNativeFunction *func = NULL;
    if(result != NULL)
    {
        func = totemNativeFunctionBuffer_At(&runtime->NativeFunctions, result->Value);
        *addressOut = (totemOperandXUnsigned)result->Value;
    }
    else
    {
		if (totemNativeFunctionBuffer_Num(&runtime->NativeFunctions) >= TOTEM_MAX_NATIVEFUNCTIONS - 1)
        {
            return totemLinkStatus_Break(totemLinkStatus_TooManyNativeFunctions);
        }
        
		*addressOut = (totemOperandXUnsigned)totemNativeFunctionBuffer_Num(&runtime->NativeFunctions);
		func = totemNativeFunctionBuffer_Secure(&runtime->NativeFunctions, 1);
        if(!func)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        func->Address = *addressOut;
        
        totemRegister value;
		TOTEM_ZERO(&value);
        totemRuntimeStringValue newStr;
        
        if(totemRuntime_InternString(runtime, name, &newStr) != totemLinkStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
		if (!totemRegisterBuffer_Insert(&runtime->NativeFunctionNames, &value, 1))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        if(!totemHashMap_Insert(&runtime->NativeFunctionsLookup, name->Value, name->Length, *addressOut))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    func->Callback = cb;
    return totemLinkStatus_Success;
}

totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, totemOperandXUnsigned *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    if(result != NULL)
    {
        *addressOut = (totemOperandXUnsigned)result->Value;
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemBool totemRuntime_GetNativeFunctionName(totemRuntime *runtime, totemOperandXUnsigned addr, totemRuntimeStringValue *val)
{
    totemRegister *reg = totemRegisterBuffer_At(&runtime->NativeFunctionNames, addr);
    if (reg)
    {
        totemRegister_GetStringValue(reg, val);
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters)
{
    state->Runtime = runtime;
    
    if (numRegisters > 0)
    {
        totemRegister *regs = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
        
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

totemExecStatus totemExecStatus_Break(totemExecStatus status)
{
    return status;
}

void *totemExecState_Alloc(totemExecState *state, size_t size)
{
    void *ptr = totem_CacheMalloc(size);
    if (!ptr)
    {
        totemExecState_CollectGarbage(state, totemBool_True);
        ptr = totem_CacheMalloc(size);
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
    
	TOTEM_ZERO(call);
    return call;
}

void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call)
{
    call->Prev = state->CallStackFreeList;
    state->CallStackFreeList = call;
}

totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRegister *strOut)
{
    totemRuntimeStringValue newStr;
    
    if (totemRuntime_InternString(state->Runtime, str, &newStr) != totemLinkStatus_Success)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemExecState_AssignNewString(state, strOut, &newStr);
    return totemExecStatus_Continue;
}

void totemExecState_CleanupRegisterList(totemExecState *state, totemRegister *regs, totemInt num)
{
	for (totemInt i = 0; i < num; i++)
    {
        totemExecState_DecRefCount(state, &regs[i]);
    }
}

void totemExecState_Init(totemExecState *state)
{
	TOTEM_ZERO(state);
    totemExecState_InitGC(state);
    state->GCByteThreshold = 1024 * 1024;
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
	TOTEM_DEBUG_ASSERT(state->LocalRegisters == state->NextFreeRegister);

    // cleanup remaining gc objects
    totemExecState_CleanupGC(state);
    
    // function call free-list
    for (totemFunctionCall *call = state->CallStackFreeList; call; )
    {
        totemFunctionCall *next = call->Prev;
        totem_CacheFree(call, sizeof(totemFunctionCall));
        call = next;
    }
    
    state->CallStackFreeList = NULL;
    
    // gc free-list
    for (totemGCObject *gc = state->GCFreeList; gc;)
    {
        totemGCObject *next = gc->Header.Next.Obj;
        totem_CacheFree(gc, sizeof(totemGCObject));
        gc = next;
    }
    
    state->GCFreeList = NULL;
    
    // cleanup local stack
	if (state->LocalRegisters)
	{
		totem_CacheFree(state->LocalRegisters, sizeof(totemRegister) * (state->LastFreeRegister - state->LocalRegisters));
		state->LocalRegisters = NULL;
	}
}

const char *totemExecStatus_Describe(totemExecStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemExecStatus_Continue);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InstructionOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_NativeFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RegisterOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InstanceFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_ScriptNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnexpectedDataType);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnrecognisedOperation);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Stop);
            TOTEM_STRINGIFY_CASE(totemExecStatus_IndexOutOfBounds);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RefCountOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_DivideByZero);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InternalBufferOverrun);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InvalidKey);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InvalidDispatch);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnrecognisedInstructionType);
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
    }
    
    return "UNKNOWN";
}