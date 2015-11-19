//
//  parse.c
//  TotemScript
//
//  Created by Timothy Smale on 19/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <TotemScript/base.h>
#include <string.h>
#include <ctype.h>

#define TOTEM_PARSE_SKIPWHITESPACE(token) while((*token)->Type == totemTokenType_Whitespace) { (*token)++; }
#define TOTEM_PARSE_COPYPOSITION(token, dest) memcpy(&dest->Position, &(*token)->Position, sizeof(totemBufferPositionInfo))
#define TOTEM_PARSE_ALLOC(dest, type, tree) dest = (type*)totemParseTree_Alloc(tree, sizeof(type)); if(!dest) return totemParseStatus_Break(totemParseStatus_OutOfMemory);
#define TOTEM_PARSE_CHECKRETURN(exp) { totemParseStatus s = exp; if(s != totemParseStatus_Success) return s; }
#define TOTEM_PARSE_ENFORCETOKEN(token, type) if((*token)->Type != type) return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
#define TOTEM_PARSE_ENFORCENOTTOKEN(token, type) if((*token)->Type == type) return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
#define TOTEM_PARSE_INC_NOT_ENDSCRIPT(token) (*token)++; TOTEM_PARSE_ENFORCENOTTOKEN(token, totemTokenType_EndScript);

const static totemTokenDesc s_symbolTokenValues[] =
{
    { totemTokenType_Variable, "$" },
    { totemTokenType_Plus, "+" },
    { totemTokenType_Minus, "-" },
    { totemTokenType_Multiply, "*" },
    { totemTokenType_Divide, "/" },
    { totemTokenType_Not, "!" },
    { totemTokenType_And, "&" },
    { totemTokenType_Or, "|" },
    { totemTokenType_Assign, "=" },
    { totemTokenType_LBracket, "(" },
    { totemTokenType_RBracket, ")" },
    { totemTokenType_LessThan, "<" },
    { totemTokenType_MoreThan, ">" },
    { totemTokenType_LCBracket, "{" },
    { totemTokenType_RCBracket, "}" },
    { totemTokenType_Dot, "." },
    { totemTokenType_PowerTo, "^" },
    { totemTokenType_Semicolon, ";" },
    { totemTokenType_Whitespace, " " },
    { totemTokenType_SingleQuote, "'" },
    { totemTokenType_DoubleQuote, "\"" },
    { totemTokenType_LSBracket, "[" },
    { totemTokenType_RSBracket, "]" },
    { totemTokenType_Comma, "," },
    { totemTokenType_Colon, ":" },
    { totemTokenType_Backslash, "\\" },
    { totemTokenType_Slash, "/" },
};

const static totemTokenDesc s_reservedWordValues[] =
{
    { totemTokenType_If, "if" },
    { totemTokenType_Do, "do" },
    { totemTokenType_While, "while" },
    { totemTokenType_For, "for" },
    { totemTokenType_Return, "return" },
    { totemTokenType_Switch, "switch" },
    { totemTokenType_Case, "case" },
    { totemTokenType_Break, "break" },
    { totemTokenType_Function, "function" },
    { totemTokenType_Default, "default" },
    { totemTokenType_Else, "else" },
    { totemTokenType_True, "true" },
    { totemTokenType_False, "false" },
};

#define TOTEM_LEX_CHECKRETURN(status, exp) status = exp; if(status == totemLexStatus_OutOfMemory) return status;

void totemTokenList_Reset(totemTokenList *list)
{
    totemMemoryBuffer_Reset(&list->Tokens, sizeof(totemToken));
}

/**
 * Lex buffer into token list
 */
totemLexStatus totemTokenList_Lex(totemTokenList *list, const char *buffer, size_t length)
{
    size_t currentTokenLength = 0;
    size_t currentTokenStart = 0;
    size_t currentLine = 1;
    size_t currentLineChar = 0;
    const char *toCheck = NULL;
    totemLexStatus status = totemLexStatus_Success;
    
    for(size_t i = 0; i < length; ++i)
    {
        if(buffer[i] == '\r' || buffer[i] == '\t')
        {
            continue;
        }
        
        if(buffer[i] == '\n')
        {
            ++currentLine;
            currentLineChar = 0;
            continue;
        }
        else
        {
            ++currentLineChar;
        }
        
        // lexing new token
        if(currentTokenLength == 0)
        {
            currentTokenStart = i;
            toCheck = buffer + currentTokenStart;
            //currentToken->Position.LineNumber = currentLine;
            //currentToken->Position.CharNumber = currentLineChar;
            // TODO: line & char numbers
        }
        else if(buffer[i] == ' ')
        {
            // presumed end of token
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexReservedWordToken(list, toCheck, currentTokenLength));
            if(status != totemLexStatus_Success)
            {
                TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexNumberOrIdentifierToken(list, toCheck, currentTokenLength));
            }
            
            currentTokenLength = 0;
            --i;
            
            continue;
        }
        
        ++currentTokenLength;
    
        // do we have a one-char symbol?
        if(currentTokenLength == 1)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck));
            if(status == totemLexStatus_Success)
            {
                currentTokenLength = 0;
            }
            
            continue;
        }

        // we now have to presume that more than one token are sat right next to each other
        // let's look for our first one-char symbol, then split it from whatever it was sat after
        for(size_t j = 1; j < currentTokenLength; ++j)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck + j));
            if(status == totemLexStatus_Success)
            {
                // we found a symbol - now lex whatever was in front of it
                TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexReservedWordToken(list, toCheck, j));
                if(status != totemLexStatus_Success)
                {
                    TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexNumberOrIdentifierToken(list, toCheck, j));
                }
                
                // swap previous two tokens around so they're in order
                size_t numTokens = totemMemoryBuffer_GetNumObjects(&list->Tokens);
                totemToken *current = totemMemoryBuffer_Get(&list->Tokens, numTokens - 1);
                totemToken *last = totemMemoryBuffer_Get(&list->Tokens, numTokens - 2);
                totemToken swap;
                memcpy(&swap, current, sizeof(totemToken));
                memcpy(current, last, sizeof(totemToken));
                memcpy(last, &swap, sizeof(totemToken));
                
                currentTokenLength = 0;
            }
        }
    }
    
    totemToken *endToken;
    TOTEM_LEX_ALLOC(endToken, list);
    endToken->Type = totemTokenType_EndScript;
    endToken->Value.Value = NULL;
    endToken->Value.Length = 0;
    endToken->Position.LineNumber = currentLine;
    endToken->Position.CharNumber = currentLineChar;
    
    return totemLexStatus_Success;
}

totemToken *totemTokenList_Alloc(totemTokenList *list)
{
    size_t index = totemMemoryBuffer_GetNumObjects(&list->Tokens);
    if(totemMemoryBuffer_Secure(&list->Tokens, 1) == totemBool_True)
    {
        totemToken *currentToken = totemMemoryBuffer_Get(&list->Tokens, index);
        memset(currentToken, 0, sizeof(totemToken));
        currentToken->Value.Value = NULL;
        currentToken->Value.Length = 0;
        currentToken->Type = totemTokenType_None;
        return currentToken;
    }
    
    return NULL;
}

totemLexStatus totemTokenList_LexNumberOrIdentifierToken(totemTokenList *list, const char *toCheck, size_t length)
{
    totemToken *currentToken = NULL;
    TOTEM_LEX_ALLOC(currentToken, list);
    currentToken->Type = totemTokenType_Number;
    currentToken->Value.Value = toCheck;
    currentToken->Value.Length = (uint32_t)length;
    
    for(size_t j = 0; j < length; ++j)
    {
        if(!isdigit(toCheck[j]))
        {
            currentToken->Type = totemTokenType_Identifier;
            break;
        }
    }
    
    return totemLexStatus_Success;
}

totemLexStatus totemTokenList_LexReservedWordToken(totemTokenList *list, const char *buffer, size_t length)
{
    for(size_t i = 0 ; i < TOTEM_ARRAYSIZE(s_reservedWordValues); ++i)
    {
        size_t tokenLength = strlen(s_reservedWordValues[i].Value);
        if(tokenLength == length && strncmp(buffer, s_reservedWordValues[i].Value, length) == 0)
        {
            totemToken *token = NULL;
            TOTEM_LEX_ALLOC(token, list);
            token->Type = s_reservedWordValues[i].Type;
            token->Value.Value = buffer;
            token->Value.Length = (uint32_t)tokenLength;
            return totemLexStatus_Success;
        }
    }
    
    return totemLexStatus_Failure;
}

totemLexStatus totemTokenList_LexSymbolToken(totemTokenList *list, const char *toCheck)
{
    for(size_t i = 0 ; i < TOTEM_ARRAYSIZE(s_symbolTokenValues); ++i)
    {
        if(toCheck[0] == s_symbolTokenValues[i].Value[0])
        {
            // combine whitespace
            if(s_symbolTokenValues[i].Type == totemTokenType_Whitespace)
            {
                size_t numTokens = totemMemoryBuffer_GetNumObjects(&list->Tokens);
                if(numTokens > 0)
                {
                    totemToken *last = totemMemoryBuffer_Get(&list->Tokens, numTokens - 1);
                    if(last->Type == totemTokenType_Whitespace)
                    {
                        last->Value.Length++;
                        return totemLexStatus_Success;
                    }
                }
            }
            
            totemToken *token = NULL;
            TOTEM_LEX_ALLOC(token, list);
            token->Type = s_symbolTokenValues[i].Type;
            token->Value.Value = toCheck;
            token->Value.Length = 1;
            return totemLexStatus_Success;
        }
    }
    
    return totemLexStatus_Failure;
}

void *totemParseTree_Alloc(totemParseTree *tree, size_t objectSize)
{
    return totemMemoryBlock_Alloc(&tree->LastMemBlock, objectSize);
}

void totemParseTree_Cleanup(totemParseTree *tree)
{
    while(tree->LastMemBlock)
    {
        void *ptr = tree->LastMemBlock;
        tree->LastMemBlock = tree->LastMemBlock->Prev;
        totem_Free(ptr);
    }
    
    tree->FirstBlock = NULL;
    tree->CurrentToken = NULL;
}

totemParseStatus totemParseStatus_Break(totemParseStatus status)
{
    return status;
}

/**
 * Create parse tree from token list
 */
totemParseStatus totemParseTree_Parse(totemParseTree *tree, totemTokenList *tokens)
{
    totemBlockPrototype *lastBlock = NULL;
    
    for(tree->CurrentToken = (totemToken*)tokens->Tokens.Data; tree->CurrentToken->Type != totemTokenType_EndScript; )
    {
        totemBlockPrototype *block;
        TOTEM_PARSE_ALLOC(block, totemBlockPrototype, tree);
        
        totemParseStatus status = totemBlockPrototype_Parse(block, &tree->CurrentToken, tree);
        if(status != totemParseStatus_Success)
        {
            return status;
        }
        
        if(lastBlock == NULL)
        {
            tree->FirstBlock = block;
        }
        else
        {
            lastBlock->Next = block;
        }
        
        lastBlock = block;
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemBlockPrototype_Parse(totemBlockPrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    
    switch((*token)->Type)
    {
        case totemTokenType_Function:
        {
            block->Type = totemBlockType_FunctionDeclaration;
            TOTEM_PARSE_ALLOC(block->FuncDec, totemFunctionDeclarationPrototype, tree);
            return totemFunctionDeclarationPrototype_Parse(block->FuncDec, token, tree);
        }
            
        default:
        {
            block->Type = totemBlockType_Statement;
            TOTEM_PARSE_ALLOC(block->Statement, totemStatementPrototype, tree);
            return totemStatementPrototype_Parse(block->Statement, token, tree);
        }
    }
}

totemParseStatus totemStatementPrototype_Parse(totemStatementPrototype *statement, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, statement);
    
    memcpy(&statement->Position, &(*token)->Position, sizeof(totemBufferPositionInfo));
    
    switch((*token)->Type)
    {
        case totemTokenType_While:
        {
            statement->Type = totemStatementType_WhileLoop;
            TOTEM_PARSE_ALLOC(statement->WhileLoop, totemWhileLoopPrototype, tree);
            return totemWhileLoopPrototype_Parse(statement->WhileLoop, token, tree);
        }
            
        case totemTokenType_Do:
        {
            statement->Type = totemStatementType_DoWhileLoop;
            TOTEM_PARSE_ALLOC(statement->DoWhileLoop, totemDoWhileLoopPrototype, tree);
            return totemDoWhileLoopPrototype_Parse(statement->DoWhileLoop, token, tree);
        }
            
        case totemTokenType_For:
        {
            statement->Type = totemStatementType_ForLoop;
            TOTEM_PARSE_ALLOC(statement->ForLoop, totemForLoopPrototype, tree);
            return totemForLoopPrototype_Parse(statement->ForLoop, token, tree);
        }
            
        case totemTokenType_If:
        {
            statement->Type = totemStatementType_IfBlock;
            TOTEM_PARSE_ALLOC(statement->IfBlock, totemIfBlockPrototype, tree);
            return totemIfBlockPrototype_Parse(statement->IfBlock, token, tree);
        }
            
        case totemTokenType_Switch:
        {
            statement->Type = totemStatementType_SwitchBlock;
            TOTEM_PARSE_ALLOC(statement->SwitchBlock, totemSwitchBlockPrototype, tree);
            return totemSwitchBlockPrototype_Parse(statement->SwitchBlock, token, tree);
        }
            
        case totemTokenType_Return:
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_SKIPWHITESPACE(token);
            
            statement->Type = totemStatementType_Return;
            TOTEM_PARSE_ALLOC(statement->Return, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(statement->Return, token, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            return totemParseStatus_Success;
        }
            
        default:
        {
            statement->Type = totemStatementType_Simple;
            TOTEM_PARSE_ALLOC(statement->Expression, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(statement->Expression, token, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            return totemParseStatus_Success;
        }
    }
}

totemParseStatus totemStatementPrototype_ParseInSet(totemStatementPrototype **first, totemStatementPrototype **last, totemToken **token, totemParseTree *tree)
{
    totemStatementPrototype *statement = NULL;
    TOTEM_PARSE_ALLOC(statement, totemStatementPrototype, tree);
    
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_Parse(statement, token, tree))
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if(*first == NULL)
    {
        *first = statement;
    }
    else
    {
        (*last)->Next = statement;
    }
    
    *last = statement;
    return totemParseStatus_Success;
}

totemParseStatus totemStatementPrototype_ParseSet(totemStatementPrototype **first, totemStatementPrototype **last, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LCBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
	while((*token)->Type != totemTokenType_RCBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseInSet(first, last, token, tree));
    }
    
    (*token)++;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterList(totemVariablePrototype **first, totemVariablePrototype **last, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    while((*token)->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterInList(first, last, token, tree));
        
        if((*token)->Type == totemTokenType_Comma)
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_SKIPWHITESPACE(token);
            TOTEM_PARSE_ENFORCENOTTOKEN(token, totemTokenType_RBracket);
        }
    }
    
    (*token)++;
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_ParseParameterList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    while((*token)->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterInList(first, last, token, tree));
        
        if((*token)->Type == totemTokenType_Comma)
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_SKIPWHITESPACE(token);
            TOTEM_PARSE_ENFORCENOTTOKEN(token, totemTokenType_RBracket);
        }
    }
    
    (*token)++;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterInList(totemVariablePrototype **first, totemVariablePrototype **last, totemToken **token, totemParseTree *tree)
{
    totemVariablePrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemVariablePrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_Parse(parameter, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemTokenType nextType = (*token)->Type;
    
    if(nextType != totemTokenType_RBracket && nextType != totemTokenType_Comma)
    {
        return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
    }
    
    if(*first == NULL)
    {
        *first = parameter;
    }
    else
    {
        (*last)->Next = parameter;
    }
    
    *last = parameter;
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_ParseParameterInList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemToken **token, totemParseTree *tree)
{
    totemExpressionPrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(parameter, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type != totemTokenType_RBracket && (*token)->Type != totemTokenType_Comma)
    {
        return totemParseStatus_UnexpectedToken;
    }
    
    if(*first == NULL)
    {
        *first = parameter;
    }
    else
    {
        (*last)->Next = parameter;
    }
    
    *last = parameter;
    return totemParseStatus_Success;
}

totemParseStatus totemWhileLoopPrototype_Parse(totemWhileLoopPrototype *loop, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, loop);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemDoWhileLoopPrototype_Parse(totemDoWhileLoopPrototype *loop, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, loop);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Do);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    loop->StatementsStart = firstStatement;
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
    
    (*token)++;
    return totemParseStatus_Success;
}

totemParseStatus totemIfBlockPrototype_Parse(totemIfBlockPrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    block->ElseType = totemIfElseBlockType_None;
    
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_If);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, token, tree));
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    block->StatementsStart = firstStatement;
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type == totemTokenType_Else)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        
        switch((*token)->Type)
        {
            case totemTokenType_If:
            {
                block->ElseType = totemIfElseBlockType_ElseIf;
                TOTEM_PARSE_ALLOC(block->IfElseBlock, totemIfBlockPrototype, tree);
                TOTEM_PARSE_CHECKRETURN(totemIfBlockPrototype_Parse(block->IfElseBlock, token, tree));
                break;
            }
                
            case totemTokenType_LCBracket:
            {
                block->ElseType = totemIfElseBlockType_Else;
                TOTEM_PARSE_ALLOC(block->ElseBlock, totemElseBlockPrototype, tree);
                TOTEM_PARSE_COPYPOSITION(token, block->ElseBlock);
                
                totemStatementPrototype *firstElseStatement = NULL, *lastElseStatement = NULL;
                TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstElseStatement, &lastElseStatement, token, tree));
                block->ElseBlock->StatementsStart = firstElseStatement;
                break;
            }
                
            default:
                return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
        }
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemForLoopPrototype_Parse(totemForLoopPrototype *loop, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, loop);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_For);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    TOTEM_PARSE_ALLOC(loop->Initialisation, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Initialisation, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->Condition, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Condition, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->AfterThought, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->AfterThought, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type == totemTokenType_RBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemSwitchBlockPrototype_Parse(totemSwitchBlockPrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Switch);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LCBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    totemSwitchCasePrototype *first = NULL, *last = NULL;
    while((*token)->Type != totemTokenType_RCBracket)
    {
        totemSwitchCasePrototype *switchCase = NULL;
        TOTEM_PARSE_ALLOC(switchCase, totemSwitchCasePrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemSwitchCasePrototype_Parse(switchCase, token, tree));
        TOTEM_PARSE_SKIPWHITESPACE(token);
        
        if(first == NULL)
        {
            first = switchCase;
        }
        else
        {
            last->Next = switchCase;
        }
        
        last = switchCase;
    }
    (*token)++;
    block->CasesStart = first;
    
    return totemParseStatus_Success;
}

totemParseStatus totemSwitchCasePrototype_Parse(totemSwitchCasePrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    
    if((*token)->Type != totemTokenType_Default)
    {
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Case);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        
        block->Type = totemSwitchCaseType_Expression;
        TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, token, tree));
    }
    else
    {
        block->Type = totemSwitchCaseType_Default;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Colon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    
    while((*token)->Type != totemTokenType_Break && (*token)->Type != totemTokenType_Case && (*token)->Type != totemTokenType_Default)
    {
        TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseInSet(&firstStatement, &lastStatement, token, tree));
    }
    
    block->StatementsStart = firstStatement;
    
    if((*token)->Type == totemTokenType_Break)
    {
        block->HasBreak = totemBool_True;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    else
    {
        block->HasBreak = totemBool_False;
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionDeclarationPrototype_Parse(totemFunctionDeclarationPrototype *func, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, func);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Function);
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    TOTEM_PARSE_ALLOC(func->Identifier, totemString, tree);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(func->Identifier, token, tree));
    
    totemVariablePrototype *firstParameter = NULL, *lastParameter = NULL;
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterList(&firstParameter, &lastParameter, token, tree));
    func->ParametersStart = firstParameter;
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    func->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_Parse(totemExpressionPrototype *expression, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, expression);
    TOTEM_PARSE_CHECKRETURN(totemPreUnaryOperatorType_Parse(&expression->PreUnaryOperator, token, tree));
    
    if((*token)->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        expression->LValueType = totemLValueType_Expression;
        TOTEM_PARSE_ALLOC(expression->LValueExpression, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->LValueExpression, token, tree));
        
        TOTEM_PARSE_SKIPWHITESPACE(token);
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_RBracket);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    else
    {
        expression->LValueType = totemLValueType_Argument;
        TOTEM_PARSE_ALLOC(expression->LValueArgument, totemArgumentPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemArgumentPrototype_Parse(expression->LValueArgument, token, tree));
    }
    
    TOTEM_PARSE_CHECKRETURN(totemPostUnaryOperatorType_Parse(&expression->PostUnaryOperator, token, tree));
    TOTEM_PARSE_CHECKRETURN(totemBinaryOperatorType_Parse(&expression->BinaryOperator, token, tree));
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        TOTEM_PARSE_ALLOC(expression->RValue, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->RValue, token, tree));
    }
    
    // TODO: reorder with operator precedence
    
    return totemParseStatus_Success;
}

totemParseStatus totemPreUnaryOperatorType_Parse(totemPreUnaryOperatorType *type, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    switch((*token)->Type)
    {
        case totemTokenType_Plus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Plus);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemPreUnaryOperatorType_Inc;
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            if((*token)->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                *type = totemPreUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            *type = totemPreUnaryOperatorType_Negative;
            return totemParseStatus_Success;
            
        case totemTokenType_Not:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemPreUnaryOperatorType_LogicalNegate;
            return totemParseStatus_Success;
            
        default:
            *type = totemPreUnaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemPostUnaryOperatorType_Parse(totemPostUnaryOperatorType *type, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    switch((*token)->Type)
    {
        case totemTokenType_Plus:
            if((*token + 1)->Type == totemTokenType_Plus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                *type = totemPostUnaryOperatorType_Inc;
                return totemParseStatus_Success;
            }
            
            *type = totemPostUnaryOperatorType_None;
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            if((*token + 1)->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                *type = totemPostUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            *type  = totemPostUnaryOperatorType_None;
            return totemParseStatus_Success;
            
        default:
            *type  = totemPostUnaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemBinaryOperatorType_Parse(totemBinaryOperatorType *type, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    switch((*token)->Type)
    {
        case totemTokenType_Assign:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_Equals;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Assign;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_MoreThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_MoreThanEquals;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_MoreThan;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_LessThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_LessThanEquals;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_LessThan;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Divide:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_DivideAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Divide;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_MinusAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Minus;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Multiply:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_MultiplyAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Multiply;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Plus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_PlusAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Plus;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_And:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_And);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemBinaryOperatorType_LogicalAnd;
            return totemParseStatus_Success;
            
        case totemTokenType_Or:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Or);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemBinaryOperatorType_LogicalOr;
            return totemParseStatus_Success;
            
        default:
            *type = totemBinaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemVariablePrototype_Parse(totemVariablePrototype *variable, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, variable);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Variable);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&variable->Identifier, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionCallPrototype_Parse(totemFunctionCallPrototype *call, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&call->Identifier, token, tree));
    
    totemExpressionPrototype *first = NULL, *last = NULL;
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterList(&first, &last, token, tree));
    call->ParametersStart = first;
    return totemParseStatus_Success;
}

totemParseStatus totemArgumentPrototype_Parse(totemArgumentPrototype *argument, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, argument);
    
    // variable
    if((*token)->Type == totemTokenType_Variable)
    {
        argument->Type = totemArgumentType_Variable;
        TOTEM_PARSE_ALLOC(argument->Variable, totemVariablePrototype, tree);
        return totemVariablePrototype_Parse(argument->Variable, token, tree);
    }
    
    // number constant
    if((*token)->Type == totemTokenType_Number)
    {
        argument->Number.Value = (*token)->Value.Value;
        argument->Type = totemArgumentType_Number;
        
        while((*token)->Type == totemTokenType_Number || (*token)->Type == totemTokenType_Dot)
        {
            argument->Number.Length += (*token)->Value.Length;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        }
        
        (*token)--;
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Number);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        
        return totemParseStatus_Success;
    }
    
    // string constant
    if((*token)->Type == totemTokenType_DoubleQuote)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        
        argument->Type = totemArgumentType_String;
        const char *begin = (*token)->Value.Value;
        uint32_t len = 0;
        
        while((*token)->Type != totemTokenType_DoubleQuote)
        {
            len += (*token)->Value.Length;
            
            if((*token)->Type == totemTokenType_Backslash)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                
                if((*token)->Type == totemTokenType_DoubleQuote)
                {
                    len += (*token)->Value.Length;
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                }
            }
            else
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            }
        }
        
        argument->String.Value = begin;
        argument->String.Length = len;
        
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        return totemParseStatus_Success;
    }
    
    // function call
    if((*token)->Type == totemTokenType_Identifier)
    {
        argument->Type = totemArgumentType_FunctionCall;
        TOTEM_PARSE_ALLOC(argument->FunctionCall, totemFunctionCallPrototype, tree);
        return totemFunctionCallPrototype_Parse(argument->FunctionCall, token, tree);
    }
    
    // bool constant
    if((*token)->Type == totemTokenType_True || (*token)->Type == totemTokenType_False)
    {
        argument->Type = totemArgumentType_Number;
        totemString_FromLiteral(&argument->Number, (*token)->Type == totemTokenType_True ? "1" : "0");
        
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        return totemParseStatus_Success;
    }
    
    return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
}

totemParseStatus totemString_ParseIdentifier(totemString *string, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Identifier);
    
    const char *start = (*token)->Value.Value;
    uint32_t length = 0;
    
    while((*token)->Type == totemTokenType_Identifier || (*token)->Type == totemTokenType_Colon)
    {
        length += (*token)->Value.Length;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    string->Length = length;
    string->Value = start;
    
    (*token)--;
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Identifier);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    return totemParseStatus_Success;
}

const char *totemParseStatus_Describe(totemParseStatus status)
{
    switch (status)
    {
        TOTEM_STRINGIFY_CASE(totemParseStatus_OutOfMemory);
        TOTEM_STRINGIFY_CASE(totemParseStatus_Success);
        TOTEM_STRINGIFY_CASE(totemParseStatus_UnexpectedToken);
        TOTEM_STRINGIFY_CASE(totemParseStatus_ValueTooLarge);
    }
    
    return "UNKNOWN";
}

const char *totemLexStatus_Describe(totemLexStatus status)
{
    switch(status)
    {
        TOTEM_STRINGIFY_CASE(totemLexStatus_OutOfMemory);
        TOTEM_STRINGIFY_CASE(totemLexStatus_Success);
        TOTEM_STRINGIFY_CASE(totemLexStatus_Failure);
    }
    
    return "UNKNOWN";
}

const char *totemTokenType_Describe(totemTokenType type)
{
    switch(type)
    {
        TOTEM_STRINGIFY_CASE(totemTokenType_And);
        TOTEM_STRINGIFY_CASE(totemTokenType_Assign);
        TOTEM_STRINGIFY_CASE(totemTokenType_Backslash);
        TOTEM_STRINGIFY_CASE(totemTokenType_Break);
        TOTEM_STRINGIFY_CASE(totemTokenType_Case);
        TOTEM_STRINGIFY_CASE(totemTokenType_Colon);
        TOTEM_STRINGIFY_CASE(totemTokenType_Comma);
        TOTEM_STRINGIFY_CASE(totemTokenType_Default);
        TOTEM_STRINGIFY_CASE(totemTokenType_Divide);
        TOTEM_STRINGIFY_CASE(totemTokenType_Do);
        TOTEM_STRINGIFY_CASE(totemTokenType_Dot);
        TOTEM_STRINGIFY_CASE(totemTokenType_DoubleQuote);
        TOTEM_STRINGIFY_CASE(totemTokenType_Else);
        TOTEM_STRINGIFY_CASE(totemTokenType_EndScript);
        TOTEM_STRINGIFY_CASE(totemTokenType_False);
        TOTEM_STRINGIFY_CASE(totemTokenType_For);
        TOTEM_STRINGIFY_CASE(totemTokenType_Function);
        TOTEM_STRINGIFY_CASE(totemTokenType_Identifier);
        TOTEM_STRINGIFY_CASE(totemTokenType_If);
        TOTEM_STRINGIFY_CASE(totemTokenType_LBracket);
        TOTEM_STRINGIFY_CASE(totemTokenType_LCBracket);
        TOTEM_STRINGIFY_CASE(totemTokenType_LessThan);
        TOTEM_STRINGIFY_CASE(totemTokenType_LSBracket);
        TOTEM_STRINGIFY_CASE(totemTokenType_Max);
        TOTEM_STRINGIFY_CASE(totemTokenType_Minus);
        TOTEM_STRINGIFY_CASE(totemTokenType_MoreThan);
        TOTEM_STRINGIFY_CASE(totemTokenType_Multiply);
        TOTEM_STRINGIFY_CASE(totemTokenType_None);
        TOTEM_STRINGIFY_CASE(totemTokenType_Not);
        TOTEM_STRINGIFY_CASE(totemTokenType_Number);
        TOTEM_STRINGIFY_CASE(totemTokenType_Or);
        TOTEM_STRINGIFY_CASE(totemTokenType_Plus);
        TOTEM_STRINGIFY_CASE(totemTokenType_PowerTo);
        TOTEM_STRINGIFY_CASE(totemTokenType_RBracket);
        TOTEM_STRINGIFY_CASE(totemTokenType_RCBracket);
        TOTEM_STRINGIFY_CASE(totemTokenType_Return);
        TOTEM_STRINGIFY_CASE(totemTokenType_RSBracket);
        TOTEM_STRINGIFY_CASE(totemTokenType_Semicolon);
        TOTEM_STRINGIFY_CASE(totemTokenType_SingleQuote);
        TOTEM_STRINGIFY_CASE(totemTokenType_Slash);
        TOTEM_STRINGIFY_CASE(totemTokenType_Switch);
        TOTEM_STRINGIFY_CASE(totemTokenType_True);
        TOTEM_STRINGIFY_CASE(totemTokenType_Variable);
        TOTEM_STRINGIFY_CASE(totemTokenType_While);
        TOTEM_STRINGIFY_CASE(totemTokenType_Whitespace);
    }
    
    return "UNKNOWN";
}