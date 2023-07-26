#!/usr/bin/env node

// calc   ::= atom
// atom   ::= number | unary atom | atom binary atom
// number ::= "0123456789"
// unary  ::= "+-"
// binary ::= "+-*/^"

// token types:
//  - "number",   value: the number itself
//  - "operator", value: the character (one of "+-*/^")
//  - "open-par"
//  - "close-par"
//  - "end"

// node types:
//  - "number", value: the number itself
//  - "unary",  expr: a node
//  - "binary", left: a node, opChar: the character, right: a node
//  - "par",    expr: a node

var input = "";
var index = 0;

function isNumberChar(char) {
  return "0123456789".includes(char);
}

function opPrecedence(opToken) {
  if ("+" == opToken.value || "-" == opToken.value) {
    return 1;
  }
  if ("*" == opToken.value || "/" == opToken.value) {
    return 2;
  }
  if ("^" == opToken.value) { // out of scope: left associativity
    return 3;
  }
  throw "unreachable: precedence for operator: " + opToken.value;
}

function nextToken() {
  if (index < input.length) {
    var char = input.at(index);
    while (" " == char) {
      index = index + 1;
      char = input.at(index);
    }

    if ("+" == char || "-" == char || "*" == char || "/" == char || "^" == char) {
      index = index + 1;
      return {type: "operator", value: char};
    }

    if ("(" == char) {
      index = index + 1;
      return {type: "open-par"};
    }
    if (")" == char) {
      index = index + 1;
      return {type: "close-par"};
    }

    if (isNumberChar(char)) {
      var complete = "";
      do {
        complete = complete + char;
        index = index + 1;
        char = input.at(index);
      } while (isNumberChar(char));
      return {type: "number", value: parseInt(complete)};
    }

    throw "error: unknown character: " + char;
  }

  return {type: "end"};
}

function nextAtom() {
  var token = nextToken();

  if ("number" == token.type) {
    return {type: "number", value: token.value};
  }

  if ("operator" == token.type) {
    return {type: "unary", opChar: token.value, expr: nextAtom()};
  }

  if ("open-par" == token.type) {
    var firstAtom = nextAtom();
    var opToken = nextToken();

    if ("close-par" == opToken.type || "end" == opToken.type) {
      // (first .)
      return {type: "par", expr: firstAtom};
    }

    if ("operator" == opToken.type) {
      // (first [op] ...)
      return {type: "par", expr: buildTree(firstAtom, opToken)};
    }

    throw "error: expected operator, got: " + token.type;
  }

  throw "error: expected atom, got: " + token.type;
}

// left [lastOp] ...
function buildTree(leftNode, lastOpToken) {
  var rightAtom = nextAtom();
  var nextOpToken = nextToken();
  // left [lastOp] right [nextOp] ...

  if ("close-par" == nextOpToken.type || "end" == nextOpToken.type) {
    // (left [lastOp] right) .
    return {type: "binary", left: leftNode, opChar: lastOpToken.value, right: rightAtom};
  }

  if ("operator" == nextOpToken.type) {
    if (opPrecedence(lastOpToken) < opPrecedence(nextOpToken)) {
      // left [lastOp] (right [nextOp] ...)
      return {type: "binary", left: leftNode, opChar: lastOpToken.value, right: buildTree(rightAtom, nextOpToken)};
    }
    else {
      // (left [lastOp] right) [nextOp] ...
      return buildTree({type: "binary", left: leftNode, opChar: lastOpToken.value, right: rightAtom}, nextOpToken);
    }
  }

  throw "error: expected operator, got: " + nextOpToken.type;
}

function computeResult(node) {
  if ("number" == node.type) {
    return node.value;
  }

  if ("unary" == node.type) {
    var op = node.opChar;
    var exprResult = computeResult(node.expr);

    if ("+" == op) return exprResult;
    if ("-" == op) return -exprResult;
  }

  if ("binary" == node.type) {
    var leftResult = computeResult(node.left);
    var op = node.opChar;
    var rightResult = computeResult(node.right);

    if ("+" == op) { return leftResult + rightResult; }
    if ("-" == op) { return leftResult - rightResult; }
    if ("*" == op) { return leftResult * rightResult; }
    if ("/" == op) { return leftResult / rightResult; }
    if ("^" == op) { return leftResult ** rightResult; }
  }

  if ("par" == node.type) {
    return computeResult(node.expr);
  }

  throw "unreachable: computing result for node: " + node.type;
}

function lineEntered(line) {
  input = line;
  index = 0;

  var firstAtom = nextAtom();
  var opToken = nextToken();

  var root = undefined;
  if ("end" == opToken.type) {
    root = firstAtom;
  }
  else if ("operator" == opToken.type) {
    root = buildTree(firstAtom, opToken);
  }
  else {
    throw "error: expected operator, got: " + opToken.type;
  }

  // use -d to also print the parsed AST
  if ("-d" == process.argv.at(-1)) {
    console.dir(root, {depth: 99});
  }

  var result = computeResult(root);
  console.log("result:", result);
}

// end

require("node:readline")
  .createInterface({
    input: process.stdin,
    output: process.stdout,
    prompt: process.stdin.isTTY ? "? = " : "",
  })
  .on("line", function (line) {
    try {
      lineEntered(line);
    } catch (err) {
      console.error(err);
    }
    this.prompt();
  })
  .prompt();
