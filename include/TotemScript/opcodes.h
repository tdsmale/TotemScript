//
//  opcodes.h
//  TotemScript
//
//  Created by Timothy Smale on 23/08/2016.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_OPCODES_H
#define TOTEMSCRIPT_OPCODES_H

#define TOTEM_EMIT_OPCODES() \
TOTEM_OPCODE_FORMAT(totemOperationType_Move)                 \
TOTEM_OPCODE_FORMAT(totemOperationType_Add)                 \
TOTEM_OPCODE_FORMAT(totemOperationType_Subtract)            \
TOTEM_OPCODE_FORMAT(totemOperationType_Multiply)            \
TOTEM_OPCODE_FORMAT(totemOperationType_Divide)              \
TOTEM_OPCODE_FORMAT(totemOperationType_Equals)              \
TOTEM_OPCODE_FORMAT(totemOperationType_NotEquals)           \
TOTEM_OPCODE_FORMAT(totemOperationType_LessThan)            \
TOTEM_OPCODE_FORMAT(totemOperationType_LessThanEquals)      \
TOTEM_OPCODE_FORMAT(totemOperationType_MoreThan)           \
TOTEM_OPCODE_FORMAT(totemOperationType_MoreThanEquals)     \
TOTEM_OPCODE_FORMAT(totemOperationType_LogicalOr)          \
TOTEM_OPCODE_FORMAT(totemOperationType_LogicalAnd)         \
TOTEM_OPCODE_FORMAT(totemOperationType_ConditionalGoto)    \
TOTEM_OPCODE_FORMAT(totemOperationType_Goto)               \
TOTEM_OPCODE_FORMAT(totemOperationType_FunctionArg)        \
TOTEM_OPCODE_FORMAT(totemOperationType_Return)             \
TOTEM_OPCODE_FORMAT(totemOperationType_NewArray)           \
TOTEM_OPCODE_FORMAT(totemOperationType_ComplexGet)         \
TOTEM_OPCODE_FORMAT(totemOperationType_ComplexSet)         \
TOTEM_OPCODE_FORMAT(totemOperationType_MoveToLocal)        \
TOTEM_OPCODE_FORMAT(totemOperationType_MoveToGlobal)       \
TOTEM_OPCODE_FORMAT(totemOperationType_Is)                 \
TOTEM_OPCODE_FORMAT(totemOperationType_As)                 \
TOTEM_OPCODE_FORMAT(totemOperationType_Invoke)				\
TOTEM_OPCODE_FORMAT(totemOperationType_NewObject)			\
TOTEM_OPCODE_FORMAT(totemOperationType_ComplexShift)		\
TOTEM_OPCODE_FORMAT(totemOperationType_PreInvoke)			\
TOTEM_OPCODE_FORMAT(totemOperationType_LogicalNegate)	\

#endif