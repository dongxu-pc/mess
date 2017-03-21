/* See LICENSE file for copyright and license details. */

static const char bar_font[]           = "Yuppy SC";
static const unsigned int bar_fontpx   = 18;
static const unsigned int bar_linepx   = 1;
static const unsigned int bar_borderpx = 2;
static const unsigned int bar_padding  = 1;

static const unsigned int borderpx     = 1;
static const unsigned int snap         = 32;
static const Bool showbar              = True;

static const long body_fg_color       = 0x394f70;
static const long body_bg_color       = 0x002B36;

static const long bar_fg_color_normal = 0x5d6059;
static const long bar_bg_color_normal = 0x131313;
static const long bar_fg_color_sel    = 0x000000;
static const long bar_bg_color_sel    = 0x0b4040;

//static const long border_color_normal = 0x303030;
static const long border_color_normal = 0x206092;
//static const long border_color_sel    = 0x206092;
static const long border_color_sel    = 0x8cff00;

static const long square_color_normal = 0x7f7f00;
static const long square_color_urgent = 0x75507b;

/* tagging */
static const char *tags[] =
	{ "劝", "君", "莫", "惜", "金", "缕", "衣",
	  "劝", "君", "惜", "取", "少", "年", "时", };

static const Rule rules[] = {
	/* class      instance   title   tag         isfloating   monitor */
};

/* layout(s) */
static const float mfact      = 0.50;
static const int nmaster      = 1;

static const Layout layouts[] = {
	{ "[0-0]",    tile },
	{ "[0]",      monocle },
};

/* commands */
static const char *termcmd[]  = { "xterm", NULL };
static const char *scrotcmd[] = { "scrot", "-s", "/home/dong/mess/pic/%Y-%m-%d_%H:%M:%S.png", NULL };
static const char *lockcmd[] = { "lockscreen", NULL };
static const char *filecmd[] = { "nautilus","--no-desktop", NULL };
static const char *menucmd[] = { "dmenu_run",NULL};

#define MODKEY Mod4Mask

static Key keys[] = {
	{ MODKEY,              XK_Return,    spawn,          {.v = termcmd } },
	{ MODKEY,              XK_y,         spawn,          {.v = lockcmd } },
	{ MODKEY,              XK_e,         spawn,          {.v = filecmd } },
	{ MODKEY|ShiftMask,    XK_p,         spawn,          {.v = scrotcmd } },
	{ MODKEY,			   XK_p,         spawn,          {.v = menucmd } },
	{ MODKEY,              XK_b,         togglebar,      {0} },
	{ MODKEY,              XK_j,         focusstack,     {.i = +1 } },
	{ MODKEY,              XK_k,         focusstack,     {.i = -1 } },
	{ MODKEY,              XK_Up,        focusmon,       {.i = +1 } },
	{ MODKEY,              XK_Down,      focusmon,       {.i = -1 } },
	{ MODKEY,              XK_i,         incnmaster,     {.i = +1 } },
	{ MODKEY,              XK_o,         incnmaster,     {.i = -1 } },
	{ MODKEY,              XK_h,         setmfact,       {.f = -0.05} },
	{ MODKEY,              XK_l,         setmfact,       {.f = +0.05} },
	{ MODKEY,              XK_space,     setmfact,       {.f = +1.50} },
	{ MODKEY|ShiftMask,    XK_Return,    zoom,           {0} },
	{ MODKEY|ShiftMask,    XK_z,         killclient,     {0} },
	{ MODKEY,              XK_n,         setlayout,      {.ui = 0} },
	{ MODKEY,              XK_m,         setlayout,      {.ui = 1} },
	{ MODKEY|ShiftMask,    XK_space,     togglefloating, {0} },
	{ MODKEY|ShiftMask,    XK_h,         movemouse,      {.i = MouseLeft  } },
	{ MODKEY|ShiftMask,    XK_j,         movemouse,      {.i = MouseDown  } },
	{ MODKEY|ShiftMask,    XK_k,         movemouse,      {.i = MouseUp    } },
	{ MODKEY|ShiftMask,    XK_l,         movemouse,      {.i = MouseRight } },
	{ MODKEY|ShiftMask,    XK_n,         clickmouse,     {.i = MouseLBtn  } },
	{ MODKEY|ShiftMask,    XK_m,         clickmouse,     {.i = MouseMBtn  } },
	{ MODKEY|ShiftMask,    XK_b,         clickmouse,     {.i = MouseRBtn  } },
	{ MODKEY,              XK_comma,     focusmon,       {.i = -1 } },
	{ MODKEY,              XK_period,    focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,    XK_comma,     tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,    XK_period,    tagmon,         {.i = +1 } },
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,              KEY,          view,           {.ui = TAG} }, \
	{ MODKEY|ShiftMask,    KEY,          tag,            {.ui = TAG} },
	TAGKEYS(               XK_1,                         0)
	TAGKEYS(               XK_2,                         1)
	TAGKEYS(               XK_3,                         2)
	TAGKEYS(               XK_4,                         3)
	TAGKEYS(               XK_5,                         4)
	TAGKEYS(               XK_6,                         5)
	TAGKEYS(               XK_7,                         6)
	TAGKEYS(               XK_8,                         7)
	TAGKEYS(               XK_9,                         8)
	TAGKEYS(               XK_0,                         9)
	TAGKEYS(               XK_minus,                     10)
	TAGKEYS(               XK_equal,                     11)
	TAGKEYS(               XK_backslash,                 12)
	TAGKEYS(               XK_grave,                     13)
	{ MODKEY,              XK_w,         cycleview,      {.i = +1 } },
	{ MODKEY,              XK_q,         cycleview,      {.i = -1 } },
	{ MODKEY,              XK_Right,     cycleview,      {.i = +1 } },
	{ MODKEY,              XK_Left,      cycleview,      {.i = -1 } },
	{ MODKEY,              XK_Tab,       view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask|ControlMask, XK_Delete, quit,     {0} },
};

static Button buttons[] = {
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        tag,            {0} },
	{ ClkTagBar,            0,              Button4,        cycleview,      {.i = +1} },
	{ ClkLtSymbol,          0,              Button4,        cycleview,      {.i = +1} },
	{ ClkWinTitle,          0,              Button4,        cycleview,      {.i = +1} },
	{ ClkTagBar,            0,              Button5,        cycleview,      {.i = -1} },
	{ ClkLtSymbol,          0,              Button5,        cycleview,      {.i = -1} },
	{ ClkWinTitle,          0,              Button5,        cycleview,      {.i = -1} },
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {.ui = ~0} },
	{ ClkClientWin,         MODKEY,         Button1,        mousemove,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        mouseresize,    {0} },
};
