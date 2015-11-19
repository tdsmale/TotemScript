//
//  base.h
//  TotemScript
//
//  Created by Timothy Smale on 27/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_BASE_H
#define TOTEMSCRIPT_BASE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    // TODO: declutter headers
    
/**
 * Register-based
 * There is a global stack for global variables & constants (strings mainly) that is unique to each instantiation of a compiled script
 *
 * Registers are 64-bit & can store either:
 *  - 64-bit floating point number
 *  - reference to string constant, represented as an index into global stack
 *  - a user-supplied "reference" to data stored elsewhere
 * 
 * Basic arithmetic can only be applied to numbers
 * 
 * String constants are stored as normal C strings, but are immutable, and are accessed via reference
 * Registers are passed by value
 *
 * 32-bit instruction size
 * 
 -----------------------------------------------------------------------
 |0    4|5                 13|14                22|23                31|
 -----------------------------------------------------------------------
 |OPTYPE|OPERANDA            |OPERANDB            |OPERANDC            | ABCInstruction
 -----------------------------------------------------------------------
 |5     |9                   |9                   |9                   |
 -----------------------------------------------------------------------
 
 Operands can have two formats:
 
 1. Registers:
 ---------------------------
 |0        1|2            9|
 ---------------------------
 |IS GLOBAL?|REGISTER INDEX|
 ---------------------------
 |1         |8             |
 ---------------------------
  first bit indicates if it is a global register or not
  next eight bits are the register index
 
 2. Values:
 ---------------------------
 |0                       9|
 ---------------------------
 |VALUE                    |
 ---------------------------
 |9                        |
 ---------------------------
  some operations prefer to utilise the entire 9 bit value
  
  
 Register Headers
 -------------------------------
 |0      3|4                 15|
 -------------------------------
 |DATATYPE|STACKSIZE           |
 -------------------------------
 |4       |12                  |
 -------------------------------
 
 -----------------------------------------------------------------------
 |0    4|5                                                           31|
 -----------------------------------------------------------------------
 |OPTYPE|OPERANDAxx                                                    | AxxInstruction
 -----------------------------------------------------------------------
 |5     |27                                                            |
 -----------------------------------------------------------------------
 
 -----------------------------------------------------------------------
 |0    4|5                 13|14                                     31|
 -----------------------------------------------------------------------
 |OPTYPE|OPERANDA            |OPERANDBx                                | ABxInstruction
 -----------------------------------------------------------------------
 |5     |9                   |18                                       |
 -----------------------------------------------------------------------
 
 Basic idea:
 
 1. define "Actors"
 2. "Actors" have functions & variables
 3. Other actors can invoke functions, but not access variables

function test() {
    $x = 1;
    MOVE $x 1
    
    $z = "This string is stored as a constant in the global stack. String values are immutable.";
    MOVE $z string
    
    $z = "Another string. The first string hasn't gone anywhere.";
    MOVE $z string
    
    $z = 123;
    MOVE $z 123 ($z is now a number!)
    
    if($x == $globalNumber) {
        EQUALS a $x $globalNumber
        CONDITIONALGOTO a 123
    }
    
    while($x == $globalNumber) {
        EQUALS a $x $globalNumber
        CONDITIONALGOTO a 123
        
        GOTO 122
    }
    
    x($a, $b, $c, $d);
    NATIVEFUNCTION ?? x
    FUNCARG $a 0
    FUNCARG $b 0
    FUNCARG $c 0
    FUNCARG $d 1 // each argument is inserted into function's local registers, function uses whatever it wants
    
    return $z;
    RETURN $z
    
    ///
    
    $x = 123;
 
    $z = scriptFunction(10, 20);
    SCRIPTFUNCTION $z scriptFunction
    FUNCARG 10 0
    FUNCARG 20 1
}
*/
    
#define TOTEM_ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#define TOTEM_STRINGIFY_CASE(x) case x: return #x
    
#define TOTEM_BITMASK(start, length) (((((unsigned)1) << (length)) - 1) << (start))
#define TOTEM_GETBITS(i, mask) ((i) & (mask))
#define TOTEM_SETBITS(i, mask) ((i) |= (mask))
#define TOTEM_GETBITS_OFFSET(i, mask, offset) (TOTEM_GETBITS((i), (mask)) >> (offset))
#define TOTEM_SETBITS_OFFSET(i, mask, offset) (TOTEM_SETBITS((i), ((mask) << (offset))))
#define TOTEM_MAXVAL_UNSIGNED(numBits) ((1 << (numBits)) - 1)
#define TOTEM_MINVAL_UNSIGNED(numBits) (0)
#define TOTEM_MAXVAL_SIGNED(numBits) ((totemOperandXSigned)TOTEM_MAXVAL_UNSIGNED((numBits) - 1))
#define TOTEM_MINVAL_SIGNED(numBits) (-(TOTEM_MAXVAL_SIGNED(numBits)) - 1)
#define TOTEM_NUMBITS(type) (sizeof(type) * CHAR_BIT)

    typedef enum
    {
        totemBool_True = 1,
        totemBool_False = 0
    }
    totemBool;
    
    typedef enum
    {
        totemReturnOption_Implicit = 0,
        totemReturnOption_Register = 1
    }
    totemReturnOption;
    
    typedef uint8_t totemRegisterIndex;
    typedef int32_t totemOperandXSigned;
    typedef uint32_t totemOperandXUnsigned;
    typedef double totemNumber;
    typedef size_t totemReference;
    
    totemOperandXSigned totemOperandXSigned_FromUnsigned(totemOperandXUnsigned val, uint32_t numBits);

    enum
    {
        totemStringSize_Length = 32,
        totemStringSize_Index = 32
    };

    typedef struct
    {
        uint32_t Length;
        uint32_t Index;
    }
    totemRuntimeString;
    
    // TODO: global string-value cache attached to runtime, instead of per-actor
    // TODO: "fast-strings" (i.e. strings that are 8 or less chars long can sit inside register value)
    // TODO: ascii char support (e.g. 'a', 'b' etc.)
    // TODO: function pointers
        
    typedef struct
    {
        const char *Value;
        uint32_t Length;
    }
    totemString;
    
    void totemString_FromLiteral(totemString *strOut, const char *str);

    typedef union
    {
        totemNumber Number;
        totemReference Reference;
        totemRuntimeString String;
        uint64_t Data;
    }
    totemRegisterValue;

    enum
    {
        totemDataType_Number = 0,     // double stored in register
        totemDataType_String = 1,	  // immutable C-style string stored in cache
        totemDataType_Reference = 2   // void *pointer stored in register
    };
    typedef int8_t totemDataType;
    const char *totemDataType_Describe(totemDataType type);
        
    typedef struct
    {
        totemRegisterValue Value;
        totemDataType DataType;
    }
    totemRegister;

    typedef enum
    {
        totemFunctionType_Script,
        totemFunctionType_Native
    }
    totemFunctionType;
    
    /**
     * Operation Types
     */
    enum
    {
        totemOperationType_None = 0,                // empty instruction, used at end of script
        totemOperationType_Move = 1,                // A = B
        totemOperationType_Add = 2,                 // A = B + C
        totemOperationType_Subtract = 3,            // A = B - C
        totemOperationType_Multiply = 4,            // A = B * C
        totemOperationType_Divide = 5,              // A = B / C
        totemOperationType_Power = 6,               // A = B ^ C
        totemOperationType_Equals = 7,              // A = B == C
        totemOperationType_NotEquals = 8,           // A = B != C
        totemOperationType_LessThan = 9,            // A = B < C
        totemOperationType_LessThanEquals = 10,     // A = B <= C
        totemOperationType_MoreThan = 11,           // A = B > C
        totemOperationType_MoreThanEquals = 12,     // A = B >= C
        totemOperationType_LogicalOr = 13,          // A = B && C
        totemOperationType_LogicalAnd = 14,         // A = B || C
        totemOperationType_ConditionalGoto = 15,    // if(A is 0) skip Bx instructions (can be negative)
        totemOperationType_Goto = 16,               // skip Bx instructions (can be negative)
        totemOperationType_NativeFunction = 17,     // A = Bx(), where Bx is the index of a native function
        totemOperationType_ScriptFunction = 18,     // A = Bx(), where Bx is the index of a script function
        totemOperationType_FunctionArg = 19,        // A is register to pass, Bx is 1 or 0 to indicate if this is the last argument to pass
        totemOperationType_Return = 20              // return A
    };
    typedef uint8_t totemOperationType;
    const char *totemOperationType_Describe(totemOperationType op);

    /**
     * Instruction Format
     */
    typedef enum
    {
        totemInstructionType_Abc = 1,
        totemInstructionType_Abx,
        totemInstructionType_Axx
    }
    totemInstructionType;
    totemInstructionType totemOperationType_GetInstructionType(totemOperationType op);

    enum
    {
        totemInstructionSize_Op = 5,
        totemInstructionSize_Operand = 9,
        totemInstructionSize_A = 9,
        totemInstructionSize_Ax = 27,
        totemInstructionSize_B = 9,
        totemInstructionSize_Bx = 18,
        totemInstructionSize_C = 9
    };
    
    enum
    {
        totemInstructionStart_Op = 0,
        totemInstructionStart_A = 5,
        totemInstructionStart_B = 14,
        totemInstructionStart_C = 23
    };

#define TOTEM_OPERANDX_SIGNED_MAX TOTEM_MAXVAL_SIGNED(totemInstructionSize_Bx)
#define TOTEM_OPERANDX_SIGNED_MIN TOTEM_MINVAL_SIGNED(totemInstructionSize_Bx)
#define TOTEM_OPERANDX_UNSIGNED_MAX TOTEM_MAXVAL_UNSIGNED(totemInstructionSize_Bx)
#define TOTEM_OPERANDX_UNSIGNED_MIN TOTEM_MINVAL_UNSIGNED(totemInstructionSize_Bx)

#define TOTEM_MAX_NATIVEFUNCTIONS TOTEM_OPERANDX_UNSIGNED_MAX
#define TOTEM_MAX_SCRIPTFUNCTIONS TOTEM_OPERANDX_UNSIGNED_MAX
#define TOTEM_MAX_REGISTERS TOTEM_MAXVAL_UNSIGNED(totemOperandSize_RegisterIndex)
    
    enum
    {
        totemOperandType_LocalRegister = 0,
        totemOperandType_GlobalRegister = 1
    }
    totemOperandType;
        
    enum
    {
        totemOperandSize_RegisterType = 1,
        totemOperandSize_RegisterIndex = 8
    }
    totemOperandSize;
    
    enum
    {
        totemRegisterScopeType_Local = 0,
        totemRegisterScopeType_Global = 1
    };
    typedef uint8_t totemRegisterScopeType;
    const char *totemRegisterScopeType_GetOperandTypeCode(totemRegisterScopeType type);
    
    typedef uint32_t totemInstruction;
    
    void totemInstruction_PrintBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbcBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbxBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAxxBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintList(FILE *file, totemInstruction *instructions, size_t num);
    void totemInstruction_Print(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbcInstruction(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbxInstruction(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAxxInstruction(FILE *file, totemInstruction instruction);
    
#define TOTEM_INSTRUCTION_MASK_REGISTERA TOTEM_BITMASK(totemInstructionStart_A, totemInstructionSize_A)
#define TOTEM_INSTRUCTION_MASK_REGISTERA_SCOPE TOTEM_BITMASK(totemInstructionStart_A, 1)
#define TOTEM_INSTRUCTION_MASK_REGISTERA_INDEX TOTEM_BITMASK(totemInstructionStart_A + 1, totemInstructionSize_A - 1)
    
#define TOTEM_INSTRUCTION_MASK_REGISTERB TOTEM_BITMASK(totemInstructionStart_B, totemInstructionSize_B)
#define TOTEM_INSTRUCTION_MASK_REGISTERB_SCOPE TOTEM_BITMASK(totemInstructionStart_B, 1)
#define TOTEM_INSTRUCTION_MASK_REGISTERB_INDEX TOTEM_BITMASK(totemInstructionStart_B + 1, totemInstructionSize_B - 1)
    
#define TOTEM_INSTRUCTION_MASK_REGISTERC TOTEM_BITMASK(totemInstructionStart_C, totemInstructionSize_C)
#define TOTEM_INSTRUCTION_MASK_REGISTERC_SCOPE TOTEM_BITMASK(totemInstructionStart_C, 1)
#define TOTEM_INSTRUCTION_MASK_REGISTERC_INDEX TOTEM_BITMASK(totemInstructionStart_C + 1, totemInstructionSize_C - 1)
    
#define TOTEM_INSTRUCTION_MASK_AX TOTEM_BITMASK(totemInstructionStart_A, totemInstructionSize_Ax)
#define TOTEM_INSTRUCTION_MASK_BX TOTEM_BITMASK(totemInstructionStart_B, totemInstructionSize_Bx)
#define TOTEM_INSTRUCTION_MASK_OP TOTEM_BITMASK(totemInstructionStart_Op, totemInstructionSize_Op)
    
#define TOTEM_INSTRUCTION_GET_OP(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_OP, totemInstructionStart_Op)
    
#define TOTEM_INSTRUCTION_GET_AX_UNSIGNED(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_AX, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_AX_SIGNED(ins) totemOperandXSigned_FromUnsigned(TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_AX, totemInstructionStart_A), totemInstructionSize_Ax)
    
#define TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_BX, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_BX_SIGNED(ins) totemOperandXSigned_FromUnsigned(TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_BX, totemInstructionStart_B), totemInstructionSize_Bx)
    
#define TOTEM_INSTRUCTION_GET_REGISTERA(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERA, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERA_SCOPE, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERA_INDEX, totemInstructionStart_A + 1)
    
#define TOTEM_INSTRUCTION_GET_REGISTERB(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERB, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERB_SCOPE, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERB_INDEX, totemInstructionStart_B + 1)
    
#define TOTEM_INSTRUCTION_GET_REGISTERC(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERC, totemInstructionStart_C)
#define TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERC_SCOPE, totemInstructionStart_C)
#define TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERC_INDEX, totemInstructionStart_C + 1)
    
    typedef void *(*totemMallocCb)(size_t);
    typedef void (*totemFreeCb)(void*);
    typedef uint32_t (*totemHashCb)(const char*, size_t);

    void *totem_Malloc(size_t len);
    void totem_Free(void * mem);
    void *totem_CacheMalloc(size_t len);
    void totem_CacheFree(void *ptr, size_t len);

    void totem_printBits(FILE *file, uint32_t data, uint32_t numBits, uint32_t start);
    void totem_Exit(int code);
    
    uint32_t totem_Hash(const char *data, size_t len);
    void totem_SetMemoryCallbacks(totemMallocCb malloc, totemFreeCb free);
    void totem_SetHashCallback(totemHashCb hash);
    
    typedef struct
    {
        char *Data;
        size_t ObjectSize;
        size_t MaxLength;
        size_t Length;
    }
    totemMemoryBuffer;
    
    totemBool totemMemoryBuffer_Secure(totemMemoryBuffer *buffer, size_t amount);
    void *totemMemoryBuffer_Get(totemMemoryBuffer *buffer, size_t index);
    size_t totemMemoryBuffer_GetNumObjects(totemMemoryBuffer *buffer);
    size_t totemMemoryBuffer_GetMaxObjects(totemMemoryBuffer *buffer);
    void totemMemoryBuffer_Reset(totemMemoryBuffer *buffer, size_t objectSize);
    void totemMemoryBuffer_Cleanup(totemMemoryBuffer *buffer);
    
#define TOTEM_MEMORYBLOCK_DATASIZE (512)
    
    typedef struct totemMemoryBlock
    {
        char Data[TOTEM_MEMORYBLOCK_DATASIZE];
        size_t Remaining;
        struct totemMemoryBlock *Prev;
    }
    totemMemoryBlock;
    
    void *totemMemoryBlock_Alloc(totemMemoryBlock **blockHead, size_t objectSize);
    
    typedef struct totemHashMapEntry
    {
        const char *Key;
        size_t KeyLen;
        size_t Value;
        size_t Hash;
        struct totemHashMapEntry *Next;
    }
    totemHashMapEntry;
    
    typedef struct
    {
        totemHashMapEntry **Buckets;
        totemHashMapEntry *FreeList;
        size_t NumBuckets;
        size_t NumKeys;
    }
    totemHashMap;
    
    totemBool totemHashMap_Insert(totemHashMap *hashmap, const char *Key, size_t KeyLen, size_t Value);
    totemHashMapEntry *totemHashMap_Find(totemHashMap *hashmap, const char *Key, size_t keyLen);
    void totemHashMap_Reset(totemHashMap *hashmap);
    void totemHashMap_Cleanup(totemHashMap *hashmap);
    
    typedef struct
    {
        totemMemoryBuffer Registers;
        totemMemoryBuffer StringData;
        totemHashMap Variables;
        totemHashMap Strings;
        totemHashMap Numbers;
        totemRegisterScopeType Scope;
    }
    totemRegisterListPrototype;
    
    typedef struct
    {
        size_t ScriptHandle;
        totemMemoryBuffer GlobalData;
        totemMemoryBuffer GlobalRegisters;
    }
    totemActor;
    
    typedef struct totemFunctionCall
    {
        struct totemFunctionCall *Prev;
        totemActor *Actor;
        totemRegister *ReturnRegister;
        totemRegister *RegisterFrameStart;
        totemInstruction *ResumeAt;
        size_t FunctionHandle;
        totemFunctionType Type;
        uint8_t NumArguments;
    }
    totemFunctionCall;
    
    typedef struct
    {
        totemMemoryBuffer NativeFunctions;
        totemMemoryBuffer Scripts;

        totemHashMap ScriptLookup;
        totemHashMap NativeFunctionsLookup;
        
        totemMemoryBlock *LastMemBlock;
        totemFunctionCall *FunctionCallFreeList;
    }
    totemRuntime;
    
    typedef struct
    {
        totemRegisterListPrototype GlobalRegisters;
        totemHashMap FunctionLookup;
        totemMemoryBuffer Functions;
        totemMemoryBuffer Instructions;
        void *ErrorContext;
        totemRuntime *Runtime;
    }
    totemBuildPrototype;
    
    typedef struct
    {
        size_t InstructionsStart;
        size_t RegistersNeeded;
        totemString Name;
    }
    totemFunction;

#ifdef __cplusplus
}
#endif
    
#endif
