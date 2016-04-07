/*-
 * See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance.  Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag.  Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */

#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & \
		(ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tag == C->mon->tag[C->mon->seltag]))
#define LENGTH(X)               (sizeof(X) / sizeof(X[0]))
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)

#define MAXTAGS                 20
#define HEX2RGB(hex, r, g, b)   do {					\
	typeof(hex) _h = (hex);						\
	(r) = ((_h >> 16) & 0xff) / 255.0;				\
	(g) = ((_h >> 8)  & 0xff) / 255.0;				\
	(b) = ((_h >> 0)  & 0xff) / 255.0;				\
} while (0)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast };	/* cursor */
enum { NetSupported, NetWMName, NetWMState,
	NetWMFullscreen, NetActiveWindow, NetWMWindowType,
	NetWMWindowTypeDialog, NetLast
};				/* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };	/* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
	ClkClientWin, ClkRootWin, ClkLast
};				/* clicks */
enum { MouseUp, MouseDown, MouseLeft, MouseRight };	/* mouse directions */
enum { MouseLBtn, MouseMBtn, MouseRBtn };		/* mouse buttons */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func) (const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tag;
	Bool isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Client *next;
	Client *snext;
	Monitor *mon;
	Window win;
};

typedef struct {
	cairo_surface_t *cs;
	cairo_t *cr;
} Cairo;

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func) (const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange) (Monitor *);
} Layout;

typedef struct {
	unsigned int lt;
	char ltsymbol[16];

	float mfact;
	int nmaster;
} Tag;

struct Monitor {
	int num;
	int mx, my, mw, mh;
	int wx, wy, ww, wh;
	unsigned int seltag;
	unsigned int tag[2];
	Tag *tags[MAXTAGS];
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	int by;
	Bool showbar;
	Window barwin;
	Cairo barwin_cairo;
	Window bodywin;
	Cairo bodywin_cairo;
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tag;
	Bool isfloating;
	int monitor;
} Rule;

static Bool aloneintag(Client *c);
static void applyrules(Client *c);
static Bool applysizehints(Client *c, int *x, int *y, int *w, int *h,
			   Bool interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void bar_cleanupdc(void);
static void bar_drawbg(long color);
static void bar_drawfg(long color, const char *text);
static void bar_drawsquare(Bool filled, Bool empty, Bool urgent);
static void bar_drawtext_lt(const char *text);
static void bar_drawtext_other(const char *text);
static void bar_drawtext_tag(const char *text, Bool selected);
static void bar_initdc(void);
static void bar_initfont(void);
static int bar_textrw(const char *text);
static int bar_textw(const char *text);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clearurgent(Client *c);
static void clickmouse(const Arg *arg);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static Tag *createtag(void);
static void cycleview(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static void die(const char *errstr, ...);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawbaronbody(Monitor *m);
static void drawbody(Monitor *m);
static void drawbodys(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void initdc(void);
static void initrc(void);
static Bool getrootptr(int *x, int *y);
static long getstate(Window w);
static Atom getatomprop(Client *c, Atom prop);
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, Bool focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static Tag *montotag(Monitor *m);
static void motionnotify(XEvent *e);
static void mousemove(const Arg *arg);
static void mouseresize(const Arg *arg);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, Bool interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static Bool sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, Bool fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void unfocus(Client *c, Bool setfocus);
static void unmanage(Client *c, Bool destroyed);
static void unmapnotify(XEvent *e);
static Bool updategeom(void);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updatebodys(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatewindowtype(Client *c);
static void updatetitle(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;
static int bh, blw = 0;
static int (*xerrorxlib) (Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static Atom wmatom[WMLast], netatom[NetLast];
static Bool running = True;
static Cursor cursor[CurLast];
static Display *dpy;
static Monitor *mons = NULL, *selmon = NULL;
static Window root;

#include "config.h"

struct NumTags {
	char limitexceeded[LENGTH(tags) > MAXTAGS ? -1 : 1];
};

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, True);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tag = m->tag[m->seltag];
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
			PropModeReplace, (unsigned char *)data, 2);
}

Bool
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	Bool exists = False;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return (exists);
}

void
setfullscreen(Client *c, Bool fullscreen)
{
	if (fullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace,
				(unsigned char *)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = True;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = True;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char *)0, 0);
		c->isfullscreen = False;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return (0);
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return (xerrorxlib(dpy, ee));
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return (0);
}

int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running\n");
	return (-1);
}

static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click;
	Arg arg = { 0 };
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, True);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += bar_textw(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags))
			;
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = i;
		} else if (ev->x < x + blw)
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - bar_textw(stext))
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func
		    && buttons[i].button == ev->button
		    && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar
					&& buttons[i].arg.i ==
					0 ? &arg : &buttons[i].arg);
}

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XkbKeycodeToKeysym(dpy, (KeyCode) ev->keycode, 0, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		    && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		    && keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, False);
	}
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, True);
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		    || cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c,
			     cme->data.l[0] == 1
			 || (cme->data.l[0] == 2
			     && !c->isfullscreen));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (!ISVISIBLE(c)) {
			c->mon->seltag ^= 1;
			c->mon->tag[c->mon->seltag] = c->tag;
		}
		pop(c);
	}
}

void
expose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;
	Monitor *m = wintomon(ev->window);

	if (m == NULL)
		return;

	if (ev->count != 0)
		return;

	if (ev->window == m->barwin)
		drawbar(m);

	if (ev->window == m->bodywin)
		drawbody(m);
}

void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2);
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2);
			if ((ev->value_mask & (CWX | CWY))
			    && !(ev->value_mask & (CWWidth | CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w,
						  c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	XConfigureEvent *ev = &e->xconfigure;
	Bool dirty;

	if (ev->window == root) {
		dirty = (sw != ev->width);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			updatebodys();
			updatebars();
			for (m = mons; m; m = m->next) {
				XMoveResizeWindow(dpy, m->bodywin, m->mx, m->my,
						  m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by,
						  m->ww, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior)
	    && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, True);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return;
	else if ((c = wintoclient(ev->window))) {
		switch (ev->atom) {
		default:
			break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating
			    && (XGetTransientForHint(dpy, c->win, &trans))
			    && (c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

typedef struct {
	int x, y, w;
	void *buffer;
	cairo_surface_t *cs;
	cairo_t *cr;
	cairo_font_face_t *ff;
	struct {
		int ascent;
		int descent;
		int height;
	} font;
} BarDC;

static BarDC bar_dc;
static unsigned int bar_text_h_margin;
static unsigned int bar_text_v_margin;

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
drawbar(Monitor *m)
{
	unsigned int i, occ = 0, urg = 0;
	int lt_end;
	Client *c;
	Tag *t;

	for (c = m->clients; c; c = c->next) {
		occ |= (1 << c->tag);
		if (c->isurgent)
			urg |= (1 << c->tag);
	}

	bar_dc.x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		bar_dc.w = bar_textw(tags[i]);
		bar_drawtext_tag(tags[i], m->tag[m->seltag] == i);
		bar_drawsquare(False, (occ & (1 << i)), (urg & (1 << i)));
		bar_dc.x += bar_dc.w;
	}

	t = montotag(m);
	bar_dc.w = blw = bar_textw(t->ltsymbol);
	bar_drawtext_lt(t->ltsymbol);
	bar_dc.x += bar_dc.w;
	lt_end = bar_dc.x;

	bar_dc.w = m->ww - lt_end;
	if (m->sel) {
		bar_drawtext_other(m->sel->name);
		bar_drawsquare(m->sel->isfixed, m->sel->isfloating,
			       False);
	} else {
		bar_drawtext_other(NULL);
	}

	if (m == selmon) {
		bar_dc.w = bar_textw(stext);
		bar_dc.x = m->ww - bar_dc.w;
		if (bar_dc.x < lt_end) {
			bar_dc.x = lt_end;
			bar_dc.w = m->ww - lt_end;
		}
		bar_drawtext_other(stext);
	}

	cairo_set_source_surface(m->barwin_cairo.cr, bar_dc.cs, 0, 0);
	cairo_paint(m->barwin_cairo.cr);
	XSync(dpy, False);

	drawbaronbody(m);
}

void
bar_drawtext_lt(const char *text)
{
	bar_drawbg(bar_bg_color_normal);
	bar_drawfg(bar_fg_color_normal, text);
}

void
bar_drawtext_tag(const char *text, Bool selected)
{
	long color;

	color = (selected ? bar_bg_color_sel : bar_bg_color_normal);
	bar_drawbg(color);
	color = (selected ? bar_fg_color_sel : bar_fg_color_normal);
	bar_drawfg(color, text);
}

void
bar_drawtext_other(const char *text)
{
	bar_drawbg(bar_bg_color_normal);
	if (!text)
		return;
	bar_drawfg(bar_fg_color_normal, text);
}

void
bar_drawsquare(Bool filled, Bool empty, Bool urgent)
{
	int x;
	double r, g, b;

	x = (bar_dc.font.ascent + bar_dc.font.descent + 2) / 4;

	HEX2RGB((urgent ? square_color_urgent : square_color_normal), r, g, b);
	cairo_set_source_rgb(bar_dc.cr, r, g, b);

	if (filled) {
		cairo_rectangle(bar_dc.cr, bar_dc.x + 1, bar_dc.y + 1, x + 1, x + 1);
		cairo_fill(bar_dc.cr);
	} else if (empty) {
		cairo_rectangle(bar_dc.cr, bar_dc.x + 1, bar_dc.y + 1, x, x);
		cairo_stroke(bar_dc.cr);
	}
}

void
bar_drawbg(long color)
{
	double r, g, b;

	cairo_rectangle(bar_dc.cr, bar_dc.x, bar_dc.y, bar_dc.w, bh);
	HEX2RGB(color, r, g, b);
	cairo_set_source_rgb(bar_dc.cr, r, g, b);
	cairo_fill(bar_dc.cr);
}

void
bar_drawfg(long color, const char *text)
{
	double r, g, b;
	int x, y;

	x = bar_dc.x + bar_text_h_margin;
	y = bar_dc.y + bar_text_v_margin;

	cairo_move_to(bar_dc.cr, x, y);
	HEX2RGB(color, r, g, b);
	cairo_set_source_rgb(bar_dc.cr, r, g, b);
	cairo_show_text(bar_dc.cr, text);
}

int
bar_textrw(const char *text)
{
	cairo_text_extents_t extents;

	cairo_text_extents(bar_dc.cr, text, &extents);

	return (extents.width);
}

int
bar_textw(const char *text)
{
	int margin = 2 * bar_text_h_margin;

	return (bar_textrw(text) + margin);
}

void
bar_initfont(void)
{
	cairo_font_extents_t extents;
	int sum;

	bar_dc.ff = cairo_toy_font_face_create(bar_font,
					   CAIRO_FONT_SLANT_NORMAL,
					   CAIRO_FONT_WEIGHT_BOLD);

	cairo_set_font_face(bar_dc.cr, bar_dc.ff);
	cairo_set_font_size(bar_dc.cr, bar_fontpx);
	cairo_font_extents(bar_dc.cr, &extents);
	bar_dc.font.ascent = extents.ascent;
	bar_dc.font.descent = extents.descent;
	sum = bar_dc.font.ascent + bar_dc.font.descent;
	bh = sum + bar_padding;
	bar_text_h_margin = sum / 2;
	bar_text_v_margin = (bh / 2) + bar_dc.font.ascent - (sum / 2);
}

void
bar_initdc(void)
{
	int stride;

	stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, sw);
	bar_dc.buffer = malloc(sh * stride);
	if (bar_dc.buffer == NULL)
		die("fatal: could not malloc() %d bytes\n", sh * stride);

	bar_dc.cs = cairo_image_surface_create_for_data(bar_dc.buffer,
						    CAIRO_FORMAT_ARGB32,
						    sw, sh, stride);
	bar_dc.cr = cairo_create(bar_dc.cs);

	cairo_set_line_width(bar_dc.cr, bar_linepx);

	bar_initfont();
}

void
bar_cleanupdc(void)
{
	cairo_destroy(bar_dc.cr);
	cairo_surface_destroy(bar_dc.cs);
	cairo_font_face_destroy(bar_dc.ff);
	free(bar_dc.buffer);
}

void
updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = bar_bg_color_normal,
		.event_mask = ButtonPressMask | ExposureMask
	};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin =
		    XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0,
				  DefaultDepth(dpy, screen), CopyFromParent,
				  DefaultVisual(dpy, screen),
				  CWOverrideRedirect | CWBackPixel |
				  CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]);
		m->barwin_cairo.cs = cairo_xlib_surface_create(dpy,
					m->barwin, DefaultVisual(dpy, 0),
					m->ww, bh);
		m->barwin_cairo.cr = cairo_create(m->barwin_cairo.cs);
		XMapRaised(dpy, m->barwin);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->wy;
		m->wy = m->wy + bh;
	} else
		m->by = -bh;
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "xyd");
	drawbar(selmon);
}

void
drawbodys(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbody(m);
}

void
drawbody(Monitor *m)
{
	double r, g, b;

	HEX2RGB(body_bg_color, r, g, b);

	cairo_rectangle(m->bodywin_cairo.cr, 0, 0, m->mw, bh);
	cairo_rectangle(m->bodywin_cairo.cr, 0, bh + bar_borderpx,
			m->mw, m->mh - (bh + bar_borderpx));
	cairo_set_source_rgb(m->bodywin_cairo.cr, r, g, b);
	cairo_fill(m->bodywin_cairo.cr);

	drawbaronbody(m);
}

void
drawbaronbody(Monitor *m)
{
	double r, g, b;

	HEX2RGB((m->showbar ? border_color_normal : body_bg_color),
		r, g, b);

	cairo_move_to(m->bodywin_cairo.cr, 0, bh);
	cairo_line_to(m->bodywin_cairo.cr, m->mw, bh);
	cairo_set_source_rgb(m->bodywin_cairo.cr, r, g, b);
	cairo_set_line_width(m->bodywin_cairo.cr, bar_borderpx);
	cairo_stroke(m->bodywin_cairo.cr);
}

void
updatebodys(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = body_bg_color,
		.event_mask = ButtonPressMask | ExposureMask
	};
	for (m = mons; m; m = m->next) {
		if (m->bodywin)
			continue;
		m->bodywin =
		    XCreateWindow(dpy, root, m->mx, m->my, m->mw, m->mh, 0,
				  DefaultDepth(dpy, screen), CopyFromParent,
				  DefaultVisual(dpy, screen),
				  CWOverrideRedirect | CWBackPixel |
				  CWEventMask, &wa);
		XDefineCursor(dpy, m->bodywin, cursor[CurNormal]);
		m->bodywin_cairo.cs = cairo_xlib_surface_create(dpy,
					m->bodywin, DefaultVisual(dpy, 0),
					m->mw, m->mh);
		m->bodywin_cairo.cr = cairo_create(m->bodywin_cairo.cs);
		XMapRaised(dpy, m->bodywin);
	}

	drawbodys();
}

void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("Can't install SIGCHLD handler");
	while (0 < waitpid(-1, NULL, WNOHANG)) ;
}

Bool
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return (XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui));
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return (r);
}

Tag *
montotag(Monitor *mon)
{
	unsigned int tagidx = mon->tag[mon->seltag];

	return (mon->tags[tagidx]);
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return (c);
	return (NULL);
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return (recttomon(x, y, 1, 1));
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return (m);
	if ((c = wintoclient(w)))
		return (c->mon);
	return (selmon);
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next) ;
	else
		for (m = mons; m->next != selmon; m = m->next) ;
	return (m);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc != NULL && *tc != c; tc = &(*tc)->next)
		;
	*tc = c->next;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc != NULL && *tc != c; tc = &(*tc)->snext)
		;
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
			;
		c->mon->sel = t;
	}
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext) ;
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, False);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			clearurgent(c);
		detachstack(c);
		attachstack(c);

		grabbuttons(c, True);
		if (aloneintag(c))
			XSetWindowBorder(dpy, c->win, border_color_normal);
		else
			XSetWindowBorder(dpy, c->win, border_color_sel);

		setfocus(c);
	} else
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
	selmon->sel = c;
	drawbars();
}

void
unfocus(Client *c, Bool setfocus)
{
	if (!c)
		return;
	grabbuttons(c, False);
	XSetWindowBorder(dpy, c->win, border_color_normal);
	if (setfocus)
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
}

void
setfocus(Client *c)
{
	if (!c->neverfocus)
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	sendevent(c, wmatom[WMTakeFocus]);
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else
		for (m = mons; m; m = m->next)
			showhide(m->stack);
	if (m)
		arrangemon(m);
	else
		for (m = mons; m; m = m->next)
			arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	Tag *t = montotag(m);

	if (t->lt != ~0) {
		strncpy(t->ltsymbol, layouts[t->lt].symbol,
			sizeof(t->ltsymbol));
		layouts[t->lt].arrange(m);
	}
	restack(m);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		XMoveWindow(dpy, c->win, c->x, c->y);
		if (c->isfloating && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, False);
		showhide(c->snext);
	} else {
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating)
		XRaiseWindow(dpy, m->sel->win);
	wc.stack_mode = Below;
	wc.sibling = m->barwin;
	for (c = m->stack; c; c = c->snext)
		if (!c->isfloating && ISVISIBLE(c)) {
			XConfigureWindow(dpy, c->win,
					 CWSibling | CWStackMode, &wc);
			wc.sibling = c->win;
		}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) ;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next) ;
	return (c);
}

void
tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;
	Tag *t = montotag(m);

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) ;
	if (n == 0) {
		t->mfact = mfact;
		t->nmaster = nmaster;
		return;
	}

	snprintf(t->ltsymbol, sizeof(t->ltsymbol), "[%d-%d]",
	    MIN(t->nmaster, n), MAX((int)(n-t->nmaster), 0));

	if (n > t->nmaster)
		mw = t->nmaster ? m->ww * t->mfact : 0;
	else
		mw = m->ww;
	for (i = my = ty = 0, c = nexttiled(m->clients); c;
	     c = nexttiled(c->next), i++)
		if (i < t->nmaster) {
			h = (m->wh - my) / (MIN(n, t->nmaster) - i);
			resize(c, m->wx, m->wy + my, mw - (2 * c->bw),
			       h - (2 * c->bw), False);
			my += HEIGHT(c);
		} else {
			h = (m->wh - ty) / (n - i);
			resize(c, m->wx + mw, m->wy + ty,
			       m->ww - mw - (2 * c->bw), h - (2 * c->bw),
			       False);
			ty += HEIGHT(c);
		}
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;
	Tag *t = montotag(m);

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0)
		snprintf(t->ltsymbol, sizeof(t->ltsymbol), "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw,
		       False);
}

void
resize(Client *c, int x, int y, int w, int h, Bool interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

Bool
applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact)
{
	Bool baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (c->isfloating) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) {	/* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) {	/* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return (*x != c->x || *y != c->y || *w != c->w || *h != c->h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x;
	c->x = wc.x = x;
	c->oldy = c->y;
	c->y = wc.y = y;
	c->oldw = c->w;
	c->w = wc.width = w;
	c->oldh = c->h;
	c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win,
		 CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *) & ce);
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	if (!(c = calloc(1, sizeof(Client))))
		die("fatal: could not malloc() %u bytes\n", sizeof(Client));
	c->win = w;
	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tag = t->tag;
	} else {
		c->mon = selmon;
		applyrules(c);
	}
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y =
	    MAX(c->y,
		((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
		 && (c->x + (c->w / 2) <
		     c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, border_color_normal);
	configure(c);		/* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w,
		     EnterWindowMask | FocusChangeMask | PropertyChangeMask |
		     StructureNotifyMask);
	grabbuttons(c, False);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h);	/* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, False);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
}

void
unmanage(Client *c, Bool destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);	/* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	arrange(m);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &num))
		return;

	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		    || wa.override_redirect
		    || XGetTransientForHint(dpy, wins[i], &d1))
			continue;
		if (wa.map_state == IsViewable
		    || getstate(wins[i]) == IconicState)
			manage(wins[i], &wa);
	}

	for (i = 0; i < num; i++) {	/* now the transients */
		if (!XGetWindowAttributes(dpy, wins[i], &wa))
			continue;
		if (XGetTransientForHint(dpy, wins[i], &d1)
		    && (wa.map_state == IsViewable
			|| getstate(wins[i]) == IconicState))
			manage(wins[i], &wa);
	}

	if (wins)
		XFree(wins);
}

void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tag = ~0;
	XGetClassHint(dpy, c->win, &ch);
	class = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name ? ch.res_name : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		    && (!r->class || strstr(class, r->class))
		    && (!r->instance || strstr(instance, r->instance))) {
			c->isfloating = r->isfloating;
			c->tag = r->tag;
			for (m = mons; m && m->num != r->monitor; m = m->next) ;
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	if (c->tag >= LENGTH(tags))
		c->tag = c->mon->tag[c->mon->seltag];
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty
	    (dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState], &real,
	     &format, &n, &extra, (unsigned char **)&p) != Success)
		return (-1);
	if (n != 0)
		result = *p;
	XFree(p);
	return (result);
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty
	    (dpy, c->win, prop, 0L, sizeof(atom), False, XA_ATOM, &da, &di, &dl,
	     &dl, &p) == Success && p) {
		atom = *(Atom *) p;
		XFree(p);
	}
	return (atom);
}

Bool
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return (False);
	text[0] = '\0';
	XGetTextProperty(dpy, w, &name, atom);
	if (!name.nitems)
		return (False);
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success
		    && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return (True);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof(c->name)))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof(c->name));
	if (c->name[0] == '\0')	/* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->minw && c->maxh && c->minh
		      && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, True);

	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = True;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent =
			    (wmh->flags & XUrgencyHint) ? True : False;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = False;
		XFree(wmh);
	}
}

void
clearurgent(Client *c)
{
	XWMHints *wmh;

	c->isurgent = False;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags &= ~XUrgencyHint;
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

Bool
aloneintag(Client *c)
{
	Client *tc;

	for (tc = c->mon->clients; tc != NULL; tc = tc->next)
		if (ISVISIBLE(tc) && tc != c && tc->tag == c->tag)
			return (False);
	return (True);
}

Tag *
createtag(void)
{
	Tag *t;

	if ((t = calloc(1, sizeof(Tag))) == NULL)
		die("fatal: could not malloc() %u bytes\n", sizeof(Tag));

	t->mfact = mfact;
	t->nmaster = nmaster;
	t->lt = 0;
	strncpy(t->ltsymbol, layouts[0].symbol, sizeof(t->ltsymbol));
	return (t);
}

Monitor *
createmon(void)
{
	Monitor *m;
	int i;

	if ((m = calloc(1, sizeof(Monitor))) == NULL)
		die("fatal: could not malloc() %u bytes\n", sizeof(Monitor));
	m->tag[0] = m->tag[1] = 0;
	m->seltag = 0;
	m->showbar = showbar;
	for (i = 0; i < LENGTH(tags); i++)
		m->tags[i] = createtag();
	return (m);
}

#ifdef XINERAMA
static Bool
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if(unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return (False);
	return (True);
}
#endif /* XINERAMA */

Bool
updategeom(void)
{
	Bool dirty = False;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++)
			;
		/* only consider unique geometries as separate screens */
		if (!(unique = malloc(sizeof(XineramaScreenInfo) * nn)))
			die("fatal: could not malloc() %u bytes\n",
			    sizeof(XineramaScreenInfo) * nn);
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i],
				    sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) {
			for (i = 0; i < (nn - n); i++) { /* new monitors available */
				for (m = mons; m && m->next; m = m->next)
					;
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n ||
				    (unique[i].x_org != m->mx ||
				     unique[i].y_org != m->my ||
				     unique[i].width != m->mw ||
				     unique[i].height != m->mh)) {
					dirty = True;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next)
					;
				while (m->clients) {
					dirty = True;
					c = m->clients;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	}
	else
#endif /* XINERAMA */
	/* default monitor setup */
	{
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = True;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return (dirty);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;
	int i;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next)
			;
		m->next = mon->next;
	}

	cairo_destroy(mon->barwin_cairo.cr);
	cairo_surface_destroy(mon->barwin_cairo.cs);
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);

	cairo_destroy(mon->bodywin_cairo.cr);
	cairo_surface_destroy(mon->bodywin_cairo.cs);
	XUnmapWindow(dpy, mon->bodywin);
	XDestroyWindow(dpy, mon->bodywin);

	for (i = 0; i < LENGTH(tags); i++)
		free(mon->tags[i]);
	free(mon);
}

static int movemousedelta = 10;

static void
resetmovemousedelta(int dummy)
{
	movemousedelta = 10;
}

void
movemouse(const Arg *arg)
{
	int direction = arg->i;
	struct itimerval it;
	int x, y;

	if (!getrootptr(&x, &y))
		return;

	if (movemousedelta < 70)
		movemousedelta += 10;

	signal(SIGALRM, resetmovemousedelta);

	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 50000;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &it, NULL);

	switch (direction) {
	case MouseUp:
		y -= movemousedelta;
		break;
	case MouseDown:
		y += movemousedelta;
		break;
	case MouseLeft:
		x -= movemousedelta;
		break;
	case MouseRight:
		x += movemousedelta;
		break;
	}

	x = MAX(x, 0);
	x = MIN(x, sw-1);

	y = MAX(y, 0);
	y = MIN(y, sh-1);

	XWarpPointer(dpy, None, root, 0, 0, 0, 0, x, y);
	XFlush(dpy);
}

void
clickmouse(const Arg *arg)
{
	int button = arg->i;
	XEvent ev;

	memset(&ev, 0, sizeof(ev));
	ev.xbutton.same_screen = True;
	ev.xbutton.subwindow = root;

	switch (button) {
	case MouseLBtn:
		ev.xbutton.button = Button1;
		break;
	case MouseMBtn:
		ev.xbutton.button = Button2;
		break;
	case MouseRBtn:
		ev.xbutton.button = Button3;
		break;
	}

	while (ev.xbutton.subwindow) {
		ev.xbutton.window = ev.xbutton.subwindow;
		XQueryPointer(dpy, ev.xbutton.window,
			      &ev.xbutton.root,
			      &ev.xbutton.subwindow,
			      &ev.xbutton.x_root,
			      &ev.xbutton.y_root,
			      &ev.xbutton.x,
			      &ev.xbutton.y,
			      &ev.xbutton.state);
	}

	ev.type = ButtonPress;
	if (XSendEvent(dpy, PointerWindow, True, ButtonPressMask, &ev) == 0)
		fprintf(stderr, "dwm: failed to send the button press event.\n");
	XFlush(dpy);

	usleep(100000);

	ev.type = ButtonRelease;
	if (XSendEvent(dpy, PointerWindow, True, ButtonReleaseMask, &ev) == 0)
		fprintf(stderr, "dwm: failed to send the button release event.\n");
	XFlush(dpy);
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by,
			  selmon->ww, bh);
	arrange(selmon);
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->sel || selmon->sel->isfloating)
		return;
	if (c == nexttiled(selmon->clients))
		if (!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

void
incnmaster(const Arg *arg)
{
	Tag *t = montotag(selmon);
	Client *c;
	int n;

	for (n = 0, c = nexttiled(selmon->clients);
	     c;
	     c = nexttiled(c->next), n++)
		;

	t->nmaster = MAX(t->nmaster + arg->i, 0);
	t->nmaster = MIN(t->nmaster, n);
	arrange(selmon);
}

void
setmfact(const Arg *arg)
{
	Tag *t = montotag(selmon);
	float f;

	f = arg->f < 1.0 ? arg->f + t->mfact : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;
	t->mfact = f;
	arrange(selmon);
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui < LENGTH(tags)) {
		selmon->sel->tag = arg->ui;
		focus(NULL);
		arrange(selmon);
	}
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel)
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next)
			;
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c);
			     c = c->next) ;
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating
	    || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
		       selmon->sel->w, selmon->sel->h, False);
	arrange(selmon);
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
mousemove(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer
	    (dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None,
	     cursor[CurMove], CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy,
			   MOUSEMASK | ExposureMask | SubstructureRedirectMask,
			   &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type] (&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (nx >= selmon->wx && nx <= selmon->wx + selmon->ww
			    && ny >= selmon->wy
			    && ny <= selmon->wy + selmon->wh) {
				if (abs(selmon->wx - nx) < snap)
					nx = selmon->wx;
				else if (abs
					 ((selmon->wx + selmon->ww) -
					  (nx + WIDTH(c))) < snap)
					nx = selmon->wx + selmon->ww - WIDTH(c);
				if (abs(selmon->wy - ny) < snap)
					ny = selmon->wy;
				else if (abs
					 ((selmon->wy + selmon->wh) -
					  (ny + HEIGHT(c))) < snap)
					ny = selmon->wy + selmon->wh -
					    HEIGHT(c);
				if (!c->isfloating
				    && (abs(nx - c->x) > snap
					|| abs(ny - c->y) > snap))
					togglefloating(NULL);
			}
			if (c->isfloating)
				resize(c, nx, ny, c->w, c->h, True);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
mouseresize(const Arg *arg)
{
	int ocx, ocy;
	int nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer
	    (dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None,
	     cursor[CurResize], CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
		     c->h + c->bw - 1);
	do {
		XMaskEvent(dpy,
			   MOUSEMASK | ExposureMask | SubstructureRedirectMask,
			   &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type] (&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx
			    && c->mon->wx + nw <= selmon->wx + selmon->ww
			    && c->mon->wy + nh >= selmon->wy
			    && c->mon->wy + nh <= selmon->wy + selmon->wh) {
				if (!c->isfloating
				    && (abs(nw - c->w) > snap
					|| abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (c->isfloating)
				resize(c, c->x, c->y, nw, nh, True);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
		     c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) ;
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, True);
	selmon = m;
	focus(NULL);
}

void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void
quit(const Arg *arg)
{
	running = False;
}

void
setlayout(const Arg *arg)
{
	Tag *t = montotag(selmon);

	t->lt = (arg->ui < 2) ? (arg->ui) : (t->lt ^ 1);

	strncpy(t->ltsymbol, layouts[t->lt].symbol, sizeof(t->ltsymbol));

	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void
view(const Arg *arg)
{
	if (arg->ui == selmon->tag[selmon->seltag])
		return;
	selmon->seltag ^= 1;
	if (arg->ui < LENGTH(tags))
		selmon->tag[selmon->seltag] = arg->ui;
	focus(NULL);
	arrange(selmon);
}

void
cycleview(const Arg *arg)
{
	int newtag;
	newtag = (int)selmon->tag[selmon->seltag] + arg->i;
	if (newtag < 0)
		newtag = LENGTH(tags) - 1;
	if (newtag >= LENGTH(tags))
		newtag = 0;
	selmon->seltag ^= 1;
	selmon->tag[selmon->seltag] = newtag;
	focus(NULL);
	arrange(selmon);
}

void
run(void)
{
	XEvent ev;
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type] (&ev);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0 };
	Monitor *m;
	Tag *t;

	view(&a);
	t = montotag(selmon);
	t->lt = ~0;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, False);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	bar_cleanupdc();
	XFreeCursor(dpy, cursor[CurNormal]);
	XFreeCursor(dpy, cursor[CurResize]);
	XFreeCursor(dpy, cursor[CurMove]);
	while (mons)
		cleanupmon(mons);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
initdc(void)
{
	bar_initdc();
}

void
setup(void)
{
	XSetWindowAttributes wa;
	int dummy = 0, xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
	sigchld(0);
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	initdc();
	updategeom();
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] =
	    XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMFullscreen] =
	    XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] =
	    XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] =
	    XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
	cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);
	updatebodys();
	updatebars();
	updatestatus();
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *)netatom, NetLast);
	wa.cursor = cursor[CurNormal];
	wa.event_mask =
	    SubstructureRedirectMask | SubstructureNotifyMask |
	    PointerMotionMask | EnterWindowMask | LeaveWindowMask |
	    StructureNotifyMask | PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	if (!XkbQueryExtension(dpy, &dummy, &dummy, &dummy,
			       &xkbmajor, &xkbminor))
		die("Cannot find a compatible version of the X Keyboard "
		    "Extension in the server.\n");
	grabkeys();
	focus(NULL);
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] =
		    { 0, LockMask, numlockmask, numlockmask | LockMask };
		KeyCode code;
		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code,
						 keys[i].mod | modifiers[j],
						 root, True, GrabModeAsync,
						 GrabModeAsync);
	}
}

void
grabbuttons(Client *c, Bool focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] =
		    { 0, LockMask, numlockmask, numlockmask | LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (focused) {
			for (i = 0; i < LENGTH(buttons); i++)
				if (buttons[i].click == ClkClientWin)
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabButton(dpy,
							    buttons[i].button,
							    buttons[i].
							    mask | modifiers[j],
							    c->win, False,
							    BUTTONMASK,
							    GrabModeAsync,
							    GrabModeSync, None,
							    None);
		} else
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				    BUTTONMASK, GrabModeAsync, GrabModeSync,
				    None, None);
	}
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
			    == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
initrc(void)
{
	const char *home;
	char rcfile[MAXPATHLEN];
	char cmd[MAXPATHLEN + 10];

	home = getenv("HOME");
	if (home == NULL)
		die("dwm: failed to get $HOME");

	sprintf(rcfile, "%s/.dwminitrc", home);
	if (access(rcfile, R_OK) != 0)
		return;

	sprintf(cmd, "sh %s", rcfile);
	if (system(cmd) != 0)
		die("dwm: failed to execute %s", rcfile);
}

int
main(int argc, char *argv[])
{
	if (argc != 1)
		die("usage: dwm\n");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display\n");
	checkotherwm();
	setup();
	scan();
	initrc();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return (EXIT_SUCCESS);
}
