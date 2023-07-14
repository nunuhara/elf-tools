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

#ifndef NULIB_COMMAND_H
#define NULIB_COMMAND_H

#include "nulib.h"
#include <getopt.h>

#define COMMAND_MAX_SUBCOMMANDS 32
#define COMMAND_MAX_OPTIONS 64

struct command_option {
	char *name;
	char short_opt;
	const char *description;
	int has_arg;
	int val;
};

struct command {
	const char *name;
	const char *usage;
	const char *description;
	bool hidden;
	struct command *parent;
	struct command *commands[COMMAND_MAX_SUBCOMMANDS];
	int (*fun)(int, char *[]);
	struct command_option options[COMMAND_MAX_OPTIONS];
};

void command_print_usage(struct command *cmd) attr_nonnull;
int command_getopt(int argc, char *argv[], struct command *cmd) attr_nonnull_arg(3);
_Noreturn void command_usage_error(struct command *cmd, const char *fmt, ...) attr_nonnull_arg(1, 2);
int command_execute(struct command *cmd, int argc, char *argv[]) attr_nonnull_arg(1);

#endif
