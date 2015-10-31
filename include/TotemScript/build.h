//
//  build.h
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_BUILD_H
#define TOTEMSCRIPT_BUILD_H

#include <TotemScript/base.h>
#include <TotemScript/eval.h>
#include <TotemScript/exec.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Build bytecode from instructions
     */
    totemBool build(totemBuildPrototype *build, totemScript *scriptOut);
    totemInstruction buildABCInstruction(totemABCInstructionPrototype *prototype);
    totemInstruction buildABxInstruction(totemABxInstructionPrototype *prototype);
    totemInstruction buildAxxInstruction(totemAxxInstructionPrototype *prototype);

#ifdef __cplusplus
}
#endif
    
#endif
