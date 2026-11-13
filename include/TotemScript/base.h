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
#include <TotemScript/instructions.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOTEM_NONE
#define TOTEM_X(x) x
#define TOTEM_STRINGIFY(x) #x
#define TOTEM_MAX_VERSION 0
#define TOTEM_MIN_VERSION 2
#define TOTEM_PATCH_VERSION 0
#define TOTEM_VERSION_STRING TOTEM_STRINGIFY(TOTEM_MAX_VERSION) "." TOTEM_STRINGIFY(TOTEM_MIN_VERSION) "." TOTEM_STRINGIFY(TOTEM_PATCH_VERSION)
#define TOTEM_SIZEOF_STATIC(type, member) sizeof(((type*)0)->member)
#define TOTEM_UNUSED(x) do { (void)(x); } while((void)0,0)
#define TOTEM_SET_N(ptr, num, val) do { memset((void*)(ptr), val, sizeof((ptr)[0]) * (num)); } while((void)0,0)
#define TOTEM_SET(ptr, val) TOTEM_SET_N(ptr, 1, val)
#define TOTEM_ZERO_N(ptr, num) TOTEM_SET_N(ptr, num, 0)
#define TOTEM_ZERO(obj) do { memset((void*)(&(obj)), 0, sizeof(obj)); } while((void)0,0)
#define TOTEM_CONCAT(x, y) x ## y
#define TOTEM_ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#define TOTEM_STRING_LITERAL_SIZE(x) (TOTEM_ARRAY_SIZE(x) - 1)
#define TOTEM_STRINGIFY_CASE(x) case x: return TOTEM_STRINGIFY(x)
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
#define TOTEM_TOGGLEBITS(i, mask) do { ((i) ^= (~(mask))); } while((void)0,0)
#define TOTEM_GETBITS_OFFSET(i, mask, offset) TOTEM_GETBITS((i), (mask)) >> (offset)
#define TOTEM_SETBITS_OFFSET(i, mask, offset) TOTEM_SETBITS((i), ((mask) << (offset)))
#define TOTEM_HASBITS_OFFSET(i, mask, offset) TOTEM_HASBITS((i), ((mask) << (offset)))
#define TOTEM_TOGGLEBITS_OFFSET(i, mask, offset) TOTEM_TOGGLEBITS((i), ((mask) << (offset)))
#define TOTEM_UNSETBITS_OFFSET(i, mask, offset) TOTEM_UNSETBITS((i), ((mask) << (offset)))
#define TOTEM_FORCEBITS(i, mask) TOTEM_UNSETBITS((i), (mask)); TOTEM_SETBITS((i), (mask))
#define TOTEM_FORCEBITS_OFFSET(i, mask, offset) TOTEM_UNSETBITS_OFFSET((i), (mask), (offset)); TOTEM_SETBITS_OFFSET((i), (mask), (offset))
#define TOTEM_MAXVAL_UNSIGNED(type, numBits) ((type)(((0x1ULL << ((numBits) - 1)) - 1) | (0xFULL << ((numBits) - 4))))
#define TOTEM_MINVAL_UNSIGNED(type, numBits) ((type)0)
#define TOTEM_MAXVAL_SIGNED(type, numBits) (type)((((0x1ULL << ((numBits) - 1)) - 1) | (0x7ULL << ((numBits) - 4))))
#define TOTEM_MINVAL_SIGNED(type, numBits) ((-(TOTEM_MAXVAL_SIGNED(type, (numBits)))) - 1)
#define TOTEM_IS_POWOF2(x) (((x) & ((x) - 1)) == 0)

TOTEM_STATIC_ASSERT(TOTEM_STRING_LITERAL_SIZE("test") == 4, "String length test");
    
	enum
	{
		totemBool_True = 1,
		totemBool_False = 0
	};

    typedef int32_t totemOperandX;
    typedef uint32_t totemOperandXu;
	typedef int8_t totemOperand1;
	typedef uint8_t totemOperand1u;
	typedef int16_t totemOperand2;
	typedef uint16_t totemOperand2u;
	typedef int32_t totemOperand3;
	typedef uint32_t totemOperand3u;
	
	typedef uint64_t totemHashValue;
	typedef uintptr_t totemHashMapValue;
#define TOTEM_HASH_PRINTF PRIu64

	typedef size_t totemStringLength;

	typedef uint8_t totemBool;
	typedef char totemChar;
    
	typedef double totemFloat;

#if TOTEM_VMOPT_NANBOXING
	typedef int32_t totemInt;
#define TOTEM_INT_PRINTF PRId32
#define totem_strtoInt(x) strtol((x), NULL, 10)
#define TOTEM_INT_WIDTH 32
#else
	typedef int64_t totemInt;
#define TOTEM_INT_PRINTF PRId64
#define totem_strtoInt(x) strtol((x), NULL, 10)
#define TOTEM_INT_WIDTH 64
#endif

#define TOTEM_INT_LENGTH (TOTEM_INT_WIDTH / CHAR_BIT)

#define totem_strtoFloat strtod
#define totem_toFloat(x) ((totemFloat)(x))
#define totem_toInt(x) ((totemInt)(x))
    
    totemOperandX totemOperandX_FromUnsigned(totemOperandXu val, uint32_t signedMask, uint32_t valueMask);

#define TOTEM_BITARRAY(name, length) totemInt name [(length / TOTEM_INT_WIDTH) + 1]

    totemBool totemBitArray_Get(totemInt *arr, totemInt index);
    void totemBitArray_Set(totemInt *arr, totemInt index, totemBool val);

	typedef struct totemString
    {
        const char *Value;
        totemStringLength Length;
    }
    totemString;
    
    void totemString_FromLiteral(totemString *strOut, const char *str);
    totemBool totemString_Equals(totemString *a, totemString *b);
	totemBool totemString_Is(totemString *a, const char *str);
    
    struct totemLogOutput;
    #define totemLog struct totemLogOutput

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
        totemPublicDataType_Instance = 11,
        totemPublicDataType_Max = 12
    };

	typedef uint64_t totemPublicDataType;
    
    const char *totemPublicDataType_Describe(totemPublicDataType type);
	totemBool totemPublicDataType_IsGarbageCollected(totemPublicDataType type);
    
    enum
    {
        totemOperationBehaviourType_Return,     // will exit current stack frame
        totemOperationBehaviourType_Pure,       // does not require any input beyond operands (i.e. doesn't need totemExecState*)
        totemOperationBehaviourType_PureControl,// will modify instruction pointer with return value
        totemOperationBehaviourType_None
    };

    typedef uint32_t totemOperationBehaviourType;

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
    totemOperationBehaviourType totemOperationType_GetBehaviourFlag(totemOperationType op);
    totemBool totemOperationType_IsPure(totemOperationType op);
    
	/**
	 * 32-bit instruction size
	 */
	typedef uint32_t totemInstruction;
	void totemInstruction_PrintBits(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAbcBits(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAb2Bits(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintA3Bits(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintList(totemLog *file, totemInstruction *instructions, size_t num);
	void totemInstruction_Print(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAbc1Instruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAbc1uInstruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAbcInstruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAb2Instruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAb2uInstruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAbInstruction(totemLog *file, totemInstruction instruction);
    void totemInstruction_PrintAb1c1Instruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintA3Instruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintA3uInstruction(totemLog *file, totemInstruction instruction);
	void totemInstruction_PrintAInstruction(totemLog *file, totemInstruction instruction);

	TOTEM_STATIC_ASSERT(sizeof(totemInstruction) == 4, "Totem Instruction must be 4 bytes");

	typedef enum totemInstructionType
    {
#define TOTEM_FORMAT_INSTRUCTION_ENUM(ins) totemInstructionType_ ## ins,
        TOTEM_EMIT_INSTRUCTION_TYPES(TOTEM_FORMAT_INSTRUCTION_ENUM)
#undef EMIT_INS
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

#define TOTEM_A3_MAX TOTEM_MAXVAL_SIGNED(totemOperandX, totemInstructionSize_A3)
#define TOTEM_A3_MIN TOTEM_MINVAL_SIGNED(totemOperandX, totemInstructionSize_A3)
#define TOTEM_A3U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXu, totemInstructionSize_A3)
#define TOTEM_A3U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXu, totemInstructionSize_A3)

#define TOTEM_B2_MAX TOTEM_MAXVAL_SIGNED(totemOperandX, totemInstructionSize_B2)
#define TOTEM_B2_MIN TOTEM_MINVAL_SIGNED(totemOperandX, totemInstructionSize_B2)
#define TOTEM_B2U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXu, totemInstructionSize_B2)
#define TOTEM_B2U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXu, totemInstructionSize_B2)

#define TOTEM_B1_MAX TOTEM_MAXVAL_SIGNED(totemOperandX, totemInstructionSize_B1)
#define TOTEM_B1_MIN TOTEM_MINVAL_SIGNED(totemOperandX, totemInstructionSize_B1)
#define TOTEM_B1U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXu, totemInstructionSize_B1)
#define TOTEM_B1U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXu, totemInstructionSize_B1)

#define TOTEM_C1_MAX TOTEM_MAXVAL_SIGNED(totemOperandX, totemInstructionSize_C1)
#define TOTEM_C1_MIN TOTEM_MINVAL_SIGNED(totemOperandX, totemInstructionSize_C1)
#define TOTEM_C1U_MAX TOTEM_MAXVAL_UNSIGNED(totemOperandXu, totemInstructionSize_C1)
#define TOTEM_C1U_MIN TOTEM_MINVAL_UNSIGNED(totemOperandXu, totemInstructionSize_C1)

#define TOTEM_INT_MIN TOTEM_MINVAL_SIGNED(totemInt, TOTEM_NUMBITS(totemInt))
#define TOTEM_INT_MAX TOTEM_MAXVAL_SIGNED(totemInt, TOTEM_NUMBITS(totemInt))
    
#define TOTEM_MAX_NATIVEFUNCTIONS TOTEM_B2U_MAX
#define TOTEM_MAX_SCRIPTFUNCTIONS TOTEM_B2U_MAX
#define TOTEM_MAX_LOCAL_REGISTERS TOTEM_MAXVAL_UNSIGNED(totemOperandXu, totemInstructionSize_A)
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
    
#define TOTEM_INSTRUCTION_MASK_A TOTEM_BITMASK(totemInstruction, totemInstructionStart_A, totemInstructionSize_A)
#define TOTEM_INSTRUCTION_MASK_B TOTEM_BITMASK(totemInstruction, totemInstructionStart_B, totemInstructionSize_B)
#define TOTEM_INSTRUCTION_MASK_C TOTEM_BITMASK(totemInstruction, totemInstructionStart_C, totemInstructionSize_C)
    
#define TOTEM_INSTRUCTION_MASK_OP TOTEM_BITMASK(totemInstruction, totemInstructionStart_Op, totemInstructionSize_Op)
#define TOTEM_INSTRUCTION_MASK_A3 TOTEM_BITMASK(totemInstruction, totemInstructionStart_A, totemInstructionSize_A3)
#define TOTEM_INSTRUCTION_MASK_B2 TOTEM_BITMASK(totemInstruction, totemInstructionStart_B, totemInstructionSize_B2)
#define TOTEM_INSTRUCTION_MASK_B1 TOTEM_BITMASK(totemInstruction, totemInstructionStart_B, totemInstructionSize_B1)
#define TOTEM_INSTRUCTION_MASK_C1 TOTEM_BITMASK(totemInstruction, totemInstructionStart_C, totemInstructionSize_C1)
    
#define TOTEM_INSTRUCTION_GET_OP(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_OP, totemInstructionStart_Op)

#define TOTEM_INSTRUCTION_GET_A3U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_A3, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_A3(ins) \
	totemOperandX_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_A3, totemInstructionStart_A), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_A3 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_A3 - 1))
    
#define TOTEM_INSTRUCTION_GET_B2U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B2, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_B2(ins) \
	totemOperandX_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B2, totemInstructionStart_B), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_B2 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_B2 - 1))

#define TOTEM_INSTRUCTION_GET_B1U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B1, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_B1(ins) \
	totemOperandX_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B1, totemInstructionStart_B), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_B1 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_B1 - 1))
    
#define TOTEM_INSTRUCTION_GET_C1U(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_C1, totemInstructionStart_C)
#define TOTEM_INSTRUCTION_GET_C1(ins)  \
	totemOperandX_FromUnsigned( \
		TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_C1, totemInstructionStart_C), \
		TOTEM_BITMASK(totemInstruction, totemInstructionSize_C1 - 1, 1), \
		TOTEM_BITMASK(totemInstruction, 0, totemInstructionSize_C1 - 1))
    
#define TOTEM_INSTRUCTION_GET_A(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_A, totemInstructionStart_A)
#define TOTEM_INSTRUCTION_GET_B(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_B, totemInstructionStart_B)
#define TOTEM_INSTRUCTION_GET_C(ins) TOTEM_GETBITS_OFFSET(ins, TOTEM_INSTRUCTION_MASK_C, totemInstructionStart_C)
    
    totemBool totem_getcwd(char *buffer, size_t size);
    
	totemHashValue totem_HashBegin(void);
	totemHashValue totem_HashChar(char c, totemHashValue hash);
    totemHashValue totem_HashStep(const void *data, size_t len, totemHashValue hash);
    totemHashValue totem_Hash(const void *data, size_t len);
	
    void totem_Init(void);
	void totem_Cleanup(void);

	void totem_Fatal(void);
    
    FILE *totem_fopen(const char *str, const char *mode);
    totemBool totem_fchdir(FILE *file);
	
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

	enum
	{
#define EMIT(op, type, behaviour) totemOperationInstructionType_ ## op = totemInstructionType_ ## type,
		TOTEM_EMIT_OPCODES(EMIT)
#undef EMIT
	};

#ifdef __cplusplus
}
#endif

#endif
