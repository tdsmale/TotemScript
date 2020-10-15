//
//  opcodes.h
//  TotemScript
//
//  Created by Timothy Smale on 23/08/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_OPCODES_H
#define TOTEMSCRIPT_OPCODES_H

typedef enum totemOpcodeBehaviourFlag
{
    totemOpcodeBehaviourFlag_None				= 0,
    totemOpcodeBehaviourFlag_Control			= 1,		// increment instruction pointer by return value of dispatch
    totemOpcodeBehaviourFlag_Return				= 1 << 1,	// return after dispatch
}
totemOpcodeBehaviourFlag;

/**
 * Opcode definition format:
 *  - op        = totemOperationType
 *  - type      = totemInstructionType
 *  - behaviour = totemOpcodeBehaviourFlag
 */
#define TOTEM_EMIT_OPCODE(_, op, type, behaviour) _(op, type, behaviour)

#define TOTEM_EMIT_OPCODE_A(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, A, behaviour)
#define TOTEM_EMIT_OPCODE_A3(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, A3, behaviour)
#define TOTEM_EMIT_OPCODE_A3U(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, A3u, behaviour)
#define TOTEM_EMIT_OPCODE_AB(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, Ab, behaviour)
#define TOTEM_EMIT_OPCODE_AB2(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, Ab2, behaviour)
#define TOTEM_EMIT_OPCODE_AB2U(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Ab2u, behaviour)
#define TOTEM_EMIT_OPCODE_AB2UG(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Ab2ug, behaviour)
#define TOTEM_EMIT_OPCODE_ABC(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, Abc, behaviour)
#define TOTEM_EMIT_OPCODE_ABC1(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Abc1, behaviour)
#define TOTEM_EMIT_OPCODE_ABC1U(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Abc1u, behaviour)
#define TOTEM_EMIT_OPCODE_AB1C1(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Ab1c1, behaviour)

#define TOTEM_EMIT_OPCODE_A_NORMAL(_, op)			TOTEM_EMIT_OPCODE_A(_, op, None)
#define TOTEM_EMIT_OPCODE_AB_NORMAL(_, op)			TOTEM_EMIT_OPCODE_AB(_, op, None)
#define TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op)			TOTEM_EMIT_OPCODE_ABC(_, op, None)
#define TOTEM_EMIT_OPCODE_AB2_NORMAL(_, op)			TOTEM_EMIT_OPCODE_AB2(_, op, None)
#define TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, op)		TOTEM_EMIT_OPCODE_AB2U(_, op, None)
#define TOTEM_EMIT_OPCODE_AB2UG_NORMAL(_, op)		TOTEM_EMIT_OPCODE_AB2UG(_, op, None)
#define TOTEM_EMIT_OPCODE_ABC1_NORMAL(_, op)		TOTEM_EMIT_OPCODE_ABC1(_, op, None)
#define TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, op)		TOTEM_EMIT_OPCODE_ABC1U(_, op, None)
#define TOTEM_EMIT_OPCODE_AB1C1_NORMAL(_, op)		TOTEM_EMIT_OPCODE_AB1C1(_, op, None)

#define TOTEM_EMIT_OPCODE_A_RETURN(_, op)			TOTEM_EMIT_OPCODE_A(_, op,  Return)
#define TOTEM_EMIT_OPCODE_A3U_RETURN(_, op)			TOTEM_EMIT_OPCODE_A3U(_, op, Return)

#define TOTEM_EMIT_OPCODE_A3_CONTROL(_, op)			TOTEM_EMIT_OPCODE_A3(_, op, Control)
#define TOTEM_EMIT_OPCODE_AB2_CONTROL(_, op)		TOTEM_EMIT_OPCODE_AB2(_, op, Control)

#define TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op ## _IntInt)					/* a = b op c, where b and c are known to be ints */ \
	TOTEM_EMIT_OPCODE_ABC1_NORMAL(_, op ## _IntIntConst)			/* a = b op c1, where b is known to be an int */ \
	TOTEM_EMIT_OPCODE_AB1C1_NORMAL(_, op ## _IntConstIntConst)		/* a = b1 op c1 */ \

#define TOTEM_EMIT_OPCODE_ABC_ARITH_FLOAT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op ## _FloatFloat)				/* a = b op c, where b and c are known to be floats */ \

#define TOTEM_EMIT_OPCODE_ABC_ARITH(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op) 							/* a = b op c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_ABC_ARITH_FLOAT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op) 							/* a = b op c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODE_AB_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, op ## _Int)			/* a = op b, where b is known to be an int */ \
	TOTEM_EMIT_OPCODE_AB2_NORMAL(_, op ## _IntConst) 	/* a = op b2 */ \

#define TOTEM_EMIT_OPCODE_AB_ARITH_FLOAT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, op ## _Float)		/* a = op b, where b is known to be a float */ \

#define TOTEM_EMIT_OPCODE_AB_ARITH(_, op) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, op) 					/* a = op b */ \
	TOTEM_EMIT_OPCODE_AB_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_AB_ARITH_FLOAT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODE_AB_ARITH_INT(_, op) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, op) 					/* a = op b */ \
	TOTEM_EMIT_OPCODE_AB_ARITH_INT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODES(_) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Move) 							/* a = b*/ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveNull) 						/* a = null*/ \
	TOTEM_EMIT_OPCODE_AB2_NORMAL(_, MoveInt) 						/* a = b2 */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, MoveTrue) 						/* a = true */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, MoveFalse) 						/* a = false */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeNull)						/* a = typeof(null) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeInt)						/* a = typeof(int) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeFloat)					/* a = typeof(float) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeString)					/* a = typeof(string) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeArray)					/* a = typeof(array) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeType)						/* a = typeof(type) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeFunction)					/* a = typeof(function) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeCoroutine)				/* a = typeof(coroutine) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeObject)					/* a = typeof(object) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeUserdata)					/* a = typeof(userdata) */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, MoveTypeBoolean)					/* a = typeof(bool) */ \
	TOTEM_EMIT_OPCODE_AB2UG_NORMAL(_, MoveToLocal)					/* a = b2ug */ \
	TOTEM_EMIT_OPCODE_AB2UG_NORMAL(_, MoveToGlobal) 				/* b2ug = a */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Add) 							/* a = b + c */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, AddArrayArray)					/* a = b + c, where b and c are known to be arrays */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, AddStringString) 				/* a = b + c, where b and c are known to be strings */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Subtract)						/* a = b - c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Multiply)						/* a = b * c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Divide)							/* a = b / c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Modulus)							/* a = b % c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryLeftShift)				/* a = b << c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryRightShift)			/* a = b >> c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryAnd)					/* a = b & c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryOr)					/* a = b | c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryXor)					/* a = b ^ c */ \
	TOTEM_EMIT_OPCODE_AB_ARITH_INT(_, BinaryNot)					/* a = ~b */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, LessThan)						/* a = b < c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, LessThanEquals)					/* a = b <= c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, MoreThan)						/* a = b > c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, MoreThanEquals)					/* a = b >= c */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, Equals)							/* a = b == c */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, NotEquals)						/* a = b != c */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, LogicalOr)						/* a = b || c */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, LogicalAnd)						/* a = b && c */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, LogicalNegate)					/* a = !b */ \
	TOTEM_EMIT_OPCODE_AB_ARITH(_, Negate) 							/* a = -b */ \
	TOTEM_EMIT_OPCODE_AB2_CONTROL(_, ConditionalGoto)				/* if a goto instruction b2 */ \
	TOTEM_EMIT_OPCODE_A3_CONTROL(_, Goto) 							/* goto instruction a3 */ \
	TOTEM_EMIT_OPCODE_A_RETURN(_, Return)							/* return a */ \
	TOTEM_EMIT_OPCODE_A3U_RETURN(_, Yield)							/* pause at instruction a3u, yield */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewArray)						/* a = array of size b */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, NewArrayWithLength) 			/* a = array of size b2u */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewObject) 						/* a = object with initial size b */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, NewObjectWithLength) 			/* a = object with initial size b2u */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, LengthOf) 						/* a = lengthof(b) */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, LengthOfArray) 					/* a = lengthof(b) where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, LengthOfObject) 					/* a = lengthof(b) where b is known to be an object */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, LengthOfString) 					/* a = lengthof(b) where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, CloneOf) 						/* a = clone(b) */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, CloneOfGCObject) 				/* a = clone(b) where b is known to be a gcobject */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, CloneOfArray) 					/* a = clone(b) where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, CloneOfObject) 					/* a = clone(b) where b is known to be an object */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, CloneOfUserdata) 				/* a = clone(b) where b is known to be userdata */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, CloneOfCoroutine) 				/* a = clone(b) where b is known to be a coroutine */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, CloneOfInstance) 				/* a = clone(b) where b is known to be an instance */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet) 					/* a = b[c] */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGetArray) 				/* a = b[c] where b in known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexGetArrayAtIndex)		/* a = b[c1u] where b in known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGetObject)				/* a = b[c] where b in known to be an object */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGetString)				/* a = b[c] where b in known to be a string */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexGetStringAtIndex)		/* a = b[c1u] where b in known to be a string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet)						/* b[c] = a */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSetArray)				/* b[c] = a where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexSetArrayAtIndex)		/* b[c1u] = a where b in known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSetObject)				/* b[c] = a where b in known to be an object */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, Is)								/* a = b is type of c */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, IsType)						/* a = b is type with id c1u */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, Cast)							/* a = b as type of c */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToInt)						/* a = b as int */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_BoolToInt)					/* a = b as int, where b is known to be a bool */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_FloatToInt)					/* a = b as int, where b is known to be a float */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_StringToInt)				/* a = b as int, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ArrayToInt)					/* a = b as int, where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ObjectToInt)				/* a = b as int, where b is known to be an object */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToFloat)					/* a = b as float */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_BoolToFloat)				/* a = b as float, where b is known to be a bool */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_IntToFloat)					/* a = b as float, where b is known to be an int */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_StringToFloat)				/* a = b as float, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ArrayToFloat)				/* a = b as int, where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ObjectToFloat)				/* a = b as int, where b is known to be an object */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToString)					/* a = b as string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_IntToString)				/* a = b as string, where b is known to be an int */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_TypeToString)				/* a = b as string, where b is known to be a type */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ArrayToString)				/* a = b as string, where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ObjectToString)				/* a = b as string, where b is known to be an object */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_FloatToString)				/* a = b as string, where b is known to be a float */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_CoroutineToString)			/* a = b as string, where b is known to be a coroutine */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_FunctionToString)			/* a = b as string, where b is known to be a function */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToArray)					/* a = b as array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_StringToArray)				/* a = b as array, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ObjectToArray)				/* a = b as array, where b is known to be an object */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToFunction)					/* a = b as function */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_StringToFunction)			/* a = b as function, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_CoroutineToFunction)		/* a = b as function, where b is known to be a coroutine */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToCoroutine)				/* a = b as coroutine */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_FunctionToCoroutine)		/* a = b as coroutine, where b is known to be a function */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToBoolean)					/* a = b as bool */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToObject)					/* a = b as object */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ArrayToObject)				/* a = b as object, where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_StringToObject)				/* a = b as object, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToType)						/* a = typeof(b) */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke)						/* a() with b2u args */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke_Native)				/* a() with b2u args, where a is known to be a native call */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke_Instance)			/* a() with b2u args, where a is known to be an instance */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke_Coroutine)			/* a() with b2u args, where a is known to be a coroutine */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args)				/* a() with 2 args */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args_Native)			/* a() with 2 args, where a is known to be a native call */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args_Instance)		/* a() with 2 args, where a is known to be an instance */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args_Coroutine)		/* a() with 2 args, where a is known to be a coroutine */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, Invoke)							/* a() */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FunctionArg_Invoke)				/* a(b) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FunctionArg_2_Invoke)			/* a(b, c) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FunctionArg_3)					/* (a, b, c) */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs)				/* a = b() */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs_Native)		/* a = b(), where b is assumed to be a native call */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs_Instance)		/* a = b(), where b is assumed to be an instance */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs_Coroutine)		/* a = b(), where b is assumed to be a coroutine */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg)				/* a = b(c) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg_Native)			/* a = b(c), where b is assumed to be a native call */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg_Instance)		/* a = b(c), where b is assumed to be an instance */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg_Coroutine)		/* a = b(c), where b is assumed to be a coroutine */ \

#endif