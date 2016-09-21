#include <X11/Xlib.h>

#define TYPE Window
#include "once/stack.h"

#include <stdio.h>

static int xerror;
static Window rootw;

static int xpush(Display *d, STACK_ELEMENT **stack, Window w) {
	int revert_to_return;
	if (!w) XGetInputFocus(d, &w, &revert_to_return);
	if (w != rootw) {
		XUnmapWindow(d, w);
		XSync(d, 0);
		if (!xerror) *stack = PUSH(*stack, w);
		return xerror;
	}

	return 1;
}

static int xpop(Display *d, STACK_ELEMENT **stack_r) {
	Window pop_window;
	if (POP(stack_r, &pop_window)) return 1;
	XMapWindow(d, pop_window);
	XSync(d, 0);
	return xerror;
}


static int x_err_handler(Display *d, XErrorEvent *e) {
	xerror = 1;
	return 0;
}

static int x_io_err_handler(Display *d) {
	xerror = 1;
	return 0;
}

#include <signal.h>
volatile sig_atomic_t not_term;

void term_handler(int sig) {
	not_term = 0;
}

static void
cleanup_windows(Display *d, STACK_ELEMENT *first) {
	while (first) {
		STACK_ELEMENT *prev = first;
		XMapWindow(d, first->data);
		first = first->next;
		free(prev);
	}
}

#include "common.h"
/* #include <stdlib.h> */
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
int main(int argc, char **argv) {
	port_t port;
	if (argc>1) port = (port_t) strtoul(argv[1], NULL, 10);
	else port = 32005;

	sigset_t mask, orig_mask;
	struct sigaction act;
	int lfd, yes = 1;

	memset(&act, 0, sizeof(act));
	act.sa_handler = term_handler;
	if (sigaction(SIGINT, &act, 0) < 0) {
		perror("sigaction");
		return 1;
	}

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0)
	{ perror("sigprocmask"); return 1; }

	lfd = socket(AF_INET, SOCK_STREAM, 0);

	if (lfd < 0)
	{ perror("socket"); return 1; }

	if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{ perror("setsockopt"); close(lfd); return 1; }

	struct sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(port);

	if (bind(lfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
	{ perror("bind"); close(lfd); return 1; }

	if (listen(lfd, 1) < 0)
	{ perror("listen"); close(lfd); return 1; }

	Display *display = XOpenDisplay(NULL);

	if (display == NULL)
	{ fputs("couldn't open display\n", stderr); close(lfd); return 1; }

	XSetErrorHandler(x_err_handler);
	XSetIOErrorHandler(x_io_err_handler);
	rootw = XDefaultRootWindow(display);
	STACK_ELEMENT *stack = NULL;
	not_term = 1;

	while (not_term) {
		fd_set fds;
		int res;

		FD_ZERO(&fds);
		FD_SET(lfd, &fds);
		res = pselect(lfd + 1, &fds, NULL, NULL, NULL, &orig_mask);
		if (res<0) {
			if (errno == EINTR) break;
			perror("pselect");
			continue;
		}

		xerror = 0;

		if (!not_term) break;
		if (res == 0) continue;

		if (FD_ISSET(lfd, &fds)) {
			struct sockaddr_in client;
		 	int client_len, cfd = accept(lfd, (struct sockaddr*)&client, &client_len);

			if (cfd < 0) {
				if (cfd == -1) {
					perror("accept");
					continue;
				}
				break;
			}

#define MAYBE_READ(store, type) {\
	register int status = read(cfd, &store, sizeof(type));\
	if (status < 0) {\
		if (status == -1) {\
			perror("read " # store); close(cfd); continue;\
		}; close(cfd); break;\
	}}

			cmd_t cmd;
			MAYBE_READ(cmd, cmd_t);

			status_t failed;
			if (cmd < 2) {
				if (cmd) {
					xwin_t windowid;
					MAYBE_READ(windowid, xwin_t);

					if (!(failed = xpush(display, &stack, windowid)))
						printf("U%lu\n", windowid);

				} else if (!(failed = xpush(display, &stack, 0)))
					puts("u");
			} else if (!(failed = xpop(display, &stack)))
				puts("o");

			write(cfd, &failed, sizeof(status_t));
			close(cfd);
		}
	}

	close(lfd);
	cleanup_windows(display, stack);
	XCloseDisplay(display);
	fputs("OK\n", stderr);
	return 0;
}
