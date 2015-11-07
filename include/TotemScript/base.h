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

#ifdef __cplusplus
extern "C" {
#endif
    
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
 * no classes
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
 
 Compile Process:
 * 
 * 1. lex into tokens
 * 2. parse into tree
 * 3. eval into prototype
 * 4. build to bytecode
 * 5. invalidate existing script instances
 * 6. insert new script at new location
 * 7. restart existing script instances with new global stacks
 * 
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
    
    $x = new Dictionary();
    NATIVEFUNCTION $x (new dictionary callback)
    // x now stores a reference returned by callback
    
    $x.add("what", 123);
    NATIVEFUNCTION $x.add (dictionary.add callback)
    FUNCARG $x 0
    FUNCARG "what" 0
    FUNCARG 123 1
    
    $x.whatever;
    NATIVEFUNCTION $x.whatever
    
    $x.whatever = 123;
    NATIVEFUNCTION $x.whatever
    FUNCARG 123 1
    
    $x = 123;
    MOVE $x 123
    // $x is now a number! the dictionary reference must be garbage-collected
    // OR, the reference could be collected manually like this:
    delete $x;
    
    $y = $x.somethingElse($y, 123 + 123);
    ADD ?? 123 123
    NATIVEFUNCTION $y $x.somethingElse
    FUNCARG $y 0
    FUNCARG ?? 1
    
    $z = scriptFunction(10, 20);
    SCRIPTFUNCTION $z scriptFunction
    FUNCARG 10 0
    FUNCARG 20 1
    
    $x = new ScriptClass();
    SCRIPTFUNCTION $x ScriptClass Constructor
}

*/

    
#define TOTEM_ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
    
    typedef enum
    {
        totemBool_True,
        totemBool_False
    }
    totemBool;
    
    typedef uint8_t totemRegisterIndex;
    typedef int16_t totemOperand;
    typedef int32_t totemOperandX;
    typedef double totemNumber;
    typedef size_t totemReference;

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
        totemDataType_String = 1,	 // immutable C-style string stored in stack
        totemDataType_Reference = 2   // void *pointer stored in register
    };
    typedef int8_t totemDataType;
        
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
        totemOperation_None,			// empty instruction, used at end of script
        totemOperation_Move,            // A = B
        totemOperation_Add,             // A = B + C
        totemOperation_Subtract,        // A = B - C
        totemOperation_Multiply,        // A = B * C
        totemOperation_Divide,          // A = B / C
        totemOperation_Power,           // A = B ^ C
        totemOperation_Equals,          // A = B == C
        totemOperation_NotEquals,       // A = B != C
        totemOperation_LessThan,        // A = B < C
        totemOperation_LessThanEquals,  // A = B <= C
        totemOperation_MoreThan,        // A = B > C
        totemOperation_MoreThanEquals,  // A = B >= C
        totemOperation_LogicalOr,       // A = B && C
        totemOperation_LogicalAnd,      // A = B || C
        totemOperation_ConditionalGoto, // if(A is 0) skip Bx instructions (can be negative)
        totemOperation_Goto,            // skip Bx instructions (can be negative)
        totemOperation_NativeFunction,  // A = Bx(), where Bx is the index of a native function
        totemOperation_ScriptFunction,  // A = Bx(), where Bx is the index of a script function
        totemOperation_FunctionArg,     // A is register to pass, Bx is 1 or 0 to indicate if this is the last argument to pass
        totemOperation_Return           // return A
    };
    typedef int8_t totemOperation;

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

    enum
    {
        totemInstructionSize_Op = 5,
        totemInstructionSize_Operand = 9,
        totemInstructionSize_A = 9,
        totemInstructionSize_Axx = 27,
        totemInstructionSize_B = 9,
        totemInstructionSize_Bx = 18,
        totemInstructionSize_C = 9
    };
        
    #define TOTEM_MAX_NATIVEFUNCTIONS (1 << totemInstructionSize_Bx)
    #define TOTEM_MAX_SCRIPTFUNCTIONS (1 << totemInstructionSize_Bx)
    #define TOTEM_MAX_SCRIPTS (1 << totemInstructionSize_A)
    #define TOTEM_MAX_OBJECT_MEMBERS (1 << totemInstructionSize_A)

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
    
#define TOTEM_MAX_REGISTERS (1 << totemOperandSize_RegisterIndex)

#define TOTEM_INSTRUCTION_OPERAND_DEF(name) \
    name##Type:totemOperandSize_RegisterType, name##Index:totemOperandSize_RegisterIndex \
    
    typedef enum
    {
        totemRegisterScopeType_Local = 0,
        totemRegisterScopeType_Global = 1
    }
    totemRegisterScopeType;
        
    typedef struct
    {
        uint32_t Operation:totemInstructionSize_Op,
        TOTEM_INSTRUCTION_OPERAND_DEF(OperandA),
        TOTEM_INSTRUCTION_OPERAND_DEF(OperandB),
        TOTEM_INSTRUCTION_OPERAND_DEF(OperandC);
    }
    totemABCInstruction;

    typedef struct
    {
        uint32_t Operation:totemInstructionSize_Op,
        TOTEM_INSTRUCTION_OPERAND_DEF(OperandA),
        OperandBx:totemInstructionSize_Bx;
    }
    totemABxInstruction;

    typedef struct
    {
        uint32_t Operation:totemInstructionSize_Op,
        OperandAxx:totemInstructionSize_Axx;
    }
    totemAxxInstruction;

    typedef union
    {
        totemABCInstruction Abc;
        totemABxInstruction Abx;
        totemAxxInstruction Axx;
        uint32_t Value;
    }
    totemInstruction;
        
    const char *totemOperation_GetName(totemOperation op);
    const char *totemDataType_GetName(totemDataType type);
    const char *totemRegisterScopeType_GetOperandTypeCode(totemRegisterScopeType type);
        
    void totemInstruction_PrintList(FILE *file, totemInstruction *instructions, size_t num);
    void totemInstruction_Print(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbcInstruction(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbxInstruction(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAxxInstruction(FILE *file, totemInstruction instruction);
    
    typedef void *(*totemMallocCb)(size_t);
    typedef void (*totemFreeCb)(void*);
    typedef uint32_t (*totemHashCb)(char*, size_t);
    
    void *totem_Malloc(size_t len);
    void totem_Free(void * mem);
    uint32_t totem_Hash(char *data, size_t len);
    void totem_SetGlobalCallbacks(totemMallocCb malloc, totemFreeCb free, totemHashCb hash);
    
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
    
    // TODO: Dynamically managed list of freelists / slabs to serve entire runtime
    
#define TOTEM_MEMORYBLOCK_DATASIZE (1024)
    
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
        char *Key;
        size_t KeyLen;
        size_t Value;
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
    }
    totemActor;
    
    typedef struct totemFunctionCall
    {
        struct totemFunctionCall *Prev;
        totemActor *Actor;
        totemRegister *ReturnRegister;
        totemRegister *RegisterFrameStart;
        totemInstruction *LastInstruction;
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
