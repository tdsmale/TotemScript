### TotemScript?
* A small, accessible scripting language.
* Designed to be small, simple and dumb - easily plugged into existing applications.
* Features dynamic typing, full garbage-collection, cooperative multitasking, and a silly name.
* Source Repository: [https://github.com/tdsmale/TotemScript](https://github.com/tdsmale/TotemScript)

### Hello, World!
```php
// Statements can either end with ';' or a new-line
print("Hello, World!");
```
#### Defining Variables
Variables are dynamically-typed, supporting the following types:
* float - 64-bit floating point
* int - 32-bit unsigned integers
* string - Interned, immutable strings
* function - First-class functions
* array - Fixed-size Arrays
* object - String/Value Maps
* coroutine - Cooperative coroutines
* instance - Script instances
* type - Type objects (e.g. int, float, type etc.)
* userdata - Data supplied by native C functions
* boolean - true/false
* null
```php
// variables are declared by using the "var" keyword
var a = 123;

// variables are dynamically-typed and can hold values of any type
a = "This is now a string.";

function test()
{
	// this will reference the variable in global-scope
	a = 456;

	// variables can also be declared as read-only using the "let" keyword
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
	};

	// variable names can be reused within a new scope by just redefining them
	var a = 123;

	// control loops also have their own scope
	if(true)
	{
		var a = 456; 
	}
}

// Explicitly declare new scopes like this:
{
	var x = 123;
}

```
#### Loading External Scripts
```php
// combine scripts by using "import"
// this will load that script as an instance that you can use to call its functions
// any instructions defined in the script's "global" scope will be ran automatically upon creation
let a = import("./myfilename.totem");

a.callFunc(123, 456);

// you can create as many individual instances as you want
let b = import("./myfilename.totem");

// use "import_shared" to create & load a shared, cached version
// calling this multiple times yields the same instance
let c = import_shared("./myfilename.totem");

// the instance cannot be directly modified, and only string keys can be used on it
let d = c[123]; // runtime error
c["what"] = 123; // runtime error
```
#### Functions
```php
// declaring a function
function test(var a, var b)
{
	return a + b;
}

var a = test(123, 456);

// arguments default to null when not provided by caller
a = test(123); 

// additional arguments provided by the caller are discarded
a = test(123, 456, 789); 

// functions can also be declared anonymously and assigned as values
let x = function(var a, var b)
{
	return a + b;
};
```
#### Strings
```php
// Strings are immutable, but can be combined to create new strings
var a = "Hello, ";
var b = a + " World!";
print(b); // Hello, World!

// you can also access individual characters
var c = a[2];
print(c); // l
```
#### Arrays
```php
// Defines a garbage-collected array that holds 5 values
var a = [5:1, 2, 3, 4, 5];

// ...or they can be declared with just their size and no elements
a = [4:];

// ...or with just a set of elements
a = [1, 2, 3, 4, 5];

// you can access elements like this:
var b = a[0] + a[1];

// Arrays cannot be resized and must be redeclared if additional space is needed
a[20] = 1; // runtime error

// Arrays are automatically garbage-collected when no-longer referenced
a = null; 

```
#### Object
```php
// Create new garbage-collected object
var obj = {};

// Objects map strings to values
obj.test = 123;
var val = obj.test;

// Objects can use either bracket-notation or dot-notation
obj["test"] = 456; 

// Objects can store any sort of value
obj["test2"] = function(var x, var y)
{
	return x * y;
};

obj["test2"](123, 456);

// Objects do not need to be resized to insert new elements, but are less efficient than arrays
var key = "key";
obj[key] = [20];

for(var i = 0; i < 20; i++)
{
	obj[key][i] = i;
}

// Objects can be initialized with values just like arrays
var y = { a:456, "b":"Hello!" };

// values can be removed by assigning null
obj[key] = null;
assert(obj[key] == null);

```
#### Coroutines
```php
// Coroutines are functions that can be paused and resumed later
var co = coroutine(var start, var end)
{
	for(var i = start; i < end; i++)
	{
		// yield statements pause the coroutine
		// when called again, the coroutine will resume from this position
		yield i; 
	}

	// once a return statement is hit, the coroutine is reset
	return -1; 
};

// Coroutines can also be created by casting a function to a coroutine
var co = myFunction as coroutine;

co(11, 20);

// Coroutines are also garbage-collected, just like arrays - when no longer referenced, they are destroyed
co = null;

```
### Version History

#### 0.1
* Initial release

#### 0.2
* Removed include statement
* Added "import" & "import_shared" functionality
* Can define anonymous blocks
* Removed reference-counting as a garbage-collection option, now just mark-and-sweep
* Removed 32-bit build support
* Array declarations can now explicitly state the number of elements as well as the included elements
* Removed "shift" operation
* Added bitwise operations for integers
* Coroutine expression e.g. let x = coroutine(let a, let b) { stmts }
* Expanded instruction set
* Better register/value evaluation

### Planned Feature Creep

### 0.3
* Add loop { stmts }
* both signed & unsigned integer support
* integer / float folding at compile time
* Array range support
* Unary precedence
* push pop operators
	* ->
	* <-
* Make arrays resizable with push operator
	* push n entries with optional values, same as array declarations e.g. x <- n:null
* Closures
	* when a function references a variable in a parent function scope, that parent scope is passed in as an array
	* should be able to reference any parent scope, not just global
	* global scope just becomes another parent scope (no need for global variables in instruction format anymore)
	* consolidate instance functions & scripts into closures - no more global instructions or variables

#### Language Backlog
* Break statement
* Continue statement
* Switch statement
* Define multiple vars e.g. let x, y = 1, 2
* Allow all possible values as keys/values in objects & instances
	* Need to hash all possible values
* Key-only entries in maps (making it a set if only keys are provided, which does not need values array)
	* e.g. var x = { "key": "value", "keyonly" }
	* if key and value are the same, store as keyonly
	* returns key on lookup
	* no need for additional value register
* Multiple return values
* integer literals
	* hex, octal, binary
* unpacking e.g.  
	* let x = [1, 2, 3]; print(x...);
	* let x = [1, 2, 3]; let a, b, c = x...;
* packing e.g.
	* let x, ...y = 1, 2, 3, 4; print(y...); //2, 3, 4
* Labelling control blocks, so break/continue statements can reference them and break out of multiple loops at once
* with (stmts) { stmts } then { stmts } - "then" always executes when leaving this scope
* object arithmetic (add, subtract, union, intersection, difference etc.)
* async (single-threaded socket/file/timer poller in an execState)
	* await { case exp1: { stmts } case exp2: case exp3: { stmts } }
		* each case is a coroutine
		* each case is a set of expressions that can be fed variables
		* if part of a case expression blocks, move onto another
		* break when at least one case has completed
	* nowait { stmts }    
		* fire-and-forget equivalent of await
	* allwait { stmts }
		* same as await, but waits for all included expressions to finish
	* execState will not "stop" until all remaining async work is complete
	* let native functions yield and resume as well
	* async variables
		* value can be checked for changes in async case statements
		* e.g. async a = 3; allwait { case a == 3: break; } / allwait(a) { case 3: break; }
* run arbitrary code at compile-time

#### Runtime Backlog
* Fold complex constants (arrays, objects, strings)
* Fold unary operations
* Instruction / Instance / Value Serialization
* Better type inference under-the-hood
* Inline caching / hidden classes for objects (similar to V8)
* have evaluator use TOTEM_JMP
* rewrite in TotemScript (incrementally)
* Instruction interrupts (replace instruction with interrupt, fire callback then execute original instruction in-place)
* encode function call info into register stack
* basic jit
	* function-call per instruction
	* write in TotemScript
* c unit tests
* encode compressed floating point numbers into instructions where lossless
	* do strings as well
* vscode plugin
