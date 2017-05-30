//
//  util.c
//  TotemScript
//
//  Created by Timothy Smale on 22/12/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <TotemScript/parse.h>
#include <TotemScript/platform.h>
#include <TotemScript/exec.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static totemHashCb hashCb = NULL;

void totem_SetGlobalCallbacks(totemHashCb newHashCb)
{
    hashCb = newHashCb;
}

totemHash totem_Hash(const void *data, size_t len)
{
    if(hashCb)
    {
        return hashCb(data, len);
    }
    
    const char *ptr = data;
    
    totemHash hash = 5831;
    for(uint32_t i = 0; i < len; ++i)
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
            TOTEM_STRINGIFY_CASE(totemOperationType_LogicalNegate);
            TOTEM_STRINGIFY_CASE(totemOperationType_LogicalOr);
            TOTEM_STRINGIFY_CASE(totemOperationType_MoreThan);
            TOTEM_STRINGIFY_CASE(totemOperationType_MoreThanEquals);
            TOTEM_STRINGIFY_CASE(totemOperationType_Move);
            TOTEM_STRINGIFY_CASE(totemOperationType_Multiply);
            TOTEM_STRINGIFY_CASE(totemOperationType_NewObject);
            TOTEM_STRINGIFY_CASE(totemOperationType_NotEquals);
            TOTEM_STRINGIFY_CASE(totemOperationType_Return);
            TOTEM_STRINGIFY_CASE(totemOperationType_Subtract);
            TOTEM_STRINGIFY_CASE(totemOperationType_ComplexGet);
            TOTEM_STRINGIFY_CASE(totemOperationType_ComplexSet);
            TOTEM_STRINGIFY_CASE(totemOperationType_NewArray);
            TOTEM_STRINGIFY_CASE(totemOperationType_MoveToGlobal);
            TOTEM_STRINGIFY_CASE(totemOperationType_MoveToLocal);
            TOTEM_STRINGIFY_CASE(totemOperationType_Invoke);
            TOTEM_STRINGIFY_CASE(totemOperationType_PreInvoke);
            TOTEM_STRINGIFY_CASE(totemOperationType_As);
            TOTEM_STRINGIFY_CASE(totemOperationType_Is);
            TOTEM_STRINGIFY_CASE(totemOperationType_ComplexShift);
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
    switch(op)
    {
        case totemOperationType_ConditionalGoto:
        case totemOperationType_FunctionArg:
        case totemOperationType_Return:
        case totemOperationType_MoveToGlobal:
        case totemOperationType_MoveToLocal:
        case totemOperationType_PreInvoke:
        case totemOperationType_Invoke:
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
        case totemInstructionType_Abcx:
            totemInstruction_PrintAbcxInstruction(file, instruction);
            break;
            
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

char totemOperandType_GetChar(totemOperandType type)
{
    return type == totemOperandType_GlobalRegister ? 'g' : 'l';
}

void totemInstruction_PrintAbcxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%c%d b:%c%d c:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction),
            TOTEM_INSTRUCTION_GET_CX_UNSIGNED(instruction));
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

void totemInstruction_PrintAbxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%c%d bx:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            totemOperandType_GetChar(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
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
        case totemInstructionType_Abcx:
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

void totem_Init()
{
#define TOTEM_OPCODE_FORMAT(x) 0,
    TOTEM_STATIC_ASSERT(TOTEM_ARRAY_SIZE((int[]){TOTEM_EMIT_OPCODES()}) < TOTEM_MAXVAL_UNSIGNED(size_t, totemInstructionSize_Op), "Too many opcodes defined!");
#undef TOTEM_OPCODE_FORMAT
    
    TOTEM_STATIC_ASSERT(TOTEM_STRING_LITERAL_SIZE("test") == 4, "String length test");
    
    TOTEM_STATIC_ASSERT(sizeof(totemInstruction) == 4, "Totem Instruction must be 4 bytes");
    TOTEM_STATIC_ASSERT(sizeof(size_t) == sizeof(void*), "size_t must be able to hold any memory address");
    
#if !TOTEM_VMOPT_NANBOXING
    TOTEM_STATIC_ASSERT(sizeof(totemRegisterValue) == 8, "Totem Register Values must be 8 bytes");
    TOTEM_STATIC_ASSERT(sizeof(totemFloat) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(totemInt) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(void*) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(totemPublicDataType) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(uint64_t) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT((sizeof(char) * (TOTEM_MINISTRING_MAXLENGTH + 1)) <= sizeof(uint64_t), "Value-size mismatch");
#else
    TOTEM_STATIC_ASSERT(sizeof(totemFloat) == sizeof(totemRegister), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(uint64_t) == sizeof(totemRegister), "Value-size mismatch");
#endif
    
    TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.NextHdr) == offsetof(totemGCHeader, NextHdr), "totemGCObject* must be able to masquerade as a totemGCHeader*");
    TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.PrevHdr) == offsetof(totemGCHeader, PrevHdr), "totemGCObject* must be able to masquerade as a totemGCHeader*");
    TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.NextObj) == offsetof(totemGCHeader, NextObj), "totemGCObject* must be able to masquerade as a totemGCHeader*");
    TOTEM_STATIC_ASSERT(offsetof(totemGCObject, Header.PrevObj) == offsetof(totemGCHeader, PrevObj), "totemGCObject* must be able to masquerade as a totemGCHeader*");
    
    totem_InitMemory();
}

void totem_Cleanup()
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
#ifdef TOTEM_LINUX
    int fd = fileno(file);
    
    char procfs[PATH_MAX];
    char path[PATH_MAX];
    sprintf(procfs, "/proc/self/fd/%d", fd);
    if (readlink(procfs, path, PATH_MAX) != -1)
    {
        size_t len = strnlen(path, PATH_MAX);
        for(char *c = path + len - 1; c >= path; --c)
        {
            if(*c=='/' || *c == '\\')
            {
                *c = '\0';
                break;
            }
        }
        if(chdir(path) == 0)
        {
            return totemBool_True;
        }
    }
    return totemBool_False;
#endif
}
