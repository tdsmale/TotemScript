### TotemScript?
* A small, accessible scripting language.
* Designed to be small, simple and dumb - easily plugged into existing applications.
* Features dynamic typing, full garbage-collection, cooperative multitasking, and a silly name.
* Source Repository: [https://github.com/tdsmale/TotemScript](https://github.com/tdsmale/TotemScript)

### Examples
```php
print("Hello, World!");
```
#### Defining Variables
Variables are dynamically-typed, supporting the following types:
* int - 64-bit signed integers
* float - 64-bit floating point
* string - Interned, immutable strings
* function - First-class functions
* array - Fixed-size Arrays
* object - String/Value Maps
* coroutine - First-class coroutines
* type - Type objects (e.g. int, float, type etc.)
* userdata - Data supplied by native C functions
* boolean - true/false
* null
```php
// variables are declared thusly:
var a = 123;

// variables can hold values of any type
a = "This is now a string.";

function test()
{
    // this will reference the variable in global-scope
    a = 456;

    // variables can also be declared as read-only
    let x = "This variable cannot be modified";

    // values can be typecast
    a = 123 as string;

    // can check type at runtime
    if(a is string)
    {
        // a is a string!
    }

    // can also retrieve type as value
    var t = a as type;

    // values can also be "shifted" from one location to another
	// in this case, "t" would now be null
    a << t;
}
```
#### Variable Scope
```php
// Variables declared outside of functions are in global scope, and can be accessed by any function.
var a = "This can be accessed by any function.";

function test()
{
    return a + " See?";
}

// Variables declared inside of functions are in local scope, and can only be accessed by that function.
function localTest()
{
    var c = 123;

    var b = function()
    {
        return c; // "variable undefined" error
    }

    // variable names can be reused within a new scope by just redefining them
    var a = 123;
	
	if(true)
	{
		var a = 456; // control loops also have their own scope
	}
}

```
#### Combining scripts
```php
#include ../otherDir/otherFile.totem;
// include statements must be the first statements in a file
// file paths are relative to the current file's path
```
#### Functions
```php
// declaring a function
// named functions can only be declared in global scope
function test(var a, var b)
{
    return a + b;
}

var a = test(123, 456); 
a = test(123); // arguments default to null when not provided by caller

var b = test; // functions can also be stored in variables
a = b(123, 456, 789); // additional arguments provided by caller are discarded

// functions can also be anonymous, which can be declared anywhere
b = function(var a, var b)
{
    return a * b;
};

a = b(123, 456);

a = function(var b, var c)
{
    return b - c;
}
(456, 123);
```
#### Strings
```php
// Strings are immutable, but can be combined to create new strings
var a = "Hello, ";
var b = a + " World!";
print(b); // Hello, World!
var c = a[2];
print(c); // l
```
#### Arrays
```php
// Defines an Array that holds 20 values
var a = [20];

for(var i = 0; i < 20; i++)
{
	a[i] = i;
}

var b = a[0] + a[1];

// Arrays cannot be resized and must be redeclared if additional space is needed
a[20] = 1; // runtime error

a = "some other value"; // Arrays are automatically garbage-collected when no-longer referenced

// Arrays can also be created with values already inserted
a = [1, 2, "3", b];
```
#### Objects
```php
// Create new garbage-collected object
var obj = {};

// Objects map values to names
obj.test = 123;
var val = obj.test;

obj["test"] = 456; // objects can use either bracket-notation or dot-notation

// Objects can store any sort of value
obj["test2"] = function(var x, var y)
{
    return x * y;
};

val["test2"](123, 456);

// Objects are less efficient than arrays, but can hold any amount of values 
var key = "key";
obj[key] = [20];

for(var i = 0; i < 20; i++)
{
    obj[key][i] = i;
}

// non-string keys are implicitly cast to strings
obj[123] = 456;
assert(obj[123] == obj[(123 as string)]);

// objects can be initialized with values just like arrays
var y = { 123:456, "789":"Hello!" };

// values can be removed by shifting them out
key << obj[key];
```
#### Coroutines
```php
// Coroutines are functions that can be paused and resumed later

// Coroutines are created by casting any function to a coroutine, and invoked like a regular function
var co = function(var start, var end)
{
    for(var i = start; i < end; i++)
    {
        yield; // this will pause the function without returning anything
    }

    return -1; // once the return statement is hit, the coroutine is reset
} as coroutine;

co(11, 20);

for(var val = 0; val != -1; val = co())
{
	// will loop until the return statement is hit
    print(val);
}

// Coroutines are also garbage-collected, just like Arrays - when no longer referenced, they are destroyed
co = 123;

```
### Feature Creep
* Specialized opcodes
* Num operator (e.g. #{});
* Clone operator (e.g. *{});
* Unary precedence
* bitwise, logical & modulo ops
* Break statement (e.g. break 2;)
* Continue statement (e.g. continue 2;)
* Switch statement
* Try-catch-finally, throw, "error" value type
* optional statically-typed vars (e.g. var a:int = 123; a = "this will not compile";)
 * can assign to "generic" var, but not the other way around without a cast first!
 * function return types
* classes that eval to objects (e.g. class vec2 { var x:float = 0.0; var y:float = 0.0; }; var pos:vec2 = { x:4.5, y:5.5 }; pos = "this will not compile"; )
 * member access = array access, rather than key lookup
 * should degrade to normal objects on assignment
* change object representation - v8-style
 * hashmaps are shared amongst commonly-organised objects - also contain "transitions" between hashmaps when adding or removing elements
  * once assigned to "generic" var, becomes a normal object again
   * thus, no private vars - no point!
 * "loose" functions - eval to normal functions that take object as first parameter
  * don't have overhead of accessing member, but still need to be accessible as members since objects degrade to dynamic vars
* closures!
 * when a function references a variable in a parent scope, that variable should be stored in an anon array rather than in register
 * then when referencing that variable, parent scope will just reference the anon array to update value
 * each of these objects (one per scope) become additional parameters
  * global vars are just one of these objects
   * so I guess..."instance functions" and "instances" get combined into "closures"?
    * closures carry around "totemScriptFunction" pointer, and array of scope capture objects?
* at this point, *should* be able to unify all gc objects...array of registers & pointer to...
 * arrays - members & NULL
 * objects - members & object layout
 * coroutines - stack & closure
 * closure - scope captures & function prototype
 * userdata - members, clone & destructor callbacks
* import statement
 * similar to include, but registers as a closure e.g. import "math.totem" as math; var a = math();
* make import & include statements part of the parser
* remove all the "options" :
 * no global operands - they're just a table now
 * global cache stays on - but more than one scope now
 * keep & expand expanded opcodes
 * stick to mark & sweep, rather than ref-counting
 * NaN values
* potentially encode frame info into register stack
 * instead of linked list
* large ints
 * use remaining 16-bits to expand integer value somehow?
* native classes
 * should be degradable to objects as well
* go-style async primitives
 * channels
 * select
* some sort of task scheduler
 * make use of yield
  * also allow c functions to yield
 * execState would have to wait until all pending tasks have finished
 * how do we adapt our memory model? either:
  * gc for each execState would need to be concurrent
  * tasks would need to be fully isolated and not share memory (blegh)
* basic jit

var x = 1;

function whatever()
{
	var c = 2;
	
	var d = function()
	{
		var e = x;
		var f = d;
	};
	
	return d;
}

whatever()();

...

var globals = [1];
globals[0] = 1;

function whatever(var globalsRef)
{
	function _anonFunction0(var globals2Ref)
	{
		var e = globals2Ref[0][0];
		var f = globals2Ref[1];
	}

	var globals2 = [2];
	globals2[0] = globalsRef;
	globals2[1] = createClosure(_anonFunction0, globals2);
	return globals2[1];
}

var _whatever = createClosure(whatever, globals);
whatever()();
