//
//  base.h
//  TotemScript
//
//  Created by Timothy Smale on 27/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_BASE_H
#define TOTEMSCRIPT_BASE_H

#include <TotemScript/platform.h>
#include <TotemScript/opcodes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     Register-based
     32-bit instruction size

     -----------------------------------------------------------------------
     |0    4|5                 13|14                22|23                31|
     -----------------------------------------------------------------------
     |OPTYPE|OPERANDA            |OPERANDB            |OPERANDC            | ABCInstruction
     -----------------------------------------------------------------------
     |5     |9                   |9                   |9                   |
     -----------------------------------------------------------------------

     Operands
     ---------------------------
     |0        1|2            9|
     ---------------------------
     |IS GLOBAL?|REGISTER INDEX|
     ---------------------------
     |1         |8             |
     ---------------------------
     first bit indicates if it is a global register or not
     next eight bits are the register index

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
     */

#define TOTEM_ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#define TOTEM_STRING_LITERAL_SIZE(x) (TOTEM_ARRAY_SIZE(x) - 1)
#define TOTEM_STRINGIFY_CASE(x) case x: return #x
#define TOTEM_STATIC_ASSERT(test, explanation) { static char _assert[(test) ? 1 : -1]; (void)_assert; }

#define TOTEM_BITMASK(start, length) (((((unsigned)1) << (length)) - 1) << (start))
#define TOTEM_HASBITS(i, mask) (((i) & (mask)) == (mask))
#define TOTEM_HASANYBITS(i, mask) (((i) & (mask)) != 0)
#define TOTEM_GETBITS(i, mask) ((i) & (mask))
#define TOTEM_SETBITS(i, mask) ((i) |= (mask))
#define TOTEM_UNSETBITS(i, mask) ((i) &= (~(mask)))
#define TOTEM_GETBITS_OFFSET(i, mask, offset) (TOTEM_GETBITS((i), (mask)) >> (offset))
#define TOTEM_SETBITS_OFFSET(i, mask, offset) (TOTEM_SETBITS((i), ((mask) << (offset))))
#define TOTEM_UNSETBITS_OFFSET(i, mask, offset) (TOTEM_UNSETBITS((i), ((mask) << (offset))))
#define TOTEM_FORCEBIT(i, oneOrZero, offset) (TOTEM_UNSETBITS_OFFSET((i), 1, (offset))); (TOTEM_SETBITS_OFFSET((i), (!!(oneOrZero)), (offset)))
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
        totemReturnFlag_None = 0,
        totemReturnFlag_Register = 1,
        totemReturnFlag_Last = 2
    }
    totemReturnFlag;

    typedef int32_t totemOperandXSigned;
    typedef uint32_t totemOperandXUnsigned;
    typedef double totemFloat;
    typedef int64_t totemInt;
    typedef uint32_t totemHash;
    typedef uintptr_t totemHashValue;
    typedef size_t totemStringLength;

    totemOperandXSigned totemOperandXSigned_FromUnsigned(totemOperandXUnsigned val, uint32_t numBits);

    typedef struct
    {
        const char *Value;
        totemStringLength Length;
    }
    totemString;

    void totemString_FromLiteral(totemString *strOut, const char *str);
    totemBool totemString_Equals(totemString *a, totemString *b);
#define TOTEM_STRING_VAL(x) {x, (totemStringLength)strlen(x)}

    enum
    {
        totemFunctionType_Script = 0,
        totemFunctionType_Native = 1
    };
    typedef uint8_t totemFunctionType;

    typedef struct
    {
        totemHash Hash;
        totemStringLength Length;
        char Data[1];
    }
    totemInternedStringHeader;

#define TOTEM_MINISTRING_MAXLENGTH (7)
    typedef struct
    {
        char Value[TOTEM_MINISTRING_MAXLENGTH + 1];
    }
    totemRuntimeMiniString;

    enum
    {
        totemPublicDataType_Null = 0,
        totemPublicDataType_Int = 1,
        totemPublicDataType_Float = 2,
        totemPublicDataType_String = 3,
        totemPublicDataType_Array = 4,
        totemPublicDataType_Type = 5,
        totemPublicDataType_Function = 6,
        totemPublicDataType_Coroutine = 7,
        totemPublicDataType_Object = 8,
        totemPublicDataType_Userdata = 9,
        totemPublicDataType_Boolean = 10,
        totemPublicDataType_Max = 11
    };
    typedef uint64_t totemPublicDataType;
    const char *totemPublicDataType_Describe(totemPublicDataType type);

    /**
     * Operation Types
     */
    enum
    {
#define TOTEM_OPCODE_FORMAT(x) x,
		TOTEM_EMIT_OPCODES()
#undef TOTEM_OPCODE_FORMAT
    };
    typedef uint8_t totemOperationType;
    const char *totemOperationType_Describe(totemOperationType op);

    /**
     * Instruction Format
     */
    typedef enum
    {
        totemInstructionType_Abc = 1,
        totemInstructionType_Abcx,
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
        totemInstructionSize_C = 9,
        totemInstructionSize_Cx = 9
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
#define TOTEM_MAX_LOCAL_REGISTERS TOTEM_MAXVAL_UNSIGNED(totemOperandSize_RegisterIndex)
#define TOTEM_MAX_GLOBAL_REGISTERS TOTEM_OPERANDX_UNSIGNED_MAX

    typedef enum
    {
        totemOperandType_LocalRegister = 0,
        totemOperandType_GlobalRegister = 1
    }
    totemOperandType;

    typedef enum
    {
#if TOTEM_VMOPT_GLOBAL_OPERANDS
        totemOperandSize_RegisterType = 1,
        totemOperandSize_RegisterIndex = 8
#else
        totemOperandSize_RegisterType = 0,
        totemOperandSize_RegisterIndex = 9
#endif
    }
    totemOperandSize;

    typedef uint32_t totemInstruction;
    void totemInstruction_PrintBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbcBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbxBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAxxBits(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintList(FILE *file, totemInstruction *instructions, size_t num);
    void totemInstruction_Print(FILE *file, totemInstruction instruction);
    void totemInstruction_PrintAbcxInstruction(FILE *file, totemInstruction instruction);
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
#define TOTEM_INSTRUCTION_MASK_CX TOTEM_BITMASK(totemInstructionStart_C, totemInstructionSize_Cx)
#define TOTEM_INSTRUCTION_MASK_OP TOTEM_BITMASK(totemInstructionStart_Op, totemInstructionSize_Op)

#define TOTEM_INSTRUCTION_GET_OP(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_OP, totemInstructionStart_Op)

#define TOTEM_INSTRUCTION_GET_AX_UNSIGNED(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_AX, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_AX_SIGNED(ins) totemOperandXSigned_FromUnsigned(TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_AX, totemInstructionStart_A), totemInstructionSize_Ax)

#define TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_BX, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_BX_SIGNED(ins) totemOperandXSigned_FromUnsigned(TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_BX, totemInstructionStart_B), totemInstructionSize_Bx)

#define TOTEM_INSTRUCTION_GET_CX_UNSIGNED(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_CX, totemInstructionStart_C)
#define TOTEM_INSTRUCTION_GET_CX_SIGNED(ins) totemOperandXSigned_FromUnsigned(TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_CX, totemInstructionStart_C), totemInstructionSize_Cx)

#define TOTEM_INSTRUCTION_GET_REGISTERA(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERA, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_REGISTERB(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERB, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_REGISTERC(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERC, totemInstructionStart_C)

#if TOTEM_VMOPT_GLOBAL_OPERANDS
#define TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERA_SCOPE, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERA_INDEX, totemInstructionStart_A + 1)

#define TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERB_SCOPE, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERB_INDEX, totemInstructionStart_B + 1)

#define TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERC_SCOPE, totemInstructionStart_C)
#define TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERC_INDEX, totemInstructionStart_C + 1)
#else
#define TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(ins) (totemOperandType_LocalRegister)
#define TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(ins) TOTEM_INSTRUCTION_GET_REGISTERA(ins)

#define TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(ins) (totemOperandType_LocalRegister)
#define TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(ins) TOTEM_INSTRUCTION_GET_REGISTERB(ins)

#define TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(ins) (totemOperandType_LocalRegister)
#define TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(ins) TOTEM_INSTRUCTION_GET_REGISTERC(ins)
#endif

    typedef void *(*totemMallocCb)(size_t);
    typedef void (*totemFreeCb)(void*);
    typedef uint32_t (*totemHashCb)(const void*, size_t);

    void *totem_CacheMalloc(size_t len);
    void totem_CacheFree(void *ptr, size_t len);

    void totem_printBits(FILE *file, uint32_t data, uint32_t numBits, uint32_t start);
    totemBool totem_getcwd(char *buffer, size_t size);

    totemHash totem_Hash(const void *data, size_t len);
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

    void totemMemoryBuffer_Init(totemMemoryBuffer *buffer, size_t objectSize);
    void totemMemoryBuffer_Reset(totemMemoryBuffer *buffer);
    void totemMemoryBuffer_Cleanup(totemMemoryBuffer *buffer);
    void *totemMemoryBuffer_Secure(totemMemoryBuffer *buffer, size_t amount);
    void *totemMemoryBuffer_Insert(totemMemoryBuffer *buffer, void *data, size_t amount);
    void *totemMemoryBuffer_TakeFrom(totemMemoryBuffer *buffer, totemMemoryBuffer *from);
    void totemMemoryBuffer_Pop(totemMemoryBuffer *buffer, size_t amount);
    void *totemMemoryBuffer_Top(totemMemoryBuffer *buffer);
    void *totemMemoryBuffer_Bottom(totemMemoryBuffer *buffer);
    void *totemMemoryBuffer_Get(totemMemoryBuffer *buffer, size_t index);
    size_t totemMemoryBuffer_GetNumObjects(totemMemoryBuffer *buffer);
    size_t totemMemoryBuffer_GetMaxObjects(totemMemoryBuffer *buffer);
    totemBool totemMemoryBuffer_Reserve(totemMemoryBuffer *buffer, size_t amount);

#define TOTEM_MEMORYBLOCK_DATASIZE (512)

    typedef struct totemMemoryBlock
    {
        char Data[TOTEM_MEMORYBLOCK_DATASIZE];
        size_t Remaining;
        struct totemMemoryBlock *Prev;
    }
    totemMemoryBlock;

    void *totemMemoryBlock_Alloc(totemMemoryBlock **blockHead, size_t objectSize);
    void totemMemoryBlock_Cleanup(totemMemoryBlock **blockHead);

    typedef struct totemHashMapEntry
    {
        const void *Key;
        size_t KeyLen;
        totemHash Hash;
        totemHashValue Value;
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

    void totemHashMap_Init(totemHashMap *hashmap);
    void totemHashMap_Reset(totemHashMap *hashmap);
    void totemHashMap_Cleanup(totemHashMap *hashmap);
    totemBool totemHashMap_TakeFrom(totemHashMap *hashmap, totemHashMap *from);
    totemBool totemHashMap_Insert(totemHashMap *hashmap, const void *key, size_t KeyLen, totemHashValue Value);
    totemBool totemHashMap_InsertPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue value, totemHash hash);
    totemBool totemHashMap_InsertPrecomputedWithoutSearch(totemHashMap *hashmap, const void *key, size_t keyLen, totemHashValue value, totemHash hash);
    totemHashMapEntry *totemHashMap_Remove(totemHashMap *hashmap, const void *key, size_t KeyLen);
    totemHashMapEntry *totemHashMap_RemovePrecomputed(totemHashMap *hashmap, const void *key, size_t KeyLen, totemHash hash);
    totemHashMapEntry *totemHashMap_Find(totemHashMap *hashmap, const void *key, size_t keyLen);
    totemHashMapEntry *totemHashMap_FindPrecomputed(totemHashMap *hashmap, const void *key, size_t keyLen, totemHash hash);

    void totem_Init();
    void totem_InitMemory();

    void totem_Cleanup();
    void totem_CleanupMemory();

    FILE *totem_fopen(const char *str, const char *mode);
    totemBool totem_fchdir(FILE *file);

#ifdef TOTEM_DEBUG
#define totem_assert(a) assert(a)
#else
#define totem_assert(a)
#endif

#ifdef __cplusplus
}
#endif

#endif
