A parser & virtual machine for a custom scripting language written in C.

### backlog

#### new language features
* loop scope for vars
* yield - stores this function's call stack in a local var, allowing it to be resumed later as a function pointer (e.g. $x = callFunc(); $x();)
* operator precedence reordering
* exceptions - try, catch, finally & throw
 * both user & system-generated exceptions

#### syntactic sugar
* type hinting, allowing compile-time type checking (e.g. $x:int = "this won't compile!"; )
 * cannot mix dynamic vars and type-checked vars
* erlang-style records that eval to arrays at runtime
 * vars MUST be type-hinted, to prevent inappropriate array access (e.g. $a:vec2 = new vec2; ), including arrays (e.g. $x:int = [10]; $x[0] = 123;) 
 * public & private access specifiers (e.g. record vec2 { public $x:float = 0, public $y:float = 0 };
 * "loose" functions eval to normal functions with $this corresponding to record value, e.g. record a { function whatever() { print($this); } }
 * "is" must be evaluated at compile-time with record types
 * type-casting?
* initializer lists for arrays, both with and without indices

#### runtime improvements
* ref-counting cycle detection
* line/char/len numbers for eval, link & exec errors
* new internal string representations:
 * "small" strings that can fit within the register value
 * dynamic strings, which are created when a string is modified, then interned when that functionality is needed
* pre-compute value-only expressions
 * discard values that aren't referenced anywhere else
* unroll determinate loops
* escape analysis for arrays
* JIT
* bytecode evaluation
 * check register & function addresses, function arguments
* register-allocation improvement
 * track how many times a variable is referenced
 * local const vars should eval to global vars wherever possible
 * global const vars should eval straight to value register
* piggy-back on free global registers if any are available and local scope is full
* breakpoints
 * replace chosen instruction with break instruction, store original instruction alongside breakpoint
 * call user-supplied callback
 * when removing a breakpoint, replace instruction with original
* make sequences of array / string concatenation more efficient