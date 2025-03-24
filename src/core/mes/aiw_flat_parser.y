%define api.prefix {aiw_mf_}
%define parse.error detailed
%define parse.lac full

%{

#include "src/core/mes/flat_parser.h"
#include "aiw_flat_parser.tab.h"

%}

%union {
    int token;
    string string;
    struct mes_expression *expression;
    mes_expression_list arguments;
    struct aiw_mes_menu_case menu_case;
    aiw_menu_table cases;
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
%token  <token>		VAR4 VAR16 VAR32 ARROW BYTE WORD DWORD RANDOM JZ GOTO
%token  <token>		JUMP CALL MENUEXEC FUNCTION RETURN DEFPROC DEFMENU
%token  <token>		SYSVAR CASE OP_0x35 OP_0x37 OP_0xFE

%type   <program>	stmts str
%type   <statement>	stmt
%type   <arguments>	exprs
%type   <parameter>	param
%type   <parameters>	params param_list
%type   <expression>	expr primary_expr mul_expr add_expr rel_expr eq_expr bitand_expr
%type   <expression>	xor_expr bitior_expr and_expr or_expr
%type   <menu_case>	case
%type   <cases>		cases

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
	  { $$ = aiw_mf_parse_string_literal($1); }
	| IDENTIFIER ':' STRING_LITERAL ';'
	  { $$ = aiw_mf_parse_string_literal($3); mf_push_label($1, vector_A($$, 0)); }
	;

stmt
	: IDENTIFIER ':' stmt
	  { mf_push_label($1, $3); $$ = $3; }
	| IDENTIFIER params ';'
	  { $$ = aiw_mf_parse_builtin($1, $2); }
	| RETURN ';'
	  { $$ = aiw_mes_stmt_end(); }
	| VAR4 '[' expr ']' '=' exprs ';'
	  { $$ = aiw_mes_stmt_set_flag($3, $6); }
	| VAR16 '[' expr ']' '=' exprs ';'
	  { $$ = aiw_mes_stmt_set_var16($3, $6); }
	| SYSVAR '[' expr ']' '=' exprs ';'
	  { $$ = aiw_mes_stmt_set_sysvar($3, $6); }
	| VAR32 '[' I_CONSTANT ']' '=' expr ';'
	  { $$ = aiw_mes_stmt_set_var32(mf_parse_u8($3), $6); }
	| VAR32 '[' I_CONSTANT ']' ARROW BYTE '[' expr ']' '=' exprs ';'
	  { $$ = aiw_mes_stmt_ptr_set8(mf_parse_u8($3), $8, $11); }
	| VAR32 '[' I_CONSTANT ']' ARROW WORD '[' expr ']' '=' exprs ';'
	  { $$ = aiw_mes_stmt_ptr_set16(mf_parse_u8($3), $8, $11); }
	| JZ expr IDENTIFIER ';'
	  { $$ = aiw_mes_stmt_jz($2); mf_push_label_ref($$, $3); }
	| GOTO IDENTIFIER ';'
	  { $$ = aiw_mes_stmt_jmp(); mf_push_label_ref($$, $2); }
	| JUMP params ';'
	  { $$ = _aiw_mes_stmt_call(AIW_MES_STMT_JMP_MES, $2); }
	| CALL params ';'
	  { $$ = aiw_mf_stmt_call($2); }
	| DEFPROC expr IDENTIFIER ';'
	  { $$ = aiw_mes_stmt_defproc($2); mf_push_label_ref($$, $3); }
	| MENUEXEC exprs ';'
	  { $$ = aiw_mes_stmt_menuexec($2); }
	| DEFMENU expr '{' cases '}'
	  { $$ = aiw_mes_stmt_defmenu($2, $4); }
        | OP_0x35 I_CONSTANT I_CONSTANT ';'
          { $$ = aiw_mes_stmt_0x35(mf_parse_u16($2), mf_parse_u16($3)); }
        | OP_0x37 I_CONSTANT ';'
          { $$ = aiw_mes_stmt_0x37(mf_parse_u32($2)); }
        | OP_0xFE ';'
          { $$ = aiw_mes_stmt_0xfe(); }
	;

cases
	: case { $$ = mf_push_case((aiw_menu_table)vector_initializer, $1); }
	| cases case { $$ = mf_push_case($1, $2); }
	;

case
	: CASE '{' stmts '}' { $$ = aiw_mes_menu_case(NULL, $3); }
	| CASE expr '{' stmts '}' { $$ = aiw_mes_menu_case($2, $4); }
	;

exprs
	: expr { $$ = mf_push_expression((mes_expression_list)vector_initializer, $1); }
	| exprs ',' expr { $$ = mf_push_expression($1, $3); }
	;

primary_expr
	: I_CONSTANT { $$ = mf_parse_constant($1); }
	| VAR4 '[' expr ']' { $$ = aiw_mes_expr_var4($3); }
	| VAR16 '[' expr ']' { $$ = aiw_mes_expr_var16($3); }
	| SYSVAR '[' expr ']' { $$ = aiw_mes_expr_sysvar($3); }
	| VAR32 '[' I_CONSTANT ']' { $$ = aiw_mes_expr_var32(mf_parse_u8($3)); }
	| VAR32 '[' I_CONSTANT ']' ARROW BYTE '[' expr ']'
	  { $$ = aiw_mes_expr_ptr_get8(mf_parse_u8($3), $8); }
	| RANDOM '(' I_CONSTANT ')' { $$ = aiw_mes_expr_random(mf_parse_u16($3)); }
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
