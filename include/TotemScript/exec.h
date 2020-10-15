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
#include <TotemScript/memory.h>
#include <stdint.h>
#include <setjmp.h>
#include <limits.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
    
	typedef enum totemExecStatus
    {
        totemExecStatus_Continue,
        totemExecStatus_Stop,
        totemExecStatus_ScriptNotFound,
        totemExecStatus_InstanceFunctionNotFound,
        totemExecStatus_NativeFunctionNotFound,
        totemExecStatus_UnexpectedDataType,
        totemExecStatus_InvalidKey,
        totemExecStatus_UnrecognisedOperation,
		totemExecStatus_UnrecognisedInstructionType,
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
    
	typedef enum totemInterpreterStatus
    {
        totemInterpreterStatus_Success,
        totemInterpreterStatus_FileError,
        totemInterpreterStatus_LexError,
        totemInterpreterStatus_ParseError,
        totemInterpreterStatus_EvalError
    }
    totemInterpreterStatus;
    
	typedef enum totemLinkStatus
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

	struct totemExecState;

	typedef void(*totemRuntimeFunctionCb)(struct totemExecState*);
	
	typedef struct totemScriptFunction
	{
		totemRuntimeFunctionCb Invoke;
		totemInstruction *InstructionsStart;
		size_t NumInstructions;
		totemOperandXUnsigned Address;
		totemOperandXUnsigned RegistersNeeded;
	}
	totemScriptFunction;

	TOTEM_CACHEBUFFER_DEFINITION(totemScriptFunction);

	typedef struct totemInstanceFunction
	{
		totemScriptFunction *Function;
		struct totemGCObject *Instance;
	}
	totemInstanceFunction;

	TOTEM_CACHEBUFFER_DEFINITION(totemInstanceFunction);

	struct totemScript;
    
	typedef struct totemInstance
    {
		totemInstanceFunctionBuffer LocalFunctions;
        struct totemScript *Script;
    }
    totemInstance;
    
    struct totemGCObject;
    
	typedef struct totemNativeFunctionPrototype
    {
        totemRuntimeFunctionCb Callback;
        totemString Name;
    }
    totemNativeFunctionPrototype;
    
	typedef struct totemNativeFunction
    {
        totemRuntimeFunctionCb Callback;
        totemOperandXUnsigned Address;
    }
    totemNativeFunction;
    
	typedef struct totemInternedStringHeader
    {
        totemHashValue Hash;
        totemStringLength Length;
        char Data[4];
    }
    totemInternedStringHeader;
    
	typedef enum totemFunctionCallFlag
    {
        totemFunctionCallFlag_None = 0,
        totemFunctionCallFlag_FreeStack = 1,
        totemFunctionCallFlag_IsCoroutine = 2
    }
    totemFunctionCallFlag;
    
#if TOTEM_VMOPT_NANBOXING
    
    /*
     NaN-Boxing
	 - storing all possible register values in a single 8-byte float
     
     assumptions:
     - the IEEE-754 floating point standard is being used
     - x86-64 only deals in pointers up to 48-bits in width
     
     IEEE 754 format:
     
     1-bit Sign
     |
     | 11-bit Exponent (if all bits are set, this is NaN)
     | |
     | |           52-bit Mantissa (if the most-significant bit is 1, then this is a "quiet" NaN and we can use it to store values)
     | |           |
     | |           |
     |S|NNNNNNNNNNN|MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
     |                                                                 |
     |Most Significant                                                 | Least Significant
     
     -------------------------------------------------------------------
     
     The format we use for storing non-floating point values is:
     
     16-bit tag that specifies type (A is always 1, L is the most-sigificant type bit, R is the remaining 3 type bits)
     |
     |                48-bit value
     |                |
     |                |
     |LAAAAAAAAAAAARRR|VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
     |                                                                |
     |Most Significant                                                | Least Significant

     The only two types that lose out are:
     - ints, which are reduced to 32-bits from 64
     - mini-strings, which are reduced to 5 chars from 6
	 - totemRegister_GetType() is also more expensive, but it's rarely relied on

	 Normal floating point values are easily distinguished from other types by checking if it is a "quiet" NaN or not

     */
    
#define TOTEM_DATATYPE(x) ((uint16_t)( \
	TOTEM_BITMASK(uint16_t, 3, 12) |  /* all exponent bits + quiet NaN flag are set */ \
	((TOTEM_GETBITS((uint16_t)(x), TOTEM_BITMASK(uint16_t, 3, 1))) << 12) | /* the most significant bit becomes the "sign" bit*/ \
	((TOTEM_GETBITS((uint16_t)(x), TOTEM_BITMASK(uint16_t, 0, 3)))) )) /* the rest become the least-significant bits */
    
	enum
	{
		totemPrivateDataType_Float				= 0,
		totemPrivateDataType_Int				= TOTEM_DATATYPE(1),
		totemPrivateDataType_InternedString		= TOTEM_DATATYPE(2),
		totemPrivateDataType_Type				= TOTEM_DATATYPE(3),
		totemPrivateDataType_NativeFunction		= TOTEM_DATATYPE(4),
		totemPrivateDataType_InstanceFunction	= TOTEM_DATATYPE(5),
		totemPrivateDataType_MiniString			= TOTEM_DATATYPE(6),
		totemPrivateDataType_Boolean			= TOTEM_DATATYPE(7),
		totemPrivateDataType_Null				= TOTEM_DATATYPE(8),

		// these should all be GC'd objects
		totemPrivateDataType_Array		= TOTEM_DATATYPE(9),
		totemPrivateDataType_Coroutine	= TOTEM_DATATYPE(10),
		totemPrivateDataType_Object		= TOTEM_DATATYPE(11),
		totemPrivateDataType_Userdata	= TOTEM_DATATYPE(12),
		totemPrivateDataType_Unused1	= TOTEM_DATATYPE(13),
		totemPrivateDataType_Unused2	= TOTEM_DATATYPE(14),
		totemPrivateDataType_Unused3	= TOTEM_DATATYPE(15)		
	};
    
#define TOTEM_MINISTRING_MAXLENGTH (5)
    
    typedef uint16_t totemPrivateDataType;
    
	typedef union totemRegister
    {
        totemFloat AsFloat;
        uint64_t AsBits;
        struct
        {
            uint16_t ValA;
            uint16_t ValB;
            uint16_t ValC;
            uint16_t Tag;
        }
        AsTagVal;
        struct
        {
            char MiniString[TOTEM_MINISTRING_MAXLENGTH + 1];
            char TagA;
            char TagB;
        }
        AsMiniString;
        struct
        {
            totemInt LSW;
            totemInt MSW;
        }
        AsInt;
        struct
        {
            char A, B, C, D, E, F, G, H;
        }
        AsChar;
    }
    totemRegister;

	TOTEM_STATIC_ASSERT(sizeof(totemFloat) == sizeof(totemRegister), "Value-size mismatch");
	TOTEM_STATIC_ASSERT(sizeof(uint64_t) == sizeof(totemRegister), "Value-size mismatch");
    
#else
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
        totemPrivateDataType_Unused1 = 13,
        totemPrivateDataType_Unused2 = 14,
        totemPrivateDataType_Unused3 = 15
    };
	typedef totemPublicDataType totemPrivateDataType;

#if defined(TOTEM_64)
#define TOTEM_MINISTRING_MAXLENGTH (7)
#elif defined(TOTEM_32)
#define TOTEM_MINISTRING_MAXLENGTH (3)
#endif
    
	typedef union totemRegisterValue
    {
        totemFloat Float;
        totemInt Int;
        totemInternedStringHeader *InternedString;
        char MiniString[TOTEM_MINISTRING_MAXLENGTH + 1];
        struct totemGCObject *GCObject;
        totemNativeFunction *NativeFunction;
        totemInstanceFunction *InstanceFunction;
        totemPublicDataType DataType;
    }
    totemRegisterValue;

	TOTEM_STATIC_ASSERT(sizeof(totemFloat) == sizeof(totemRegisterValue), "Value-size mismatch");
	TOTEM_STATIC_ASSERT(sizeof(totemInt) == sizeof(totemRegisterValue), "Value-size mismatch");
	TOTEM_STATIC_ASSERT(sizeof(void*) == sizeof(totemRegisterValue), "Value-size mismatch");
	TOTEM_STATIC_ASSERT(sizeof(totemPublicDataType) == sizeof(totemRegisterValue), "Value-size mismatch");
	TOTEM_STATIC_ASSERT((sizeof(char) * (TOTEM_MINISTRING_MAXLENGTH + 1)) == sizeof(totemRegisterValue), "Value-size mismatch");
	TOTEM_STATIC_ASSERT(sizeof(totemPrivateDataType) == sizeof(totemRegisterValue), "Value-size mismatch");
    
    typedef struct totemRegister
    {
        totemRegisterValue Value;
        totemPrivateDataType DataType;
    }
    totemRegister;
#endif

	TOTEM_CACHEBUFFER_DEFINITION(totemRegister);
    
    const char *totemPrivateDataType_Describe(totemPrivateDataType type);
    totemPublicDataType totemPrivateDataType_ToPublic(totemPrivateDataType type);
	
	typedef union totemRuntimeStringValueSource
	{
		totemInternedStringHeader *InternedString;
		char MiniString[TOTEM_MINISTRING_MAXLENGTH + 1];
	}
	totemRuntimeStringValueSource;

	typedef struct totemRuntimeStringValue
    {
		totemRuntimeStringValueSource Src;
        const char *Value;
        totemPrivateDataType Type;
    }
    totemRuntimeStringValue;
    
    totemStringLength totemMiniString_GetLength(char *c);
    
    void totemRegister_InitList(totemRegister *reg, size_t num);
    void totemRegister_GetStringValue(totemRegister *reg, totemRuntimeStringValue *val);
    totemStringLength totemRegister_GetStringLength(totemRegister *reg);
    totemHashValue totemRegister_GetStringHash(totemRegister *reg);
    totemPrivateDataType totemRegister_GetType(totemRegister *reg);
    totemBool totemRegister_IsType(totemRegister *reg, totemPrivateDataType type);
    totemBool totemRegister_IsNull(totemRegister *reg);
    totemBool totemRegister_IsBoolean(totemRegister *reg);
    totemBool totemRegister_IsUserdata(totemRegister *reg);
    totemBool totemRegister_IsGarbageCollected(totemRegister *reg);
    totemBool totemRegister_IsString(totemRegister *reg);
    totemBool totemRegister_IsInternedString(totemRegister *reg);
    totemBool totemRegister_IsMiniString(totemRegister *reg);
    totemBool totemRegister_IsInt(totemRegister *reg);
    totemBool totemRegister_IsFloat(totemRegister *reg);
    totemBool totemRegister_IsArray(totemRegister *reg);
    totemBool totemRegister_IsObject(totemRegister *reg);
    totemBool totemRegister_IsNativeFunction(totemRegister *reg);
    totemBool totemRegister_IsInstanceFunction(totemRegister *reg);
    totemBool totemRegister_IsCoroutine(totemRegister *reg);
	totemBool totemRegister_IsFunction(totemRegister *reg);
    totemBool totemRegister_IsTypeValue(totemRegister *reg);
    totemBool totemRegister_IsZero(totemRegister *reg);
    totemBool totemRegister_IsNotZero(totemRegister *reg);
    totemBool totemRegister_Equals(totemRegister *a, totemRegister *b);
    
    struct totemGCObject *totemRegister_GetGCObject(totemRegister *reg);
    totemInstanceFunction *totemRegister_GetInstanceFunction(totemRegister *reg);
    totemNativeFunction *totemRegister_GetNativeFunction(totemRegister *reg);
    totemPublicDataType totemRegister_GetTypeValue(totemRegister *reg);
    totemInt totemRegister_GetInt(totemRegister *reg);
    totemFloat totemRegister_GetFloat(totemRegister *reg);
    totemInternedStringHeader *totemRegister_GetInternedString(totemRegister *reg);
    void totemRegister_GetMiniString(totemRegister *reg, char *strOut);
    
    void totemRegister_SetInt(totemRegister *reg, totemInt val);
    void totemRegister_SetFloat(totemRegister *reg, totemFloat val);
    void totemRegister_SetTypeValue(totemRegister *reg, totemPublicDataType type);
    void totemRegister_SetBoolean(totemRegister *reg, totemBool val);
    void totemRegister_SetNull(totemRegister *reg);
    void totemRegister_SetNativeFunction(totemRegister *reg, totemNativeFunction *func);
    void totemRegister_SetInstanceFunction(totemRegister *reg, totemInstanceFunction *func);
    void totemRegister_SetInternedString(totemRegister *reg, totemInternedStringHeader *str);
    void totemRegister_SetMiniString(totemRegister *reg, char *str);
    
	typedef union totemFunctionPtr
	{
		totemInstanceFunction *Instance;
		totemNativeFunction *Native;
		void *Void;
	}
	totemFunctionPtr;

    typedef struct totemFunctionCall
    {
        struct totemFunctionCall *Prev;
        struct totemGCObject *Instance;
        totemRegister *ReturnRegister;
        totemRegister *PreviousFrameStart;
		totemRegister *PreviousGlobals;
        totemRegister *FrameStart;
        totemInstruction *ResumeAt;
		totemRuntimeFunctionCb Invoke;
		totemFunctionPtr Function;
        totemFunctionCallFlag Flags;
        uint32_t NumRegisters;
		uint32_t NumArguments;
    }
    totemFunctionCall;

	TOTEM_CACHEBUFFER_DEFINITION(totemNativeFunction);

	typedef struct totemRuntime
    {
        totemNativeFunctionBuffer NativeFunctions;
        totemRegisterBuffer NativeFunctionNames;
        totemHashMap NativeFunctionsLookup;
        totemHashMap InternedStrings;
        totemLock InternedStringsLock;
    }
    totemRuntime;
    
	typedef union totemGCLink
	{
		struct totemGCHeader *Hdr;
		struct totemGCObject *Obj;
	}
	totemGCLink;

    typedef struct totemGCHeader
    {
		totemGCLink Prev;
		totemGCLink Next;
    }
    totemGCHeader;
    
	typedef enum totemGCObjectType
    {
        totemGCObjectType_Deleting = 0,
        totemGCObjectType_Array,
        totemGCObjectType_Coroutine,
        totemGCObjectType_Object,
        totemGCObjectType_Userdata,
        totemGCObjectType_Instance
    } 
	totemGCObjectType;
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
    
	typedef enum totemGCObjectMarkSweepFlag
    {
        totemGCObjectMarkSweepFlag_None = 0,
        totemGCObjectMarkSweepFlag_Mark = 1,
        totemGCObjectMarkSweepFlag_IsGrey = 1 << 1,
        totemGCObjectMarkSweepFlag_IsUsed = 1 << 2
    }
    totemGCObjectMarkSweepFlag;

	typedef union totemGCObjectPtr
	{
		totemFunctionCall *Coroutine;
		totemHashMap *Object;
		totemUserdataDestructor UserdataDestructor;
		totemInstance *Instance;
	}
	totemGCObjectPtr;

	typedef union totemGCObjectData
	{
		totemRegister *Registers;
		void *Userdata;
	}
	totemGCObjectData;
    
    typedef struct totemGCObject
    {
        totemGCHeader Header; // MUST be the first member - this struct should be able to masquerade as a totemGCHeader
		totemGCObjectPtr Ptr;
		totemGCObjectData Data;
        totemInt NumRegisters;
        
#if TOTEM_GCTYPE_ISREFCOUNTING
        totemRefCount RefCount;
        totemRefCount CycleDetectCount;
#elif TOTEM_GCTYPE_MARKANDSWEEP
        totemGCObjectMarkSweepFlag MarkFlags;
#endif
        
        totemGCObjectType Type;
    }
    totemGCObject;

	TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.Next.Hdr) == offsetof(totemGCHeader, Next.Hdr), "totemGCObject* must be able to masquerade as a totemGCHeader*");
	TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.Prev.Hdr) == offsetof(totemGCHeader, Prev.Hdr), "totemGCObject* must be able to masquerade as a totemGCHeader*");
	TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.Next.Obj) == offsetof(totemGCHeader, Next.Obj), "totemGCObject* must be able to masquerade as a totemGCHeader*");
	TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.Prev.Obj) == offsetof(totemGCHeader, Prev.Obj), "totemGCObject* must be able to masquerade as a totemGCHeader*");
    
    typedef struct totemJmpNode
    {
		totemJmpBuffer Buffer;
        struct totemJmpNode *Prev;
        totemExecStatus Status;
    }
    totemJmpNode;
    
	typedef enum totemMarkSweepState
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
        totemRegister *GlobalRegisters;
		totemRegister *LocalRegisters;
        totemRegister *NextFreeRegister;
		totemRegister *LastFreeRegister;
        const char **ArgV;
        size_t ArgC;
    }
    totemExecState;

	typedef union totemInterpreterSubStatus
	{
		totemLoadScriptStatus FileStatus;
		totemLexStatus LexStatus;
		totemParseStatus ParseStatus;
		totemEvalStatus EvalStatus;
		totemLinkStatus LinkStatus;
	}
	totemInterpreterSubStatus;
    
	typedef struct totemInterpreterResult
    {
        size_t ErrorLine;
        size_t ErrorChar;
        size_t ErrorLength;
        totemInterpreterStatus Status;
		totemInterpreterSubStatus SubStatus;
    }
    totemInterpreterResult;
    
	typedef struct totemInterpreter
    {
        totemScriptFile Script;
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
    
	typedef struct totemScript
	{
		totemHashMap FunctionNameLookup;
		totemScriptFunctionBuffer Functions;
		totemRegisterBuffer FunctionNames;
		totemRegisterBuffer GlobalRegisters;
		totemInstructionBuffer Instructions;
	}
	totemScript;

	typedef void(*totemBootstrapFunctionCb)(totemExecState*, void*, totemRegister*);

    void totemScript_Init(totemScript *script);
    void totemScript_Reset(totemScript *script);
    void totemScript_Cleanup(totemScript *script);
    totemBool totemScript_GetFunctionName(totemScript *script, totemOperandXUnsigned addr, totemRuntimeStringValue *valOut);
    
    void totemRuntime_Init(totemRuntime *runtime);
    void totemRuntime_Reset(totemRuntime *runtime);
    void totemRuntime_Cleanup(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkStdLib(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters);
    totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *scriptOut);
    totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemRuntimeFunctionCb func, totemString *name, totemOperandXUnsigned *addressOut);
    totemLinkStatus totemRuntime_LinkNativeFunctions(totemRuntime *runtime, totemNativeFunctionPrototype *funcs, totemOperandXUnsigned num);
    totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRuntimeStringValue * valOut);
    totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, totemOperandXUnsigned *addressOut);
    totemBool totemRuntime_GetNativeFunctionName(totemRuntime *runtime, totemOperandXUnsigned addr, totemRuntimeStringValue *val);
    
    void totemExecState_Init(totemExecState *state);
    void totemExecState_Cleanup(totemExecState *state);
    void totemExecState_SetArgV(totemExecState *state, const char **argv, int num);
    void *totemExecState_Alloc(totemExecState *state, size_t size);
	totemExecStatus totemExecState_StartInstance(totemExecState *state, totemInstanceFunction *function);
	totemExecStatus totemExecState_StartNative(totemExecState *state, totemNativeFunction *function);
	void totemExecState_ExecuteInstructions(totemExecState *state);
    
    void totemExecState_InitGC(totemExecState *state);
    void totemExecState_CleanupGC(totemExecState *state);
    void totemGCHeader_Reset(totemGCHeader *obj);
    totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type, totemInt numRegisters);
    totemBool totemExecState_ExpandGCObject(totemExecState *state, totemGCObject *obj);
	totemExecStatus totemExecState_CloneGCObject(totemExecState *state, totemGCObject *src, totemGCObject **dst);
    totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj);

    totemExecStatus totemExecState_CreateInstance(totemExecState *state, totemScript *script, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneInstance(totemExecState *state, totemGCObject *src, totemGCObject **gcOut);

    totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemInstanceFunction *function, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneCoroutine(totemExecState *state, totemGCObject *src, totemGCObject **gcOut);

    totemExecStatus totemExecState_CreateObject(totemExecState *state, totemInt size, totemGCObject **objOut);
	totemExecStatus totemExecState_CloneObject(totemExecState *state, totemGCObject *src, totemGCObject **objOut);

    totemExecStatus totemExecState_CreateArray(totemExecState *state, totemInt numRegisters, totemGCObject **objOut);
	totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, totemInt numRegisters, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneArray(totemExecState *state, totemGCObject *src, totemGCObject **objOut);

    totemExecStatus totemExecState_CreateUserdata(totemExecState *state, void *data, totemUserdataDestructor destructor, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneUserdata(totemExecState *state, totemGCObject *src, totemGCObject **gcOut);

    void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co);
    void totemExecState_DestroyObject(totemExecState *state, totemHashMap *obj);
    void totemExecState_DestroyInstance(totemExecState *state, totemInstance *obj);
    void totemExecState_CollectGarbage(totemExecState *state, totemBool full);
    
#if TOTEM_GCTYPE_ISREFCOUNTING
    void totemExecState_IncRefCount(totemExecState *state, totemRegister *gc);
	void totemExecState_DecRefCount(totemExecState *state, totemRegister *gc);
#define totemExecState_WriteBarrier(x, y)
#elif TOTEM_GCTYPE_ISMARKANDSWEEP
#define totemExecState_IncRefCount(x, y) do { TOTEM_UNUSED(x); TOTEM_UNUSED(y); } while((void)0,0)
#define totemExecState_DecRefCount(x, y) do { TOTEM_UNUSED(x); TOTEM_UNUSED(y); } while((void)0,0)
    void totemExecState_WriteBarrier(totemExecState *state, totemGCObject *gc);
#endif
        
    totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state);
    void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call);
    
    void totemExecState_PrintRegister(totemExecState *state, FILE *file, totemRegister *reg);
    void totemExecState_PrintRegisterList(totemExecState *state, FILE *file, totemRegister *regs, size_t numRegisters);
    void totemExecState_CleanupRegisterList(totemExecState *state, totemRegister *regs, totemInt num);
    
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
    
    totemExecStatus totemExecState_ArrayGet(totemExecState *state, totemRegister *arr, totemInt len, totemInt index, totemRegister *dst);
	totemExecStatus totemExecState_ArraySet(totemExecState *state, totemRegister *arr, totemInt len, totemInt index, totemRegister *src);
    totemExecStatus totemExecState_ConcatArrays(totemExecState *state, totemRegister *src1, totemRegister *src2, totemRegister *dst);
    
    totemExecStatus totemExecState_ObjectGet(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *dst);
    totemExecStatus totemExecState_ObjectSet(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *src);
    
    void totemExecState_Assign(totemExecState *state, totemRegister *dst, totemRegister *src);
    void totemExecState_AssignNewInt(totemExecState *state, totemRegister *dst, totemInt newVal);
    void totemExecState_AssignNewFloat(totemExecState *state, totemRegister *dst, totemFloat newVal);
    void totemExecState_AssignNewType(totemExecState *state, totemRegister *dst, totemPublicDataType newVal);
    void totemExecState_AssignNewInternedString(totemExecState *state, totemRegister *dst, totemInternedStringHeader *newVal);
    void totemExecState_AssignNewNativeFunction(totemExecState *state, totemRegister *dst, totemNativeFunction *func);
    void totemExecState_AssignNewInstanceFunction(totemExecState *state, totemRegister *dst, totemInstanceFunction *func);
    void totemExecState_AssignNewString(totemExecState *state, totemRegister *dst, totemRuntimeStringValue *src);
    void totemExecState_AssignNewArray(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewCoroutine(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewObject(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
    void totemExecState_AssignNewUserdata(totemExecState *state, totemRegister *dst, totemGCObject *newVal);
	void totemExecState_AssignNewGCObject(totemExecState *state, totemRegister *dst, totemGCObject *newVal, totemPrivateDataType type);
    void totemExecState_AssignNewBoolean(totemExecState *state, totemRegister *dst, totemBool newVal);
    void totemExecState_AssignNull(totemExecState *state, totemRegister *dst);

#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }
    
#ifdef __cplusplus
}
#endif

#endif
