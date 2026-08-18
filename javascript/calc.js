// calc.js -- a small arithmetic evaluator for the mx-kernel calculator example.
//
// Wired downstream of [route execute], it turns a Jupyter cell into a value and
// emits a complete message for the kernel inlet:
//
//     result <value>
//     result error <ename> <evalue>
//
// Written in ES5 with no `eval`, so it runs in both the classic [js] engine and
// [v8], and so a cell cannot execute arbitrary code. The parser is plain
// JavaScript and is unit tested outside Max -- see tests/test_calc.js.

// ---------------------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------------------

function CalcError(name, message) {
    this.name = name;
    this.message = message;
}

function fail(name, message) {
    throw new CalcError(name, message);
}

function isDigit(c) {
    return c >= "0" && c <= "9";
}

function isNameStart(c) {
    return (c >= "a" && c <= "z") || (c >= "A" && c <= "Z") || c === "_";
}

function tokenize(src) {
    var tokens = [];
    var i = 0;

    while (i < src.length) {
        var c = src.charAt(i);

        if (c === " " || c === "\t" || c === "\n" || c === "\r") {
            i++;
            continue;
        }

        if (isDigit(c) || (c === "." && isDigit(src.charAt(i + 1)))) {
            var start = i;
            var seenDot = false;
            while (i < src.length) {
                var d = src.charAt(i);
                if (isDigit(d)) {
                    i++;
                } else if (d === "." && !seenDot) {
                    seenDot = true;
                    i++;
                } else if ((d === "e" || d === "E") &&
                           (isDigit(src.charAt(i + 1)) ||
                            ((src.charAt(i + 1) === "-" || src.charAt(i + 1) === "+") &&
                             isDigit(src.charAt(i + 2))))) {
                    i += 2;
                } else {
                    break;
                }
            }
            var text = src.substring(start, i);
            var value = parseFloat(text);
            if (isNaN(value)) {
                fail("SyntaxError", "bad number: " + text);
            }
            tokens.push({ type: "number", value: value });
            continue;
        }

        if (isNameStart(c)) {
            var nameStart = i;
            while (i < src.length &&
                   (isNameStart(src.charAt(i)) || isDigit(src.charAt(i)))) {
                i++;
            }
            tokens.push({ type: "name", value: src.substring(nameStart, i).toLowerCase() });
            continue;
        }

        if ("+-*/%^(),".indexOf(c) !== -1) {
            tokens.push({ type: c });
            i++;
            continue;
        }

        fail("SyntaxError", "unexpected character '" + c + "'");
    }

    return tokens;
}

// ---------------------------------------------------------------------------
// Parser: recursive descent, standard precedence
//
//   expr    := term (('+' | '-') term)*
//   term    := power (('*' | '/' | '%') power)*
//   power   := unary ('^' power)?          right associative
//   unary   := ('-' | '+') unary | primary
//   primary := number | name | name '(' args ')' | '(' expr ')'
// ---------------------------------------------------------------------------

var CONSTANTS = {
    pi: Math.PI,
    e: Math.E
};

var FUNCTIONS = {
    sqrt: { arity: 1, fn: Math.sqrt },
    abs: { arity: 1, fn: Math.abs },
    sin: { arity: 1, fn: Math.sin },
    cos: { arity: 1, fn: Math.cos },
    tan: { arity: 1, fn: Math.tan },
    exp: { arity: 1, fn: Math.exp },
    log: { arity: 1, fn: Math.log },
    floor: { arity: 1, fn: Math.floor },
    ceil: { arity: 1, fn: Math.ceil },
    round: { arity: 1, fn: Math.round },
    pow: { arity: 2, fn: Math.pow },
    min: { arity: 2, fn: Math.min },
    max: { arity: 2, fn: Math.max }
};

function Parser(tokens) {
    this.tokens = tokens;
    this.pos = 0;
}

Parser.prototype.peek = function () {
    return this.pos < this.tokens.length ? this.tokens[this.pos] : null;
};

Parser.prototype.next = function () {
    return this.tokens[this.pos++];
};

Parser.prototype.accept = function (type) {
    var t = this.peek();
    if (t && t.type === type) {
        this.pos++;
        return true;
    }
    return false;
};

Parser.prototype.expect = function (type) {
    if (!this.accept(type)) {
        var t = this.peek();
        fail("SyntaxError",
             "expected '" + type + "' but found " +
             (t ? (t.type === "number" ? String(t.value) : "'" + (t.value || t.type) + "'")
                : "end of input"));
    }
};

Parser.prototype.parseExpr = function () {
    var left = this.parseTerm();
    for (;;) {
        if (this.accept("+")) {
            left = left + this.parseTerm();
        } else if (this.accept("-")) {
            left = left - this.parseTerm();
        } else {
            return left;
        }
    }
};

Parser.prototype.parseTerm = function () {
    var left = this.parsePower();
    for (;;) {
        if (this.accept("*")) {
            left = left * this.parsePower();
        } else if (this.accept("/")) {
            var divisor = this.parsePower();
            if (divisor === 0) {
                fail("ZeroDivisionError", "division by zero");
            }
            left = left / divisor;
        } else if (this.accept("%")) {
            var modulus = this.parsePower();
            if (modulus === 0) {
                fail("ZeroDivisionError", "modulo by zero");
            }
            left = left % modulus;
        } else {
            return left;
        }
    }
};

Parser.prototype.parsePower = function () {
    var base = this.parseUnary();
    if (this.accept("^")) {
        return Math.pow(base, this.parsePower()); // right associative
    }
    return base;
};

Parser.prototype.parseUnary = function () {
    if (this.accept("-")) {
        return -this.parseUnary();
    }
    if (this.accept("+")) {
        return this.parseUnary();
    }
    return this.parsePrimary();
};

Parser.prototype.parsePrimary = function () {
    var t = this.peek();
    if (!t) {
        fail("SyntaxError", "unexpected end of input");
    }

    if (t.type === "number") {
        this.next();
        return t.value;
    }

    if (t.type === "name") {
        this.next();
        var name = t.value;

        if (this.accept("(")) {
            var spec = FUNCTIONS[name];
            if (!spec) {
                fail("NameError", "unknown function '" + name + "'");
            }
            var args = [];
            if (!this.accept(")")) {
                args.push(this.parseExpr());
                while (this.accept(",")) {
                    args.push(this.parseExpr());
                }
                this.expect(")");
            }
            if (args.length !== spec.arity) {
                fail("TypeError",
                     name + "() takes " + spec.arity + " argument" +
                     (spec.arity === 1 ? "" : "s") + ", got " + args.length);
            }
            return spec.arity === 1 ? spec.fn(args[0]) : spec.fn(args[0], args[1]);
        }

        if (Object.prototype.hasOwnProperty.call(CONSTANTS, name)) {
            return CONSTANTS[name];
        }
        if (Object.prototype.hasOwnProperty.call(FUNCTIONS, name)) {
            fail("SyntaxError", "'" + name + "' is a function -- did you mean " + name + "(...)?");
        }
        fail("NameError", "unknown name '" + name + "'");
    }

    if (this.accept("(")) {
        var value = this.parseExpr();
        this.expect(")");
        return value;
    }

    fail("SyntaxError", "unexpected '" + t.type + "'");
};

// Evaluate an expression, returning a number. Throws CalcError.
function calculate(src) {
    if (src === null || src === undefined) {
        fail("SyntaxError", "empty expression");
    }
    var text = String(src);
    if (text.replace(/\s/g, "") === "") {
        fail("SyntaxError", "empty expression");
    }

    var parser = new Parser(tokenize(text));
    var value = parser.parseExpr();

    if (parser.peek() !== null) {
        var t = parser.peek();
        fail("SyntaxError",
             "unexpected trailing " +
             (t.type === "number" ? String(t.value) : "'" + (t.value || t.type) + "'"));
    }

    if (typeof value !== "number" || !isFinite(value)) {
        fail("MathError", "result is not a finite number");
    }

    return value;
}

// ---------------------------------------------------------------------------
// Max glue. Skipped outside Max, so the parser above can be unit tested.
// ---------------------------------------------------------------------------

if (typeof outlet === "function") {
    outlets = 1;

    // [route execute] emits the cell text as the message selector, so the
    // expression usually arrives as the message name with no arguments.
    // Rebuilding it from both keeps either shape working.
    function anything() {
        var parts = [messagename];
        var args = arrayfromargs(arguments);
        for (var i = 0; i < args.length; i++) {
            parts.push(args[i]);
        }
        evaluate_expression(parts.join(" "));
    }

    function evaluate(/* ... */) {
        evaluate_expression(arrayfromargs(arguments).join(" "));
    }

    function evaluate_expression(text) {
        var value;
        try {
            value = calculate(text);
        } catch (err) {
            var name = err && err.name ? err.name : "CalcError";
            var message = err && err.message ? err.message : String(err);
            // Fails the Jupyter cell rather than returning the word "error".
            outlet(0, "result", ["error", name, message]);
            return;
        }
        outlet(0, "result", [value]);
    }
}

if (typeof module !== "undefined" && module.exports) {
    module.exports = { calculate: calculate, tokenize: tokenize, CalcError: CalcError };
}
