//
//  parse.h
//  ColossusEngine
//
//  Created by Timothy Smale on 27/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef ColossusEngine_parse_h
#define ColossusEngine_parse_h

#include <stddef.h>
#include <stdio.h>
#include <TotemScript/base.h>

#ifdef __cplusplus
extern "C" {
#endif
    
/*
 grammatically parses a given string buffer and creates a tree structure that can then be eval()'d
 
 variable = variable-start-token [ colon-token identifier-token ] identifier-token
 string = double-quote-token { * } double-quote-token
 function-call = function-token identifier-token lbracket-token { expression } rbracket-token
 argument = variable | number-token | string | function-call
 
 generic-expression = [ pre-unary operator ] ( argument | expression ) [ post-unary operator ] { binary-operator expression }
 assignment-expression = argument assignment-operator expression
 expression = generic-expression | assignment-expression
 
 while-loop = while-token expression lcbracket { statement } rcbracket-token
 for-loop = for-token lbracket statement statement statement rbracket rbracket lcbracket { statement } rcbracket-token
 do-while-loop = do-token lcbracket { statement } rcbracket while-token statement
 if-loop = if-token expression lcbracket { statement } rcbracket
 simple-statement = expression end-statement
 
 statement = while-loop | for-loop | do-while-loop | if-loop | switch | return | simple-statement
 function-declaration = identifier-token lbracket-token { variable } rbracket-token lcbracket-token { statement } rcbracket-token
  
 script = { function-declaration | statement } end-script-token
*/
    
    typedef enum
    {
        totemLexStatus_Success,
        totemLexStatus_Failure,
        totemLexStatus_OutOfMemory
    }
    totemLexStatus;
    
    const char *totemLexStatus_Describe(totemLexStatus status);
    
    typedef enum
    {
        totemParseStatus_Success,
        totemParseStatus_UnexpectedToken,
        totemParseStatus_ValueTooLarge,
        totemParseStatus_OutOfMemory
    }
    totemParseStatus;
    
    const char *totemParseStatus_Describe(totemParseStatus status);
        
    typedef enum
    {
        totemArgumentType_Variable = 1,
        totemArgumentType_Null,
        totemArgumentType_String,
        totemArgumentType_Number,
        totemArgumentType_FunctionCall,
    }
    totemArgumentType;

    typedef enum
    {
        totemExpressionType_Generic = 1,
        totemExpressionType_Assignment
    }
    totemExpressionType;

    typedef enum
    {
        totemPreUnaryOperatorType_None = 0,
        totemPreUnaryOperatorType_Dec,
        totemPreUnaryOperatorType_Inc,
        totemPreUnaryOperatorType_Negative,
        totemPreUnaryOperatorType_LogicalNegate
    }
    totemPreUnaryOperatorType;

    typedef enum
    {
        totemPostUnaryOperatorType_None = 0,
        totemPostUnaryOperatorType_Dec,
        totemPostUnaryOperatorType_Inc
    }
    totemPostUnaryOperatorType;

    typedef enum
    {
        totemBinaryOperatorType_None = 0,
        totemBinaryOperatorType_Plus,
        totemBinaryOperatorType_PlusAssign,
        totemBinaryOperatorType_Minus,
        totemBinaryOperatorType_MinusAssign,
        totemBinaryOperatorType_Multiply,
        totemBinaryOperatorType_MultiplyAssign,
        totemBinaryOperatorType_Divide,
        totemBinaryOperatorType_DivideAssign,
        totemBinaryOperatorType_Assign,
        totemBinaryOperatorType_Equals,
        totemBinaryOperatorType_MoreThan,
        totemBinaryOperatorType_LessThan,
        totemBinaryOperatorType_MoreThanEquals,
        totemBinaryOperatorType_LessThanEquals,
        totemBinaryOperatorType_LogicalAnd,
        totemBinaryOperatorType_LogicalOr
    }
    totemBinaryOperatorType;

    typedef enum
    {
        totemStatementType_WhileLoop = 1,
        totemStatementType_ForLoop,
        totemStatementType_IfBlock,
        totemStatementType_DoWhileLoop,
        totemStatementType_Return,
        totemStatementType_Simple
    }
    totemStatementType;

    typedef enum
    {
        totemBlockType_FunctionDeclaration = 1,
        totemBlockType_Statement
    }
    totemBlockType;

    typedef enum
    {
        totemLValueType_Expression = 1,
        totemLValueType_Argument
    }
    totemLValueType;

    typedef enum
    {
        totemIfElseBlockType_None = 0,
        totemIfElseBlockType_ElseIf,
        totemIfElseBlockType_Else
    }
    totemIfElseBlockType;

    typedef enum
    {
        totemTokenType_None = 0,
        totemTokenType_Null,
        totemTokenType_Variable,
        totemTokenType_Identifier,
        totemTokenType_Plus,
        totemTokenType_Minus,
        totemTokenType_Multiply,
        totemTokenType_Divide,
        totemTokenType_If,
        totemTokenType_Do,
        totemTokenType_While,
        totemTokenType_For,
        totemTokenType_Not,
        totemTokenType_And,
        totemTokenType_Or,
        totemTokenType_Assign,
        totemTokenType_LBracket,
        totemTokenType_RBracket,
        totemTokenType_LessThan,
        totemTokenType_MoreThan,
        totemTokenType_LCBracket,
        totemTokenType_RCBracket,
        totemTokenType_Dot,
        totemTokenType_Return,
        totemTokenType_PowerTo,
        totemTokenType_Semicolon,
        totemTokenType_Whitespace,
        totemTokenType_DoubleQuote,
        totemTokenType_SingleQuote,
        totemTokenType_LSBracket,
        totemTokenType_RSBracket,
        //totemTokenType_Switch,
        totemTokenType_Case,
        totemTokenType_Function,
        totemTokenType_EndScript,
        totemTokenType_Number,
        totemTokenType_Comma,
        totemTokenType_Break,
        totemTokenType_Colon,
        totemTokenType_Default,
        totemTokenType_Else,
        totemTokenType_True,
        totemTokenType_False,
        totemTokenType_Backslash,
        totemTokenType_Slash,
        totemTokenType_Max
    }
    totemTokenType;
    
    const char *totemTokenType_Describe(totemTokenType type);

    typedef struct
    {
        totemTokenType Type;
        const char *Value;
    }
    totemTokenDesc;

    typedef struct
    {
        size_t LineNumber;
        size_t CharNumber;
    }
    totemBufferPositionInfo;

    typedef struct
    {
        totemString Value;
        totemBufferPositionInfo Position;
        totemTokenType Type;
    }
    totemToken;

    typedef struct totemVariablePrototype
    {
        totemString Identifier;
        totemBufferPositionInfo Position;
        struct totemVariablePrototype *Next;
    }
    totemVariablePrototype;
        
    struct totemExpressionPrototype;

    typedef struct totemFunctionCallPrototype
    {
        struct totemExpressionPrototype *ParametersStart;
        totemString Identifier;
        totemBufferPositionInfo Position;
    }
    totemFunctionCallPrototype;

    typedef struct
    {
        union
        {
            totemVariablePrototype *Variable;
            totemString String;
            totemString Number;
            totemFunctionCallPrototype *FunctionCall;
        };
        totemBufferPositionInfo Position;
        totemArgumentType Type;
    }
    totemArgumentPrototype;

    typedef struct totemExpressionPrototype
    {
        union
        {
            struct totemExpressionPrototype *LValueExpression;
            totemArgumentPrototype *LValueArgument;
        };
        
        struct totemExpressionPrototype *RValue;
        struct totemExpressionPrototype *Next;
        
        totemPreUnaryOperatorType PreUnaryOperator;
        totemPostUnaryOperatorType PostUnaryOperator;
        totemBinaryOperatorType BinaryOperator;
        totemLValueType LValueType;
        
        totemBufferPositionInfo Position;
    }
    totemExpressionPrototype;
        
    struct totemStatementPrototype;

    typedef struct
    {
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
        totemBufferPositionInfo Position;
    }
    totemWhileLoopPrototype;

    typedef struct
    {
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Initialisation;
        totemExpressionPrototype *Condition;
        totemExpressionPrototype *AfterThought;
        totemBufferPositionInfo Position;
    }
    totemForLoopPrototype;

    typedef struct
    {
        struct totemStatementPrototype *StatementsStart;
        totemBufferPositionInfo Position;
    }
    totemElseBlockPrototype;

    typedef struct totemIfBlockPrototype
    {
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
        union
        {
            struct totemIfBlockPrototype *IfElseBlock;
            totemElseBlockPrototype *ElseBlock;
        };
        totemBufferPositionInfo Position;
        totemIfElseBlockType ElseType;
    }
    totemIfBlockPrototype;

    typedef struct
    {
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
        totemBufferPositionInfo Position;
    }
    totemDoWhileLoopPrototype;

    typedef struct
    {
        totemVariablePrototype *ParametersStart;
        struct totemStatementPrototype *StatementsStart;
        totemString *Identifier;
        totemBufferPositionInfo Position;
    }
    totemFunctionDeclarationPrototype;

    typedef struct totemStatementPrototype
    {
        union
        {
            totemWhileLoopPrototype *WhileLoop;
            totemForLoopPrototype *ForLoop;
            totemIfBlockPrototype *IfBlock;
            totemDoWhileLoopPrototype *DoWhileLoop;
            //totemSwitchBlockPrototype *SwitchBlock;
            totemExpressionPrototype *Return;
            totemExpressionPrototype *Expression;
        };
        struct totemStatementPrototype *Next;
        totemBufferPositionInfo Position;
        totemStatementType Type;
    }
    totemStatementPrototype;

    typedef struct totemBlockPrototype
    {
        union
        {
            totemFunctionDeclarationPrototype *FuncDec;
            totemStatementPrototype *Statement;
        };
        struct totemBlockPrototype *Next;
        totemBufferPositionInfo Position;
        totemBlockType Type;
    }
    totemBlockPrototype;

    typedef struct
    {
        totemMemoryBuffer Tokens;
        size_t CurrentLine;
        size_t NextTokenStart;
        size_t CurrentChar;
    }
    totemTokenList;
    
    typedef struct
    {
        totemToken *CurrentToken;
        totemMemoryBlock *LastMemBlock;
        totemBlockPrototype *FirstBlock;
    }
    totemParseTree;
    
    // TODO: Parse file pointer
    
    /**
     * Lex script into tokens
     */
    void totemTokenList_Reset(totemTokenList *list);
    void totemTokenList_Cleanup(totemTokenList *list);
    
    totemToken *totemTokenList_Alloc(totemTokenList *list);
    totemLexStatus totemTokenList_Lex(totemTokenList *list, const char *buffer, size_t length);
    totemLexStatus totemTokenList_LexSymbolToken(totemTokenList *token, const char *buffer);
    totemLexStatus totemTokenList_LexReservedWordToken(totemTokenList *list, const char *buffer, size_t length);
    totemLexStatus totemTokenList_LexNumberOrIdentifierToken(totemTokenList *list, const char *toCheck, size_t length);

#define TOTEM_LEX_ALLOC(dest, list) dest = totemTokenList_Alloc(list); if(!dest) return totemLexStatus_OutOfMemory;
    
    /**
     * Grammatically parse tokens
     */
    totemParseStatus totemParseTree_Parse(totemParseTree *tree, totemTokenList *token);
    void *totemParseTree_Alloc(totemParseTree *tree, size_t size);
    void totemParseTree_Reset(totemParseTree *tree);
    void totemParseTree_Cleanup(totemParseTree *tree);
    
    totemParseStatus totemBlockPrototype_Parse(totemBlockPrototype *block, totemToken **token, totemParseTree *tree);
    totemParseStatus totemStatementPrototype_Parse(totemStatementPrototype *statement, totemToken **token, totemParseTree *tree);
    totemParseStatus totemStatementPrototype_ParseSet(totemStatementPrototype **firstStatement, totemStatementPrototype **lastStatement, totemToken **token, totemParseTree *tree);
    totemParseStatus totemForLoopPrototype_Parse(totemForLoopPrototype *loop, totemToken **token, totemParseTree *tree);
    totemParseStatus totemWhileLoopPrototype_Parse(totemWhileLoopPrototype *loop, totemToken **token, totemParseTree *tree);
    totemParseStatus totemDoWhileLoopPrototype_Parse(totemDoWhileLoopPrototype *loop, totemToken **token, totemParseTree *tree);
    totemParseStatus totemIfBlockPrototype_Parse(totemIfBlockPrototype *block, totemToken **token, totemParseTree *tree);
    totemParseStatus totemFunctionDeclarationPrototype_Parse(totemFunctionDeclarationPrototype *func, totemToken **token, totemParseTree *tree);
    totemParseStatus totemExpressionPrototype_Parse(totemExpressionPrototype *expression, totemToken **token, totemParseTree *tree);
    totemParseStatus totemExpressionPrototype_ParseParameterList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemToken **token, totemParseTree *tree);
    totemParseStatus totemExpressionPrototype_ParseParameterInList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemToken **token, totemParseTree *tree);
    totemParseStatus totemArgumentPrototype_Parse(totemArgumentPrototype *argument, totemToken **token, totemParseTree *tree);
    totemParseStatus totemVariablePrototype_Parse(totemVariablePrototype *variable, totemToken **token, totemParseTree *tree);
    totemParseStatus totemFunctionCallPrototype_Parse(totemFunctionCallPrototype *call, totemToken **token, totemParseTree *tree);
    totemParseStatus totemVariablePrototype_ParseParameterList(totemVariablePrototype **first, totemVariablePrototype **last, totemToken **token, totemParseTree *tree);
    totemParseStatus totemVariablePrototype_ParseParameterInList(totemVariablePrototype **first, totemVariablePrototype **last, totemToken **token, totemParseTree *tree);
    
    totemParseStatus totemString_ParseIdentifier(totemString *string, totemToken **token, totemParseTree *tree);
    totemParseStatus totemPreUnaryOperatorType_Parse(totemPreUnaryOperatorType *type, totemToken **token, totemParseTree *tree);
    totemParseStatus totemPostUnaryOperatorType_Parse(totemPostUnaryOperatorType *type, totemToken **token, totemParseTree *tree);
    totemParseStatus totemBinaryOperatorType_Parse(totemBinaryOperatorType *type, totemToken **token, totemParseTree *tree);
    
    void totemToken_PrintList(FILE *target, totemToken *token, size_t num);
        
#ifdef __cplusplus
}
#endif

#endif