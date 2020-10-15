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

static void totemParseTree_ParseExpressionSet(totemParseTree *, totemExpressionPrototype **, totemExpressionPrototype **, totemTokenType, totemTokenType, totemTokenType);
static void totemParseTree_ParseExpression(totemParseTree *, totemExpressionPrototype *);
static void totemParseTree_ParseArgument(totemParseTree *, totemArgumentPrototype *);
static void totemParseTree_ParseStatementSet(totemParseTree *, totemStatementPrototype **, totemStatementPrototype **);

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
    TOTEM_PARSE_PRIORITY_RIGHT(2, totemBinaryOperatorType_LeftShift),
	TOTEM_PARSE_PRIORITY_RIGHT(2, totemBinaryOperatorType_RightShift),
    
    TOTEM_PARSE_PRIORITY_LEFT(3, totemBinaryOperatorType_LogicalOr),
    
    TOTEM_PARSE_PRIORITY_LEFT(4, totemBinaryOperatorType_LogicalAnd),
    
    TOTEM_PARSE_PRIORITY_LEFT(5, totemBinaryOperatorType_Equals),
    TOTEM_PARSE_PRIORITY_LEFT(5, totemBinaryOperatorType_NotEquals),
    TOTEM_PARSE_PRIORITY_LEFT(5, totemBinaryOperatorType_IsType),
    
    TOTEM_PARSE_PRIORITY_LEFT(6, totemBinaryOperatorType_MoreThan),
    TOTEM_PARSE_PRIORITY_LEFT(6, totemBinaryOperatorType_LessThan),
    TOTEM_PARSE_PRIORITY_LEFT(6, totemBinaryOperatorType_MoreThanEquals),
    TOTEM_PARSE_PRIORITY_LEFT(6, totemBinaryOperatorType_LessThanEquals),
    
    TOTEM_PARSE_PRIORITY_LEFT(7, totemBinaryOperatorType_Plus),
    TOTEM_PARSE_PRIORITY_LEFT(7, totemBinaryOperatorType_Minus),
    
    TOTEM_PARSE_PRIORITY_LEFT(8, totemBinaryOperatorType_Multiply),
    TOTEM_PARSE_PRIORITY_LEFT(8, totemBinaryOperatorType_Divide),
    
    TOTEM_PARSE_PRIORITY_LEFT(9, totemBinaryOperatorType_AsType),
};

void totemParseTree_Cleanup(totemParseTree *tree)
{
	totemTokenizer_Cleanup(&tree->Tokenizer);
    totemMemoryBlock_Cleanup(&tree->LastMemBlock);
    tree->FirstBlock = NULL;
}

void totemParseTree_Reset(totemParseTree *tree)
{
    totemParseTree_Cleanup(tree);
}

void totemParseTree_Init(totemParseTree *tree)
{
	TOTEM_ZERO(tree);
}

static totemToken *totemParseTree_Current(totemParseTree *tree)
{
	return &tree->Tokenizer.CurrentToken;
}

static void totemParseTree_Break(totemParseTree *tree, totemParseStatus status, totemBufferPositionInfo *location)
{
	tree->ErrorAt = *location;
	tree->JmpStatus = status;
	TOTEM_JMP_THROW(&tree->Jmp);
}

static void totemParseTree_Rewind(totemParseTree *tree)
{
	totemTokenizer_Rewind(&tree->Tokenizer);
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

static void totemParseTree_SkipToken(totemParseTree *tree)
{
	totemTokenizer_Step(&tree->Tokenizer);
}

static void totemParseTree_SkipTokenNotEndScript(totemParseTree *tree)
{
	totemParseTree_SkipToken(tree);
	totemParseTree_EnforceNotToken(tree, totemTokenType_EndScript);
}

static void totemParseTree_SkipWhitespace(totemParseTree *tree)
{
	while (totemParseTree_Current(tree)->Type == totemTokenType_Whitespace) 
	{ 
		totemParseTree_SkipToken(tree);
	}
}

static void *totemParseTree_Alloc(totemParseTree *tree, size_t objectSize)
{
	void *ptr = totemMemoryBlock_Alloc(&tree->LastMemBlock, objectSize);
	if (!ptr)
	{
		totemParseTree_Break(tree, totemParseStatus_OutOfMemory, NULL);
	}

	return ptr;
}

static void totemParseTree_ParseIdentifier(totemParseTree *tree, totemString *string, totemBool strict)
{
	totemParseTree_SkipWhitespace(tree);
	totemBufferPositionInfo startingPos = totemParseTree_Current(tree)->Position;
	
	const char *start = totemParseTree_Current(tree)->Position.Start;
	totemStringLength length = 0;
	totemBool onlyNumbers = totemBool_True;
	totemTokenCategory categories = 0;
	size_t numTokens = 0;

	// cannot start with a number
	totemParseTree_EnforceNotToken(tree, totemTokenType_Number);

	while (totemParseTree_Current(tree)->Type != totemTokenType_Whitespace
		&& totemParseTree_Current(tree)->Category != totemTokenCategory_Symbol
		&& totemParseTree_Current(tree)->Type != totemTokenType_EndScript)
	{
		categories |= 1 << totemParseTree_Current(tree)->Category;
		onlyNumbers &= totemParseTree_Current(tree)->Type == totemTokenType_Number;
		length += totemParseTree_Current(tree)->Position.Length;
		numTokens++;
		totemParseTree_SkipTokenNotEndScript(tree);
	}

	// empty identifier
	if (numTokens == 0 || length == 0)
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

		// cannot just have numbers
		if (onlyNumbers)
		{
			totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &startingPos);
		}
	}

	string->Length = length;
	string->Value = start;
}

static void totemParseTree_ParseNumber(totemParseTree *tree, totemString *number)
{
	number->Length = 0;
	number->Value = totemParseTree_Current(tree)->Position.Start;

	if (totemParseTree_Current(tree)->Type == totemTokenType_Minus || totemParseTree_Current(tree)->Type == totemTokenType_Plus)
	{
		number->Length += totemParseTree_Current(tree)->Position.Length;
		totemParseTree_SkipTokenNotEndScript(tree);
	}

	totemBool dot = totemBool_False;
	totemTokenType lastTokenType = totemParseTree_Current(tree)->Type;

	while (totemParseTree_Current(tree)->Type == totemTokenType_Number || totemParseTree_Current(tree)->Type == totemTokenType_Dot)
	{
		if (totemParseTree_Current(tree)->Type == totemTokenType_Dot)
		{
			if (dot)
			{
				totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
			}

			dot = totemBool_True;
		}

		number->Length += totemParseTree_Current(tree)->Position.Length;
		lastTokenType = totemParseTree_Current(tree)->Type;
		totemParseTree_SkipTokenNotEndScript(tree);
	}

	if (lastTokenType != totemTokenType_Number)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
	}

	if (totemParseTree_Current(tree)->Type == totemTokenType_Identifier &&
		totemParseTree_Current(tree)->Position.Length == 1 &&
		(totemParseTree_Current(tree)->Position.Start[0] == 'e' || totemParseTree_Current(tree)->Position.Start[0] == 'E'))
	{
		number->Length += totemParseTree_Current(tree)->Position.Length;
		totemParseTree_SkipTokenNotEndScript(tree);

		if (totemParseTree_Current(tree)->Type == totemTokenType_Minus || totemParseTree_Current(tree)->Type == totemTokenType_Plus)
		{
			number->Length += totemParseTree_Current(tree)->Position.Length;
			totemParseTree_SkipTokenNotEndScript(tree);
		}

		totemParseTree_EnforceToken(tree, totemTokenType_Number);
		number->Length += totemParseTree_Current(tree)->Position.Length;
		totemParseTree_SkipTokenNotEndScript(tree);
	}
}

static void totemParseTree_ParsePreUnaryOperator(totemParseTree *tree, totemPreUnaryOperatorPrototype **type)
{
	totemParseTree_SkipWhitespace(tree);

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Multiply:
			*type = totemParseTree_Alloc(tree, sizeof(totemPreUnaryOperatorPrototype));
			totemParseTree_SkipTokenNotEndScript(tree);
			(*type)->Type = totemPreUnaryOperatorType_CloneOf;
			break;

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

			// parse parameters
			totemExpressionPrototype *first = NULL, *last = NULL;
			totemParseTree_ParseExpressionSet(tree, &first, &last, totemTokenType_LBracket, totemTokenType_RBracket, totemTokenType_Comma);
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
			(*type)->Parameters->LValue.Argument->Value.String = totemParseTree_Alloc(tree, sizeof(totemString));
			(*type)->Parameters->LValue.Argument->Type = totemArgumentType_String;

			totemParseTree_ParseIdentifier(tree, (*type)->Parameters->LValue.Argument->Value.String, totemBool_True);
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
					*type = totemBinaryOperatorType_RightShift;
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
					*type = totemBinaryOperatorType_LeftShift;
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

		case totemTokenType_Multiply:
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
			totemParseTree_EnforceToken(tree, totemTokenType_And);
			totemParseTree_SkipTokenNotEndScript(tree);
			*type = totemBinaryOperatorType_LogicalAnd;
			break;

		case totemTokenType_Or:
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_EnforceToken(tree, totemTokenType_Or);
			totemParseTree_SkipTokenNotEndScript(tree);
			*type = totemBinaryOperatorType_LogicalOr;
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

		default:
			*type = totemBinaryOperatorType_None;
			break;
	}
}

static totemBinaryOperatorType totemParseTree_ParseSubExpression(totemParseTree *tree, totemExpressionPrototype *expression, uint8_t priority)
{
	totemParseTree_SkipWhitespace(tree);
	expression->Position = totemParseTree_Current(tree)->Position;

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
	TOTEM_DEBUG_ASSERT(lValue->BinaryOperator < TOTEM_ARRAY_SIZE(s_binaryPriority) && lValue->BinaryOperator >= 0);
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

		TOTEM_DEBUG_ASSERT(lValue->BinaryOperator < TOTEM_ARRAY_SIZE(s_binaryPriority) && lValue->BinaryOperator >= 0);
		lastOp = lValue->BinaryOperator;
		binaryDesc = &s_binaryPriority[lValue->BinaryOperator];
	}

	// we may have prematurely parsed the next binary operator, return it if so
	totemBinaryOperatorType toReturn = lValue->BinaryOperator;
	lValue->BinaryOperator = totemBinaryOperatorType_None;
	return toReturn;
}

static void totemParseTree_ParseExpression(totemParseTree *tree, totemExpressionPrototype *expression)
{
	totemParseTree_ParseSubExpression(tree, expression, 0);
}

static void totemParseTree_ParseExpressionSet(totemParseTree *tree, totemExpressionPrototype **first, totemExpressionPrototype **last, totemTokenType start, totemTokenType end, totemTokenType split)
{
	totemParseTree_SkipWhitespace(tree);
	totemParseTree_EnforceToken(tree, start);
	totemParseTree_SkipTokenNotEndScript(tree);

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
		if (obj->Num + 1 == TOTEM_INT_MAX)
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
	totemBufferPositionInfo startingPos = totemParseTree_Current(tree)->Position;

	totemExpressionPrototype *first = NULL, *last = NULL;
	totemParseTree_ParseExpressionSet(tree, &first, &last, totemTokenType_LSBracket, totemTokenType_RSBracket, totemTokenType_Comma);

	if (!first)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &startingPos);
	}

	arr->Accessor = first;
	arr->Num = (totemInt)(last - first) + 1;
	if (arr->Num > TOTEM_INT_MAX || arr->Num < 0)
	{
		totemParseTree_Break(tree, totemParseStatus_TooManyItems, &startingPos);
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

static void totemParseTree_ParseFunctionDeclaration(totemParseTree *tree, totemFunctionDeclarationPrototype *func, totemBool isAnonymous)
{
	totemParseTree_SkipWhitespace(tree);
	// assuming that position has already been set
	// and "function" keyword has been skipped

	//totemParseTree_EnforceToken(tree, totemTokenType_Function);
	//func->Position = totemParseTree_Current(tree)->Position;
	//totemParseTree_SkipTokenNotEndScript(tree);
	//totemParseTree_SkipWhitespace(tree);

	if (!isAnonymous)
	{
		func->Identifier = totemParseTree_Alloc(tree, sizeof(totemString));
		totemParseTree_ParseIdentifier(tree, func->Identifier, totemBool_True);
	}

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

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Function:
		{
			totemBufferPositionInfo position = totemParseTree_Current(tree)->Position;
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_SkipWhitespace(tree);

			if (totemParseTree_Current(tree)->Type == totemTokenType_LBracket)
			{
				// anonymous function
				argument->Type = totemArgumentType_FunctionDeclaration;
				argument->Value.FunctionDeclaration = totemParseTree_Alloc(tree, sizeof(totemFunctionDeclarationPrototype));
				argument->Value.FunctionDeclaration->Position = position;
				totemParseTree_ParseFunctionDeclaration(tree, argument->Value.FunctionDeclaration, totemBool_True);
			}
			else
			{
				// function type
				argument->Value.DataType = totemPublicDataType_Function;
				argument->Type = totemArgumentType_Type;
			}

			break;
		}

		// boolean
		case totemTokenType_True:
		case totemTokenType_False:
			argument->Type = totemArgumentType_Boolean;
			argument->Value.Boolean = totemParseTree_Current(tree)->Type == totemTokenType_True;
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

		case totemTokenType_Coroutine:
			argument->Value.DataType = totemPublicDataType_Coroutine;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Userdata:
			argument->Value.DataType = totemPublicDataType_Userdata;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

		case totemTokenType_Boolean:
			argument->Value.DataType = totemPublicDataType_Boolean;
			argument->Type = totemArgumentType_Type;
			totemParseTree_SkipTokenNotEndScript(tree);
			break;

			// new object
		case totemTokenType_LCBracket:
			argument->Type = totemArgumentType_NewObject;
			argument->Value.NewObject = totemParseTree_Alloc(tree, sizeof(totemNewObjectPrototype));
			totemParseTree_ParseNewObject(tree, argument->Value.NewObject);
			break;

			// new array
		case totemTokenType_LSBracket:
			argument->Type = totemArgumentType_NewArray;
			argument->Value.NewArray = totemParseTree_Alloc(tree, sizeof(totemNewArrayPrototype));
			totemParseTree_ParseNewArray(tree, argument->Value.NewArray);
			break;

			// variable
		case totemTokenType_Let:
		case totemTokenType_Var:
			argument->Type = totemArgumentType_Variable;
			argument->Value.Variable = totemParseTree_Alloc(tree, sizeof(totemVariablePrototype));
			totemParseTree_ParseVariable(tree, argument->Value.Variable);
			break;

			// number
		case totemTokenType_Number:
			argument->Type = totemArgumentType_Number;
			argument->Value.Number = totemParseTree_Alloc(tree, sizeof(totemString));
			totemParseTree_ParseNumber(tree, argument->Value.Number);
			break;

			// string
		case totemTokenType_DoubleQuote:
			totemParseTree_SkipTokenNotEndScript(tree);

			argument->Type = totemArgumentType_String;
			const char *begin = totemParseTree_Current(tree)->Position.Start;
			totemStringLength len = 0;

			while (totemParseTree_Current(tree)->Type != totemTokenType_DoubleQuote)
			{
				len += totemParseTree_Current(tree)->Position.Length;

				if (totemParseTree_Current(tree)->Type == totemTokenType_Backslash)
				{
					totemParseTree_SkipTokenNotEndScript(tree);

					if (totemParseTree_Current(tree)->Type == totemTokenType_DoubleQuote)
					{
						len += totemParseTree_Current(tree)->Position.Length;
						totemParseTree_SkipTokenNotEndScript(tree);
					}
				}
				else
				{
					totemParseTree_SkipTokenNotEndScript(tree);
				}
			}

			argument->Value.String = totemParseTree_Alloc(tree, sizeof(totemString));
			argument->Value.String->Value = begin;
			argument->Value.String->Length = len;

			totemParseTree_SkipTokenNotEndScript(tree);
			break;

			// identifier
		case totemTokenType_Identifier:
			argument->Type = totemArgumentType_Identifier;
			argument->Value.Identifier = totemParseTree_Alloc(tree, sizeof(totemString));
			totemParseTree_ParseIdentifier(tree, argument->Value.Identifier, totemBool_True);
			break;

		default:
			totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
			break;
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
    
    totemParseTree_SkipTokenNotEndScript(tree);
    totemParseTree_SkipWhitespace(tree);
    totemParseTree_EnforceToken(tree, totemTokenType_While);
    totemParseTree_SkipTokenNotEndScript(tree);
    
    loop->Expression = totemParseTree_Alloc(tree, sizeof(totemExpressionPrototype));
	totemParseTree_ParseExpression(tree, loop->Expression);
    totemParseTree_SkipWhitespace(tree);

    totemParseTree_EnforceToken(tree, totemTokenType_Semicolon);
	totemParseTree_SkipToken(tree);
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
    
	if (totemParseTree_Current(tree)->Type == totemTokenType_LBracket)
    {
        totemParseTree_SkipTokenNotEndScript(tree);
    }
    
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
    
	if (totemParseTree_Current(tree)->Type == totemTokenType_RBracket)
    {
        totemParseTree_SkipTokenNotEndScript(tree);
    }
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
	totemParseTree_ParseStatementSet(tree, &firstStatement, &lastStatement);
    loop->StatementsStart = firstStatement;
}

static void totemParseTree_ParseSimpleStatement(totemParseTree *tree, totemExpressionPrototype *exp)
{
	totemParseTree_ParseExpression(tree, exp);

	totemParseTree_SkipWhitespace(tree);
	totemParseTree_EnforceToken(tree, totemTokenType_Semicolon);
	totemParseTree_SkipToken(tree);
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

	while (totemParseTree_Current(tree)->Type != totemTokenType_RCBracket)
	{
		totemParseTree_ParseStatementInSet(tree, first, last);
	}

	totemParseTree_SkipToken(tree);
}

static void totemParseTree_ParseBlock(totemParseTree *tree, totemBlockPrototype *block)
{
	totemParseTree_SkipWhitespace(tree);
	block->Position = totemParseTree_Current(tree)->Position;

	switch (totemParseTree_Current(tree)->Type)
	{
		case totemTokenType_Function:
		{
			block->Type = totemBlockType_FunctionDeclaration;
			block->Ptr.FuncDec = totemParseTree_Alloc(tree, sizeof(totemFunctionDeclarationPrototype));
			block->Position = totemParseTree_Current(tree)->Position;
			totemParseTree_SkipTokenNotEndScript(tree);
			totemParseTree_ParseFunctionDeclaration(tree, block->Ptr.FuncDec, totemBool_False);
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

/**
* Create parse tree from token list
*/
totemParseStatus totemParseTree_Parse(totemParseTree *tree, const char *start, size_t len)
{
	totemParseStatus status = totemParseStatus_Success;
	
	TOTEM_JMP_TRY(&tree->Jmp)
	{
		totemTokenizer_Begin(&tree->Tokenizer, start, len);

		totemBlockPrototype *lastBlock = NULL;
		while (totemParseTree_Current(tree)->Type != totemTokenType_EndScript)
		{
			totemBlockPrototype *block = totemParseTree_Alloc(tree, sizeof(totemBlockPrototype));
			totemParseTree_ParseBlock(tree, block);

			if (lastBlock == NULL)
			{
				tree->FirstBlock = block;
			}
			else
			{
				lastBlock->Next = block;
			}

			lastBlock = block;
		}
	}
	TOTEM_JMP_CATCH(tree->Jmp)
	{
		status = tree->JmpStatus;
	}

	return status;
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
            TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_LeftShift);
			TOTEM_STRINGIFY_CASE(totemBinaryOperatorType_RightShift);
    }
    
    return "UNKNOWN";
}