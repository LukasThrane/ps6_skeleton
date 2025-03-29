%{
#include "vslc.h"

// State variables from the flex generated scanner
extern int yylineno;  // The line currently being read
extern char yytext[]; // The text of the last consumed lexeme

// The main flex driver function used by the parser
int yylex(void);

// The function called by the parser when errors occur
int yyerror(const char *error)
{
  fprintf(stderr, "%s on line %d\n", error, yylineno);
  exit(EXIT_FAILURE);
}

// Helper macros for creating nodes
#define N0C(type) \
  node_create( (type), 0 )
#define N1C(type, child0) \
  node_create( (type), 1, (child0) )
#define N2C(type, child0, child1) \
  node_create( (type), 2, (child0), (child1) )
#define N3C(type, child0, child1, child2) \
  node_create( (type), 3, (child0), (child1), (child2) )
%}

%token FUNC PRINT RETURN BREAK IF THEN ELSE WHILE DO VAR
%token NUMBER_TOKEN IDENTIFIER_TOKEN STRING_TOKEN

// Use operator precedence to ensure order of operations is correct
%left '=' '!'
%left '<' '>'
%left '+' '-'
%left '*' '/'
%right UNARY_OPERATORS

// Resolve the nested if-if-else ambiguity with precedence
%nonassoc IF THEN
%nonassoc ELSE

%%
program :
      global_list { root = $1; }
    ;
global_list :
      global { $$ = N1C(LIST, $1); }
    | global_list global { $$ = append_to_list_node($1, $2); }
    ;
global :
      function { $$ = $1; }
    | global_declaration { $$ = $1; }
    ;
global_declaration :
      VAR global_variable_list { $$ = N1C(GLOBAL_DECLARATION, $2); }
    ;
global_variable_list :
      global_variable { $$ = N1C(LIST, $1); }
    | global_variable_list ',' global_variable { $$ = append_to_list_node($1, $3); }
    ;
global_variable :
      identifier { $$ = $1; }
    | array_indexing { $$ = $1; }
    ;
array_indexing:
      identifier '[' expression ']' { $$ = N2C(ARRAY_INDEXING, $1, $3); }
    ;
variable_list :
      identifier { $$ = N1C(LIST, $1); }
    | variable_list ',' identifier { $$ = append_to_list_node($1, $3); }
    ;
local_declaration :
      VAR variable_list { $$ = $2; }
    ;
local_declaration_list :
      local_declaration { $$ = N1C(LIST, $1); }
    | local_declaration_list local_declaration { $$ = append_to_list_node($1, $2); }
    ;
parameter_list :
     /* epsilon */ { $$ = N0C(LIST); }
    | variable_list { $$ = $1; }
    ;
function :
      FUNC identifier '(' parameter_list ')' statement
        { $$ = N3C(FUNCTION, $2, $4, $6); }
    ;
statement :
      assignment_statement { $$ = $1; }
    | return_statement { $$ = $1; }
    | print_statement { $$ = $1; }
    | if_statement { $$ = $1; }
    | while_statement { $$ = $1; }
    | break_statement { $$ = $1; }
    | function_call { $$ = $1; }
    | block { $$ = $1; }
    ;
block :
      '{' local_declaration_list statement_list '}'
        { $$ = N2C(BLOCK, $2, $3); }
    | '{' statement_list '}'
        { $$ = N1C(BLOCK, $2); }
    ;
statement_list :
      statement { $$ = N1C(LIST, $1); }
    | statement_list statement { $$ = append_to_list_node($1, $2); }
    ;
assignment_statement :
      identifier '=' expression { $$ = N2C(ASSIGNMENT_STATEMENT, $1, $3); }
    | array_indexing '=' expression { $$ = N2C(ASSIGNMENT_STATEMENT, $1, $3); }
    ;
return_statement :
      RETURN expression
        { $$ = N1C(RETURN_STATEMENT, $2); }
    ;
print_statement :
      PRINT print_list
        { $$ = N1C(PRINT_STATEMENT, $2); }
    ;
print_list :
      print_item { $$ = N1C(LIST, $1); }
    | print_list ',' print_item { $$ = append_to_list_node($1, $3); }
    ;
print_item :
      expression { $$ = $1; }
    | string { $$ = $1; }
    ;
break_statement :
      BREAK { $$ = N0C(BREAK_STATEMENT); }
    ;
if_statement :
      IF expression THEN statement
        { $$ = N2C(IF_STATEMENT, $2, $4); }
    | IF expression THEN statement ELSE statement
        { $$ = N3C(IF_STATEMENT, $2, $4, $6); }
    ;
while_statement :
      WHILE expression DO statement
        { $$ = N2C(WHILE_STATEMENT, $2, $4); }
    ;
expression :
      expression '=' '=' expression
        {
          $$ = N2C(OPERATOR, $1, $4);
          $$->data.operator = "==";
        }
    | expression '!' '=' expression
        {
          $$ = N2C(OPERATOR, $1, $4);
          $$->data.operator = "!=";
        }
    | expression '<' expression
        {
          $$ = N2C(OPERATOR, $1, $3);
          $$->data.operator = "<";
        }
    | expression '<' '=' expression
        {
          $$ = N2C(OPERATOR, $1, $4);
          $$->data.operator = "<=";
        }
    | expression '>' expression
        {
          $$ = N2C(OPERATOR, $1, $3);
          $$->data.operator = ">";
        }
    | expression '>' '=' expression
        {
          $$ = N2C(OPERATOR, $1, $4);
          $$->data.operator = ">=";
        }
    | expression '+' expression
        {
          $$ = N2C(OPERATOR, $1, $3);
          $$->data.operator = "+";
        }
    | expression '-' expression
        {
          $$ = N2C(OPERATOR, $1, $3);
          $$->data.operator = "-";
        }
    | expression '*' expression
        {
          $$ = N2C(OPERATOR, $1, $3);
          $$->data.operator = "*";
        }
    | expression '/' expression
        {
          $$ = N2C(OPERATOR, $1, $3);
          $$->data.operator = "/";
        }
    | '-' expression %prec UNARY_OPERATORS
        {
          $$ = N1C(OPERATOR, $2);
          $$->data.operator = "-";
        }
    | '!' expression %prec UNARY_OPERATORS
        {
          $$ = N1C(OPERATOR, $2);
          $$->data.operator = "!";
        }
    | '(' expression ')' { $$ = $2; }
    | number { $$ = $1; }
    | identifier { $$ = $1; }
    | array_indexing { $$ = $1; }
    | function_call { $$ = $1; }
    ;
function_call :
      identifier '(' argument_list ')' { $$ = N2C(FUNCTION_CALL, $1, $3); }
argument_list :
      expression_list { $$ = $1; }
    | /* epsilon */   { $$ = N0C(LIST); }
    ;
expression_list :
      expression { $$ = N1C(LIST, $1); }
    | expression_list ',' expression { $$ = append_to_list_node($1, $3); }
    ;
identifier :
      IDENTIFIER_TOKEN
      {
        $$ = N0C(IDENTIFIER);
        // Allocate a copy of yytext to keep in the syntax tree as data
        $$->data.identifier = strdup(yytext);
      }
number :
      NUMBER_TOKEN
      {
        $$ = N0C(NUMBER_LITERAL);
        $$->data.number_literal = strtol(yytext, NULL, 10);
      }
string :
      STRING_TOKEN
      {
        $$ = N0C(STRING_LITERAL);
        $$->data.string_literal = strdup(yytext);
      }
%%
