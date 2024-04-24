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
// Coroutines are functions that pause when they return, and can be resumed later

// Coroutines are created by casting any function to a coroutine, and invoked like a regular function
var co = function(var start, var end)
{
    for(var i = start; i < end; i++)
    {
        return i + start + end;
    }

    return 0;
} as coroutine;

var start = 11;
var end = 20;

// Parameters can be overridden with new values on subsequent calls
co(start, end);

for(var numLoops = 1; var val = co(); numLoops++)
{
    print(val);
}

// Coroutines reset once they reach the end of a function
co(start + 1, end + 1);
for(var numLoops = 1; var val = co(); numLoops++)
{
    print(val);
}

// Coroutines are also garbage-collected, just like Arrays - when no longer referenced, they are destroyed
co = 123;

```
### Feature Creep
* Break statement (e.g. break 2;)
* Continue statement (e.g. continue 2;)
* Switch statement
* Strict compile-time type-hinting e.g. var x:int = 4;
* Investigate equalizing the size of all operands (including opcodes) to a single byte
 * Will expand total number of possible opcodes, making room for more specialised operations
 * Won't waste operands on certain opcodes
 * Won't need to do any bitshifting during interpretation
* #import statement 
 * same as #include, but stored in a local variable e.g. #import math.totem as math; math.abs(a);
 * global variables are inaccessible
 * modules get cached locally to execState and are ran once when linked
* make #include & #import part of the parser
* bytecode serialization
* erlang-style records that eval to arrays
 * public / private
 * inheritance / protected
 * compile-time duck-typing
 * requires type-hinted variables
 * "as" is disallowed by default
 * "is" is determined at compile-time
 * operator overloading
