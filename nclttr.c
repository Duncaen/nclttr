/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2019 Duncan Overbruck <mail@duncano.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

int running = 1;
int selfpipe[2];

static void
sigint(int sig)
{
	running = 0;
	while (write(selfpipe[1], "\n", 1) == -1 && errno == EINTR)
		;
}

int
main(int argc, char *argv[])
{
	unsigned char mask[(XI_LASTEVENT + 7)/8] = { 0 };
	Window root;
	Display *dpy;
	int screen;
	XIEventMask evmask = {
		.deviceid = XIAllMasterDevices,
		.mask_len = sizeof mask,
		.mask = mask,
	};
	XEvent ev;
	XGenericEventCookie *cookie = &ev.xcookie;
	fd_set readset;
	int fd, hidden = 0, jitter = 0, timeout = 5, nfds;
	struct timeval tv = {0};
	double x = 0, y = 0;

	while ((fd = getopt(argc, argv, "j:t:")) != -1)
		switch (fd) {
		case 'j':
			jitter = atoi(optarg);
			jitter = jitter*jitter;
			break;
		case 't':
			timeout = atoi(optarg);
			break;
		default:
			fprintf(stderr, "usage: %s [-t timeout] [-j jitter]", *argv);
			return 1;
		}

	if (pipe(selfpipe) == -1)
		perror("pipe");
	if (fcntl(selfpipe[0], F_SETFL, O_NONBLOCK) == -1 ||
	    fcntl(selfpipe[1], F_SETFL, O_NONBLOCK) == -1)
		perror("fctnl");

	if ((dpy = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr, "%s: cannot open display", *argv);
		return 1;
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	XISetMask(mask, XI_RawMotion);
	XISetMask(mask, XI_RawButtonPress);
	XISetMask(mask, XI_RawTouchUpdate);
	XISelectEvents(dpy, root, &evmask, 1);

	fd = ConnectionNumber(dpy);
	nfds = (fd > selfpipe[0] ? fd : selfpipe[0])+1;

	signal(SIGINT, sigint);
	XSync(dpy, False);
	while (running) {
		if (!hidden && XPending(dpy) == 0) {
			tv.tv_sec = timeout;
			FD_ZERO(&readset);
			FD_SET(fd, &readset);
			FD_SET(selfpipe[0], &readset);
			if (select(nfds, &readset, NULL, NULL, &tv) == 0) {
				hidden = 1;
				x = y = 0;
				XFixesHideCursor(dpy, root);
				continue;
			}
			if (FD_ISSET(selfpipe[0], &readset))
				break;
		}
		if (XNextEvent(dpy, &ev))
			break;
		if (!hidden)
			continue;
		if (jitter) {
			if (!XGetEventData(dpy, cookie)) {
				XFreeEventData(dpy, cookie);
				continue;
			}
			if (cookie->evtype == XI_RawMotion) {
				XIRawEvent *re;
				re = ((XIRawEvent *)cookie->data);
				x += re->raw_values[0];
				y += re->raw_values[1];
				if (x*x + y*y < jitter) {
					XFreeEventData(dpy, cookie);
					continue;
				}
			}
			XFreeEventData(dpy, cookie);
		}
		XFixesShowCursor(dpy, root);
		hidden = 0;
	}
	XCloseDisplay(dpy);
	return 0;
}
