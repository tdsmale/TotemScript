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

#define TOTEM_PARSE_SKIPWHITESPACE(token) while(token->Type == totemTokenType_Whitespace) { token++; }
#define TOTEM_PARSE_COPYPOSITION(token, dest) memcpy(&dest->Position, &token->Position, sizeof(totemBufferPositionInfo))
#define TOTEM_PARSE_ALLOC(dest, type, tree) dest = (type*)totemParseTree_Alloc(tree, sizeof(type)); if(!dest) return totemParseStatus_Break(totemParseStatus_OutOfMemory);
#define TOTEM_PARSE_CHECKRETURN(exp) { totemParseStatus s = exp; if(s != totemParseStatus_Success) return s; }
#define TOTEM_PARSE_ENFORCETOKEN(token, type) if(token->Type != type) return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
#define TOTEM_PARSE_ENFORCENOTTOKEN(token, type) if(token->Type == type) return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
#define TOTEM_PARSE_INC_NOT_ENDSCRIPT(token) token++; TOTEM_PARSE_ENFORCENOTTOKEN(token, totemTokenType_EndScript);

void *totemParseTree_Alloc(totemParseTree *tree, size_t objectSize)
{
    return totemMemoryBlock_Alloc(&tree->LastMemBlock, objectSize);
}

void totemParseTree_Cleanup(totemParseTree *tree)
{
    totemMemoryBlock_Cleanup(&tree->LastMemBlock);
    tree->FirstBlock = NULL;
    tree->CurrentToken = NULL;
}

void totemParseTree_Reset(totemParseTree *tree)
{
    totemParseTree_Cleanup(tree);
}

void totemParseTree_Init(totemParseTree *tree)
{
    memset(tree, 0, sizeof(totemParseTree));
    totemParseTree_Reset(tree);
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
        
        totemParseStatus status = totemBlockPrototype_Parse(block, tree);
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

totemParseStatus totemBlockPrototype_Parse(totemBlockPrototype *block, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, block);
    
    switch (tree->CurrentToken->Type)
    {
        case totemTokenType_Function:
        {
            block->Type = totemBlockType_FunctionDeclaration;
            TOTEM_PARSE_ALLOC(block->FuncDec, totemFunctionDeclarationPrototype, tree);
            return totemFunctionDeclarationPrototype_Parse(block->FuncDec, tree, totemBool_False);
        }
            
        default:
        {
            block->Type = totemBlockType_Statement;
            TOTEM_PARSE_ALLOC(block->Statement, totemStatementPrototype, tree);
            return totemStatementPrototype_Parse(block->Statement, tree);
        }
    }
}

totemParseStatus totemStatementPrototype_Parse(totemStatementPrototype *statement, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, statement);
    
    memcpy(&statement->Position, &tree->CurrentToken->Position, sizeof(totemBufferPositionInfo));
    
    switch (tree->CurrentToken->Type)
    {
        case totemTokenType_While:
        {
            statement->Type = totemStatementType_WhileLoop;
            TOTEM_PARSE_ALLOC(statement->WhileLoop, totemWhileLoopPrototype, tree);
            return totemWhileLoopPrototype_Parse(statement->WhileLoop, tree);
        }
            
        case totemTokenType_Do:
        {
            statement->Type = totemStatementType_DoWhileLoop;
            TOTEM_PARSE_ALLOC(statement->DoWhileLoop, totemDoWhileLoopPrototype, tree);
            return totemDoWhileLoopPrototype_Parse(statement->DoWhileLoop, tree);
        }
            
        case totemTokenType_For:
        {
            statement->Type = totemStatementType_ForLoop;
            TOTEM_PARSE_ALLOC(statement->ForLoop, totemForLoopPrototype, tree);
            return totemForLoopPrototype_Parse(statement->ForLoop, tree);
        }
            
        case totemTokenType_If:
        {
            statement->Type = totemStatementType_IfBlock;
            TOTEM_PARSE_ALLOC(statement->IfBlock, totemIfBlockPrototype, tree);
            return totemIfBlockPrototype_Parse(statement->IfBlock, tree);
        }
            
        case totemTokenType_Return:
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            
            statement->Type = totemStatementType_Return;
            TOTEM_PARSE_ALLOC(statement->Return, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(statement->Return, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
            tree->CurrentToken++;
            return totemParseStatus_Success;
        }
            
        default:
        {
            statement->Type = totemStatementType_Simple;
            TOTEM_PARSE_ALLOC(statement->Simple, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(statement->Simple, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
            tree->CurrentToken++;
            return totemParseStatus_Success;
        }
    }
}

totemParseStatus totemStatementPrototype_ParseInSet(totemStatementPrototype **first, totemStatementPrototype **last, totemParseTree *tree)
{
    totemStatementPrototype *statement = NULL;
    TOTEM_PARSE_ALLOC(statement, totemStatementPrototype, tree);
    
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_Parse(statement, tree))
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
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

totemParseStatus totemStatementPrototype_ParseSet(totemStatementPrototype **first, totemStatementPrototype **last, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LCBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    while (tree->CurrentToken->Type != totemTokenType_RCBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseInSet(first, last, tree));
    }
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterList(totemVariablePrototype **first, totemVariablePrototype **last, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    while (tree->CurrentToken->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterInList(first, last, tree));
        
        if (tree->CurrentToken->Type == totemTokenType_Comma)
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCENOTTOKEN(tree->CurrentToken, totemTokenType_RBracket);
        }
    }
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_ParseParameterList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    while (tree->CurrentToken->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterInList(first, last, tree));
        
        if (tree->CurrentToken->Type == totemTokenType_Comma)
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCENOTTOKEN(tree->CurrentToken, totemTokenType_RBracket);
        }
    }
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterInList(totemVariablePrototype **first, totemVariablePrototype **last, totemParseTree *tree)
{
    totemVariablePrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemVariablePrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_Parse(parameter, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemTokenType nextType = tree->CurrentToken->Type;
    
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

totemParseStatus totemExpressionPrototype_ParseParameterInList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemParseTree *tree)
{
    totemExpressionPrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(parameter, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type != totemTokenType_RBracket && tree->CurrentToken->Type != totemTokenType_Comma)
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

totemParseStatus totemWhileLoopPrototype_Parse(totemWhileLoopPrototype *loop, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, loop);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemDoWhileLoopPrototype_Parse(totemDoWhileLoopPrototype *loop, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, loop);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Do);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    loop->StatementsStart = firstStatement;
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemIfBlockPrototype_Parse(totemIfBlockPrototype *block, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, block);
    block->ElseType = totemIfElseBlockType_None;
    
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_If);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, tree));
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    block->StatementsStart = firstStatement;
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_Else)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
        
        switch(tree->CurrentToken->Type)
        {
            case totemTokenType_If:
            {
                block->ElseType = totemIfElseBlockType_ElseIf;
                TOTEM_PARSE_ALLOC(block->IfElseBlock, totemIfBlockPrototype, tree);
                TOTEM_PARSE_CHECKRETURN(totemIfBlockPrototype_Parse(block->IfElseBlock, tree));
                break;
            }
                
            case totemTokenType_LCBracket:
            {
                block->ElseType = totemIfElseBlockType_Else;
                TOTEM_PARSE_ALLOC(block->ElseBlock, totemElseBlockPrototype, tree);
                TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, block->ElseBlock);
                
                totemStatementPrototype *firstElseStatement = NULL, *lastElseStatement = NULL;
                TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstElseStatement, &lastElseStatement, tree));
                block->ElseBlock->StatementsStart = firstElseStatement;
                break;
            }
                
            default:
                return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
        }
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemForLoopPrototype_Parse(totemForLoopPrototype *loop, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, loop);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_For);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
    TOTEM_PARSE_ALLOC(loop->Initialisation, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Initialisation, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->Condition, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Condition, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->AfterThought, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->AfterThought, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_RBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionDeclarationPrototype_Parse(totemFunctionDeclarationPrototype *func, totemParseTree *tree, totemBool isAnonymous)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Function);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, func);
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(!isAnonymous)
    {
        TOTEM_PARSE_ALLOC(func->Identifier, totemString, tree);
        TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(func->Identifier, tree, totemBool_True));
    }
    
    totemVariablePrototype *firstParameter = NULL, *lastParameter = NULL;
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterList(&firstParameter, &lastParameter, tree));
    func->ParametersStart = firstParameter;
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    func->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_Parse(totemExpressionPrototype *expression, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, expression);
    
    // pre-unary operators
    for(totemPreUnaryOperatorPrototype **preUnaryType = &expression->PreUnaryOperators; totemBool_True; /* nada */)
    {
        TOTEM_PARSE_CHECKRETURN(totemPreUnaryOperatorPrototype_Parse(preUnaryType, tree));
        if(!(*preUnaryType))
        {
            break;
        }
        else
        {
            preUnaryType = &(*preUnaryType)->Next;
        }
    }
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
        expression->LValueType = totemLValueType_Expression;
        TOTEM_PARSE_ALLOC(expression->LValueExpression, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->LValueExpression, tree));
        
        TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
        TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RBracket);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    else
    {
        expression->LValueType = totemLValueType_Argument;
        TOTEM_PARSE_ALLOC(expression->LValueArgument, totemArgumentPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemArgumentPrototype_Parse(expression->LValueArgument, tree));
    }
    
    // post-unary operators
    for(totemPostUnaryOperatorPrototype **postUnaryType = &expression->PostUnaryOperators; totemBool_True; /* nada */)
    {
        TOTEM_PARSE_CHECKRETURN(totemPostUnaryOperatorPrototype_Parse(postUnaryType, tree));
        if(!(*postUnaryType))
        {
            break;
        }
        else
        {
            postUnaryType = &(*postUnaryType)->Next;
        }
    }
    
    TOTEM_PARSE_CHECKRETURN(totemBinaryOperatorType_Parse(&expression->BinaryOperator, tree));
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        TOTEM_PARSE_ALLOC(expression->RValue, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->RValue, tree));
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemPreUnaryOperatorPrototype_Parse(totemPreUnaryOperatorPrototype **type, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Plus:
            TOTEM_PARSE_ALLOC(*type, totemPreUnaryOperatorPrototype, tree);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Plus);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            (*type)->Type = totemPreUnaryOperatorType_Inc;
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_ALLOC(*type, totemPreUnaryOperatorPrototype, tree);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            if(tree->CurrentToken->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                (*type)->Type = totemPreUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            (*type)->Type = totemPreUnaryOperatorType_Negative;
            return totemParseStatus_Success;
            
        case totemTokenType_Not:
            TOTEM_PARSE_ALLOC(*type, totemPreUnaryOperatorPrototype, tree);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            (*type)->Type = totemPreUnaryOperatorType_LogicalNegate;
            return totemParseStatus_Success;
            
        default:
            return totemParseStatus_Success;
    }
}

totemParseStatus totemPostUnaryOperatorPrototype_Parse(totemPostUnaryOperatorPrototype **type, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Plus:
            if ((tree->CurrentToken + 1)->Type == totemTokenType_Plus)
            {
                TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
                
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                (*type)->Type = totemPostUnaryOperatorType_Inc;
                return totemParseStatus_Success;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            if((tree->CurrentToken + 1)->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                (*type)->Type = totemPostUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_LBracket:
            TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
            (*type)->Type = totemPostUnaryOperatorType_Invocation;
            
            // parse parameters
            totemExpressionPrototype *first = NULL, *last = NULL;
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterList(&first, &last, tree));
            (*type)->InvocationParametersStart = first;
            
            return totemParseStatus_Success;
            
        case totemTokenType_Dot:
        {
            TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
            (*type)->Type = totemPostUnaryOperatorType_ArrayAccess;
            
            // parse identifier
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ALLOC((*type)->ArrayAccess, totemExpressionPrototype, tree);
            (*type)->ArrayAccess->LValueType = totemLValueType_Argument;
            TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, (*type)->ArrayAccess);
            
            TOTEM_PARSE_ALLOC((*type)->ArrayAccess->LValueArgument, totemArgumentPrototype, tree);
            TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, (*type)->ArrayAccess->LValueArgument);
            TOTEM_PARSE_ALLOC((*type)->ArrayAccess->LValueArgument->String, totemString, tree);
            (*type)->ArrayAccess->LValueArgument->Type = totemArgumentType_String;
            
            TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier((*type)->ArrayAccess->LValueArgument->String, tree, totemBool_True));
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            return totemParseStatus_Success;
        }
            
        case totemTokenType_LSBracket:
        {
            TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
            (*type)->Type = totemPostUnaryOperatorType_ArrayAccess;
            
            // parse index
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ALLOC((*type)->ArrayAccess, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse((*type)->ArrayAccess, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RSBracket);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            
            return totemParseStatus_Success;
        }
            
        default:
            return totemParseStatus_Success;
    }
}

totemParseStatus totemBinaryOperatorType_Parse(totemBinaryOperatorType *type, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Assign:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_Equals;
                break;
                
            default:
                *type = totemBinaryOperatorType_Assign;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_MoreThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_MoreThanEquals;
                break;
                
            default:
                *type = totemBinaryOperatorType_MoreThan;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_LessThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_LessThan:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_Shift;
                break;
                
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_LessThanEquals;
                break;
                
            default:
                *type = totemBinaryOperatorType_LessThan;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Divide:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_DivideAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Divide;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_MinusAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Minus;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Multiply:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_MultiplyAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Multiply;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Plus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_PlusAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Plus;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_And:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_And);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_LogicalAnd;
            return totemParseStatus_Success;
            
        case totemTokenType_Or:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Or);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_LogicalOr;
            return totemParseStatus_Success;
            
        case totemTokenType_Is:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_IsType;
            return totemParseStatus_Success;
            
        case totemTokenType_As:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_AsType;
            return totemParseStatus_Success;
            
        case totemTokenType_Not:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch (tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_NotEquals;
                break;
                
            default:
                *type = totemBinaryOperatorType_None;
                tree->CurrentToken--;
                break;
        }
            
            return totemParseStatus_Success;
            
        default:
            *type = totemBinaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemVariablePrototype_Parse(totemVariablePrototype *variable, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, variable);
    
    variable->Flags = totemVariablePrototypeFlag_None;
    
    for(totemBool loop = totemBool_True; loop; )
    {
        switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Let:
                if (TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsDeclaration))
                {
                    return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
                }
                
                TOTEM_SETBITS(variable->Flags, totemVariablePrototypeFlag_IsConst);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
                break;
                
            case totemTokenType_Var:
                if (TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsConst))
                {
                    return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
                }
                
                TOTEM_SETBITS(variable->Flags, totemVariablePrototypeFlag_IsDeclaration);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
                break;
                
            case totemTokenType_Local:
                TOTEM_SETBITS(variable->Flags, totemVariablePrototypeFlag_IsLocal);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
                break;
                
            default:
                loop = totemBool_False;
                break;
        }
    }
    
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Variable);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&variable->Identifier, tree, totemBool_False));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionCallPrototype_Parse(totemFunctionCallPrototype *call, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&call->Identifier, tree, totemBool_True));
    
    totemExpressionPrototype *first = NULL, *last = NULL;
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterList(&first, &last, tree));
    call->ParametersStart = first;
    return totemParseStatus_Success;
}

totemParseStatus totemString_ParseNumber(totemString *number, totemParseTree *tree)
{
    number->Length = 0;
    number->Value = tree->CurrentToken->Value.Value;
    
    while(tree->CurrentToken->Type == totemTokenType_Number || tree->CurrentToken->Type == totemTokenType_Dot)
    {
        number->Length += tree->CurrentToken->Value.Length;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
    tree->CurrentToken--;
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Number);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemArgumentPrototype_Parse(totemArgumentPrototype *argument, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, argument);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Function:
        {
            totemToken *reset = tree->CurrentToken;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            
            if(tree->CurrentToken->Type == totemTokenType_LBracket)
            {
                // anonymous function
                tree->CurrentToken = reset;
                argument->Type = totemArgumentType_FunctionDeclaration;
                TOTEM_PARSE_ALLOC(argument->FunctionDeclaration, totemFunctionDeclarationPrototype, tree);
                TOTEM_PARSE_CHECKRETURN(totemFunctionDeclarationPrototype_Parse(argument->FunctionDeclaration, tree, totemBool_True));
            }
            else
            {
                // function type
                argument->DataType = totemPublicDataType_Function;
                argument->Type = totemArgumentType_Type;
            }
            
            break;
        }
            
            // function pointer
        case totemTokenType_At:
            argument->Type = totemArgumentType_FunctionPointer;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ALLOC(argument->FunctionPointer, totemString, tree);
            TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(argument->FunctionPointer, tree, totemBool_True));
            break;
            
            // types
        case totemTokenType_Array:
            argument->DataType = totemPublicDataType_Array;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Int:
            argument->DataType = totemPublicDataType_Int;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Float:
            argument->DataType = totemPublicDataType_Float;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_String:
            argument->DataType = totemPublicDataType_String;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Type:
            argument->DataType = totemPublicDataType_Type;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Object:
            argument->DataType = totemPublicDataType_Object;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Coroutine:
            argument->DataType = totemPublicDataType_Coroutine;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Userdata:
            argument->DataType = totemPublicDataType_Userdata;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Null:
            argument->DataType = totemPublicDataType_Null;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_True:
            argument->DataType = totemPublicDataType_True;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_False:
            argument->DataType = totemPublicDataType_False;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
            // new object
        case totemTokenType_LCBracket:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RCBracket);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            argument->Type = totemArgumentType_NewObject;
            break;
            
            // new array
        case totemTokenType_LSBracket:
            argument->Type = totemArgumentType_NewArray;
            TOTEM_PARSE_ALLOC(argument->NewArray, totemNewArrayPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemNewArrayPrototype_Parse(argument->NewArray, tree));
            break;
            
            // variable
        case totemTokenType_Variable:
        case totemTokenType_Let:
        case totemTokenType_Local:
        case totemTokenType_Var:
            argument->Type = totemArgumentType_Variable;
            TOTEM_PARSE_ALLOC(argument->Variable, totemVariablePrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_Parse(argument->Variable, tree));
            break;
            
            // number
        case totemTokenType_Number:
            argument->Type = totemArgumentType_Number;
            TOTEM_PARSE_ALLOC(argument->Number, totemString, tree);
            TOTEM_PARSE_CHECKRETURN(totemString_ParseNumber(argument->Number, tree));
            break;
            
            // string
        case totemTokenType_DoubleQuote:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            argument->Type = totemArgumentType_String;
            const char *begin = tree->CurrentToken->Value.Value;
            uint32_t len = 0;
            
            while(tree->CurrentToken->Type != totemTokenType_DoubleQuote)
            {
                len += tree->CurrentToken->Value.Length;
                
                if(tree->CurrentToken->Type == totemTokenType_Backslash)
                {
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                    
                    if(tree->CurrentToken->Type == totemTokenType_DoubleQuote)
                    {
                        len += tree->CurrentToken->Value.Length;
                        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                    }
                }
                else
                {
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                }
            }
            
            TOTEM_PARSE_ALLOC(argument->String, totemString, tree);
            argument->String->Value = begin;
            argument->String->Length = len;
            
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
            // function call
        case totemTokenType_Identifier:
            argument->Type = totemArgumentType_FunctionCall;
            TOTEM_PARSE_ALLOC(argument->FunctionCall, totemFunctionCallPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemFunctionCallPrototype_Parse(argument->FunctionCall, tree));
            break;
            
        default:
            return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
    }
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemNewArrayPrototype_Parse(totemNewArrayPrototype *arr, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LSBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(arr->Accessor, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(arr->Accessor, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RSBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemString_ParseIdentifier(totemString *string, totemParseTree *tree, totemBool strict)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    const char *start = tree->CurrentToken->Value.Value;
    uint32_t length = 0;
    
    if(strict)
    {
        TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Identifier);
        
        while(tree->CurrentToken->Type == totemTokenType_Identifier)
        {
            length += tree->CurrentToken->Value.Length;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        }
    }
    else
    {
        while(tree->CurrentToken->Type != totemTokenType_Whitespace && (tree->CurrentToken->Category == totemTokenCategory_ReservedWord || tree->CurrentToken->Type == totemTokenType_Identifier))
        {
            length += tree->CurrentToken->Value.Length;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        }
    }
    
    string->Length = length;
    string->Value = start;
    
    if(strict)
    {
        tree->CurrentToken--;
        TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Identifier);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
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