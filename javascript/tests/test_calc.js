// Tests for javascript/calc.js. Run with: node javascript/tests/test_calc.js
//
// The parser is plain ES5 with no Max dependency, so it can be exercised
// outside Max. Only the [js] glue at the bottom of calc.js is untested here.

var calc = require("../calc.js");

var passed = 0;
var failed = 0;

function near(a, b) {
    return Math.abs(a - b) < 1e-9;
}

function ok(expr, expected) {
    var actual;
    try {
        actual = calc.calculate(expr);
    } catch (e) {
        console.log("FAIL  " + JSON.stringify(expr) + "  threw " + e.name + ": " + e.message);
        failed++;
        return;
    }
    if (near(actual, expected)) {
        passed++;
    } else {
        console.log("FAIL  " + JSON.stringify(expr) + "  expected " + expected + ", got " + actual);
        failed++;
    }
}

function err(expr, expectedName) {
    var actual;
    try {
        actual = calc.calculate(expr);
    } catch (e) {
        if (e.name === expectedName) {
            passed++;
        } else {
            console.log("FAIL  " + JSON.stringify(expr) + "  expected " + expectedName +
                        ", got " + e.name + ": " + e.message);
            failed++;
        }
        return;
    }
    console.log("FAIL  " + JSON.stringify(expr) + "  expected " + expectedName +
                ", but got value " + actual);
    failed++;
}

// --- arithmetic ---
ok("1+1", 2);
ok("2 + 3 * 4", 14);            // precedence
ok("(2 + 3) * 4", 20);
ok("10 - 4 - 3", 3);            // left associative
ok("100 / 5 / 2", 10);
ok("7 % 3", 1);
ok("2 ^ 3 ^ 2", 512);           // right associative: 2^(3^2)
ok("-3 + 5", 2);
ok("--3", 3);
ok("+-+4", -4);
ok("3 * -2", -6);
ok("2 ^ -1", 0.5);

// --- numbers ---
ok("1.5 + 1.5", 3);
ok(".5 * 4", 2);
ok("1e3", 1000);
ok("1.5e-2", 0.015);
ok("  42  ", 42);

// --- constants and functions ---
ok("pi", Math.PI);
ok("e", Math.E);
ok("sqrt(16)", 4);
ok("abs(-7)", 7);
ok("floor(3.9)", 3);
ok("ceil(3.1)", 4);
ok("round(2.5)", 3);
ok("pow(2, 10)", 1024);
ok("min(3, 7)", 3);
ok("max(3, 7)", 7);
ok("sqrt(pow(3,2) + pow(4,2))", 5);
ok("cos(0)", 1);
ok("SQRT(16)", 4);              // case insensitive

// --- errors ---
err("1/0", "ZeroDivisionError");
err("5 % 0", "ZeroDivisionError");
err("1+", "SyntaxError");
err("", "SyntaxError");
err("   ", "SyntaxError");
err("(1+2", "SyntaxError");
err("1 2", "SyntaxError");
err("$", "SyntaxError");
err("foo(1)", "NameError");
err("bar", "NameError");
err("sqrt", "SyntaxError");     // function used as a value
err("pow(1)", "TypeError");     // wrong arity
err("min(1,2,3)", "TypeError");
err("log(-1)", "MathError");    // NaN
err("1e400", "MathError");      // Infinity

// --- no arbitrary code execution ---
// A cell must not be able to reach the host. These are all just unknown names
// or syntax errors, never evaluated JavaScript.
err("max.constructor", "SyntaxError");
err("this", "NameError");
err("[1,2]", "SyntaxError");

console.log("");
console.log(passed + " passed, " + failed + " failed");
process.exit(failed === 0 ? 0 : 1);
