//
//  exec_lib.c
//  TotemScript
//
//  Created by Timothy Smale on 19/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <TotemScript/dispatch.h>
#include <string.h>
#include <math.h>

typedef enum totemExpectedArgFlag
{
    totemExpectedArgFlag_None = 0,
    totemExpectedArgFlag_Required = 1,
    totemExpectedArgFlag_Found = 1 << 1,
    totemExpectedArgFlag_EnforceType = 1 << 2
}
totemExpectedArgFlag;

typedef struct totemExpectedArg
{
    totemPublicDataType Type;
    totemExpectedArgFlag Flags;
}
totemExpectedArg;

static void totemLibCheckArgs(totemExecState *state, uint32_t minimumArgs, totemExpectedArg* args, totemInt totalNumArgs)
{
    if(state->CallStack->NumRegisters < minimumArgs)
    {
        TOTEM_LOG_V(Exec, Lib, Error, "arguments less than min %"TOTEM_INT_PRINTF" argv\n", minimumArgs);
        TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    for(size_t i = 0; i < state->CallStack->NumRegisters; i++)
    {
        if(i >= totalNumArgs)
        {
            break;
        }
        
        if(TOTEM_GETBITS(args[i].Flags, totemExpectedArgFlag_EnforceType))
        {
            totemRegister *reg = &state->LocalRegisters[i];
            totemPublicDataType type = totemPrivateDataType_ToPublic(totemRegister_GetType(reg));
            if(type != args[i].Type)
            {
                TOTEM_LOG_V(Exec, Lib, Error, "argument %i not expected type %s (received %s) argv\n",
                            i,
                            totemPublicDataType_Describe(args[i].Type),
                            totemPublicDataType_Describe(type));
                TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
        }
        
        TOTEM_SETBITS(args[i].Flags, totemExpectedArgFlag_Found);
    }
}

void totemArgV(totemExecState *state)
{
    totemExpectedArg args[] = {
        {
            .Type = totemPublicDataType_Int,
            .Flags = totemExpectedArgFlag_Required | totemExpectedArgFlag_EnforceType
        }
    };
    totemLibCheckArgs(state, 1, args, TOTEM_ARRAY_SIZE(args));
    
    totemInt val = totemRegister_GetInt(&state->LocalRegisters[0]);
    
    if (!state->ArgV || val >= (totemInt)state->ArgC)
    {
        totemRegister_SetNull(state->CallStack->ReturnRegister);
    }
    else
    {
		totemString str;
		totemString_FromLiteral(&str, state->ArgV[val]);
        totemRuntimeStringValue result;
        TOTEM_DISPATCH_BREAK(totemExecState_InternString(state, &str, &result), state);
        totemRegister_SetString(state->CallStack->ReturnRegister, &result);
    }
}

void totemPrint(totemExecState *state)
{
    TOTEM_LOG_DO(Exec, Lib, Output, dst, totemRuntime_PrintRegisterList(state->Runtime, dst, state->LocalRegisters, state->CallStack->NumRegisters));
}

void totemAssert(totemExecState *state)
{
    totemExpectedArg args[] =
    {
        {
            .Flags = totemExpectedArgFlag_Required
        }
    };
    totemLibCheckArgs(state, 1, args, TOTEM_ARRAY_SIZE(args));
    
    totemRegister *reg = &state->LocalRegisters[0];
    if (totemRegister_IsZero(reg))
    {
        TOTEM_LOG(Exec, Lib, Error, "assertion failed\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
}

void totemSqrt(totemExecState *state)
{
    totemExpectedArg args[] =
    {
        {
            .Flags = totemExpectedArgFlag_Required
        }
    };
    totemLibCheckArgs(state, 1, args, TOTEM_ARRAY_SIZE(args));
    
    totemRegister *reg = &state->LocalRegisters[0];
    
    if (totemRegister_IsFloat(reg))
    {
        totemRegister_SetFloat(state->CallStack->ReturnRegister, sqrt(totemRegister_GetFloat(reg)));
    }
    else if (totemRegister_IsInt(reg))
    {
        totemRegister_SetFloat(state->CallStack->ReturnRegister, sqrt(totem_toFloat(totemRegister_GetInt(reg))));
    }
    else
    {
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
    }
}

void totemFileDestructor(totemExecState *state, void *data)
{
    fclose((FILE*)data);
}

void totemFOpen(totemExecState *state)
{
    totemExpectedArg args[] =
    {
        {
            .Type = totemPublicDataType_String,
            .Flags = totemExpectedArgFlag_Required | totemExpectedArgFlag_EnforceType
        },
        {
            .Type = totemPublicDataType_String,
            .Flags = totemExpectedArgFlag_Required | totemExpectedArgFlag_EnforceType
        },
    };
    totemLibCheckArgs(state, 2, args, TOTEM_ARRAY_SIZE(args));
	
    totemRegister *srcReg = &state->LocalRegisters[0];
    totemRegister *modeReg = &state->LocalRegisters[1];
    totemRuntimeStringValue srcVal;
    totemRuntimeStringValue modeVal;
    totemRegister_GetString(srcReg, &srcVal);
    totemRegister_GetString(modeReg, &modeVal);
    
    const char *src = totemRuntimeStringValue_GetVal(&srcVal);
    const char *mode = totemRuntimeStringValue_GetVal(&modeVal);
    
    FILE *f = totem_fopen(src, mode);
    if (!f)
    {
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    totemGCObject *gc = NULL;
    totemExecStatus status = totemExecState_CreateUserdata(state, (void*)f, totemFileDestructor, &gc);
    if (status != totemExecStatus_Continue)
    {
        fclose(f);
		TOTEM_DISPATCH_ERROR(state, status);
    }
    
    totemRegister_SetUserdata(state->CallStack->ReturnRegister, gc);
}

void totemGCCollect(totemExecState *state)
{
	totemExecState_CollectGarbage(state, state->CallStack->NumRegisters ? !totemRegister_IsZero(&state->LocalRegisters[0]) : totemBool_False);
}

void totemGCNum(totemExecState *state)
{
    totemRegister_SetInt(state->CallStack->ReturnRegister, (totemInt)state->GCNum);
}

void totemImport(totemExecState *state)
{
    totemExpectedArg args[] = {
        {
            .Type = totemPublicDataType_String,
            .Flags = totemExpectedArgFlag_Required | totemExpectedArgFlag_EnforceType
        }
    };
    totemLibCheckArgs(state, 1, args, TOTEM_ARRAY_SIZE(args));
    
    totemRegister *srcReg = &state->LocalRegisters[0];
    totemRuntimeStringValue srcVal;
    totemRegister_GetString(srcReg, &srcVal);
    totemScript *script = NULL;

    TOTEM_LOG_BEGIN(Exec, Lib, Debug, logger);
        totemLog_Write(logger, "");
        totemRuntimeStringValue_Print(&srcVal, logger);
    TOTEM_LOG_END();
    
    totemLinkStatus status = totemRuntime_LoadAndCacheScript(state->Runtime, &srcVal, &script);
    if(status != totemLinkStatus_Success)
    {
        TOTEM_LOG_V(Exec, Lib, Error, "error loading & caching script %s: %s",
                    totemRuntimeStringValue_GetVal(&srcVal),
                    totemLinkStatus_Describe(status));
        TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    // no need for this to be a root object, that way it can be collected when no longer referenced
    totemGCObject *instanceOut;
    totemExecStatus execStatus = totemExecState_CreateInstance(state, script, totemBool_False, &instanceOut);
    if(execStatus != totemExecStatus_Continue)
    {
        TOTEM_LOG_V(Exec, Lib, Error, "error creating instance for script %s: %s",
                    totemRuntimeStringValue_GetVal(&srcVal),
                    totemExecStatus_Describe(execStatus));
        TOTEM_DISPATCH_ERROR(state, execStatus);
    }
    
    totemRegister_SetInstance(state->CallStack->ReturnRegister, instanceOut);
}

void totemImportShared(totemExecState *state)
{
    totemExpectedArg args[] = {
        {
            .Type = totemPublicDataType_String,
            .Flags = totemExpectedArgFlag_Required | totemExpectedArgFlag_EnforceType
        }
    };
    totemLibCheckArgs(state, 1, args, TOTEM_ARRAY_SIZE(args));
    
    totemRegister *srcReg = &state->LocalRegisters[0];
    totemRuntimeStringValue srcVal;
    totemRegister_GetString(srcReg, &srcVal);
    totemGCObject *instance = NULL;
    
    totemExecStatus status = totemExecState_LoadAndCacheInstance(state, &srcVal, &instance);
    if(status != totemExecStatus_Continue)
    {
        TOTEM_LOG_V(Exec, Lib, Error, "error loading & caching instance for script %s: %s",
                    totemRuntimeStringValue_GetVal(&srcVal),
                    totemExecStatus_Describe(status));
        TOTEM_DISPATCH_ERROR(state, status);
    }
    
    totemRegister_SetInstance(state->CallStack->ReturnRegister, instance);
}

totemLinkStatus totemRuntime_LinkStdLib(totemRuntime *runtime)
{
    totemNativeFunctionPrototype funcs[] =
    {
		{ totemPrint, { "print", strlen("print") } },
		{ totemAssert, { "assert", strlen("assert") } },
		{ totemFOpen, { "fopen", strlen("fopen") } },
		{ totemGCCollect, { "gc_collect", strlen("gc_collect") } },
		{ totemGCNum, { "gc_num", strlen("gc_num") } },
		{ totemSqrt, { "sqrt", strlen("sqrt") } },
		{ totemArgV, { "argv", strlen("argv") } },
        { totemImportShared, { "import_shared", strlen("import_shared") } },
        { totemImport, { "import", strlen("import") } },
    };
    
    return totemRuntime_LinkNativeFunctions(runtime, funcs, TOTEM_ARRAY_SIZE(funcs));
}
