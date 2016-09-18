/*
#!/bin/sh

# PUSH(window_id = current, tag)
# push a window into an invisible stack

set -e

cache_dir=/tmp/xdostack
[ ! -d $cache_dir ] && mkdir $cache_dir

wid=`[ -z $1 ] && bspc query -N -n || echo $1`

tag=${2:-misc}
stack_file=$cache_dir/$tag/stack

newpath=$([ -d $cache_dir/$tag ] &&
	head -c -1 -q $stack_file ||
	echo -n $cache_dir/$tag )/$wid

mkdir -p $newpath
echo $newpath > $stack_file

xdo hide $wid
*/


#include <X11/Xlib.h>

#define TYPE Window
#include "once/stack.h"

#include <stdio.h>

static int xpush(Display *d, Window rootw, STACK_ELEMENT **stack, Window w) {
	int revert_to_return;
	if (!w) XGetInputFocus(d, &w, &revert_to_return);
	/* if (w != None && w != PointerRoot && w != rootw) { */
	if (w != rootw) {
		/* fprintf(stderr, "r: %d, w: %d\n", revert_to_return, w); */
		*stack = PUSH(*stack, w);
		XUnmapWindow(d, w);
		return 0;
	}

	return 1;
}

static int xpop(Display *d, STACK_ELEMENT **stack_r) {
	Window pop_window;
	if (POP(stack_r, &pop_window)) return 1;
	XMapWindow(d, pop_window);
	return 0;
}

static int xerror;

static int x_err_handler(Display *d, XErrorEvent *e) {
	char buf[60];
	fputs("X - ", stderr);
	XGetErrorText(d, e->error_code, buf, 60 * sizeof(char));
	fputs(buf, stderr);
	fputc('\n', stderr);
	xerror = 1;
	return 0;
}

static int x_io_err_handler(Display *d) {
	fputs("XIO\n", stderr);
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
		/* XDestroyWindow(d, first->data); */
		first = first->next;
		free(prev);
	}
}

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
int main() {
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

	if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
		perror("sigprocmask");
		return 1;
	}

	lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd < 0) {
		perror("socket");
		return 1;
	}

	if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		close(lfd);
		return 1;
	}

	struct sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(32005);

	if (bind(lfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind");
		close(lfd);
		return 1;
	}

	if (listen(lfd, 1) < 0) {
		perror("listen");
		close(lfd);
		return 1;
	}

	Display *display = XOpenDisplay(NULL);
	if (display == NULL) {
		fputs("couldn't open display\n", stderr);
		close(lfd);
		return 1;
	}

	/* Atom a = XInternAtom(display, "__NET_CLIENT_LIST", true); */
	XSetErrorHandler(x_err_handler);
	XSetIOErrorHandler(x_io_err_handler);
	Window rootw = XDefaultRootWindow(display);
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
		 	int client_len, cfd =
				accept(lfd, (struct sockaddr*)&client, &client_len);

			if (cfd<0) {
				if (cfd==-1) {
					perror("accept");
					continue;
				}
				break;
			}

			int cmd_type, read0 = read(cfd, &cmd_type, sizeof(int));

			if (read0 < 0) {
				if (read0 == -1) {
					perror("read0");
					close(cfd);
					continue;
				}
				close(cfd);
				break;
			}

			int ret;
			if (cmd_type == 1) {
				puts("u");
				ret = xpush(display, rootw, &stack, 0);
			} else if (cmd_type == 2) {
				puts("o");
				ret = xpop(display, &stack);
			} else {
				int wid, read1;
				putchar('U');
				read1 = read(cfd, &wid, sizeof(int));
				if (read1 < 0) {
					if (read1 == -1) {
						perror("read1");
						continue;
					}
					break;
				}
				printf("%d\n", wid);
				ret = xpush(display, rootw, &stack, wid);
			}

			XSync(display, 0);
			/* XFlush(display); */
			ret = ret || xerror;
			write(cfd, &ret, sizeof(int));
			/* fprintf(stderr, "ret: %d\n", ret); */
			close(cfd);
		}
	}

	close(lfd);
	cleanup_windows(display, stack);
	XCloseDisplay(display);
	fputs("OK\n", stderr);
	return 0;
}
