A parser & virtual machine for a custom scripting language written in C.

todo: new language features
---------------------------
ref-counting cycle detection
array operators
string operators
function pointers, e.g. $x = @funcName; $x($y, $z);
loop scope for vars
yield - stores this function's call stack in a local var, allowing it to be resumed later as a function pointer (e.g. $x = callFunc(); $x();)
operator precedence reordering
"is" operator, e.g. "$x is int" or "$y is float"
exceptions - try, catch & throw
    - both user & system-generated exceptions

todo: syntactic sugar
---------------------
anonymous functions that eval to function pointers
type hinting, allowing compile-time type checking (e.g. $x:int = "this won't compile!"; )
    - cannot mix dynamic vars and type-checked vars
erlang-style records that eval to arrays at runtime
    - vars MUST be type-hinted, to prevent inappropiate array access (e.g. $a:vec2 = new vec2; ), including arrays (e.g. $x:int = [10]; $x[0] = new vec2;)
    - public & private access specifiers (e.g. record vec2 { public $x:float = 0, public $y:float = 0 };
    - "loose" functions eval to normal functions with $this corresponding to record value, e.g. record a { function whatever() { print($this); } }

todo: runtime improvements
--------------------------
line/char numbers for eval errors
move-to-global and move-to-local instructions, to expand potential number of global vars from 255 to TOTEM_OPERANDX_UNSIGNED_MAX
global string-value cache attached to runtime, instead of per-actor
two string types behind-the-scenes: the default interned strings, and a mutable string type that is created when a string is modified (which is then interned when needed e.g. equality check, hash check etc.)
local const vars should eval to global vars wherever possible
unroll determinant loops
register-allocation improvement
    - track how many times a variable is referenced
    - recycle temp registers
remove unneccessary move instructions
    - will need to differentiate between "strong" array access and "weak" array access, to ensure ref counts aren't mangled
escape analysis for arrays & records
JIT