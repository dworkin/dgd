# include <Resources.h>
# include <Quickdraw.h>
# include <Fonts.h>
# include <Windows.h>
# include <Menus.h>
# include <Devices.h>
# include <Events.h>
# include <TextEdit.h>
# include <Dialogs.h>
# include <Scrap.h>
# include <ToolUtils.h>
# include <SegLoad.h>
# include <OSUtils.h>
# include <Processes.h>
# define INCLUDE_FILE_IO
# include "dgd.h"


static bool argstart, running;
static jmp_buf env;
static char config_buf[STRINGSZ], dump_buf[STRINGSZ];
static char *config_file, *dump_file;


enum windows {
    ABOUT = 128,
    ALERT = 129,
    MAINFRAME = 130
};

enum aboutbox {
    FONTCHANGE = 5
};

/*
 * NAME:	changefont()
 * DESCRIPTION:	change the font used in the about box
 */
pascal static void changefont(WindowPtr window, short item)
{
    short id;
    
    GetFNum("\pgeneva", &id);
    TextFont(id);
    TextSize(9);
}

/*
 * NAME:	aboutbox()
 * DESCRIPTION:	display about box
 */
static void aboutbox(void)
{
    DialogPtr about;
    short itype, item;
    Handle handle;
    Rect rect;

    about = GetNewDialog(ABOUT, NULL, (WindowPtr) -1);
    GetDialogItem(about, FONTCHANGE, &itype, &handle, &rect);
    SetDialogItem(about, FONTCHANGE, itype,
		  (Handle) NewUserItemProc(changefont), &rect);
    ShowWindow(about);
    ModalDialog(NULL, &item);
    DisposeDialog(about);
}


enum screen {
    WIDTH = 80,
    HEIGHT = 25,
    CHARWIDTH = 6,
    CHARHEIGHT = 11
};

static WindowPtr mainframe;
static TEHandle te;
static int lines = 1;
static int linelength;
static bool selected;

/*
 * NAME:	windowstart()
 * DESCRIPTION:	create the output window
 */
static void windowstart(void)
{
    GrafPtr port;
    EventRecord evt;
    short id;
    Rect bounds;

    if (mainframe != NULL) {
	return;
    }

    GetPort(&port);
    mainframe = GetNewWindow(MAINFRAME, NULL, (WindowPtr) -1);
    GetNextEvent(nullEvent, &evt);
    ShowWindow(mainframe);
    SetPort(mainframe);
    GetFNum("\pmonaco", &id);
    TextFont(id);
    TextSize(9);
    bounds.top = bounds.left = 2;
    bounds.right = 2 + WIDTH * CHARWIDTH;
    bounds.bottom = 2 + HEIGHT * CHARHEIGHT;
    te = TENew(&bounds, &bounds);
    (*te)->crOnly = -1;
    selected = FALSE;
    SetPort(port);
}

/*
 * NAME:	windowupdt()
 * DESCRIPTION:	update a window
 */
static void windowupdt(WindowPtr window)
{
    BeginUpdate(window);
    TEUpdate(&window->portRect, te);
    EndUpdate(window);
}

/*
 * NAME:	windowact()
 * DESCRIPTION:	activate or deactivate a window
 */
static void windowact(int active)
{
    if (mainframe != NULL) {
	if (active) {
	    TEActivate(te);
	} else {
	    TEDeactivate(te);
	}
    }
}

/*
 * NAME:	P->message()
 * DESCRIPTION:	put a message on the mainframe window
 */
void P_message(char *mesg)
{
    GrafPtr port;
    static char cr[] = "\015";	/* CR */
    char *nl;
    int start, end, len;

    /* display message in window */
    GetPort(&port);
    SetPort(mainframe);
    if (selected) {
	start = (*te)->selStart;
	end = (*te)->selEnd;
	(*te)->selStart = (*te)->selEnd = (*te)->teLength;
    }
    do {
	nl = strchr(mesg, LF);
	len = (nl != NULL) ? nl - mesg : strlen(mesg);
	if (len > WIDTH - linelength) {
	    len = WIDTH - linelength;
	}
	TEInsert(mesg, len, te);
	linelength += len;

	if (linelength == WIDTH || mesg[len] == LF) {
	    TEInsert(cr, 1, te);
	    if (lines == HEIGHT) {
		(*te)->teLength -= linelength = (*te)->lineStarts[1];
		if (selected) {
		    start -= linelength;
		    end -= linelength;
		    if (end <= 0) {
			selected = FALSE;
		    } else if (start < 0) {
			start = 0;
		    }
		}
		TEScroll(0, -CHARHEIGHT, te);
		(*te)->destRect = (*te)->viewRect;
		Munger((*te)->hText, 0, 0, linelength, "", 0);
		TECalText(te);
		(*te)->selStart = (*te)->selEnd = (*te)->teLength;
	    } else {
		lines++;
	    }
	    linelength = 0;
	}
	mesg += len;
	if (mesg[0] == LF) {
	    mesg++;
	}
    } while (mesg[0] != '\0');
    TEScroll(0,  0, te);
    if (selected) {
	(*te)->selStart = start;
	(*te)->selEnd = end;
    }
    SetPort(port);
}


enum mbar {
    APPLE = 128,
    FILE = 129,
    EDIT = 130
};

enum mfile {
    CONFIG = 1,
    RESTORE = 2,
    START = 4,
    QUIT = 6
};

enum medit {
    UNDO = 1,
    CUT = 3,
    COPY = 4,
    PASTE = 5,
    CLEAR = 6,
    SELECT = 8
};

static MenuHandle applemenu, filemenu, editmenu;
static bool editing;

/*
 * NAME:	menuinit()
 * DESCRIPTION:	initialize menus
 */
static void menuinit(void)
{
    Handle mbar;

    mbar = GetNewMBar(128);
    SetMenuBar(mbar);
    DrawMenuBar();
    applemenu = GetMenuHandle(APPLE);
    filemenu = GetMenuHandle(FILE);
    editmenu = GetMenuHandle(EDIT);
    AppendResMenu(applemenu, 'DRVR');
    editing = FALSE;
}

/*
 * NAME:	setmenu()
 * DESCRIPTION:	set current menu options
 */
static void setmenu(void)
{
    WindowPeek wp;

    wp = (WindowPeek) FrontWindow();
    if (wp != NULL && wp->windowKind < 0) {
	/* enable edit menu for desk accessory */
	if (!editing) {
	    EnableItem(editmenu, 0);
	    DrawMenuBar();
	    editing = TRUE;
	}
	EnableItem(editmenu, UNDO);
	EnableItem(editmenu, CUT);
	EnableItem(editmenu, COPY);
	EnableItem(editmenu, PASTE);
	EnableItem(editmenu, CLEAR);
	DisableItem(editmenu, SELECT);
    } else if (mainframe != NULL) {
	DisableItem(editmenu, UNDO);
	DisableItem(editmenu, CUT);
	if (selected) {
	    EnableItem(editmenu, COPY);
	} else {
	    DisableItem(editmenu, COPY);
	}
	DisableItem(editmenu, PASTE);
	DisableItem(editmenu, CLEAR);
	if ((*te)->teLength != 0) {
	    EnableItem(editmenu, SELECT);
	} else {
	    DisableItem(editmenu, SELECT);
	}
    } else if (editing) {
	DisableItem(editmenu, 0);
	DrawMenuBar();
	editing = FALSE;
    }
    CheckItem(filemenu, RESTORE, dump_file != NULL);
}

/*
 * NANE:	menusel()
 * DESCRIPTION:	handle a menu command
 */
static bool menusel(long menuitem)
{
    int menu, item;
    Str255 name;
    GrafPtr port;
    char *file;

    menu = HiWord(menuitem);
    item = LoWord(menuitem);
    switch (menu) {
    case APPLE:
	if (item == 1) {
	    /* about box */
	    aboutbox();
	} else {
	    GetPort(&port);
	    GetMenuItemText(applemenu, item, name);
	    OpenDeskAcc(name);
	    SetPort(port);
	}
	break;

    case FILE:
	switch (item) {
	case CONFIG:
	    /* filedialog to obtain config file */
	    file = getfile(config_buf, 'pref');
	    if (file != NULL) {
		config_file = file;
		EnableItem(filemenu, START);
	    }
	    break;

	case RESTORE:
	    /* filedialog to obtain restore file */
	    dump_file = getfile(dump_buf, 'TEXT');
	    break;

	case START:
	    return TRUE;	/* start DGD */

	case QUIT:
	    if (running) {
		if (Alert(ALERT, NULL) != 2) {
		    interrupt();
		}
	    } else {
		ExitToShell();
	    }
	    break;
	}
	break;

    case EDIT:
	if (!SystemEdit(item - 1)) {
	    switch (item) {
	    case COPY:
		ZeroScrap();
		PutScrap((*te)->selEnd - (*te)->selStart, 'TEXT',
			 *(*te)->hText + (*te)->selStart);
		break;

	    case SELECT:
		TESetSelect(0, (*te)->teLength, te);
		selected = TRUE;
		break;
	    }
	}
	break;
    }

    return FALSE;
}

/*
 * NAME:	menurun()
 * DESCRIPTION:	change menus in preparation of running DGD
 */
static void menurun(void)
{
    HiliteMenu(0);
    DisableItem(filemenu, CONFIG);
    DisableItem(filemenu, RESTORE);
    DisableItem(filemenu, START);
    if (!editing) {
	EnableItem(editmenu, 0);
	DrawMenuBar();
	editing = TRUE;
    }
}

/*
 * NAME:	menustop()
 * DESCRIPTION:	change menus after stopping DGD
 */
static void menustop(void)
{
    EnableItem(filemenu, CONFIG);
    EnableItem(filemenu, RESTORE);
    EnableItem(filemenu, START);
}


/*
 * NAME:	getevent()
 * DESCRIPTION:	get and process the next event
 */
int getevent(void)
{
    EventRecord evt;
    WindowPtr window;
    Rect limits;
    GrafPtr port;

    HiliteMenu(0);
    SystemTask();
    setmenu();

    if (GetNextEvent(everyEvent, &evt)) {
	switch (evt.what) {
	case mouseDown:
	    switch (FindWindow(evt.where, &window)) {
	    case inSysWindow:
		SystemClick(&evt, window);
		break;

	    case inMenuBar:
		return menusel(MenuSelect(evt.where));

	    case inDrag:
		/* handle window drag */
		limits = qd.screenBits.bounds;
		limits.top += 4;
		limits.left += 4;
		limits.bottom -= 4;
		limits.right -= 4;
		DragWindow(window, evt.where, &limits);
		break;

	    case inContent:
		GetPort(&port);
		SetPort(window);
		GlobalToLocal(&evt.where);
		TEClick(evt.where, (evt.modifiers & shiftKey) ? TRUE : FALSE,
			te);
		if ((*te)->selStart != (*te)->selEnd) {
		    selected = TRUE;
		} else {
		    TEScroll(0, 0, te);
		    (*te)->selStart = (*te)->selEnd = (*te)->teLength;
		    selected = FALSE;
		}
		SetPort(port);
		break;
	    }
	    break;

	case keyDown:
	    if (evt.modifiers & cmdKey) {
		return menusel(MenuKey((char) (evt.message & charCodeMask)));
	    }
	    break;

	case activateEvt:
	    windowact(evt.modifiers & activeFlag);
	    break;

	case updateEvt:
	    windowupdt((WindowPtr) evt.message);
	    break;

	case osEvt:
	    if ((evt.message >> 24) == suspendResumeMessage) {
		windowact(evt.message & resumeFlag);
	    }
	    break;
	}
    }
    return FALSE;
}


/*
 * NAME:	getargs()
 * DESCRIPTION:	get arguments
 */
static bool getargs(void)
{
    short what, count, i;
    AppFile file;

    CountAppFiles(&what, &count);
    if (count == 0) {
	return FALSE;
    }
    if (count > 2 || what != appOpen) {
	ExitToShell();
    }

    for (i = 1; i <= count; i++) {
	GetAppFiles(i, &file);
	if (file.fType == 'pref') {
	    if (config_file != NULL) {
		/* 2 config files? */
		SysBeep(47);
		ExitToShell();
	    }
	    config_file = getpath(config_buf, file.vRefNum, file.fName);
	} else {
	    dump_file = getpath(dump_buf, file.vRefNum, file.fName);
	}
	ClrAppFiles(i);
    }

    if (config_file == NULL) {
	/* assume that the user accidentally clicked on a text file */
	dump_file = NULL;
	return FALSE;
    }

    return TRUE;
}

/*
 * NAME:	main()
 * DESCRIPTION:	main function called on program startup
 */
int main(int argc, char *argv[])
{
    char *args[3];

    /* initialize memory */
    SetApplLimit(GetApplLimit() - 262144);
    MaxApplZone();

    /* initialize Mac app */
    InitGraf((Ptr) &qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    FlushEvents(everyEvent, 0);
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    menuinit();

    /* initialize DGD */
    fsinit('DGD ', 'TEXT');
    tminit();
    P_srandom((long) P_time());

    if (getargs()) {
	argstart = TRUE;
    } else {
	setjmp(env);
	while (!getevent()) ;
    }

    menurun();
    windowstart();
    running = TRUE;

    argc = 2 + (dump_file != NULL);
    args[0] = "dgd";
    args[1] = config_file;
    args[2] = dump_file;
    args[3] = NULL;
    exit(dgd_main(argc, args));
}

/*
 * NAME:	exit()
 * DESCRIPTION:	Terminate the application gracefully
 */
void exit(int status)
{
    running = FALSE;
    if (status != 0 && !argstart) {
	menustop();
	longjmp(env, 1);
    }
    ExitToShell();
}

/*
 * NAME:	abort()
 * DESCRIPTION:	abort execution and terminate
 */
void abort(void)
{
    SysBeep(47);
    running = FALSE;
    for (;;) {
	/* loop until user quits */
	getevent();
    }
}
