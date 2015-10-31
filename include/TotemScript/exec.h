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

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum
    {
        totemFunctionType_Script,
        totemFunctionType_Native
    }
    totemFunctionType;

    enum
    {
        totemInstructionReturnType_Stop = 0,
        totemInstructionReturnType_Continue = 1,
        totemInstructionReturnType_Return = 2
    };
    typedef uint8_t totemInstructionReturnType;
        
    #define TOTEM_STATUS totemInstructionReturnType
    #define TOTEM_CONTINUE totemInstructionReturnType_Continue
    #define TOTEM_STOP totemInstructionReturnType_Stop
    #define TOTEM_RETURN totemInstructionReturnType_Return

    typedef struct
    {
        totemInstruction *Instructions;
        size_t NumInstructions;
        size_t *FunctionAddresses;
        size_t NumFunctions;
        char *GlobalData;
        size_t GlobalDataLength;
        totemRegister *GlobalRegisters;
        size_t NumGlobalRegisters;
        totemString Name;
    }
    totemScript;
    
    typedef struct
    {
        size_t ScriptHandle;
        char *GlobalData;
        size_t GlobalDataLength;
    }
    totemActor;
    
    typedef struct totemFunctionCall
    {
        struct totemFunctionCall *Prev;
        totemActor *Actor;
        totemRegister *Arguments;
        totemRegister *ReturnRegister;
        totemRegister *RegisterFrameStart;
        totemInstruction *CurrentInstruction;
        size_t FunctionHandle;
        totemFunctionType Type;
        uint8_t NumArguments;
    }
    totemFunctionCall;

    typedef struct
    {
        totemFunctionCall *CallStack;
        totemRegister *Registers;
        size_t MaxRegisters;
    }
    totemExecState;

    typedef TOTEM_STATUS(*totemNativeFunction)(totemExecState*);

    typedef struct
    {
        size_t ScriptHandle;
        size_t FunctionIndex;
    }
    totemScriptFunction;
    
    typedef struct
    {
        totemNativeFunction *NativeFunctions;
        size_t NumNativeFunctions;
        totemScriptFunction *ScriptFunctions;
        size_t NumScriptFunctions;
        totemScript *Scripts;
        size_t NumScripts;
    }
    totemRuntime;

    totemBool totemRuntime_RegisterScript(totemRuntime *runtime, totemScript *script, size_t *indexOut);
    totemBool totemRuntime_GetScriptAddress(totemRuntime *runtime, totemString name, size_t *addressOut);
    totemBool totemRuntime_GetScriptFunctionAddress(totemRuntime *runtime, totemString name, size_t *addressOut);
    totemBool totemRuntime_GetScriptFunction(totemRuntime *runtime, size_t address, totemScriptFunction **scriptOut);
            
    totemBool totemRuntime_RegisterNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString name, size_t *addressOut);
    totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString name, size_t *addressOut);
    totemBool totemRuntime_GetNativeFunction(totemRuntime *runtime, size_t address, totemNativeFunction *funcOut);

    totemBool totemRuntime_CreateState(totemRuntime *runtime, size_t scriptHandle, totemExecState *state);

    /**
     * Execute bytecode
     */
    TOTEM_STATUS totemExecState_Exec(totemExecState *state, totemRuntime *runtime, size_t functionHandle, totemRegister *returnRegister);
    TOTEM_STATUS totemExecState_ExecInstruction(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecMove(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecAdd(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecSubtract(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecMultiply(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecDivide(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecPower(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecEquals(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecNotEquals(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecLessThan(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecLessThanEquals(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecMoreThan(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecMoreThanEquals(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecReturn(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecGoto(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecConditionalGoto(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecNativeFunction(totemExecState *state);
    TOTEM_STATUS totemExecState_ExecScriptFunction(totemExecState *state);
            
#ifdef __cplusplus
}
#endif

#endif
