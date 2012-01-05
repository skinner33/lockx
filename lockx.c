/* © 2008 André Prata <andreprata at bugflux dot org>
 * See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>

#if HAVE_BSD_AUTH
#include <login_cap.h>
#include <bsd_auth.h>
#endif

char *
reverse(char *str)
{
	char *end = str + strlen(str) - 1;
	char *start = str;

	while(start < end)
	{
		char tmp = *start;
		*start = *end;
		*end = tmp;
		end--; start++;
	}

	return str;
}



static void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

#ifndef HAVE_BSD_AUTH
static const char *
get_password()
{ /* only run as root */
	const char *rval;
	struct passwd *pw;

	if(geteuid() != 0)
	{
		die("lockx: cannot retrieve password entry (make sure to suid lockx)\n");
	}
	pw = getpwuid(getuid());
	endpwent();
	rval =  pw->pw_passwd;

#if HAVE_SHADOW_H
	{
		struct spwd *sp;
		sp = getspnam(getenv("USER"));
		endspent();
		rval = sp->sp_pwdp;
	}
#endif
	/* drop privileges */
	if(setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0)
	{
		die("lockx: cannot drop privileges\n");
	}
	return rval;
}
#endif


int 
main(int argc, char **argv)
{
	char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
	char buf[32], passwd[256] = { 0 };
	int num, screen;
#ifndef HAVE_BSD_AUTH
	const char *pws;
#endif
	unsigned int len;
	Bool running = True, dpmsRestore = True;
	BOOL dpmsWasActive;
	CARD16 dpmsPowerLevel;
	Cursor invisible;
	Display *dpy;
	KeySym ksym;
	Pixmap pmap;
	Window root, w;
	XColor black, dummy;
	XEvent ev;
	XSetWindowAttributes wa;
	int i, k, count;
	char tempbuf[256];


	if((argc == 2) && !strcmp("-v", argv[1]))
	{
		die("lockx-"VERSION", © 2008 Andre Prata\n");
	}
	else if(argc != 1)
	{
		die("usage: lockx [-v]\n");
	}
#ifndef HAVE_BSD_AUTH
	pws = get_password();
#endif
	if(!(dpy = XOpenDisplay(0)))
	{
		die("lockx: cannot open display\n");
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	/* init */
	wa.override_redirect = 1;
	wa.background_pixel = BlackPixel(dpy, screen);
	w = XCreateWindow(dpy, root, 0, 0, DisplayWidth(dpy, screen), DisplayHeight(dpy, screen),
			0, DefaultDepth(dpy, screen), CopyFromParent,
			DefaultVisual(dpy, screen), CWOverrideRedirect | CWBackPixel, &wa);
	XAllocNamedColor(dpy, DefaultColormap(dpy, screen), "black", &black, &dummy);
	pmap = XCreateBitmapFromData(dpy, w, curs, 8, 8);
	invisible = XCreatePixmapCursor(dpy, pmap, pmap, &black, &black, 0, 0);
	XDefineCursor(dpy, w, invisible);
	XMapRaised(dpy, w);
	for(len = 1000; len; len--)
	{
		if(XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
			GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime) == GrabSuccess)
		{
			break;
		}
		usleep(1000);
	}
	if((running = running && (len > 0)))
	{
		for(len = 1000; len; len--)
		{
			if(XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
			{
				break;
			}
			usleep(1000);
		}
		running = (len > 0);
	}
	len = 0;
	XSync(dpy, False);

	/* get old DPMS status and enable DPMS if needed */
	if(True == DPMSInfo(dpy, &dpmsPowerLevel, &dpmsWasActive)) {
		if(False == dpmsWasActive) {
			DPMSEnable(dpy);
		}
	}
	else {
		dpmsRestore = False;
	}

	/* main event loop */
	while(running && !XNextEvent(dpy, &ev))
	{
		if(len == 0)
		{
			DPMSForceLevel(dpy, DPMSModeOff);
		}
		if(ev.type == KeyPress)
		{
			buf[0] = 0;
			num = XLookupString(&ev.xkey, buf, sizeof buf, &ksym, 0);
			if(IsKeypadKey(ksym))
			{
				if(ksym == XK_KP_Enter)
				{
					ksym = XK_Return;
				}
				else if(ksym >= XK_KP_0 && ksym <= XK_KP_9)
				{
					ksym = (ksym - XK_KP_0) + XK_0;
				}
			}
			if(IsFunctionKey(ksym) || IsKeypadKey(ksym)
					|| IsMiscFunctionKey(ksym) || IsPFKey(ksym)
					|| IsPrivateKeypadKey(ksym))
			{
				continue;
			}

			switch(ksym)
			{
			case XK_Return:
				len = 0;
				break;

			case XK_Escape:
				len = 0;
				break;

			case XK_BackSpace:
				len = --len < 0 ? sizeof passwd - 1 : len;
				break;

			default:
				if(num && !iscntrl((int) buf[0]))
				{ 
					for(i = 0; i < num; len %= sizeof passwd, i++)
					{
						passwd[len++] = buf[i];

					}

					for(count = 1; count <= strlen(passwd); count++)
					{
						for(i = (len - 1) % sizeof passwd, k = 0; k < count; i = --i < 0 ? sizeof passwd - 1 : i, k++)
						{
							tempbuf[k] = passwd[i];
						}
						tempbuf[k] = 0;

						reverse(tempbuf);
#ifdef HAVE_BSD_AUTH
						running = !auth_userokay(getlogin(), NULL, "auth-xlock", passwd);
#else
						if((running = strcmp(crypt(tempbuf, pws), pws)) == 0)
						{
							break;
						}
#endif
					}
				}
				break;
			}
		}
	}

	/* restore dpms */
	if(dpmsRestore) {
		if(!dpmsWasActive) {
			DPMSDisable(dpy);
		}
	}

	XUngrabPointer(dpy, CurrentTime);
	XFreePixmap(dpy, pmap);
	XDestroyWindow(dpy, w);
	XCloseDisplay(dpy);
	return 0;
}
