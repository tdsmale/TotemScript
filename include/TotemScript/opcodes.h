//
//  opcodes.h
//  TotemScript
//
//  Created by Timothy Smale on 23/08/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_OPCODES_H
#define TOTEMSCRIPT_OPCODES_H

/**
 * Opcode definition format:
 *  - op        = totemOperationType
 *  - type      = totemInstructionType
 *  - behaviour = totemOperationBehaviourType
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
#define TOTEM_EMIT_OPCODE_AB2U_RETURN(_, op)		TOTEM_EMIT_OPCODE_AB2U(_, op, Return)

#define TOTEM_EMIT_OPCODE_A3_CONTROL(_, op)			TOTEM_EMIT_OPCODE_A3(_, op, PureControl)
#define TOTEM_EMIT_OPCODE_AB2_CONTROL(_, op)		TOTEM_EMIT_OPCODE_AB2(_, op, PureControl)

#define TOTEM_EMIT_OPCODE_A_PURE(_, op)				TOTEM_EMIT_OPCODE_A(_, op, Pure)
#define TOTEM_EMIT_OPCODE_AB_PURE(_, op)			TOTEM_EMIT_OPCODE_AB(_, op, Pure)
#define TOTEM_EMIT_OPCODE_AB2U_PURE(_, op)			TOTEM_EMIT_OPCODE_AB2U(_, op, Pure)
#define TOTEM_EMIT_OPCODE_AB2UG_PURE(_, op)			TOTEM_EMIT_OPCODE_AB2UG(_, op, Pure)
#define TOTEM_EMIT_OPCODE_ABC_PURE(_, op)			TOTEM_EMIT_OPCODE_ABC(_, op, Pure)
#define TOTEM_EMIT_OPCODE_ABC1U_PURE(_, op)			TOTEM_EMIT_OPCODE_ABC1U(_, op, Pure)

#define TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, op ## _IntInt)							/* a = b op c, where b and c are known to be ints */ \
	TOTEM_EMIT_OPCODE_ABC1U_PURE(_, op ## _IntIntConst)						/* a = b op c1u, where b is known to be an int */ \

#define TOTEM_EMIT_OPCODE_ABC_ARITH_FLOAT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, op ## _FloatFloat)						/* a = b op c, where b and c are known to be floats */ \

#define TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY_DIVIDE(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op ## _IntInt)							/* a = b op c, where b and c are known to be ints */ \
	TOTEM_EMIT_OPCODE_ABC1U_PURE(_, op ## _IntIntConst)						/* a = b op c1u, where b is known to be an int */ \

#define TOTEM_EMIT_OPCODE_ABC_ARITH_FLOAT_ONLY_DIVIDE(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op ## _FloatFloat)					/* a = b op c, where b and c are known to be floats */ \

#define TOTEM_EMIT_OPCODE_ABC_ARITH(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op) 									/* a = b op c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_ABC_ARITH_FLOAT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODE_ABC_ARITH_DIVIDE(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op) 									/* a = b op c */ \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, op ## Unsafe_IntInt) \
	TOTEM_EMIT_OPCODE_ABC_ARITH_FLOAT_ONLY(_, op ## Unsafe) \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY_DIVIDE(_, op) \
	TOTEM_EMIT_OPCODE_ABC_ARITH_FLOAT_ONLY_DIVIDE(_, op) \

#define TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, op) \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, op) 									/* a = b op c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODE_AB_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_AB_PURE(_, op ## _Int)								/* a = op b, where b is known to be an int */ \

#define TOTEM_EMIT_OPCODE_AB_ARITH_FLOAT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_AB_PURE(_, op ## _Float)								/* a = op b, where b is known to be a float */ \

#define TOTEM_EMIT_OPCODE_AB_ARITH(_, op) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, op) 										/* a = op b */ \
	TOTEM_EMIT_OPCODE_AB_ARITH_INT_ONLY(_, op) \
	TOTEM_EMIT_OPCODE_AB_ARITH_FLOAT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODE_AB_ARITH_INT(_, op) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, op) 										/* a = op b */ \
	TOTEM_EMIT_OPCODE_AB_ARITH_INT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODE_AB_ARITH_FLOAT(_, op) \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, op) 										/* a = op b */ \
	TOTEM_EMIT_OPCODE_AB_ARITH_FLOAT_ONLY(_, op) \

#define TOTEM_EMIT_OPCODES_BINARY_ARITH(_) \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Add) 									/* a = b + c */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, Add_ArrayArray)							/* a = b + c, where b and c are known to be arrays */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, Add_StringString) 						/* a = b + c, where b and c are known to be strings */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Subtract)								/* a = b - c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, Multiply)								/* a = b * c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_DIVIDE(_, Divide)							/* a = b / c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_DIVIDE(_, Modulus)							/* a = b % c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryLeftShift)						/* a = b << c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryRightShift)					/* a = b >> c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryAnd)							/* a = b & c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryOr)							/* a = b | c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH_INT(_, BinaryXor)							/* a = b ^ c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, LessThan)								/* a = b < c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, LessThanEquals)							/* a = b <= c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, MoreThan)								/* a = b > c */ \
	TOTEM_EMIT_OPCODE_ABC_ARITH(_, MoreThanEquals)							/* a = b >= c */ \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, Equals)									/* a = b == c */ \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, NotEquals)								/* a = b != c */ \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, LogicalOr)								/* a = b || c */ \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, LogicalAnd)								/* a = b && c */ \

#define TOTEM_EMIT_OPCODES_UNARY_ARITH(_) \
    TOTEM_EMIT_OPCODE_AB_ARITH_INT(_, BinaryNot)                            /* a = ~b */ \
    TOTEM_EMIT_OPCODE_AB_PURE(_, LogicalNegate)                                /* a = !b */ \

#define TOTEM_EMIT_OPCODES(_) \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Move) 										/* a = b */ \
	TOTEM_EMIT_OPCODE_A_PURE(_, Move_Null) 									/* a = null */ \
	TOTEM_EMIT_OPCODE_AB2U_PURE(_, Move_Int) 								/* a = int(b2) */ \
	TOTEM_EMIT_OPCODE_AB2U_PURE(_, Move_Bool) 								/* a = bool(b2) */ \
	TOTEM_EMIT_OPCODE_AB2U_PURE(_, Move_Type)								/* a = type(b2) */ \
	TOTEM_EMIT_OPCODE_AB2UG_PURE(_, MoveToLocal)							/* a = b2ug */ \
	TOTEM_EMIT_OPCODE_AB2UG_NORMAL(_, MoveToGlobal) 						/* b2ug = a */ \
	TOTEM_EMIT_OPCODES_BINARY_ARITH(_) \
    TOTEM_EMIT_OPCODES_UNARY_ARITH(_) \
	TOTEM_EMIT_OPCODE_AB_ARITH_FLOAT(_, Negate) 							/* a = -b */ \
	TOTEM_EMIT_OPCODE_AB2_CONTROL(_, ConditionalGoto)						/* if a goto instruction b2 */ \
	TOTEM_EMIT_OPCODE_A3_CONTROL(_, Goto) 									/* goto instruction a3 */ \
	TOTEM_EMIT_OPCODE_A_RETURN(_, Return)									/* return a */ \
	TOTEM_EMIT_OPCODE_AB2U_RETURN(_, Yield)									/* pause at instruction b2u, yield a */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, NewArray_WithIntConst) 				/* a = array of size b2u */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewArray_WithInt)						/* a = array of size b */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewArray_WithFloat)						/* a = array of size b */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewArray)								/* a = array of size b */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, NewObject_WithIntConst)				/* a = Object with initial size b2u */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewObject_WithInt)						/* a = object of size b */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewObject_WithFloat)					    /* a = object of size b */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, NewObject) 								/* a = Object with initial size b */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, LengthOf_Array) 							/* a = lengthof(b) where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, LengthOf_Object) 							/* a = lengthof(b) where b is known to be a Object */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, LengthOf_String) 							/* a = lengthof(b) where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, LengthOf) 									/* a = lengthof(b) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_String_WithInt)				/* a = b[c] where b in known to be a string and c is known to be an int */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_String_WithFloat)			/* a = b[c] where b in known to be a string and c is known to be a float */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexGet_String_WithIntConst)		/* a = b[c1u] where b in known to be a string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_String)						/* a = b[c] where b in known to be a string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_Array_WithFloat)			/* a = b[c] where b in known to be an array and c is known to be a float */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_Array_WithInt)				/* a = b[c] where b in known to be an array and c is known to be an int */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexGet_Array_WithIntConst)		/* a = b[c1u] where b in known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_Array) 						/* a = b[c] where b in known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_Object_WithInternedString)	/* a = b[c], where c is an interned string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_Object_WithMiniString)		/* a = b[c], where c is a mini string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_Object)						/* a = b[c] where b in known to be a Object */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_WithFloat)					/* a = b[c], where c is a float */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_WithInt) 					/* a = b[c], where c is an int */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexGet_WithIntConst)				/* a = b[c1u] */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_WithInternedString)			/* a = b[c], where c is an interned string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet_WithMiniString)				/* a = b[c], where c is a mini string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexGet) 							/* a = b[c] */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_Array_WithFloat)			/* b[c] = a where b in known to be an array and c is known to be a float */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_Array_WithInt)				/* b[c] = a where b in known to be an array and c is known to be an int */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexSet_Array_WithIntConst)		/* b[c1u] = a where b in known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_Array) 						/* b[c] = a where b in known to be an array */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_Object_WithInternedString)	/* b[c] = a, where c is an interned string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_Object_WithMiniString)		/* b[c] = a, where c is a mini string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_Object)						/* b[c] = a where b in known to be a Object */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_WithFloat)					/* b[c] = a, where c is a float */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_WithInt) 					/* b[c] = a, where c is an int */ \
	TOTEM_EMIT_OPCODE_ABC1U_NORMAL(_, ComplexSet_WithIntConst)				/* b[c1u] = a */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_WithInternedString)			/* b[c] = a, where c is an interned string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet_WithMiniString)				/* b[c] = a, where c is a mini string */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, ComplexSet) 							/* b[c] = a */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, ComplexUnset_Object_WithInternedString)	/* a[b] = null, where c is an interned string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, ComplexUnset_Object_WithMiniString)		/* a[b] = null, where c is a mini string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, ComplexUnset_Object)						/* a[b] = null where b in known to be a Object */ \
	TOTEM_EMIT_OPCODE_ABC_PURE(_, Is)										/* a = b is type of c */ \
	TOTEM_EMIT_OPCODE_ABC1U_PURE(_, IsType)									/* a = b is type with id c1u */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_Float_ToInt)							/* a = b as int, where b is known to be a float */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_String_ToInt)							/* a = b as int, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_Object_ToInt)							/* a = b as int, where b is known to be an objectg */ \
    TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_ToInt)								/* a = b as int */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_Int_ToFloat)							/* a = b as float, where b is known to be an int */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_String_ToFloat)						/* a = b as float, where b is known to be a string */ \
    TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_ToFloat)								/* a = b as float */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Int_ToString)						/* a = b as string, where b is known to be an int */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Type_ToString)						/* a = b as string, where b is known to be a type */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Array_ToString)						/* a = b as string, where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Object_ToString)					/* a = b as string, where b is known to be a Object */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Float_ToString)					    /* a = b as string, where b is known to be a float */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Function_ToString)					/* a = b as coroutine, where b is known to be a function */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Coroutine_ToString)					/* a = b as string, where b is known to be a coroutine */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToString)							/* a = b as string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_String_ToArray)						/* a = b as array, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Object_ToArray)						/* a = b as array, where b is known to be a Object */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToArray)							/* a = b as array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_String_ToFunction)					/* a = b as function, where b is known to be a string */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Coroutine_ToFunction)				/* a = b as function, where b is known to be a coroutine */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToFunction)							/* a = b as function */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Function_ToCoroutine)				/* a = b as coroutine, where b is known to be a function */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToCoroutine)						/* a = b as coroutine */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_ToBool)								/* a = b as bool */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_Array_ToInt)							/* a = b as int, where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_Array_ToObject)						/* a = b as Object, where b is known to be an array */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_String_ToObject)					/* a = b as Object, where b is known to be a string */ \
    TOTEM_EMIT_OPCODE_AB_NORMAL(_, Cast_ToObject)							/* a = b as Object */ \
	TOTEM_EMIT_OPCODE_AB_PURE(_, Cast_ToType)								/* a = typeof(b) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, Cast)									/* a = b as type of c */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke)								/* a() with b2u args */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke_Native)						/* a() with b2u args, where a is known to be a native call */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke_Instance)					/* a() with b2u args, where a is known to be an instance call */ \
	TOTEM_EMIT_OPCODE_AB2U_NORMAL(_, PreInvoke_Coroutine)					/* a() with b2u args, where a is known to be a coroutine */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args)						/* a() with 2 args */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args_Native)					/* a() with 2 args, where a is known to be a native call */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args_Instance)				/* a() with 2 args, where a is known to be an instance */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, PreInvoke_2Args_Coroutine)				/* a() with 2 args, where a is known to be a coroutine */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, Invoke)									/* a = () */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, Invoke_Coroutine)							/* a = () */ \
	TOTEM_EMIT_OPCODE_A_NORMAL(_, Invoke_Subroutine)						/* a = () */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FunctionArg_Invoke)						/* b = (a) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FunctionArg_2_Invoke)					/* c = (a, b) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FunctionArg_3)							/* (a, b, c) */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs)						/* a = b() */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs_Native)				/* a = b(), where b is assumed to be a native call */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs_Instance)				/* a = b(), where b is assumed to be an instance call */ \
	TOTEM_EMIT_OPCODE_AB_NORMAL(_, FullInvoke_NoArgs_Coroutine)				/* a = b(), where b is assumed to be a coroutine */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg)						/* a = b(c) */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg_Native)					/* a = b(c), where b is assumed to be a native call */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg_Instance)				/* a = b(c), where b is assumed to be an instance call */ \
	TOTEM_EMIT_OPCODE_ABC_NORMAL(_, FullInvoke_1Arg_Coroutine)				/* a = b(c), where b is assumed to be a coroutine */ \

#endif
