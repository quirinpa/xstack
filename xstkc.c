/* #define VERSION "0.001a" */

/* const char *argp_program_version = "xstkc-"VERSION, */
/* 			*argp_program_bug_address = "quirinpa@gmail.com"; */

/* static char doc[] = "Send a command to xstkd", */
/* 						args_doc[] = "command (arg) (options)"; */

/* #include <argp.h> */
/* static struct argp_option options[] = { */
/* 	{ "port", 'p', "UINT", 0, "send command to this specific port (default: 32005)", 0 }, */
/* 	{ "host", 'h', "STRING", 0 "send command to this specific host (default: localhost)", 0 }, */
/* 	{0, 0, 0, 0, 0, 0} */
/* }; */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
int main(int argc, char **argv) {
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server;

	if (lfd < 0) {
		perror("socket");
		close(lfd);
		return 1;
	}

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons(32005);

	if (connect(lfd, (struct sockaddr*)&server, sizeof(server)) < 0) {
		perror("connect");
		close(lfd);
		return 1;
	}

	if (argc > 1) {
		char *command = argv[1];
		int cmd;

		if (strcmp(command, "push")) {
			if (strcmp(command, "pop")) {
				fputs("unknown command\n", stderr);
				close(lfd);
				return 1;
			}

			fputs("pop\n", stderr);
			cmd = 2;
		} else {
			fputs("push", stderr);

			if (argc > 2) {
				fputs(argv[2], stderr);
				fputc('\n', stderr);
				if (write(lfd, &cmd, sizeof(cmd)) < 0) {
					perror("write0");
					close(lfd);
					return 1;
				}
				cmd = atoi(argv[2]);
			} else cmd = 1;
		}

		if (write(lfd, &cmd, sizeof(cmd)) < 0) {
			perror("write0?");
			close(lfd);
			return 1;
		}
		
		int ret;
		if (read(lfd, &ret, sizeof(ret)) < 0) {
			perror("read");
		} else {
			close(lfd);

			if (ret) {
				fputs("server: no way, Jose", stderr);
				return ret;
			}

			return 0;
		}

	} else
		fputs("no command\n", stderr);

	close(lfd);
	return 1;
}
