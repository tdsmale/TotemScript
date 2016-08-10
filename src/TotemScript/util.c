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
#include <stdio.h>

#define TOTEM_SCOPE_CHAR(x) ((x) == totemOperandType_GlobalRegister ? 'g' : 'l')

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

void totemInstruction_PrintAbcxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%08x %s a:%c%d b:%c%d c:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_SCOPE_CHAR(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction),
            TOTEM_SCOPE_CHAR(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)),
            TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction),
            TOTEM_INSTRUCTION_GET_CX_UNSIGNED(instruction));
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

size_t totemMiniString_GetLength(char *c)
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

totemStringLength totemRegister_GetStringLength(totemRegister *reg)
{
    switch(reg->DataType)
    {
        case totemPrivateDataType_InternedString:
            return reg->Value.InternedString->Length;
            
        case totemPrivateDataType_MiniString:
            return totemMiniString_GetLength(reg->Value.MiniString.Value);
            
        default:
            return 0;
    }
}

totemHash totemRegister_GetStringHash(totemRegister *reg)
{
    switch (reg->DataType)
    {
        case totemPrivateDataType_InternedString:
            return reg->Value.InternedString->Hash;
            
        case totemPrivateDataType_MiniString:
            return totem_Hash(reg->Value.MiniString.Value, totemMiniString_GetLength(reg->Value.MiniString.Value));
            
        default:
            return 0;
    }
}

const char *totemRegister_GetStringValue(totemRegister *reg)
{
    switch(reg->DataType)
    {
        case totemPrivateDataType_InternedString:
            return reg->Value.InternedString->Data;
            
        case totemPrivateDataType_MiniString:
            return reg->Value.MiniString.Value;
            
        default:
            return NULL;
    }
}

void totemExecState_PrintRegisterRecursive(totemExecState *state, FILE *file, totemRegister *reg, size_t indent)
{
    if (indent > 50)
    {
        return;
    }
    
    switch(reg->DataType)
    {
        case totemPrivateDataType_NativeFunction:
        {
            totemRegister val;
            if (totemRuntime_GetNativeFunctionName(state->Runtime, reg->Value.NativeFunction->Address, &val.Value, &val.DataType))
            {
                fprintf(file, "%s: %.*s\n", totemPrivateDataType_Describe(reg->DataType), (int)totemRegister_GetStringLength(&val), totemRegister_GetStringValue(&val));
            }
            break;
        }
            
        case totemPrivateDataType_InstanceFunction:
        {
            totemRegister val;
            if (totemScript_GetFunctionName(reg->Value.InstanceFunction->Instance->Script, reg->Value.InstanceFunction->Function->Address, &val.Value, &val.DataType))
            {
                fprintf(file, "%s: %.*s\n", totemPrivateDataType_Describe(reg->DataType), (int)totemRegister_GetStringLength(&val), totemRegister_GetStringValue(&val));
            }
            break;
        }
            
        case totemPrivateDataType_Coroutine:
        {
            totemFunctionCall *cr = reg->Value.GCObject->Coroutine;
            totemRegister val;
            if (totemScript_GetFunctionName(cr->InstanceFunction->Instance->Script, cr->InstanceFunction->Function->Address, &val.Value, &val.DataType))
            {
                fprintf(file, "%s: %.*s\n", totemPrivateDataType_Describe(reg->DataType), (int)totemRegister_GetStringLength(&val), totemRegister_GetStringValue(&val));
            }
            break;
        }
            
        case totemPrivateDataType_Type:
            fprintf(file, "%s: %s\n", totemPrivateDataType_Describe(reg->DataType), totemPublicDataType_Describe(reg->Value.DataType));
            break;
            
        case totemPrivateDataType_InternedString:
        case totemPrivateDataType_MiniString:
            fprintf(file, "%s \"%.*s\" (%i) \n",
                    totemPrivateDataType_Describe(reg->DataType),
                    (int)totemRegister_GetStringLength(reg),
                    totemRegister_GetStringValue(reg),
                    (int)totemRegister_GetStringLength(reg));
            break;
            
        case totemPrivateDataType_Object:
        {
            indent += 5;
            totemObject *obj = reg->Value.GCObject->Object;
            
            fprintf(file, "object {\n");
            
            for (size_t i = 0; i < obj->Lookup.NumBuckets; i++)
            {
                totemHashMapEntry *entry = obj->Lookup.Buckets[i];
                
                while (entry)
                {
                    for (size_t i = 0; i < indent; i++)
                    {
                        fprintf(file, " ");
                    }
                    
                    fprintf(file, "\"%.*s\": ", (int)entry->KeyLen, entry->Key);
                    totemRegister *val = totemMemoryBuffer_Get(&obj->Registers, entry->Value);
                    if (val)
                    {
                        totemExecState_PrintRegisterRecursive(state, file, val, indent);
                    }
                    
                    entry = entry->Next;
                }
            }
            
            indent -= 5;
            
            for (size_t i = 0; i < indent; i++)
            {
                fprintf(file, " ");
            }
            
            fprintf(file, "}\n");
            
            break;
        }
            
        case totemPrivateDataType_Array:
        {
            indent += 5;
            totemArray *arr = reg->Value.GCObject->Array;
            
            fprintf(file, "array[%u] {\n", arr->NumRegisters);
            
            for(totemInt i = 0; i < arr->NumRegisters; ++i)
            {
                for(size_t i = 0; i < indent; i++)
                {
                    fprintf(file, " ");
                }
                
                fprintf(file, "%lld: ", i);
                
                totemRegister *val = &arr->Registers[i];
                totemExecState_PrintRegisterRecursive(state, file, val, indent);
            }
            
            indent -= 5;
            
            for(size_t i = 0; i < indent; i++)
            {
                fprintf(file, " ");
            }
            
            fprintf(file, "}\n");
            break;
        }
            
        case totemPrivateDataType_Float:
            fprintf(file, "%s %f\n", totemPrivateDataType_Describe(reg->DataType), reg->Value.Float);
            break;
            
        case totemPrivateDataType_Int:
            fprintf(file, "%s %lli\n", totemPrivateDataType_Describe(reg->DataType), reg->Value.Int);
            break;
            
        case totemPrivateDataType_Boolean:
            fprintf(file, "%s %s\n", totemPrivateDataType_Describe(reg->DataType), reg->Value.Data ? "true" : "false");
            break;
            
        case totemPrivateDataType_Null:
        case totemPrivateDataType_Userdata:
            fprintf(file, "%s\n", totemPrivateDataType_Describe(reg->DataType));
            break;
            
        default:
            fprintf(file, "%s %d %f %lli %p\n", totemPrivateDataType_Describe(reg->DataType), reg->DataType, reg->Value.Float, reg->Value.Int, reg->Value.GCObject);
            break;
    }
}

void totemExecState_PrintRegister(totemExecState *state, FILE *file, totemRegister *reg)
{
    totemExecState_PrintRegisterRecursive(state, file, reg, 0);
}

void totemExecState_PrintRegisterList(totemExecState *state, FILE *file, totemRegister *regs, size_t numRegisters)
{
    for (size_t i = 0; i < numRegisters; i++)
    {
        totemExecState_PrintRegister(state, file, &regs[i]);
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

totemBool totem_getcwd(char *buffer, size_t size)
{
    return getcwd(buffer, (totemCwdSize_t)size) == buffer;
}

void totem_Init()
{
    TOTEM_STATIC_ASSERT(TOTEM_STRING_LITERAL_SIZE("test") == 4, "String length test");
    
    TOTEM_STATIC_ASSERT(sizeof(totemRegisterValue) == 8, "Totem Register Values must be 8 bytes");
    TOTEM_STATIC_ASSERT(sizeof(totemInstruction) == 4, "Totem Instruction must be 4 bytes");
    TOTEM_STATIC_ASSERT(sizeof(size_t) == sizeof(void*), "size_t must be able to hold any memory address");
    
    TOTEM_STATIC_ASSERT(sizeof(totemFloat) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(totemInt) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(void*) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(totemRuntimeMiniString) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(totemPublicDataType) == sizeof(totemRegisterValue), "Value-size mismatch");
    TOTEM_STATIC_ASSERT(sizeof(uint64_t) == sizeof(totemRegisterValue), "Value-size mismatch");
    
    TOTEM_STATIC_ASSERT(&((totemGCObject*)0)->Header.NextHdr == &((totemGCHeader*)0)->NextHdr, "totemGCObject* must be able to masquerade as a totemGCHeader*");
    
#ifdef TOTEM_X86
    TOTEM_STATIC_ASSERT(sizeof(void*) == 4, "Expected pointer size to be 4 bytes");
#endif
    
#ifdef TOTEM_X64
    TOTEM_STATIC_ASSERT(sizeof(void*) == 8, "Expected pointer size to be 8 bytes");
#endif
    
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
}