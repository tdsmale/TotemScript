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
        totemExecStatus_FunctionNotFound,
        totemExecStatus_UnexpectedDataType,
        totemExecStatus_UnrecognisedOperation,
        totemExecStatus_InvalidDispatch,
        totemExecStatus_RegisterOverflow,
        totemExecStatus_OutOfMemory,
        totemExecStatus_IndexOutOfBounds,
        totemExecStatus_DivideByZero,
        totemExecStatus_InternalBufferOverrun,
        totemExecStatus_NullPointer
    }
    totemExecStatus;
    
    totemExecStatus totemExecStatus_Break(totemExecStatus status);
    const char *totemExecStatus_Describe(totemExecStatus status);
    
	typedef enum totemInterpreterStatus
    {
        totemInterpreterStatus_Success,
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
        totemLinkStatus_UnexpectedValueType,
        totemLinkStatus_CannotFindFile
    }
    totemLinkStatus;
    
    const char *totemLinkStatus_Describe(totemLinkStatus status);
    totemLinkStatus totemLinkStatus_Break(totemLinkStatus status);

	struct totemExecState;

	typedef void(*totemRuntimeFunctionCb)(struct totemExecState*);
	
	typedef struct totemScriptFunction
	{
		totemRuntimeFunctionCb Invoke;
		totemInstruction *InstructionsStart;
		size_t NumInstructions;
		totemOperandXu Address;
		totemOperandXu RegistersNeeded;
	}
	totemScriptFunction;

    TOTEM_DYNBUFFER_DECLARATION(totemScriptFunction);

	typedef struct totemInstanceFunction
	{
		totemScriptFunction *Function;
		struct totemGCObject *Instance;
	}
	totemInstanceFunction;

	struct totemScript;
    
	typedef struct totemInstance
    {
        totemInstanceFunction *LocalFunctions;
        struct totemScript *Script;
        size_t NumLocalFunctions;
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
        totemOperandXu Address;
    }
    totemNativeFunction;
    
	typedef struct totemInternedStringHeader
    {
        totemHashValue Hash;
        totemStringLength Length;
        char Data[4];
    }
    totemInternedStringHeader;

    void totemInternedStringHeader_Free(totemInternedStringHeader* str);
    
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

     The format used for non-floating point values is:

     16-bit tag that specifies type (A is always 1, L is the most-sigificant type bit, R is the remaining 3 type bits)
     |
     |                48-bit value
     |                |
     |                |
     |LAAAAAAAAAAAARRR|VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
     |                                                                |
     |Most Significant                                                | Least Significant

     The only two types that lose out are:
     - ints, which are reduced to 32-bits
     - mini-strings, which are reduced to 5 chars or less
     */

#define TOTEM_DATATYPE(x) ((uint16_t)( \
TOTEM_BITMASK(uint16_t, 3, 12) |  /* all exponent bits + quiet NaN flag are set */ \
((TOTEM_GETBITS((uint16_t)(x), TOTEM_BITMASK(uint16_t, 3, 1))) << 12) | /* the most significant bit becomes the "sign" bit*/ \
((TOTEM_GETBITS((uint16_t)(x), TOTEM_BITMASK(uint16_t, 0, 3)))) )) /* the rest become the least-significant bits */

#define totemPrivateDataType_Float (0)
#define totemPrivateDataType_Int TOTEM_DATATYPE(1)
#define totemPrivateDataType_InternedString TOTEM_DATATYPE(2)
#define totemPrivateDataType_Type TOTEM_DATATYPE(3)
#define totemPrivateDataType_NativeFunction TOTEM_DATATYPE(4)
#define totemPrivateDataType_InstanceFunction TOTEM_DATATYPE(5)
#define totemPrivateDataType_MiniString TOTEM_DATATYPE(6)
#define totemPrivateDataType_Boolean TOTEM_DATATYPE(7)
#define totemPrivateDataType_Null TOTEM_DATATYPE(8)

     // these should all be GC'd objects
#define totemPrivateDataType_Array TOTEM_DATATYPE(9)
#define totemPrivateDataType_Coroutine TOTEM_DATATYPE(10)
#define totemPrivateDataType_Object TOTEM_DATATYPE(11)
#define totemPrivateDataType_Userdata TOTEM_DATATYPE(12)
#define totemPrivateDataType_Instance TOTEM_DATATYPE(13)
#define totemPrivateDataType_Unused2 TOTEM_DATATYPE(14)
#define totemPrivateDataType_Unused3 TOTEM_DATATYPE(15)

#define TOTEM_TYPEPAIR(a, b) ((((uint32_t)(a)) << 16) | ((uint32_t)(b)))
#define TOTEM_MINISTRING_MAXLENGTH (5)

    typedef uint16_t totemPrivateDataType;

    typedef union
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
        totemPrivateDataType_Instance = 13,
        totemPrivateDataType_Unused2 = 14,
        totemPrivateDataType_Unused3 = 15
    };
	typedef totemPublicDataType totemPrivateDataType;

#define TOTEM_MINISTRING_MAXLENGTH (7)
    
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

    typedef struct totemRegister
    {
        totemRegisterValue Value;
        totemPrivateDataType DataType;
    }
    totemRegister;
#endif

    TOTEM_DYNBUFFER_DECLARATION(totemRegister);

#if !TOTEM_VMOPT_NANBOXING
    TOTEM_STATIC_ASSERT(sizeof(totemRegisterValue) == 8, "Totem Register Values must be 8 bytes");
    TOTEM_STATIC_ASSERT(sizeof(totemFloat) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(totemInt) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(void*) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(totemPublicDataType) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(uint64_t) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT((sizeof(char)* (TOTEM_MINISTRING_MAXLENGTH + 1)) <= sizeof(uint64_t), "Value-size mismatch");
#else
    TOTEM_STATIC_ASSERT(sizeof(totemFloat) == sizeof(totemRegister), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(uint64_t) == sizeof(totemRegister), "Value-size mismatch");
#endif

    const char *totemPrivateDataType_Describe(totemPrivateDataType type);
    totemPublicDataType totemPrivateDataType_ToPublic(totemPrivateDataType type);
	
	typedef union totemRuntimeStringValueSource
	{
		totemInternedStringHeader *InternedString;
		char MiniString[TOTEM_MINISTRING_MAXLENGTH + 1];
	}
	totemRuntimeStringValueSource;

    TOTEM_STATIC_ASSERT(sizeof(totemRuntimeStringValueSource) == TOTEM_SIZEOF_STATIC(totemRuntimeStringValueSource, InternedString), "Value-size mismatch");
    TOTEM_STATIC_ASSERT((sizeof(char)* (TOTEM_MINISTRING_MAXLENGTH + 1)) <= TOTEM_SIZEOF_STATIC(totemRuntimeStringValueSource, InternedString), "Value-size mismatch");

	typedef struct totemRuntimeStringValue
    {
		totemRuntimeStringValueSource Src;
        totemPrivateDataType Type;
        totemStringLength Length;
        totemHashValue Hash;
    }
    totemRuntimeStringValue;

    TOTEM_DYNBUFFER_DECLARATION(totemRuntimeStringValue);

    void totemRuntimeStringValue_FromInternedString(totemInternedStringHeader *str, totemRuntimeStringValue *val);
    void totemRuntimeStringValue_FromMiniStringRegister(totemRegister *str, totemRuntimeStringValue* val);
    void totemRuntimeStringValue_SetMiniString(const char *str, uint8_t len, totemRuntimeStringValue *val);
    void totemRuntimeStringValue_Print(totemRuntimeStringValue *val, totemLog *dst);
    const char *totemRuntimeStringValue_GetVal(totemRuntimeStringValue *val);
    totemBool totemRuntimeStringValue_Compare(totemRuntimeStringValue *val, totemRuntimeStringValue *with);
    totemBool totemRuntimeStringValue_CompareWithString(totemRuntimeStringValue *val, totemString *with);
    
    typedef struct totemRuntimeStringSetLiteralKey
    {
        totemString Key;
        totemRuntimeStringValue *Value;
    }
    totemRuntimeStringSetLiteralKey;

    typedef struct totemRuntimeStringSetParseKey
    {
        totemParseString *Key;
        totemRuntimeStringValue* Value;
    }
    totemRuntimeStringSetParseKey;

    typedef struct totemRuntimeStringSetEntry
    {
        totemHashMapEntry Entry;
        totemRuntimeStringValue Data;
    }
    totemRuntimeStringSetEntry;

    TOTEM_HASHMAP_DECLARATION(totemRuntimeStringSet);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemRuntimeStringSet, totemRuntimeStringValue);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemRuntimeStringSet, totemRuntimeStringValue);
    TOTEM_HASHMAP_SEARCH_DECLARATION_SUFFIX(totemRuntimeStringSet, totemRuntimeStringSetParseKey, Parse);
    TOTEM_HASHMAP_SEARCH_DECLARATION_SUFFIX(totemRuntimeStringSet, totemRuntimeStringSetLiteralKey, Literal);

    typedef struct totemRuntimeStringMapKey
    {
        totemRuntimeStringValue Key;
        totemOperandXu Value;
    }
    totemRuntimeStringMapKey;

    typedef struct totemRuntimeStringMapLiteralKey
    {
        totemString Key;
        totemOperandXu Value;
    }
    totemRuntimeStringMapLiteralKey;

    typedef struct totemRuntimeStringMapParseKey
    {
        totemParseString* Key;
        totemOperandXu Value;
    }
    totemRuntimeStringMapParseKey;

    typedef struct totemRuntimeStringMapEntry
    {
        totemHashMapEntry Entry;
        totemRuntimeStringMapKey Data;
    }
    totemRuntimeStringMapEntry;

    TOTEM_HASHMAP_DECLARATION(totemRuntimeStringMap);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemRuntimeStringMap, totemRuntimeStringMapKey);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemRuntimeStringMap, totemRuntimeStringMapKey);
    TOTEM_HASHMAP_SEARCH_DECLARATION_SUFFIX(totemRuntimeStringMap, totemRuntimeStringMapParseKey, Parse);
    TOTEM_HASHMAP_SEARCH_DECLARATION_SUFFIX(totemRuntimeStringMap, totemRuntimeStringMapLiteralKey, Literal);

    totemStringLength totemMiniString_GetLength(const char *c);
    totemHashValue totemMiniString_GetHash(const char* c);
    totemBool totemMiniString_Compare(const char *a, const char *b);
    
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

    TOTEM_DYNBUFFER_DECLARATION(totemNativeFunction);

    typedef struct totemScriptCacheKey
    {
        totemRuntimeStringValue Key;
        struct totemScript *Value;
    }
    totemScriptCacheKey;

    typedef struct totemScriptCacheEntry
    {
        totemHashMapEntry Entry;
        totemScriptCacheKey Data;
    }
    totemScriptCacheEntry;

    TOTEM_HASHMAP_DECLARATION(totemScriptCache);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemScriptCache, totemScriptCacheKey);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemScriptCache, totemScriptCacheKey);

	typedef struct totemRuntime
    {
        totemScriptCache ScriptCache;
        totemNativeFunctionDynBuffer NativeFunctions;
        totemRuntimeStringValueDynBuffer NativeFunctionNames;
        totemRuntimeStringMap NativeFunctionsLookup;
        totemRuntimeStringSet InternedStrings;
        totemLock InternedStringsLock;
        totemLock ScriptCacheLock;
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
    
	typedef enum totemGCObjectFlag
    {
        totemGCObjectFlag_None = 0,
        totemGCObjectFlag_Mark = 1,
        totemGCObjectFlag_IsGrey = 1 << 1,
        totemGCObjectFlag_IsUsed = 1 << 2,
        totemGCObjectFlag_IsRoot = 1 << 3
    }
    totemGCObjectFlag;

	typedef union totemGCObjectPtr
	{
		totemFunctionCall *Coroutine;
        totemRuntimeStringMap *Object;
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
        uint32_t NumRegisters;
        totemGCObjectFlag Flags;
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

    typedef struct totemInstanceCacheKey
    {
        totemRuntimeStringValue Key;
        struct totemGCObject *Value;
    }
    totemInstanceCacheKey;

    typedef struct totemInstanceCacheEntry
    {
        totemHashMapEntry Entry;
        totemInstanceCacheKey Data;
    }
    totemInstanceCacheEntry;

    TOTEM_HASHMAP_DECLARATION(totemInstanceCache);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemInstanceCache, totemInstanceCacheKey);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemInstanceCache, totemInstanceCacheKey);
    
    typedef struct totemExecState
    {
        totemInstanceCache InstanceCache;
        totemLock InstanceCacheLock;
        
        totemGCHeader GCWhite; // candidate for deletion
        totemGCHeader GCGrey; // accessible, contains registers to be scanned
        totemGCHeader GCBlack; // accessible, already scanned
        totemGCHeader GCRoots; // not deleted until the execState is destroyed
        totemGCHeader GCSweep; // to be destroyed
        totemMarkSweepState GCState;
        totemBool GCCurrentBit;
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
        totemParseTree ParseTree;
        totemBuildPrototype Build;
        totemInterpreterResult Result;
    }
    totemInterpreter;
    
    void totemInterpreter_InitWithString(totemInterpreter *interpreter, const char *str, size_t strLen);
    void totemInterpreter_InitWithFile(totemInterpreter *interpreter, FILE *file);
    void totemInterpreter_Reset(totemInterpreter *interpreter);
    void totemInterpreter_Cleanup(totemInterpreter *interpreter);
    totemBool totemInterpreter_Interpret(totemInterpreter *interpreter);
    void totemInterpreter_PrintResult(totemInterpreter *interpreter, totemLog *target);
    
    void totemInstance_Init(totemInstance *instance);
    void totemInstance_Reset(totemInstance *instance);
    void totemInstance_Cleanup(totemInstance *instance);
    
    typedef struct totemScriptFunctionAddr
    {
        size_t RegisterIndex;
        size_t FunctionAddr;
    }
    totemScriptFunctionAddr;

    TOTEM_DYNBUFFER_DECLARATION(totemScriptFunctionAddr);

	typedef struct totemScript
	{
        totemScriptFunctionAddrDynBuffer ScriptFunctionAddrs;
        totemRuntimeStringMap FunctionNameLookup;
		totemRegister *FunctionNames;
		totemRegister *GlobalRegisters;
        totemInstruction *Instructions;
        totemScriptFunction *Functions;
        size_t NumInstructions;
        size_t NumFunctions;
        size_t NumGlobalRegisters;
	}
	totemScript;

	typedef void(*totemBootstrapFunctionCb)(totemExecState*, void*, totemRegister*);

    void totemScript_Init(totemScript *script);
    void totemScript_Reset(totemScript *script);
    void totemScript_Cleanup(totemScript *script);
    totemBool totemScript_GetFunctionName(totemScript *script, totemOperandXu addr, totemRuntimeStringValue *valOut);
    
    void totemRuntime_Init(totemRuntime *runtime);
    void totemRuntime_Reset(totemRuntime *runtime);
    void totemRuntime_Cleanup(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkStdLib(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters);
    totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *scriptOut);
    totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemRuntimeFunctionCb func, totemString *name, totemOperandXu *addressOut);
    totemLinkStatus totemRuntime_LinkNativeFunctions(totemRuntime *runtime, totemNativeFunctionPrototype *funcs, totemOperandXu num);
    totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRuntimeStringValue *valOut);
    totemLinkStatus totemRuntime_InternParseString(totemRuntime *runtime, totemParseString *str, totemRuntimeStringValue *valOut);
    totemLinkStatus totemRuntime_LoadAndCacheScript(totemRuntime *runtime, totemRuntimeStringValue *filename, totemScript **scriptOut);
    
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
    totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type, uint32_t numRegisters, totemBool isRoot);
    totemBool totemExecState_ExpandGCObject(totemExecState *state, totemGCObject *obj);
	totemExecStatus totemExecState_CloneGCObject(totemExecState *state, totemGCObject *src, totemGCObject **dst);
    totemGCObject *totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj);

    totemExecStatus totemExecState_CreateInstance(totemExecState *state, totemScript *script, totemBool isRoot, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneInstance(totemExecState *state, totemGCObject *src, totemBool isRoot, totemGCObject **gcOut);
    totemExecStatus totemExecState_LoadAndCacheInstance(totemExecState *state, totemRuntimeStringValue *name, totemGCObject **instanceOut);

    totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemInstanceFunction *function, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneCoroutine(totemExecState *state, totemGCObject *src, totemGCObject **gcOut);

    totemExecStatus totemExecState_CreateObject(totemExecState *state, uint32_t size, totemGCObject **objOut);
	totemExecStatus totemExecState_CloneObject(totemExecState *state, totemGCObject *src, totemGCObject **objOut);

    totemExecStatus totemExecState_CreateArray(totemExecState *state, uint32_t numRegisters, totemGCObject **objOut);
	totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, uint32_t numRegisters, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneArray(totemExecState *state, totemGCObject *src, totemGCObject **objOut);

    totemExecStatus totemExecState_CreateUserdata(totemExecState *state, void *data, totemUserdataDestructor destructor, totemGCObject **gcOut);
	totemExecStatus totemExecState_CloneUserdata(totemExecState *state, totemGCObject *src, totemGCObject **gcOut);

    void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co);
    void totemExecState_DestroyObject(totemExecState *state, totemRuntimeStringMap *obj);
    void totemExecState_DestroyInstance(totemExecState *state, totemInstance *obj);
    void totemExecState_CollectGarbage(totemExecState *state, totemBool full);
    
    void totemExecState_WriteBarrier(totemExecState *state, totemGCObject *gc);
        
    totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state);
    void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call);
    
    void totemRuntime_PrintRegister(totemRuntime *state, totemLog *file, totemRegister *reg, size_t index, totemBool isMultiple);
    void totemRuntime_PrintRegisterList(totemRuntime *state, totemLog *file, totemRegister *regs, size_t numRegisters);
    
    totemExecStatus totemExecState_ToString(totemExecState *state, totemRegister *src, totemRuntimeStringValue *dst);
    totemExecStatus totemExecState_EmptyString(totemExecState *state, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_IntToString(totemExecState *state, totemInt val, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_FloatToString(totemExecState *state, totemFloat val, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_TypeToString(totemExecState *state, totemPublicDataType type, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_NativeFunctionToString(totemExecState *state, totemNativeFunction *func, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_InstanceFunctionToString(totemExecState *state, totemInstanceFunction *func, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemRegister *arr, size_t len, totemRuntimeStringValue *strOut);
	totemExecStatus totemExecState_ObjectToString(totemExecState *state, totemGCObject *gc, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRuntimeStringValue *strOut);
    totemExecStatus totemExecState_InternStringChar(totemExecState *state, totemRegister *src, uint32_t index, totemRuntimeStringValue *dst);
    totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRuntimeStringValue *strOut);
    
    totemExecStatus totemExecState_StringToFunction(totemExecState* state, totemRuntimeStringValue* src, totemRegister* dst);

    totemExecStatus totemExecState_ArrayGet(totemExecState *state, totemGCObject* obj, uint32_t index, totemRegister *dst);
	totemExecStatus totemExecState_ArraySet(totemExecState *state, totemGCObject *obj, uint32_t index, totemRegister *src);
    totemExecStatus totemExecState_ConcatArrays(totemExecState *state, totemRegister *src1, totemRegister *src2, totemRegister *dst);
    
    totemExecStatus totemExecState_ObjectGet(totemExecState *state, totemGCObject *obj, totemRuntimeStringValue* key, totemRegister *dst);
    totemExecStatus totemExecState_ObjectSet(totemExecState *state, totemGCObject *obj, totemRuntimeStringValue* key, totemRegister *src);

    void totemRegister_InitList(totemRegister* reg, size_t num);
    totemBool totemRegister_GetString(totemRegister* reg, totemRuntimeStringValue* val);
    totemStringLength totemRegister_GetStringLength(totemRegister* reg);
    totemHashValue totemRegister_GetStringHash(totemRegister* reg);
    totemPrivateDataType totemRegister_GetType(totemRegister* reg);
    totemBool totemRegister_IsType(totemRegister* reg, totemPrivateDataType type);
    totemBool totemRegister_IsNull(totemRegister* reg);
    totemBool totemRegister_IsBoolean(totemRegister* reg);
    totemBool totemRegister_IsUserdata(totemRegister* reg);
    totemBool totemRegister_IsGarbageCollected(totemRegister* reg);
    totemBool totemRegister_IsString(totemRegister* reg);
    totemBool totemRegister_IsInternedString(totemRegister* reg);
    totemBool totemRegister_IsMiniString(totemRegister* reg);
    totemBool totemRegister_IsInt(totemRegister* reg);
    totemBool totemRegister_IsFloat(totemRegister* reg);
    totemBool totemRegister_IsArray(totemRegister* reg);
    totemBool totemRegister_IsObject(totemRegister* reg);
    totemBool totemRegister_IsNativeFunction(totemRegister* reg);
    totemBool totemRegister_IsInstanceFunction(totemRegister* reg);
    totemBool totemRegister_IsCoroutine(totemRegister* reg);
    totemBool totemRegister_IsFunction(totemRegister* reg);
    totemBool totemRegister_IsTypeValue(totemRegister* reg);
    totemBool totemRegister_IsInstance(totemRegister *reg);
    totemBool totemRegister_IsZero(totemRegister* reg);
    totemBool totemRegister_IsNotZero(totemRegister* reg);
    totemBool totemRegister_Equals(totemRegister* a, totemRegister* b);

    struct totemGCObject* totemRegister_GetGCObject(totemRegister* reg);
    totemInstanceFunction* totemRegister_GetInstanceFunction(totemRegister* reg);
    totemNativeFunction* totemRegister_GetNativeFunction(totemRegister* reg);
    totemPublicDataType totemRegister_GetTypeValue(totemRegister* reg);
    totemInt totemRegister_GetInt(totemRegister* reg);
    totemFloat totemRegister_GetFloat(totemRegister* reg);
    totemInternedStringHeader* totemRegister_GetInternedString(totemRegister* reg);
    void totemRegister_GetMiniString(totemRegister* reg, char* strOut);
    char* totemRegister_GetMiniStringPtr(totemRegister* reg);

    void totemRegister_SetInt(totemRegister* reg, totemInt val);
    void totemRegister_SetFloat(totemRegister* reg, totemFloat val);
    void totemRegister_SetTypeValue(totemRegister* reg, totemPublicDataType type);
    void totemRegister_SetBool(totemRegister* reg, totemBool val);
    void totemRegister_SetNull(totemRegister* reg);
    void totemRegister_SetNativeFunction(totemRegister* reg, totemNativeFunction* func);
    void totemRegister_SetInstanceFunction(totemRegister* reg, totemInstanceFunction* func);
    void totemRegister_SetInternedString(totemRegister* reg, totemInternedStringHeader* str);
    void totemRegister_SetMiniString(totemRegister* reg, char* str, uint8_t len);
    void totemRegister_SetArray(totemRegister* dst, struct totemGCObject* val);
    void totemRegister_SetObject(totemRegister* dst, struct totemGCObject* val);
    void totemRegister_SetCoroutine(totemRegister* dst, struct totemGCObject* val);
    void totemRegister_SetUserdata(totemRegister* dst, struct totemGCObject* val);
    void totemRegister_SetInstance(totemRegister *dst, struct totemGCObject *val);
    void totemRegister_Set(totemRegister* dst, totemRegister* src);
    void totemRegister_SetString(totemRegister* dst, totemRuntimeStringValue* src);
    void totemRegister_IntFromFloat(totemRegister *dst, totemRegister *src);

#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }
    
#ifdef __cplusplus
}
#endif

#endif
