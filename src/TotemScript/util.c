//
//  util.c
//  TotemScript
//
//  Created by Timothy Smale on 22/12/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <TotemScript/parse.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static totemHashCb hashCb = NULL;

void totem_SetGlobalCallbacks(totemHashCb newHashCb)
{
    hashCb = newHashCb;
}

uint32_t totem_Hash(const char *data, size_t len)
{
    if(hashCb)
    {
        return hashCb(data, len);
    }
    
    uint32_t hash = 5831;
    for(uint32_t i = 0; i < len; ++i)
    {
        hash = 33 * hash + data[i];
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

const char *totemOperationType_Describe(totemOperationType op)
{
    switch (op)
    {
        TOTEM_STRINGIFY_CASE(totemOperationType_Add);
        TOTEM_STRINGIFY_CASE(totemOperationType_ConditionalGoto);
        TOTEM_STRINGIFY_CASE(totemOperationType_Divide);
        TOTEM_STRINGIFY_CASE(totemOperationType_Equals);
        TOTEM_STRINGIFY_CASE(totemOperationType_FunctionArg);
        TOTEM_STRINGIFY_CASE(totemOperationType_Goto);
        TOTEM_STRINGIFY_CASE(totemOperationType_LessThan);
        TOTEM_STRINGIFY_CASE(totemOperationType_LessThanEquals);
        TOTEM_STRINGIFY_CASE(totemOperationType_LogicalAnd);
        TOTEM_STRINGIFY_CASE(totemOperationType_LogicalOr);
        TOTEM_STRINGIFY_CASE(totemOperationType_MoreThan);
        TOTEM_STRINGIFY_CASE(totemOperationType_MoreThanEquals);
        TOTEM_STRINGIFY_CASE(totemOperationType_Move);
        TOTEM_STRINGIFY_CASE(totemOperationType_Multiply);
        TOTEM_STRINGIFY_CASE(totemOperationType_NativeFunction);
        TOTEM_STRINGIFY_CASE(totemOperationType_None);
        TOTEM_STRINGIFY_CASE(totemOperationType_NotEquals);
        TOTEM_STRINGIFY_CASE(totemOperationType_Power);
        TOTEM_STRINGIFY_CASE(totemOperationType_Return);
        TOTEM_STRINGIFY_CASE(totemOperationType_ScriptFunction);
        TOTEM_STRINGIFY_CASE(totemOperationType_Subtract);
        TOTEM_STRINGIFY_CASE(totemOperationType_ArrayGet);
        TOTEM_STRINGIFY_CASE(totemOperationType_ArraySet);
        TOTEM_STRINGIFY_CASE(totemOperationType_NewArray);
    }
    
    return "UNKNOWN";
}

const char *totemDataType_Describe(totemDataType type)
{
    switch(type)
    {
        TOTEM_STRINGIFY_CASE(totemDataType_Null);
        TOTEM_STRINGIFY_CASE(totemDataType_Float);
        TOTEM_STRINGIFY_CASE(totemDataType_Int);
        TOTEM_STRINGIFY_CASE(totemDataType_InternedString);
    }
    
    return "UNKNOWN";
}

totemInstructionType totemOperationType_GetInstructionType(totemOperationType op)
{
    switch(op)
    {
        case totemOperationType_ConditionalGoto:
        case totemOperationType_FunctionArg:
        case totemOperationType_NativeFunction:
        case totemOperationType_ScriptFunction:
        case totemOperationType_Return:
            return totemInstructionType_Abx;
            
        case totemOperationType_Goto:
            return totemInstructionType_Axx;
            
        default:
            return totemInstructionType_Abc;
    }
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
        case totemInstructionType_Abc:
            totemInstruction_PrintAbcInstruction(file, instruction);
            break;
            
        case totemInstructionType_Abx:
            totemInstruction_PrintAbxInstruction(file, instruction);
            break;
            
        case totemInstructionType_Axx:
            totemInstruction_PrintAxxInstruction(file, instruction);
            break;
    }
}

void totemInstruction_PrintAbcInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%s%d b:%s%d c:%s%d\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            totemRegisterScopeType_GetOperandTypeCode(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            totemRegisterScopeType_GetOperandTypeCode(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction),
            totemRegisterScopeType_GetOperandTypeCode(TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction));
}

void totemInstruction_PrintAbxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%s%d bx:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            totemRegisterScopeType_GetOperandTypeCode(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction));
}

void totemInstruction_PrintAxxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s ax:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_INSTRUCTION_GET_AX_UNSIGNED(instruction));
}

void totemInstruction_PrintBits(FILE *file, totemInstruction instruction)
{
    totemOperationType type = TOTEM_INSTRUCTION_GET_OP(instruction);
    totemInstructionType instType = totemOperationType_GetInstructionType(type);
    
    switch(instType)
    {
        case totemInstructionType_Abc:
            totemInstruction_PrintAbcBits(file, instruction);
            break;
            
        case totemInstructionType_Abx:
            totemInstruction_PrintAbxBits(file, instruction);
            break;
            
        case totemInstructionType_Axx:
            totemInstruction_PrintAxxBits(file, instruction);
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

void totemInstruction_PrintAbxBits(FILE *file, totemInstruction instruction)
{
    totem_printBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_A, totemInstructionStart_A);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_Bx, totemInstructionStart_B);
    fprintf(file, "\n");
}

void totemInstruction_PrintAxxBits(FILE *file, totemInstruction instruction)
{
    totem_printBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    fprintf(file, "|");
    totem_printBits(file, instruction, totemInstructionSize_Ax, totemInstructionStart_A);
    fprintf(file, "\n");
}

const char *totemRegisterScopeType_GetOperandTypeCode(totemRegisterScopeType type)
{
    switch(type)
    {
        case totemRegisterScopeType_Global:
            return "g";
            
        case totemRegisterScopeType_Local:
            return "l";
            
        default:
            return "x";
    }
}

void totem_printBits(FILE *file, uint32_t data, uint32_t numBits, uint32_t start)
{
    uint32_t end = start + numBits;
    for(uint32_t i = start; i < end; i++)
    {
        fprintf(file, "%i", TOTEM_GETBITS_OFFSET(data, TOTEM_BITMASK(i, 1), i));
    }
}

void totem_Exit(int code)
{
    exit(code);
}

totemOperandXSigned totemOperandXSigned_FromUnsigned(totemOperandXUnsigned val, uint32_t numBits)
{
    if(TOTEM_GETBITS_OFFSET(val, TOTEM_BITMASK(numBits - 1, 1), numBits - 1))
    {
        uint32_t actualVal = TOTEM_GETBITS(val, TOTEM_BITMASK(0, numBits - 1));
        actualVal = ~actualVal;
        actualVal++;
        return *((totemOperandXSigned*)(&actualVal));
    }
    
    return val;
}

void totem_freecwd(const char *cwd)
{
    totem_CacheFree((void*)cwd, PATH_MAX + 1);
}

const char *totem_getcwd()
{
    size_t size = PATH_MAX + 1;

    char *buffer = totem_CacheMalloc(size);
    if(getcwd(buffer, size) == buffer)
    {
        return buffer;
    }
    
    return NULL;
}