//
//  exec.c
//  TotemScript
//
//  Created by Timothy Smale on 17/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <stdarg.h>
#include <math.h>
#include <TotemScript/exec.h>
#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <string.h>

#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }

#define TOTEM_GET_OPERANDA_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))]
#define TOTEM_GET_OPERANDB_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))]
#define TOTEM_GET_OPERANDC_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))]

#define TOTEM_REGISTER_ASSIGN(dst, perform) \
if((dst)->DataType == totemDataType_Array) \
{ \
    totemRuntimeArrayHeader_DefRefCount((dst)->Value.Array); \
} \
perform; \
if((dst)->DataType == totemDataType_Array) \
{ \
    TOTEM_EXEC_CHECKRETURN(totemRuntimeArrayHeader_IncRefCount((dst)->Value.Array)); \
}

#define TOTEM_REGISTER_ASSIGN_GENERIC(dst, src) TOTEM_REGISTER_ASSIGN((dst), memcpy((dst), (src), sizeof(totemRegister)))
#define TOTEM_REGISTER_ASSIGN_FLOAT(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemDataType_Float; (dst)->Value.Float = (val))
#define TOTEM_REGISTER_ASSIGN_INT(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemDataType_Int; (dst)->Value.Int = (val))
#define TOTEM_REGISTER_ASSIGN_TYPE(dst,val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemDataType_Type; (dst)->Value.DataType = (val))
#define TOTEM_REGISTER_ASSIGN_NULL(dst) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemDataType_Null; (dst)->Value.Int = 0)
#define TOTEM_REGISTER_ASSIGN_ARRAY(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemDataType_Array; (dst)->Value.Array = (val))
#define TOTEM_REGISTER_ASSIGN_STRING(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemDataType_String; (dst)->Value.InternedString = (val))
#define TOTEM_REGISTER_ASSIGN_FUNC(dst, val, type) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemDataType_Function; (dst)->Value.FunctionPointer.Address = (val); (dst)->Value.FunctionPointer.Type = (type))

totemLinkStatus totemLinkStatus_Break(totemLinkStatus status)
{
    return status;
}

totemExecStatus totemExecStatus_Break(totemExecStatus status)
{
    return status;
}

totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemInternedStringHeader **strOut)
{
    *strOut = totemRuntime_InternString(state->Runtime, str);
    if(!*strOut)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_EmptyString(totemExecState *state, totemInternedStringHeader **strOut)
{
    totemString empty;
    empty.Value = "";
    empty.Length = 0;
    
    return totemExecState_InternString(state, &empty, strOut);
}

totemExecStatus totemExecState_IntToString(totemExecState *state, totemInt val, totemInternedStringHeader **strOut)
{
    char buffer[256];
    int result = snprintf(buffer, TOTEM_ARRAYSIZE(buffer), "%llu", val);
    if(result < 0 || result >= TOTEM_ARRAYSIZE(buffer))
    {
        return totemExecStatus_Break(totemExecStatus_InternalBufferOverrun);
    }
    
    totemString string;
    string.Length = result;
    string.Value = buffer;
    
    return totemExecState_InternString(state, &string, strOut);
}

totemExecStatus totemExecState_FloatToString(totemExecState *state, totemFloat val, totemInternedStringHeader **strOut)
{
    char buffer[256];
    int result = snprintf(buffer, TOTEM_ARRAYSIZE(buffer), "%.6g", val);
    if(result < 0 || result >= TOTEM_ARRAYSIZE(buffer))
    {
        return totemExecStatus_Break(totemExecStatus_InternalBufferOverrun);
    }
    
    totemString string;
    string.Length = result;
    string.Value = buffer;
    
    return totemExecState_InternString(state, &string, strOut);
}

totemExecStatus totemExecState_TypeToString(totemExecState *state, totemDataType type, totemInternedStringHeader **strOut)
{
    totemString str;
    
    switch(type)
    {
        case totemDataType_Int:
            totemString_FromLiteral(&str, "int");
            break;
            
        case totemDataType_Type:
            totemString_FromLiteral(&str, "type");
            break;
            
        case totemDataType_Null:
            totemString_FromLiteral(&str, "null");
            break;
            
        case totemDataType_Array:
            totemString_FromLiteral(&str, "array");
            break;
            
        case totemDataType_Float:
            totemString_FromLiteral(&str, "float");
            break;
            
        case totemDataType_String:
            totemString_FromLiteral(&str, "string");
            break;
            
        case totemDataType_Function:
            totemString_FromLiteral(&str, "function");
            break;
            
        default:
            return totemExecState_EmptyString(state, strOut);
    }
    
    return totemExecState_InternString(state, &str, strOut);
}

totemExecStatus totemExecState_NullString(totemExecState *state, totemInternedStringHeader **strOut)
{
    totemString nullStr;
    totemString_FromLiteral(&nullStr, "null");
    
    return totemExecState_InternString(state, &nullStr, strOut);
}

totemExecStatus totemExecState_FunctionPointerToString(totemExecState *state, totemFunctionPointer *ptr, totemInternedStringHeader **hdr)
{
    switch(ptr->Type)
    {
        case totemFunctionType_Native:
        {
            hdr = totemMemoryBuffer_Get(&state->Runtime->NativeFunctionNames, ptr->Address);
            if(!hdr)
            {
                return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
            }
            break;
        }
            
        case totemFunctionType_Script:
        {
            totemScript *scr = totemMemoryBuffer_Get(&state->Runtime->Scripts, state->CallStack->CurrentActor->ScriptHandle);
            if(!scr)
            {
                return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
            }
            
            hdr = totemMemoryBuffer_Get(&scr->FunctionNames, ptr->Address);
            if(!hdr)
            {
                return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
            }
            
            break;
        }
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemRuntimeArrayHeader *arr, totemInternedStringHeader **strOut)
{
    if(arr->NumRegisters == 0)
    {
        return totemExecState_EmptyString(state, strOut);
    }
    
    totemInternedStringHeader **strings = totem_CacheMalloc(sizeof(totemInternedStringHeader*) * arr->NumRegisters);
    if(!strings)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    uint32_t len = 0;
    
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
    for(size_t i = 0; i < arr->NumRegisters; i++)
    {
        totemInternedStringHeader *str = NULL;
        
        switch(regs[i].DataType)
        {
            case totemDataType_Int:
                TOTEM_EXEC_CHECKRETURN(totemExecState_IntToString(state, regs[i].Value.Int, &str));
                break;
                
            case totemDataType_Null:
                TOTEM_EXEC_CHECKRETURN(totemExecState_NullString(state, &str));
                break;
                
            case totemDataType_Type:
                TOTEM_EXEC_CHECKRETURN(totemExecState_TypeToString(state, regs[i].Value.DataType, &str));
                break;
                
            case totemDataType_Array:
                TOTEM_EXEC_CHECKRETURN(totemExecState_ArrayToString(state, regs[i].Value.Array, &str));
                break;
                
            case totemDataType_Float:
                TOTEM_EXEC_CHECKRETURN(totemExecState_FloatToString(state, regs[i].Value.Float, &str));
                break;
                
            case totemDataType_String:
                str = regs[i].Value.InternedString;
                break;
                
            case totemDataType_Function:
                TOTEM_EXEC_CHECKRETURN(totemExecState_FunctionPointerToString(state, &regs[i].Value.FunctionPointer, &str));
                break;
        }
        
        if(!str)
        {
            totem_CacheFree(strings, sizeof(totemInternedStringHeader*) * arr->NumRegisters);
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
        }
        
        len += str->Length;
        strings[i] = str;
    }
    
    if(!len)
    {
        totem_CacheFree(strings, sizeof(totemInternedStringHeader*) * arr->NumRegisters);
        return totemExecState_EmptyString(state, strOut);
    }
    
    char *buffer = totem_CacheMalloc(len + 1);
    if(!buffer)
    {
        totem_CacheFree(strings, sizeof(totemInternedStringHeader*) * arr->NumRegisters);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    char *current = buffer;
    for(size_t i = 0; i < arr->NumRegisters; i++)
    {
        totemInternedStringHeader *str = strings[i];
        memcpy(current, totemInternedStringHeader_GetString(str), str->Length);
        current += str->Length;
    }
    
    totemString toIntern;
    toIntern.Value = buffer;
    toIntern.Length = len;
    
    totemExecStatus status = totemExecState_InternString(state, &toIntern, strOut);
    totem_CacheFree(buffer, len + 1);
    return status;
}

totemRegister *totemRuntimeArrayHeader_GetRegisters(totemRuntimeArrayHeader *hdr)
{
    char *ptr = (char*)hdr;
    return (totemRegister*)(ptr + (sizeof(totemRuntimeArrayHeader)));
}

totemRuntimeArrayHeader *totemRuntimeArrayHeader_Create(uint32_t numRegisters)
{
    size_t toAllocate = sizeof(totemRuntimeArrayHeader) + (numRegisters * sizeof(totemRegister));
    totemRuntimeArrayHeader *hdr = totem_CacheMalloc(toAllocate);
    if(!hdr)
    {
        return NULL;
    }
    
    hdr->RefCount = 0;
    hdr->NumRegisters = numRegisters;
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(hdr);
    memset(regs, 0, sizeof(totemRegister) * numRegisters);
    return hdr;
}

void totemRuntimeArrayHeader_Destroy(totemRuntimeArrayHeader *arr)
{
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
    
    for(uint32_t i = 0; i < arr->NumRegisters; i++)
    {
        if(regs[i].DataType == totemDataType_Array)
        {
            totemRuntimeArrayHeader_DefRefCount(regs[i].Value.Array);
        }
    }
    
    totem_CacheFree(arr, sizeof(totemRuntimeArrayHeader) + (sizeof(totemRegister) * arr->NumRegisters));
}

void totemRuntimeArrayHeader_DefRefCount(totemRuntimeArrayHeader *arr)
{
    switch(arr->RefCount)
    {
        case 0:
            break;
            
        case 1:
            arr->RefCount = 0;
            totemRuntimeArrayHeader_Destroy(arr);
            break;
            
        default:
            arr->RefCount--;
            break;
    }
}

totemExecStatus totemRuntimeArrayHeader_IncRefCount(totemRuntimeArrayHeader *arr)
{
    if(arr->RefCount == UINT32_MAX - 1)
    {
        return totemExecStatus_Break(totemExecStatus_RefCountOverflow);
    }
    
    arr->RefCount++;
    
    return totemExecStatus_Continue;
}

totemExecStatus totemActor_Init(totemActor *actor, totemRuntime *runtime, size_t scriptAddress)
{
    memset(actor, 0, sizeof(totemActor));
    totemMemoryBuffer_Init(&actor->GlobalRegisters, sizeof(totemRegister));
    
    actor->ScriptHandle = scriptAddress;
    
    totemScript *script = totemMemoryBuffer_Get(&runtime->Scripts, scriptAddress);
    if(script == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
    }

	if (!totemMemoryBuffer_TakeFrom(&actor->GlobalRegisters, &script->GlobalRegisters))
	{
		return totemExecStatus_Break(totemExecStatus_OutOfMemory);
	}
    
    return totemExecStatus_Continue;
}

void totemActor_Cleanup(totemActor *actor)
{
    totemMemoryBuffer_Cleanup(&actor->GlobalRegisters);
}

const char *totemInternedStringHeader_GetString(totemInternedStringHeader *hdr)
{
    char *ptr = (char*)hdr;
    return (const char*)(ptr + sizeof(totemInternedStringHeader));
}

void totemInternedStringHeader_Destroy(totemInternedStringHeader *str)
{
    totem_CacheFree(str, sizeof(totemInternedStringHeader) + 1 + str->Length);
}

void totemScript_Init(totemScript *script)
{
    totemMemoryBuffer_Init(&script->Functions, sizeof(totemScriptFunction));
    totemMemoryBuffer_Init(&script->FunctionNames, sizeof(totemInternedStringHeader*));
    totemMemoryBuffer_Init(&script->GlobalRegisters, sizeof(totemRegister));
    totemMemoryBuffer_Init(&script->Instructions, sizeof(totemInstruction));
    totemHashMap_Init(&script->FunctionNameLookup);
}

void totemScript_Reset(totemScript *script)
{
    totemMemoryBuffer_Reset(&script->Functions);
    totemMemoryBuffer_Reset(&script->FunctionNames);
    totemMemoryBuffer_Reset(&script->GlobalRegisters);
    totemMemoryBuffer_Reset(&script->Instructions);
    totemHashMap_Reset(&script->FunctionNameLookup);
}

void totemScript_Cleanup(totemScript *script)
{
    totemMemoryBuffer_Cleanup(&script->Functions);
    totemMemoryBuffer_Cleanup(&script->FunctionNames);
    totemMemoryBuffer_Cleanup(&script->GlobalRegisters);
    totemMemoryBuffer_Cleanup(&script->Instructions);
    totemHashMap_Cleanup(&script->FunctionNameLookup);
}

void totemRuntime_Init(totemRuntime *runtime)
{
    totemMemoryBuffer_Init(&runtime->NativeFunctions, sizeof(totemNativeFunction));
    totemMemoryBuffer_Init(&runtime->Scripts, sizeof(totemScript));
    totemMemoryBuffer_Init(&runtime->NativeFunctionNames, sizeof(totemInternedStringHeader*));
    totemHashMap_Init(&runtime->ScriptLookup);
    totemHashMap_Init(&runtime->NativeFunctionsLookup);
    totemHashMap_Init(&runtime->InternedStrings);
    runtime->FunctionCallFreeList = NULL;
}

void totemRuntime_Reset(totemRuntime *runtime)
{
    totemMemoryBuffer_Reset(&runtime->NativeFunctions);
    totemMemoryBuffer_Reset(&runtime->Scripts);
    totemMemoryBuffer_Reset(&runtime->NativeFunctionNames);
    totemHashMap_Reset(&runtime->ScriptLookup);
    totemHashMap_Reset(&runtime->NativeFunctionsLookup);
    totemHashMap_Reset(&runtime->InternedStrings);
}

void totemRuntime_Cleanup(totemRuntime *runtime)
{
    totemMemoryBuffer_Cleanup(&runtime->NativeFunctions);
    totemMemoryBuffer_Cleanup(&runtime->Scripts);
    totemMemoryBuffer_Cleanup(&runtime->NativeFunctionNames);
    totemHashMap_Cleanup(&runtime->ScriptLookup);
    totemHashMap_Cleanup(&runtime->NativeFunctionsLookup);
    
    // clean up interned strings
    for(size_t i = 0; i < runtime->InternedStrings.NumBuckets; i++)
    {
        for(totemHashMapEntry *entry = runtime->InternedStrings.Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemInternedStringHeader *hdr = (totemInternedStringHeader*)entry->Value;
            if(hdr)
            {
                totemInternedStringHeader_Destroy(hdr);
            }
        }
    }
    
    totemHashMap_Cleanup(&runtime->InternedStrings);
    
    // clean up function calls
    while(runtime->FunctionCallFreeList)
    {
        totemFunctionCall *call = runtime->FunctionCallFreeList;
        runtime->FunctionCallFreeList = call->Prev;
        totem_CacheFree(call, sizeof(totemFunctionCall));
    }
}

totemInternedStringHeader *totemRuntime_InternStringPrecomputed(totemRuntime *runtime, totemInternedStringHeader *hdr)
{
    const char *str = totemInternedStringHeader_GetString(hdr);
    
    if(!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, str, hdr->Length, (uintptr_t)str, hdr->Hash))
    {
        return NULL;
    }
    
    return hdr;
}

totemInternedStringHeader *totemRuntime_InternString(totemRuntime *runtime, totemString *str)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->InternedStrings, str->Value, str->Length);
    if(result)
    {
        return (totemInternedStringHeader*)result->Value;
    }
    
    size_t toAllocate = str->Length + sizeof(totemInternedStringHeader) + 1;
    
    totemInternedStringHeader *newStr = totem_CacheMalloc(toAllocate);
    if(!newStr)
    {
        return NULL;
    }
    newStr->Length = str->Length;
    
    if(!newStr)
    {
        return NULL;
    }
    
    char *newStrVal = (char*)totemInternedStringHeader_GetString(newStr);
    
    newStr->Hash = totem_Hash(str->Value, newStr->Length);
    memcpy(newStrVal, str->Value, newStr->Length);
    newStrVal[newStr->Length] = 0;
    
    if(!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, newStrVal, newStr->Length, (uintptr_t)newStr, newStr->Hash))
    {
        totemInternedStringHeader_Destroy(newStr);
        return NULL;
    }
    
    return newStr;
}

totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemString *name, size_t *indexOut)
{
    totemScript *script = NULL;
    totemHashMapEntry *existing = totemHashMap_Find(&runtime->ScriptLookup, name->Value, name->Length);
    if(existing == NULL)
    {
        *indexOut = totemMemoryBuffer_GetNumObjects(&runtime->Scripts);
        if(!totemHashMap_Insert(&runtime->ScriptLookup, name->Value, name->Length, *indexOut))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        script = totemMemoryBuffer_Secure(&runtime->Scripts, 1);
        if(!script)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        totemScript_Init(script);
    }
    else
    {
        *indexOut = existing->Value;
        script = totemMemoryBuffer_Get(&runtime->Scripts, *indexOut);
        totemScript_Reset(script);
    }
    
    // 1. intern strings
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.GlobalRegisterStrings); i++)
    {
        totemOperandXUnsigned index = *((totemOperandXUnsigned*)totemMemoryBuffer_Get(&build->GlobalRegisters.GlobalRegisterStrings, i));
        totemRegister *reg = totemMemoryBuffer_Get(&build->GlobalRegisters.Registers, index);
        
        totemString *str = (totemString*)reg->Value.InternedString;
        totemInternedStringHeader *internedStr = totemRuntime_InternString(runtime, str);
        if(!internedStr)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        reg->Value.InternedString = internedStr;
    }
    
    // 2. check script function names against existing native functions
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->Functions); i++)
    {
        totemOperandXUnsigned dummyAddr;
        totemScriptFunctionPrototype *func = totemMemoryBuffer_Get(&build->Functions, i);
        
        if(totemRuntime_GetNativeFunctionAddress(runtime, &func->Name, &dummyAddr))
        {
            build->ErrorContext = func;
            return totemLinkStatus_Break(totemLinkStatus_FunctionAlreadyDeclared);
        }
    }
    
    // 3. link native functions
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->NativeFunctionCallInstructions); i++)
    {
        size_t *instructionAddr = totemMemoryBuffer_Get(&build->NativeFunctionCallInstructions, i);
        totemInstruction *instruction = totemMemoryBuffer_Get(&build->Instructions, *instructionAddr);
        
        totemOperandXUnsigned functionAddr = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*instruction);
        
        totemString *funcName = totemMemoryBuffer_Get(&build->NativeFunctionNames, functionAddr);
        if(!funcName)
        {
            return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionName);
        }
        
        totemOperandXUnsigned funcAddr = 0;
        if(!totemRuntime_GetNativeFunctionAddress(runtime, funcName, &funcAddr))
        {
            build->ErrorContext = funcName;
            return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
        }
        
        totemOperandXUnsigned realFuncAddr = (totemOperandXUnsigned)funcAddr;
        if(totemInstruction_SetBxUnsigned(instruction, realFuncAddr) != totemEvalStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionAddress);
        }
    }
    
    // 4. link native function pointers
    for(size_t i = 0; i < build->GlobalRegisters.FunctionPointers.NumBuckets; i++)
    {
        for(totemHashMapEntry *entry = build->GlobalRegisters.FunctionPointers.Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemOperandXUnsigned registerIndex = (totemOperandXUnsigned)entry->Value;
            totemRegisterPrototype *reg = totemMemoryBuffer_Get(&build->GlobalRegisters.Registers, registerIndex);
            
            if(reg == NULL || reg->DataType != totemDataType_Function)
            {
                return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionAddress);
            }
            
            if(reg->Value.FunctionPointer.Type == totemFunctionType_Native)
            {
                totemString *funcName = totemMemoryBuffer_Get(&build->NativeFunctionNames, reg->Value.FunctionPointer.Address);
                if(!funcName)
                {
                    return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionName);
                }
                
                totemOperandXUnsigned funcAddr = 0;
                if(!totemRuntime_GetNativeFunctionAddress(runtime, funcName, &funcAddr))
                {
                    build->ErrorContext = funcName;
                    return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                }
                
                reg->Value.FunctionPointer.Address = (totemOperandXUnsigned)funcAddr;
            }
        }
    }
    
    // 4. global register values
    size_t numGlobalRegs = totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.Registers);
    
    if(!totemMemoryBuffer_Secure(&script->GlobalRegisters, numGlobalRegs))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    for(size_t i = 0; i < numGlobalRegs; i++)
    {
        totemRegisterPrototype *prototype = totemMemoryBuffer_Get(&build->GlobalRegisters.Registers, i);
        totemRegister *reg = totemMemoryBuffer_Get(&script->GlobalRegisters, i);
        
        reg->DataType = prototype->DataType;
        reg->Value.Data = prototype->Value.Data;
    }
    
    // 5. instructions
    if(!totemMemoryBuffer_TakeFrom(&script->Instructions, &build->Instructions))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    // 6. functions
    size_t numFunctions = totemMemoryBuffer_GetNumObjects(&build->Functions);
    if(!totemMemoryBuffer_Secure(&script->Functions, numFunctions) || !totemMemoryBuffer_Secure(&script->FunctionNames, numFunctions))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    totemScriptFunction *funcs = totemMemoryBuffer_Get(&script->Functions, 0);
    totemInternedStringHeader **strs = totemMemoryBuffer_Get(&script->FunctionNames, 0);
    totemScriptFunctionPrototype *funcProts = totemMemoryBuffer_Get(&build->Functions, 0);
    for(size_t i = 0; i < numFunctions; i++)
    {
        funcs[i].InstructionsStart = funcProts[i].InstructionsStart;
        funcs[i].RegistersNeeded = funcProts[i].RegistersNeeded;
        
        strs[i] = totemRuntime_InternString(runtime, &funcProts[i].Name);
        if(!strs[i])
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    // 7. function name lookup
    if(!totemHashMap_TakeFrom(&script->FunctionNameLookup, &build->FunctionLookup))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString *name, totemOperandXUnsigned *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    totemNativeFunction *addr = NULL;;
    if(result != NULL)
    {
        addr = totemMemoryBuffer_Get(&runtime->NativeFunctions, result->Value);
        *addressOut = (totemOperandXUnsigned)result->Value;
    }
    else
    {
        if(totemMemoryBuffer_GetNumObjects(&runtime->NativeFunctions) >= TOTEM_MAX_NATIVEFUNCTIONS - 1)
        {
            return totemLinkStatus_Break(totemLinkStatus_TooManyNativeFunctions);
        }
        
        *addressOut = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&runtime->NativeFunctions);
        addr = totemMemoryBuffer_Secure(&runtime->NativeFunctions, 1);
        if(!addr)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        totemInternedStringHeader *funcName = totemRuntime_InternString(runtime, name);
        if(!funcName)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        if(!totemMemoryBuffer_Insert(&runtime->NativeFunctionNames, &funcName, sizeof(totemInternedStringHeader*)))
        {
            totem_CacheFree(funcName, name->Length + 1);
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        if(!totemHashMap_Insert(&runtime->NativeFunctionsLookup, name->Value, name->Length, *addressOut))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    *addr = func;
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

totemFunctionCall *totemRuntime_SecureFunctionCall(totemRuntime *runtime)
{
    totemFunctionCall *call = NULL;
    
    if(runtime->FunctionCallFreeList)
    {
        call = runtime->FunctionCallFreeList;
        runtime->FunctionCallFreeList = call->Prev;
    }
    else
    {
        call = totem_CacheMalloc(sizeof(totemFunctionCall));
        if(!call)
        {
            return NULL;
        }
    }
    
    memset(call, 0, sizeof(totemFunctionCall));
    return call;
}

void totemRuntime_FreeFunctionCall(totemRuntime *runtime, totemFunctionCall *call)
{
    call->Prev = NULL;
    
    if(runtime->FunctionCallFreeList)
    {
        call->Prev = runtime->FunctionCallFreeList;
    }
    
    runtime->FunctionCallFreeList = call;
}

totemBool totemExecState_Init(totemExecState *state, totemRuntime *runtime, size_t numRegisters)
{
    memset(state, 0, sizeof(totemExecState));
    
    state->Runtime = runtime;
    
    if(state->Registers[totemOperandType_LocalRegister])
    {
        totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
    }
    
    state->Registers[totemOperandType_LocalRegister] = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
    if(state->Registers[totemOperandType_LocalRegister] == NULL)
    {
        return totemBool_False;
    }
    
    memset(state->Registers[totemOperandType_LocalRegister], 0, sizeof(totemRegister) * state->MaxLocalRegisters);
    state->MaxLocalRegisters = numRegisters;
    state->UsedLocalRegisters = 0;
    return totemBool_True;
}

void totemExecState_Cleanup(totemExecState *state)
{
    totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
}

totemExecStatus totemExecState_PushFunctionCall(totemExecState *state, totemFunctionType funcType, size_t functionAddress, totemActor *actor, totemRegister *returnReg, uint8_t numRegisters)
{
    totemFunctionCall *call = totemRuntime_SecureFunctionCall(state->Runtime);
    if(call == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    if((state->MaxLocalRegisters - state->UsedLocalRegisters) < numRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
    }
    
    call->CurrentActor = actor;
    call->ReturnRegister = returnReg;
    call->PreviousFrameStart = state->Registers[totemOperandType_LocalRegister];
    call->Type = funcType;
    call->FunctionHandle = functionAddress;
    call->ResumeAt = NULL;
    call->Prev = NULL;
    call->NumArguments = 0;
    call->NumRegisters = numRegisters;
    call->FrameStart = state->Registers[totemOperandType_LocalRegister] + (state->CallStack ? state->CallStack->NumRegisters : 0);
    
    // reset registers to be used
    memset(call->FrameStart, 0, numRegisters * sizeof(totemRegister));
    
    if(state->CurrentInstruction)
    {
        totemOperationType type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
        
        if(type == totemOperationType_FunctionArg)
        {
            totemOperandXUnsigned numArgs = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*state->CurrentInstruction);
            
            if(numArgs > call->NumRegisters)
            {
                return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
            }
            
            do
            {
                totemInstruction_Print(stdout, *state->CurrentInstruction);
                totemRegister *argument = TOTEM_GET_OPERANDA_REGISTER(state, (*state->CurrentInstruction));
                
                TOTEM_REGISTER_ASSIGN_GENERIC(&call->FrameStart[call->NumArguments], argument);
                
                state->CurrentInstruction++;
                call->NumArguments++;
                type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
            }
            while(type == totemOperationType_FunctionArg);
        }
    }
    
    state->UsedLocalRegisters += numRegisters;
    state->Registers[totemOperandType_LocalRegister] = call->FrameStart;
    state->Registers[totemOperandType_GlobalRegister] = (totemRegister*)actor->GlobalRegisters.Data;
    
    call->ResumeAt = state->CurrentInstruction;
    
    if(state->CallStack)
    {
        call->Prev = state->CallStack;
    }
    
    state->CallStack = call;
    
    return totemExecStatus_Continue;
}

void totemExecState_PopFunctionCall(totemExecState *state)
{
    if(state->CallStack)
    {
        // clean up any remaining arrays
        for(size_t i = 0; i < state->CallStack->NumRegisters; i++)
        {
            totemRegister *reg = &state->Registers[totemOperandType_LocalRegister][i];
            
            if(reg->DataType == totemDataType_Array)
            {
                totemRuntimeArrayHeader_DefRefCount(reg->Value.Array);
            }
        }
        
        state->CurrentInstruction = state->CallStack->ResumeAt;
        state->UsedLocalRegisters -= state->CallStack->NumRegisters;
        state->Registers[totemOperandType_LocalRegister] = state->CallStack->PreviousFrameStart;
        state->Registers[totemOperandType_GlobalRegister] = (totemRegister*)state->CallStack->CurrentActor->GlobalRegisters.Data;
        
        totemFunctionCall *call = state->CallStack;
        totemFunctionCall *prev = call->Prev;
        totemRuntime_FreeFunctionCall(state->Runtime, call);
        state->CallStack = prev;
    }
}

totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionAddress, totemRegister *returnRegister)
{
    totemScript *script = totemMemoryBuffer_Get(&state->Runtime->Scripts, actor->ScriptHandle);
    if(script == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
    }
    
    totemScriptFunction *function = totemMemoryBuffer_Get(&script->Functions, functionAddress);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Script, functionAddress, actor, returnRegister, function->RegistersNeeded);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    state->CurrentInstruction = totemMemoryBuffer_Get(&script->Instructions, function->InstructionsStart);
    if(state->CurrentInstruction == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    do
    {
        status = totemExecState_ExecInstruction(state);
    }
    while(status == totemExecStatus_Continue);
    
    totemExecState_PopFunctionCall(state);
    
    if(status == totemExecStatus_Return)
    {
        if(state->CallStack)
        {
            return totemExecStatus_Continue;
        }
    }
    
    return status;
}

totemExecStatus totemExecState_ExecNative(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionHandle, totemRegister *returnRegister)
{
    totemNativeFunction *function = totemMemoryBuffer_Get(&state->Runtime->NativeFunctions, functionHandle);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
    }
    
    // num args
    uint8_t numArgs = 0;
    if(TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction) == totemOperationType_FunctionArg)
    {
        numArgs = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*state->CurrentInstruction);
    }
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Native, functionHandle, actor, returnRegister, numArgs);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    status = (*function)(state);
    totemExecState_PopFunctionCall(state);
    return status;
}

totemExecStatus totemExecState_ExecInstruction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    
    totemOperationType op = TOTEM_INSTRUCTION_GET_OP(instruction);
    totemExecStatus status = totemExecStatus_Continue;
    
    totemInstruction_Print(stdout, instruction);
    
    switch(op)
    {
        case totemOperationType_None:
            state->CurrentInstruction++;
            status = totemExecStatus_Continue;
            break;
            
        case totemOperationType_Move:
            status = totemExecState_ExecMove(state);
            break;
            
        case totemOperationType_Add:
            status = totemExecState_ExecAdd(state);
            break;
            
        case totemOperationType_Subtract:
            status = totemExecState_ExecSubtract(state);
            break;
            
        case totemOperationType_Multiply:
            status = totemExecState_ExecMultiply(state);
            break;
            
        case totemOperationType_Divide:
            status = totemExecState_ExecDivide(state);
            break;
            
        case totemOperationType_Power:
            status = totemExecState_ExecPower(state);
            break;
            
        case totemOperationType_Equals:
            status = totemExecState_ExecEquals(state);
            break;
            
        case totemOperationType_NotEquals:
            status = totemExecState_ExecNotEquals(state);
            break;
            
        case totemOperationType_LessThan:
            status = totemExecState_ExecLessThan(state);
            break;
            
        case totemOperationType_LessThanEquals:
            status = totemExecState_ExecLessThanEquals(state);
            break;
            
        case totemOperationType_MoreThan:
            status = totemExecState_ExecMoreThan(state);
            break;
            
        case totemOperationType_MoreThanEquals:
            status = totemExecState_ExecMoreThanEquals(state);
            break;
            
        case totemOperationType_ConditionalGoto:
            status = totemExecState_ExecConditionalGoto(state);
            break;
            
        case totemOperationType_Goto:
            status = totemExecState_ExecGoto(state);
            break;
            
        case totemOperationType_NativeFunction:
            status = totemExecState_ExecNativeFunction(state);
            break;
            
        case totemOperationType_ScriptFunction:
            status = totemExecState_ExecScriptFunction(state);
            break;
            
        case totemOperationType_Return:
            status = totemExecState_ExecReturn(state);
            break;
            
        case totemOperationType_NewArray:
            status = totemExecState_ExecNewArray(state);
            break;
            
        case totemOperationType_ArrayGet:
            status = totemExecState_ExecArrayGet(state);
            break;
            
        case totemOperationType_ArraySet:
            status = totemExecState_ExecArraySet(state);
            break;
            
        case totemOperationType_MoveToGlobal:
            status = totemExecState_ExecMoveToGlobal(state);
            break;
            
        case totemOperationType_MoveToLocal:
            status = totemExecState_ExecMoveToLocal(state);
            break;
            
        case totemOperationType_Is:
            status = totemExecState_ExecIs(state);
            break;
            
        case totemOperationType_As:
            status = totemExecState_ExecAs(state);
            break;
            
        case totemOperationType_FunctionPointer:
            status = totemExecState_ExecFunctionPointer(state);
            break;
            
        case totemOperationType_Assert:
            status = totemExecState_ExecAssert(state);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
    }
    
    // debug
    /*
     totemInstructionType type = totemOperationType_GetInstructionType(op);
     switch(type)
     {
     case totemInstructionType_Abc:
     {
     totemRegister *a = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
     totemRegister *b = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
     totemRegister *c = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
     totemRegister_Print(stdout, a);
     totemRegister_Print(stdout, b);
     totemRegister_Print(stdout, c);
     break;
     }
     
     case totemInstructionType_Abx:
     {
     totemRegister *a = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
     totemRegister_Print(stdout, a);
     break;
     }
     
     case totemInstructionType_Axx:
     break;
     }*/
    
    return status;
}

totemExecStatus totemExecState_ExecAssert(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    
    if(dst->Value.Data != 0)
    {
        state->CurrentInstruction++;
        return totemExecStatus_Continue;
    }
    else
    {
        return totemExecStatus_Break(totemExecStatus_FailedAssertion);
    }
}

totemExecStatus totemExecState_ExecFunctionPointer(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    if(src->DataType != totemDataType_Function)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemExecStatus status = totemExecStatus_Continue;
    
    switch(src->Value.FunctionPointer.Type)
    {
        case totemFunctionType_Native:
            state->CurrentInstruction++;
            status = totemExecState_ExecNative(state, state->CallStack->CurrentActor, src->Value.FunctionPointer.Address, dst);
            break;
            
        case totemFunctionType_Script:
            state->CurrentInstruction++;
            status = totemExecState_Exec(state, state->CallStack->CurrentActor, src->Value.FunctionPointer.Address, dst);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
    }
    
    return status;
}

totemExecStatus totemExecState_ExecAs(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *typeSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemDataType type = typeSrc->Value.DataType;
    
    if(typeSrc->DataType != totemDataType_Type)
    {
        type = typeSrc->DataType;
    }
    
    
    
    switch(TOTEM_TYPEPAIR(src->DataType, type))
    {
        /*
         * int
         */
            
        // int as int
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
        // int as float
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(dst, (totemFloat)src->Value.Int);
            break;
            
        // int as new array
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(1);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            TOTEM_REGISTER_ASSIGN_INT(&regs[0], src->Value.Int);
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
        // int as string
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_String):
        {
            totemInternedStringHeader *str = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_IntToString(state, src->Value.Int, &str));
            TOTEM_REGISTER_ASSIGN_STRING(dst, str);
            break;
        }
        
        // int as null (just null)
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Null):
            TOTEM_REGISTER_ASSIGN_NULL(dst);
            break;
            
        // int as type
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemDataType_Int);
            break;
            
        /*
         * floats
         */
            
        // float as float
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
        // float as int
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(dst, (totemInt)src->Value.Float);
            break;
            
        // float as new array
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(1);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            TOTEM_REGISTER_ASSIGN_FLOAT(&regs[0], src->Value.Float);
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
        // float as string
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_String):
        {
            totemInternedStringHeader *str = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_FloatToString(state, src->Value.Float, &str));
            TOTEM_REGISTER_ASSIGN_STRING(dst, str);
            break;
        }
            
        // float as null (just null)
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Null):
            TOTEM_REGISTER_ASSIGN_NULL(dst);
            break;
            
        // float as type
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemDataType_Float);
            break;
            
        /*
         * arrays
         */
            
        // array as new array
        case TOTEM_TYPEPAIR(totemDataType_Array, totemDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(src->Value.Array->NumRegisters);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            totemRegister *srcRegs = totemRuntimeArrayHeader_GetRegisters(src->Value.Array);
            
            for(uint32_t i = 0; i < src->Value.Array->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(&regs[i], &srcRegs[i]);
            }
            
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
        // array as int (length)
        case TOTEM_TYPEPAIR(totemDataType_Array, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(dst, (totemInt)src->Value.Array->NumRegisters);
            break;
            
        // array as float (length)
        case TOTEM_TYPEPAIR(totemDataType_Array, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(dst, (totemFloat)src->Value.Array->NumRegisters);
            break;
            
        // array as string (implode)
        case TOTEM_TYPEPAIR(totemDataType_Array, totemDataType_String):
        {
            totemInternedStringHeader *str = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_ArrayToString(state, src->Value.Array, &str));
            TOTEM_REGISTER_ASSIGN_STRING(dst, str);
            break;
        }
            
        // array as null (just null)
        case TOTEM_TYPEPAIR(totemDataType_Array, totemDataType_Null):
            TOTEM_REGISTER_ASSIGN_NULL(dst);
            break;
            
        // array as type
        case TOTEM_TYPEPAIR(totemDataType_Array, totemDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemDataType_Array);
            break;
            
        /*
         * types
         */
        // type as array
        case TOTEM_TYPEPAIR(totemDataType_Type, totemDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(1);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            TOTEM_REGISTER_ASSIGN_TYPE(&regs[0], src->Value.DataType);
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
        // type as type
        case TOTEM_TYPEPAIR(totemDataType_Type, totemDataType_Type):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
        // type as string (type name)
        case TOTEM_TYPEPAIR(totemDataType_Type, totemDataType_String):
        {
            totemInternedStringHeader *str = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_TypeToString(state, src->Value.DataType, &str));
            TOTEM_REGISTER_ASSIGN_STRING(dst, str);
            break;
        }
            
        // type as null (just null)
        case TOTEM_TYPEPAIR(totemDataType_Type, totemDataType_Null):
            TOTEM_REGISTER_ASSIGN_NULL(dst);
            break;
            
        /*
         * strings
         */
        
        // string as int (attempt atoi)
        case TOTEM_TYPEPAIR(totemDataType_String, totemDataType_Int):
        {
            const char *str = totemInternedStringHeader_GetString(src->Value.InternedString);
            totemInt val = strtoll(str, NULL, 10);
            TOTEM_REGISTER_ASSIGN_INT(dst, val);
            break;
        }
            
        // string as float (attempt atof)
        case TOTEM_TYPEPAIR(totemDataType_String, totemDataType_Float):
        {
            const char *str = totemInternedStringHeader_GetString(src->Value.InternedString);
            totemFloat val = strtod(str, NULL);
            TOTEM_REGISTER_ASSIGN_FLOAT(dst, val);
            break;
        }
            
        // explode string into array of strings, one for each char
        case TOTEM_TYPEPAIR(totemDataType_String, totemDataType_Array):
        {
            const char *str = totemInternedStringHeader_GetString(src->Value.InternedString);
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(src->Value.InternedString->Length);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            for(size_t i = 0; i < arr->NumRegisters; i++)
            {
                totemString newStr;
                newStr.Length = 1;
                newStr.Value = &str[i];
                
                totemInternedStringHeader *newInternedStr = totemRuntime_InternString(state->Runtime, &newStr);
                if(!newInternedStr)
                {
                    return totemExecStatus_Break(totemExecStatus_OutOfMemory);
                }
                
                TOTEM_REGISTER_ASSIGN_STRING(&regs[i], newInternedStr);
            }
            
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
        // string as type
        case TOTEM_TYPEPAIR(totemDataType_String, totemDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemDataType_String);
            break;
            
        // string as string
        case TOTEM_TYPEPAIR(totemDataType_String, totemDataType_String):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
        // string as null
        case TOTEM_TYPEPAIR(totemDataType_String, totemDataType_Null):
            TOTEM_REGISTER_ASSIGN_NULL(dst);
            break;
            
        // lookup function pointer by name
        case TOTEM_TYPEPAIR(totemDataType_String, totemDataType_Function):
        {
            totemString lookup;
            lookup.Value = totemInternedStringHeader_GetString(src->Value.InternedString);
            lookup.Length = src->Value.InternedString->Length;
            
            totemOperandXUnsigned addr = 0;
            if(totemRuntime_GetNativeFunctionAddress(state->Runtime, &lookup, &addr))
            {
                TOTEM_REGISTER_ASSIGN_FUNC(dst, addr, totemFunctionType_Native);
            }
            else
            {
                totemScript *script = totemMemoryBuffer_Get(&state->Runtime->Scripts, state->CallStack->CurrentActor->ScriptHandle);
                if(!script)
                {
                    return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
                }
                
                totemHashMapEntry *entry = totemHashMap_Find(&script->FunctionNameLookup, lookup.Value, lookup.Length);
                if(entry)
                {
                    addr = (totemOperandXUnsigned)entry->Value;
                    TOTEM_REGISTER_ASSIGN_FUNC(dst, addr, totemFunctionType_Script);
                }
                else
                {
                    TOTEM_REGISTER_ASSIGN_NULL(dst);
                }
            }
            
            break;
        }
            
        /*
         * null
         */
            
        // null as int
        case TOTEM_TYPEPAIR(totemDataType_Null, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(dst, 0);
            break;
            
        // null as float
        case TOTEM_TYPEPAIR(totemDataType_Null, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(dst, 0);
            break;
            
        // null as array
        case TOTEM_TYPEPAIR(totemDataType_Null, totemDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(1);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            TOTEM_REGISTER_ASSIGN_NULL(&regs[0]);
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
        // null as type
        case TOTEM_TYPEPAIR(totemDataType_Null, totemDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemDataType_Null);
            break;
            
        // null string
        case TOTEM_TYPEPAIR(totemDataType_Null, totemDataType_String):
        {
            totemInternedStringHeader *str = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_NullString(state, &str));
            TOTEM_REGISTER_ASSIGN_STRING(dst, str);
            break;
        }
            
        // null as null
        case TOTEM_TYPEPAIR(totemDataType_Null, totemDataType_Null):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
        /*
         * functions
         */
        // function as type
        case TOTEM_TYPEPAIR(totemDataType_Function, totemDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemDataType_Function);
            break;
            
        // function as string
        case TOTEM_TYPEPAIR(totemDataType_Function, totemDataType_String):
        {
            totemInternedStringHeader *str = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_FunctionPointerToString(state, &src->Value.FunctionPointer, &str));
            TOTEM_REGISTER_ASSIGN_STRING(dst, str);
            break;
        }
            
        // function as null
        case TOTEM_TYPEPAIR(totemDataType_Function, totemDataType_Null):
            TOTEM_REGISTER_ASSIGN_NULL(dst);
            break;
            
        // function as function
        case TOTEM_TYPEPAIR(totemDataType_Function, totemDataType_Function):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecIs(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *typeSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemDataType type = typeSrc->Value.DataType;
    
    if(typeSrc->DataType != totemDataType_Type)
    {
        type = typeSrc->DataType;
    }
    
    TOTEM_REGISTER_ASSIGN_INT(dst, src->DataType == type);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMove(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    TOTEM_REGISTER_ASSIGN_GENERIC(destination, source);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoveToGlobal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *src = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    
    TOTEM_REGISTER_ASSIGN_GENERIC(globalReg, src);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoveToLocal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    
    TOTEM_REGISTER_ASSIGN_GENERIC(dst, globalReg);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecAdd(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int + source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) + source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float + source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float + ((totemFloat)source2->Value.Int));
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecSubtract(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int - source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float - source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float - ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) - source2->Value.Float);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMultiply(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int * source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float * source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float * ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) * source2->Value.Float);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecDivide(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    if(source2->Value.Int == 0)
    {
        return totemExecStatus_Break(totemExecStatus_DivideByZero);
    }
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int / source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float / source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float / ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) / source2->Value.Float);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecPower(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow((totemFloat)source1->Value.Int, (totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow(source1->Value.Float, source2->Value.Float));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow(source1->Value.Float, (totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow((totemFloat)source1->Value.Int, source2->Value.Float));
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Data == source2->Value.Data && source1->DataType == source2->DataType);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecNotEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Data != source2->Value.Data || source1->DataType != source2->DataType);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int < source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float < source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float < ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) < source2->Value.Float);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int <= source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float <= source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float <= ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) <= source2->Value.Float);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int > source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float > source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float > ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) > source2->Value.Float);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int >= source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float >= source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float >= ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) >= source2->Value.Float);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecGoto(totemExecState *state)
{
    totemOperandXSigned offset = TOTEM_INSTRUCTION_GET_AX_SIGNED(*state->CurrentInstruction);
    
    state->CurrentInstruction += offset;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecConditionalGoto(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    
    totemOperandXSigned offset = TOTEM_INSTRUCTION_GET_BX_SIGNED(instruction);
    
    if(source->Value.Data == 0)
    {
        state->CurrentInstruction += offset;
    }
    else
    {
        state->CurrentInstruction++;
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecScriptFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned functionIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    state->CurrentInstruction++;
    return totemExecState_Exec(state, state->CallStack->CurrentActor, functionIndex, destination);
}

totemExecStatus totemExecState_ExecNativeFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *returnRegister = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned functionHandle = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    state->CurrentInstruction++;
    return totemExecState_ExecNative(state, state->CallStack->CurrentActor, functionHandle, returnRegister);
}

totemExecStatus totemExecState_ExecReturn(totemExecState *state)
{
    totemFunctionCall *call = state->CallStack;
    
    totemInstruction instruction = *state->CurrentInstruction;
    totemOperandXUnsigned option = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    if(option == totemReturnOption_Register)
    {
        totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
        TOTEM_REGISTER_ASSIGN_GENERIC(call->ReturnRegister, source);
    }
    
    return totemExecStatus_Return;
}

totemExecStatus totemExecState_ExecNewArray(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    uint32_t numRegisters = (uint32_t)indexSrc->Value.Int;
    
    if(numRegisters == 0 || indexSrc->Value.Int == UINT32_MAX)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(numRegisters);
    if(!arr)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecArrayGet(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    if(src->DataType != totemDataType_Array)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemInt index = indexSrc->Value.Int;
    
    if(index < 0 || index >= UINT32_MAX || index >= src->Value.Array->NumRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(src->Value.Array);
    TOTEM_REGISTER_ASSIGN_GENERIC(dst, &regs[index]);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecArraySet(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    if(dst->DataType != totemDataType_Array)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemInt index = indexSrc->Value.Int;
    
    if(index < 0 || index >= UINT32_MAX || index >= dst->Value.Array->NumRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(dst->Value.Array);
    TOTEM_REGISTER_ASSIGN_GENERIC(&regs[index], src);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

const char *totemExecStatus_Describe(totemExecStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemExecStatus_FailedAssertion);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Continue);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InstructionOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_NativeFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RegisterOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Return);
            TOTEM_STRINGIFY_CASE(totemExecStatus_ScriptFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_ScriptNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnexpectedDataType);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnrecognisedOperation);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Stop);
            TOTEM_STRINGIFY_CASE(totemExecStatus_IndexOutOfBounds);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RefCountOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_DivideByZero);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InternalBufferOverrun);
    }
    
    return "UNKNOWN";
}

const char *totemLinkStatus_Describe(totemLinkStatus status)
{
    switch(status)
    {
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