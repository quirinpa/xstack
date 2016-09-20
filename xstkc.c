#define VERSION "0.001a"

const char *argp_program_version = "xstkc-"VERSION,
			*argp_program_bug_address = "quirinpa@gmail.com";

static char doc[] = "Send a command to xstkd",
						args_doc[] = "pop/push [windowid]";

#include <argp.h>
static struct argp_option options[] = {
	{ "addr", 'a', "STRING", 0, "host:port (localhost:32005)", 0 },
	{0, 0, 0, 0, 0, 0}
};

#include "common.h"
typedef struct {
	bool is_push:1,
			 has_command:1,
			 has_windowid:1,
			 has_address:1;

	xwin_t wid;
	port_t port;
	char *addr;
} arguments_t;


#include <stdlib.h>
#include <string.h>
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	arguments_t *args = (arguments_t*) state->input;

	switch (key) {
		case 'a':
			{
				register char *end = arg;
				args->has_address = true;
				while (end < arg + sizeof(arg)) {
					register char c = *arg;
					if (c == '\0') break;
					if (c == ':') {
						args->port = (port_t)strtoul(end, NULL, 10);
						break;
					}
					end++;
				}

				register size_t end_pos = end + 1 - arg;
				args->addr = (char*)
					malloc(end_pos * sizeof(char));
				strncpy(args->addr, arg, end_pos);
			}
			break;
		case ARGP_KEY_NO_ARGS: argp_usage(state); break;
		case ARGP_KEY_ARG:
			if (args->has_command) {
				args->wid = (xwin_t) strtoul(arg, NULL, 10);
				args->has_windowid = true;
			} else {
				if (!(args->is_push = !strcmp(arg, "push")) &&
						strcmp(arg, "pop") ) {
					fputs("unknown command \"", stderr);
					fputs(arg, stderr);
					fputs("\"\n", stderr);
					return 1;
				}
				args->has_command = true;
			}
			break;
		default: return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, NULL, 0 };
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
int main(int argc, char **argv) {
	arguments_t args;

	args.addr = "127.0.0.1";
	args.port = 32005;
	args.has_command = false;
	args.has_windowid = false;
	args.has_address = false;
	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (!args.has_command) {
		fputs("no command\n", stderr);
		return 1;
	}

	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server;

	if (lfd < 0) {
		perror("socket");
		close(lfd);
		return 1;
	}

	server.sin_addr.s_addr = inet_addr(args.addr);
	server.sin_family = AF_INET;
	server.sin_port = htons(args.port);
	/* printf("Connecting to %s:%u\n", args.addr, args.port); */

	if (connect(lfd, (struct sockaddr*)&server, sizeof(server)) < 0) {
		perror("connect");
		close(lfd);
		return 1;
	}

	if (args.is_push) {
		cmd_t cmd = (cmd_t) args.has_windowid;

		if (write(lfd, &cmd, sizeof(cmd_t)) < 0) {
			perror("write push");
			close(lfd);
			return 1;
		}

		if (cmd && write(lfd, &args.wid, sizeof(xwin_t)) < 0) {
			perror("write windowid");
			close(lfd);
			return 1;
		}

	} else {
		cmd_t cmd = 2;

		if (write(lfd, &cmd, sizeof(cmd_t)) < 0) {
			perror("write pop");
			close(lfd);
			return 1;
		}
	}

	status_t failed;
	if (read(lfd, &failed, sizeof(failed)) < 0) {
		perror("read failed");
		close(lfd);
		return 1;
	}

	close(lfd);
	if (failed) {
		fputs("The server refused the command.\n", stderr);
		return 1;
	}

	return 0;
}
