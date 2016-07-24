//
//  exec_vm.c
//  TotemScript
//
//  Created by Timothy Smale on 10/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

/*
 ideal:
 - compile-time type-hinting
	- can have instructions tailored for specific type pairs, cutting down on type-checking costs at runtime
 - have it be possible for each "dispatch target" to be interpreted as a function definition
	- signature tailored to its instruction type (abc, abcx etc.)
	- we can isolate the code for each instruction type in its own callable function
	- don't have vm call them, though - inline it instead
 - generate platform-specific ASM
	- completely eliminate instruction parsing
	- layout:
 - have a table of gotos to each instruction
 - on entry, check instruction offset is within bounds
 - if so, lookup goto
 - otherwise goto error
 - on-return, we can just store the precomputed offset
 - coroutines would use this table to resume
 - gotos wouldn't need to use it however - the destination address can be precomputed
 - for each instruction, generate an equivelant call to the predefined dispatch target, e.g. equivelant C code for an assign might look like:
 a = localBase + aOffset
 b = globalBase + bOffset
 totemOperationType_Move_Dispatch(state, a, b);
 */

#if 0
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, base, state) totemExecState_PrintInstructionDetailed(state, base, ins, stdout)
#else
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, base, state)
#endif

#define TOTEM_VM_PREDISPATCH() \
    ins = *insPtr; \
    TOTEM_INSTRUCTION_PRINT_DEBUG(ins, base, state); \
    op = TOTEM_INSTRUCTION_GET_OP(ins);

#define TOTEM_VM_DISPATCH_TARGET_LABEL(name) vm_dispatch_target_##name
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL TOTEM_VM_DISPATCH_TARGET_LABEL(badop)
#define TOTEM_VM_DISPATCH_TABLE_ENTRY(val) [val] = &&TOTEM_VM_DISPATCH_TARGET_LABEL(val)

#if TOTEM_VMOPT_THREADED_DISPATCH
#define TOTEM_VM_DEFINE_DISPATCH_TABLE() static const void *opcodes[256] = \
{ \
    [29 ... 255] = &&TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL, \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Move), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Add), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Subtract), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Multiply), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Divide), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Equals), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_NotEquals), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_LessThan), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_LessThanEquals), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_MoreThan), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_MoreThanEquals), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_LogicalOr), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_LogicalAnd), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_ConditionalGoto), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Goto), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_FunctionArg), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Return), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_NewArray), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_ComplexGet), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_ComplexSet), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_MoveToLocal), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_MoveToGlobal), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Is), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_As), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_Invoke), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_NewObject), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_ComplexShift), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_PreInvoke), \
    TOTEM_VM_DISPATCH_TABLE_ENTRY(totemOperationType_LogicalNegate), \
}

#define TOTEM_VM_DISPATCH_TARGET(name) TOTEM_VM_DISPATCH_TARGET_LABEL(name):
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET() TOTEM_VM_DISPATCH_DEFAULT_TARGET_LABEL:

#define TOTEM_VM_DISPATCH() \
    TOTEM_VM_PREDISPATCH(); \
    goto *opcodes[op];

#define TOTEM_VM_DISPATCH_START() \
    TOTEM_VM_DEFINE_DISPATCH_TABLE(); \
    TOTEM_VM_DISPATCH();

#define TOTEM_VM_DISPATCH_END()

#else

#define TOTEM_VM_DISPATCH_TARGET(name) case name:
#define TOTEM_VM_DISPATCH_DEFAULT_TARGET() default:
#define TOTEM_VM_DISPATCH() continue
#define TOTEM_VM_DISPATCH_START() \
    for(;;) \
    { \
        TOTEM_VM_PREDISPATCH(); \
        switch(op)
#define TOTEM_VM_DISPATCH_END() \
    }

#endif

#if TOTEM_VMOPT_GLOBAL_OPERANDS
#define TOTEM_VM_GET_OPERANDA(base, instruction) &base[TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)][(TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))]
#define TOTEM_VM_GET_OPERANDB(base, instruction) &base[TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)][(TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))]
#define TOTEM_VM_GET_OPERANDC(base, instruction) &base[TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)][(TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))]
#define TOTEM_VM_GET_GLOBAL(base, index) &base[totemOperandType_GlobalRegister][index]
#else
#define TOTEM_VM_GET_OPERANDA(base, instruction) &base[(TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))]
#define TOTEM_VM_GET_OPERANDB(base, instruction) &base[(TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))]
#define TOTEM_VM_GET_OPERANDC(base, instruction) &base[(TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))]
#define TOTEM_VM_GET_GLOBAL(base, index) &globals[index]
#endif

#define TOTEM_VM_ERROR(state, status) \
    state->JmpNode->Status = totemExecStatus_Break(status); \
    goto totem_vm_error; \

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
void totemExecState_PrintInstructionDetailed(totemExecState *state, totemRegister **base, totemInstruction ins, FILE *file)
#else
void totemExecState_PrintInstructionDetailed(totemExecState *state, totemRegister *base, totemInstruction ins, FILE *file)
#endif
{
    totemInstruction_Print(file, (ins));
    totemOperationType op = TOTEM_INSTRUCTION_GET_OP((ins));
    
    totemInstructionType type = totemOperationType_GetInstructionType(op);
    switch (type)
    {
        case totemInstructionType_Abcx:
        {
            totemRegister *a = TOTEM_VM_GET_OPERANDA(base, ins);
            totemRegister *b = TOTEM_VM_GET_OPERANDB(base, ins);
            fprintf(file, "a:");
            totemExecState_PrintRegister(state, file, a);
            fprintf(file, "b:");
            totemExecState_PrintRegister(state, file, b);
            break;
        }
            
        case totemInstructionType_Abc:
        {
            totemRegister *a = TOTEM_VM_GET_OPERANDA(base, ins);
            totemRegister *b = TOTEM_VM_GET_OPERANDB(base, ins);
            totemRegister *c = TOTEM_VM_GET_OPERANDC(base, ins);
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
            totemRegister *a = TOTEM_VM_GET_OPERANDA(base, ins);
            totemExecState_PrintRegister(state, file, a);
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
    totemRegister *a, *b, *c;
    totemOperandXUnsigned xu;
    totemOperandXSigned xs;
    totemGCObject *gc;
    totemInstruction *insPtr;
    size_t numCalls = 0;
    totemFunctionCall *call = state->CallStack;
    
totem_vm_reset:
    
    insPtr = call->ResumeAt;
    
#if TOTEM_VMOPT_GLOBAL_OPERANDS
    totemRegister *base[2];
    base[totemOperandType_GlobalRegister] = state->GlobalRegisters;
    base[totemOperandType_LocalRegister] = state->LocalRegisters;
#else
    totemRegister *base, *globals;
    base = state->LocalRegisters;
    globals = state->GlobalRegisters;
#endif
    
    TOTEM_VM_DISPATCH_START()
    {
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Move)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            
            totemExecState_Assign(state, a, b);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Add)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_Add(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Subtract)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_Subtract(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Multiply)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_Multiply(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Divide)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_Divide(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Equals)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            totemExecState_AssignNewBoolean(state, a, b->Value.Data == c->Value.Data && b->DataType == c->DataType);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_NotEquals)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            totemExecState_AssignNewBoolean(state, a, b->Value.Data != c->Value.Data || b->DataType != c->DataType);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LessThan)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_LessThan(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LessThanEquals)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_LessThanEquals(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoreThan)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_MoreThan(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoreThanEquals)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_MoreThanEquals(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalOr)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            totemExecState_AssignNewBoolean(state, a, b->Value.Data || c->Value.Data);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalAnd)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            totemExecState_AssignNewBoolean(state, a, b->Value.Data && c->Value.Data);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_LogicalNegate)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            
            totemExecState_AssignNewBoolean(state, a, !b->Value.Data);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ConditionalGoto)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            xs = TOTEM_INSTRUCTION_GET_BX_SIGNED(ins);
            insPtr += a->Value.Data ? 1 : xs;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Goto)
        {
            xs = TOTEM_INSTRUCTION_GET_AX_SIGNED(ins);
            insPtr += xs;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Return)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            call->ResumeAt = insPtr;
            
            totemExecState_Assign(state, call->ReturnRegister, a);
            
            if (numCalls == 0)
            {
                return;
            }
            else
            {
                totemExecState_PopRoutine(state);
                call = state->CallStack;
                numCalls--;
                goto totem_vm_reset;
            }
            
            // never get here
            TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch);
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_NewArray)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            
            TOTEM_VM_ASSERT(b->DataType == totemPrivateDataType_Int, state, totemExecStatus_UnexpectedDataType);
            TOTEM_VM_ASSERT(b->Value.Int >= 0 && b->Value.Int < UINT32_MAX, state, totemExecStatus_IndexOutOfBounds);
            
            TOTEM_VM_BREAK(totemExecState_CreateArray(state, (uint32_t)b->Value.Int, &gc), state);
            totemExecState_AssignNewArray(state, a, gc);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexGet)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            switch (b->DataType)
            {
                case totemPrivateDataType_InternedString:
                case totemPrivateDataType_MiniString:
                    TOTEM_VM_ASSERT(c->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_ASSERT(c->Value.Int >= 0 && c->Value.Int < UINT32_MAX, state, totemExecStatus_IndexOutOfBounds);
                    TOTEM_VM_BREAK(totemExecState_InternStringChar(state, b, (totemStringLength)c->Value.Int, a), state);
                    break;
                    
                case totemPrivateDataType_Array:
                    TOTEM_VM_ASSERT(c->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_ASSERT(c->Value.Int >= 0 && c->Value.Int < UINT32_MAX, state, totemExecStatus_IndexOutOfBounds);
                    TOTEM_VM_BREAK(totemExecState_ArrayGet(state, b->Value.GCObject->Array, (uint32_t)c->Value.Int, a), state);
                    break;
                    
                case totemPrivateDataType_Object:
                    TOTEM_VM_BREAK(totemExecState_ObjectGet(state, b->Value.GCObject->Object, c, a), state);
                    break;
                    
                default:
                    TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexSet)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            switch (a->DataType)
            {
                case totemPrivateDataType_Array:
                    TOTEM_VM_ASSERT(b->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_ASSERT(b->Value.Int >= 0 && b->Value.Int < UINT32_MAX, state, totemExecStatus_IndexOutOfBounds);
                    TOTEM_VM_BREAK(totemExecState_ArraySet(state, a->Value.GCObject->Array, (uint32_t)b->Value.Int, c), state);
                    break;
                    
                case totemPrivateDataType_Object:
                    TOTEM_VM_BREAK(totemExecState_ObjectSet(state, a->Value.GCObject->Object, b, c), state);
                    break;
                    
                default:
                    TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_ComplexShift)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            switch (b->DataType)
            {
                case totemPrivateDataType_Array:
                    TOTEM_VM_ASSERT(c->DataType == totemPrivateDataType_Int, state, totemExecStatus_InvalidKey);
                    TOTEM_VM_ASSERT(c->Value.Int >= 0 && c->Value.Int < UINT32_MAX, state, totemExecStatus_IndexOutOfBounds);
                    TOTEM_VM_BREAK(totemExecState_ArrayShift(state, b->Value.GCObject->Array, (uint32_t)c->Value.Int, a), state);
                    break;
                    
                case totemPrivateDataType_Object:
                    TOTEM_VM_BREAK(totemExecState_ObjectShift(state, b->Value.GCObject->Object, c, a), state);
                    break;
                    
                default:
                    TOTEM_VM_ERROR(state, totemExecStatus_UnexpectedDataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoveToGlobal)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            xu = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins);
            b = TOTEM_VM_GET_GLOBAL(base, xu);
            totemExecState_Assign(state, b, a);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_MoveToLocal)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            xu = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins);
            b = TOTEM_VM_GET_GLOBAL(base, xu);
            totemExecState_Assign(state, a, b);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Is)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            if (c->DataType != totemPrivateDataType_Type)
            {
                totemExecState_AssignNewBoolean(state, a, totemPrivateDataType_ToPublic(b->DataType) == totemPrivateDataType_ToPublic(c->DataType));
            }
            else
            {
                totemExecState_AssignNewBoolean(state, a, totemPrivateDataType_ToPublic(b->DataType) == c->Value.DataType);
            }
            
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_As)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_Cast(state, a, b, c), state);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_PreInvoke)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            xu = TOTEM_INSTRUCTION_GET_CX_UNSIGNED(ins);
            
            switch (b->DataType)
            {
                case totemPrivateDataType_NativeFunction:
                    TOTEM_VM_BREAK(totemExecState_CreateSubroutine(
                                                                   state,
                                                                   xu,
                                                                   call->CurrentInstance,
                                                                   a,
                                                                   totemFunctionType_Native,
                                                                   b->Value.NativeFunction,
                                                                   &call), state);
                    totemExecState_PushRoutine(state, call, NULL);
                    break;
                    
                case totemPrivateDataType_InstanceFunction:
                    TOTEM_VM_BREAK(totemExecState_CreateSubroutine(
                                                                   state,
                                                                   xu <= b->Value.InstanceFunction->Function->RegistersNeeded ? b->Value.InstanceFunction->Function->RegistersNeeded : xu,
                                                                   b->Value.InstanceFunction->Instance,
                                                                   a,
                                                                   totemFunctionType_Script,
                                                                   b->Value.InstanceFunction,
                                                                   &call), state);
                    
                    totemExecState_PushRoutine(state, call, b->Value.InstanceFunction->Function->InstructionsStart);
                    break;
                    
                case totemPrivateDataType_Coroutine:
                    TOTEM_VM_ASSERT(xu <= call->NumRegisters, state, totemExecStatus_RegisterOverflow);
                    
                    call = b->Value.GCObject->Coroutine;
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
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            totemExecState_Assign(state, &call->FrameStart[call->NumArguments++], a);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_Invoke)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            b = TOTEM_VM_GET_OPERANDB(base, ins);
            c = TOTEM_VM_GET_OPERANDC(base, ins);
            call->Prev->ResumeAt = ++insPtr;
            
            switch (call->Type)
            {
                case totemFunctionType_Native:
                    TOTEM_VM_BREAK(call->NativeFunction->Callback(state), state);
                    totemExecState_PopRoutine(state);
                    call = state->CallStack;
                    TOTEM_VM_DISPATCH();
                    break;
                    
                case totemFunctionType_Script:
                    numCalls++;
                    goto totem_vm_reset;
            }
            
            // never get here
            TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch);
        }
        
        TOTEM_VM_DISPATCH_TARGET(totemOperationType_NewObject)
        {
            a = TOTEM_VM_GET_OPERANDA(base, ins);
            
            TOTEM_VM_BREAK(totemExecState_CreateObject(state, &gc), state);
            totemExecState_AssignNewObject(state, a, gc);
            insPtr++;
            TOTEM_VM_DISPATCH();
        }
        
        TOTEM_VM_DISPATCH_DEFAULT_TARGET()
        {
            TOTEM_VM_ERROR(state, totemExecStatus_UnrecognisedOperation);
        }
    }
    TOTEM_VM_DISPATCH_END();
    
    // should never reach this
    TOTEM_VM_ERROR(state, totemExecStatus_InvalidDispatch);
    
totem_vm_error:
    call->ResumeAt = insPtr;
    TOTEM_JMP_THROW(state->JmpNode->Buffer);
}