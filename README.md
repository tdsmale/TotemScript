### TotemScript?
* A small, accessible scripting language.
* Designed to be small, simple and dumb - easily plugged into existing applications.
* Features dynamic typing, full garbage-collection, cooperative multitasking, and a silly name.

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
var va = 123;

// variables can hold values of any type
va = "This is now a string.";

function test()
{
    // this will reference the variable in global-scope
    va = 456;

    // variables can also be declared as read-only
    let x = "This variable cannot be modified";

    // values can be typecast
    va = 123 as string;

    // can check type at runtime
    if(va is string)
    {
        // va is a string!
    }

    // can also retrieve type as value
    type = va as type;

    // values can also be "shifted" from one location to another
    a << type;
}
```
#### Variable Scope
```php
// Variables declared outside of functions are in global scope, and can be accessed by any function.
var global = "This can be accessed by any function.";

function test()
{
    return global + " See?";
}

// Variables declared inside of functions are in local scope, and can only be accessed by that function.
function localTest()
{
    var a = 123;

    var b = function()
    {
        return a; // "variable undefined" error
    }

    // variable names can be reused within a new scope by just redefining them
    var global = 123;
	
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

// Objects can store any sort of value, but can only use strings as keys
obj["test2"] = function(x, y)
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
* Remove variable & pointer prefixes
* Break statement
* Continue statement
* Array ranges e.g. var x = [ 0 .. 12 ];
* Object init lists e.g. var x = { "a":123, b:c };
* Object/Array keys - anything not valid is implicitly cast to the correct type
* Strict compile-time type-hinting e.g. var x:int = 4;
* Investigate NaN-boxing for representing register values more efficiently
* Investigate equalizing the size of all operands (including opcodes) to a single byte
 * Will expand total number of possible opcodes, making room for more specialised operations
 * Won't waste operands on certain opcodes
* Operator precedence
* #import statement 
 * same as #include, but stored in a local variable  e.g. #import math.totem as math; math.abs(a);
 * global variables are inaccessible
 * modules get stored in execState and are ran once when loaded
* make #include & #import part of the parser
* bytecode serialization