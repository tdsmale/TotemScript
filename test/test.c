//
//  test
//  TotemScript
//
//  Created by Timothy Smale on 05/11/2015.
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <TotemScript/totem.h>

void totemPrintRegister(totemRegister *reg, totemExecState *state, size_t indent)
{
    switch(reg->DataType)
    {
        case totemDataType_String:
            printf("%s %.*s (%i) \n", totemDataType_Describe(reg->DataType), reg->Value.String.Length, &state->CallStack->Actor->GlobalData.Data[reg->Value.String.Index], reg->Value.String.Length);
            break;
            
        case totemDataType_Array:
        {
            indent += 5;
            totemRuntimeArray *arr = reg->Value.Array;
            printf("array[%u] {\n", arr->NumRegisters);
            
            for(totemInt i = 0; i < arr->NumRegisters; ++i)
            {
                for(size_t i = 0; i < indent; i++)
                {
                    printf(" ");
                }
                
                printf("%lld: ", i);
                
                totemRegister *val = &arr->Registers[i];
                totemPrintRegister(val, state, indent);
            }
            
            indent -= 5;
            
            for(size_t i = 0; i < indent; i++)
            {
                printf(" ");
            }
            
            printf("}\n");
            break;
        }
            
        default:
            printf("%s %f %lli\n", totemDataType_Describe(reg->DataType), reg->Value.Float, reg->Value.Int);
            break;
    }
}

totemExecStatus totemPrint(totemExecState *state)
{
    totemRegister *reg = &state->CallStack->RegisterFrameStart[0];
    totemPrintRegister(reg, state, 0);
    return totemExecStatus_Continue;
}

int main(int argc, const char * argv[])
{
    // load file
    totemMemoryBuffer scriptContents;
    totemLoadScriptError err;
    if(totemMemoryBuffer_LoadScriptFromFile(&scriptContents, "test.totem", &err) != totemBool_True)
    {
        printf("Load script error: %s: %.*s\n", totemLoadScriptStatus_Describe(err.Status), err.Description.Length, err.Description.Value);
        return 1;
    }
    
    for(uint32_t i = 0; i < scriptContents.Length; i++)
    {
        printf("%c", scriptContents.Data[i]);
    }
    
    printf("\n");
    
    // init runtime
    totemRuntime runtime;
    memset(&runtime, 0, sizeof(totemRuntime));
    totemRuntime_Reset(&runtime);
    
    // register print function
    size_t printAddr = 0;
    totemString printName;
    totemString_FromLiteral(&printName, "print");
    if(!totemRuntime_RegisterNativeFunction(&runtime, &totemPrint, &printName, &printAddr))
    {
        printf("Could not register print\n");
        return 1;
    }
    
    // lex
    totemTokenList tokens;
    memset(&tokens, 0, sizeof(totemTokenList));
    totemTokenList_Reset(&tokens);
    totemLexStatus lexStatus = totemTokenList_Lex(&tokens, scriptContents.Data, scriptContents.Length);
    if(lexStatus != totemLexStatus_Success)
    {
        printf("Lex error\n");
        return 1;
    }
    
    totemToken_PrintList(stdout, (totemToken*)tokens.Tokens.Data, totemMemoryBuffer_GetNumObjects(&tokens.Tokens));
    
    // parse
    totemParseTree parseTree;
    memset(&parseTree, 0, sizeof(totemParseTree));
    totemParseStatus parseStatus = totemParseTree_Parse(&parseTree, &tokens);
    if(parseStatus != totemParseStatus_Success)
    {
        printf("Parse error %s (%s) at %.*s \n", totemParseStatus_Describe(parseStatus), totemTokenType_Describe(parseTree.CurrentToken->Type), 50, parseTree.CurrentToken->Value.Value);
        return 1;
    }
    
    // eval
    totemBuildPrototype build;
    memset(&build, 0, sizeof(totemBuildPrototype));
    totemBuildPrototype_Init(&build, &runtime);
    totemEvalStatus evalStatus = totemBuildPrototype_Eval(&build, &parseTree);
    if(evalStatus != totemEvalStatus_Success)
    {
        printf("Eval error %s\n", totemEvalStatus_Describe(evalStatus));
        return 1;
    }
    
    totemInstruction_PrintList(stdout, totemMemoryBuffer_Get(&build.Instructions, 0), totemMemoryBuffer_GetNumObjects(&build.Instructions));
    printf("\n");

    totemString scriptName;
    totemString_FromLiteral(&scriptName, "TotemTest");
    size_t scriptAddr = 0;
    if(!totemRuntime_RegisterScript(&runtime, &build, &scriptName, &scriptAddr))
    {
        printf("Could not register script\n");
        return 1;
    }
    
    totemActor actor;
    memset(&actor, 0, sizeof(totemActor));
    if(totemActor_Init(&actor, &runtime, scriptAddr) != totemExecStatus_Continue)
    {
        printf("Could not create actor\n");
        return 1;
    }
    
    totemExecState execState;
    memset(&execState, 0, sizeof(totemExecState));
    if(!totemExecState_Init(&execState, &runtime, 4096))
    {
        printf("Could not create exec state\n");
        return 1;
    }
    
    totemScript *script = totemMemoryBuffer_Get(&runtime.Scripts, scriptAddr);
    totemHashMapEntry *function = totemHashMap_Find(&script->FunctionNameLookup, "test", 4);

    totemRegister returnRegister;
    memset(&returnRegister, 0, sizeof(totemRegister));
    
    // init global vars
    totemExecStatus execStatus = totemExecState_Exec(&execState, &actor, 0, &returnRegister);
    if(execStatus != totemExecStatus_Return)
    {
        printf("exec error %s\n", totemExecStatus_Describe(execStatus));
        return 1;
    }
    
    // run test
    execStatus = totemExecState_Exec(&execState, &actor, function->Value, &returnRegister);
    if(execStatus != totemExecStatus_Return)
    {
        printf("exec error %s\n", totemExecStatus_Describe(execStatus));
        return 1;
    }
    
    // free
    totemExecState_Cleanup(&execState);
    totemActor_Cleanup(&actor);
    
    totemBuildPrototype_Cleanup(&build);
    totemParseTree_Cleanup(&parseTree);
    totemTokenList_Cleanup(&tokens);
    
    totemRuntime_Cleanup(&runtime);
    totemMemoryBuffer_Cleanup(&scriptContents);
    
    return 0;
}