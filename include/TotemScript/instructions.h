//
//  instructions.h
//  TotemScript
//
//  Created by Timothy Smale on 22/05/2026
//  Copyright © 2026 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_INSTRUCTIONS_H
#define TOTEMSCRIPT_INSTRUCTIONS_H

/**
 * Instruction Formats
 *
 * Instructions are 32-bit and encode:
 *  - an 8-bit optype
 *  - one or more 8-bit register operands
 *  - one or more unsigned operands (either 8-bit, 16-bit or 24-bit)
 *  - one or more signed operands (either 8-bit, 16-bit or 24-bit)
 *
 * Instruction types are named in a way that describes their operands at a glance e.g.
 *  - register operands are named A, B or C
 *  - signed operands are named after the number of bytes that it takes up e.g. A2 means a signed operand that is 2-bytes in length
 *  - unsigned operands are the same, except that they are suffixed with a "u" e.g. A3u means an unsigned operand that is 3-bytes in length
 */
#define TOTEM_EMIT_INSTRUCTION_TYPES(_) \
    _(A) \
    _(A3) \
    _(Ab) \
    _(Ab2) \
    _(Ab2u) \
    _(Ab2ug) \
    _(Abc) \
    _(Abc1) \
    _(Abc1u)
    /*
    -----------------------------------------------------------------------
    |0     |                    |                                       31|
    -----------------------------------------------------------------------
    |OpType|Register A          |Unused/Available                         | AInstruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                                                            31|
    -----------------------------------------------------------------------
    |OpType|A3 (Signed Int)                                               | A3Instruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                                                            31|
    -----------------------------------------------------------------------
    |OpType|A3u (Unsigned Int)                                            | A3uInstruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                    |                  31|
    -----------------------------------------------------------------------
    |OpType|Register A          |Register B          |Unused/Available    | AbInstruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                                       31|
    -----------------------------------------------------------------------
    |OpType|Register A          |B2 (Signed Int)                          | Ab2Instruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                                       31|
    -----------------------------------------------------------------------
    |OpType|Register A          |B2u (Unsigned Int)                       | Ab2uInstruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                                       31|
    -----------------------------------------------------------------------
    |OpType|Register A          |B2ug (Global Register Index)             | Ab2ugInstruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                    |                  31|
    -----------------------------------------------------------------------
    |OpType|Register A          |B1  (Signed Int)    |C1  (Signed Int)    | Ab1c1Instruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                    |                  31|
    -----------------------------------------------------------------------
    |OpType|Register A          |Register B          |Register C          | AbcInstruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                    |                  31|
    -----------------------------------------------------------------------
    |OpType|Register A          |Register B          |C1 (Signed Int)     | Abc1Instruction
    -----------------------------------------------------------------------
    */
    /*
    -----------------------------------------------------------------------
    |0     |                    |                    |                  31|
    -----------------------------------------------------------------------
    |OpType|Register A          |Register B          |C1u (Unsigned Int)  | Abc1uInstruction
    -----------------------------------------------------------------------
    */

#endif /* TOTEMSCRIPT_INSTRUCTIONS_H */
