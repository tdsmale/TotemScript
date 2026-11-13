//
//  parse.c
//  TotemScript
//
//  Created by Timothy Smale on 19/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <TotemScript/base.h>
#include <TotemScript/log.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static size_t totemParseTree_ParseExpressionSet(totemParseTree*, totemExpressionPrototype**, totemExpressionPrototype**, totemTokenType, totemTokenType);
static void totemParseTree_ParseExpression(totemParseTree *, totemExpressionPrototype *);
static void totemParseTree_ParseArgument(totemParseTree *, totemArgumentPrototype *);
static void totemParseTree_ParseStatementSet(totemParseTree *, totemStatementPrototype **, totemStatementPrototype **);
static void totemParseTree_ParseBlockSet(totemParseTree* tree, totemBlockPrototype** store, totemTokenType endToken);


typedef struct
{
	uint8_t Left, Right;
}
totemBinaryPrecedenceDesc;

#define TOTEM_PARSE_PRIORITY_LEFT(level, type) [type] = { level, level }
#define TOTEM_PARSE_PRIORITY_RIGHT(level, type) [type] = { level, level - 1 }

// binary operator precedence table
// higher level = higher priority
static const totemBinaryPrecedenceDesc s_binaryPriority[] =
{
    TOTEM_PARSE_PRIORITY_LEFT(0, totemBinaryOperatorType_None),
    
    TOTEM_PARSE_PRIORITY_RIGHT(2, totemBinaryOperatorType_Assign),
    TOTEM_PARSE_PRIORITY_RIGHT(2, totemBinaryOperatorType_PlusAssign),
    TOTEM_PARSE_PRIORITY_RIGHT(2, totemBinaryOperatorType_MinusAssign),
    TOTEM_PARSE_PRIORITY_RIGHT(2, totemBinaryOperatorType_MultiplyAssign),
    TOTEM_PARSE_PRIORITY_RIGHT(2, totemBinaryOperatorType_DivideAssign),
    
    TOTEM_PARSE_PRIORITY_LEFT(3, totemBinaryOperatorType_LogicalOr),
    
    TOTEM_PARSE_PRIORITY_LEFT(4, totemBinaryOperatorType_LogicalAnd),
    
    TOTEM_PARSE_PRIORITY_LEFT(5, totemBinaryOperatorType_BinaryOr),
    
    TOTEM_PARSE_PRIORITY_LEFT(6, totemBinaryOperatorType_BinaryXor),
    
    TOTEM_PARSE_PRIORITY_LEFT(7, totemBinaryOperatorType_BinaryAnd),
    
    TOTEM_PARSE_PRIORITY_LEFT(8, totemBinaryOperatorType_Equals),
    TOTEM_PARSE_PRIORITY_LEFT(8, totemBinaryOperatorType_NotEquals),
    TOTEM_PARSE_PRIORITY_LEFT(8, totemBinaryOperatorType_IsType),
    
    TOTEM_PARSE_PRIORITY_LEFT(9, totemBinaryOperatorType_MoreThan),
    TOTEM_PARSE_PRIORITY_LEFT(9, totemBinaryOperatorType_LessThan),
    TOTEM_PARSE_PRIORITY_LEFT(9, totemBinaryOperatorType_MoreThanEquals),
    TOTEM_PARSE_PRIORITY_LEFT(9, totemBinaryOperatorType_LessThanEquals),
    
    TOTEM_PARSE_PRIORITY_LEFT(10, totemBinaryOperatorType_BinaryLeftShift),
    TOTEM_PARSE_PRIORITY_LEFT(11, totemBinaryOperatorType_BinaryRightShift),
    
    TOTEM_PARSE_PRIORITY_LEFT(12, totemBinaryOperatorType_Plus),
    TOTEM_PARSE_PRIORITY_LEFT(12, totemBinaryOperatorType_Minus),
    
    TOTEM_PARSE_PRIORITY_LEFT(13, totemBinaryOperatorType_Multiply),
    TOTEM_PARSE_PRIORITY_LEFT(13, totemBinaryOperatorType_Divide),
    
    TOTEM_PARSE_PRIORITY_LEFT(14, totemBinaryOperatorType_AsType),
};

totemToken *totemParseTree_Current(totemParseTree *tree)
{
	return &tree->CurrentToken;
}

void totemParseTree_Break(totemParseTree *tree, totemParseStatus status, totemTokenPositionInfo *location)
{
    if(location)
    {
        tree->ErrorAt = *location;
    }
    
	tree->JmpStatus = status;
	TOTEM_JMP_THROW(&tree->Jmp);
}

static void totemParseTree_Rewind(totemParseTree *tree)
{
    tree->CurrentToken = tree->LastToken;
}

static void totemParseTree_EnforceToken(totemParseTree *tree, totemTokenType type)
{
	if (totemParseTree_Current(tree)->Type != type)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
	}
}

static void totemParseTree_EnforceNotToken(totemParseTree *tree, totemTokenType type)
{
	if (totemParseTree_Current(tree)->Type == type)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
	}
}

void totemParseTree_AllocMemBlock(totemParseTree* tree)
{
    totemParseMemBlock *chosenBlock = NULL;
    
    if(tree->MemBlockFreeList)
    {
        chosenBlock = tree->MemBlockFreeList;
        tree->MemBlockFreeList = chosenBlock->Next;
    }
    else
    {
        chosenBlock = (totemParseMemBlock*)TOTEM_MALLOC(sizeof(totemParseMemBlock));
        if (chosenBlock == NULL)
        {
            totemParseTree_Break(tree, totemParseStatus_OutOfMemory, NULL);
        }
    }

	TOTEM_LOG_DO(Parse, Memory, Debug, logger, totemLog_Write(logger, "Allocated mem block: %p", chosenBlock));
    
    chosenBlock->Remaining = sizeof(chosenBlock->Data);
    chosenBlock->Next = tree->LastMemBlock;
    tree->LastMemBlock = chosenBlock;
}

static void totemParseTree_SkipToken(totemParseTree *tree)
{
	if (tree->StringParseHead)
	{
		totemParseTree_StringParseStep(tree);
	}

	totemParseTree_LexToken(tree);
}

static void *totemParseTree_Alloc(totemParseTree *tree, size_t objectSize)
{
	if (tree->StringParseHead)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &tree->CurrentToken.Position);
	}

    // align to ptr
	size_t allocSize = objectSize;
	size_t extra = allocSize % sizeof(uintptr_t);
	if (extra != 0)
	{
		allocSize += sizeof(uintptr_t) - extra;
	}
    TOTEM_ASSERT(allocSize >= objectSize);
    
	if (!tree->LastMemBlock || tree->LastMemBlock->Remaining < allocSize)
	{
		totemParseTree_AllocMemBlock(tree);
        TOTEM_ASSERT(tree->LastMemBlock->Remaining <= sizeof(tree->LastMemBlock->Data));
	}

    size_t alreadyAllocated = (sizeof(tree->LastMemBlock->Data) - tree->LastMemBlock->Remaining);
	char* ptr = tree->LastMemBlock->Data + alreadyAllocated;
	TOTEM_ZERO_N(ptr, objectSize);
    
	tree->LastMemBlock->Remaining -= allocSize;
	return (void*)ptr;
}

static void totemParseTree_SkipTokenNotEndScript(totemParseTree* tree)
{
	totemParseTree_SkipToken(tree);
	totemParseTree_EnforceNotToken(tree, totemTokenType_EndScript);
}

static totemBool totemParseTree_SkipWhitespace(totemParseTree* tree)
{
	totemBool foundNewLine = totemBool_False;

	while (totemParseTree_Current(tree)->Type == totemTokenType_Whitespace)
	{
		foundNewLine |= tree->NumLineBreaksInCurrentToken != 0;
		totemParseTree_SkipToken(tree);
	}

	return foundNewLine;
}

static void totemParseTree_SkipIdentifier(totemParseTree* tree, totemBool strict)
{
	totemTokenPositionInfo startingPos = totemParseTree_Current(tree)->Position;
	totemTokenCategory categories = 0;
	size_t numTokens = 0;

	// cannot start with a number
	totemParseTree_EnforceNotToken(tree, totemTokenType_Digits);

	while (1)
	{
		totemToken* currentToken = totemParseTree_Current(tree);
		totemTokenCategory currentTokenCategory = totemTokenType_ToCategory(currentToken->Type);
		if (currentToken->Type == totemTokenType_Whitespace
			|| currentTokenCategory == totemTokenCategory_Symbol
			|| currentToken->Type == totemTokenType_EndScript)
		{
			break;
		}
		categories |= 1 << currentTokenCategory;
		numTokens++;
		totemParseTree_SkipTokenNotEndScript(tree);
	}

	// empty identifier
	if (numTokens == 0)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &startingPos);
	}

	if (strict)
	{
		// cannot just be a single reserved word
		if (categories == (1 << totemTokenCategory_ReservedWord) && numTokens == 1)
		{
			totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &startingPos);
		}
	}
}

static void totemParseTree_ParseIdentifier(totemParseTree *tree, totemParseString *string, totemBool strict)
{
	totemParseTree_SkipWhitespace(tree);
	totemParseTree_StringParseBegin(tree);
	totemParseTree_SkipIdentifier(tree, strict);
	totemParseTree_StringParseEnd(tree, string);
}

static void totemParseTree_ParseNumber(totemParseTree *tree, totemNumberPrototype *numberPrototype)
{
    totemParseString *number = &numberPrototype->Value;
    
	totemParseTree_StringParseBegin(tree);

	if (totemParseTree_Current(tree)->Type == totemTokenType_Minus || totemParseTree_Current(tree)->Type == totemTokenType_Plus)
	{
		totemParseTree_SkipTokenNotEndScript(tree);
	}

	totemBool dot = totemBool_False;
	totemTokenType lastTokenType = totemParseTree_Current(tree)->Type;

	while (totemParseTree_Current(tree)->Type == totemTokenType_Digits || totemParseTree_Current(tree)->Type == totemTokenType_Dot)
	{
		if (totemParseTree_Current(tree)->Type == totemTokenType_Dot)
		{
			if (dot)
			{
				// can't have more than one dot
				totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
			}

            numberPrototype->IsFloat = totemBool_True;
			dot = totemBool_True;
		}

		lastTokenType = totemParseTree_Current(tree)->Type;
		totemParseTree_SkipTokenNotEndScript(tree);
	}

	if (lastTokenType != totemTokenType_Digits)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
	}

	char firstTokenChar = totemParseTree_GetAbs(tree, tree->CurrentToken.Position.Start);

    // float exponent
	if (totemParseTree_Current(tree)->Type == totemTokenType_Identifier &&
		totemParseTree_Current(tree)->Position.Length == 1 &&
		(firstTokenChar == 'e' || firstTokenChar == 'E'))
	{
		totemParseTree_SkipTokenNotEndScript(tree);
        numberPrototype->IsFloat = totemBool_True;

		if (totemParseTree_Current(tree)->Type == totemTokenType_Minus || totemParseTree_Current(tree)->Type == totemTokenType_Plus)
		{
			totemParseTree_SkipTokenNotEndScript(tree);
		}

		totemParseTree_EnforceToken(tree, totemTokenType_Digits);
		totemParseTree_SkipTokenNotEndScript(tree);
	}

	totemParseTree_StringParseEnd(tree, number);
}

static void totemParseTree_ParseStringLiteral(totemParseTree *tree, totemParseString *string)
{
	totemParseTree_EnforceToken(tree, totemTokenType_DoubleQuote);
	totemParseTree_SkipTokenNotEndScript(tree);

	totemParseTree_StringParseBegin(tree);

	while (totemParseTree_Current(tree)->Type != totemTokenType_DoubleQuote)
	{
		totemTokenType prevType = totemParseTree_Current(tree)->Type;
		totemParseTree_SkipTokenNotEndScript(tree);

		if (prevType == totemTokenType_Backslash && totemParseTree_Current(tree)->Type == totemTokenType_DoubleQuote)
		{
			totemParseTree_SkipTokenNotEndScript(tree);
		}
	}

	totemParseTree_StringParseEnd(tree, string);
	totemParseTree_SkipTokenNotEndScript(tree);
}

static void totemParseTree_ParsePreUnaryOperator(totemParseTree *tree, totemPreUnaryOperatorPrototype **type)
{
	totemParseTree_SkipWhitespace(tree);

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Hash:
			*type = totemParseTree_Alloc(tree, sizeof(totemPreUnaryOperatorPrototype));
			totemParseTree_SkipTokenNotEndScript(tree);
			(*type)->Type = totemPreUnaryOperatorType_LengthOf;
			break;

		case totemTokenType_Plus:
			*type = totemParseTree_Alloc(tree, sizeof(totemPreUnaryOperatorPrototype));
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_EnforceToken(tree, totemTokenType_Plus);
			totemParseTree_SkipTokenNotEndScript(tree);
			(*type)->Type = totemPreUnaryOperatorType_Inc;
			break;

		case totemTokenType_Minus:
			*type = totemParseTree_Alloc(tree, sizeof(totemPreUnaryOperatorPrototype));
			totemParseTree_SkipTokenNotEndScript(tree);

			if (totemParseTree_Current(tree)->Type == totemTokenType_Minus)
			{
				totemParseTree_SkipTokenNotEndScript(tree);
				(*type)->Type = totemPreUnaryOperatorType_Dec;
				break;
			}

			(*type)->Type = totemPreUnaryOperatorType_Negate;
			break;

		case totemTokenType_Not:
			*type = totemParseTree_Alloc(tree, sizeof(totemPreUnaryOperatorPrototype));
			totemParseTree_SkipTokenNotEndScript(tree);
			(*type)->Type = totemPreUnaryOperatorType_LogicalNegate;
			break;

		case totemTokenType_Tilde:
			*type = totemParseTree_Alloc(tree, sizeof(totemPreUnaryOperatorPrototype));
			totemParseTree_SkipTokenNotEndScript(tree);
			(*type)->Type = totemPreUnaryOperatorType_BinaryNot;
			break;

	default:
		break;
	}
}

static void totemParseTree_ParsePostUnaryOperator(totemParseTree *tree, totemPostUnaryOperatorPrototype **type)
{
	totemParseTree_SkipWhitespace(tree);

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Plus:
			totemParseTree_SkipTokenNotEndScript(tree);

			if (totemParseTree_Current(tree)->Type == totemTokenType_Plus)
			{
				*type = totemParseTree_Alloc(tree, sizeof(totemPostUnaryOperatorPrototype));
				totemParseTree_SkipTokenNotEndScript(tree);
				(*type)->Type = totemPostUnaryOperatorType_Inc;
				break;
			}
			else
			{
				totemParseTree_Rewind(tree);
			}

			break;

		case totemTokenType_Minus:
			totemParseTree_SkipTokenNotEndScript(tree);

			if (totemParseTree_Current(tree)->Type == totemTokenType_Minus)
			{
				*type = totemParseTree_Alloc(tree, sizeof(totemPostUnaryOperatorPrototype));
				
				totemParseTree_SkipTokenNotEndScript(tree);
				(*type)->Type = totemPostUnaryOperatorType_Dec;
				break;
			}
			else
			{
				totemParseTree_Rewind(tree);
			}

			break;

		case totemTokenType_LBracket:
			*type = totemParseTree_Alloc(tree, sizeof(totemPostUnaryOperatorPrototype));
			(*type)->Type = totemPostUnaryOperatorType_Invocation;

			totemParseTree_SkipTokenNotEndScript(tree);
			
			// parse parameters
			totemExpressionPrototype *first = NULL, *last = NULL;
			totemParseTree_ParseExpressionSet(tree, &first, &last, totemTokenType_RBracket, totemTokenType_Comma);
			(*type)->Parameters = first;

			break;

		case totemTokenType_Dot:
		{
			*type = totemParseTree_Alloc(tree, sizeof(totemPostUnaryOperatorPrototype));
			(*type)->Type = totemPostUnaryOperatorType_ArrayAccess;

			// parse identifier
			totemParseTree_SkipTokenNotEndScript(tree);
			(*type)->Parameters = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			(*type)->Parameters->LValueType = totemLValueType_Argument;
			(*type)->Parameters->Position = totemParseTree_Current(tree)->Position;

			(*type)->Parameters->LValue.Argument = totemParseTree_Alloc(tree, sizeof(totemArgumentPrototype));
			(*type)->Parameters->LValue.Argument->Position = totemParseTree_Current(tree)->Position;
			(*type)->Parameters->LValue.Argument->Type = totemArgumentType_String;

			totemParseTree_ParseIdentifier(tree, &(*type)->Parameters->LValue.Argument->Value.String, totemBool_True);
			totemParseTree_SkipWhitespace(tree);
			break;
		}

		case totemTokenType_LSBracket:
		{
			*type = totemParseTree_Alloc(tree, sizeof(totemPostUnaryOperatorPrototype));
			(*type)->Type = totemPostUnaryOperatorType_ArrayAccess;

			// parse index
			totemParseTree_SkipTokenNotEndScript(tree);
			(*type)->Parameters = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			totemParseTree_ParseExpression(tree, (*type)->Parameters);

			totemParseTree_SkipWhitespace(tree);
			totemParseTree_EnforceToken(tree, totemTokenType_RSBracket);
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);

			break;
		}

		default:
			break;
	}
}

static void totemParseTree_ParseBinaryOperator(totemParseTree *tree, totemBinaryOperatorType *type)
{
	totemParseTree_SkipWhitespace(tree);

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Assign:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
				case totemTokenType_Assign:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_Equals;
					break;

				default:
					*type = totemBinaryOperatorType_Assign;
					break;
			}
			break;

		case totemTokenType_MoreThan:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
			case totemTokenType_MoreThan:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_BinaryRightShift;
					break;

				case totemTokenType_Assign:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_MoreThanEquals;
					break;

				default:
					*type = totemBinaryOperatorType_MoreThan;
					break;
			}
			break;

		case totemTokenType_LessThan:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
				case totemTokenType_LessThan:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_BinaryLeftShift;
					break;

				case totemTokenType_Assign:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_LessThanEquals;
					break;

				default:
					*type = totemBinaryOperatorType_LessThan;
					break;
			}
			break;

		case totemTokenType_Slash:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
				case totemTokenType_Assign:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_DivideAssign;
					break;

				default:
					*type = totemBinaryOperatorType_Divide;
					break;
			}
			break;

		case totemTokenType_Minus:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
				case totemTokenType_Assign:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_MinusAssign;
					break;

				default:
					*type = totemBinaryOperatorType_Minus;
					break;
			}
			break;

		case totemTokenType_Star:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
				case totemTokenType_Assign:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_MultiplyAssign;
					break;

				default:
					*type = totemBinaryOperatorType_Multiply;
					break;
			}
			break;

		case totemTokenType_Plus:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
				case totemTokenType_Assign:
					totemParseTree_SkipTokenNotEndScript(tree);
					*type = totemBinaryOperatorType_PlusAssign;
					break;

				default:
					*type = totemBinaryOperatorType_Plus;
					break;
			}
			break;

		case totemTokenType_And:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
			case totemTokenType_And:
				totemParseTree_SkipTokenNotEndScript(tree);
				*type = totemBinaryOperatorType_LogicalAnd;
				break;

			default:
				*type = totemBinaryOperatorType_BinaryAnd;
				break;
			}
			break;

		case totemTokenType_Or:
			totemParseTree_SkipTokenNotEndScript(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
			case totemTokenType_And:
				totemParseTree_SkipTokenNotEndScript(tree);
				*type = totemBinaryOperatorType_LogicalOr;
				break;

			default:
				*type = totemBinaryOperatorType_BinaryOr;
				break;
			}
			break;

		case totemTokenType_Is:
			totemParseTree_SkipTokenNotEndScript(tree);
			*type = totemBinaryOperatorType_IsType;
			break;

		case totemTokenType_As:
			totemParseTree_SkipTokenNotEndScript(tree);
			*type = totemBinaryOperatorType_AsType;
			break;

		case totemTokenType_Not:
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_EnforceToken(tree, totemTokenType_Assign);
			totemParseTree_SkipTokenNotEndScript(tree);
			*type = totemBinaryOperatorType_NotEquals;
			break;

		case totemTokenType_Caret:
			totemParseTree_SkipTokenNotEndScript(tree);
			*type = totemBinaryOperatorType_BinaryXor;
			break;

		default:
			*type = totemBinaryOperatorType_None;
			break;
	}
}

static totemBinaryOperatorType totemParseTree_ParseSubExpression(totemParseTree *tree, totemExpressionPrototype *expression, uint8_t priority)
{
	totemParseTree_SkipWhitespace(tree);
	expression->Position = totemParseTree_Current(tree)->Position;
    size_t expressionStart = expression->Position.Start;

	// pre-unary operators
	for (totemPreUnaryOperatorPrototype **preUnaryType = &expression->PreUnaryOperators; preUnaryType; /* nada */)
	{
		totemParseTree_ParsePreUnaryOperator(tree, preUnaryType);
		if (!(*preUnaryType))
		{
			break;
		}
		else
		{
			preUnaryType = &(*preUnaryType)->Next;
		}
	}

	totemParseTree_SkipWhitespace(tree);

	if (totemParseTree_Current(tree)->Type == totemTokenType_LBracket)
	{
		totemParseTree_SkipTokenNotEndScript(tree);
		totemParseTree_SkipWhitespace(tree);
		expression->LValueType = totemLValueType_Expression;
		expression->LValue.Expression = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
		totemParseTree_ParseSubExpression(tree, expression->LValue.Expression, 0);

		totemParseTree_SkipWhitespace(tree);
		totemParseTree_EnforceToken(tree, totemTokenType_RBracket);
		totemParseTree_SkipTokenNotEndScript(tree);
	}
	else
	{
		expression->LValueType = totemLValueType_Argument;
		expression->LValue.Argument = totemParseTree_Alloc(tree, sizeof(totemArgumentPrototype));
		totemParseTree_ParseArgument(tree, expression->LValue.Argument);
	}

	// post-unary operators
	for (totemPostUnaryOperatorPrototype **postUnaryType = &expression->PostUnaryOperators; postUnaryType; /* nada */)
	{
		totemParseTree_ParsePostUnaryOperator(tree, postUnaryType);
		if (!(*postUnaryType))
		{
			break;
		}
		else
		{
			postUnaryType = &(*postUnaryType)->Next;
		}
	}

	totemExpressionPrototype *lValue = expression;

	totemParseTree_ParseBinaryOperator(tree, &lValue->BinaryOperator);
	TOTEM_ASSERT(lValue->BinaryOperator < TOTEM_ARRAY_SIZE(s_binaryPriority) && lValue->BinaryOperator >= 0);
	totemBinaryOperatorType lastOp = lValue->BinaryOperator;
	const totemBinaryPrecedenceDesc *binaryDesc = &s_binaryPriority[lValue->BinaryOperator];
	
	while (lValue->BinaryOperator != totemBinaryOperatorType_None && priority < binaryDesc->Left)
	{
		lValue->RValue = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
		lValue->RValue->LValue.Expression = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
		lValue->RValue->LValueType = totemLValueType_Expression;
		lValue->RValue->Position = totemParseTree_Current(tree)->Position;

		lValue->RValue->BinaryOperator = totemParseTree_ParseSubExpression(tree, lValue->RValue->LValue.Expression, binaryDesc->Right);
		lValue = lValue->RValue;

		TOTEM_ASSERT(lValue->BinaryOperator < TOTEM_ARRAY_SIZE(s_binaryPriority) && lValue->BinaryOperator >= 0);
		lastOp = lValue->BinaryOperator;
		binaryDesc = &s_binaryPriority[lValue->BinaryOperator];
	}

	// we may have prematurely parsed the next binary operator, return it if so
	totemBinaryOperatorType toReturn = lValue->BinaryOperator;
	lValue->BinaryOperator = totemBinaryOperatorType_None;
    
    // determine length of expression
    size_t expressionEnd = totemParseTree_Current(tree)->Position.Start + totemParseTree_Current(tree)->Position.Length;
    size_t expressionLen = expressionEnd - expressionStart;
    expression->Position.Length = expressionLen;
    
	return toReturn;
}

static void totemParseTree_ParseExpression(totemParseTree *tree, totemExpressionPrototype *expression)
{
	totemParseTree_ParseSubExpression(tree, expression, 0);
}

static size_t totemParseTree_ParseExpressionSet(totemParseTree *tree, totemExpressionPrototype **first, totemExpressionPrototype **last, totemTokenType end, totemTokenType split)
{
    size_t numExpressionsParsed = 0;
    
	while (totemParseTree_Current(tree)->Type != end)
	{
		totemExpressionPrototype *parameter = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
		totemParseTree_ParseExpression(tree, parameter);
		totemParseTree_SkipWhitespace(tree);

		if (totemParseTree_Current(tree)->Type != end && totemParseTree_Current(tree)->Type != split)
		{
			totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
		}

		if (*first == NULL)
		{
			*first = parameter;
		}
		else
		{
			(*last)->Next = parameter;
		}

		*last = parameter;
        
        numExpressionsParsed++;

		if (totemParseTree_Current(tree)->Type == split)
		{
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);
			totemParseTree_EnforceNotToken(tree, totemTokenType_RBracket);
		}
		else if (totemParseTree_Current(tree)->Type == end)
		{
			break;
		}
		else
		{
			totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
		}
	}

	totemParseTree_SkipToken(tree);
    
    return numExpressionsParsed;
}

static void totemParseTree_ParseNewObject(totemParseTree *tree, totemNewObjectPrototype *obj)
{
	totemParseTree_EnforceToken(tree, totemTokenType_LCBracket);
	totemParseTree_SkipTokenNotEndScript(tree);
	totemParseTree_SkipWhitespace(tree);

	totemExpressionPrototype *keys = NULL;
	totemExpressionPrototype *values = NULL;
	obj->Num = 0;

	while (totemParseTree_Current(tree)->Type != totemTokenType_RCBracket)
	{
		if (obj->Num + 1 == (uint32_t)TOTEM_INT_MAX)
		{
			totemParseTree_Break(tree, totemParseStatus_TooManyItems, &totemParseTree_Current(tree)->Position);
		}

		if (obj->Keys)
		{
			obj->Keys->Next = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			obj->Keys = obj->Keys->Next;
		}
		else
		{
			keys = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			obj->Keys = keys;
		}

		totemParseTree_ParseExpression(tree, obj->Keys);
		totemParseTree_SkipWhitespace(tree);
		totemParseTree_EnforceToken(tree, totemTokenType_Colon);
		totemParseTree_SkipTokenNotEndScript(tree);
		totemParseTree_SkipWhitespace(tree);

		if (obj->Values)
		{
			obj->Values->Next = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			obj->Values = obj->Values->Next;
		}
		else
		{
			values = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			obj->Values = values;
		}

		totemParseTree_ParseExpression(tree, obj->Values);
		totemParseTree_SkipWhitespace(tree);

		if (totemParseTree_Current(tree)->Type == totemTokenType_Comma)
		{
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);
		}

		obj->Num++;
	}

	obj->Keys = keys;
	obj->Values = values;

	totemParseTree_SkipTokenNotEndScript(tree);
}

static void totemParseTree_ParseNewArray(totemParseTree *tree, totemNewArrayPrototype *arr)
{
	totemParseTree_SkipWhitespace(tree);
	totemParseTree_EnforceToken(tree, totemTokenType_LSBracket);
	totemParseTree_SkipTokenNotEndScript(tree);
	totemParseTree_EnforceNotToken(tree, totemTokenType_RSBracket);

	totemExpressionPrototype* first = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, first);
	totemParseTree_SkipWhitespace(tree);

	switch (totemParseTree_Current(tree)->Type)
	{
	case totemTokenType_Colon:
	{
		// first expression is the size - the rest are its elements
        totemParseTree_SkipTokenNotEndScript(tree);
		totemExpressionPrototype* next = NULL, *last = NULL;
		arr->NumElements = totemParseTree_ParseExpressionSet(tree, &next, &last, totemTokenType_RSBracket, totemTokenType_Comma);
		arr->NumExpression = first;
		arr->Accessor = next;
		break;
	}
    
    case totemTokenType_Comma:
    {
        // first expression is the first element, parse the rest
        totemParseTree_SkipTokenNotEndScript(tree);
        totemExpressionPrototype* next = NULL, *last = NULL;
        arr->NumElements = 1 + totemParseTree_ParseExpressionSet(tree, &next, &last, totemTokenType_RSBracket, totemTokenType_Comma);
        first->Next = next;
        arr->NumExpression = NULL;
        arr->Accessor = first;
        break;
    }

	case totemTokenType_RSBracket:
	{
		// this is the only element
		arr->Accessor = first;
        arr->NumElements = 1;
		totemParseTree_SkipTokenNotEndScript(tree);
		totemParseTree_SkipWhitespace(tree);
		break;
	}

	default:
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
		break;
	}
}

static void totemParseTree_ParseVariable(totemParseTree *tree, totemVariablePrototype *variable)
{
	totemParseTree_SkipWhitespace(tree);
	variable->Position = totemParseTree_Current(tree)->Position;
	variable->Flags = totemVariablePrototypeFlag_None;

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Let:
			TOTEM_SETBITS(variable->Flags, totemVariablePrototypeFlag_IsConst);
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);
			break;

		case totemTokenType_Var:
			TOTEM_SETBITS(variable->Flags, totemVariablePrototypeFlag_IsDeclaration);
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);
			break;

		default:
			totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &variable->Position);
			break;
	}

	totemParseTree_ParseIdentifier(tree, &variable->Identifier, totemBool_True);
	totemParseTree_SkipWhitespace(tree);

	if (totemParseTree_Current(tree)->Type == totemTokenType_Colon)
	{
		totemParseTree_SkipTokenNotEndScript(tree);
		totemParseTree_SkipWhitespace(tree);

		totemParseTree_ParseIdentifier(tree, &variable->Identifier, totemBool_True);
		totemParseTree_SkipWhitespace(tree);
	}
}

static void totemParseTree_ParseVariableInSet(totemParseTree *tree, totemVariablePrototype **first, totemVariablePrototype **last)
{
	totemVariablePrototype *parameter = totemParseTree_Alloc(tree, sizeof(totemVariablePrototype));
	totemParseTree_ParseVariable(tree, parameter);
	totemParseTree_SkipWhitespace(tree);

	totemTokenType nextType = totemParseTree_Current(tree)->Type;

	if (nextType != totemTokenType_RBracket && nextType != totemTokenType_Comma)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
	}

	if (*first == NULL)
	{
		*first = parameter;
	}
	else
	{
		(*last)->Next = parameter;
	}

	*last = parameter;
}

static void totemParseTree_ParseVariableSet(totemParseTree *tree, totemVariablePrototype **first, totemVariablePrototype **last)
{
	totemParseTree_SkipWhitespace(tree);
	totemParseTree_EnforceToken(tree, totemTokenType_LBracket);
	totemParseTree_SkipTokenNotEndScript(tree);

	while (totemParseTree_Current(tree)->Type != totemTokenType_RBracket)
	{
		totemParseTree_ParseVariableInSet(tree, first, last);

		if (totemParseTree_Current(tree)->Type == totemTokenType_Comma)
		{
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);
			totemParseTree_EnforceNotToken(tree, totemTokenType_RBracket);
		}
	}

	totemParseTree_SkipToken(tree);
}

static void totemParseTree_ParseFunctionDeclaration(totemParseTree *tree, totemFunctionDeclarationPrototype *func)
{
	totemParseTree_SkipWhitespace(tree);
	// assuming that position has already been set
	// and "function" keyword has been skipped

	totemVariablePrototype *firstParameter = NULL, *lastParameter = NULL;
	totemParseTree_ParseVariableSet(tree, &firstParameter, &lastParameter);
	func->ParametersStart = firstParameter;

	totemParseTree_SkipWhitespace(tree);

	totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
	totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
	func->StatementsStart = firstStatement;
}

static void totemParseTree_ParseArgument(totemParseTree *tree, totemArgumentPrototype *argument)
{
	totemParseTree_SkipWhitespace(tree);
	argument->Position = totemParseTree_Current(tree)->Position;
	totemTokenType token = totemParseTree_Current(tree)->Type;
    
    totemBool isGlobal = totemBool_True;

	switch (token)
	{
		case totemTokenType_Coroutine:
		case totemTokenType_Function:
		{
			totemTokenPositionInfo position = totemParseTree_Current(tree)->Position;
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);

			switch (totemParseTree_Current(tree)->Type)
			{
			case totemTokenType_LBracket:
			case totemTokenType_Colon:
				// anonymous/coroutine function
				argument->Type = token == totemTokenType_Coroutine ? totemArgumentType_CoroutineDeclaration : totemArgumentType_FunctionDeclaration;
				argument->Value.Func = totemParseTree_Alloc(tree, sizeof(totemFunctionDeclarationPrototype));
				argument->Value.Func->Position = position;
				totemParseTree_ParseFunctionDeclaration(tree, argument->Value.Func);
				break;

			default:
				// function/coroutine type
				argument->Value.DataType = token == totemTokenType_Coroutine ? totemPublicDataType_Coroutine : totemPublicDataType_Function;
				argument->Type = totemArgumentType_Type;
				break;
			}

			break;
		}

		// Bool
		case totemTokenType_True:
		case totemTokenType_False:
			argument->Type = totemArgumentType_Bool;
			argument->Value.Bool = totemParseTree_Current(tree)->Type == totemTokenType_True;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

			// null
		case totemTokenType_Null:
			argument->Type = totemArgumentType_Null;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

			// type objects
		case totemTokenType_Array:
			argument->Value.DataType = totemPublicDataType_Array;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Int:
			argument->Value.DataType = totemPublicDataType_Int;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Float:
			argument->Value.DataType = totemPublicDataType_Float;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_String:
			argument->Value.DataType = totemPublicDataType_String;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Type:
			argument->Value.DataType = totemPublicDataType_Type;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Object:
			argument->Value.DataType = totemPublicDataType_Object;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Userdata:
			argument->Value.DataType = totemPublicDataType_Userdata;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Bool:
			argument->Value.DataType = totemPublicDataType_Boolean;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

			// new object
		case totemTokenType_LCBracket:
            isGlobal = totemBool_False;
			argument->Type = totemArgumentType_NewObject;
			argument->Value.NewObject = totemParseTree_Alloc(tree, sizeof(totemNewObjectPrototype));
			totemParseTree_ParseNewObject(tree, argument->Value.NewObject);
			break;

			// new array
		case totemTokenType_LSBracket:
            isGlobal = totemBool_False;
			argument->Type = totemArgumentType_NewArray;
			argument->Value.NewArray = totemParseTree_Alloc(tree, sizeof(totemNewArrayPrototype));
			totemParseTree_ParseNewArray(tree, argument->Value.NewArray);
			break;

			// variable
		case totemTokenType_Let:
		case totemTokenType_Var:
            isGlobal = totemBool_False;
			argument->Type = totemArgumentType_Variable;
			argument->Value.Variable = totemParseTree_Alloc(tree, sizeof(totemVariablePrototype));
			totemParseTree_ParseVariable(tree, argument->Value.Variable);
			break;

			// number
		case totemTokenType_Digits:
			argument->Type = totemArgumentType_Number;
            argument->Value.Number = totemParseTree_Alloc(tree, sizeof(totemNumberPrototype));
			totemParseTree_ParseNumber(tree, argument->Value.Number);
			break;

			// string
		case totemTokenType_DoubleQuote:
			argument->Type = totemArgumentType_String;
			totemParseTree_ParseStringLiteral(tree, &argument->Value.String);
			break;

			// identifier
		case totemTokenType_Identifier:
            isGlobal = totemBool_False;
			argument->Type = totemArgumentType_Identifier;
			totemParseTree_ParseIdentifier(tree, &argument->Value.Identifier, totemBool_True);
			break;

		default:
			totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
			break;
	}

	totemParseTree_SkipWhitespace(tree);
    
    if(isGlobal)
    {
        totemParseGlobalValue *val;
        val = (totemParseGlobalValue*)totemParseTree_Alloc(tree, sizeof(totemParseGlobalValue));
        val->Next = tree->Globals;
        val->Arg = argument;
        tree->Globals = val;
    }
}

static void totemParseTree_ParseStatementEnd(totemParseTree* tree)
{
	if (!totemParseTree_SkipWhitespace(tree))
	{
		totemParseTree_EnforceToken(tree, totemTokenType_Semicolon);
		totemParseTree_SkipToken(tree);
	}

	totemParseTree_SkipWhitespace(tree);
}

static void totemParseTree_ParseWhileLoop(totemParseTree *tree, totemWhileLoopPrototype *loop)
{
    totemParseTree_SkipWhitespace(tree);
	loop->Position = totemParseTree_Current(tree)->Position;
    totemParseTree_EnforceToken(tree, totemTokenType_While);
    totemParseTree_SkipTokenNotEndScript(tree);
    
    loop->Expression = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, loop->Expression);
    totemParseTree_SkipWhitespace(tree);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
	totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
    loop->StatementsStart = firstStatement;
}

static void totemParseTree_ParseDoWhileLoop(totemParseTree *tree, totemDoWhileLoopPrototype *loop)
{
    totemParseTree_SkipWhitespace(tree);
	loop->Position = totemParseTree_Current(tree)->Position;
    totemParseTree_EnforceToken(tree, totemTokenType_Do);
    totemParseTree_SkipTokenNotEndScript(tree);
    totemParseTree_SkipWhitespace(tree);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
	totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
    loop->StatementsStart = firstStatement;
    
    totemParseTree_SkipWhitespace(tree);
    totemParseTree_EnforceToken(tree, totemTokenType_While);
    totemParseTree_SkipTokenNotEndScript(tree);
    
    loop->Expression = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, loop->Expression);
    
	totemParseTree_ParseStatementEnd(tree);
}

static void totemParseTree_ParseIfBlock(totemParseTree *tree, totemIfBlockPrototype *block)
{
    totemParseTree_SkipWhitespace(tree);
	block->Position = totemParseTree_Current(tree)->Position;
    block->ElseType = totemIfElseBlockType_None;
    
    totemParseTree_EnforceToken(tree, totemTokenType_If);
    totemParseTree_SkipTokenNotEndScript(tree);
    block->Expression = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, block->Expression);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
	totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
    block->StatementsStart = firstStatement;
    totemParseTree_SkipWhitespace(tree);
    
	if (totemParseTree_Current(tree)->Type == totemTokenType_Else)
    {
        totemParseTree_SkipTokenNotEndScript(tree);
        totemParseTree_SkipWhitespace(tree);
        
		switch (totemParseTree_Current(tree)->Type)
        {
            case totemTokenType_If:
            {
                block->ElseType = totemIfElseBlockType_ElseIf;
                block->Next.IfElseBlock = totemParseTree_Alloc(tree, sizeof(totemIfBlockPrototype));
				totemParseTree_ParseIfBlock(tree, block->Next.IfElseBlock);
                break;
            }
                
            case totemTokenType_LCBracket:
            {
                block->ElseType = totemIfElseBlockType_Else;
				block->Next.ElseBlock = totemParseTree_Alloc(tree, sizeof(totemElseBlockPrototype));
				block->Next.ElseBlock->Position = totemParseTree_Current(tree)->Position;
                
                totemStatementPrototype *firstElseStatement = NULL, *lastElseStatement = NULL;
				totemParseTree_ParseStatementSet(tree, &firstElseStatement, &lastElseStatement);
				block->Next.ElseBlock->StatementsStart = firstElseStatement;
                break;
            }
                
            default:
				totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
				break;
        }
    }
}

static void totemParseTree_ParseForLoop(totemParseTree *tree, totemForLoopPrototype *loop)
{
    totemParseTree_SkipWhitespace(tree);
	loop->Position = totemParseTree_Current(tree)->Position;
    totemParseTree_EnforceToken(tree, totemTokenType_For);
    totemParseTree_SkipTokenNotEndScript(tree);
    totemParseTree_SkipWhitespace(tree);
	totemParseTree_EnforceToken(tree, totemTokenType_LBracket);
	totemParseTree_SkipTokenNotEndScript(tree);

    loop->Initialisation = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, loop->Initialisation);
    
    totemParseTree_SkipWhitespace(tree);
    totemParseTree_EnforceToken(tree, totemTokenType_Semicolon);
    totemParseTree_SkipTokenNotEndScript(tree);
    
    loop->Condition = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, loop->Condition);
    
    totemParseTree_SkipWhitespace(tree);
    totemParseTree_EnforceToken(tree, totemTokenType_Semicolon);
    totemParseTree_SkipTokenNotEndScript(tree);
    
    loop->AfterThought = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, loop->AfterThought);
    
    totemParseTree_SkipWhitespace(tree);
	totemParseTree_EnforceToken(tree, totemTokenType_RBracket);
	totemParseTree_SkipTokenNotEndScript(tree);

    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
	totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
    loop->StatementsStart = firstStatement;
}

static void totemParseTree_ParseSimpleStatement(totemParseTree *tree, totemExpressionPrototype *exp)
{
	totemParseTree_ParseExpression(tree, exp);
	totemParseTree_ParseStatementEnd(tree);
}

static void totemParseTree_ParseStatement(totemParseTree *tree, totemStatementPrototype *statement)
{
	totemParseTree_SkipWhitespace(tree);
	statement->Position = totemParseTree_Current(tree)->Position;
	
	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_While:
		{
			statement->Type = totemStatementType_WhileLoop;
			statement->Ptr.WhileLoop = totemParseTree_Alloc(tree, sizeof(totemWhileLoopPrototype));
			totemParseTree_ParseWhileLoop(tree, statement->Ptr.WhileLoop);
			break;
		}

		case totemTokenType_Loop:
		{
			statement->Type = totemStatementType_Loop;
			totemStatementPrototype* firstStatement = NULL, * lastStatement = NULL;
			totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
			statement->Ptr.Set = firstStatement;
			break;
		}

		case totemTokenType_Do:
		{
			statement->Type = totemStatementType_DoWhileLoop;
			statement->Ptr.DoWhileLoop = totemParseTree_Alloc(tree, sizeof(totemDoWhileLoopPrototype));
			totemParseTree_ParseDoWhileLoop(tree, statement->Ptr.DoWhileLoop);
			break;
		}

		case totemTokenType_For:
		{
			statement->Type = totemStatementType_ForLoop;
			statement->Ptr.ForLoop = totemParseTree_Alloc(tree, sizeof(totemForLoopPrototype));
			totemParseTree_ParseForLoop(tree, statement->Ptr.ForLoop);
			break;
		}

		case totemTokenType_If:
		{
			statement->Type = totemStatementType_IfBlock;
			statement->Ptr.IfBlock = totemParseTree_Alloc(tree, sizeof(totemIfBlockPrototype));
			totemParseTree_ParseIfBlock(tree, statement->Ptr.IfBlock);
			break;
		}

		case totemTokenType_Return:
		{
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);

			statement->Type = totemStatementType_Return;
			statement->Ptr.Return = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			totemParseTree_ParseSimpleStatement(tree, statement->Ptr.Return);
			break;
		}

		case totemTokenType_Yield:
		{
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);

			statement->Type = totemStatementType_Yield;
			statement->Ptr.Return = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			totemParseTree_ParseSimpleStatement(tree, statement->Ptr.Return);
			break;
		}

		case totemTokenType_LCBracket:
		{
			statement->Type = totemStatementType_Scope;
			
			totemStatementPrototype* firstStatement = NULL, * lastStatement = NULL;
			totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
			statement->Ptr.Set = firstStatement;
			break;
		}

		default:
		{
			statement->Type = totemStatementType_Simple;
			statement->Ptr.Simple = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
			totemParseTree_ParseSimpleStatement(tree, statement->Ptr.Simple);
			break;
		}
	}

	totemParseTree_SkipWhitespace(tree);
}

static void totemParseTree_ParseStatementInSet(totemParseTree *tree, totemStatementPrototype **first, totemStatementPrototype **last)
{
	totemStatementPrototype *statement = totemParseTree_Alloc(tree, sizeof(totemStatementPrototype));
	totemParseTree_ParseStatement(tree, statement);

	if (*first == NULL)
	{
		*first = statement;
	}
	else
	{
		(*last)->Next = statement;
	}

	*last = statement;
}

static void totemParseTree_ParseStatementSet(totemParseTree *tree, totemStatementPrototype **first, totemStatementPrototype **last)
{
	totemParseTree_SkipWhitespace(tree);
	totemParseTree_EnforceToken(tree, totemTokenType_LCBracket);
	totemParseTree_SkipTokenNotEndScript(tree);
    totemParseTree_SkipWhitespace(tree);

	while (totemParseTree_Current(tree)->Type != totemTokenType_RCBracket)
	{
		totemParseTree_ParseStatementInSet(tree, first, last);
	}

	totemParseTree_SkipToken(tree);
    totemParseTree_SkipWhitespace(tree);
}

static void totemParseTree_ParseBlock(totemParseTree *tree, totemBlockPrototype *block)
{
	totemParseTree_SkipWhitespace(tree);
	block->Position = totemParseTree_Current(tree)->Position;

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Function:
		{
			block->Type = totemBlockType_Function;
			block->Ptr.Function = totemParseTree_Alloc(tree, sizeof(totemFunctionBlockPrototype));
			block->Ptr.Function->Dec.Position = block->Position;

			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);
			totemParseTree_ParseIdentifier(tree, &block->Ptr.Function->Identifier, totemBool_True);
			totemParseTree_SkipWhitespace(tree);
			totemParseTree_ParseFunctionDeclaration(tree, &block->Ptr.Function->Dec);
			break;
		}

		default:
		{
			block->Type = totemBlockType_Statement;
			block->Ptr.Statement = totemParseTree_Alloc(tree, sizeof(totemStatementPrototype));
			totemParseTree_ParseStatement(tree, block->Ptr.Statement);
			break;
		}
	}
}

static totemBlockPrototype *totemParseTree_AllocAndParseBlock(totemParseTree* tree)
{
	totemBlockPrototype* block = totemParseTree_Alloc(tree, sizeof(totemBlockPrototype));
	totemParseTree_ParseBlock(tree, block);
	return block;
}

static void totemParseTree_ParseBlockSet(totemParseTree* tree, totemBlockPrototype** store, totemTokenType endToken)
{
	totemBlockPrototype* lastBlock = NULL;
	totemBlockPrototype* block = totemParseTree_AllocAndParseBlock(tree);
	*store = block;
	lastBlock = block;

	while (totemParseTree_Current(tree)->Type != endToken)
	{
		block = totemParseTree_AllocAndParseBlock(tree);
		lastBlock->Next = block;
		lastBlock = block;
	}
}

void totemParseTree_Cleanup(totemParseTree* tree)
{
    // free memory blocks
    while (tree->LastMemBlock)
    {
        void* ptr = tree->LastMemBlock;
        tree->LastMemBlock = tree->LastMemBlock->Next;
        TOTEM_FREE(ptr, sizeof(totemParseMemBlock));
    }
    
    tree->MemBlockFreeList = NULL;
}

void totemParseTree_Reset(totemParseTree* tree)
{
    // put memory blocks onto free list
    if(tree->MemBlockFreeList == NULL)
    {
        tree->MemBlockFreeList = tree->LastMemBlock;
        tree->LastMemBlock = NULL;
    }
    else while(tree->LastMemBlock)
    {
        totemParseMemBlock* ptr = tree->LastMemBlock;
        tree->LastMemBlock = tree->LastMemBlock->Next;
        
        ptr->Next = tree->MemBlockFreeList;
        tree->MemBlockFreeList = ptr;
    }
    
	tree->FirstBlock = NULL;
	tree->StringParseHead = NULL;
	tree->StringParseTail = NULL;
    tree->Globals = NULL;
    tree->Callbacks->Reset(tree->SrcCxt);
    tree->SrcPos = 0;
    
    tree->CurrentPos = 0;
    tree->NumLineBreaksInCurrentToken = 0;
    tree->InsideStringLiteral = totemBool_False;
    tree->CurrentLineChar = 1;
    tree->CurrentLine = 1;
    TOTEM_ZERO(tree->CurrentToken);
    TOTEM_ZERO(tree->LastToken);
}

static void totemParseTree_Init(totemParseTree* tree)
{
    TOTEM_ZERO(*tree);
}

/**
* Create parse tree from token list
*/
totemParseStatus totemParseTree_Parse(totemParseTree *tree)
{
	totemParseStatus status = totemParseStatus_Success;
    totemParseTree_Reset(tree);
	
	TOTEM_JMP_TRY(&tree->Jmp)
	{
        // initial read
        totemParseTree_InitialRead(tree);
        
        // parse first token
        totemParseTree_LexToken(tree);

		totemParseTree_SkipWhitespace(tree);
		totemParseTree_ParseBlockSet(tree, &tree->FirstBlock, totemTokenType_EndScript);
	}
	TOTEM_JMP_CATCH(tree->Jmp)
	{
		status = tree->JmpStatus;
	}

	return status;
}

/**
 * file parsing
 */

static size_t ParseFileReader(void *cxtIn, char *buffer, size_t len, size_t pos)
{
	FILE* f = (FILE*)cxtIn;
	return fread(buffer, 1, len, f);
}

static void ParseFileReset(void *cxtIn)
{
    FILE* f = (FILE*)cxtIn;
    fseek(f, 0, SEEK_SET);
}

static void ParseFilePrinter(void *cxtIn, totemLog *fDst, size_t len, size_t pos)
{
    FILE* fSrc = (FILE*)cxtIn;
	long prevSeekPos = ftell(fSrc);
    if(prevSeekPos == -1)
    {
        TOTEM_LOG_V(Parse, File, Error, "error getting file position %i\n", errno);
    }
    else
    {
		long err = fseek(fSrc, (long)pos, SEEK_SET);
        if(err != 0)
        {
            TOTEM_LOG_V(Parse, File, Error, "error setting file position %i\n", errno);
        }
        else
        {
            char buffer[64];
            size_t left = len;

            while(left)
            {
                size_t toRead = left > TOTEM_ARRAY_SIZE(buffer) ? TOTEM_ARRAY_SIZE(buffer) : left;
                left -= toRead;
                
                // read from src
                size_t numRead = fread(buffer, 1, toRead, fSrc);
				
                if (numRead != toRead)
                {
                    TOTEM_LOG_V(Parse, File, Error, "error reading file %i, numRead(%i) != toRead(%i)\n", errno, numRead, toRead);
                    break;
                }
                
                // write to dst
                totemLog_Write(fDst, "%.*s", numRead, buffer);
            }

            //err = fsetpos(fSrc, &prevSeekPos);
			err = fseek(fSrc, prevSeekPos, SEEK_SET);
            if(err != 0)
            {
                TOTEM_LOG_V(Parse, File, Error, "error setting file position %i\n", errno);
            }
        }
    }
}

void totemParseTree_InitWithFile(totemParseTree *tree, FILE *file)
{
    totemParseTree_Init(tree);
    
    // todo - start from existing position, not just zero
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    tree->SrcCxt = file;
    tree->SrcLen = length;
    tree->SrcPos = 0;
    
    const static totemParseTreeClass Callbacks =
    {
        .Step = ParseFileReader,
        .Reset = ParseFileReset,
        .Print = ParseFilePrinter
    };
    tree->Callbacks = &Callbacks;
}

/**
 * string parsing
 */

static size_t ParseStringCopy(char *buffer, size_t bufferLen, const char *src, size_t srcPos)
{
    memcpy(buffer, src + srcPos, bufferLen);
    return bufferLen;
}

static void ParseStringReset(void *cxtIn)
{
    
}

static size_t ParseStringReader(void* cxtIn, char* buffer, size_t bufferLen, size_t srcPos)
{
	const char *src = (const char *)cxtIn;
    return ParseStringCopy(buffer, bufferLen, src, srcPos);
}

static void ParseStringPrinter(void *cxtIn, totemLog *f, size_t srcLen, size_t srcPos)
{
    const char *src = (const char *)cxtIn;
    totemLog_Write(f, "%.*s", (int)srcLen, src + srcPos);
}

void totemParseTree_InitWithString(totemParseTree *tree, const char* str, size_t strLen)
{
    totemParseTree_Init(tree);
    
    tree->SrcCxt = (void*)str;
    tree->SrcLen = strLen;
    tree->SrcPos = 0;
    
    const static totemParseTreeClass Callbacks =
    {
        .Step = ParseStringReader,
        .Reset = ParseStringReset,
        .Print = ParseStringPrinter
    };
    tree->Callbacks = &Callbacks;
}

const char *totemParseStatus_Describe(totemParseStatus status)
{
    switch (status)
    {
            TOTEM_STRINGIFY_CASE(totemParseStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemParseStatus_Success);
            TOTEM_STRINGIFY_CASE(totemParseStatus_UnexpectedToken);
            TOTEM_STRINGIFY_CASE(totemParseStatus_ValueTooLarge);
            TOTEM_STRINGIFY_CASE(totemParseStatus_TooManyItems);
            TOTEM_STRINGIFY_CASE(totemParseStatus_LexError);
    }
    
    return "UNKNOWN";
}

const char *totemBinaryOperatorType_Describe(totemBinaryOperatorType type)
{
    switch (type)
    {
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_Assign);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_AsType);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_Divide);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_DivideAssign);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_Equals);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_IsType);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_LessThan);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_LessThanEquals);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_LogicalAnd);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_LogicalOr);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_Minus);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_MinusAssign);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_MoreThan);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_MoreThanEquals);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_Multiply);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_MultiplyAssign);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_None);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_NotEquals);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_Plus);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_PlusAssign);
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_BinaryLeftShift);
			TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_BinaryRightShift);
			TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_BinaryAnd);
			TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_BinaryOr);
			TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_BinaryXor);
    }
    
    return "UNKNOWN";
}
