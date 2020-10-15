//
//  util.c
//  TotemScript
//
//  Created by Timothy Smale on 22/12/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <TotemScript/parse.h>
#include <TotemScript/exec.h>
#include <TotemScript/dispatch.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>

static totemHashCb hashCb = NULL;

void totem_SetGlobalCallbacks(totemHashCb newHashCb)
{
    hashCb = newHashCb;
}

totemHashValue totem_Hash(const void *data, size_t len)
{
    if(hashCb)
    {
        return hashCb(data, len);
    }
    
    const char *ptr = data;
    
    totemHashValue hash = 5831;
	for (size_t i = 0; i < len; ++i)
    {
        hash = 33 * hash + ptr[i];
    }
    
    return hash;
}

void totemString_FromLiteral(totemString *strOut, const char *str)
{
    strOut->Value = str;
    strOut->Length = (uint32_t)strlen(str);
}

totemBool totemString_Equals(totemString *a, totemString *b)
{
    if(a->Length != b->Length)
    {
        return totemBool_False;
    }
    
    return strncmp(a->Value, b->Value, a->Length) == 0;
}

totemBool totemString_Is(totemString *a, const char *str)
{
	return strncmp(a->Value, str, a->Length);
}

const char *totemOperationType_Describe(totemOperationType op)
{
    switch (op)
    {
#define TOTEM_STRINGIFY_OPCODE(op, type, flags) TOTEM_STRINGIFY_CASE(totemOperationType_ ## op);
		TOTEM_EMIT_OPCODES(TOTEM_STRINGIFY_OPCODE)
#undef TOTEM_STRINGIFY_OPCODE
    }
    
    return "UNKNOWN";
}

const char *totemPublicDataType_Describe(totemPublicDataType type)
{
    switch(type)
    {
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Float);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Int);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_String);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Array);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Type);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Function);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Coroutine);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Object);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Userdata);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Null);
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Boolean);
        default: return "UNKNOWN";
    }
}

const char *totemPrivateDataType_Describe(totemPrivateDataType type)
{
    switch(type)
    {
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Int);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Type);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Array);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Float);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_NativeFunction);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_InstanceFunction);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_MiniString);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_InternedString);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Coroutine);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Object);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Userdata);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Null);
            TOTEM_STRINGIFY_CASE(totemPrivateDataType_Boolean);
        default: return "UNKNOWN";
    }
}

totemInstructionType totemOperationType_GetInstructionType(totemOperationType op)
{
	switch (op)
	{
#define TOTEM_CASEIFY_OPCODE_TYPE(op, type, flags) case totemOperationType_ ## op: return totemInstructionType_ ## type;
		TOTEM_EMIT_OPCODES(TOTEM_CASEIFY_OPCODE_TYPE)
#undef TOTEM_CASEIFY_OPCODE_TYPE
	}

	return 0;
}

void totemInstruction_PrintList(FILE *file, totemInstruction *instructions, size_t num)
{
    for(size_t i = 0; i < num; ++i)
    {
        totemInstruction_Print(file, instructions[i]);
    }
}

void totemInstruction_Print(FILE *file, totemInstruction instruction)
{
    totemOperationType type = TOTEM_INSTRUCTION_GET_OP(instruction);
    totemInstructionType instType = totemOperationType_GetInstructionType(type);
    
    switch(instType)
    {
        case totemInstructionType_Abc1:
            totemInstruction_PrintAbc1Instruction(file, instruction);
            break;

		case totemInstructionType_Abc1u:
			totemInstruction_PrintAbc1uInstruction(file, instruction);
			break;
            
        case totemInstructionType_Abc:
            totemInstruction_PrintAbcInstruction(file, instruction);
            break;
            
        case totemInstructionType_Ab2:
			totemInstruction_PrintAb2Instruction(file, instruction);
			break;

		case totemInstructionType_Ab2u:
        case totemInstructionType_Ab2ug:
			totemInstruction_PrintAb2uInstruction(file, instruction);
			break;

		case totemInstructionType_Ab:
			totemInstruction_PrintAbInstruction(file, instruction);
			break;
            
        case totemInstructionType_A3:
			totemInstruction_PrintA3Instruction(file, instruction);
			break;

		case totemInstructionType_A3u:
			totemInstruction_PrintA3uInstruction(file, instruction);
			break;

		case totemInstructionType_A:
			totemInstruction_PrintAInstruction(file, instruction);
			break;
    }
}

char totemOperandType_GetChar(totemOperandType type)
{
    return type == totemOperandType_GlobalRegister ? 'g' : 'l';
}

void totemInstruction_PrintAbc1uInstruction(FILE *file, totemInstruction instruction)
{
	fprintf(file, "%08x %s a:%c%d b:%c%d c:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
		TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
		totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
		TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction),
		TOTEM_INSTRUCTION_GET_C1U(instruction));
}

void totemInstruction_PrintAbc1Instruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%c%d b:%c%d c:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction),
            TOTEM_INSTRUCTION_GET_C1(instruction));
}

void totemInstruction_PrintAbcInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%c%d b:%c%d c:%c%d\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction));
}

void totemInstruction_PrintAb2uInstruction(FILE *file, totemInstruction instruction)
{
	fprintf(file, "%08x %s a:%c%d bx:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
		TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
		TOTEM_INSTRUCTION_GET_B2U(instruction));
}

void totemInstruction_PrintAb2Instruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%c%d bx:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            TOTEM_INSTRUCTION_GET_B2(instruction));
}

void totemInstruction_PrintAbInstruction(FILE *file, totemInstruction instruction)
{
	fprintf(file, "%08x %s a:%c%d b:%c%d\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
		TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
		totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
		TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction));
}

void totemInstruction_PrintA3uInstruction(FILE *file, totemInstruction instruction)
{
	fprintf(file, "%08x %s ax:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A3U(instruction));
}

void totemInstruction_PrintA3Instruction(FILE *file, totemInstruction instruction)
{
	fprintf(file, "%08x %s ax:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A3(instruction));
}

void totemInstruction_PrintAInstruction(FILE *file, totemInstruction instruction)
{
	fprintf(file, "%08x %s a:%c%d\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
		TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction));
}

void totemInstruction_PrintBits(FILE *file, totemInstruction instruction)
{
    totemOperationType type = TOTEM_INSTRUCTION_GET_OP(instruction);
    totemInstructionType instType = totemOperationType_GetInstructionType(type);
    
    switch(instType)
    {
		case totemInstructionType_Ab:
        case totemInstructionType_Abc:
        case totemInstructionType_Abc1:
		case totemInstructionType_Abc1u:
            totemInstruction_PrintAbcBits(file, instruction);
            break;
        
		case totemInstructionType_A:
        case totemInstructionType_Ab2:
		case totemInstructionType_Ab2u:
        case totemInstructionType_Ab2ug:
            totemInstruction_PrintAb2Bits(file, instruction);
            break;
            
        case totemInstructionType_A3:
		case totemInstructionType_A3u:
            totemInstruction_PrintA3Bits(file, instruction);
            break;
    }
}

void totemInstruction_PrintAbcBits(FILE *file, totemInstruction instruction)
{
    totem_printBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_A, totemInstructionStart_A);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_B, totemInstructionStart_B);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_C, totemInstructionStart_C);
    fprintf(file, "\n");
}

void totemInstruction_PrintAb2Bits(FILE *file, totemInstruction instruction)
{
    totem_printBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_A, totemInstructionStart_A);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_B2, totemInstructionStart_B);
    fprintf(file, "\n");
}

void totemInstruction_PrintA3Bits(FILE *file, totemInstruction instruction)
{
    totem_printBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_A3, totemInstructionStart_A);
    fprintf(file, "\n");
}

totemStringLength totemMiniString_GetLength(char *c)
{
    for(size_t i = 0; i < TOTEM_MINISTRING_MAXLENGTH; i++)
    {
        if(!c[i])
        {
            return i;
        }
    }
    
    return TOTEM_MINISTRING_MAXLENGTH;
}

void totem_printBits(FILE *file, uint64_t data, uint64_t numBits, uint64_t start)
{
    uint64_t end = start + numBits;
    for(uint64_t i = end; i > start; i--)
    {
        fprintf(file, "%"PRIu64, TOTEM_GETBITS_OFFSET(data, TOTEM_BITMASK(uint64_t, i - 1, 1), i - 1));
    }
}

totemOperandXSigned totemOperandXSigned_FromUnsigned(totemOperandXUnsigned val, uint32_t signedMask, uint32_t valueMask)
{
    if (TOTEM_GETBITS(val, signedMask))
    {
        uint32_t actualVal = TOTEM_GETBITS(val, valueMask);
        actualVal = ~actualVal;
        actualVal++;
        return *((totemOperandXSigned*)(&actualVal));
    }
    
    return val;
}

totemBool totem_getcwd(char *buffer, size_t size)
{
    return getcwd(buffer, (totemCwdSize_t)size) == buffer;
}

void totem_Init(void)
{
    totem_InitMemory();
}

void totem_Cleanup(void)
{
    totem_CleanupMemory();
}

FILE *totem_fopen(const char *path, const char *mode)
{
#ifdef TOTEM_MSC
    FILE *result = NULL;
    if(fopen_s(&result, path, mode) == 0)
    {
        return result;
    }
    
    return NULL;
#else
    return fopen(path, mode);
#endif
}

totemBool totem_fchdir(FILE *file)
{
#ifdef TOTEM_WIN
    HANDLE handle = (HANDLE)_get_osfhandle(file->_file);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return totemBool_False;
    }
    
    CHAR buffer[PATH_MAX];
    LPSTR filename = buffer;
    
    DWORD length = GetFinalPathNameByHandleA(handle, filename, TOTEM_ARRAY_SIZE(buffer), VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
    if (length < TOTEM_ARRAY_SIZE(buffer))
    {
        // skip \\?\ prefix
        LPSTR toSkip = "\\\\?\\";
        size_t toSkipLen = strlen(toSkip);
        if (strstr(filename, toSkip) == filename)
        {
            filename += toSkipLen;
        }
        
        size_t len = strlen(filename);
        
        for (CHAR *c = filename + len - 1; c >= filename; c--)
        {
            if(c[0] == '/' || c[0] == '\\')
            {
                *c = 0;
                break;
            }
        }
        
        if(totem_chdir(filename) == 0)
        {
            return totemBool_True;
        }
    }
    
    return totemBool_False;
#endif
#ifdef TOTEM_APPLE
    int no = fileno(file);
    
    char filePath[MAXPATHLEN];
    if (fcntl(no, F_GETPATH, filePath) != -1)
    {
        size_t len = strnlen(filePath, MAXPATHLEN);
        
        for(char *c = filePath + len - 1; c >= filePath; c--)
        {
            if(c[0] == '/' || c[0] == '\\')
            {
                *c = 0;
                break;
            }
        }
        
        if(chdir(filePath) == 0)
        {
            return totemBool_True;
        }
    }
    
    return totemBool_False;
#endif
}
