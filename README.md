TotemScript is a light-weight, general-purpose scripting language.
### Examples
```
print("Hello, World!");
```
#### Defining Variables
Variables are dynamic by default, supporting the following types:
* int - 64-bit signed integers
* float - 64-bit floating point
* string - Interned, immutable strings
* function - function pointers
* array - Garbage-collected Arrays
* type - Type objects (e.g. int, float, type etc.)
```
// variables can be declared and redeclared at any point
$var = 123;
$var = "This is now a string.";

function test()
{
    // this will reference the variable in global-scope
    $var = 456;

    // vars can also be read-only
    const $x = "This variable cannot be modified";
}
```
#### Functions
```
// declaring a function
function test($a, $b)
{
    return $a + $b;
}

$a = test(123, 456); 
$a = test(123); // arguments default to an integer value of 0 when not provided by caller

$b = @test; // functions can also be stored in variables
$a = $b(123, 456, 789); // additional arguments provided by caller are discarded

$b = function($a, $b)
{
    return $a * $b;
};

$a = $b(123, 456);

$a = function($b, $c)
{
    return $b - $c;
}
(456, 123);
```
#### Strings
```
// Strings are immutable
$a = "Hello, ";
$b = $a + " World!";
print($b); // Hello, World!
$c = $a[2];
print($c); // l
```
#### Arrays
```
// Defines an Array that holds 20 values
$a = [20];

$a[0] = 1;
$a[1] = 2;

$b = $a[0] + $a[1];

// Arrays cannot be resized and must be redeclared if additional space is needed
$a[20] = 1; // runtime error

function test()
{
    $a = [5];
    $a = "some other value"; // Arrays are automatically garbage-collected when no-longer referenced
}
```
### Backlog
#### New Language Features
* coroutines
 * defined using the coroutine keyword, e.g. coroutine test { return 0; } 
 * instances are created like function-pointers, e.g. $x = @test;
 * instances each have their own dynamically-allocated stack, and are garbage-collected
 * "return" stores the last known instruction & callstack
* operator precedence reordering
* exceptions - try, catch, finally & throw
 * both user & system-generated exceptions
* type hinting, allowing compile-time type checking (e.g. $x:int = "this won't compile!"; )
 * cannot mix dynamic vars and type-checked vars
 * "is" operator should be evaluated at compile-time
* erlang-style records that eval to arrays at runtime
 * vars MUST be type-hinted, to prevent inappropriate array access (e.g. $a:vec2 = {}; ) 
 * public & private access specifiers (e.g. record vec2 { public $x:float = 0, public $y:float = 0 };
 * "loose" functions should eval to normal functions with $this corresponding to record value, e.g. record a { function whatever() { print($this); } }
* overriding operators 
* initializer lists for arrays & records, both with and without indices/names e.g. $a:vec2 = { $x:1.75, $y:1.45 };
* loop scope for vars

#### Runtime Improvements
* ref-counting cycle detection
* line/char/len numbers for eval, link & exec errors
* pre-compute value-only expressions
* unroll determinate loops
* escape analysis for arrays
* JIT
* bytecode serialisation
 * check register & function addresses, function arguments
* register-allocation improvement
 * track how many times a variable is referenced
 * local const vars should eval to global vars wherever possible
 * global const vars should eval straight to value register
 * piggy-back on free global registers if any are available and local scope is full
* make sequences of array / string concatenation more efficient
* breakpoints
 * lookup first instruction at given line of source code, replace with breakpoint instruction, store original
 * call user-supplied callback, then execute original instruction