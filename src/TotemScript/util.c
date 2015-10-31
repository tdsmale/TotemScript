//
//  util.cpp
//  TotemScript
//
//  Created by Timothy Smale on 22/12/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <TotemScript/parse.h>
#include <string.h>

static totemMallocCb mallocCb = NULL;
static totemFreeCb freeCb = NULL;
static totemHashCb hashCb = NULL;

void totem_SetGlobalCallbacks(totemMallocCb newMallocCb, totemFreeCb newFreeCb, totemHashCb newHashCb)
{
    mallocCb = newMallocCb;
    freeCb = newFreeCb;
    hashCb = newHashCb;
}

void *totem_Malloc(size_t len)
{
    if(mallocCb)
    {
        return mallocCb(len);
    }
    
    return malloc(len);
}

void totem_Free(void *mem)
{
    if(freeCb)
    {
        return freeCb(mem);
    }
    
    return free(mem);
}

uint32_t totem_Hash(char *data, size_t len)
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

const char *totemOperation_GetName(totemOperation op)
{
    switch(op)
    {
        case totemOperation_Add:
            return "ADD";
            break;
            
        case totemOperation_ConditionalGoto:
            return "CONDITIONALGOTO";
            break;
            
        case totemOperation_Divide:
            return "DIVIDE";
            break;
            
        case totemOperation_Equals:
            return "EQUALS";
            break;
            
        case totemOperation_FunctionArg:
            return "FUNCTIONARG";
            break;
            
        case totemOperation_Goto:
            return "GOTO";
            break;
            
        case totemOperation_LessThan:
            return "LESSTHAN";
            break;
            
        case totemOperation_LessThanEquals:
            return "LESSTHANEQUALS";
            break;
            
        case totemOperation_LogicalAnd:
            return "LOGICALAND";
            break;
            
        case totemOperation_LogicalOr:
            return "LOGICALOR";
            break;
            
        case totemOperation_MoreThan:
            return "MORETHAN";
            break;
            
        case totemOperation_MoreThanEquals:
            return "MORETHANEQUALS";
            break;
            
        case totemOperation_Move:
            return "MOVE";
            break;
            
        case totemOperation_Multiply:
            return "MULTIPLY";
            break;
            
        case totemOperation_NativeFunction:
            return "NATIVEFUNCTION";
            break;
            
        case totemOperation_None:
            return "NONE";
            break;
            
        case totemOperation_NotEquals:
            return "NOTEQUALS";
            break;
            
        case totemOperation_Power:
            return "POWER";
            break;
            
        case totemOperation_Return:
            return "RETURN";
            break;
            
        case totemOperation_ScriptFunction:
            return "SCRIPTFUNCTION";
            break;
            
        case totemOperation_Subtract:
            return "SUBTRACT";
            break;
            
        default:
            return "UNKNOWN";
            break;
    }
}


const char *totemTokenType_GetName(totemTokenType token)
{
    switch(token) {
        case totemTokenType_Switch:
            return "SWITCH";
            break;
            
        case totemTokenType_And:
            return "AND";
            break;
            
        case totemTokenType_Assign:
            return "ASSIGN";
            break;
            
        case totemTokenType_Divide:
            return "DIVIDE";
            break;
            
        case totemTokenType_Do:
            return "DO";
            break;
            
        case totemTokenType_Dot:
            return "DOT";
            break;
            
        case totemTokenType_Semicolon:
            return "SEMICOLON";
            break;
            
        case totemTokenType_For:
            return "FOR";
            break;
            
        case totemTokenType_Identifier:
            return "IDENTIFIER";
            break;
            
        case totemTokenType_If:
            return "IF";
            break;
            
        case totemTokenType_LBracket:
            return "LBRACKET";
            break;
            
        case totemTokenType_LCBracket:
            return "LCBRACKET";
            break;
            
        case totemTokenType_LessThan:
            return "LESSTHAN";
            break;
            
        case totemTokenType_Minus:
            return "MINUS";
            break;
            
        case totemTokenType_MoreThan:
            return "MORETHAN";
            break;
            
        case totemTokenType_Multiply:
            return "MULTIPLY";
            break;
            
        case totemTokenType_Not:
            return "NOT";
            break;
            
        case totemTokenType_Or:
            return "OR";
            break;
            
        case totemTokenType_Plus:
            return "PLUS";
            break;
            
        case totemTokenType_PowerTo:
            return "POWERTO";
            break;
            
        case totemTokenType_RBracket:
            return "RBRACKET";
            break;
            
        case totemTokenType_RCBracket:
            return "RCBRACKET";
            break;
            
        case totemTokenType_Return:
            return "RETURN";
            break;
            
        case totemTokenType_Variable:
            return "VAR";
            break;
            
        case totemTokenType_While:
            return "WHILE";
            break;
            
        case totemTokenType_Whitespace:
            return "WHITESPACE";
            break;
            
        case totemTokenType_SingleQuote:
            return "SQUOTE";
            break;
            
        case totemTokenType_DoubleQuote:
            return "DQUOTE";
            break;
            
        case totemTokenType_LSBracket:
            return "LSBRACKET";
            break;
            
        case totemTokenType_RSBracket:
            return "RSBRACKET";
            break;
            
        case totemTokenType_Function:
            return "FUNCTION";
            break;
            
        case totemTokenType_Break:
            return "BREAK";
            break;
            
        case totemTokenType_Comma:
            return "COMMA";
            break;
            
        case totemTokenType_Colon:
            return "COLON";
            break;
            
        case totemTokenType_EndScript:
            return "ENDSCRIPT";
            break;
            
        case totemTokenType_Number:
            return "NUMBER";
            break;
            
        case totemTokenType_Case:
            return "CASE";
            break;
            
        case totemTokenType_Default:
            return "DEFAULT";
            break;
            
        case totemTokenType_Else:
            return "ELSE";
            break;
            
        case totemTokenType_True:
            return "TRUE";
        	break;
        	
        case totemTokenType_False:
            return "FALSE";
        	break;
            
        case totemTokenType_Backslash:
            return "BACKSLASH";
            break;
            
        case totemTokenType_Slash:
            return "FORWARDSLASH";
            break;
        	
        default:
            return "UNKNOWN";
            break;
    }
}

const char *totemDataType_GetName(totemDataType type)
{
    switch(type)
    {
        case totemDataType_Number:
            return "NUMBER (64-Bit DOUBLE)";
            
        case totemDataType_Reference:
            return "REFERENCE";
            
        case totemDataType_String:
            return "STRING";
            
        default:
            return "UNKNOWN";
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
    switch(instruction.Abc.Operation)
    {
        case totemOperation_Add:
        case totemOperation_Divide:
        case totemOperation_Equals:
        case totemOperation_LessThan:
        case totemOperation_LessThanEquals:
        case totemOperation_LogicalAnd:
        case totemOperation_LogicalOr:
        case totemOperation_MoreThan:
        case totemOperation_MoreThanEquals:
        case totemOperation_Multiply:
        case totemOperation_Power:
        case totemOperation_Subtract:
        case totemOperation_Move:
        case totemOperation_NotEquals:
            totemInstruction_PrintAbcInstructon(file, instruction);
            break;
            
        case totemOperation_ConditionalGoto:
        case totemOperation_FunctionArg:
        case totemOperation_NativeFunction:
        case totemOperation_ScriptFunction:
            totemInstruction_PrintAbxInstruction(file, instruction);
            break;
            
        case totemOperation_Goto:
        case totemOperation_Return:
            totemInstruction_PrintAxxInstruction(file, instruction);
            break;
            
        default:
            fprintf(file, "Unrecognised operation %i\n", instruction.Abc.Operation);
            break;
    }
}

void totemInstruction_PrintAbcInstructon(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%s %s%d %s%d %s%d",
            totemOperation_GetName(instruction.Abc.Operation),
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abc.OperandAType),
            instruction.Abc.OperandAIndex,
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abc.OperandBType),
            instruction.Abc.OperandBIndex,
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abc.OperandCType),
            instruction.Abc.OperandCIndex);
}

void totemInstruction_PrintAbxInstructon(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%s %s%d %d",
            totemOperation_GetName(instruction.Abx.Operation),
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abx.OperandAType),
            instruction.Abx.OperandAIndex,
            instruction.Abx.OperandBx);
}

void totemInstruction_PrintAxxInstructon(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%s %d",
            totemOperation_GetName(instruction.Axx.Operation),
            instruction.Axx.OperandAxx);
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

void totemToken_PrintList(FILE *target, totemToken *tokens, size_t num)
{
    for(size_t i = 0 ; i < num; ++i)
    {
        totemToken *token = tokens + i;
        fprintf(target, "%zu %s %.*s", i, totemTokenType_GetName(token->Type), (int)token->Value.Length, token->Value.Value);
    }
}