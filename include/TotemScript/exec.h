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
    
    struct totemGCObject;
    
    typedef struct
    {
        totemScriptFunction *Function;
        struct totemGCObject *Instance;
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
    totemBool totemRegister_GetString(totemRegister *reg, const char **str, totemStringLength *len, totemHash *hash);
    totemBool totemRegister_IsGarbageCollected(totemRegister *reg);
    totemBool totemRegister_IsString(totemRegister *reg);
    
    typedef struct totemFunctionCall
    {
        struct totemFunctionCall *Prev;
        struct totemGCObject *Instance;
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
        totemGCObjectType_Instance
    };
    typedef uint8_t totemGCObjectType;
    const char *totemGCObjectType_Describe(totemGCObjectType);
    
    struct totemExecState;
    typedef void(*totemUserdataDestructor)(struct totemExecState*, void*);
    
    /*
     assuming that ref counts will never overflow
     - it is effectively impossible for every addressable memory location to hold a gc object
     - by design, ref counts never go negative
     - therefore, values in the farthest upper range can be safely reserved for special use
     - if none of this holds, then all apologies
     */
    typedef size_t totemRefCount;
    
    typedef enum
    {
        totemGCObjectMarkSweepFlag_None = 0,
        totemGCObjectMarkSweepFlag_Mark = 1,
        totemGCObjectMarkSweepFlag_IsGrey = 1 << 1,
        totemGCObjectMarkSweepFlag_IsUsed = 1 << 2
    }
    totemGCObjectMarkSweepFlag;
    
    typedef struct totemGCObject
    {
        totemGCHeader Header; // MUST be the first member - this struct should be able to masquerade as a totemGCHeader
        
        union
        {
            totemFunctionCall *Coroutine;
            totemHashMap *Object;
            totemUserdataDestructor UserdataDestructor;
            totemInstance *Instance;
        };
        
        union
        {
            totemRegister *Registers;
            void *Userdata;
        };
        
        size_t NumRegisters;
        
#if TOTEM_GCTYPE_ISREFCOUNTING
        totemRefCount RefCount;
        totemRefCount CycleDetectCount;
#elif TOTEM_GCTYPE_MARKANDSWEEP
        totemGCObjectMarkSweepFlag MarkFlags;
#endif
        
        totemGCObjectType Type;
    }
    totemGCObject;
    
#ifdef __cplusplus
#define TOTEM_JMP_TYPE char
#define TOTEM_JMP_TRY(jmp) try
#define TOTEM_JMP_CATCH(jmp) catch (...)
#define TOTEM_JMP_THROW(jmp) throw jmp; TOTEM_UNREACHABLE()
#else
#define TOTEM_JMP_TYPE jmp_buf
#define TOTEM_JMP_TRY(jmp) if (!totem_setjmp(jmp))
#define TOTEM_JMP_CATCH(jmp) else
#define TOTEM_JMP_THROW(jmp) totem_longjmp(jmp); TOTEM_UNREACHABLE()
#endif
    
    typedef struct totemJmpNode
    {
        TOTEM_JMP_TYPE Buffer;
        struct totemJmpNode *Prev;
        totemExecStatus Status;
    }
    totemJmpNode;
    
    typedef enum
    {
        totemMarkSweepState_Reset = 0,
        totemMarkSweepState_Mark = 1,
        totemMarkSweepState_Sweep = 2
    }
    totemMarkSweepState;
    
    typedef struct totemExecState
    {
#if TOTEM_GCTYPE_ISREFCOUNTING
        totemGCHeader GC;
        totemGCHeader GC2;
#elif TOTEM_GCTYPE_ISMARKANDSWEEP
        totemGCHeader GCWhite;
        totemGCHeader GCGrey;
        totemGCHeader GCBlack;
        totemGCHeader GCRoots;
        totemGCHeader GCSweep;
        totemMarkSweepState GCState;
        totemBool GCCurrentBit;
#endif
        size_t GCNum;
        size_t GCNumBytes;
        size_t GCByteThreshold;
        
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
        const char **ArgV;
        int ArgC;
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
    
    void totemExecState_Init(totemExecState *state);
    void totemExecState_Cleanup(totemExecState *state);
    void totemExecState_SetArgV(totemExecState *state, const char **argv, int num);
    void *totemExecState_Alloc(totemExecState *state, size_t size);
    totemExecStatus totemExecState_Exec(totemExecState *state, totemInstanceFunction *function);
    void totemExecState_ExecuteInstructions(totemExecState *state);
    
    void totemExecState_InitGC(totemExecState *state);
    void totemExecState_CleanupGC(totemExecState *state);
    void totemGCHeader_Reset(totemGCHeader *obj);
    totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type, size_t numRegisters);
    totemBool totemExecState_ExpandGCObject(totemExecState *state, totemGCObject *obj);
    totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj);
    totemExecStatus totemExecState_CreateInstance(totemExecState *state, totemScript *script, totemGCObject **gcOut);
    totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemInstanceFunction *function, totemGCObject **gcOut);
    totemExecStatus totemExecState_CreateObject(totemExecState *state, totemInt size, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateArray(totemExecState *state, totemInt numRegisters, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateUserdata(totemExecState *state, void *data, totemUserdataDestructor destructor, totemGCObject **gcOut);
    totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, size_t numRegisters, totemGCObject **objOut);
    void totemExecState_IncRefCount(totemExecState *state, totemRegister *gc);
    void totemExecState_DecRefCount(totemExecState *state, totemRegister *gc);
    void totemExecState_WriteBarrier(totemExecState *state, totemGCObject *gc);
    void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co);
    void totemExecState_DestroyObject(totemExecState *state, totemHashMap *obj);
    void totemExecState_DestroyInstance(totemExecState *state, totemInstance *obj);
    void totemExecState_CollectGarbage(totemExecState *state, totemBool full);
    
    totemExecStatus totemExecState_CreateSubroutine(totemExecState *state, uint8_t numRegisters, totemGCObject *instance, totemRegister *returnReg, totemFunctionType funcType, void *function, totemFunctionCall **callOut);
    void totemExecState_PushRoutine(totemExecState *state, totemFunctionCall *call, totemInstruction *startAt);
    void totemExecState_PopRoutine(totemExecState *state);
    
    totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state);
    void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call);
    
    void totemExecState_PrintRegister(totemExecState *state, FILE *file, totemRegister *reg);
    void totemExecState_PrintRegisterList(totemExecState *state, FILE *file, totemRegister *regs, size_t numRegisters);
    void totemExecState_CleanupRegisterList(totemExecState *state, totemRegister *regs, size_t num);
    
    totemExecStatus totemExecState_ToString(totemExecState *state, totemRegister *src, totemRegister *dst);
    totemExecStatus totemExecState_EmptyString(totemExecState *state, totemRegister *strOut);
    totemExecStatus totemExecState_IntToString(totemExecState *state, totemInt val, totemRegister *strOut);
    totemExecStatus totemExecState_FloatToString(totemExecState *state, totemFloat val, totemRegister *strOut);
    totemExecStatus totemExecState_TypeToString(totemExecState *state, totemPublicDataType type, totemRegister *strOut);
    totemExecStatus totemExecState_NativeFunctionToString(totemExecState *state, totemNativeFunction *func, totemRegister *strOut);
    totemExecStatus totemExecState_InstanceFunctionToString(totemExecState *state, totemInstanceFunction *func, totemRegister *strOut);
    totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemRegister *arr, size_t len, totemRegister *strOut);
    totemExecStatus totemExecState_CoroutineToString(totemExecState *state, totemFunctionCall *cr, totemRegister *strOut);
    totemExecStatus totemExecState_StringToFunction(totemExecState *state, totemRegister *src, totemRegister *dst);
    totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRegister *strOut);
    totemExecStatus totemExecState_InternStringChar(totemExecState *state, totemRegister *src, totemInt index, totemRegister *dst);
    totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut);
    
    totemExecStatus totemExecState_ArrayGet(totemExecState *state, totemRegister *arr, size_t len, totemInt index, totemRegister *dst);
    totemExecStatus totemExecState_ArraySet(totemExecState *state, totemRegister *arr, size_t len, totemInt index, totemRegister *src);
    totemExecStatus totemExecState_ArrayShift(totemExecState *state, totemRegister *arr, size_t len, totemInt index, totemRegister *dst);
    totemExecStatus totemExecState_ConcatArrays(totemExecState *state, totemRegister *src1, totemRegister *src2, totemRegister *dst);
    
    totemExecStatus totemExecState_ObjectGet(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *dst);
    totemExecStatus totemExecState_ObjectSet(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *src);
    totemExecStatus totemExecState_ObjectShift(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *dst);
    
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
    
#ifdef __cplusplus
}
#endif

#endif
