//
//  exec_vm.c
//  TotemScript
//
//  Created by Timothy Smale on 10/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>
#include <limits.h>

#if TOTEM_DEBUGOPT_PRINT_VM_ACTIVITY
#if TOTEM_VMOPT_GLOBAL_OPERANDS
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, base, state) totemExecState_PrintInstructionDetailed(state, base, base, ins, stdout)
#else
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, base, state) totemExecState_PrintInstructionDetailed(state, base, globals, ins, stdout)
#endif
#else
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, base, state)
#endif

#define TOTEM_VM_ERROR(state, status) \
    state->JmpNode->Status = totemExecStatus_Break(status); \
    call->ResumeAt = insPtr; \
    TOTEM_JMP_THROW(state->JmpNode->Buffer);

#define TOTEM_VM_ASSERT(x, state, status) if(!(x)) { TOTEM_VM_ERROR(state, status); }

#define TOTEM_VM_BREAK(x, state) \
    { \
        totemExecStatus status = (x); \
        if(status != totemExecStatus_Continue) \
        { \
            TOTEM_VM_ERROR(state, status); \
        } \
    }

#if TOTEM_VMOPT_GLOBAL_OPERANDS
#define TOTEM_VM_GET_A(base, instruction) (&base[TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)][(TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))])
#define TOTEM_VM_GET_B(base, instruction) (&base[TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)][(TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))])
#define TOTEM_VM_GET_C(base, instruction) (&base[TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)][(TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))])
#define TOTEM_VM_GET_GLOBAL(base, index) (&base[totemOperandType_GlobalRegister][index])

#define TOTEM_VM_RESET() \
    insPtr = call->ResumeAt; \
    base[totemOperandType_GlobalRegister] = state->GlobalRegisters; \
    base[totemOperandType_LocalRegister] = state->LocalRegisters; \

#else
#define TOTEM_VM_GET_A(base, instruction) (&base[(TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))])
#define TOTEM_VM_GET_B(base, instruction) (&base[(TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))])
#define TOTEM_VM_GET_C(base, instruction) (&base[(TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))])
#define TOTEM_VM_GET_GLOBAL(base, index) (&globals[index])

#define TOTEM_VM_RESET() \
    insPtr = call->ResumeAt; \
    base = state->LocalRegisters; \
    globals = state->GlobalRegisters; \

#endif

#define TOTEM_VM_PREDISPATCH() \
    ins = *insPtr; \
    TOTEM_INSTRUCTION_PRINT_DEBUG(ins, base, state); \
    op = TOTEM_INSTRUCTION_GET_OP(ins);

#define TOTEM_VM_DISPATCH_TARGET_LABEL(name) totem_vm_dispatch_target_##name
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL TOTEM_VM_DISPATCH_TARGET_LABEL(badop)
#define TOTEM_VM_DISPATCH_TABLE_ENTRY(val) [val] = &&TOTEM_VM_DISPATCH_TARGET_LABEL(val)

#if TOTEM_VMOPT_THREADED_DISPATCH
#define TOTEM_VM_DISPATCH() \
    TOTEM_VM_PREDISPATCH(); \
    goto *s_opcodes[op];

#define TOTEM_VM_LOOP() TOTEM_VM_DISPATCH()
#define TOTEM_VM_DISPATCH_TARGET(name) TOTEM_VM_DISPATCH_TARGET_LABEL(name):
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET() TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL:
#define TOTEM_OPCODE_FORMAT(x) TOTEM_VM_DISPATCH_TABLE_ENTRY(x),

#define TOTEM_VM_DEFINE_DISPATCH_TABLE() static const void *s_opcodes[UINT8_MAX + 1] = \
    { \
        [29 ... 255] = &&TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL, \
        TOTEM_EMIT_OPCODES() \
    }

#else
#define TOTEM_VM_DEFINE_DISPATCH_TABLE()

#if TOTEM_VMOPT_SIMULATED_THREADED_DISPATCH
#define TOTEM_OPCODE_FORMAT(x) case x: goto TOTEM_VM_DISPATCH_TARGET_LABEL(x);
#define TOTEM_VM_DISPATCH() \
    TOTEM_VM_PREDISPATCH(); \
    switch (op) \
	{ \
        TOTEM_EMIT_OPCODES() \
        default: TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch); \
	}

#define TOTEM_VM_LOOP() TOTEM_VM_DISPATCH();
#define TOTEM_VM_DISPATCH_TARGET(name) TOTEM_VM_DISPATCH_TARGET_LABEL(name):
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET()

#else
#define TOTEM_VM_DISPATCH() goto totem_vm_loop_entry;
#define TOTEM_VM_LOOP() \
    totem_vm_loop_entry: \
    TOTEM_VM_PREDISPATCH(); \
    switch (op)
#define TOTEM_VM_DISPATCH_TARGET(name) case name:
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET() default:
#endif
#endif

#if TOTEM_VMOPT_GLOBAL_OPERANDS
void totemExecState_PrintInstructionDetailed(totemExecState *state, totemRegister **base, totemRegister **globals, totemInstruction ins, FILE *file)
#else
void totemExecState_PrintInstructionDetailed(totemExecState *state, totemRegister *base, totemRegister *globals, totemInstruction ins, FILE *file)
#endif
{
    totemInstruction_Print(file, (ins));
    totemOperationType op = TOTEM_INSTRUCTION_GET_OP((ins));
    totemInstructionType type = totemOperationType_GetInstructionType(op);
    switch (type)
    {
        case totemInstructionType_Abcx:
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            fprintf(file, "a:");
            totemExecState_PrintRegister(state, file, a);
            fprintf(file, "b:");
            totemExecState_PrintRegister(state, file, b);
            break;
        }
            
        case totemInstructionType_Abc:
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            fprintf(file, "a:");
            totemExecState_PrintRegister(state, file, a);
            fprintf(file, "b:");
            totemExecState_PrintRegister(state, file, b);
            fprintf(file, "c:");
            totemExecState_PrintRegister(state, file, c);
            break;
        }
        case totemInstructionType_Abx:
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            fprintf(file, "local :");
            totemExecState_PrintRegister(state, file, a);
            
            if (op == totemOperationType_MoveToLocal || op == totemOperationType_MoveToGlobal)
            {
                a = TOTEM_VM_GET_GLOBAL(base, TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins));
                fprintf(file, "global:");
                totemExecState_PrintRegister(state, file, a);
            }
            
            break;
        }
        case totemInstructionType_Axx:
            break;
    }
    
    fprintf(file, "\n");
}

void totemExecState_ExecuteInstructions(totemExecState *state)
{
    totemInstruction ins;
    totemOperationType op;
    totemInstruction *insPtr;
    totemFunctionCall *call = state->CallStack;
    
#if TOTEM_VMOPT_GLOBAL_OPERANDS
    totemRegister *base[2];
#else
    totemRegister *base, *globals;
#endif
    
    TOTEM_VM_DEFINE_DISPATCH_TABLE();
    TOTEM_VM_RESET();
    TOTEM_VM_LOOP()
    {
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Move)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemExecState_Assign(state, a, b);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Add)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_Add(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Subtract)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_Subtract(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Multiply)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_Multiply(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Divide)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_Divide(state, a, b,c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Equals)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            totemExecState_AssignNewBoolean(state, a, totemRegister_Equals(b, c));
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_NotEquals)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            totemExecState_AssignNewBoolean(state, a, !totemRegister_Equals(b, c));
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LessThan)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_LessThan(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LessThanEquals)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_LessThanEquals(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoreThan)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_MoreThan(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoreThanEquals)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_MoreThanEquals(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalOr)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            totemExecState_AssignNewBoolean(state, a, totemRegister_IsNotZero(b) || totemRegister_IsNotZero(c));
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalAnd)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            totemExecState_AssignNewBoolean(state, a, totemRegister_IsNotZero(b) && totemRegister_IsNotZero(c));
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalNegate)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemExecState_AssignNewBoolean(state, a, totemRegister_IsZero(b));
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ConditionalGoto)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            
            if (totemRegister_IsNotZero(a))
            {
                insPtr++;
            }
            else
            {
                totemOperandXSigned bx = TOTEM_INSTRUCTION_GET_BX_SIGNED(ins);
                insPtr += bx;
            }
            
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Goto)
        {
            totemOperandXSigned ax = TOTEM_INSTRUCTION_GET_AX_SIGNED(ins);
            insPtr += ax;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Return)
        {
            call->ResumeAt = insPtr;
            
            if (!call->Prev)
            {
                return;
            }
            else
            {
                totemExecState_Assign(state, call->ReturnRegister, TOTEM_VM_GET_A(base, ins));
                totemExecState_PopRoutine(state);
                call = state->CallStack;
                TOTEM_VM_RESET();
                TOTEM_VM_DISPATCH();
            }
            
            // never get here
            TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch);
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_NewArray)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemGCObject *gc;
            
            TOTEM_VM_ASSERT(totemRegister_IsInt(b), state, totemExecStatus_UnexpectedDataType);
            TOTEM_VM_BREAK(totemExecState_CreateArray(state, totemRegister_GetInt(b), &gc), state);
            totemExecState_AssignNewArray(state, a, gc);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexGet)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            totemGCObject *gc;
            
            if (totemRegister_IsString(b))
            {
                TOTEM_VM_ASSERT(totemRegister_IsInt(c), state, totemExecStatus_InvalidKey);
                TOTEM_VM_BREAK(totemExecState_InternStringChar(state, b, totemRegister_GetInt(c), a), state);
            }
            else if (totemRegister_IsArray(b))
            {
                gc = totemRegister_GetGCObject(b);
                TOTEM_VM_ASSERT(totemRegister_IsInt(c), state, totemExecStatus_InvalidKey);
                TOTEM_VM_BREAK(totemExecState_ArrayGet(state, gc->Registers, gc->NumRegisters, totemRegister_GetInt(c), a), state);
            }
            else if (totemRegister_IsObject(b))
            {
                gc = totemRegister_GetGCObject(b);
                TOTEM_VM_BREAK(totemExecState_ObjectGet(state, gc, c, a), state);
            }
            else
            {
                TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexSet)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            totemGCObject *gc;
            
            if (totemRegister_IsArray(a))
            {
                gc = totemRegister_GetGCObject(a);
                TOTEM_VM_ASSERT(totemRegister_IsInt(b), state, totemExecStatus_InvalidKey);
                TOTEM_VM_BREAK(totemExecState_ArraySet(state, gc->Registers, gc->NumRegisters, totemRegister_GetInt(b), c), state);
            }
            else if (totemRegister_IsObject(a))
            {
                gc = totemRegister_GetGCObject(a);
                TOTEM_VM_BREAK(totemExecState_ObjectSet(state, gc, b, c), state);
            }
            else
            {
                TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            totemExecState_WriteBarrier(state, gc);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexShift)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            
            if (totemRegister_IsArray(b))
            {
                totemGCObject *gc = totemRegister_GetGCObject(b);
                TOTEM_VM_ASSERT(totemRegister_IsInt(c), state, totemExecStatus_InvalidKey);
                TOTEM_VM_BREAK(totemExecState_ArrayShift(state, gc->Registers, gc->NumRegisters, totemRegister_GetInt(c), a), state);
            }
            else if (totemRegister_IsObject(b))
            {
                TOTEM_VM_BREAK(totemExecState_ObjectShift(state, totemRegister_GetGCObject(b), c, a), state);
            }
            else
            {
                TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoveToGlobal)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemOperandXUnsigned bx = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins);
            totemRegister *b = TOTEM_VM_GET_GLOBAL(base, bx);
            totemExecState_Assign(state, b, a);
            insPtr++;
#ifndef TOTEM_VMOPT_GLOBAL_OPERANDS
            totemExecState_WriteBarrier(state, call->Instance);
#endif
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoveToLocal)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemOperandXUnsigned bx = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins);
            totemRegister *b = TOTEM_VM_GET_GLOBAL(base, bx);
            totemExecState_Assign(state, a, b);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Is)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            
            totemPrivateDataType bType = totemRegister_GetType(b);
            
            if (!totemRegister_IsTypeValue(c))
            {
                totemExecState_AssignNewBoolean(state, a, totemPrivateDataType_ToPublic(bType) == totemPrivateDataType_ToPublic(totemRegister_GetType(c)));
            }
            else
            {
                totemExecState_AssignNewBoolean(state, a, totemPrivateDataType_ToPublic(bType) == totemRegister_GetTypeValue(c));
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_As)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            TOTEM_VM_BREAK(totemExecState_Cast(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_PreInvoke)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemOperandXUnsigned xu = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins);
            
            if (totemRegister_IsNativeFunction(a))
            {
                TOTEM_VM_BREAK(totemExecState_CreateSubroutine(
                                                               state,
                                                               xu,
                                                               call->Instance,
                                                               NULL,
                                                               totemFunctionType_Native,
                                                               totemRegister_GetNativeFunction(a),
                                                               &call), state);
                totemExecState_PushRoutine(state, call, NULL);
            }
            else if (totemRegister_IsInstanceFunction(a))
            {
                totemInstanceFunction *func = totemRegister_GetInstanceFunction(a);
                TOTEM_VM_BREAK(totemExecState_CreateSubroutine(
                                                               state,
                                                               xu <= func->Function->RegistersNeeded ? func->Function->RegistersNeeded : xu,
                                                               func->Instance,
                                                               NULL,
                                                               totemFunctionType_Script,
                                                               func,
                                                               &call), state);
                
                totemExecState_PushRoutine(state, call, func->Function->InstructionsStart);
            }
            else if (totemRegister_IsCoroutine(a))
            {
                totemGCObject *gc = totemRegister_GetGCObject(a);
                TOTEM_VM_ASSERT(xu <= gc->Coroutine->NumRegisters, state, totemExecStatus_RegisterOverflow);
                
                call = gc->Coroutine;
                call->NumArguments = 0;
                call->ReturnRegister = a;
                
                totemExecState_PushRoutine(state, call, call->ResumeAt ? call->ResumeAt : call->InstanceFunction->Function->InstructionsStart);
            }
            else
            {
                TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_FunctionArg)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemExecState_Assign(state, &call->FrameStart[call->NumArguments++], a);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Invoke)
        {
            call->ReturnRegister = TOTEM_VM_GET_A(base, ins);
            call->Prev->ResumeAt = ++insPtr;
            
            switch (call->Type)
            {
                case totemFunctionType_Native:
                    TOTEM_VM_BREAK(call->NativeFunction->Callback(state), state);
                    totemExecState_PopRoutine(state);
                    call = state->CallStack;
                    TOTEM_VM_DISPATCH();
                    
                case totemFunctionType_Script:
                    TOTEM_VM_RESET();
                    TOTEM_VM_DISPATCH();
            }
            
            // never get here
            TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch);
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_NewObject)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemGCObject *gc;
            
            TOTEM_VM_ASSERT(totemRegister_IsInt(b), state, totemExecStatus_UnexpectedDataType);
            
            TOTEM_VM_BREAK(totemExecState_CreateObject(state, totemRegister_GetInt(b), &gc), state);
            totemExecState_AssignNewObject(state, a, gc);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_DEFAULT_TARGET()
        {
            TOTEM_VM_ERROR(state, totemExecStatus_UnrecognisedOperation);
        }
    }
    
    // should never reach this
    TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch);
}

#undef TOTEM_OPCODE_FORMAT