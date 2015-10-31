//
//  exec.cpp
//  TotemScript
//
//  Created by Timothy Smale on 17/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <stdarg.h>
#include <math.h>

#define COL_SCRIPT_GET_OPERANDA_REGISTER(s, i) &s.m_registers[i.m_abc.m_operandAType][i.m_abc.m_operandAIndex]
#define COL_SCRIPT_GET_OPERANDB_REGISTER(s, i) &s.m_registers[i.m_abc.m_operandBType][i.m_abc.m_operandBIndex]
#define COL_SCRIPT_GET_OPERANDC_REGISTER(s, i) &s.m_registers[i.m_abc.m_operandCType][i.m_abc.m_operandCIndex]

bool col::script::getClass(size_t handle, col::script::Class *&classDefOut) {
    classDefOut = s_classes.find(handle);
    return classDefOut != NULL;
}

bool col::script::registerScript(col::memory::ConstantString &&name, col::script::Script &script, size_t &handleOut) {
    size_t *existingHandle = s_scriptNames.find(name);
    if(existingHandle == NULL) {
        handleOut = s_scripts.insert(script);
        s_scriptNames[name] = handleOut;
        
    } else {
        handleOut = *existingHandle;
        s_scripts[handleOut] = script;
    }
    
    // functions
    for(size_t i = 0; i < script.m_functionAddresses.length(); ++i) {
        col::memory::DynamicString *functionName = &script.m_functionNames[i];
        col::memory::ConstantString functionNameStr(functionName->cstr(), functionName->length());
        if(s_scriptFunctionNames.find(functionNameStr) != NULL) {
            COL_LOG_ERROR("Script function %s already exists, cannot register script", functionNameStr.cstr());
            return false;
        }
        
        size_t functionHandle = s_scriptFunctions.secure();
        col::script::ScriptFunction *function = &s_scriptFunctions[functionHandle];
        function->m_functionIndex = script.m_functionAddresses[i];
        function->m_scriptHandle = handleOut;
        
        s_scriptFunctionNames[functionNameStr] = functionHandle;
    }
    
    return true;
}

bool col::script::getScriptAddress(col::memory::ConstantString &name, size_t &addressOut) {
    size_t *result = s_scriptNames.find(name);
    if(result == NULL) {
        COL_LOG_ERROR("Could not find script address for \"%s\"", name.cstr());
        return false;
    }

    addressOut = *result;
    return true;
}

bool col::script::getScriptFunction(size_t address, col::script::ScriptFunction *&out) {
    out = s_scriptFunctions.find(address);
    if(out == NULL) {
        COL_LOG_ERROR("Could not find script with address %zu", address);
        return false;
    }
    
    return true;
}

bool col::script::getScriptFunctionAddress(col::memory::ConstantString &name, size_t &addressOut) {
    size_t *result = s_scriptFunctionNames.find(name);
    if(result == NULL) {
        COL_LOG_ERROR("Could not find script function address for \"%s\"", name.cstr());
        return false;
    }
    
    addressOut = *result;
    return true;
}

bool col::script::createState(size_t scriptHandle, col::script::State &stateOut) {
    col::script::Script *script = s_scripts.find(scriptHandle);
    if(script == NULL) {
        COL_LOG_ERROR("Could not find script with handle %zu", scriptHandle);
        return false;
    }

    stateOut.m_globalStack = script->m_globalStack; // copy global stack
    stateOut.m_scriptHandle = scriptHandle;
    stateOut.m_registers[OPERANDTYPE_LOCALREGISTER].reset();
    stateOut.m_registers[OPERANDTYPE_LOCALREGISTER].secure(COL_SCRIPT_MAX_REGISTERS);
    stateOut.m_registers[OPERANDTYPE_GLOBALREGISTER] = script->m_globalRegisters; // copy initial global registers
    return true;
}

bool col::script::registerNativeFunction(col::script::NativeFunction func, col::memory::ConstantString &&name, size_t &addressOut) {
    size_t *existingHandle = s_nativeFunctionNames.find(name);
    if(existingHandle == NULL) {
        addressOut = s_nativeFunctions.insert(func);
        s_nativeFunctionNames[name] = addressOut;
        
    } else {
        addressOut = *existingHandle;
        s_nativeFunctions[addressOut] = func;
    }
    
    return true;
}

COL_SCRIPT_STATUS col::script::exec(size_t handle, size_t functionIndex, col::script::State &state, col::script::Register *returnRegister) {
    col::script::Script *script = &s_scripts[handle];
    
    if(script->m_functionAddresses.length() < functionIndex) {
    	COL_LOG_ERROR("script %zu does not have function at index %zu", handle, functionIndex);
    	return COL_SCRIPT_STOP;
    }
    
    col::script::FunctionCall functionCall;
    functionCall.m_type = FUNCTIONTYPE_SCRIPT;
    functionCall.m_handle = functionIndex;
    
    if(returnRegister == NULL) {
        functionCall.m_returnRegister = &state.m_registers[col::script::REGISTERSCOPETYPE_LOCAL][COL_SCRIPT_MAX_REGISTERS - 1];
    	
    } else {
    	functionCall.m_returnRegister = returnRegister;
    }
    
    state.m_callStack.insert(&functionCall);
    
    size_t entryPoint = script->m_functionAddresses[functionIndex];
    state.m_currentInstruction = &script->m_instructions[entryPoint];
    state.m_currentInstructionIndex = entryPoint;
    state.m_maxInstructionIndex = script->m_instructions.length();

    COL_SCRIPT_STATUS status;

    do {
        status = col::script::execInstruction(state);
        
    } while(status == COL_SCRIPT_CONTINUE && state.m_currentInstruction != script->m_instructions.end());
    
    state.m_callStack.pop();
    
    if(status == COL_SCRIPT_RETURN) {
        return COL_SCRIPT_CONTINUE;
    }
    
    return status;
}

bool col::script::getNativeFunction(size_t handle, col::script::NativeFunction &out) {
    col::script::NativeFunction *result = s_nativeFunctions.find(handle);
    if(result == NULL) {
        return false;
    }
    
	out = *result;
	return true;
}

bool col::script::getNativeFunctionAddress(col::memory::ConstantString &name, size_t &addressOut) {
    size_t *existingAddress = s_nativeFunctionNames.find(name);
    if(existingAddress == NULL) {
        return false;
    }
    
    addressOut = *existingAddress;
    return true;
}

bool col::script::enforceRegisterDataType(col::script::State &state, col::script::Register &reg, col::script::DataType givenDataType) {
    if(reg.m_dataType != givenDataType) {
        const char *given = col::script::getDataTypeName(reg.m_dataType);
        const char *expected = col::script::getDataTypeName(givenDataType);
        COL_LOG_ERROR("Unexpected data type %s, expected %s", given, expected);
        killScript(state.m_scriptHandle);
        return false;
    }
    
    return true;
}

COL_SCRIPT_STATUS col::script::execInstruction(col::script::State &state) {
    switch(state.m_currentInstruction->m_abc.m_operation) {
        case col::script::OP_NONE:
            return COL_SCRIPT_CONTINUE;
            
        case totemOperation_Move:
            return col::script::execMove(state);

        case totemOperationType_Add:
            return col::script::execAdd(state);
            
        case totemOperation_Subtract:
            return col::script::execSubtract(state);
            
        case totemOperation_Multiply:
            return col::script::execMultiply(state);
            
        case totemOperation_DIvide:
            return col::script::execDivide(state);
            
        case totemOperation_Power:
            return col::script::execPower(state);
            
        case totemOperation_Equals:
            return col::script::execEquals(state);

        case totemOperation_NotEquals:
            return col::script::execNotEquals(state);
            
        case totemOperation_LessThan:
            return col::script::execLessThan(state);
            
        case totemOperation_LessThanEQUALS:
            return col::script::execLessThanEquals(state);
            
        case totemOperation_MoreThan:
            return col::script::execMoreThan(state);
            
        case totemOperation_MoreThanEQUALS:
            return col::script::execMoreThanEquals(state);
            
        case totemOperation_ConditionalGoto:
            return col::script::execConditionalGoto(state);
            
        case totemOperation_Goto:
            return col::script::execGoto(state);
            
        case totemOperation_NativeFunction:
            return col::script::execNativeFunction(state);
            
        case totemOperation_ScriptFunction:
            return col::script::execScriptFunction(state);

		case totemOperation_Return:
            return col::script::execReturn(state);
            
            /*
        case col::script::OP_NEWOBJ:
            return col::script::execNew(state);*/

        default:
            COL_LOG_FATAL("Unrecognised instruction operation (%d)", state.m_currentInstruction->m_abc.m_operation);
            return COL_SCRIPT_STOP;
    }
}

COL_SCRIPT_STATUS col::script::execMove(col::script::State &state) {
    COL_LOG_DEBUG("exec move");

    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    
    *destination = *source;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execAdd(col::script::State &state) {
    COL_LOG_DEBUG("exec add");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);

    COL_SCRIPT_ENFORCE_TYPE(state, *source1, DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, DATATYPE_NUMBER);
    
    destination->m_value.m_number = source1->m_value.m_number + source2->m_value.m_number;
    destination->m_dataType = DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execSubtract(col::script::State &state) {
    COL_LOG_DEBUG("exec subtract");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, source1->m_dataType);

    destination->m_value.m_number = source1->m_value.m_number - source2->m_value.m_number;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execMultiply(col::script::State &state) {
    COL_LOG_DEBUG("exec multiply");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, col::script::DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, col::script::DATATYPE_NUMBER);
    
    destination->m_value.m_number = source1->m_value.m_number * source2->m_value.m_number;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execDivide(col::script::State &state) {
    COL_LOG_DEBUG("exec divide");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, col::script::DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, col::script::DATATYPE_NUMBER);
    
    destination->m_value.m_number = source1->m_value.m_number / source2->m_value.m_number;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execPower(col::script::State &state) {
    COL_LOG_DEBUG("exec power");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, col::script::DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, col::script::DATATYPE_NUMBER);
    
    destination->m_value.m_number = pow(source1->m_value.m_number, source2->m_value.m_number);
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execReturn(col::script::State &state) {
    COL_LOG_DEBUG("exec return");
    col::script::FunctionCall *call = *state.m_callStack.last();
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *source = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    call->m_returnRegister->m_value = source->m_value;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_RETURN;
}

COL_SCRIPT_STATUS col::script::execEquals(col::script::State &state) {
    COL_LOG_DEBUG("exec equals");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, source2->m_dataType);
    
    destination->m_value.m_number = source1->m_value.m_data == source2->m_value.m_data;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execNotEquals(col::script::State &state) {
    COL_LOG_DEBUG("exec not equals");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, source2->m_dataType);
    
    destination->m_value.m_number = source1->m_value.m_data != source2->m_value.m_data;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execLessThan(col::script::State &state) {
    COL_LOG_DEBUG("exec less than");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, col::script::DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, col::script::DATATYPE_NUMBER);
    
    destination->m_value.m_number = source1->m_value.m_number < source2->m_value.m_number;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execLessThanEquals(col::script::State &state) {
    COL_LOG_DEBUG("exec less than equals");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, col::script::DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, col::script::DATATYPE_NUMBER);
    
    destination->m_value.m_number = source1->m_value.m_number <= source2->m_value.m_number;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execMoreThan(col::script::State &state) {
    COL_LOG_DEBUG("exec more than");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, col::script::DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, col::script::DATATYPE_NUMBER);
    
    destination->m_value.m_number = source1->m_value.m_number > source2->m_value.m_number;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execMoreThanEquals(col::script::State &state) {
    COL_LOG_DEBUG("exec more than equals");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Register *source1 = COL_SCRIPT_GET_OPERANDB_REGISTER(state, instruction);
    col::script::Register *source2 = COL_SCRIPT_GET_OPERANDC_REGISTER(state, instruction);
    
    COL_SCRIPT_ENFORCE_TYPE(state, *source1, col::script::DATATYPE_NUMBER);
    COL_SCRIPT_ENFORCE_TYPE(state, *source2, col::script::DATATYPE_NUMBER);
    
    destination->m_value.m_number = source1->m_value.m_number >= source2->m_value.m_number;
    destination->m_dataType = col::script::DATATYPE_NUMBER;
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execGoto(col::script::State &state) {
    COL_LOG_DEBUG("exec goto");
    col::script::OperandX offset = state.m_currentInstruction->m_axx.m_operandAxx;
        
    if(state.m_currentInstructionIndex + offset > state.m_maxInstructionIndex) {
        COL_LOG_ERROR("goto offset out-of-bounds (%d + %d > %d)", state.m_currentInstructionIndex, offset, state.m_maxInstructionIndex);
        return COL_SCRIPT_STOP;
    }
    
    state.m_currentInstruction += offset;
    state.m_currentInstructionIndex += offset;
    
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execConditionalGoto(col::script::State &state) {
    COL_LOG_DEBUG("exec conditional goto");

    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *source = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::OperandX offset = instruction.m_abx.m_operandBx;
    
    if(state.m_currentInstructionIndex + offset > state.m_maxInstructionIndex) {
        COL_LOG_ERROR("conditional goto offset out-of-bounds (%d + %d > %d)", state.m_currentInstructionIndex, offset, state.m_maxInstructionIndex);
        return COL_SCRIPT_STOP;
    }
    
    if(source->m_value.m_data == 0) {
        state.m_currentInstruction += offset;
        state.m_currentInstructionIndex += offset;
    }
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

COL_SCRIPT_STATUS col::script::execNativeFunction(col::script::State &state) {
    COL_LOG_DEBUG("exec native function");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::OperandX functionHandle = instruction.m_abx.m_operandBx;
    
    col::script::FunctionCall call;
    call.m_returnRegister = destination;
    call.m_type = col::script::FUNCTIONTYPE_NATIVE;
    call.m_handle = functionHandle;
    call.m_arguments.alloc(10);
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    
    while(state.m_currentInstruction->m_abc.m_operation == totemOperation_FunctionArg) {
        col::script::Register *argument = COL_SCRIPT_GET_OPERANDA_REGISTER(state, (*state.m_currentInstruction));
        call.m_arguments.insert(argument);
        COL_SCRIPT_NEXT_INSTRUCTION(state);
    }
    
    col::script::NativeFunction func;
    if(!col::script::getNativeFunction(functionHandle, func)) {
    	COL_LOG_ERROR("Native Function not found with handle %zu", functionHandle);
    	return COL_SCRIPT_STOP;
    }
    
    state.m_callStack.insert(&call);

    COL_SCRIPT_STATUS status = func(state);
    state.m_callStack.pop();
    return status;
}

COL_SCRIPT_STATUS col::script::execScriptFunction(col::script::State &state) {
    COL_LOG_DEBUG("exec script function");
    
    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::Operand functionIndex = instruction.m_abx.m_operandBx;
    
    col::script::ScriptFunction *func = s_scriptFunctions.find(functionIndex);
    if(func == NULL) {
    	COL_LOG_ERROR("Cannot find script function %zu", functionIndex);
    	return COL_SCRIPT_STOP;
    }
    
    COL_SCRIPT_STATUS status = col::script::exec(func->m_scriptHandle, func->m_functionIndex, state, destination);
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return status;
}

COL_SCRIPT_STATUS col::script::execNew(col::script::State &state) {
    COL_LOG_DEBUG("exec new");

    col::script::Instruction instruction = *state.m_currentInstruction;
    col::script::Register *destination = COL_SCRIPT_GET_OPERANDA_REGISTER(state, instruction);
    col::script::OperandX classType = instruction.m_abx.m_operandBx;
    
    col::script::Class *classDef = NULL;
    if(!col::script::getClass(classType, classDef)) {
        COL_LOG_ERROR("Class with handle %zu not found", classType);
        return COL_SCRIPT_STOP;
    }
    
    col::script::gc_new(*destination);
    col::script::Object *object = (col::script::Object*)destination->m_value.m_reference;
    object->m_classHandle = classType;
    object->m_members.reset();
    object->m_members.secure(classDef->m_members.length());
    
    COL_SCRIPT_NEXT_INSTRUCTION(state);
    return COL_SCRIPT_CONTINUE;
}

void col::script::killScript(size_t handle) {
	// TODO: kill script
    COL_LOG_INFO("TODO: kill script");
}