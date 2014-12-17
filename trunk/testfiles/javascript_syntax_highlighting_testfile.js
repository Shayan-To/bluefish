/* Bluefish HTML Editor
 * javascript_syntax_highlighting_testfile.js
 *
 * Copyright (C) 2014 Balázs Úr
 * Copyright (C) 2014 Olivier Sessink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/**
 * Reserved words
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Reserved_Words
 */
break
case
catch
continue
debugger
default
delete
do
else
finally
for
function
if
in
instanceof
new
return
switch
this
throw
try
typeof
var
void
while
with



/**
 * Standard built-in objects
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
 */

// Value properties
Infinity
NaN
undefined

// Function properties
eval()
uneval() // Not standardized
isFinite()
isNaN()
parseFloat()
parseInt()
decodeURI()
decodeURIComponent()
encodeURI()
encodeURIComponent()
escape() // Deprecated
unescape() // Deprecated

// Fundamental objects
Object
Function
Boolean
Symbol // Experimantal
Error
EvalError
InternalError
RangeError
ReferenceError
StopIteration
SyntaxError
TypeError
URIError

// Numbers and dates
Number
Math
Date

// Text processing
String
RegExp

// Indexed collections
Array
Float32Array
Float64Array
Int16Array
Int32Array
Int8Array
Uint16Array
Uint32Array
Uint8Array
Uint8ClampedArray
ParallelArray // Not standardized

//Keyed collections
Map // Experimental
Set // Experimental
WeakMap // Experimental
WeakSet // Experimental

//Structured data
ArrayBuffer
DataView
JSON

//Control abstraction objects
Iterator // Not standardized
Generator // Experimental
Promise // Experimental

// Reflection
Reflect // Experimental
Proxy // Experimental

//Internationalization
Intl
Intl.Collator
Intl.DateTimeFormat
Intl.NumberFormat

// Other
arguments



/**
 * The Date object
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date
 */

// Constructors
new Date();
new Date(value);
new Date(dateString);
new Date(year, month, day, hour, minute, second, millisecond);

// Properties
Date.prototype
Date.length

// Properties inherited from Function:
Date.arity
Date.caller
Date.constructor
Date.length
Date.name

// Methods
Date.now()
Date.parse()
Date.UTC()

// Methods inherited from Function:
Date.apply()
Date.call()
Date.toSource()
Date.toString()

// Methods inherited from Object:
Date.__defineGetter__()
Date.__defineSetter__()
Date.hasOwnProperty()
Date.isPrototypeOf()
Date.__lookupGetter__()
Date.__lookupSetter__()
Date.__noSuchMethod__()
Date.propertyIsEnumerable()
Date.unwatch()
Date.watch()

// Methods from Date.prototype
Date.prototype.getDate()
Date.prototype.getDay()
Date.prototype.getFullYear()
Date.prototype.getHours()
Date.prototype.getMilliseconds()
Date.prototype.getMinutes()
Date.prototype.getMonth()
Date.prototype.getSeconds()
Date.prototype.getTime()
Date.prototype.getTimezoneOffset()
Date.prototype.getUTCDate()
Date.prototype.getUTCDay()
Date.prototype.getUTCFullYear()
Date.prototype.getUTCHours()
Date.prototype.getUTCMilliseconds()
Date.prototype.getUTCMinutes()
Date.prototype.getUTCMonth()
Date.prototype.getUTCSeconds()
Date.prototype.getYear() // Deprecated
Date.prototype.setDate()
Date.prototype.setFullYear()
Date.prototype.setHours()
Date.prototype.setMilliseconds()
Date.prototype.setMinutes()
Date.prototype.setMonth()
Date.prototype.setSeconds()
Date.prototype.setTime()
Date.prototype.setUTCDate()
Date.prototype.setUTCFullYear()
Date.prototype.setUTCHours()
Date.prototype.setUTCMilliseconds()
Date.prototype.setUTCMinutes()
Date.prototype.setUTCMonth()
Date.prototype.setUTCSeconds()
Date.prototype.setYear() // Deprecated
Date.prototype.toDateString()
Date.prototype.toISOString()
Date.prototype.toJSON()
Date.prototype.toGMTString() // Deprecated
Date.prototype.toLocaleDateString()
Date.prototype.toLocaleFormat() // Not standardized
Date.prototype.toLocaleString()
Date.prototype.toLocaleTimeString()
Date.prototype.toSource() // Not standardized
Date.prototype.toString()
Date.prototype.toTimeString()
Date.prototype.toUTCString()
Date.prototype.valueOf()

debugger

/**
 * The Math object
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math
 */

// Properties
Math.E
Math.LN2
Math.LN10
Math.LOG2E
Math.LOG10E
Math.PI
Math.SQRT1_2
Math.SQRT2

// Properties inherited from Object:
Math.constructor
Math.__parent__
Math.__proto__

// Methods
Math.abs(x)
Math.acos(x)
Math.acosh(x) // Experimantal
Math.asin(x)
Math.asinh(x) // Experimantal
Math.atan(x)
Math.atanh(x) // Experimantal
Math.atan2(y, x)
Math.cbrt(x) // Experimantal
Math.ceil(x)
Math.clz32(x)
Math.cos(x)
Math.cosh(x) // Experimantal
Math.exp(x)
Math.expm1(x) // Experimantal
Math.floor(x)
Math.fround(x) // Experimantal
Math.hypot([x[,y[,…]]]) // Experimantal
Math.imul(x) // Experimantal
Math.log(x)
Math.log1p(x) // Experimantal
Math.log10(x) // Experimantal
Math.log2(x) // Experimantal
Math.max([x[,y[,…]]])
Math.min([x[,y[,…]]])
Math.pow(x,y)
Math.random()
Math.round(x)
Math.sign(x) // Experimantal
Math.sin(x)
Math.sinh(x) // Experimantal
Math.sqrt(x)
Math.tan(x)
Math.tanh(x) // Experimantal
Math.toSource()
Math.trunc(x) // Experimantal



/**
 * The Number object
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number
 */

// Constructor
new Number(value)

// Properties
Number.EPSILON
Number.MAX_VALUE
Number.MIN_VALUE
Number.NaN
Number.NEGATIVE_INFINITY
Number.POSITIVE_INFINITY
Number.prototype

// Properties inherited from Function:
Number.arity
Number.caller
Number.constructor
Number.length
Number.name

// Methods
Number.isNaN() // Experimental
Number.isFinite() // Experimental
Number.isInteger() // Experimental
Number.toInteger() // Not standardized
Number.parseFloat() // Experimental
Number.parseInt() // Experimental

// Methods inherited from Function:
Number.apply()
Number.call()
Number.toSource()
Number.toString()

// Methods from Number.prototype
Number.prototype.toExponential()
Number.prototype.toFixed()
Number.prototype.toLocaleString()
Number.prototype.toPrecision()
Number.prototype.toSource() // Not standardized
Number.prototype.toString()
Number.prototype.valueOf()

// Methods inherited from Object:
Number.__defineGetter__()
Number.__defineSetter__()
Number.hasOwnProperty()
Number.isPrototypeOf()
Number.__lookupGetter__()
Number.__lookupSetter__()
Number.__noSuchMethod__()
Number.propertyIsEnumerable()
Number.unwatch()
Number.watch()



/**
 * The String object
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String
 */

// Properties
String.prototype

// Properties inherited from Function:
String.arity
String.caller
String.constructor
String.length
String.name

// Methods
String.fromCharCode()
String.fromCodePoint()

// Methods inherited from Function:
String.apply()
String.call()
String.toSource()
String.toString()

/**
 * String instances
 */

// Properties
String.prototype.constructor
String.prototype.length
String.prototype.N

// Properties inherited from Object:
String.prototype.__parent__
String.prototype.__proto__

// Methods unrelated to HTML
String.prototype.charAt()
String.prototype.charCodeAt()
String.prototype.codePointAt() // Experimantal
String.prototype.concat()
String.prototype.contains() // Experimantal
String.prototype.endsWith() // Experimental
String.prototype.indexOf()
String.prototype.lastIndexOf()
String.prototype.localeCompare()
String.prototype.match()
String.prototype.quote() // Not standardized
String.prototype.repeat() // Experimantal
String.prototype.replace()
String.prototype.search()
String.prototype.slice()
String.prototype.split()
String.prototype.startsWith() // Experimental
String.prototype.substr()
String.prototype.substring()
String.prototype.toLocaleLowerCase()
String.prototype.toLocaleUpperCase()
String.prototype.toLowerCase()
String.prototype.toSource() // Not standardized
String.prototype.toString()
String.prototype.toUpperCase()
String.prototype.trim()
String.prototype.trimLeft() // Not standardized
String.prototype.trimRight() // Not standardized
String.prototype.valueOf()

// HTML wrapper methods
String.prototype.anchor()
String.prototype.big()
String.prototype.blink()
String.prototype.bold()
String.prototype.fixed()
String.prototype.fontcolor()
String.prototype.fontsize()
String.prototype.italics()
String.prototype.link()
String.prototype.small()
String.prototype.strike()
String.prototype.sub()
String.prototype.sup()

// Methods inherited from Object:
String.prototype.__defineGetter__()
String.prototype.__defineSetter__()
String.prototype.hasOwnProperty()
String.prototype.isPrototypeOf()
String.prototype.__lookupGetter__()
String.prototype.__lookupSetter__()
String.prototype.__noSuchMethod__()
String.prototype.propertyIsEnumerable()
String.prototype.toLocaleString()
String.prototype.unwatch()
String.prototype.watch()



// ==========================
// === Web API Interfaces ===
// ==========================
//
// Source:
// https://developer.mozilla.org/en-US/docs/Web/API


/**
 * The HTMLCanvasElement object
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement
 */

// Properties
HTMLCanvasElement.height
HTMLCanvasElement.width

// Methods
HTMLCanvasElement.getContext()
HTMLCanvasElement.supportsContext() // Experimental
HTMLCanvasElement.setContext() // Experimental
HTMLCanvasElement.transferControlToProxy() // Experimental
HTMLCanvasElement.toDataURL()
HTMLCanvasElement.toDataURLHD() // Experimental
HTMLCanvasElement.toBlob()
HTMLCanvasElement.toBlobHD() // Experimental

/**
 * The MouseEvent object
 * Source:
 * https://developer.mozilla.org/en-US/docs/Web/API/MouseEvent
 */

// Properties
MouseEvent.altKey
MouseEvent.button
MouseEvent.buttons
MouseEvent.clientX
MouseEvent.clientY
MouseEvent.ctrlKey
MouseEvent.metaKey
MouseEvent.movementX
MouseEvent.movementY
MouseEvent.relatedTarget
MouseEvent.screenX
MouseEvent.screenY
MouseEvent.shiftKey
MouseEvent.mozPressure // Not standardized
MouseEvent.mozInputSource // Not standardized

// Methods
MouseEvent.getModifierState() 
do {
	moveBy();
	400576
}

//Regex expressions

hello =/test/;
geet = /a+b[/]/gm;
hi = /match\//;
var re = /ab+c/gi; 
var re = new RegExp("ab+c");
return string.replace( /([.*+?^=!:${}()|\[\]\/\\])/, "\\$1");

var str = "Mr Blue has a blue house and a blue car\\.\..";
var res = str.replace(/blue/gi, "red");
var res2 = str.match.replace(/ij/gi,"'");
foo = bar.replace( ")" , "" );
var res = str.search.match(/blue/gi);
var res = str.search().match().toString("hello");
var res = str.match.search.match.toString('hello');
var res = str.toString.toString('hello');

var myArray = /d(b+)d/g.exec("cdbbdbsbz");
console.log("The value of lastIndex is " +  /d(b+)d/g.lastIndex);

var re = /(?:\d{3}|\(\d{3}\))([-\/\.])\d{3}\1\d{4}/; 

trimwhitespaces: function(str) {
        return str.replace(/^\s\s*/, '').replace(/\s\s*$/, '');
}

var tststring = toString(ds) + true; var good = true; 

= window.document.addEventListener(true)
var testo =  window.document.style;
String.body.height
prototype
Number.prototype

window.body.height
window.window 
var foo = document.body.textAnchor;
window[0]

a = a + 1;
a=a+1;
123456789
a=123456789;


