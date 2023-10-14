%define api.prefix {mf_}
%define parse.error detailed
%define parse.lac full

%{

#include "src/core/mes/flat_parser.h"
#include "flat_parser.tab.h"

%}

%union {
    int token;
    string string;
    struct mes_expression *expression;
    mes_qname path;
    mes_expression_list arguments;
    mes_parameter_list parameters;
    struct mes_parameter parameter;
    struct mes_statement *statement;
    mes_statement_list program;
}

%code requires {
    #include "mes.h"
}

%token  <string>	IDENTIFIER I_CONSTANT STRING_LITERAL
%token  <token>		LPAREN RPAREN PLUS MINUS MUL DIV MOD INVALID_TOKEN
%token  <token>		AND_OP OR_OP LE_OP GE_OP EQ_OP NE_OP
%token  <token>		VAR4 VAR16 VAR32 ARROW BYTE WORD DWORD RANDOM SYSTEM
%token  <token>		JZ GOTO JUMP CALL UTIL LINE MENUEXEC
%token  <token>		FUNCTION RETURN DEFPROC DEFMENU

%type   <program>	stmts str
%type   <statement>	stmt
%type   <arguments>	exprs
%type   <parameter>     param
%type   <parameters>    params param_list
%type   <expression>	expr primary_expr mul_expr add_expr rel_expr eq_expr bitand_expr
%type   <expression>	xor_expr bitior_expr and_expr or_expr
%type   <path>		path

%start program

%%

program
	: stmts { mf_program($1); }
	;

stmts
	: stmt { $$ = mf_push_statement((mes_statement_list)vector_initializer, $1); }
	| stmts stmt { $$ = mf_push_statement($1, $2); }
	// XXX: string literal can expand to multiple statements (mixed zenkaku/hankaku)
	| str { $$ = $1; }
	| stmts str { $$ = mf_append_statements($1, $2); }
	;

str
	: STRING_LITERAL ';'
	  { $$ = mf_parse_string_literal($1); }
	| IDENTIFIER ':' STRING_LITERAL ';'
	  { $$ = mf_parse_string_literal($3); mf_push_label($1, vector_A($$, 0)); }
	;

stmt
	: IDENTIFIER ':' stmt
	  { mf_push_label($1, $3); $$ = $3; }
	| RETURN ';'
	  { $$ = mes_stmt_end(); }
	| VAR4 '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_setrbx($3, $6); }
	| VAR16 '[' I_CONSTANT ']' '=' exprs ';'
	  { $$ = mes_stmt_setv(mf_parse_u8($3), $6); }
	| VAR32 '[' I_CONSTANT ']' '=' exprs ';'
	  { $$ = mes_stmt_setrd(mf_parse_u8($3), $6); }
	| VAR16 '[' I_CONSTANT ']' ARROW BYTE '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_setac(mf_parse_u8($3), $8, $11); }
	| VAR16 '[' I_CONSTANT ']' ARROW WORD '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_seta_at(mf_parse_u8($3) + 1, $8, $11); }
	| VAR32 '[' I_CONSTANT ']' ARROW BYTE '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_setab(mf_parse_u8($3), $8, $11); }
	| VAR32 '[' I_CONSTANT ']' ARROW WORD '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_setaw(mf_parse_u8($3), $8, $11); }
	| VAR32 '[' I_CONSTANT ']' ARROW DWORD '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_setad(mf_parse_u8($3) + 1, $8, $11); }
	| JZ expr IDENTIFIER ';'
	  { $$ = mes_stmt_jz($2); mf_push_label_ref($$, $3); }
	| GOTO IDENTIFIER ';'
	  { $$ = mes_stmt_jmp(); mf_push_label_ref($$, $2); }
	| JUMP params ';'
	  { $$ = mes_stmt_goto($2); }
	| CALL params ';'
	  { $$ = mf_stmt_call($2); }
	| UTIL params ';'
	  { $$ = mes_stmt_util($2); }
	| LINE I_CONSTANT ';'
	  { $$ = mes_stmt_line(mf_parse_u8($2)); }
	| MENUEXEC ';'
	  { $$ = mes_stmt_menus(); }
	| SYSTEM '.' path params ';'
	  { $$ = mf_stmt_named_sys($3, $4); }
	| SYSTEM '.' IDENTIFIER '=' exprs ';'
	  { $$ = mf_stmt_sys_named_var_set($3, $5); }
	| SYSTEM '.' VAR16 '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_sys_var16_set($5, $8); }
	| SYSTEM '.' VAR32 '[' expr ']' '=' exprs ';'
	  { $$ = mes_stmt_sys_var32_set($5, $8); }
	| DEFPROC expr IDENTIFIER ';'
	  { $$ = mes_stmt_procd($2); mf_push_label_ref($$, $3); }
	| DEFMENU params IDENTIFIER ';'
	  { $$ = mes_stmt_menui($2); mf_push_label_ref($$, $3); }
	;

path
	: IDENTIFIER
	  { $$ = mf_push_qname_ident((mes_qname)vector_initializer, $1); }
	| FUNCTION '[' I_CONSTANT ']'
	  { $$ = mf_push_qname_number((mes_qname)vector_initializer, mf_parse_u8($3)); }
	| path '.' IDENTIFIER
	  { $$ = mf_push_qname_ident($1, $3); }
	| path '.' FUNCTION '[' I_CONSTANT ']'
	  { $$ = mf_push_qname_number($1, mf_parse_u8($5)); }
	;

exprs
	: expr { $$ = mf_push_expression((mes_expression_list)vector_initializer, $1); }
	| exprs ',' expr { $$ = mf_push_expression($1, $3); }
	;

primary_expr
	: I_CONSTANT { $$ = mf_parse_constant($1); }
	| VAR4 '[' expr ']' { $$ = mes_expr_var4($3); }
	| VAR16 '[' I_CONSTANT ']' { $$ = mes_expr_var16(mf_parse_u8($3)); }
	| VAR32 '[' I_CONSTANT ']' { $$ = mes_expr_var32(mf_parse_u8($3)); }
	| VAR16 '[' I_CONSTANT ']' ARROW BYTE '[' expr ']'
	  { $$ = mes_expr_array_index(MES_EXPR_ARRAY16_GET8, mf_parse_u8($3), $8); }
	| VAR16 '[' I_CONSTANT ']' ARROW WORD '[' expr ']'
	  { $$ = mes_expr_array_index(MES_EXPR_ARRAY16_GET16, mf_parse_u8($3), $8); }
	| VAR32 '[' I_CONSTANT ']' ARROW BYTE '[' expr ']'
	  { $$ = mes_expr_array_index(MES_EXPR_ARRAY32_GET8, mf_parse_u8($3), $8); }
	| VAR32 '[' I_CONSTANT ']' ARROW WORD '[' expr ']'
	  { $$ = mes_expr_array_index(MES_EXPR_ARRAY32_GET16, mf_parse_u8($3), $8); }
	| VAR32 '[' I_CONSTANT ']' ARROW DWORD '[' expr ']'
	  { $$ = mes_expr_array_index(MES_EXPR_ARRAY32_GET32, mf_parse_u8($3), $8); }
	| SYSTEM '.' VAR16 '[' expr ']' { $$ = mes_expr_system_var16($5); }
	| SYSTEM '.' VAR32 '[' expr ']' { $$ = mes_expr_system_var32($5); }
	| SYSTEM '.' IDENTIFIER { $$ = mf_expr_named_sysvar($3); }
	| RANDOM '(' expr ')' { $$ = mes_expr_random($3); }
	| '(' expr ')' { $$ = $2; }
	;

mul_expr
	: primary_expr { $$ = $1; }
	| mul_expr '*' primary_expr { $$ = mes_binary_expr(MES_EXPR_MUL, $1, $3); }
	| mul_expr '/' primary_expr { $$ = mes_binary_expr(MES_EXPR_DIV, $1, $3); }
	| mul_expr '%' primary_expr { $$ = mes_binary_expr(MES_EXPR_MOD, $1, $3); }
	;

add_expr
	: mul_expr { $$ = $1; }
	| add_expr '+' mul_expr { $$ = mes_binary_expr(MES_EXPR_PLUS, $1, $3); }
	| add_expr '-' mul_expr { $$ = mes_binary_expr(MES_EXPR_MINUS, $1, $3); }
	;

rel_expr
	: add_expr { $$ = $1; }
	| rel_expr '<' add_expr { $$ = mes_binary_expr(MES_EXPR_LT, $1, $3); }
	| rel_expr '>' add_expr { $$ = mes_binary_expr(MES_EXPR_GT, $1, $3); }
	| rel_expr LE_OP add_expr { $$ = mes_binary_expr(MES_EXPR_LTE, $1, $3); }
	| rel_expr GE_OP add_expr { $$ = mes_binary_expr(MES_EXPR_GTE, $1, $3); }
	;

eq_expr
	: rel_expr { $$ = $1; }
	| eq_expr EQ_OP rel_expr { $$ = mes_binary_expr(MES_EXPR_EQ, $1, $3); }
	| eq_expr NE_OP rel_expr { $$ = mes_binary_expr(MES_EXPR_NEQ, $1, $3); }
	;

bitand_expr
	: eq_expr { $$ = $1; }
	| bitand_expr '&' eq_expr { $$ = mes_binary_expr(MES_EXPR_BITAND, $1, $3); }
	;

xor_expr
	: bitand_expr { $$ = $1; }
	| xor_expr '^' bitand_expr { $$ = mes_binary_expr(MES_EXPR_BITXOR, $1, $3); }
	;

bitior_expr
	: xor_expr { $$ = $1; }
	| bitior_expr '|' xor_expr { $$ = mes_binary_expr(MES_EXPR_BITIOR, $1, $3); }
	;

and_expr
	: bitior_expr { $$ = $1; }
	| and_expr AND_OP bitior_expr { $$ = mes_binary_expr(MES_EXPR_AND, $1, $3); }
	;

or_expr
	: and_expr { $$ = $1; }
	| or_expr OR_OP and_expr { $$ = mes_binary_expr(MES_EXPR_OR, $1, $3); }
	;

expr
	: or_expr { $$ = $1; }
	;

params
	: '(' ')' { $$ = (mes_parameter_list)vector_initializer; }
	| '(' param_list ')' { $$ = $2; }
	;

param_list
	: param { $$ = mf_push_param((mes_parameter_list)vector_initializer, $1); }
	| param_list ',' param { $$ = mf_push_param($1, $3); }
	;

param
	: STRING_LITERAL { $$ = mes_param_str($1); }
	| expr { $$ = mes_param_expr($1); }
	;

%%
