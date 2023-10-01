%define api.prefix {arc_mf_}
%define parse.error detailed
%define parse.lac full

%union {
    int token;
    string string;
    arc_string_list row;
    arc_row_list rows;
}

%code requires {
    #include "nulib/string.h"
    #include "nulib/vector.h"

    typedef vector_t(string) arc_string_list;
    typedef vector_t(arc_string_list) arc_row_list;
}

%{

#include "src/core/arc/manifest_parser.c"

%}

%token	<string>	STRING
%token	<token>		NEWLINE COMMA

%type	<row>		row values options
%type	<rows>		rows

%start file

%%

file    :	STRING options NEWLINE STRING rows end { arc_mf_output = arc_make_manifest($1, $2, $4, $5); }
	;

options :			{ $$ = (arc_string_list)vector_initializer; }
	|	options	STRING	{ $$ = push_string($1, $2); }
	;

rows	:	row      { $$ = make_row_list($1); }
	|	rows row { $$ = push_row($1, $2); }
	;

row	:       NEWLINE values { $$ = $2; }
	;

values	:	STRING              { $$ = make_string_list($1); }
	|	values COMMA STRING { $$ = push_string($1, $3); }
	;


end	:
	|	NEWLINE end
	;

%%
