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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nulib.h"
#include "nulib/command.h"

void command_print_usage(struct command *cmd)
{
	// flatten command path
	int nr_commands = 0;
	const char *cmd_path[16];
	for (struct command *p = cmd; p; p = p->parent) {
		cmd_path[nr_commands++] = p->name;
	}

	printf("Usage: ");
	for (int i = nr_commands - 1; i >= 0; i--) {
		printf("%s ", cmd_path[i]);
	}
	printf("%s\n", cmd->usage);
	printf("    %s\n", cmd->description);

	if (cmd->fun) {
		// calculate column width
		size_t width = 0;
		for (struct command_option *opt = cmd->options; opt->name; opt++) {
			size_t this_width = strlen(opt->name) + 2;
			if (opt->short_opt)
				this_width += 3;
			if (opt->has_arg == required_argument)
				this_width += 6;
			width = max(width, this_width);
		}

		printf("Command options:\n");
		for (struct command_option *opt = cmd->options; opt->name; opt++) {
			size_t written = 0;
			printf("    ");
			if (opt->short_opt) {
				written += printf("-%c,", opt->short_opt);
			}
			written += printf("--%s", opt->name);
			if (opt->has_arg == required_argument) {
				written += printf(" <arg>");
			}
			printf("%*s    %s\n", (int)(width - written), "", opt->description);
		}
		// TODO: allow user to specify common options
		//printf("Common options:\n");
		printf("    -h,--help                  Print this message and exit\n");
	} else {
		// calculate column width
		size_t width = 0;
		for (int i = 0; cmd->commands[i]; i++) {
			if (cmd->commands[i]->hidden)
				continue;
			width = max(width, strlen(cmd->commands[i]->name));
		}

		printf("Commands:\n");
		for (int i = 0; cmd->commands[i]; i++) {
			if (cmd->commands[i]->hidden)
				continue;
			printf("    ");
			for (int j = nr_commands-1; j >= 0; j--) {
				printf("%s ", cmd_path[j]);
			}
			printf("%-*s    %s\n", (int)width, cmd->commands[i]->name, cmd->commands[i]->description);
		}
	}
}

_Noreturn void command_usage_error(struct command *cmd, const char *fmt, ...)
{
	command_print_usage(cmd);

	va_list ap;
	va_start(ap, fmt);
	sys_verror(fmt, ap);
	va_end(ap);
}

enum {
	LOPT_HELP = -2
};

int command_getopt(int argc, char *argv[], struct command *cmd)
{
	static struct option long_opts[COMMAND_MAX_OPTIONS * 2];
	static char short_opts[COMMAND_MAX_OPTIONS * 2];
	static bool initialized = false;

	// process options on first invocation
	if (!initialized) {
		int nr_opts = 0;
		int nr_short_opts = 0;

		for (nr_opts = 0; cmd->options[nr_opts].name; nr_opts++) {
			long_opts[nr_opts] = (struct option) {
				.name = cmd->options[nr_opts].name,
				.has_arg = cmd->options[nr_opts].has_arg,
				.flag = NULL,
				.val = cmd->options[nr_opts].val
			};
			if (cmd->options[nr_opts].short_opt) {
				short_opts[nr_short_opts++] = cmd->options[nr_opts].short_opt;
				if (cmd->options[nr_opts].has_arg == required_argument) {
					short_opts[nr_short_opts++] = ':';
				}
			}
		}
		// TODO: allow user to specify common options
		long_opts[nr_opts++] = (struct option) { "help",            no_argument,       NULL, LOPT_HELP };
		//long_opts[nr_opts++] = (struct option) { "version",         no_argument,       NULL, LOPT_VERSION };
		long_opts[nr_opts] = (struct option) { 0, 0, 0, 0 };
		short_opts[nr_short_opts++] = 'h';
		//short_opts[nr_short_opts++] = 'v';
		short_opts[nr_short_opts] = '\0';
		initialized = true;
	}

	int option_index = 0;
	int c = getopt_long(argc, argv, short_opts, long_opts, &option_index);
	switch (c) {
	case 'h':
	case LOPT_HELP:
		command_print_usage(cmd);
		exit(0);
	// TODO: allow user to specify common options
	//case 'v':
	//case LOPT_VERSION:
	//	print_version();
	//	exit(0);
	//case LOPT_INPUT_ENCODING:
	//	set_input_encoding(optarg);
	//	break;
	//case LOPT_OUTPUT_ENCODING:
	//	set_output_encoding(optarg);
	//	break;
	case '?':
		command_usage_error(cmd, "Unrecognized command line argument");
	}
	return c;
}

int command_execute(struct command *cmd, int argc, char *argv[])
{
	if (cmd->fun) {
		if (argc < 2) {
			command_print_usage(cmd);
			exit(0);
		}
		return cmd->fun(argc, argv);
	}

	if (argc < 2) {
		command_print_usage(cmd);
		exit(0);
	}

	for (int i = 0; cmd->commands[i]; i++) {
		if (!strcmp(argv[1], cmd->commands[i]->name)) {
			return command_execute(cmd->commands[i], argc-1, argv+1);
		}
	}

	command_usage_error(cmd, "Unrecognized command: %s", argv[1]);
}
