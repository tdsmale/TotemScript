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
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif
    
#define TOTEM_SIZEOF_STATIC(type, member) sizeof(((type*)0)->member)
#define TOTEM_UNUSED(x) do { (void)(x); } while((void)0,0)
#define TOTEM_ZERO_ARRAY(ptr, num) do { memset((void*)(ptr), 0, sizeof((ptr)[0]) * (num)); } while((void)0,0)
#define TOTEM_ZERO(ptr) TOTEM_ZERO_ARRAY(ptr, 1)
#define TOTEM_CONCAT(x, y) x ## y
#define TOTEM_ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#define TOTEM_STRING_LITERAL_SIZE(x) (TOTEM_ARRAY_SIZE(x) - 1)
#define TOTEM_STRINGIFY_CASE(x) case x: return #x
#define TOTEM_STATIC_ASSERT_NAME(x, y) TOTEM_CONCAT(x, y)
#define TOTEM_STATIC_ASSERT(test, explanation) extern void TOTEM_STATIC_ASSERT_NAME(__assert, __LINE__)(int a[(test) ? 1 : -1])
#define TOTEM_NUMBITS(type) (sizeof(type) * CHAR_BIT)
#define TOTEM_BITCAST(type, x) (*((type*)(&(x))))
#define TOTEM_BITMASK(type, start, length) (((((type)1) << (length)) - 1) << (start))
#define TOTEM_GETBITS(i, mask) ((i) & (mask))
#define TOTEM_HASBITS(i, mask) ((TOTEM_GETBITS((i), (mask))) == (mask))
#define TOTEM_NHASBITS(i, mask) ((TOTEM_GETBITS((i), (mask))) != (mask))
#define TOTEM_HASANYBITS(i, mask) ((TOTEM_GETBITS((i), (mask))) != 0)
#define TOTEM_SETBITS(i, mask) do { ((i) |= (mask)); } while((void)0,0)
#define TOTEM_UNSETBITS(i, mask) do { ((i) &= (~(mask))); } while((void)0,0)
#define TOTEM_GETBITS_OFFSET(i, mask, offset) TOTEM_GETBITS((i), (mask)) >> (offset)
#define TOTEM_SETBITS_OFFSET(i, mask, offset) TOTEM_SETBITS((i), ((mask) << (offset)))
#define TOTEM_UNSETBITS_OFFSET(i, mask, offset) TOTEM_UNSETBITS((i), ((mask) << (offset)))
#define TOTEM_FORCEBITS(i, mask) TOTEM_UNSETBITS((i), (mask)); TOTEM_SETBITS((i), (mask))
#define TOTEM_FORCEBITS_OFFSET(i, mask, offset) TOTEM_UNSETBITS_OFFSET((i), (mask), (offset)); TOTEM_SETBITS_OFFSET((i), (mask), (offset))
#define TOTEM_MAXVAL_UNSIGNED(type, numBits) ((type)(((0x1ULL << ((numBits) - 1)) - 1) | (0xFULL << ((numBits) - 4))))
#define TOTEM_MINVAL_UNSIGNED(type, numBits) ((type)0)
#define TOTEM_MAXVAL_SIGNED(type, numBits) (type)((((0x1ULL << ((numBits) - 1)) - 1) | (0x7ULL << ((numBits) - 4))))
#define TOTEM_MINVAL_SIGNED(type, numBits) ((-(TOTEM_MAXVAL_SIGNED(type, (numBits)))) - 1)

TOTEM_STATIC_ASSERT(TOTEM_STRING_LITERAL_SIZE("test") == 4, "String length test");
    
	typedef enum totemBool
    {
        totemBool_True = 1,
        totemBool_False = 0
    }
    totemBool;
    
	typedef enum totemReturnFlag
    {
        totemReturnFlag_None = 0,
        totemReturnFlag_Register = 1,
        totemReturnFlag_Last = 2
    }
    totemReturnFlag;
    
    typedef int32_t totemOperandXSigned;
    typedef uint32_t totemOperandXUnsigned;
	typedef size_t totemHashValue;
	typedef uintptr_t totemHashMapValue;
	typedef size_t totemStringLength;
	typedef char totemChar;
    
#if TOTEM_VMOPT_NANBOXING || defined(TOTEM_32)
    typedef int32_t totemInt;
#define TOTEM_INT_PRINTF PRIi32
#define totem_strtoInt strtol
#else
    typedef int64_t totemInt;
#define TOTEM_INT_PRINTF PRIi64
#define totem_strtoInt strtoll
#endif

#if defined(TOTEM_64)
	typedef double totemFloat;
#define totem_strtoFloat strtod
#elif defined(TOTEM_32)
	typedef float totemFloat;
#define totem_strtoFloat strtof
#endif
    
    totemOperandXSigned totemOperandXSigned_FromUnsigned(totemOperandXUnsigned val, uint32_t signedMask, uint32_t valueMask);
    
	typedef struct totemString
    {
        const char *Value;
        totemStringLength Length;
    }
    totemString;
    
    void totemString_FromLiteral(totemString *strOut, const char *str);
    totemBool totemString_Equals(totemString *a, totemString *b);
	totemBool totemString_Is(totemString *a, const char *str);
    
	typedef enum totemFunctionType
    {
        totemFunctionType_Instance = 0,
        totemFunctionType_Native = 1
	} 
	totemFunctionType;
    
    enum
    {
        totemPublicDataType_Null = 0,
        totemPublicDataType_Int = 1,
        totemPublicDataType_Float = 2,
        totemPublicDataType_String = 3,
		totemPublicDataType_Type = 4,
		totemPublicDataType_Function = 5,
		totemPublicDataType_Boolean = 6,
        totemPublicDataType_Array = 7,
        totemPublicDataType_Coroutine = 8,
        totemPublicDataType_Object = 9,
        totemPublicDataType_Userdata = 10,
        totemPublicDataType_Max = 11
    };

#if defined(TOTEM_64)
	typedef uint64_t totemPublicDataType;
#elif defined(TOTEM_32)
	typedef uint32_t totemPublicDataType;
#endif
    
    const char *totemPublicDataType_Describe(totemPublicDataType type);
	totemBool totemPublicDataType_IsGarbageCollected(totemPublicDataType type);
    
    /**
     * Operation Types
     */
    enum
    {
#define TOTEM_OPCODE_FORMAT_ENUM(op, type, flags) totemOperationType_ ## op,
		TOTEM_EMIT_OPCODES(TOTEM_OPCODE_FORMAT_ENUM)
#undef TOTEM_OPCODE_FORMAT_ENUM
        TOTEM_NUM_OPTYPES
    };
    typedef uint8_t totemOperationType;
    const char *totemOperationType_Describe(totemOperationType op);
    
	/**
	 * 32-bit instruction size
	 */
	typedef uint32_t totemInstruction;
	void totemInstruction_PrintBits(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAbcBits(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAb2Bits(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintA3Bits(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintList(FILE *file, totemInstruction *instructions, size_t num);
	void totemInstruction_Print(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAbc1Instruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAbc1uInstruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAbcInstruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAb2Instruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAb2uInstruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAbInstruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintA3Instruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintA3uInstruction(FILE *file, totemInstruction instruction);
	void totemInstruction_PrintAInstruction(FILE *file, totemInstruction instruction);

	TOTEM_STATIC_ASSERT(sizeof(totemInstruction) == 4, "Totem Instruction must be 4 bytes");

    /**
     * Instruction Formats
	 */
	typedef enum totemInstructionType
    {
		totemInstructionType_A,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                                       31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Unused/Available                         | AInstruction
		-----------------------------------------------------------------------
		*/	
		totemInstructionType_A3,
		/*
		-----------------------------------------------------------------------
		|0     |                                                            31|
		-----------------------------------------------------------------------
		|OpType|A3 (Signed Int)                                               | AxInstruction
		-----------------------------------------------------------------------
		*/
		totemInstructionType_A3u,
		/*
		-----------------------------------------------------------------------
		|0     |                                                            31|
		-----------------------------------------------------------------------
		|OpType|Axu (Unsigned Int)                                            | AxuInstruction
		-----------------------------------------------------------------------
		*/
		totemInstructionType_Ab,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                    |                  31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Register B          |Unused/Available    | AbInstruction
		-----------------------------------------------------------------------
		*/
		totemInstructionType_Ab2,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                                       31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Bx (Signed Int)                          | AbxInstruction
		-----------------------------------------------------------------------
		*/
		totemInstructionType_Ab2u,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                                       31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Bxu (Unsigned Int)                       | AbxuInstruction
		-----------------------------------------------------------------------
		*/
		totemInstructionType_Ab2ug,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                                       31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Bxu (Global Register Index)              | AbxugInstruction
		-----------------------------------------------------------------------
		*/
		totemInstructionType_Ab1c1,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                    |                  31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Bxx (Signed Int)    |Cxx (Signed Int)    | AbxxcxxInstruction
		-----------------------------------------------------------------------
		*/
        totemInstructionType_Abc,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                    |                  31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Register B          |Register C          | AbcInstruction
		-----------------------------------------------------------------------
		*/
        totemInstructionType_Abc1,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                    |                  31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Register B          |Cxx (Signed Int)    | AbcxxInstruction
		-----------------------------------------------------------------------
		*/
		totemInstructionType_Abc1u,
		/*
		-----------------------------------------------------------------------
		|0     |                    |                    |                  31|
		-----------------------------------------------------------------------
		|OpType|Register A          |Register B          |Cxxu (Unsigned Int) | AbcxxuInstruction
		-----------------------------------------------------------------------
		*/
    }
    totemInstructionType;
    totemInstructionType totemOperationType_GetInstructionType(totemOperationType op);
    
	enum
	{
		totemInstructionSize_Op = 8,
		totemInstructionSize_A = 8,
		totemInstructionSize_A3 = 24,
		totemInstructionSize_B = 8,
		totemInstructionSize_B2 = 16,
		totemInstructionSize_B1 = 8,
		totemInstructionSize_C = 8,
		totemInstructionSize_C1 = 8
	};

	enum
	{
		totemInstructionStart_Op = 0,
		totemInstructionStart_A = 8,
		totemInstructionStart_B = 16,
		totemInstructionStart_C = 24
	};

	/*
     Register Instruction Operands
     ---------------------------
     |0        1|2            9|
     ---------------------------
     |Is Global?|Register Index|
     ---------------------------
     |1         |8             |
     ---------------------------
     first bit indicates if it is a global register or not
     next eight bits are the register index
     */
	typedef enum totemOperandSize
	{
#if TOTEM_VMOPT_GLOBAL_OPERANDS
		totemOperandSize_RegisterType = 1,
		totemOperandSize_RegisterIndex = 7
#else
		totemOperandSize_RegisterType = 0,
		totemOperandSize_RegisterIndex = 8
#endif
	}
	totemOperandSize;
    
#define TOTEM_A3_MAX TOTEM_MAXVAL_SIGNED(totemOperandXSigned, totemInstructionSize_A3)
#define TOTEM_A3_MIN TOTEM_MINVAL_SIGNED(totemOperandXSigned, totemInstructionSize_A3)
#define TOTEM_A3U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_A3)
#define TOTEM_A3U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_A3)

#define TOTEM_B2_MAX TOTEM_MAXVAL_SIGNED(totemOperandXSigned, totemInstructionSize_B2)
#define TOTEM_B2_MIN TOTEM_MINVAL_SIGNED(totemOperandXSigned, totemInstructionSize_B2)
#define TOTEM_B2U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_B2)
#define TOTEM_B2U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_B2)

#define TOTEM_B1_MAX TOTEM_MAXVAL_SIGNED(totemOperandXSigned, totemInstructionSize_B1)
#define TOTEM_B1_MIN TOTEM_MINVAL_SIGNED(totemOperandXSigned, totemInstructionSize_B1)
#define TOTEM_B1U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_B1)
#define TOTEM_B1U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_B1)

#define TOTEM_C1_MAX TOTEM_MAXVAL_SIGNED(totemOperandXSigned, totemInstructionSize_C1)
#define TOTEM_C1_MIN TOTEM_MINVAL_SIGNED(totemOperandXSigned, totemInstructionSize_C1)
#define TOTEM_C1U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_C1)
#define TOTEM_C1U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXUnsigned, totemInstructionSize_C1)

#define TOTEM_INT_MIN TOTEM_MINVAL_SIGNED(totemInt, TOTEM_NUMBITS(totemInt))
#define TOTEM_INT_MAX TOTEM_MAXVAL_SIGNED(totemInt, TOTEM_NUMBITS(totemInt))
    
#define TOTEM_MAX_NATIVEFUNCTIONS TOTEM_B2U_MAX
#define TOTEM_MAX_SCRIPTFUNCTIONS TOTEM_B2U_MAX
#define TOTEM_MAX_LOCAL_REGISTERS TOTEM_MAXVAL_UNSIGNED(totemOperandXUnsigned, totemOperandSize_RegisterIndex)
#define TOTEM_MAX_GLOBAL_REGISTERS TOTEM_INT_MAX
#define TOTEM_MAX_OPTYPES TOTEM_MAXVAL_UNSIGNED(size_t, totemInstructionSize_Op)
#define TOTEM_MAX_INSTRUCTIONS TOTEM_B2U_MAX

	TOTEM_STATIC_ASSERT(TOTEM_NUM_OPTYPES < TOTEM_MAX_OPTYPES, "Too many opcodes defined!");
    
	typedef enum totemOperandType
    {
        totemOperandType_LocalRegister = 0,
        totemOperandType_GlobalRegister = 1
    }
    totemOperandType;
    
#define TOTEM_INSTRUCTION_MASK_REGISTERA TOTEM_BITMASK(totemInstruction, totemInstructionStart_A, totemInstructionSize_A)
#define TOTEM_INSTRUCTION_MASK_REGISTERB TOTEM_BITMASK(totemInstruction, totemInstructionStart_B, totemInstructionSize_B)
#define TOTEM_INSTRUCTION_MASK_REGISTERC TOTEM_BITMASK(totemInstruction, totemInstructionStart_C, totemInstructionSize_C)
    
#define TOTEM_INSTRUCTION_MASK_OP TOTEM_BITMASK(totemInstruction, totemInstructionStart_Op, totemInstructionSize_Op)
#define TOTEM_INSTRUCTION_MASK_A3 TOTEM_BITMASK(totemInstruction, totemInstructionStart_A, totemInstructionSize_A3)
#define TOTEM_INSTRUCTION_MASK_B2 TOTEM_BITMASK(totemInstruction, totemInstructionStart_B, totemInstructionSize_B2)
#define TOTEM_INSTRUCTION_MASK_B1 TOTEM_BITMASK(totemInstruction, totemInstructionStart_B, totemInstructionSize_B1)
#define TOTEM_INSTRUCTION_MASK_C1 TOTEM_BITMASK(totemInstruction, totemInstructionStart_C, totemInstructionSize_C1)
    
#define TOTEM_INSTRUCTION_GET_OP(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_OP, totemInstructionStart_Op)

#define TOTEM_INSTRUCTION_GET_A3U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_A3, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_A3(ins) \
	totemOperandXSigned_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_A3, totemInstructionStart_A), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_A3 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_A3 - 1))
    
#define TOTEM_INSTRUCTION_GET_B2U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B2, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_B2(ins) \
	totemOperandXSigned_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B2, totemInstructionStart_B), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_B2 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_B2 - 1))

#define TOTEM_INSTRUCTION_GET_B1U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B1, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_B1(ins) \
	totemOperandXSigned_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B1, totemInstructionStart_B), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_B1 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_B1 - 1))
    
#define TOTEM_INSTRUCTION_GET_C1U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_C1, totemInstructionStart_C)
#define TOTEM_INSTRUCTION_GET_C1(ins)  \
	totemOperandXSigned_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_C1, totemInstructionStart_C), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_C1 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_C1 - 1))
    
#define TOTEM_INSTRUCTION_GET_REGISTERA(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERA, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_REGISTERB(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERB, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_REGISTERC(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_REGISTERC, totemInstructionStart_C)
    
#if TOTEM_VMOPT_GLOBAL_OPERANDS
#define TOTEM_INSTRUCTION_MASK_REGISTERA_SCOPE TOTEM_BITMASK(totemInstruction, totemInstructionStart_A, 1)
#define TOTEM_INSTRUCTION_MASK_REGISTERA_INDEX TOTEM_BITMASK(totemInstruction, totemInstructionStart_A + 1, totemInstructionSize_A - 1)

#define TOTEM_INSTRUCTION_MASK_REGISTERB_SCOPE TOTEM_BITMASK(totemInstruction, totemInstructionStart_B, 1)
#define TOTEM_INSTRUCTION_MASK_REGISTERB_INDEX TOTEM_BITMASK(totemInstruction, totemInstructionStart_B + 1, totemInstructionSize_B - 1)

#define TOTEM_INSTRUCTION_MASK_REGISTERC_SCOPE TOTEM_BITMASK(totemInstruction, totemInstructionStart_C, 1)
#define TOTEM_INSTRUCTION_MASK_REGISTERC_INDEX TOTEM_BITMASK(totemInstruction, totemInstructionStart_C + 1, totemInstructionSize_C - 1)

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
    
    totemBool totem_getcwd(char *buffer, size_t size);
    
    totemHashValue totem_Hash(const void *data, size_t len);
	typedef uint32_t(*totemHashCb)(const void*, size_t);
    void totem_SetHashCallback(totemHashCb hash);
    
    void totem_Init(void);
    void totem_Cleanup(void);
    
    FILE *totem_fopen(const char *str, const char *mode);
    totemBool totem_fchdir(FILE *file);
    
#ifdef TOTEM_DEBUG
#define TOTEM_DEBUG_ASSERT(a) assert(a)
#else
#define TOTEM_DEBUG_ASSERT(a) TOTEM_UNUSED(a)
#endif
	
#ifdef __cplusplus
	typedef struct { int _a; } totemJmpBuffer;
#define TOTEM_JMP_TRY(jmp) try
#define TOTEM_JMP_CATCH(jmp) catch(totemJmpBuffer *_jmp)
#define TOTEM_JMP_THROW(jmp) throw jmp
#else
	typedef struct { jmp_buf buf; } totemJmpBuffer;
#define TOTEM_JMP_TRY(jmp) if (!totem_setjmp((jmp)->buf))
#define TOTEM_JMP_CATCH(jmp) else
#define TOTEM_JMP_THROW(jmp) totem_longjmp((jmp)->buf); TOTEM_UNREACHABLE()
#endif
    
#ifdef __cplusplus
}
#endif

#endif
