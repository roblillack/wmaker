



#include "WINGsP.h"

#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <ctype.h>

#define CURSOR_BLINK_ON_DELAY	600
#define CURSOR_BLINK_OFF_DELAY	300



char *WMTextDidChangeNotification = "WMTextDidChangeNotification";
char *WMTextDidBeginEditingNotification = "WMTextDidBeginEditingNotification";
char *WMTextDidEndEditingNotification = "WMTextDidEndEditingNotification";


typedef struct W_TextField {
    W_Class widgetClass;
    W_View *view;

#if 0
    struct W_TextField *nextField;     /* next textfield in the chain */
    struct W_TextField *prevField;
#endif

    char *text;
    int textLen;		       /* size of text */
    int bufferSize;		       /* memory allocated for text */

    int viewPosition;		       /* position of text being shown */

    int cursorPosition;		       /* position of the insertion cursor */

    short usableWidth;
    short offsetWidth;		       /* offset of text from border */

    WMRange selection;

    WMFont *font;

    WMTextFieldDelegate *delegate;

#if 0
    WMHandlerID	timerID;	       /* for cursor blinking */
#endif
    struct {
	WMAlignment alignment:2;

	unsigned int bordered:1;

	unsigned int beveled:1;

	unsigned int enabled:1;

	unsigned int focused:1;

	unsigned int cursorOn:1;

	unsigned int secure:1;	       /* password entry style */

	unsigned int pointerGrabbed:1;

	/**/
	unsigned int notIllegalMovement:1;
    } flags;
} TextField;


#define NOTIFY(T,C,N,A)	{ WMNotification *notif = WMCreateNotification(N,T,A);\
			if ((T)->delegate && (T)->delegate->C)\
				(*(T)->delegate->C)((T)->delegate,notif);\
			WMPostNotification(notif);\
			WMReleaseNotification(notif);}


#define MIN_TEXT_BUFFER		2
#define TEXT_BUFFER_INCR	8


#define WM_EMACSKEYMASK   ControlMask

#define WM_EMACSKEY_LEFT  XK_b
#define WM_EMACSKEY_RIGHT XK_f
#define WM_EMACSKEY_HOME  XK_a
#define WM_EMACSKEY_END   XK_e
#define WM_EMACSKEY_BS    XK_h
#define WM_EMACSKEY_DEL   XK_d



#define DEFAULT_WIDTH		60
#define DEFAULT_HEIGHT		20
#define DEFAULT_BORDERED	True
#define DEFAULT_ALIGNMENT	WALeft



static void destroyTextField(TextField *tPtr);
static void paintTextField(TextField *tPtr);

static void handleEvents(XEvent *event, void *data);
static void handleTextFieldActionEvents(XEvent *event, void *data);
static void didResizeTextField();

struct W_ViewDelegate _TextFieldViewDelegate = {
    NULL,
	NULL,
	didResizeTextField,
	NULL,
	NULL
};


#define TEXT_WIDTH(tPtr, start)	(WMWidthOfString((tPtr)->font, \
		   &((tPtr)->text[(start)]), (tPtr)->textLen - (start) + 1))

#define TEXT_WIDTH2(tPtr, start, end) (WMWidthOfString((tPtr)->font, \
		   &((tPtr)->text[(start)]), (end) - (start) + 1))


static void
normalizeRange(TextField *tPtr, WMRange *range) /*fold00*/
{
    if (range->position < 0 && range->count < 0)
    	range->count = 0;

    if (range->count == 0) {
    	/*range->position = 0; why is this?*/
	return;
    }
    
    /* (1,-2) ~> (0,1) ; (1,-1) ~> (0,1) ; (2,-1) ~> (1,1) */
    if (range->count < 0) { /* && range->position >= 0 */
	if (range->position + range->count < 0) {
	    range->count = range->position;
	    range->position = 0;
	} else {
	    range->count = -range->count;
    	    range->position -= range->count;
	}
    /* (-2,1) ~> (0,0) ; (-1,1) ~> (0,0) ; (-1,2) ~> (0,1) */
    } else if (range->position < 0) { /* && range->count > 0 */
    	if (range->position + range->count < 0) {
	    range->position = range->count = 0;
	} else {
	    range->count += range->position;
	    range->position = 0;
	}
    }
    
    if (range->position + range->count > tPtr->textLen)
    	range->count = tPtr->textLen - range->position;
}

static void
memmv(char *dest, char *src, int size) /*fold00*/
{
    int i;
    
    if (dest > src) {
	for (i=size-1; i>=0; i--) {
	    dest[i] = src[i];
	}
    } else if (dest < src) {
	for (i=0; i<size; i++) {
	    dest[i] = src[i];
	}
    }
}


static int
incrToFit(TextField *tPtr) /*fold00*/
{
    int vp = tPtr->viewPosition;

    while (TEXT_WIDTH(tPtr, tPtr->viewPosition) > tPtr->usableWidth) {
	tPtr->viewPosition++;
    }
    return vp!=tPtr->viewPosition;
}


static int
incrToFit2(TextField *tPtr) /*fold00*/
{
    int vp = tPtr->viewPosition;
    while (TEXT_WIDTH2(tPtr, tPtr->viewPosition, tPtr->cursorPosition) 
	   >= tPtr->usableWidth)
	tPtr->viewPosition++;

    
    return vp!=tPtr->viewPosition;
}


static void
decrToFit(TextField *tPtr) /*fold00*/
{
    while (TEXT_WIDTH(tPtr, tPtr->viewPosition-1) < tPtr->usableWidth
	   && tPtr->viewPosition>0)
	tPtr->viewPosition--;
}

#undef TEXT_WIDTH
#undef TEXT_WIDTH2

static Bool
requestHandler(WMWidget *w, Atom selection, Atom target, Atom *type, /*fold00*/
	       void **value, unsigned *length, int *format) 
{
    TextField *tPtr = w;
    int count;
    Display *dpy = tPtr->view->screen->display;
    Atom _TARGETS;
    char *text;
    text = XGetAtomName(tPtr->view->screen->display,target);
    XFree(text);
    text = XGetAtomName(tPtr->view->screen->display,selection);
    XFree(text);

    *format = 32;
    *length = 0;
    *value = NULL;
    count = tPtr->selection.count < 0
        ? tPtr->selection.position + tPtr->selection.count
        : tPtr->selection.position;

    if (target == XA_STRING ||
            target == XInternAtom(dpy, "TEXT", False) ||
            target == XInternAtom(dpy, "COMPOUND_TEXT", False)) {
        *value = wstrdup(&(tPtr->text[count]));
        *length = abs(tPtr->selection.count);
        *format = 8;
        *type = target;
        return True;
    }
    
    _TARGETS = XInternAtom(dpy, "TARGETS", False);
    if (target == _TARGETS) {
        int *ptr;

        *length = 4;
        ptr = *value = (char *) wmalloc(4 * sizeof(Atom));
        ptr[0] = _TARGETS;
        ptr[1] = XA_STRING;
        ptr[2] = XInternAtom(dpy, "TEXT", False);
        ptr[3] = XInternAtom(dpy, "COMPOUND_TEXT", False);
        *type = target;
        return True;
    }
    /*
    *target = XA_PRIMARY;
    */
    return False;

}


static void
lostHandler(WMWidget *w, Atom selection) /*fold00*/
{
    TextField *tPtr = (WMTextField*)w;

    tPtr->selection.count = 0;
    paintTextField(tPtr);
}

static void
_notification(void *observerData, WMNotification *notification)
{
    WMTextField *to = (WMTextField*)observerData;
    WMTextField *tw = (WMTextField*)WMGetNotificationClientData(notification);
    if (to != tw) lostHandler(to, 0);
}

WMTextField*
WMCreateTextField(WMWidget *parent) /*fold00*/
{
    TextField *tPtr;

    tPtr = wmalloc(sizeof(TextField));
    memset(tPtr, 0, sizeof(TextField));

    tPtr->widgetClass = WC_TextField;
    
    tPtr->view = W_CreateView(W_VIEW(parent));
    if (!tPtr->view) {
	wfree(tPtr);
	return NULL;
    }
    tPtr->view->self = tPtr;

    tPtr->view->delegate = &_TextFieldViewDelegate;

    tPtr->view->attribFlags |= CWCursor;
    tPtr->view->attribs.cursor = tPtr->view->screen->textCursor;
    
    W_SetViewBackgroundColor(tPtr->view, tPtr->view->screen->white);
    
    tPtr->text = wmalloc(MIN_TEXT_BUFFER);
    tPtr->text[0] = 0;
    tPtr->textLen = 0;
    tPtr->bufferSize = MIN_TEXT_BUFFER;

    tPtr->flags.enabled = 1;
    
    WMCreateEventHandler(tPtr->view, ExposureMask|StructureNotifyMask
			 |FocusChangeMask, handleEvents, tPtr);

    tPtr->font = WMRetainFont(tPtr->view->screen->normalFont);

    tPtr->flags.bordered = DEFAULT_BORDERED;
    tPtr->flags.beveled = True;
    tPtr->flags.alignment = DEFAULT_ALIGNMENT;
    tPtr->offsetWidth = 
	WMAX((tPtr->view->size.height - WMFontHeight(tPtr->font))/2, 1);

    W_ResizeView(tPtr->view, DEFAULT_WIDTH, DEFAULT_HEIGHT);

    WMCreateEventHandler(tPtr->view, EnterWindowMask|LeaveWindowMask
			 |ButtonReleaseMask|ButtonPressMask|KeyPressMask|Button1MotionMask,
			 handleTextFieldActionEvents, tPtr);
    
    WMCreateSelectionHandler(tPtr, XA_PRIMARY, CurrentTime, requestHandler,
             lostHandler, NULL);
    WMAddNotificationObserver(_notification, tPtr, "_lostOwnership", tPtr);


    tPtr->flags.cursorOn = 1;

    return tPtr;
}


void
WMSetTextFieldDelegate(WMTextField *tPtr, WMTextFieldDelegate *delegate) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    tPtr->delegate = delegate;
}


void
WMInsertTextFieldText(WMTextField *tPtr, char *text, int position) /*fold00*/
{
    int len;
 
    CHECK_CLASS(tPtr, WC_TextField);
    
    if (!text)
	return;
    
    len = strlen(text);

    /* check if buffer will hold the text */
    if (len + tPtr->textLen >= tPtr->bufferSize) {
	tPtr->bufferSize = tPtr->textLen + len + TEXT_BUFFER_INCR;
	tPtr->text = wrealloc(tPtr->text, tPtr->bufferSize);
    }
    
    if (position < 0 || position >= tPtr->textLen) {
	/* append the text at the end */
	strcat(tPtr->text, text);
	
	incrToFit(tPtr);

	tPtr->textLen += len;
	tPtr->cursorPosition += len;
    } else {
	/* insert text at position */ 
	memmv(&(tPtr->text[position+len]), &(tPtr->text[position]),
                tPtr->textLen-position+1);

	memcpy(&(tPtr->text[position]), text, len);

	tPtr->textLen += len;
	if (position >= tPtr->cursorPosition) {
	    tPtr->cursorPosition += len;
	    incrToFit2(tPtr);
	} else {
	    incrToFit(tPtr);
	}
    }

    paintTextField(tPtr);
}

void
WMDeleteTextFieldRange(WMTextField *tPtr, WMRange range) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);

    normalizeRange(tPtr, &range);

    if (!range.count)
    	return;
    
    memmv(&(tPtr->text[range.position]), &(tPtr->text[range.position+range.count]),
            tPtr->textLen - (range.position+range.count) + 1);

    tPtr->textLen -= range.count;

    /* try to keep cursorPosition at the same place */
    tPtr->viewPosition -= range.count;
    if (tPtr->viewPosition < 0)
    	tPtr->viewPosition = 0;
    tPtr->cursorPosition = range.position;
    
    decrToFit(tPtr);

    paintTextField(tPtr);
}



char*
WMGetTextFieldText(WMTextField *tPtr) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
        
    return wstrdup(tPtr->text);
}


void
WMSetTextFieldText(WMTextField *tPtr, char *text) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    if ((text && strcmp(tPtr->text, text) == 0) ||
        (!text && tPtr->textLen == 0))
        return;

    if (text==NULL) {
	tPtr->text[0] = 0;
	tPtr->textLen = 0;
    } else {
	tPtr->textLen = strlen(text);
	
	if (tPtr->textLen >= tPtr->bufferSize) {
	    tPtr->bufferSize = tPtr->textLen + TEXT_BUFFER_INCR;
	    tPtr->text = wrealloc(tPtr->text, tPtr->bufferSize);
	}
	strcpy(tPtr->text, text);
    }

    tPtr->cursorPosition = tPtr->selection.position = tPtr->textLen;
    tPtr->viewPosition = 0;
    tPtr->selection.count = 0;
    
    if (tPtr->view->flags.realized)
	paintTextField(tPtr);
}


void
WMSetTextFieldAlignment(WMTextField *tPtr, WMAlignment alignment) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    tPtr->flags.alignment = alignment;
	
    if (alignment!=WALeft) {
	wwarning("only left alignment is supported in textfields");
	return;
    }
    
    if (tPtr->view->flags.realized) {
	paintTextField(tPtr);
    }
}


void
WMSetTextFieldBordered(WMTextField *tPtr, Bool bordered) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    tPtr->flags.bordered = bordered;

    if (tPtr->view->flags.realized) {
	paintTextField(tPtr);
    }
}


void
WMSetTextFieldBeveled(WMTextField *tPtr, Bool flag) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    tPtr->flags.beveled = flag;

    if (tPtr->view->flags.realized) {
	paintTextField(tPtr);
    }
}



void
WMSetTextFieldSecure(WMTextField *tPtr, Bool flag) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    tPtr->flags.secure = flag;
    
    if (tPtr->view->flags.realized) {
	paintTextField(tPtr);
    }    
}


Bool
WMGetTextFieldEditable(WMTextField *tPtr) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    return tPtr->flags.enabled;
}


void
WMSetTextFieldEditable(WMTextField *tPtr, Bool flag) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    tPtr->flags.enabled = flag;
    
    if (tPtr->view->flags.realized) {
	paintTextField(tPtr);
    }
}


void
WMSelectTextFieldRange(WMTextField *tPtr, WMRange range) /*FOLD00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    if (tPtr->flags.enabled) {
    	normalizeRange(tPtr, &range);

        tPtr->selection = range;

        tPtr->cursorPosition = range.position + range.count;

        if (tPtr->view->flags.realized) {
            paintTextField(tPtr);
        }
    }
}


void
WMSetTextFieldCursorPosition(WMTextField *tPtr, unsigned int position) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    if (tPtr->flags.enabled) {
        if (position > tPtr->textLen)
            position = tPtr->textLen;

        tPtr->cursorPosition = position;
        if (tPtr->view->flags.realized) {
            paintTextField(tPtr);
        }
    }
}


void
WMSetTextFieldNextTextField(WMTextField *tPtr, WMTextField *next) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    if (next == NULL) {
        if (tPtr->view->nextFocusChain)
            tPtr->view->nextFocusChain->prevFocusChain = NULL;
        tPtr->view->nextFocusChain = NULL;
        return;
    }

    CHECK_CLASS(next, WC_TextField);

    if (tPtr->view->nextFocusChain)
        tPtr->view->nextFocusChain->prevFocusChain = NULL;
    if (next->view->prevFocusChain)
        next->view->prevFocusChain->nextFocusChain = NULL;

    tPtr->view->nextFocusChain = next->view;
    next->view->prevFocusChain = tPtr->view;
}


void
WMSetTextFieldPrevTextField(WMTextField *tPtr, WMTextField *prev) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    if (prev == NULL) {
        if (tPtr->view->prevFocusChain)
            tPtr->view->prevFocusChain->nextFocusChain = NULL;
        tPtr->view->prevFocusChain = NULL;
        return;
    }

    CHECK_CLASS(prev, WC_TextField);

    if (tPtr->view->prevFocusChain)
        tPtr->view->prevFocusChain->nextFocusChain = NULL;
    if (prev->view->nextFocusChain)
        prev->view->nextFocusChain->prevFocusChain = NULL;

    tPtr->view->prevFocusChain = prev->view;
    prev->view->nextFocusChain = tPtr->view;
}


void 
WMSetTextFieldFont(WMTextField *tPtr, WMFont *font) /*fold00*/
{
    CHECK_CLASS(tPtr, WC_TextField);
    
    if (tPtr->font)
	WMReleaseFont(tPtr->font);
    tPtr->font = WMRetainFont(font);
  
    tPtr->offsetWidth = 
	WMAX((tPtr->view->size.height - WMFontHeight(tPtr->font))/2, 1);

    if (tPtr->view->flags.realized) {
	paintTextField(tPtr);
    }
}



WMFont*
WMGetTextFieldFont(WMTextField *tPtr) /*fold00*/
{
    return tPtr->font;
}


static void 
didResizeTextField(W_ViewDelegate *self, WMView *view) /*fold00*/
{
    WMTextField *tPtr = (WMTextField*)view->self;

    tPtr->offsetWidth = 
	WMAX((tPtr->view->size.height - WMFontHeight(tPtr->font))/2, 1);

    tPtr->usableWidth = tPtr->view->size.width - 2*tPtr->offsetWidth + 2;
}


static char*
makeHiddenString(int length) /*fold00*/
{
    char *data = wmalloc(length+1);

    memset(data, '*', length);
    data[length] = '\0';

    return data;
}


static void
paintCursor(TextField *tPtr) /*fold00*/
{
    int cx;
    WMScreen *screen = tPtr->view->screen;
    int textWidth;
    char *text;

    if (tPtr->flags.secure)
        text = makeHiddenString(strlen(tPtr->text));
    else
        text = tPtr->text;

    cx = WMWidthOfString(tPtr->font, &(text[tPtr->viewPosition]),
			 tPtr->cursorPosition-tPtr->viewPosition);

    switch (tPtr->flags.alignment) {
     case WARight:
	textWidth = WMWidthOfString(tPtr->font, text, tPtr->textLen);
	if (textWidth < tPtr->usableWidth)
	    cx += tPtr->offsetWidth + tPtr->usableWidth - textWidth + 1;
	else
	    cx += tPtr->offsetWidth + 1;
	break;
     case WALeft:
	cx += tPtr->offsetWidth + 1;
	break;
     case WAJustified:
	/* not supported */
     case WACenter:
	textWidth = WMWidthOfString(tPtr->font, text, tPtr->textLen);
	if (textWidth < tPtr->usableWidth)
	    cx += tPtr->offsetWidth + (tPtr->usableWidth-textWidth)/2;
	else
	    cx += tPtr->offsetWidth;
	break;
    }
    /*
    XDrawRectangle(screen->display, tPtr->view->window, screen->xorGC,
		   cx, tPtr->offsetWidth, 1,
		   tPtr->view->size.height - 2*tPtr->offsetWidth - 1);
	printf("%d %d\n",cx,tPtr->cursorPosition);
     */
    XDrawLine(screen->display, tPtr->view->window, screen->xorGC,
	      cx, tPtr->offsetWidth, cx,
	      tPtr->view->size.height - tPtr->offsetWidth - 1);

    if (tPtr->flags.secure)
        wfree(text);
}



static void
drawRelief(WMView *view, Bool beveled) /*fold00*/
{
    WMScreen *scr = view->screen;
    Display *dpy = scr->display;
    GC wgc;
    GC lgc;
    GC dgc;
    int width = view->size.width;
    int height = view->size.height;
    
    dgc = WMColorGC(scr->darkGray);

    if (!beveled) {
	XDrawRectangle(dpy, view->window, dgc, 0, 0, width-1, height-1);

	return;
    }
    wgc = WMColorGC(scr->white);
    lgc = WMColorGC(scr->gray);

    /* top left */
    XDrawLine(dpy, view->window, dgc, 0, 0, width-1, 0);
    XDrawLine(dpy, view->window, dgc, 0, 1, width-2, 1);
    
    XDrawLine(dpy, view->window, dgc, 0, 0, 0, height-2);
    XDrawLine(dpy, view->window, dgc, 1, 0, 1, height-3);
    
    /* bottom right */
    XDrawLine(dpy, view->window, wgc, 0, height-1, width-1, height-1);
    XDrawLine(dpy, view->window, lgc, 1, height-2, width-2, height-2);

    XDrawLine(dpy, view->window, wgc, width-1, 0, width-1, height-1);
    XDrawLine(dpy, view->window, lgc, width-2, 1, width-2, height-3);
}


static void
paintTextField(TextField *tPtr) /*fold00*/
{
    W_Screen *screen = tPtr->view->screen;
    W_View *view = tPtr->view;
    W_View viewbuffer;
    int tx, ty, tw, th;
    int rx;
    int bd;
    int totalWidth;
    char *text;
    Pixmap drawbuffer;

    
    if (!view->flags.realized || !view->flags.mapped)
	return;

    if (!tPtr->flags.bordered) {
	bd = 0;
    } else {
	bd = 2;
    }

    if (tPtr->flags.secure) {
        text = makeHiddenString(strlen(tPtr->text));
    } else {
        text = tPtr->text;
    }

    totalWidth = tPtr->view->size.width - 2*bd;
    
    drawbuffer = XCreatePixmap(screen->display, view->window,
            view->size.width, view->size.height, screen->depth);
    XFillRectangle(screen->display, drawbuffer, WMColorGC(screen->white),
            0,0, view->size.width,view->size.height);
    /* this is quite dirty */
    viewbuffer.screen = view->screen;
    viewbuffer.size = view->size;
    viewbuffer.window = drawbuffer;


    if (tPtr->textLen > 0) {
    	tw = WMWidthOfString(tPtr->font, &(text[tPtr->viewPosition]),
			     tPtr->textLen - tPtr->viewPosition);
    
	th = WMFontHeight(tPtr->font);

	ty = tPtr->offsetWidth;
	switch (tPtr->flags.alignment) {
	 case WALeft:
	    tx = tPtr->offsetWidth + 1;
	    if (tw < tPtr->usableWidth)
		XFillRectangle(screen->display, drawbuffer,
			       WMColorGC(screen->white),
			       bd+tw,bd, totalWidth-tw,view->size.height-2*bd);
	    break;
	
	 case WACenter:	    
	    tx = tPtr->offsetWidth + (tPtr->usableWidth - tw) / 2;
	    if (tw < tPtr->usableWidth)
		XClearArea(screen->display, view->window, bd, bd,
			   totalWidth, view->size.height-2*bd, False);
	    break;

	 default:
	 case WARight:
	    tx = tPtr->offsetWidth + tPtr->usableWidth - tw - 1;
	    if (tw < tPtr->usableWidth)
		XClearArea(screen->display, view->window, bd, bd,
			   totalWidth-tw, view->size.height-2*bd, False);
	    break;
	}

        if (!tPtr->flags.enabled)
            WMSetColorInGC(screen->darkGray, screen->textFieldGC);

        WMDrawImageString(screen, drawbuffer, screen->textFieldGC,
                          tPtr->font, tx, ty,
                          &(text[tPtr->viewPosition]),
                          tPtr->textLen - tPtr->viewPosition);

        if (tPtr->selection.count) {
            int count,count2;

            count = tPtr->selection.count < 0
                ? tPtr->selection.position + tPtr->selection.count
                : tPtr->selection.position;
            count2 = abs(tPtr->selection.count);
            if (count < tPtr->viewPosition) {
                count2 = abs(count2 - abs(tPtr->viewPosition - count));
                count = tPtr->viewPosition;
            }


            rx = tPtr->offsetWidth + 1 + WMWidthOfString(tPtr->font,text,count)
                - WMWidthOfString(tPtr->font,text,tPtr->viewPosition);

            XSetBackground(screen->display, screen->textFieldGC,
                    screen->gray->color.pixel);

            WMDrawImageString(screen, drawbuffer, screen->textFieldGC,
                    tPtr->font, rx, ty, &(text[count]),
                    count2);

            XSetBackground(screen->display, screen->textFieldGC,
                    screen->white->color.pixel);
        }

        if (!tPtr->flags.enabled)
            WMSetColorInGC(screen->black, screen->textFieldGC);
    } else {
            XFillRectangle(screen->display, drawbuffer,
			   WMColorGC(screen->white),
			   bd,bd, totalWidth,view->size.height-2*bd);
    }

    /* draw relief */
    if (tPtr->flags.bordered) {
        drawRelief(&viewbuffer, tPtr->flags.beveled);
    }

    if (tPtr->flags.secure)
        wfree(text);
    XCopyArea(screen->display, drawbuffer, view->window,
	      screen->copyGC, 0,0, view->size.width,
	      view->size.height,0,0);
    XFreePixmap(screen->display, drawbuffer);

    /* draw cursor */
    if (tPtr->flags.focused && tPtr->flags.enabled && tPtr->flags.cursorOn) {
        paintCursor(tPtr);
    }
}


#if 0
static void
blinkCursor(void *data) /*fold00*/
{
    TextField *tPtr = (TextField*)data;
    
    if (tPtr->flags.cursorOn) {
	tPtr->timerID = WMAddTimerHandler(CURSOR_BLINK_OFF_DELAY, blinkCursor,
					  data);
    } else {
	tPtr->timerID = WMAddTimerHandler(CURSOR_BLINK_ON_DELAY, blinkCursor,
					  data);	
    }
    paintCursor(tPtr);
    tPtr->flags.cursorOn = !tPtr->flags.cursorOn;
}
#endif


static void
handleEvents(XEvent *event, void *data) /*fold00*/
{
    TextField *tPtr = (TextField*)data;

    CHECK_CLASS(data, WC_TextField);


    switch (event->type) {
     case FocusIn:
	if (W_FocusedViewOfToplevel(W_TopLevelOfView(tPtr->view))!=tPtr->view)
	    return;
	tPtr->flags.focused = 1;
#if 0
	if (!tPtr->timerID) {
	    tPtr->timerID = WMAddTimerHandler(CURSOR_BLINK_ON_DELAY, 
					      blinkCursor, tPtr);
	}
#endif
	paintTextField(tPtr);

	NOTIFY(tPtr, didBeginEditing, WMTextDidBeginEditingNotification, NULL);

	tPtr->flags.notIllegalMovement = 0;
	break;

     case FocusOut:
	tPtr->flags.focused = 0;
#if 0
	if (tPtr->timerID)
	    WMDeleteTimerHandler(tPtr->timerID);
	tPtr->timerID = NULL;
#endif

	paintTextField(tPtr);
	if (!tPtr->flags.notIllegalMovement) {
	    NOTIFY(tPtr, didEndEditing, WMTextDidEndEditingNotification,
		   (void*)WMIllegalTextMovement);
	}
	break;

     case Expose:
	if (event->xexpose.count!=0)
	    break;
	paintTextField(tPtr);
	break;
	
     case DestroyNotify:
	destroyTextField(tPtr);
	break;
    }
}


static void
handleTextFieldKeyPress(TextField *tPtr, XEvent *event) /*FOLD00*/
{
    char buffer[64];
    KeySym ksym;
    char *textEvent = NULL;
    void *data;
    int count, refresh = 0;
    int control_pressed = 0;
    int cancelSelection = 1;

    /*printf("(%d,%d) -> ", tPtr->selection.position, tPtr->selection.count);*/
    if (((XKeyEvent *) event)->state & WM_EMACSKEYMASK)
	control_pressed = 1;

    count = XLookupString(&event->xkey, buffer, 63, &ksym, NULL);
    buffer[count] = '\0';

    switch (ksym) {
     case XK_Tab:
#ifdef XK_ISO_Left_Tab
     case XK_ISO_Left_Tab:
#endif
	if (event->xkey.state & ShiftMask) {
	    if (tPtr->view->prevFocusChain) {
		W_SetFocusOfTopLevel(W_TopLevelOfView(tPtr->view),
				     tPtr->view->prevFocusChain);
		tPtr->flags.notIllegalMovement = 1;
            }
            data = (void*)WMBacktabTextMovement;
	} else {
	    if (tPtr->view->nextFocusChain) {
		W_SetFocusOfTopLevel(W_TopLevelOfView(tPtr->view),
				     tPtr->view->nextFocusChain);
		tPtr->flags.notIllegalMovement = 1;
            }
            data = (void*)WMTabTextMovement;
	}
        textEvent = WMTextDidEndEditingNotification;
	break;

     case XK_Return:
        data = (void*)WMReturnTextMovement;
        textEvent = WMTextDidEndEditingNotification;
	break;

     case WM_EMACSKEY_LEFT:
	if (!control_pressed) {
	    goto normal_key;
	}
#ifdef XK_KP_Left
     case XK_KP_Left:
#endif
     case XK_Left:
	if (tPtr->cursorPosition > 0) {
	    paintCursor(tPtr);
            if (event->xkey.state & ControlMask) {
    	    	int i = tPtr->cursorPosition - 1;
				
		while (i > 0 && tPtr->text[i] != ' ') i--;
		while (i > 0 && tPtr->text[i] == ' ') i--;

		tPtr->cursorPosition = (i > 0) ? i + 1 : 0;
            } else
		tPtr->cursorPosition--;

	    if (tPtr->cursorPosition < tPtr->viewPosition) {
		tPtr->viewPosition = tPtr->cursorPosition;
		refresh = 1;
	    } else
		paintCursor(tPtr);
	}
	if (event->xkey.state & ShiftMask)
	    cancelSelection = 0;
	break;

    case WM_EMACSKEY_RIGHT:
        if (!control_pressed) {
            goto normal_key;
        }
#ifdef XK_KP_Right
    case XK_KP_Right:
#endif
    case XK_Right:
	if (tPtr->cursorPosition < tPtr->textLen) {
	    paintCursor(tPtr);
            if (event->xkey.state & ControlMask) {
    	    	int i = tPtr->cursorPosition;
				
    	    	while (tPtr->text[i] && tPtr->text[i] != ' ') i++;
    	    	while (tPtr->text[i] == ' ') i++;

    	    	tPtr->cursorPosition = i;
            } else {
               tPtr->cursorPosition++;
            }
	    while (WMWidthOfString(tPtr->font,
				&(tPtr->text[tPtr->viewPosition]),
				tPtr->cursorPosition-tPtr->viewPosition)
		   > tPtr->usableWidth) {
		tPtr->viewPosition++;
		refresh = 1;
	    }
	    if (!refresh)
		paintCursor(tPtr);
	}
	if (event->xkey.state & ShiftMask)
	    cancelSelection = 0;
	break;
	
    case WM_EMACSKEY_HOME:
        if (!control_pressed) {
            goto normal_key;
        }
#ifdef XK_KP_Home
    case XK_KP_Home:
#endif
    case XK_Home:
	if (tPtr->cursorPosition > 0) {
	    paintCursor(tPtr);
	    tPtr->cursorPosition = 0;
	    if (tPtr->viewPosition > 0) {
		tPtr->viewPosition = 0;
		refresh = 1;
	    } else
		paintCursor(tPtr);
	}
	if (event->xkey.state & ShiftMask)
	    cancelSelection = 0;
	break;
	
    case WM_EMACSKEY_END:
        if (!control_pressed) {
            goto normal_key;
        }
#ifdef XK_KP_End
    case XK_KP_End:
#endif
    case XK_End:
	if (tPtr->cursorPosition < tPtr->textLen) {
	    paintCursor(tPtr);
	    tPtr->cursorPosition = tPtr->textLen;
	    tPtr->viewPosition = 0;
	    while (WMWidthOfString(tPtr->font,
				   &(tPtr->text[tPtr->viewPosition]),
				   tPtr->textLen-tPtr->viewPosition)
		   > tPtr->usableWidth) {
		tPtr->viewPosition++;
		refresh = 1;
	    }
	    if (!refresh)
		paintCursor(tPtr);
	}
	if (event->xkey.state & ShiftMask)
	    cancelSelection = 0;
	break;
	
     case WM_EMACSKEY_BS:
         if (!control_pressed) {
             goto normal_key;
         }
     case XK_BackSpace:
     	if (tPtr->selection.count) {
	    WMDeleteTextFieldRange(tPtr, tPtr->selection);
            data = (void*)WMDeleteTextEvent;
            textEvent = WMTextDidChangeNotification;
	} else if (tPtr->cursorPosition > 0) {
            WMRange range;
            range.position = tPtr->cursorPosition - 1;
            range.count = 1;
	    WMDeleteTextFieldRange(tPtr, range);
            data = (void*)WMDeleteTextEvent;
            textEvent = WMTextDidChangeNotification;
	}
	break;
	
    case WM_EMACSKEY_DEL:
        if (!control_pressed) {
            goto normal_key;
        }
#ifdef XK_KP_Delete
    case XK_KP_Delete:
#endif
    case XK_Delete:
     	if (tPtr->selection.count) {
	    WMDeleteTextFieldRange(tPtr, tPtr->selection);
            data = (void*)WMDeleteTextEvent;
            textEvent = WMTextDidChangeNotification;
	} else if (tPtr->cursorPosition < tPtr->textLen) {
            WMRange range;
            range.position = tPtr->cursorPosition;
            range.count = 1;
	    WMDeleteTextFieldRange(tPtr, range);
            data = (void*)WMDeleteTextEvent;
            textEvent = WMTextDidChangeNotification;
        }
	break;

    normal_key:
     default:
	if (count > 0 && !iscntrl(buffer[0])) {
            if (tPtr->selection.count)
                WMDeleteTextFieldRange(tPtr, tPtr->selection);
	    WMInsertTextFieldText(tPtr, buffer, tPtr->cursorPosition);
            data = (void*)WMInsertTextEvent;
            textEvent = WMTextDidChangeNotification;
        } else {
            /* should we rather break and goto the notification code below? -Dan */
            return;
        }
	break;
    }

    if (!cancelSelection) {
    	if (tPtr->selection.count != tPtr->cursorPosition - tPtr->selection.position) {
	    WMNotification *notif;
	    
            tPtr->selection.count = tPtr->cursorPosition - tPtr->selection.position;

            XSetSelectionOwner(tPtr->view->screen->display,
                    	       XA_PRIMARY, tPtr->view->window,
			       event->xbutton.time);
            notif = WMCreateNotification("_lostOwnership", NULL, tPtr);
            WMPostNotification(notif);
            WMReleaseNotification(notif);

            refresh = 1;
        }
    } else {
    	if (tPtr->selection.count) {
    	    tPtr->selection.count = 0;
            refresh = 1;
	}
    	tPtr->selection.position = tPtr->cursorPosition;
    }

    /*printf("(%d,%d)\n", tPtr->selection.position, tPtr->selection.count);*/

    if (textEvent) {
        WMNotification *notif = WMCreateNotification(textEvent, tPtr, data);

        if (tPtr->delegate) {
            if (textEvent==WMTextDidBeginEditingNotification &&
                tPtr->delegate->didBeginEditing)
                (*tPtr->delegate->didBeginEditing)(tPtr->delegate, notif);
            else if (textEvent==WMTextDidEndEditingNotification &&
                     tPtr->delegate->didEndEditing)
                (*tPtr->delegate->didEndEditing)(tPtr->delegate, notif);
            else if (textEvent==WMTextDidChangeNotification &&
                     tPtr->delegate->didChange)
                (*tPtr->delegate->didChange)(tPtr->delegate, notif);
        }

        WMPostNotification(notif);
        WMReleaseNotification(notif);
    }

    if (refresh)
	paintTextField(tPtr);

    /*printf("(%d,%d)\n", tPtr->selection.position, tPtr->selection.count);*/
}


static int
pointToCursorPosition(TextField *tPtr, int x) /*fold00*/
{
    int a, b, mid;
    int tw;

    if (tPtr->flags.bordered)
	x -= 2;

    a = tPtr->viewPosition;
    b = tPtr->viewPosition + tPtr->textLen;
    if (WMWidthOfString(tPtr->font, &(tPtr->text[tPtr->viewPosition]), 
			tPtr->textLen - tPtr->viewPosition) < x)
	return tPtr->textLen;

    while (a < b && b-a>1) {
	mid = (a+b)/2;
	tw = WMWidthOfString(tPtr->font, &(tPtr->text[tPtr->viewPosition]), 
			     mid - tPtr->viewPosition);
	if (tw > x)
	    b = mid;
	else if (tw < x)
	    a = mid;
	else
	    return mid;
    }
    return (a+b)/2;
}


static void
handleTextFieldActionEvents(XEvent *event, void *data) /*fold00*/
{
    TextField *tPtr = (TextField*)data;
    static int move = 0;
    static Time lastButtonReleasedEvent = 0;
    
    CHECK_CLASS(data, WC_TextField);

    switch (event->type) {
     case KeyPress:
        if (tPtr->flags.enabled && tPtr->flags.focused) {
            handleTextFieldKeyPress(tPtr, event);
	    XGrabPointer(WMScreenDisplay(W_VIEW(tPtr)->screen),
			 W_VIEW(tPtr)->window, False, 
			 PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
			 GrabModeAsync, GrabModeAsync, None, 
			 W_VIEW(tPtr)->screen->invisibleCursor,
			 CurrentTime);
	    tPtr->flags.pointerGrabbed = 1;
	}
        break;

     case MotionNotify:
	
	if (tPtr->flags.pointerGrabbed) {
	    tPtr->flags.pointerGrabbed = 0;
	    XUngrabPointer(WMScreenDisplay(W_VIEW(tPtr)->screen), CurrentTime);
	}

        if (tPtr->flags.enabled && (event->xmotion.state & Button1Mask)) {

	    if (tPtr->viewPosition < tPtr->textLen && event->xmotion.x >
                tPtr->usableWidth) {
		if (WMWidthOfString(tPtr->font,
				    &(tPtr->text[tPtr->viewPosition]),
				    tPtr->cursorPosition-tPtr->viewPosition)
                    > tPtr->usableWidth) {
		    tPtr->viewPosition++;
		}
	    } else if (tPtr->viewPosition > 0 && event->xmotion.x < 0) {
		paintCursor(tPtr);
		tPtr->viewPosition--;
	    }

	    /*if (!tPtr->selection.count) {
		tPtr->selection.position = tPtr->cursorPosition;
	    }*/
	    
	    tPtr->cursorPosition = 
		pointToCursorPosition(tPtr, event->xmotion.x);
	    
	    tPtr->selection.count = tPtr->cursorPosition - tPtr->selection.position;
	    /*printf("(%d,%d)\n", tPtr->selection.position, tPtr->selection.count);*/
	    
	    /*
	     printf("notify %d %d\n",event->xmotion.x,tPtr->usableWidth);
	     */
	    
	    paintCursor(tPtr);
	    paintTextField(tPtr);

	}
        if (move) {
            XSetSelectionOwner(tPtr->view->screen->display,
                       XA_PRIMARY, tPtr->view->window,  event->xmotion.time);
            {
                WMNotification *notif = WMCreateNotification("_lostOwnership",
                        NULL,tPtr);
                WMPostNotification(notif);
                WMReleaseNotification(notif);
            }
        }
	break;

     case ButtonPress:
	if (tPtr->flags.pointerGrabbed) {
	    tPtr->flags.pointerGrabbed = 0;
	    XUngrabPointer(WMScreenDisplay(W_VIEW(tPtr)->screen), CurrentTime);
	    break;
	}

        move = 1;
        switch (tPtr->flags.alignment) {
            int textWidth;
         case WARight:
            textWidth = WMWidthOfString(tPtr->font, tPtr->text, tPtr->textLen);
            if (tPtr->flags.enabled && !tPtr->flags.focused) {
                WMSetFocusToWidget(tPtr);
            } else if (tPtr->flags.focused) {
		tPtr->selection.position = tPtr->cursorPosition;
                tPtr->selection.count = 0;
            }
            if(textWidth < tPtr->usableWidth){
                tPtr->cursorPosition = pointToCursorPosition(tPtr, 
				     event->xbutton.x - tPtr->usableWidth
                                     + textWidth);
            }
            else tPtr->cursorPosition = pointToCursorPosition(tPtr, 
                                     event->xbutton.x);
            /*
            tPtr->cursorPosition = pointToCursorPosition(tPtr, 
                                     event->xbutton.x);
                tPtr->cursorPosition += tPtr->usableWidth - textWidth;
            }

            tPtr->cursorPosition = pointToCursorPosition(tPtr, 
                                     event->xbutton.x);
                                     */
            paintTextField(tPtr);
            break;
        
         case WALeft:
            if (tPtr->flags.enabled && !tPtr->flags.focused) {
                WMSetFocusToWidget(tPtr);
                tPtr->cursorPosition = pointToCursorPosition(tPtr, 
                                     event->xbutton.x);
                paintTextField(tPtr);
            } else if (tPtr->flags.focused) {
                tPtr->cursorPosition = pointToCursorPosition(tPtr, 
                                     event->xbutton.x);
		tPtr->selection.position = tPtr->cursorPosition;
                tPtr->selection.count = 0;
                paintTextField(tPtr);
            }
            if (event->xbutton.button == Button2 && tPtr->flags.enabled) {
                char *text;

                text = W_GetTextSelection(tPtr->view->screen, XA_PRIMARY);
                
                if (!text) {
                    text = W_GetTextSelection(tPtr->view->screen,
                              tPtr->view->screen->clipboardAtom);
                }
                if (!text) {
		    text = W_GetTextSelection(tPtr->view->screen, 
					      XA_CUT_BUFFER0);
                }
                if (text) {
		    WMInsertTextFieldText(tPtr, text, tPtr->cursorPosition);
		    XFree(text);
                    NOTIFY(tPtr, didChange, WMTextDidChangeNotification,
                           (void*)WMInsertTextEvent);
                }
            }
            break;
        }
	break;

	case ButtonRelease:
	    if (tPtr->flags.pointerGrabbed) {
	    	tPtr->flags.pointerGrabbed = 0;
	    	XUngrabPointer(WMScreenDisplay(W_VIEW(tPtr)->screen), CurrentTime);
	    }

            move = 0;
		
	    if (event->xbutton.time - lastButtonReleasedEvent
	    	<= WINGsConfiguration.doubleClickDelay) {
	    	tPtr->selection.position = 0;
	    	tPtr->selection.count = tPtr->textLen;
	    	paintTextField(tPtr);
            	XSetSelectionOwner(tPtr->view->screen->display,
                       XA_PRIMARY, tPtr->view->window,  event->xbutton.time);
            	{
            	    WMNotification *notif = WMCreateNotification("_lostOwnership",
                        NULL,tPtr);
                    WMPostNotification(notif);
                    WMReleaseNotification(notif);
            	}
	    }
	    lastButtonReleasedEvent = event->xbutton.time;
		
        break;
    }
}


static void
destroyTextField(TextField *tPtr) /*fold00*/
{
#if 0
    if (tPtr->timerID)
	WMDeleteTimerHandler(tPtr->timerID);
#endif

    WMReleaseFont(tPtr->font);
    WMDeleteSelectionHandler(tPtr, XA_PRIMARY);
    WMRemoveNotificationObserver(tPtr);

    if (tPtr->text)
	wfree(tPtr->text);

    wfree(tPtr);
}
