/* Copyright (C) 2023 Nunuhara Cabbage <nunuhara@haniwa.technology>
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

U   [\x80-\xbf]
U2  [\xc2-\xdf]
U3  [\xe0-\xef]
U4  [\xf0-\xf4]

UCH {U2}{U}|{U3}{U}{U}|{U4}{U}{U}{U}

O   [0-7]
D   [0-9]
NZ  [1-9]
L   ([a-zA-Z_]|{UCH})
AT  ([a-zA-Z_0-9]|{UCH})
H   [a-fA-F0-9]
HP  (0[xX])
ES  (\\(['"\?\\abfnrtv]|[0-7]{1,3}|x{H}{H}|X{H}{H}{H}{H}))
WS  [ \t\v\f]

%{

#pragma GCC diagnostic ignored "-Wunused-function"

#include "flat_parser.tab.h"
#include "nulib.h"
#include "nulib/string.h"

#define RETURN_STRING(tok_type) \
    mf_lval.string = string_new_len(mf_text, mf_leng); return tok_type

#define RETURN_STRING_LITERAL \
    mf_lval.string = string_new_len(mf_text+1, mf_leng-2); return STRING_LITERAL

%}

%option noyywrap
%option yylineno
%option prefix="mf_"

%%

"//".* { /* consume //-comment */ }

	/* memory-related keywords */
"var4" { return VAR4; }
"var16" { return VAR16; }
"var32" { return VAR32; }
"arg" { return ARG; }
"->" { return ARROW; }
"byte" { return BYTE; }
"word" { return WORD; }
"dword" { return DWORD; }

	/* keywords */
"rand" { return RANDOM; }
"goto" { return GOTO; }
"jump" { return JUMP; }
"call" { return CALL; }
"call_sub" { return CALL_SUB; }
"line" { return LINE; }
"menuexec" { return MENUEXEC; }
"jz" { return JZ; }
"function" { return FUNCTION; }
"return" { return RETURN; }
"defproc" { return DEFPROC; }
"defmenu" { return DEFMENU; }
"defsub" { return DEFSUB; }

"OP_0x17" { return OP_0x17; }
"OP_0x18" { return OP_0x18; }
"OP_0x19" { return OP_0x19; }
"OP_0x1A" { return OP_0x1A; }
"OP_0x1B" { return OP_0x1B; }
"OP_0x1F" { return OP_0x1F; }

	/* namespaces */
"System" { return SYSTEM; }
"Util" { return UTIL; }

{L}{AT}* { RETURN_STRING(IDENTIFIER); }

{HP}{H}+ { RETURN_STRING(I_CONSTANT); }
{NZ}{D}* { RETURN_STRING(I_CONSTANT); }
"0"{O}*  { RETURN_STRING(I_CONSTANT); }

\"([^"\\\n]|{ES})*\" { RETURN_STRING_LITERAL; }

"&&" { return AND_OP; }
"||" { return OR_OP; }
"<=" { return LE_OP; }
">=" { return GE_OP; }
"==" { return EQ_OP; }
"!=" { return NE_OP; }
";" { return ';'; }
":" { return ':'; }
"(" { return '('; }
")" { return ')'; }
"[" { return '['; }
"]" { return ']'; }
"," { return ','; }
"=" { return '='; }
"-" { return '-'; }
"+" { return '+'; }
"*" { return '*'; }
"/" { return '/'; }
"%" { return '%'; }
"<" { return '<'; }
">" { return '>'; }
"&" { return '&'; }
"|" { return '|'; }
"^" { return '^'; }
"." { return '.'; }

\n { /* whitespace */ }
{WS}+ { /* whitespace */ }

. return INVALID_TOKEN;

%%
