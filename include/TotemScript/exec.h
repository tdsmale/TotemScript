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
#include <setjmp.h>
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
        totemExecStatus_InvalidKey,
        totemExecStatus_UnrecognisedOperation,
        totemExecStatus_RegisterOverflow,
        totemExecStatus_InstructionOverflow,
        totemExecStatus_OutOfMemory,
        totemExecStatus_IndexOutOfBounds,
        totemExecStatus_RefCountOverflow,
        totemExecStatus_DivideByZero,
        totemExecStatus_InternalBufferOverrun
    }
    totemExecStatus;
    
    totemExecStatus totemExecStatus_Break(totemExecStatus status);
    const char *totemExecStatus_Describe(totemExecStatus status);
    
    typedef enum
    {
        totemInterpreterStatus_Success,
        totemInterpreterStatus_FileError,
        totemInterpreterStatus_LexError,
        totemInterpreterStatus_ParseError,
        totemInterpreterStatus_EvalError,
        totemInterpreterStatus_LinkError
    }
    totemInterpreterStatus;
    
    typedef enum
    {
        totemLinkStatus_Success,
        totemLinkStatus_OutOfMemory,
        totemLinkStatus_FunctionAlreadyDeclared,
        totemLinkStatus_FunctionNotDeclared,
        totemLinkStatus_InvalidNativeFunctionAddress,
        totemLinkStatus_InvalidNativeFunctionName,
        totemLinkStatus_TooManyNativeFunctions,
        totemLinkStatus_UnexpectedValueType
    }
    totemLinkStatus;
    
    const char *totemLinkStatus_Describe(totemLinkStatus status);
    
    typedef struct
    {
        totemHashMap FunctionNameLookup;
        totemMemoryBuffer GlobalRegisters;
        totemMemoryBuffer Functions;
        totemMemoryBuffer FunctionNames;
        totemMemoryBuffer Instructions;
    }
    totemScript;
    
    typedef struct
    {
        totemMemoryBuffer GlobalRegisters;
        totemScript *Script;
    }
    totemActor;
    
    typedef struct
    {
        size_t InstructionsStart;
        uint8_t RegistersNeeded;
    }
    totemScriptFunction;
    
    typedef enum
    {
        totemFunctionCallFlag_None = 0,
        totemFunctionCallFlag_FreeStack = 1,
        totemFunctionCallFlag_IsCoroutine = 2
    }
    totemFunctionCallFlag;
    
    typedef struct totemFunctionCall
    {
        int *Break;
        totemExecStatus BreakStatus;
        struct totemFunctionCall *Prev;
        totemActor *CurrentActor;
        totemRegister *ReturnRegister;
        totemRegister *PreviousFrameStart;
        totemRegister *FrameStart;
        totemInstruction *ResumeAt;
        totemOperandXUnsigned FunctionHandle;
        totemFunctionType Type;
        totemFunctionCallFlag Flags;
        uint8_t NumRegisters;
        uint8_t NumArguments;
    }
    totemFunctionCall;
    
    typedef struct
    {
        totemMemoryBuffer NativeFunctions;
        totemMemoryBuffer NativeFunctionNames;
        totemHashMap NativeFunctionsLookup;
        totemHashMap InternedStrings;
        totemLock InternedStringsLock;
    }
    totemRuntime;
    
    typedef struct totemExecState
    {
        totemFunctionCall *CallStack;
        totemInstruction *CurrentInstruction;
        totemRuntime *Runtime;
        totemRegister *Registers[2];
        size_t MaxLocalRegisters;
        size_t UsedLocalRegisters;
        totemFunctionCall *FunctionCallFreeList;
    }
    totemExecState;
    
    typedef struct
    {
        size_t ErrorLine;
        size_t ErrorChar;
        size_t ErrorLength;
        totemInterpreterStatus Status;
        union
        {
            totemLoadScriptStatus FileStatus;
            totemLexStatus LexStatus;
            totemParseStatus ParseStatus;
            totemEvalStatus EvalStatus;
            totemLinkStatus LinkStatus;
        };
    }
    totemInterpreterResult;
    
    typedef struct
    {
        totemScriptFile Script;
        totemTokenList TokenList;
        totemParseTree ParseTree;
        totemBuildPrototype Build;
        totemInterpreterResult Result;
    }
    totemInterpreter;
    
    typedef totemExecStatus(*totemNativeFunction)(totemExecState*);
    
    void totemActor_Init(totemActor *actor);
    void totemActor_Reset(totemActor *actor);
    void totemActor_Cleanup(totemActor *actor);
    
    void totemScript_Init(totemScript *script);
    void totemScript_Reset(totemScript *script);
    void totemScript_Cleanup(totemScript *script);
    totemLinkStatus totemScript_LinkActor(totemScript *script, totemActor *actor);
    
    void totemRuntime_Init(totemRuntime *runtime);
    void totemRuntime_Reset(totemRuntime *runtime);
    void totemRuntime_Cleanup(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters);
    totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *scriptOut);
    totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString *name, totemOperandXUnsigned *addressOut);
    totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRegisterValue *valOut, totemPrivateDataType *typeOut);
    totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, totemOperandXUnsigned *addressOut);
    
    enum
    {
        totemGCObjectType_Array,
        totemGCObjectType_Coroutine,
        totemGCObjectType_Object,
        totemGCObjectType_Userdata
    };
    typedef uint8_t totemGCObjectType;
    const char *totemGCObjectType_Describe(totemGCObjectType);
    
    typedef struct
    {
        uint32_t NumRegisters;
        totemRegister Registers[1];
    }
    totemArray;
    
    typedef struct
    {
        totemMemoryBuffer Registers;
        totemHashMap Lookup;
    }
    totemObject;
    
    struct totemUserdata;
    typedef void(*totemUserdataDestructor)(totemExecState*, struct totemUserdata*);
    
    typedef struct totemUserdata
    {
        uint64_t Data;
        totemUserdataDestructor Destructor;
    }
    totemUserdata;
    
    typedef struct totemGCObject
    {
        union
        {
            totemArray *Array;
            totemFunctionCall *Coroutine;
            totemObject *Object;
            totemUserdata *Userdata;
        };
        volatile int64_t RefCount;
        int64_t CycleDetectCount;
        totemGCObjectType Type;
    }
    totemGCObject;
    
    totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type);
    void totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *gc);
    totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemOperandXUnsigned functionAddress, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateObject(totemExecState *state, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateArray(totemExecState *state, uint32_t numRegisters, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateUserdata(totemExecState *state, uint64_t data, totemUserdataDestructor destructor, totemGCObject **gcOut);
    totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, uint32_t numRegisters, totemGCObject **objOut);
    totemExecStatus totemExecState_IncRefCount(totemExecState *state, totemGCObject *gc);
    void totemExecState_DecRefCount(totemExecState *state, totemGCObject *gc);
    void totemExecState_DestroyArray(totemExecState *state, totemArray *arr);
    void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co);
    void totemExecState_DestroyObject(totemExecState *state, totemObject *obj);
    void totemExecState_DestroyUserdata(totemExecState *state, totemUserdata *obj);
    
    totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRegister *strOut);
    
    /**
     * Execute bytecode
     */
    void totemExecState_Init(totemExecState *state);
    void totemExecState_Cleanup(totemExecState *state);
    totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionAddress, totemRegister *returnRegister);
    totemExecStatus totemExecState_ExecNative(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionHandle, totemRegister *returnRegister);
    void totemExecState_ExecMove(totemExecState *state);
    void totemExecState_ExecMoveToGlobal(totemExecState *state);
    void totemExecState_ExecMoveToLocal(totemExecState *state);
    void totemExecState_ExecAdd(totemExecState *state);
    void totemExecState_ExecSubtract(totemExecState *state);
    void totemExecState_ExecMultiply(totemExecState *state);
    void totemExecState_ExecDivide(totemExecState *state);
    void totemExecState_ExecPower(totemExecState *state);
    void totemExecState_ExecEquals(totemExecState *state);
    void totemExecState_ExecNotEquals(totemExecState *state);
    void totemExecState_ExecLessThan(totemExecState *state);
    void totemExecState_ExecLessThanEquals(totemExecState *state);
    void totemExecState_ExecMoreThan(totemExecState *state);
    void totemExecState_ExecMoreThanEquals(totemExecState *state);
    void totemExecState_ExecReturn(totemExecState *state);
    void totemExecState_ExecGoto(totemExecState *state);
    void totemExecState_ExecConditionalGoto(totemExecState *state);
    void totemExecState_ExecNativeFunction(totemExecState *state);
    void totemExecState_ExecScriptFunction(totemExecState *state);
    void totemExecState_ExecNewArray(totemExecState *state);
    void totemExecState_ExecNewObject(totemExecState *state);
    void totemExecState_ExecArrayGet(totemExecState *state);
    void totemExecState_ExecArraySet(totemExecState *state);
    void totemExecState_ExecIs(totemExecState *state);
    void totemExecState_ExecAs(totemExecState *state);
    void totemExecState_ExecFunctionPointer(totemExecState *state);
    
    totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state);
    void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call);
    
    void totemExecState_CleanupRegisterList(totemExecState *state, totemRegister *regs, uint32_t num);
    
    void totemRegister_Print(FILE *file, totemRegister *reg);
    void totemRegister_PrintList(FILE *file, totemRegister *regs, size_t numRegisters);
    
    totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut);
    totemExecStatus totemExecState_EmptyString(totemExecState *state, totemRegister *strOut);
    totemExecStatus totemExecState_IntToString(totemExecState *state, totemInt val, totemRegister *strOut);
    totemExecStatus totemExecState_FloatToString(totemExecState *state, totemFloat val, totemRegister *strOut);
    totemExecStatus totemExecState_TypeToString(totemExecState *state, totemPublicDataType type, totemRegister *strOut);
    totemExecStatus totemExecState_FunctionPointerToString(totemExecState *state, totemFunctionPointer *ptr, totemRegister *strOut);
    totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemArray *arr, totemRegister *strOut);
    totemExecStatus totemExecState_StringToFunctionPointer(totemExecState *state, totemRegister *src, totemRegister *dst);
    
    totemExecStatus totemExecState_Assign(totemExecState *state, totemRegister *dst, totemRegister *src);
    void totemExecState_AssignNewInt(totemExecState *state, totemRegister *dst, totemInt newVal);
    void totemExecState_AssignNewFloat(totemExecState *state, totemRegister *dst, totemFloat newVal);
    void totemExecState_AssignNewType(totemExecState *state, totemRegister *dst, totemPublicDataType newVal);
    void totemExecState_AssignNewInternedString(totemExecState *state, totemRegister *dst, totemInternedStringHeader *newVal);
    void totemExecState_AssignNewFunctionPointer(totemExecState *state, totemRegister *dst, totemOperandXUnsigned addr, totemFunctionType type);
    void totemExecState_AssignNewString(totemExecState *state, totemRegister *dst, totemRegister *src);
    void totemExecState_AssignNewArray(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewCoroutine(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewObject(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewUserdata(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    
#define TOTEM_REGISTER_ISGC(x) ((x)->DataType >= totemPrivateDataType_Array && (x)->DataType <= totemPrivateDataType_Userdata)
#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }
    
#ifdef __cplusplus
}
#endif

#endif
