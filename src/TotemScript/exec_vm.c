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

#if 0
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
#define TOTEM_VM_LOOP() TOTEM_VM_DISPATCH()
#define TOTEM_VM_DISPATCH_TARGET(name) TOTEM_VM_DISPATCH_TARGET_LABEL(name):
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET() TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL:
#define TOTEM_OPCODE_FORMAT(x) TOTEM_VM_DISPATCH_TABLE_ENTRY(x),

#define TOTEM_VM_DEFINE_DISPATCH_TABLE() static const void *opcodes[256] = \
    { \
        [29 ... 255] = &&TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL, \
        TOTEM_EMIT_OPCODES() \
    }

#define TOTEM_VM_DISPATCH() \
    TOTEM_VM_PREDISPATCH(); \
    goto *opcodes[op];

#else
#define TOTEM_VM_DEFINE_DISPATCH_TABLE()

#if TOTEM_VMOPT_SIMULATED_THREADED_DISPATCH
#define TOTEM_VM_LOOP() TOTEM_VM_DISPATCH();
#define TOTEM_VM_DISPATCH_TARGET(name) TOTEM_VM_DISPATCH_TARGET_LABEL(name):
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET()
#define TOTEM_OPCODE_FORMAT(x) \
    case x: \
    goto TOTEM_VM_DISPATCH_TARGET_LABEL(x);

#define TOTEM_VM_DISPATCH() \
    TOTEM_VM_PREDISPATCH(); \
    switch (op) \
    { \
        TOTEM_EMIT_OPCODES() \
        default: TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch); \
    }

#else
#define TOTEM_VM_LOOP() \
    totem_vm_loop_entry: \
    TOTEM_VM_PREDISPATCH(); \
    switch (op)
#define TOTEM_VM_DISPATCH_TARGET(name) case name:
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET() default:
#define TOTEM_VM_DISPATCH() goto totem_vm_loop_entry;
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
            totemExecState_Assign(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins));
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Add)
        {
            TOTEM_VM_BREAK(totemExecState_Add(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Subtract)
        {
            TOTEM_VM_BREAK(totemExecState_Subtract(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Multiply)
        {
            TOTEM_VM_BREAK(totemExecState_Multiply(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Divide)
        {
            TOTEM_VM_BREAK(totemExecState_Divide(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Equals)
        {
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            
            totemExecState_AssignNewBoolean(state, TOTEM_VM_GET_A(base, ins), b->Value.Data == c->Value.Data && b->DataType == c->DataType);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_NotEquals)
        {
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            
            totemExecState_AssignNewBoolean(state, TOTEM_VM_GET_A(base, ins), b->Value.Data != c->Value.Data || b->DataType != c->DataType);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LessThan)
        {
            TOTEM_VM_BREAK(totemExecState_LessThan(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LessThanEquals)
        {
            TOTEM_VM_BREAK(totemExecState_LessThanEquals(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoreThan)
        {
            TOTEM_VM_BREAK(totemExecState_MoreThan(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoreThanEquals)
        {
            TOTEM_VM_BREAK(totemExecState_MoreThanEquals(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalOr)
        {
            totemExecState_AssignNewBoolean(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins)->Value.Data || TOTEM_VM_GET_C(base, ins)->Value.Data);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalAnd)
        {
            totemExecState_AssignNewBoolean(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins)->Value.Data && TOTEM_VM_GET_C(base, ins)->Value.Data);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalNegate)
        {
            totemExecState_AssignNewBoolean(state, TOTEM_VM_GET_A(base, ins), !TOTEM_VM_GET_B(base, ins)->Value.Data);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ConditionalGoto)
        {
            insPtr += TOTEM_VM_GET_A(base, ins)->Value.Data ? 1 : TOTEM_INSTRUCTION_GET_BX_SIGNED(ins);
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Goto)
        {
            insPtr += TOTEM_INSTRUCTION_GET_AX_SIGNED(ins);
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
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemGCObject *gc;
            
            TOTEM_VM_ASSERT(b->DataType == totemPrivateDataType_Int, state, totemExecStatus_UnexpectedDataType);
            
            TOTEM_VM_BREAK(totemExecState_CreateArray(state, b->Value.Int, &gc), state);
            totemExecState_AssignNewArray(state, TOTEM_VM_GET_A(base, ins), gc);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexGet)
        {
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            
            switch (b->DataType)
            {
                case totemPrivateDataType_InternedString:
                case totemPrivateDataType_MiniString:
                {
                    totemRegister *c = TOTEM_VM_GET_C(base, ins);
                    TOTEM_VM_ASSERT(c->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_BREAK(totemExecState_InternStringChar(state, b, c->Value.Int, TOTEM_VM_GET_A(base, ins)), state);
                    break;
                }
                    
                case totemPrivateDataType_Array:
                {
                    totemRegister *c = TOTEM_VM_GET_C(base, ins);
                    TOTEM_VM_ASSERT(c->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_BREAK(totemExecState_ArrayGet(state, b->Value.GCObject->Registers, b->Value.GCObject->NumRegisters, c->Value.Int, TOTEM_VM_GET_A(base, ins)), state);
                    break;
                }
                    
                case totemPrivateDataType_Object:
                    TOTEM_VM_BREAK(totemExecState_ObjectGet(state, b->Value.GCObject, TOTEM_VM_GET_C(base, ins), TOTEM_VM_GET_A(base, ins)), state);
                    break;
                    
                default:
                    TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexSet)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            
            switch (a->DataType)
            {
                case totemPrivateDataType_Array:
                {
                    totemRegister *b = TOTEM_VM_GET_B(base, ins);
                    TOTEM_VM_ASSERT(b->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_BREAK(totemExecState_ArraySet(state, a->Value.GCObject->Registers, a->Value.GCObject->NumRegisters, b->Value.Int, TOTEM_VM_GET_C(base, ins)), state);
                    break;
                }
                    
                case totemPrivateDataType_Object:
                    TOTEM_VM_BREAK(totemExecState_ObjectSet(state, a->Value.GCObject, TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
                    break;
                    
                default:
                    TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            totemExecState_WriteBarrier(state, a->Value.GCObject);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexShift)
        {
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            
            switch (b->DataType)
            {
                case totemPrivateDataType_Array:
                {
                    totemRegister *c = TOTEM_VM_GET_C(base, ins);
                    TOTEM_VM_ASSERT(c->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_BREAK(totemExecState_ArrayShift(state, b->Value.GCObject->Registers, b->Value.GCObject->NumRegisters, c->Value.Int, TOTEM_VM_GET_A(base, ins)), state);
                    break;
                }
                    
                case totemPrivateDataType_Object:
                    TOTEM_VM_BREAK(totemExecState_ObjectShift(state, b->Value.GCObject, TOTEM_VM_GET_C(base, ins), TOTEM_VM_GET_A(base, ins)), state);
                    break;
                    
                default:
                    TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoveToGlobal)
        {
            totemExecState_Assign(state, TOTEM_VM_GET_GLOBAL(base, TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins)), TOTEM_VM_GET_A(base, ins));
            insPtr++;
#ifndef TOTEM_VMOPT_GLOBAL_OPERANDS
            totemExecState_WriteBarrier(state, call->Instance);
#endif
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoveToLocal)
        {
            totemExecState_Assign(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_GLOBAL(base, TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins)));
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Is)
        {
            totemRegister *c = TOTEM_VM_GET_C(base, ins);
            
            if (c->DataType != totemPrivateDataType_Type)
            {
                totemExecState_AssignNewBoolean(state, TOTEM_VM_GET_A(base, ins), totemPrivateDataType_ToPublic(TOTEM_VM_GET_B(base, ins)->DataType) == totemPrivateDataType_ToPublic(c->DataType));
            }
            else
            {
                totemExecState_AssignNewBoolean(state, TOTEM_VM_GET_A(base, ins), totemPrivateDataType_ToPublic(TOTEM_VM_GET_B(base, ins)->DataType) == c->Value.DataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_As)
        {
            TOTEM_VM_BREAK(totemExecState_Cast(state, TOTEM_VM_GET_A(base, ins), TOTEM_VM_GET_B(base, ins), TOTEM_VM_GET_C(base, ins)), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_PreInvoke)
        {
            totemRegister *a = TOTEM_VM_GET_A(base, ins);
            totemOperandXUnsigned xu = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins);
            
            switch (a->DataType)
            {
                case totemPrivateDataType_NativeFunction:
                    TOTEM_VM_BREAK(totemExecState_CreateSubroutine(
                                                                   state,
                                                                   xu,
                                                                   call->Instance,
                                                                   NULL,
                                                                   totemFunctionType_Native,
                                                                   a->Value.NativeFunction,
                                                                   &call), state);
                    totemExecState_PushRoutine(state, call, NULL);
                    break;
                    
                case totemPrivateDataType_InstanceFunction:
                    TOTEM_VM_BREAK(totemExecState_CreateSubroutine(
                                                                   state,
                                                                   xu <= a->Value.InstanceFunction->Function->RegistersNeeded ? a->Value.InstanceFunction->Function->RegistersNeeded : xu,
                                                                   a->Value.InstanceFunction->Instance,
                                                                   NULL,
                                                                   totemFunctionType_Script,
                                                                   a->Value.InstanceFunction,
                                                                   &call), state);
                    
                    totemExecState_PushRoutine(state, call, a->Value.InstanceFunction->Function->InstructionsStart);
                    break;
                    
                case totemPrivateDataType_Coroutine:
                    TOTEM_VM_ASSERT(xu <= a->Value.GCObject->Coroutine->NumRegisters, state, totemExecStatus_RegisterOverflow);
                    
                    call = a->Value.GCObject->Coroutine;
                    call->NumArguments = 0;
                    call->ReturnRegister = a;
                    
                    totemExecState_PushRoutine(state, call, call->ResumeAt ? call->ResumeAt : call->InstanceFunction->Function->InstructionsStart);
                    break;
                    
                default:
                    TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_FunctionArg)
        {
            totemExecState_Assign(state, &call->FrameStart[call->NumArguments++], TOTEM_VM_GET_A(base, ins));
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
            totemRegister *b = TOTEM_VM_GET_B(base, ins);
            totemGCObject *gc;
            
            TOTEM_VM_ASSERT(b->DataType == totemPrivateDataType_Int, state, totemExecStatus_UnexpectedDataType);
            
            TOTEM_VM_BREAK(totemExecState_CreateObject(state, b->Value.Int, &gc), state);
            totemExecState_AssignNewObject(state, TOTEM_VM_GET_A(base, ins), gc);
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