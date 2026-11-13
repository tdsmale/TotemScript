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
#include <TotemScript/eval.h>
#include <TotemScript/exec.h>
#include <TotemScript/dispatch.h>
#include <TotemScript/log.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>

#if defined(TOTEM_POSIX)
void totemLock_Init(totemLock *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}
#endif

totemHashValue totem_HashBegin()
{
    return 5831;
}

totemHashValue totem_HashChar(char c, totemHashValue hash)
{
    return 33 * hash * c;
}

totemHashValue totem_HashStep(const void *data, size_t len, totemHashValue hash)
{
    const char *ptr = data;
    
    for (size_t i = 0; i < len; ++i)
    {
        hash = totem_HashChar(ptr[i], hash);
    }
    
    return hash;
}

totemHashValue totem_Hash(const void *data, size_t len)
{
    return totem_HashStep(data, len, totem_HashBegin());
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
	return strncmp(a->Value, str, a->Length) == 0;
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
            TOTEM_STRINGIFY_CASE(totemPublicDataType_Instance);
        default: return "UNKNOWN";
    }
}

const char *totemPrivateDataType_Describe(totemPrivateDataType type)
{
    switch(type)
    {
        case totemPrivateDataType_Int: return "totemPrivateDataType_Int";
        case totemPrivateDataType_Type: return "totemPrivateDataType_Type";
        case totemPrivateDataType_Array: return "totemPrivateDataType_Array";
        case totemPrivateDataType_Float: return "totemPrivateDataType_Float";
        case totemPrivateDataType_NativeFunction: return "totemPrivateDataType_NativeFunction";
        case totemPrivateDataType_InstanceFunction: return "totemPrivateDataType_InstanceFunction";
        case totemPrivateDataType_MiniString: return "totemPrivateDataType_MiniString";
        case totemPrivateDataType_InternedString: return "totemPrivateDataType_InternedString";
        case totemPrivateDataType_Coroutine: return "totemPrivateDataType_Coroutine";
        case totemPrivateDataType_Object: return "totemPrivateDataType_Object";
        case totemPrivateDataType_Userdata: return "totemPrivateDataType_Userdata";
        case totemPrivateDataType_Null: return "totemPrivateDataType_Null";
        case totemPrivateDataType_Boolean: return "totemPrivateDataType_Boolean";
        case totemPrivateDataType_Instance: return "totemPrivateDataType_Instance";
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

totemOperationBehaviourType totemOperationType_GetBehaviourFlag(totemOperationType op)
{
    switch (op)
    {
#define TOTEM_CASEIFY_OPCODE_TYPE(op, type, flags) case totemOperationType_ ## op: return totemOperationBehaviourType_ ## flags;
        TOTEM_EMIT_OPCODES(TOTEM_CASEIFY_OPCODE_TYPE)
#undef TOTEM_CASEIFY_OPCODE_TYPE
    }

    return totemOperationBehaviourType_None;
}

totemBool totemOperationType_IsPure(totemOperationType op)
{
    totemOperationBehaviourType behaviour = totemOperationType_GetBehaviourFlag(op);
    return behaviour == totemOperationBehaviourType_Pure || behaviour == totemOperationBehaviourType_PureControl;
}

void totemInstruction_PrintList(totemLog *file, totemInstruction *instructions, size_t num)
{
    for(size_t i = 0; i < num; ++i)
    {
        totemInstruction_Print(file, instructions[i]);
    }
}

void totemInstruction_Print(totemLog *file, totemInstruction instruction)
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
            
		case totemInstructionType_A:
			totemInstruction_PrintAInstruction(file, instruction);
			break;
    }
}

char totemOperandType_GetChar(totemOperandType type)
{
    return type == totemOperandType_GlobalRegister ? 'g' : 'l';
}

void totemInstruction_PrintAbc1uInstruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d b:%d c:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A(instruction),
		TOTEM_INSTRUCTION_GET_B(instruction),
		TOTEM_INSTRUCTION_GET_C1U(instruction));
}

void totemInstruction_PrintAbc1Instruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d b:%d c:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_INSTRUCTION_GET_A(instruction),
            TOTEM_INSTRUCTION_GET_B(instruction),
            TOTEM_INSTRUCTION_GET_C1(instruction));
}

void totemInstruction_PrintAbcInstruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d b:%d c:%d\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_INSTRUCTION_GET_A(instruction),
            TOTEM_INSTRUCTION_GET_B(instruction),
            TOTEM_INSTRUCTION_GET_C(instruction));
}

void totemInstruction_PrintAb2uInstruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d b2u:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A(instruction),
		TOTEM_INSTRUCTION_GET_B2U(instruction));
}

void totemInstruction_PrintAb2Instruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d b2:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_INSTRUCTION_GET_A(instruction),
            TOTEM_INSTRUCTION_GET_B2(instruction));
}

void totemInstruction_PrintAbInstruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d b:%d\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A(instruction),
		TOTEM_INSTRUCTION_GET_B(instruction));
}

void totemInstruction_PrintAb1c1Instruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d b1:%08x c1:%08x\n",
            instruction,
            totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
            TOTEM_INSTRUCTION_GET_A(instruction),
            TOTEM_INSTRUCTION_GET_B1(instruction),
            TOTEM_INSTRUCTION_GET_C1(instruction));
}

void totemInstruction_PrintA3uInstruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s ax:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A3U(instruction));
}

void totemInstruction_PrintA3Instruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s ax:%08x\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A3(instruction));
}

void totemInstruction_PrintAInstruction(totemLog *file, totemInstruction instruction)
{
    totemLog_Write(file, "%08x %s a:%d\n",
		instruction,
		totemOperationType_Describe(TOTEM_INSTRUCTION_GET_OP(instruction)),
		TOTEM_INSTRUCTION_GET_A(instruction));
}

void totemInstruction_PrintBits(totemLog *file, totemInstruction instruction)
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
            totemInstruction_PrintA3Bits(file, instruction);
            break;
    }
}

void totemInstruction_PrintAbcBits(totemLog *file, totemInstruction instruction)
{
    totem_PrintBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    totemLog_Write(file, "|");
    totem_PrintBits(file, instruction, totemInstructionSize_A, totemInstructionStart_A);
    totemLog_Write(file, "|");
    totem_PrintBits(file, instruction, totemInstructionSize_B, totemInstructionStart_B);
    totemLog_Write(file, "|");
    totem_PrintBits(file, instruction, totemInstructionSize_C, totemInstructionStart_C);
    totemLog_Write(file, "\n");
}

void totemInstruction_PrintAb2Bits(totemLog *file, totemInstruction instruction)
{
    totem_PrintBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    totemLog_Write(file, "|");
    totem_PrintBits(file, instruction, totemInstructionSize_A, totemInstructionStart_A);
    totemLog_Write(file, "|");
    totem_PrintBits(file, instruction, totemInstructionSize_B2, totemInstructionStart_B);
    totemLog_Write(file, "\n");
}

void totemInstruction_PrintA3Bits(totemLog *file, totemInstruction instruction)
{
    totem_PrintBits(file, instruction, totemInstructionSize_Op, totemInstructionStart_Op);
    totemLog_Write(file, "|");
    totem_PrintBits(file, instruction, totemInstructionSize_A3, totemInstructionStart_A);
    totemLog_Write(file, "\n");
}

void totem_PrintBits(totemLog *file, uint64_t data, uint64_t numBits, uint64_t start)
{
    uint64_t end = start + numBits;
    for(uint64_t i = end; i > start; i--)
    {
        totemLog_Write(file, "%"PRIu64, TOTEM_GETBITS_OFFSET(data, TOTEM_BITMASK(uint64_t, i - 1, 1), i - 1));
    }
}

totemOperandX totemOperandX_FromUnsigned(totemOperandXu val, uint32_t signedMask, uint32_t valueMask)
{
    if (TOTEM_GETBITS(val, signedMask))
    {
        uint32_t actualVal = TOTEM_GETBITS(val, valueMask);
        actualVal = ~actualVal;
        actualVal++;
        return *((totemOperandX*)(&actualVal));
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
    totem_InitLogging();
}

void totem_Cleanup(void)
{
    totem_CleanupMemory();
    totem_CleanupLogging();
}

void totem_Fatal(void)
{
    assert(totemBool_False);
    exit(EXIT_FAILURE);
}

FILE *totem_fopen(const char *path, const char *mode)
{
    FILE *result = NULL;
    errno_t err = 0;
#ifdef TOTEM_MSC
    err = fopen_s(&result, path, mode);
    
#else
    result = fopen(path, mode);
    if(result == NULL)
    {
        err = errno;
    }
#endif
    
    if(result == NULL)
    {
        TOTEM_LOG_V(Exec, File, Error, "Could not open file %s in mode %s (errno: %i)", path, mode, err);
    }
    
    return result;
}

totemBool totem_fchdir(FILE *file)
{
#ifdef TOTEM_WIN
    HANDLE handle = (HANDLE)_get_osfhandle(_fileno(file));
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

totemBool totemBitArray_Get(totemInt *arr, totemInt index)
{
    totemInt offset = index / TOTEM_INT_WIDTH;
    totemInt realIndex = index % TOTEM_INT_WIDTH;
    totemInt res = TOTEM_HASBITS_OFFSET(arr[offset], 1, realIndex);
    totemBool result = res != 0;
    return result;
}

void totemBitArray_Set(totemInt *arr, totemInt index, totemBool val)
{
    totemInt offset = index / TOTEM_INT_WIDTH;
    totemInt realIndex = index % TOTEM_INT_WIDTH;
    TOTEM_SETBITS_OFFSET(arr[offset], (totemInt)val, realIndex);
}
