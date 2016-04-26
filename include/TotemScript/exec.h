//
//  exec.h
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_EXEC_H
#define TOTEMSCRIPT_EXEC_H

#include <stdint.h>
#include <TotemScript/base.h>
#include <TotemScript/eval.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef enum
    {
        totemExecStatus_Continue,
        totemExecStatus_Return,
        totemExecStatus_Stop,
        totemExecStatus_ScriptNotFound,
        totemExecStatus_ScriptFunctionNotFound,
        totemExecStatus_NativeFunctionNotFound,
        totemExecStatus_UnexpectedDataType,
        totemExecStatus_UnrecognisedOperation,
        totemExecStatus_RegisterOverflow,
        totemExecStatus_InstructionOverflow,
        totemExecStatus_OutOfMemory,
        totemExecStatus_IndexOutOfBounds,
        totemExecStatus_RefCountOverflow
    }
    totemExecStatus;
    
    const char *totemExecStatus_Describe(totemExecStatus status);
    
    typedef enum
    {
        totemLinkStatus_Success,
        totemLinkStatus_OutOfMemory,
        totemLinkStatus_FunctionAlreadyDeclared,
        totemLinkStatus_FunctionNotDeclared,
        totemLinkStatus_InvalidNativeFunctionAddress,
        totemLinkStatus_InvalidNativeFunctionName
    }
    totemLinkStatus;
    
    const char *totemLinkStatus_Describe(totemLinkStatus status);
    
    typedef struct
    {
        totemHashMap FunctionNameLookup;
        totemMemoryBuffer GlobalRegisters;
        totemMemoryBuffer GlobalData;
        totemMemoryBuffer Functions;
        totemMemoryBuffer Instructions;
    }
    totemScript;
    
    typedef struct
    {
        totemMemoryBuffer NativeFunctions;
        totemMemoryBuffer Scripts;
        
        totemHashMap ScriptLookup;
        totemHashMap NativeFunctionsLookup;
        totemHashMap InternedStrings;
        
        totemFunctionCall *FunctionCallFreeList;
    }
    totemRuntime;
    
    typedef struct
    {
        totemFunctionCall *CallStack;
        totemInstruction *CurrentInstruction;
        totemActor *CurrentActor;
        totemRuntime *Runtime;
        totemRegister *Registers[2];
        size_t MaxLocalRegisters;
        size_t UsedLocalRegisters;
    }
    totemExecState;
    
    typedef totemExecStatus(*totemNativeFunction)(totemExecState*);
    
    totemExecStatus totemActor_Init(totemActor *actor, totemRuntime *runtime, size_t scriptAddress);
    void totemActor_Cleanup(totemActor *actor);
    
    void totemRuntime_Init(totemRuntime *runtime);
    void totemRuntime_Reset(totemRuntime *runtime);
    void totemRuntime_Cleanup(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemString *name, size_t *addressOut);
    totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString *name, size_t *addressOut);
    totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, size_t *addressOut);
    totemInternedStringHeader *totemRuntime_InternString(totemRuntime *runtime, totemString *str);
    
    void totemRuntimeArray_DefRefCount(totemRuntimeArray *arr);
    totemExecStatus totemRuntimeArray_IncRefCount(totemRuntimeArray *arr);
    totemExecStatus totemRegister_Assign(totemRegister *dst, totemRegister *src);
    
    /**
     * Execute bytecode
     */
    totemBool totemExecState_Init(totemExecState *state, totemRuntime *runtime, size_t numRegisters);
    void totemExecState_Cleanup(totemExecState *state);
    totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, size_t functionAddress, totemRegister *returnRegister);
    totemExecStatus totemExecState_ExecInstruction(totemExecState *state);
    totemExecStatus totemExecState_ExecMove(totemExecState *state);
    totemExecStatus totemExecState_ExecMoveToGlobal(totemExecState *state);
    totemExecStatus totemExecState_ExecMoveToLocal(totemExecState *state);
    totemExecStatus totemExecState_ExecAdd(totemExecState *state);
    totemExecStatus totemExecState_ExecSubtract(totemExecState *state);
    totemExecStatus totemExecState_ExecMultiply(totemExecState *state);
    totemExecStatus totemExecState_ExecDivide(totemExecState *state);
    totemExecStatus totemExecState_ExecPower(totemExecState *state);
    totemExecStatus totemExecState_ExecEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecNotEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecLessThan(totemExecState *state);
    totemExecStatus totemExecState_ExecLessThanEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecMoreThan(totemExecState *state);
    totemExecStatus totemExecState_ExecMoreThanEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecReturn(totemExecState *state);
    totemExecStatus totemExecState_ExecGoto(totemExecState *state);
    totemExecStatus totemExecState_ExecConditionalGoto(totemExecState *state);
    totemExecStatus totemExecState_ExecNativeFunction(totemExecState *state);
    totemExecStatus totemExecState_ExecScriptFunction(totemExecState *state);
    totemExecStatus totemExecState_ExecNewArray(totemExecState *state);
    totemExecStatus totemExecState_ExecArrayGet(totemExecState *state);
    totemExecStatus totemExecState_ExecArraySet(totemExecState *state);
    
    void totemRegister_Print(FILE *file, totemRegister *reg);
    void totemRegister_PrintList(FILE *file, totemRegister *regs, size_t numRegisters);
    
#ifdef __cplusplus
}
#endif

#endif
