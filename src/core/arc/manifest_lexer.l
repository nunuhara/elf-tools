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

#include <stdio.h>
#include "manifest_parser.tab.h"
#include "nulib.h"
#include "nulib/string.h"

static char string_buf[256];
static char *string_buf_ptr;

extern void arc_mf_error(const char *s);

%}

%option noyywrap
%option yylineno
%option prefix="arc_mf_"

%x str

%%

[ \t\r]        ;
\n             return NEWLINE;
,              return COMMA;
[^,\" \t\r\n]* arc_mf_lval.string = string_new_len(arc_mf_text, arc_mf_leng); return STRING;
\"             string_buf_ptr = string_buf; BEGIN(str);

<str>{
    \" {
        BEGIN(INITIAL);
        *string_buf_ptr = '\0';
        arc_mf_lval.string = string_new_len(string_buf, strlen(string_buf));
        return STRING;
    }

    \n arc_mf_error("Unterminated string literal");

    \\n  *string_buf_ptr++ = '\n';
    \\t  *string_buf_ptr++ = '\t';
    \\r  *string_buf_ptr++ = '\r';
    \\b  *string_buf_ptr++ = '\b';
    \\f  *string_buf_ptr++ = '\f';

    \\(.|\n)  *string_buf_ptr++ = arc_mf_text[1];

    [^\\\n\"]+ {
        char *yptr = arc_mf_text;
        while (*yptr)
            *string_buf_ptr++ = *yptr++;
    }
}

%%
