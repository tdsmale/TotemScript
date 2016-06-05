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
     
     variable = [ const-token ] variable-start-token identifier-token [ colon-token identifier-token ]
     string = double-quote-token { * } double-quote-token
     function-call = function-token identifier-token lbracket-token { expression } rbracket-token
     type = array-token | string-token | int-token | float-token | type-token
     function-pointer = at-token identifier [ lbracket { identifier } rbracket colon identifier ]
     
     new-array = lsbracket array-accessor rsbracket
     new-object = rbracket lbracket
     array-access = expression lcbracket expression rcbracket
     
     argument = variable | number-token | string | function-call | new-array | type | function-declaration-body | new-object
     
     expression = { pre-unary operator } ( argument | lbracket expression rbracket ) { post-unary operator } { binary-operator expression }
     
     while-loop = while-token expression lcbracket { statement } rcbracket-token
     for-loop = for-token lbracket statement statement statement rbracket rbracket lcbracket { statement } rcbracket-token
     do-while-loop = do-token lcbracket { statement } rcbracket while-token statement
     if-loop = if-token expression lcbracket { statement } rcbracket
     simple-statement = expression end-statement
     
     statement = while-loop | for-loop | do-while-loop | if-loop | return | simple-statement
     function-declaration-body = function-token lbracket-token { variable } rbracket-token lcbracket-token { statement } rcbracket-token
     function-declaration = identifier-token function-declaration-body
     
     block = function-declaration | statement
     script = { block } end-script-token
     */
    
    typedef enum
    {
        totemLoadScriptStatus_Success,
        totemLoadScriptStatus_FileNotFound,
        totemLoadScriptStatus_Recursion,
        totemLoadScriptStatus_OutOfMemory
    }
    totemLoadScriptStatus;
    
    const char *totemLoadScriptStatus_Describe(totemLoadScriptStatus status);
    
    typedef struct
    {
        totemString Description;
        totemLoadScriptStatus Status;
    }
    totemLoadScriptError;
    
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
        totemVariablePrototypeFlag_None = 0,
        totemVariablePrototypeFlag_IsConst = 1,
        totemVariablePrototypeFlag_IsLocal = 1 << 1
    }
    totemVariablePrototypeFlag;
    
    typedef enum
    {
        totemArgumentType_Variable = 1,
        totemArgumentType_String,
        totemArgumentType_Number,
        totemArgumentType_FunctionCall,
        totemArgumentType_NewArray,
        totemArgumentType_NewObject,
        totemArgumentType_Type,
        totemArgumentType_FunctionPointer,
        totemArgumentType_FunctionDeclaration
    }
    totemArgumentType;
    
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
        totemPostUnaryOperatorType_Inc,
        totemPostUnaryOperatorType_ArrayAccess,
        totemPostUnaryOperatorType_Invocation
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
        totemBinaryOperatorType_NotEquals,
        totemBinaryOperatorType_MoreThan,
        totemBinaryOperatorType_LessThan,
        totemBinaryOperatorType_MoreThanEquals,
        totemBinaryOperatorType_LessThanEquals,
        totemBinaryOperatorType_LogicalAnd,
        totemBinaryOperatorType_LogicalOr,
        totemBinaryOperatorType_IsType,
        totemBinaryOperatorType_AsType
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
        totemTokenType_Const,
        totemTokenType_Is,
        totemTokenType_Int,
        totemTokenType_Float,
        totemTokenType_Array,
        totemTokenType_String,
        totemTokenType_Type,
        totemTokenType_Object,
        totemTokenType_As,
        totemTokenType_At,
        totemTokenType_Coroutine,
        totemTokenType_Local,
        totemTokenType_Max
    }
    totemTokenType;
    const char *totemTokenType_Describe(totemTokenType type);
    
    typedef enum
    {
        totemTokenCategory_None = 0,
        totemTokenCategory_Symbol,
        totemTokenCategory_ReservedWord,
        totemTokenCategory_Other
    }
    totemTokenCategory;
    
    typedef struct
    {
        totemTokenType Type;
        totemTokenCategory Category;
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
        totemTokenCategory Category;
    }
    totemToken;
    
    typedef struct totemVariablePrototype
    {
        totemString Identifier;
        totemBufferPositionInfo Position;
        struct totemVariablePrototype *Next;
        totemVariablePrototypeFlag Flags;
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
    
    struct totemArrayMemberPrototype;
    struct totemNewArrayPrototype;
    
    typedef struct totemNewArrayPrototype
    {
        struct totemExpressionPrototype *Accessor;
    }
    totemNewArrayPrototype;
    
    struct totemFunctionDeclarationPrototype;
    
    typedef struct totemArgumentPrototype
    {
        union
        {
            totemVariablePrototype *Variable;
            totemString *String;
            totemString *Number;
            totemString *FunctionPointer;
            totemFunctionCallPrototype *FunctionCall;
            struct totemFunctionDeclarationPrototype *FunctionDeclaration;
            totemNewArrayPrototype *NewArray;
            totemPublicDataType DataType;
        };
        totemBufferPositionInfo Position;
        totemArgumentType Type;
    }
    totemArgumentPrototype;
    
    typedef struct totemPreUnaryOperatorPrototype
    {
        struct totemPreUnaryOperatorPrototype *Next;
        totemPreUnaryOperatorType Type;
    }
    totemPreUnaryOperatorPrototype;
    
    typedef struct totemPostUnaryOperatorPrototype
    {
        struct totemPostUnaryOperatorPrototype *Next;
        
        union
        {
            struct totemExpressionPrototype *ArrayAccess;
            struct totemExpressionPrototype *InvocationParametersStart;
        };
        
        totemPostUnaryOperatorType Type;
    }
    totemPostUnaryOperatorPrototype;
    
    typedef struct totemExpressionPrototype
    {
        union
        {
            struct totemExpressionPrototype *LValueExpression;
            totemArgumentPrototype *LValueArgument;
        };
        
        struct totemExpressionPrototype *RValue;
        struct totemExpressionPrototype *Next;
        
        totemPreUnaryOperatorPrototype *PreUnaryOperators;
        totemPostUnaryOperatorPrototype *PostUnaryOperators;
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
    
    typedef struct totemFunctionDeclarationPrototype
    {
        totemVariablePrototype *ParametersStart;
        struct totemStatementPrototype *StatementsStart;
        totemString *Identifier;
        totemBufferPositionInfo Position;
        struct totemFunctionDeclarationPrototype *Next;
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
            totemExpressionPrototype *Return;
            totemExpressionPrototype *Simple;
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
    
    typedef struct
    {
        totemMemoryBuffer Buffer;
    }
    totemScriptFile;
    
    /**
     * Load script contents
     */
    void totemScriptFile_Init(totemScriptFile *file);
    void totemScriptFile_Reset(totemScriptFile *file);
    void totemScriptFile_Cleanup(totemScriptFile *file);
    totemBool totemScriptFile_Load(totemScriptFile *dst, const char *srcPath, totemLoadScriptError *err);
    
    /**
     * Lex script into tokens
     */
    void totemTokenList_Init(totemTokenList *list);
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
    void totemParseTree_Init(totemParseTree *tree);
    void totemParseTree_Reset(totemParseTree *tree);
    void totemParseTree_Cleanup(totemParseTree *tree);
    
    totemParseStatus totemBlockPrototype_Parse(totemBlockPrototype *block, totemParseTree *tree);
    totemParseStatus totemStatementPrototype_Parse(totemStatementPrototype *statement, totemParseTree *tree);
    totemParseStatus totemStatementPrototype_ParseSet(totemStatementPrototype **firstStatement, totemStatementPrototype **lastStatement, totemParseTree *tree);
    totemParseStatus totemForLoopPrototype_Parse(totemForLoopPrototype *loop, totemParseTree *tree);
    totemParseStatus totemWhileLoopPrototype_Parse(totemWhileLoopPrototype *loop, totemParseTree *tree);
    totemParseStatus totemDoWhileLoopPrototype_Parse(totemDoWhileLoopPrototype *loop, totemParseTree *tree);
    totemParseStatus totemIfBlockPrototype_Parse(totemIfBlockPrototype *block, totemParseTree *tree);
    totemParseStatus totemFunctionDeclarationPrototype_Parse(totemFunctionDeclarationPrototype *func, totemParseTree *tree, totemBool isAnonymous);
    totemParseStatus totemExpressionPrototype_Parse(totemExpressionPrototype *expression, totemParseTree *tree);
    totemParseStatus totemExpressionPrototype_ParseParameterList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemParseTree *tree);
    totemParseStatus totemExpressionPrototype_ParseParameterInList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemParseTree *tree);
    totemParseStatus totemArgumentPrototype_Parse(totemArgumentPrototype *argument,  totemParseTree *tree);
    totemParseStatus totemNewArrayPrototype_Parse(totemNewArrayPrototype *arr, totemParseTree *tree);
    totemParseStatus totemVariablePrototype_Parse(totemVariablePrototype *variable, totemParseTree *tree);
    totemParseStatus totemFunctionCallPrototype_Parse(totemFunctionCallPrototype *call, totemParseTree *tree);
    totemParseStatus totemVariablePrototype_ParseParameterList(totemVariablePrototype **first, totemVariablePrototype **last, totemParseTree *tree);
    totemParseStatus totemVariablePrototype_ParseParameterInList(totemVariablePrototype **first, totemVariablePrototype **last, totemParseTree *tree);
    
    totemParseStatus totemString_ParseIdentifier(totemString *string, totemParseTree *tree, totemBool strict);
    totemParseStatus totemString_ParseNumber(totemString *string, totemParseTree *tree);
    
    totemParseStatus totemPreUnaryOperatorPrototype_Parse(totemPreUnaryOperatorPrototype **type, totemParseTree *tree);
    totemParseStatus totemPostUnaryOperatorPrototype_Parse(totemPostUnaryOperatorPrototype **type, totemParseTree *tree);
    totemParseStatus totemBinaryOperatorType_Parse(totemBinaryOperatorType *type, totemParseTree *tree);
    
    void totemToken_Print(FILE *target, totemToken *token);
    void totemToken_PrintList(FILE *target, totemToken *token, size_t num);
    
#ifdef __cplusplus
}
#endif

#endif