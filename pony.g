grammar pony;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  use* (typealias | class_)*
  ;

use
  :  'use' STRING ('as' ID)?
  ;

typealias
  :  'type' ID 'is' type
  ;

class_
  :  ('trait' | 'data' | 'class' | 'actor') ID type_params? cap? ('is' types)? members
  ;

members
  :  field* constructor* behaviour* function*
  ;

field
  :  ('var' | 'let') ID oftype? (assign expr)?
  ;

constructor
  :  'new' ID type_params? params '?'? body?
  ;

function
  :  'fun' cap ID type_params? params oftype? '?'? body?
  ;

behaviour
  :  'be' ID type_params? params body?
  ;

oftype
  :  ':' type
  ;

types
  :  type (',' type)*
  ;

type
  :  type_expr ('->' type_expr)*
  ;

type_expr
  :  '(' type (typeop type)* ')' // union, isect or tuple
  |  ID ('.' ID)? type_args? cap? '^'? // nominal type
  |  '{' fun_type* '}' cap? '^'? // structural type
  |  'this' // only used for viewpoint adaptation
  ;

typeop
  :  '|' | '&' | ',' // union, intersection, tuple
  ;

fun_type
  :  'be' ID? type_params? '(' types? ')'
  |  'fun' cap ID? type_params? '(' types? ')' oftype? '?'?
  ;

// the @ is a cheat: means the symbol "not on a new line"
// without the @, it could be on a new line or not
type_params
  :  '@[' type_param (',' type_param)* ']'
  ;

type_param
  :  ID oftype? (assign type)?
  ;

type_args
  :  '@[' type (',' type)* ']'
  ;

cap
  :  'iso' | 'trn' | 'ref' | 'val' | 'box' | 'tag'
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

param
  :  ID oftype (assign seq)?
  ;

body
  :  '=>' seq
  ;

seq
  :  expr+
  ;

expr
  :  (binary
  |  'return' binary
  |  'break' binary
  |  'continue'
  |  'error'
  )  ';'?
  ;

binary
  :  term (binop term)*
  ;

term
  :  local
  |  control
  |  postfix
  |  unop term
  ;

local
  :  ('var' | 'let') idseq oftype?
  ;

control
  :  'if' seq 'then' seq ('elseif' seq 'then' seq)* ('else' seq)? 'end'
  |  'match' seq case* ('else' seq)? 'end'
  |  'while' seq 'do' seq ('else' seq)? 'end'
  |  'repeat' seq 'until' seq 'end'
  |  'for' idseq oftype? 'in' seq 'do' seq ('else' seq)? 'end'
  |  'try' seq ('else' seq)? ('then' seq)? 'end'
  ;

case
  :  '|' seq? ('as' idseq oftype)? ('where' seq)? body?
  ;

postfix
  :  atom
  (  '.' (ID | INT) // member or tuple component
  |  '!' ID // partial application, syntactic sugar
  |  type_args // type arguments
  |  call // method arguments
  )*
  ;

call
  :  '@' '(' positional? named? ')'
  ;

atom
  :  INT
  |  FLOAT
  |  STRING
  |  ID
  |  'this'
  |  tuple
  |  array
  |  object
  ;

idseq
  :  ID | '(' ID (',' ID)* ')'
  ;

tuple
  :  '(' positional ')'
  ;

array
  :  '[' positional? named? ']'
  ;

object
  :  '{' ('is' types)? members '}'
  ;

positional
  :  seq (',' seq)*
  ;

named
  :  'where' term assign seq (',' term assign seq)*
  ;

unop
  :  'not' | '-' | 'consume' | 'recover'
  ;

binop
  :  'and' | 'or' | 'xor' // logic
  |  '+' | '@-' | '*' | '/' | '%' // arithmetic
  |  '<<' | '>>' // shift
  |  'is' | 'isnt' | '==' | '!=' | '<' | '<=' | '>=' | '>' // comparison
  |  assign
  ;

assign
  :  '='
  ;

/* Precedence?
1. * / %
2. + -
3. << >> // same as C, but confusing?
4. < <= => >
5. == !=
6. and
7. xor
8. or
9. =
*/

// Lexer

ID
  :  (LETTER | '_') (LETTER | DIGIT | '_' | '\'')*
  ;

INT
  :  DIGIT+
  |  '0' 'x' HEX+
  |  '0' 'o' OCTAL+
  |  '0' 'b' BINARY+
  ;

FLOAT
  :  DIGIT+ ('.' DIGIT+)? EXP?
  ;

LINECOMMENT
  :  '//' ~('\n' | '\r')* '\r'? '\n' {$channel=HIDDEN;}
  ;

NESTEDCOMMENT
  :  '/*' ( ('/*') => NESTEDCOMMENT | ~'*' | '*' ~'/')* '*/'
  ;

WS
  :  ' ' | '\t' | '\r' | '\n'
  ;

STRING
  :  '"' ( ESC | ~('\\'|'"') )* '"'
  |  '"""' ~('"""')* '"""'
  ;

fragment
EXP
  :  ('e' | 'E') ('+' | '-')? DIGIT+
  ;

fragment
LETTER
  :  'a'..'z' | 'A'..'Z'
  ;

fragment
BINARY
  :  '0'..'1'
  ;

fragment
OCTAL
  :  '0'..'7'
  ;

fragment
DIGIT
  :  '0'..'9'
  ;

fragment
HEX
  :  DIGIT | 'a'..'f' | 'A'..'F'
  ;

fragment
ESC
  :  '\\' ('a' | 'b' | 'e' | 'f' | 'n' | 'r' | 't' | 'v' | '\"' | '\\' | '0')
  |  HEX_ESC
  |  UNICODE_ESC
  |  UNICODE2_ESC
  ;

fragment
HEX_ESC
  :  '\\' 'x' HEX HEX
  ;

fragment
UNICODE_ESC
  :  '\\' 'u' HEX HEX HEX HEX
  ;

fragment
UNICODE2_ESC
  :  '\\' 'U' HEX HEX HEX HEX HEX HEX
  ;
