//
//  exec.h
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_EXEC_H
#define TOTEMSCRIPT_EXEC_H

#include <TotemScript/base.h>
#include <TotemScript/eval.h>
#include <stdint.h>
#include <setjmp.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef enum
    {
        totemExecStatus_Continue,
        totemExecStatus_Stop,
        totemExecStatus_ScriptNotFound,
        totemExecStatus_InstanceFunctionNotFound,
        totemExecStatus_NativeFunctionNotFound,
        totemExecStatus_UnexpectedDataType,
        totemExecStatus_InvalidKey,
        totemExecStatus_UnrecognisedOperation,
        totemExecStatus_InvalidDispatch,
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
        totemInterpreterStatus_EvalError
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
        totemMemoryBuffer LocalFunctions;
        totemMemoryBuffer GlobalRegisters;
        totemScript *Script;
    }
    totemInstance;
    
    typedef struct totemScriptFunction
    {
        totemInstruction *InstructionsStart;
        totemOperandXUnsigned Address;
        uint8_t RegistersNeeded;
    }
    totemScriptFunction;
    
    typedef struct
    {
        totemScriptFunction *Function;
        totemInstance *Instance;
    }
    totemInstanceFunction;
    
    struct totemExecState;
    typedef totemExecStatus(*totemNativeFunctionCb)(struct totemExecState*);
    
    typedef struct
    {
        totemNativeFunctionCb Callback;
        totemString Name;
    }
    totemNativeFunctionPrototype;
    
    typedef struct
    {
        totemNativeFunctionCb Callback;
        totemOperandXUnsigned Address;
    }
    totemNativeFunction;
    
    typedef enum
    {
        totemFunctionCallFlag_None = 0,
        totemFunctionCallFlag_FreeStack = 1,
        totemFunctionCallFlag_IsCoroutine = 2
    }
    totemFunctionCallFlag;
    
    enum
    {
        totemPrivateDataType_Null = 0,
        totemPrivateDataType_Int = 1,
        totemPrivateDataType_Float = 2,
        totemPrivateDataType_InternedString = 3,
        totemPrivateDataType_Type = 4,
        totemPrivateDataType_NativeFunction = 5,
        totemPrivateDataType_InstanceFunction = 6,
        totemPrivateDataType_MiniString = 7,
        totemPrivateDataType_Array = 8,
        totemPrivateDataType_Coroutine = 9,
        totemPrivateDataType_Object = 10,
        totemPrivateDataType_Userdata = 11,
        totemPrivateDataType_Boolean = 12,
        totemPrivateDataType_Max = 13
    };
    typedef uint8_t totemPrivateDataType;
    
    const char *totemPrivateDataType_Describe(totemPrivateDataType type);
#define TOTEM_TYPEPAIR(a, b) ((a << 8) | (b))
    
    totemPublicDataType totemPrivateDataType_ToPublic(totemPrivateDataType type);
    
    typedef union
    {
        totemFloat Float;
        totemInt Int;
        totemInternedStringHeader *InternedString;
        totemRuntimeMiniString MiniString;
        struct totemGCObject *GCObject;
        uint64_t Data;
        totemNativeFunction *NativeFunction;
        totemInstanceFunction *InstanceFunction;
        totemPublicDataType DataType;
    }
    totemRegisterValue;
    
    typedef struct totemRegister
    {
        totemRegisterValue Value;
        totemPrivateDataType DataType;
    }
    totemRegister;
    
    const char *totemRegister_GetStringValue(totemRegister *reg);
    totemStringLength totemRegister_GetStringLength(totemRegister *reg);
    totemHash totemRegister_GetStringHash(totemRegister *reg);
    
    typedef struct totemFunctionCall
    {
        struct totemFunctionCall *Prev;
        totemInstance *CurrentInstance;
        totemRegister *ReturnRegister;
        totemRegister *PreviousFrameStart;
        totemRegister *FrameStart;
        totemInstruction *ResumeAt;
        union
        {
            totemInstanceFunction *InstanceFunction;
            totemNativeFunction *NativeFunction;
            void *Function;
        };
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
    
    struct totemGCObject;
    
    typedef struct totemGCHeader
    {
        union
        {
            struct totemGCHeader *PrevHdr;
            struct totemGCObject *PrevObj;
        };
        
        union
        {
            struct totemGCHeader *NextHdr;
            struct totemGCObject *NextObj;
        };
    }
    totemGCHeader;
    
    enum
    {
        totemGCObjectType_Deleting = 0,
        totemGCObjectType_Array,
        totemGCObjectType_Coroutine,
        totemGCObjectType_Object,
        totemGCObjectType_Userdata,
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
    struct totemExecState;
    typedef void(*totemUserdataDestructor)(struct totemExecState*, struct totemUserdata*);
    
    typedef struct totemUserdata
    {
        void *Data;
        totemUserdataDestructor Destructor;
    }
    totemUserdata;
    
    /*
     assuming that ref counts will never overflow
     - it is effectively impossible for every addressable memory location to hold a gc object
     - by design, ref counts never go negative
     - therefore, values in the farthest upper range can be safely reserved for special use
     - if none of this holds, then all apologies
     */
    typedef size_t totemRefCount;
    
    typedef struct totemGCObject
    {
        totemGCHeader Header; // MUST be the first member - this struct should be able to masquerade as a totemGCHeader
        union
        {
            totemArray *Array;
            totemFunctionCall *Coroutine;
            totemObject *Object;
            totemUserdata *Userdata;
        };
        totemRefCount RefCount;
        totemRefCount CycleDetectCount;
        totemGCObjectType Type;
    }
    totemGCObject;
    
#ifdef __cplusplus
#define TOTEM_JMP_TYPE char
#define TOTEM_JMP_TRY(jmp) try
#define TOTEM_JMP_CATCH(jmp) catch (...)
#define TOTEM_JMP_THROW(jmp) throw jmp
#else
#define TOTEM_JMP_TYPE jmp_buf
#define TOTEM_JMP_TRY(jmp) if (!totem_setjmp(jmp))
#define TOTEM_JMP_CATCH(jmp) else
#define TOTEM_JMP_THROW(jmp) totem_longjmp(jmp)
#endif
    
    typedef struct totemJmpNode
    {
        TOTEM_JMP_TYPE Buffer;
        struct totemJmpNode *Prev;
        totemExecStatus Status;
    }
    totemJmpNode;
    
    typedef struct totemExecState
    {
        totemGCHeader GC;
        totemGCHeader GC2;
        totemGCObject *ArgV;
        totemJmpNode *JmpNode;
        totemFunctionCall *CallStack;
        totemFunctionCall *CallStackFreeList;
        totemGCObject *GCFreeList;
        totemRuntime *Runtime;
        totemRegister *LocalRegisters;
        totemRegister *GlobalRegisters;
        totemRegister *NextFreeRegister;
        size_t MaxLocalRegisters;
        size_t UsedLocalRegisters;
        size_t NumGC;
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
    
    void totemInterpreter_Init(totemInterpreter *interpreter);
    void totemInterpreter_Reset(totemInterpreter *interpreter);
    void totemInterpreter_Cleanup(totemInterpreter *interpreter);
    totemBool totemInterpreter_InterpretFile(totemInterpreter *interpreter, totemString *filename);
    totemBool totemInterpreter_InterpretString(totemInterpreter *interpreter, totemString *string);
    void totemInterpreter_PrintResult(FILE *target, totemInterpreter *interpreter);
    
    void totemInstance_Init(totemInstance *instance);
    void totemInstance_Reset(totemInstance *instance);
    void totemInstance_Cleanup(totemInstance *instance);
    
    void totemScript_Init(totemScript *script);
    void totemScript_Reset(totemScript *script);
    void totemScript_Cleanup(totemScript *script);
    totemBool totemScript_GetFunctionName(totemScript *script, totemOperandXUnsigned addr, totemRegisterValue *valOut, totemPrivateDataType *dataTypeOut);
    totemLinkStatus totemScript_LinkInstance(totemScript *script, totemInstance *instance);
    
    void totemRuntime_Init(totemRuntime *runtime);
    void totemRuntime_Reset(totemRuntime *runtime);
    void totemRuntime_Cleanup(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkStdLib(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters);
    totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *scriptOut);
    totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunctionCb func, totemString *name, totemOperandXUnsigned *addressOut);
    totemLinkStatus totemRuntime_LinkNativeFunctions(totemRuntime *runtime, totemNativeFunctionPrototype *funcs, totemOperandXUnsigned num);
    totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRegisterValue *valOut, totemPrivateDataType *typeOut);
    totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, totemOperandXUnsigned *addressOut);
    totemBool totemRuntime_GetNativeFunctionName(totemRuntime *runtime, totemOperandXUnsigned addr, totemRegisterValue *valOut, totemPrivateDataType *typeOut);
    
    void *totemExecState_Alloc(totemExecState *state, size_t size);
    totemExecStatus totemExecState_CreateSubroutine(totemExecState *state, uint8_t numRegisters, totemInstance *instance, totemRegister *returnReg, totemFunctionType funcType, void *function, totemFunctionCall **callOut);
    void totemExecState_PushRoutine(totemExecState *state, totemFunctionCall *call, totemInstruction *startAt);
    void totemExecState_PopRoutine(totemExecState *state);
    
    void totemGCHeader_Init(totemGCHeader *obj);
    totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type);
    totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj);
    void totemExecState_CleanupGCList(totemExecState *state, totemGCHeader *hdr);
    totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemInstanceFunction *function, totemGCObject **gcOut);
    totemExecStatus totemExecState_CreateObject(totemExecState *state, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateArray(totemExecState *state, uint32_t numRegisters, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateUserdata(totemExecState *state, void *data, totemUserdataDestructor destructor, totemGCObject **gcOut);
    totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, uint32_t numRegisters, totemGCObject **objOut);
    void totemExecState_IncRefCount(totemExecState *state, totemGCObject *gc);
    void totemExecState_DecRefCount(totemExecState *state, totemGCObject *gc);
    void totemExecState_DestroyArray(totemExecState *state, totemArray *arr);
    void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co);
    void totemExecState_DestroyObject(totemExecState *state, totemObject *obj);
    void totemExecState_DestroyUserdata(totemExecState *state, totemUserdata *obj);
    void totemExecState_CollectGarbage(totemExecState *state, totemBool full);
    
    /**
     * Execute bytecode
     */
    void totemExecState_Init(totemExecState *state);
    totemExecStatus totemExecState_SetArgV(totemExecState *state, const char **argv, uint32_t num);
    void totemExecState_Cleanup(totemExecState *state);
    totemExecStatus totemExecState_ProtectedExec(totemExecState *state, totemInstanceFunction *function, totemRegister *returnRegister);
    totemExecStatus totemExecState_UnsafeExec(totemExecState *state, totemInstanceFunction *function, totemRegister *returnRegister);
    totemExecStatus totemExecState_ExecNative(totemExecState *state, totemNativeFunction *function, totemRegister *returnRegister);
    void totemExecState_ExecuteInstructions(totemExecState *state);
    
    void totemExecState_ExecComplexSet(totemExecState *state);
    void totemExecState_ExecComplexShift(totemExecState *state);
    
    totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state);
    void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call);
    
    void totemExecState_PrintRegister(totemExecState *state, FILE *file, totemRegister *reg);
    void totemExecState_PrintRegisterList(totemExecState *state, FILE *file, totemRegister *regs, size_t numRegisters);
    void totemExecState_CleanupRegisterList(totemExecState *state, totemRegister *regs, uint32_t num);
    
    totemExecStatus totemExecState_EmptyString(totemExecState *state, totemRegister *strOut);
    totemExecStatus totemExecState_IntToString(totemExecState *state, totemInt val, totemRegister *strOut);
    totemExecStatus totemExecState_FloatToString(totemExecState *state, totemFloat val, totemRegister *strOut);
    totemExecStatus totemExecState_TypeToString(totemExecState *state, totemPublicDataType type, totemRegister *strOut);
    totemExecStatus totemExecState_NativeFunctionToString(totemExecState *state, totemNativeFunction *func, totemRegister *strOut);
    totemExecStatus totemExecState_InstanceFunctionToString(totemExecState *state, totemInstanceFunction *func, totemRegister *strOut);
    totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemArray *arr, totemRegister *strOut);
    totemExecStatus totemExecState_CoroutineToString(totemExecState *state, totemFunctionCall *cr, totemRegister *strOut);
    totemExecStatus totemExecState_StringToFunction(totemExecState *state, totemRegister *src, totemRegister *dst);
    totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRegister *strOut);
    totemExecStatus totemExecState_InternStringChar(totemExecState *state, totemRegister *src, totemStringLength index, totemRegister *dst);
    totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut);
    
    totemExecStatus totemExecState_ArrayGet(totemExecState *state, totemArray *arr, uint32_t index, totemRegister *dst);
    totemExecStatus totemExecState_ArraySet(totemExecState *state, totemArray *obj, uint32_t index, totemRegister *src);
    totemExecStatus totemExecState_ArrayShift(totemExecState *state, totemArray *obj, uint32_t index, totemRegister *dst);
    totemExecStatus totemExecState_ConcatArrays(totemExecState *state, totemRegister *src1, totemRegister *src2, totemRegister *dst);
    
    totemExecStatus totemExecState_ObjectGet(totemExecState *state, totemObject *obj, totemRegister *key, totemRegister *dst);
    totemExecStatus totemExecState_ObjectSet(totemExecState *state, totemObject *obj, totemRegister *key, totemRegister *src);
    totemExecStatus totemExecState_ObjectShift(totemExecState *state, totemObject *obj, totemRegister *key, totemRegister *dst);
    
    void totemExecState_Assign(totemExecState *state, totemRegister *dst, totemRegister *src);
    void totemExecState_AssignNewInt(totemExecState *state, totemRegister *dst, totemInt newVal);
    void totemExecState_AssignNewFloat(totemExecState *state, totemRegister *dst, totemFloat newVal);
    void totemExecState_AssignNewType(totemExecState *state, totemRegister *dst, totemPublicDataType newVal);
    void totemExecState_AssignNewInternedString(totemExecState *state, totemRegister *dst, totemInternedStringHeader *newVal);
    void totemExecState_AssignNewNativeFunction(totemExecState *state, totemRegister *dst, totemNativeFunction *func);
    void totemExecState_AssignNewInstanceFunction(totemExecState *state, totemRegister *dst, totemInstanceFunction *func);
    void totemExecState_AssignNewString(totemExecState *state, totemRegister *dst, totemRegister *src);
    void totemExecState_AssignNewArray(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewCoroutine(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewObject(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewUserdata(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewBoolean(totemExecState *state, totemRegister *dst, totemBool newVal);
    void totemExecState_AssignNull(totemExecState *state, totemRegister *dst);
    
    totemExecStatus totemExecState_Add(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_Subtract(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_Multiply(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_Divide(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_Subtract(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_Multiply(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_Divide(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_LessThan(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_LessThanEquals(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_MoreThan(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_MoreThanEquals(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    totemExecStatus totemExecState_Cast(totemExecState *state, totemRegister *dst, totemRegister *src1, totemRegister *src2);
    
    typedef enum
    {
        totemCmdFlags_None = 0,
        totemCmdFlags_DumpInstructions = 1,
        totemCmdFlags_DoNotRun = 1 << 1
    }
    totemCmdFlags;
    
#define TOTEM_REGISTER_ISGC(x) ((x)->DataType >= totemPrivateDataType_Array && (x)->DataType <= totemPrivateDataType_Userdata)
#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }
    
#ifdef __cplusplus
}
#endif

#endif
