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

totemExecStatus totemPrint(totemExecState *state)
{
    totemRegister *reg = &state->Registers[totemOperandType_LocalRegister][0];
    totemRegister_Print(stdout, reg);
    return totemExecStatus_Continue;
}

int main(int argc, const char * argv[])
{
    totem_Init();
    
    // load file
    totemScriptFile scriptContents;
    totemScriptFile_Init(&scriptContents);
    
    totemLoadScriptError err;
    if(totemScriptFile_Load(&scriptContents, "test.totem", &err) != totemBool_True)
    {
        printf("Load script error: %s: %.*s\n", totemLoadScriptStatus_Describe(err.Status), err.Description.Length, err.Description.Value);
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Script:\n");
    printf("******\n");
    for(size_t i = 0; i < scriptContents.Buffer.Length; i++)
    {
        printf("%c", scriptContents.Buffer.Data[i]);
    }
    
    printf("\n");
    
    // init runtime
    totemRuntime runtime;
    totemRuntime_Init(&runtime);
    
    // link print function
    size_t printAddr = 0;
    totemString printName;
    totemString_FromLiteral(&printName, "print");
    totemLinkStatus linkStatus = totemRuntime_LinkNativeFunction(&runtime, &totemPrint, &printName, &printAddr);
    if(linkStatus != totemLinkStatus_Success)
    {
        printf("Could not register print: %s\n", totemLinkStatus_Describe(linkStatus));
        return EXIT_FAILURE;
    }
    
    // lex
    totemTokenList tokens;
    totemTokenList_Init(&tokens);
    
    totemLexStatus lexStatus = totemTokenList_Lex(&tokens, scriptContents.Buffer.Data, scriptContents.Buffer.Length);
    if(lexStatus != totemLexStatus_Success)
    {
        printf("Lex error: %s\n", totemLexStatus_Describe(lexStatus));
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Tokens:\n");
    printf("******\n");
    totemToken_PrintList(stdout, (totemToken*)tokens.Tokens.Data, totemMemoryBuffer_GetNumObjects(&tokens.Tokens));
    
    // parse
    totemParseTree parseTree;
    totemParseTree_Init(&parseTree);
    
    totemParseStatus parseStatus = totemParseTree_Parse(&parseTree, &tokens);
    if(parseStatus != totemParseStatus_Success)
    {
        printf("Parse error %s (%s) at %.*s \n", totemParseStatus_Describe(parseStatus), totemTokenType_Describe(parseTree.CurrentToken->Type), 50, parseTree.CurrentToken->Value.Value);
        return EXIT_FAILURE;
    }
    
    // eval
    totemBuildPrototype build;
    totemBuildPrototype_Init(&build);
    
    totemEvalStatus evalStatus = totemBuildPrototype_Eval(&build, &parseTree);
    if(evalStatus != totemEvalStatus_Success)
    {
        printf("Eval error %s\n", totemEvalStatus_Describe(evalStatus));
        return EXIT_FAILURE;
    }
    
    // link
    totemString scriptName;
    totemString_FromLiteral(&scriptName, "TotemTest");
    size_t scriptAddr = 0;
    linkStatus = totemRuntime_LinkBuild(&runtime, &build, &scriptName, &scriptAddr);
    if(linkStatus != totemLinkStatus_Success)
    {
        printf("Could not register script: %s\n", totemLinkStatus_Describe(linkStatus));
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Instructions:\n");
    printf("******\n");
    totemInstruction_PrintList(stdout, totemMemoryBuffer_Get(&build.Instructions, 0), totemMemoryBuffer_GetNumObjects(&build.Instructions));
    printf("\n");
    
    // init
    totemActor actor;
    if(totemActor_Init(&actor, &runtime, scriptAddr) != totemExecStatus_Continue)
    {
        printf("Could not create actor\n");
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Globals:\n");
    printf("******\n");
    totemRegister_PrintList(stdout, totemMemoryBuffer_Get(&actor.GlobalRegisters, 0), totemMemoryBuffer_GetNumObjects(&actor.GlobalRegisters));
    printf("\n");
    
    totemExecState execState;
    if(!totemExecState_Init(&execState, &runtime, 4096))
    {
        printf("Could not create exec state\n");
        return EXIT_FAILURE;
    }
    
    totemScript *script = totemMemoryBuffer_Get(&runtime.Scripts, scriptAddr);
    totemHashMapEntry *function = totemHashMap_Find(&script->FunctionNameLookup, "test", 4);
    
    totemRegister returnRegister;
    memset(&returnRegister, 0, sizeof(totemRegister));
    
    printf("******\n");
    printf("Run:\n");
    printf("******\n");
    
    // init global vars
    totemExecStatus execStatus = totemExecState_Exec(&execState, &actor, 0, &returnRegister);
    if(execStatus != totemExecStatus_Return)
    {
        printf("exec error %s\n", totemExecStatus_Describe(execStatus));
        return EXIT_FAILURE;
    }
    
    // run test
    execStatus = totemExecState_Exec(&execState, &actor, function->Value, &returnRegister);
    if(execStatus != totemExecStatus_Return)
    {
        printf("exec error %s\n", totemExecStatus_Describe(execStatus));
        return EXIT_FAILURE;
    }
    
    // free
    totemExecState_Cleanup(&execState);
    totemActor_Cleanup(&actor);
    
    totemBuildPrototype_Cleanup(&build);
    totemParseTree_Cleanup(&parseTree);
    totemTokenList_Cleanup(&tokens);
    
    totemRuntime_Cleanup(&runtime);
    totemScriptFile_Cleanup(&scriptContents);
    
    totem_Cleanup();
    
    return 0;
}