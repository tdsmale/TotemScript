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
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#include <io.h>
#include <Shlwapi.h>

#define getcwd _getcwd
#define PATH_MAX (_MAX_PATH)

#endif

#ifdef __APPLE__
#include <unistd.h>
#include <sys/syslimits.h>
#include <fcntl.h>
#include <sys/param.h>
#include <dirent.h>

#endif

#define TOTEM_SCOPE_CHAR(x) (x) == totemOperandType_GlobalRegister ? 'g' : 'l'

static totemHashCb hashCb = NULL;

void totem_SetGlobalCallbacks(totemHashCb newHashCb)
{
    hashCb = newHashCb;
}

uint32_t totem_Hash(const void *data, size_t len)
{
    if(hashCb)
    {
        return hashCb(data, len);
    }
    
    const char *ptr = data;
    
    uint32_t hash = 5831;
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
            TOTEM_STRINGIFY_CASE(totemOperationType_MoveToGlobal);
            TOTEM_STRINGIFY_CASE(totemOperationType_MoveToLocal);
            TOTEM_STRINGIFY_CASE(totemOperationType_FunctionPointer);
            TOTEM_STRINGIFY_CASE(totemOperationType_Assert);
            TOTEM_STRINGIFY_CASE(totemOperationType_As);
            TOTEM_STRINGIFY_CASE(totemOperationType_Is);
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
            TOTEM_STRINGIFY_CASE(totemDataType_String);
            TOTEM_STRINGIFY_CASE(totemDataType_Array);
            TOTEM_STRINGIFY_CASE(totemDataType_Type);
            TOTEM_STRINGIFY_CASE(totemDataType_Function);
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
        case totemOperationType_MoveToGlobal:
        case totemOperationType_MoveToLocal:
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
    fprintf(file, "%08x %s a:%c%d b:%c%d c:%c%d\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_SCOPE_CHAR(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            TOTEM_SCOPE_CHAR(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction),
            TOTEM_SCOPE_CHAR(TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction));
}

void totemInstruction_PrintAbxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%c%d bx:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_SCOPE_CHAR(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
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

void totemRegister_PrintRecursive(FILE *file, totemRegister *reg, size_t indent)
{
    switch(reg->DataType)
    {
        case totemDataType_Function:
            fprintf(file, "%s: %d:%d\n", totemDataType_Describe(reg->DataType), reg->Value.FunctionPointer.Type, reg->Value.FunctionPointer.Address);
            break;
            
        case totemDataType_Type:
            fprintf(file, "%s: %s\n", totemDataType_Describe(reg->DataType), totemDataType_Describe(reg->Value.DataType));
            break;
            
        case totemDataType_String:
            fprintf(file, "%s \"%.*s\" (%i) \n",
                    totemDataType_Describe(reg->DataType),
                    reg->Value.InternedString->Length,
                    totemInternedStringHeader_GetString(reg->Value.InternedString),
                    reg->Value.InternedString->Length);
            break;
            
        case totemDataType_Array:
        {
            indent += 5;
            totemRuntimeArray *arr = reg->Value.Array;
            fprintf(file, "array[%u] {\n", arr->NumRegisters);
            
            for(totemInt i = 0; i < arr->NumRegisters; ++i)
            {
                for(size_t i = 0; i < indent; i++)
                {
                    fprintf(file, " ");
                }
                
                fprintf(file, "%lld: ", i);
                
                totemRegister *val = &arr->Registers[i];
                totemRegister_PrintRecursive(file, val, indent);
            }
            
            indent -= 5;
            
            for(size_t i = 0; i < indent; i++)
            {
                fprintf(file, " ");
            }
            
            fprintf(file, "}\n");
            break;
        }
            
        case totemDataType_Float:
            fprintf(file, "%s %f\n", totemDataType_Describe(reg->DataType), reg->Value.Float);
            break;
            
        case totemDataType_Int:
            fprintf(file, "%s %lli\n", totemDataType_Describe(reg->DataType), reg->Value.Int);
            break;
            
        case totemDataType_Null:
            fprintf(file, "%s\n", totemDataType_Describe(reg->DataType));
            break;
            
        default:
            fprintf(file, "%s %d %f %lli %p\n", totemDataType_Describe(reg->DataType), reg->DataType, reg->Value.Float, reg->Value.Int, reg->Value.Array);
            break;
    }
}

void totemRegister_Print(FILE *file, totemRegister *reg)
{
    totemRegister_PrintRecursive(file, reg, 0);
}

void totemRegister_PrintList(FILE *file, totemRegister *regs, size_t numRegs)
{
    for(size_t i = 0; i < numRegs; i++)
    {
        totemRegister_Print(file, &regs[i]);
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
    totem_CacheFree((void*)cwd, PATH_MAX);
}

const char *totem_getcwd()
{
    size_t size = PATH_MAX;
    
    char *buffer = totem_CacheMalloc(size);
    if(getcwd(buffer, size) == buffer)
    {
        return buffer;
    }
	else
	{
		totem_CacheFree(buffer, size);
		return NULL;
	}
}

void totem_Init()
{
    totem_InitMemory();
}

void totem_Cleanup()
{
    totem_CleanupMemory();
}

FILE *totem_fopen(const char *path, const char *mode)
{
#ifdef _WIN32
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

totemBool totem_chdir(const char *str)
{
#ifdef _WIN32
    return _chdir(str) == 0;
#else
    return chdir(str) == 0;
#endif
}

totemBool totem_fchdir(FILE *file)
{
#ifdef _WIN32
    HANDLE handle = (HANDLE)_get_osfhandle(_fileno(file));
    if (handle == INVALID_HANDLE_VALUE)
    {
        return totemBool_False;
    }
    
    TCHAR buffer[PATH_MAX];
    LPWSTR filename = buffer;
    
    DWORD length = GetFinalPathNameByHandle(handle, filename, TOTEM_ARRAYSIZE(buffer), VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
    if (length < TOTEM_ARRAYSIZE(buffer))
    {
        // skip \\?\ prefix
        LPWSTR toSkip = L"\\\\?\\";
        size_t toSkipLen = wcslen(toSkip);
        if (wcsstr(filename, toSkip) == filename)
        {
            filename += toSkipLen;
        }
        
        size_t len = wcslen(filename);
        
		for (TCHAR *c = filename + len - 1; c >= filename; c--)
        {
            if(c[0] == '/' || c[0] == '\\')
            {
                *c = 0;
                break;
            }
        }
        
        if(_wchdir(filename) == 0)
        {
            return totemBool_True;
        }
    }
    
    return totemBool_False;
#endif
#ifdef __APPLE__
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