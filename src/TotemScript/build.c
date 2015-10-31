//
//  build.cpp
//  ColossusEngine
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include "build.h"
#include "log.h"

bool col::script::build(col::script::BuildPrototype &build, col::script::Script &scriptOut) {
    scriptOut.m_functionAddresses.reset();
    scriptOut.m_functionNames.reset();
    scriptOut.m_globalRegisters.reset();
    scriptOut.m_globalStack.reset();
    scriptOut.m_instructions.reset();
    
    for(col::script::FunctionPrototype *function = build.m_functions.first(); function != build.m_functions.end(); ++function) {
    	size_t startingAddress = scriptOut.m_instructions.length();
    	
        for(col::script::InstructionPrototype *prototype = function->m_instructions.first(); prototype != function->m_instructions.end(); ++prototype) {
            col::script::Instruction instruction;
            
            switch(prototype->m_type) {
                case col::script::INSTRUCTIONTYPE_ABC:
                    instruction = col::script::buildABCInstruction(function->m_abc[prototype->m_index]);
                    break;
                    
                case col::script::INSTRUCTIONTYPE_ABX:
                    instruction = col::script::buildABxInstruction(function->m_abx[prototype->m_index]);
                    break;
                    
                case col::script::INSTRUCTIONTYPE_AXX:
                    instruction = col::script::buildAxxInstruction(function->m_axx[prototype->m_index]);
                    break;
                    
                default:
                    COL_LOG_ERROR("Unrecognised instruction type %d", prototype->m_type);
                    return false;
            }
            
            scriptOut.m_instructions.insert(instruction);
        }
        
        size_t nameAddr = scriptOut.m_functionNames.length();
        scriptOut.m_functionNames.insert(function->m_name.cstr(), function->m_name.length());
        scriptOut.m_functionNames.insert(0);
        scriptOut.m_functionNameAddresses.insert(nameAddr);
        scriptOut.m_functionAddresses.insert(startingAddress);
    }
    
    // transfer stack data
    build.m_globalStack.giveTo(scriptOut.m_globalStack);
    scriptOut.m_globalRegisters.reset();
    scriptOut.m_globalRegisters.secure(COL_SCRIPT_MAX_REGISTERS);
    
    // define global register list from prototypes
    size_t i = 0;
    for(col::script::RegisterPrototype *regPrototype = build.m_globalRegisters.first(); regPrototype != build.m_globalRegisters.end(); ++regPrototype, ++i) {
    	assert(i < COL_SCRIPT_MAX_REGISTERS);
        scriptOut.m_globalRegisters[i].m_dataType = regPrototype->m_dataType;
    	scriptOut.m_globalRegisters[i].m_value = regPrototype->m_value;
    }
    
	return true;
}

col::script::Instruction col::script::buildABCInstruction(col::script::ABCInstructionPrototype &prototype) {
    col::script::Instruction instruction;

    instruction.m_abc.m_operandAType = prototype.m_operandA.m_registerScopeType;
    instruction.m_abc.m_operandAIndex = prototype.m_operandA.m_registerIndex;
    
    instruction.m_abc.m_operandBType = prototype.m_operandB.m_registerScopeType;
    instruction.m_abc.m_operandBIndex = prototype.m_operandB.m_registerIndex;
    
    instruction.m_abc.m_operandCType = prototype.m_operandC.m_registerScopeType;
    instruction.m_abc.m_operandCIndex = prototype.m_operandC.m_registerIndex;
    
    instruction.m_abc.m_operation = prototype.m_operationType;
    return instruction;
}

col::script::Instruction col::script::buildABxInstruction(col::script::ABxInstructionPrototype &prototype) {
    col::script::Instruction instruction;
    
    instruction.m_abx.m_operation = prototype.m_operationType;
    
    instruction.m_abx.m_operandAIndex = prototype.m_operandA.m_registerIndex;
    instruction.m_abx.m_operandAType = prototype.m_operandA.m_registerScopeType;
    
    instruction.m_abx.m_operandBx = prototype.m_operandBx;
    
    return instruction;
}

col::script::Instruction col::script::buildAxxInstruction(col::script::AxxInstructionPrototype &prototype) {
    col::script::Instruction instruction;
    
    instruction.m_axx.m_operation = prototype.m_operationType;
    instruction.m_axx.m_operandAxx = prototype.m_operandAxx;
    
    return instruction;
}
