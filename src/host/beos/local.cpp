# include <kernel/OS.h>
# include <app/Message.h>
# include <app/Messenger.h>
# include <app/Application.h>
# include <storage/FilePanel.h>
# include <storage/Path.h>
# include <interface/TextView.h>
# include <interface/Window.h>
# include <interface/MenuBar.h>
# include <interface/Menu.h>
# include <interface/MenuItem.h>
# include <interface/Button.h>
# include <interface/Alert.h>
# include <signal.h>


static int32 dgd_argc;		/* argument count */
static char **dgd_argv;		/* arguments */
static bool arg_started;	/* started with arguments? */
static bool dgd_running;	/* driver running? */
static bool dgd_fatal;		/* aborted with fatal error */
static BPath config_file;
static BPath restore_file;

extern "C" {
# include "dgd.h"
# include "version.h"

# undef bool
# undef exit

extern void conn_intr(void);

static thread_id driver;		/* driver thread */

/*
 * NAME:	term()
 * DESCRIPTION:	catch SIGTERM
 */
static void term(int arg)
{
    interrupt();
    conn_intr();
}

void dgd_exit(int code)
{
    dgd_running = FALSE;
    if (code != 0 && !arg_started) {
	exit_thread(code);
    } else {
	exit(code);
    }
}

void dgd_abort()
{
    dgd_running = FALSE;
    dgd_fatal = TRUE;
    exit_thread(2);
}

}


/*
 * NAME:	run_dgd()
 * DESCRIPTION:	actually run the driver
 */
static int32 run_dgd(void *data)
{
    char *argv[4];

    P_srandom((long) P_time());
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, term);

    if (!arg_started) {
	argv[0] = "DGD";
	argv[1] = (char *) config_file.Path();
	argv[2] = (char *) restore_file.Path();
	argv[3] = NULL;
	dgd_argv = argv;
	dgd_argc = (argv[2] == NULL) ? 2 : 3;
    }
    dgd_running = true;
    dgd_exit(dgd_main(dgd_argc, dgd_argv));
}

enum {
    DGD_CONFIG =	'dgdc',
    DGD_RESTORE =	'dgdr',
    DGD_START =		'dgds',
    DGD_MESSAGE =	'dgdm',

    DGD_CONFIG_DONE =	'dgdC',
    DGD_RESTORE_DONE =	'dgdR',

    DGD_LEFT =		47,
    DGD_TOP =		47,
    DGD_MENU =		19,
    DGD_WIDTH =		560,
    DGD_HEIGHT =	375,

    ABOUT_LEFT =	80,
    ABOUT_TOP =		80,
    ABOUT_WIDTH =	480,
    ABOUT_HEIGHT =	310,
    ABOUT_INDENT =	80,

    ABOUT_GRAY =	220
};


class LogView : public BTextView {
public:
    LogView();
    virtual void Select(int32 start, int32 finish);
    virtual void MakeFocus(bool flag = true);

    bool selected;
};

/*
 * NAME:	LogView->LogView()
 * DESCRIPTION:	LogView constructor
 */
LogView::LogView()
       : BTextView(BRect(2, DGD_MENU + 2, DGD_WIDTH + 2,
			 DGD_MENU + DGD_HEIGHT + 2),
		   NULL,
		   BRect(0, 0, DGD_WIDTH, DGD_HEIGHT),
		   be_fixed_font, NULL, 0, B_WILL_DRAW)
{
    MakeEditable(false);
    SetWordWrap(false);
    SetTabWidth(7);
    selected = false;
}

/*
 * NAME:	LogView->Select()
 * DESCRIPTION:	remember if anything was selected
 */
void LogView::Select(int32 start, int32 finish)
{
    selected = (start != finish);
    BTextView::Select(start, finish);
}

/*
 * NAME:	LogView->MakeFocus()
 * DESCRIPTION:	skip the BTextView MakeFocus stage
 */
void LogView::MakeFocus(bool flag)
{
    BView::MakeFocus(flag);
}


class MainFrame : public BWindow {
public:
    MainFrame();
    virtual void MessageReceived(BMessage *message);
    virtual bool QuitRequested(void);
    virtual void MenusBeginning(void);

private:
    BMenuItem *config;
    BMenuItem *restore;
    BMenuItem *start;
    BMenuItem *copy;
    BMenuItem *select;
    BMenuBar *menubar;
    LogView *log;
    short lines, length;
    short len[25];

    BFilePanel *config_panel;
    BFilePanel *restore_panel;
    entry_ref directory;
};


MainFrame::MainFrame()
	 : BWindow(BRect(DGD_LEFT, DGD_TOP, DGD_LEFT + DGD_WIDTH + 4,
			 DGD_TOP + DGD_MENU + DGD_HEIGHT + 4),
		   "DGD", B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE)
{
    BMenu *filemenu;
    BMenu *editmenu;
    BMenuItem *item;

    /* log window */
    log = new LogView();
    AddChild(log);
    log->MakeFocus();

    /* file menu */
    filemenu = new BMenu("File");
    config = new BMenuItem("Config File" B_UTF8_ELLIPSIS,
			   new BMessage(DGD_CONFIG), 'O');
    filemenu->AddItem(config);
    restore = new BMenuItem("Restore File" B_UTF8_ELLIPSIS,
			    new BMessage(DGD_RESTORE), 'R');
    filemenu->AddItem(restore);
    filemenu->AddItem(new BSeparatorItem());
    start = new BMenuItem("Start DGD", new BMessage(DGD_START), 'S');
    filemenu->AddItem(start);
    filemenu->SetTargetForItems(this);
    filemenu->AddItem(new BSeparatorItem());
    item = new BMenuItem("About DGD" B_UTF8_ELLIPSIS,
			 new BMessage(B_ABOUT_REQUESTED));
    item->SetTarget(be_app);
    filemenu->AddItem(item);
    filemenu->AddItem(new BSeparatorItem());
    item = new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q');
    item->SetTarget(this);
    filemenu->AddItem(item);

    /* edit menu */
    editmenu = new BMenu("Edit");
    item = new BMenuItem("Undo", NULL, 'Z');
    item->SetEnabled(false);
    editmenu->AddItem(item);
    editmenu->AddItem(new BSeparatorItem());
    item = new BMenuItem("Cut", NULL, 'X');
    item->SetEnabled(false);
    editmenu->AddItem(item);
    copy = new BMenuItem("Copy", new BMessage(B_COPY), 'C');
    editmenu->AddItem(copy);
    item = new BMenuItem("Paste", NULL, 'V');
    item->SetEnabled(false);
    editmenu->AddItem(item);
    item = new BMenuItem("Clear", NULL);
    item->SetEnabled(false);
    editmenu->AddItem(item);
    editmenu->AddItem(new BSeparatorItem());
    select = new BMenuItem("Select All", new BMessage(B_SELECT_ALL), 'A');
    editmenu->AddItem(select);
    editmenu->SetTargetForItems(log);

    /* menu bar */
    menubar = new BMenuBar(BRect(0, 0, 0, 0), NULL);
    menubar->AddItem(filemenu);
    menubar->AddItem(editmenu);
    AddChild(menubar);

    lines = 0;
    length = 0;

    config_panel = NULL;
    restore_panel = NULL;
}

void MainFrame::MessageReceived(BMessage *message)
{
    entry_ref ref;

    switch (message->what) {
    case DGD_CONFIG:
	if (config_panel == NULL) {
	    config_panel = new BFilePanel(B_OPEN_PANEL, &BMessenger(this),
					  &directory, 0, false,
					  &BMessage(DGD_CONFIG_DONE));
	    config_panel->Window()->SetTitle("DGD: Config File");
	}
	config_panel->Show();
	break;

    case DGD_RESTORE:
	if (restore_panel == NULL) {
	    restore_panel = new BFilePanel(B_OPEN_PANEL, &BMessenger(this),
					   &directory, 0, false,
					   &BMessage(DGD_RESTORE_DONE));
	    restore_panel->Window()->SetTitle("DGD: Restore File");
	}
	restore_panel->Show();
	break;

    case DGD_START:
	driver = spawn_thread(run_dgd, "driver", B_NORMAL_PRIORITY, NULL);
	resume_thread(driver);
	break;

    case B_CANCEL:
	if (message->FindInt32("old_what") == DGD_CONFIG_DONE) {
	    config_panel->GetPanelDirectory(&directory);
	    delete config_panel;
	    config_panel = NULL;
	} else {
	    restore_panel->GetPanelDirectory(&directory);
	    delete restore_panel;
	    restore_panel = NULL;
	}
	break;

    case DGD_CONFIG_DONE:
	message->FindRef("refs", 0, &ref);
	BEntry(&ref).GetPath(&config_file);
	break;

    case DGD_RESTORE_DONE:
	message->FindRef("refs", 0, &ref);
	BEntry(&ref).GetPath(&restore_file);
	break;

    case DGD_MESSAGE:
	char *mesg, *p;
	int32 start, finish;
	bool eoln;
	short n;

	message->FindString("mesg", (const char **) &mesg);
	if (log->selected) {
	    log->GetSelection(&start, &finish);
	} else {
	    start = finish = 0;
	}

	if (lines == 0) {
	    /* first line */
	    select->SetEnabled(true);
	    lines = 1;
	    len[0] = 0;
	}

	eoln = false;
	for (;;) {
	    if (eoln || len[lines - 1] == 80) {
		if (!eoln) {
		    log->Insert(length, "\n", 1);
		    len[lines - 1]++;
		    length++;
		}
		if (lines == 25) {
		    /* delete first line */
		    log->Delete(0, len[0]);
		    length -= len[0];
		    start -= len[0];
		    finish -= len[0];
		    memcpy(len, len + 1, 24 * sizeof(short));
		} else {
		    /* add new line */
		    lines++;
		}
		len[lines - 1] = 0;
	    }
	    if (mesg[0] == '\0') {
		if (start < 0) {
		    start = 0;
		}
		if (finish < 0) {
		    finish = 0;
		}
		if (log->selected) {
		    log->Select(start, finish);
		}
		break;
	    }

	    p = strchr(mesg, LF);
	    if (p != NULL && len[lines - 1] + (n=++p - mesg) <= 81) {
		eoln = true;
	    } else {
		eoln = false;
		n = strlen(mesg);
		if (n + len[lines - 1] > 80) {
		    /* wrap line */
		    n = 80 - len[lines - 1];
		}
	    }

	    log->Insert(length, mesg, n);
	    mesg += n;
	    len[lines - 1] += n;
	    length += n;
	}
	break;

    default:
	BWindow::MessageReceived(message);
	break;
    }
}

bool MainFrame::QuitRequested(void)
{
    if (!dgd_running ||
	(new BAlert("",
		    "Are you sure you want to terminate the running process?",
		    "Yes", "No", NULL, B_WIDTH_AS_USUAL,
		    B_WARNING_ALERT))->Go() == 0) {
	be_app->PostMessage(&BMessage(B_QUIT_REQUESTED), NULL);
    }
    return false;
}

void MainFrame::MenusBeginning(void)
{
    config->SetEnabled(!dgd_running && !dgd_fatal);
    restore->SetEnabled(!dgd_running && !dgd_fatal);
    restore->SetMarked(restore_file.Path() != NULL);
    start->SetEnabled(!dgd_running && !dgd_fatal &&
		      config_file.Path() != NULL &&
		      config_panel == NULL && restore_panel == NULL);
    copy->SetEnabled(log->selected);
    select->SetEnabled(lines != 0);
}


static MainFrame *mainframe;

class DGD : public BApplication {
public:
    DGD();
    virtual void ArgvReceived(int32 argc, char **argv);
    virtual void RefsReceived(BMessage *message);
    virtual void AboutRequested(void);
    virtual bool QuitRequested(void);

/* private: icon */
};

DGD::DGD() : BApplication("application/x-DGD")
{
    mainframe = new MainFrame();
    mainframe->Show();
}

void DGD::ArgvReceived(int32 argc, char **argv)
{
    arg_started = true;
    driver = spawn_thread(run_dgd, "driver", B_NORMAL_PRIORITY, NULL);
    resume_thread(driver);
}

void DGD::RefsReceived(BMessage *message)
{
}

void DGD::AboutRequested(void)
{
    BWindow *about;
    BTextView *view;
    BFont font;
    uint32 dummy;
    char buf[100];
    BButton *button;

    about = new BWindow(BRect(ABOUT_LEFT, ABOUT_TOP, ABOUT_WIDTH + ABOUT_LEFT,
			      ABOUT_HEIGHT + ABOUT_TOP),
			NULL, B_MODAL_WINDOW, B_NOT_MOVABLE | B_NOT_RESIZABLE);
    view = new BTextView(BRect(0, 0, ABOUT_WIDTH, ABOUT_INDENT), NULL,
			 BRect(ABOUT_INDENT, 20, ABOUT_WIDTH - 10,
			       ABOUT_INDENT - 10),
			 0, B_WILL_DRAW);
    view->SetViewColor(ABOUT_GRAY, ABOUT_GRAY, ABOUT_GRAY);
    view->MakeEditable(false);
    view->MakeSelectable(false);
    view->GetFontAndColor(&font, &dummy);
    font.SetSize(font.Size() * 1.1);
    view->SetFontAndColor(&font);
    about->AddChild(view);
    sprintf(buf, "\
DGD %s\n© 1993 - 2009 Dworkin B.V. All Rights Reserved.", VERSION);
    view->SetText(buf);
    view = new BTextView(BRect(0, ABOUT_INDENT, ABOUT_WIDTH, ABOUT_HEIGHT),
			 NULL,
			 BRect(10, 0, ABOUT_WIDTH - 10,
			       ABOUT_HEIGHT - ABOUT_INDENT - 10),
			 0, B_WILL_DRAW);
    view->SetViewColor(ABOUT_GRAY, ABOUT_GRAY, ABOUT_GRAY);
    view->MakeEditable(false);
    view->MakeSelectable(false);
    about->AddChild(view);
    view->SetText("\
Permission is granted to copy the source and executables made therefrom, but \
any commercial distribution or use whatsoever is not allowed. Commercial \
distribution or use refers to any distribution or use from which any form of \
income is received regardless of profit therefrom, or from which any revenue \
or promotional value is received, as well as any distribution to or use in a \
corporate environment. Use of the source or executables made therefrom to \
promote or support a commercial venture is included in this restriction.\n\n\
Any modifications of this program lacking an explicit copyright notice are \
subject to the copyright notice stated herein. Any explicit copyright \
notices added to this program shall not contradict the terms stated herein, \
but may further restrict distribution or use. Any modifications should be \
made in such a way that it is clear that this copyright notice applies to \
the source, executables made therefrom and any derivatives thereof and in \
such a way that it is possible to regain the unmodified source.");
    button = new BButton(BRect(ABOUT_WIDTH / 2 - 30,
			 ABOUT_HEIGHT - ABOUT_INDENT - 40, ABOUT_WIDTH / 2 + 30,
			 ABOUT_HEIGHT - ABOUT_INDENT - 20),
			 NULL, "OK", new BMessage(B_QUIT_REQUESTED));
    view->AddChild(button);
    button->MakeDefault(true);
    button->SetTarget(about);
    about->Show();
}

bool DGD::QuitRequested()
{
    if (dgd_running) {
	interrupt();
	conn_intr();
	return false;
    }
    return true;
}


/*
 * NAME:	main()
 * DESCRIPTION:	main program
 */
int main(int argc, char *argv[])
{
    DGD *app;

    dgd_argc = argc;
    dgd_argv = argv;
    app = new DGD();
    app->Run();
    delete app;
    return 0;
}

/*
 * NAME:	P->message()
 * DESCRIPTION:	show message
 */
extern "C" void P_message(char *mesg)
{
    BMessage message(DGD_MESSAGE);

    message.AddString("mesg", mesg);
    mainframe->PostMessage(&message, NULL);
}
