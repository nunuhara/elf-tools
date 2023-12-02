/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

%{

#pragma GCC diagnostic ignored "-Wunused-function"

#include "s4_parser.tab.h"
#include "nulib.h"
#include "nulib/string.h"

#define RETURN_STRING(tok_type) \
    s4_script_lval.string = string_new_len(s4_script_text, s4_script_leng); return tok_type

%}

%option noyywrap
%option yylineno
%option prefix="s4_script_"

%%

"#".*         { /* line comment */ }
[ \t\r\n]     { /* whitespace */ }
":"           { return ':'; }
";"           { return ';'; }
","           { return ','; }
"("           { return '('; }
")"           { return ')'; }
"+"           { return '+'; }
"@"           { return '@'; }
"->"          { return ARROW; }
[0-9]+        { RETURN_STRING(I_CONSTANT); }
"STREAM"      { return STREAM; }
"NOOP"        { return NOOP; }
"CHECK_STOP"  { return CHECK_STOP; }
"STALL"       { return STALL; }
"RESET"       { return RESET; }
"HALT"        { return HALT; }
"LOOP_START"  { return LOOP_START; }
"LOOP_END"    { return LOOP_END; }
"LOOP2_START" { return LOOP2_START; }
"LOOP2_END"   { return LOOP2_END; }
"COPY"        { return COPY; }
"COPY_MASKED" { return COPY_MASKED; }
"SWAP"        { return SWAP; }
"SET_COLOR"   { return SET_COLOR; }
"COMPOSE"     { return COMPOSE; }
"FILL"        { return FILL; }
"SET_PALETTE" { return SET_PALETTE; }

. return INVALID_TOKEN;

%%
