// Scintilla source code edit control
// ScintillaGTK.h - GTK+ specific subclass of ScintillaBase
// Copyright 1998-2004 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef SCINTILLAGTK_H
#define SCINTILLAGTK_H

#ifdef SCI_NAMESPACE
namespace Scintilla {
#endif

class ScintillaGTKAccessible;

#define OBJECT_CLASS GObjectClass

class ScintillaGTK : public ScintillaBase {
	friend class ScintillaGTKAccessible;

	_ScintillaObject *sci;
	Window wText;
	Window scrollbarv;
	Window scrollbarh;
	GtkAdjustment *adjustmentv;
	GtkAdjustment *adjustmenth;
	int verticalScrollBarWidth;
	int horizontalScrollBarHeight;

	SelectionText primary;

	GdkEventButton *evbtn;
	bool capturedMouse;
	bool dragWasDropped;
	int lastKey;
	int rectangularSelectionModifier;

	GtkWidgetClass *parentClass;

	static GdkAtom atomClipboard;
	static GdkAtom atomUTF8;
	static GdkAtom atomString;
	static GdkAtom atomUriList;
	static GdkAtom atomDROPFILES_DND;
	GdkAtom atomSought;

#if PLAT_GTK_WIN32
	CLIPFORMAT cfColumnSelect;
#endif

	Window wPreedit;
	Window wPreeditDraw;
	GtkIMContext *im_context;
	PangoScript lastNonCommonScript;

	// Wheel mouse support
	unsigned int linesPerScroll;
	GTimeVal lastWheelMouseTime;
	gint lastWheelMouseDirection;
	gint wheelMouseIntensity;

#if GTK_CHECK_VERSION(3,0,0)
	cairo_rectangle_list_t *rgnUpdate;
#else
	GdkRegion *rgnUpdate;
#endif
	bool repaintFullWindow;

	guint styleIdleID;
	AtkObject *accessible;

	// Private so ScintillaGTK objects can not be copied
	ScintillaGTK(const ScintillaGTK &);
	ScintillaGTK &operator=(const ScintillaGTK &);

public:
	explicit ScintillaGTK(_ScintillaObject *sci_);
	virtual ~ScintillaGTK();
	static ScintillaGTK *FromWidget(GtkWidget *widget);
	static void ClassInit(OBJECT_CLASS* object_class, GtkWidgetClass *widget_class, GtkContainerClass *container_class);
private:
	virtual void Initialise();
	virtual void Finalise();
	virtual bool AbandonPaint();
	virtual void DisplayCursor(Window::Cursor c);
	virtual bool DragThreshold(Point ptStart, Point ptNow);
	virtual void StartDrag();
	int TargetAsUTF8(char *text);
	int EncodedFromUTF8(char *utf8, char *encoded) const;
	virtual bool ValidCodePage(int codePage) const;
public: 	// Public for scintilla_send_message
	virtual sptr_t WndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam);
private:
	virtual sptr_t DefWndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam);
	struct TimeThunk {
		TickReason reason;
		ScintillaGTK *scintilla;
		guint timer;
		TimeThunk() : reason(tickCaret), scintilla(NULL), timer(0) {}
	};
	TimeThunk timers[tickDwell+1];
	virtual bool FineTickerAvailable();
	virtual bool FineTickerRunning(TickReason reason);
	virtual void FineTickerStart(TickReason reason, int millis, int tolerance);
	virtual void FineTickerCancel(TickReason reason);
	virtual bool SetIdle(bool on);
	virtual void SetMouseCapture(bool on);
	virtual bool HaveMouseCapture();
	virtual bool PaintContains(PRectangle rc);
	void FullPaint();
	virtual PRectangle GetClientRectangle() const;
	virtual void ScrollText(int linesToMove);
	virtual void SetVerticalScrollPos();
	virtual void SetHorizontalScrollPos();
	virtual bool ModifyScrollBars(int nMax, int nPage);
	void ReconfigureScrollBars();
	virtual void NotifyChange();
	virtual void NotifyFocus(bool focus);
	virtual void NotifyParent(SCNotification scn);
	void NotifyKey(int key, int modifiers);
	void NotifyURIDropped(const char *list);
	const char *CharacterSetID() const;
	virtual CaseFolder *CaseFolderForEncoding();
	virtual std::string CaseMapString(const std::string &s, int caseMapping);
	virtual int KeyDefault(int key, int modifiers);
	virtual void CopyToClipboard(const SelectionText &selectedText);
	virtual void Copy();
	virtual void Paste();
	virtual void CreateCallTipWindow(PRectangle rc);
	virtual void AddToPopUp(const char *label, int cmd = 0, bool enabled = true);
	bool OwnPrimarySelection();
	virtual void ClaimSelection();
	void GetGtkSelectionText(GtkSelectionData *selectionData, SelectionText &selText);
	void ReceivedSelection(GtkSelectionData *selection_data);
	void ReceivedDrop(GtkSelectionData *selection_data);
	static void GetSelection(GtkSelectionData *selection_data, guint info, SelectionText *selected);
	void StoreOnClipboard(SelectionText *clipText);
	static void ClipboardGetSelection(GtkClipboard* clip, GtkSelectionData *selection_data, guint info, void *data);
	static void ClipboardClearSelection(GtkClipboard* clip, void *data);

	void UnclaimSelection(GdkEventSelection *selection_event);
	void Resize(int width, int height);

	// Callback functions
	void RealizeThis(GtkWidget *widget);
	static void Realize(GtkWidget *widget);
	void UnRealizeThis(GtkWidget *widget);
	static void UnRealize(GtkWidget *widget);
	void MapThis();
	static void Map(GtkWidget *widget);
	void UnMapThis();
	static void UnMap(GtkWidget *widget);
	gint FocusInThis(GtkWidget *widget);
	static gint FocusIn(GtkWidget *widget, GdkEventFocus *event);
	gint FocusOutThis(GtkWidget *widget);
	static gint FocusOut(GtkWidget *widget, GdkEventFocus *event);
	static void SizeRequest(GtkWidget *widget, GtkRequisition *requisition);
#if GTK_CHECK_VERSION(3,0,0)
	static void GetPreferredWidth(GtkWidget *widget, gint *minimalWidth, gint *naturalWidth);
	static void GetPreferredHeight(GtkWidget *widget, gint *minimalHeight, gint *naturalHeight);
#endif
	static void SizeAllocate(GtkWidget *widget, GtkAllocation *allocation);
#if GTK_CHECK_VERSION(3,0,0)
	gboolean DrawTextThis(cairo_t *cr);
	static gboolean DrawText(GtkWidget *widget, cairo_t *cr, ScintillaGTK *sciThis);
	gboolean DrawThis(cairo_t *cr);
	static gboolean DrawMain(GtkWidget *widget, cairo_t *cr);
#else
	gboolean ExposeTextThis(GtkWidget *widget, GdkEventExpose *ose);
	static gboolean ExposeText(GtkWidget *widget, GdkEventExpose *ose, ScintillaGTK *sciThis);
	gboolean Expose(GtkWidget *widget, GdkEventExpose *ose);
	static gboolean ExposeMain(GtkWidget *widget, GdkEventExpose *ose);
#endif
	void ForAll(GtkCallback callback, gpointer callback_data);
	static void MainForAll(GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);

	static void ScrollSignal(GtkAdjustment *adj, ScintillaGTK *sciThis);
	static void ScrollHSignal(GtkAdjustment *adj, ScintillaGTK *sciThis);
	gint PressThis(GdkEventButton *event);
	static gint Press(GtkWidget *widget, GdkEventButton *event);
	static gint MouseRelease(GtkWidget *widget, GdkEventButton *event);
	static gint ScrollEvent(GtkWidget *widget, GdkEventScroll *event);
	static gint Motion(GtkWidget *widget, GdkEventMotion *event);
	gboolean KeyThis(GdkEventKey *event);
	static gboolean KeyPress(GtkWidget *widget, GdkEventKey *event);
	static gboolean KeyRelease(GtkWidget *widget, GdkEventKey *event);
#if GTK_CHECK_VERSION(3,0,0)
	gboolean DrawPreeditThis(GtkWidget *widget, cairo_t *cr);
	static gboolean DrawPreedit(GtkWidget *widget, cairo_t *cr, ScintillaGTK *sciThis);
#else
	gboolean ExposePreeditThis(GtkWidget *widget, GdkEventExpose *ose);
	static gboolean ExposePreedit(GtkWidget *widget, GdkEventExpose *ose, ScintillaGTK *sciThis);
#endif
	AtkObject* GetAccessibleThis(GtkWidget *widget);
	static AtkObject* GetAccessible(GtkWidget *widget);

	bool KoreanIME();
	void CommitThis(char *str);
	static void Commit(GtkIMContext *context, char *str, ScintillaGTK *sciThis);
	void PreeditChangedInlineThis();
	void PreeditChangedWindowedThis();
	static void PreeditChanged(GtkIMContext *context, ScintillaGTK *sciThis);
	void MoveImeCarets(int pos);
	void DrawImeIndicator(int indicator, int len);
	void SetCandidateWindowPos();

	static void StyleSetText(GtkWidget *widget, GtkStyle *previous, void*);
	static void RealizeText(GtkWidget *widget, void*);
	static void Dispose(GObject *object);
	static void Destroy(GObject *object);
	static void SelectionReceived(GtkWidget *widget, GtkSelectionData *selection_data,
	                              guint time);
	static void SelectionGet(GtkWidget *widget, GtkSelectionData *selection_data,
	                         guint info, guint time);
	static gint SelectionClear(GtkWidget *widget, GdkEventSelection *selection_event);
	gboolean DragMotionThis(GdkDragContext *context, gint x, gint y, guint dragtime);
	static gboolean DragMotion(GtkWidget *widget, GdkDragContext *context,
	                           gint x, gint y, guint dragtime);
	static void DragLeave(GtkWidget *widget, GdkDragContext *context,
	                      guint time);
	static void DragEnd(GtkWidget *widget, GdkDragContext *context);
	static gboolean Drop(GtkWidget *widget, GdkDragContext *context,
	                     gint x, gint y, guint time);
	static void DragDataReceived(GtkWidget *widget, GdkDragContext *context,
	                             gint x, gint y, GtkSelectionData *selection_data, guint info, guint time);
	static void DragDataGet(GtkWidget *widget, GdkDragContext *context,
	                        GtkSelectionData *selection_data, guint info, guint time);
	static gboolean TimeOut(gpointer ptt);
	static gboolean IdleCallback(gpointer pSci);
	static gboolean StyleIdle(gpointer pSci);
	virtual void IdleWork();
	virtual void QueueIdleWork(WorkNeeded::workItems items, int upTo);
	virtual void SetDocPointer(Document *document);
	static void PopUpCB(GtkMenuItem *menuItem, ScintillaGTK *sciThis);

#if GTK_CHECK_VERSION(3,0,0)
	static gboolean DrawCT(GtkWidget *widget, cairo_t *cr, CallTip *ctip);
#else
	static gboolean ExposeCT(GtkWidget *widget, GdkEventExpose *ose, CallTip *ct);
#endif
	static gboolean PressCT(GtkWidget *widget, GdkEventButton *event, ScintillaGTK *sciThis);

	static sptr_t DirectFunction(sptr_t ptr,
	                             unsigned int iMessage, uptr_t wParam, sptr_t lParam);
};

// helper class to watch a GObject lifetime and get notified when it dies
class GObjectWatcher {
	GObject *weakRef;

	void WeakNotifyThis(GObject *obj G_GNUC_UNUSED) {
		PLATFORM_ASSERT(obj == weakRef);

		Destroyed();
		weakRef = 0;
	}

	static void WeakNotify(gpointer data, GObject *obj) {
		static_cast<GObjectWatcher*>(data)->WeakNotifyThis(obj);
	}

public:
	GObjectWatcher(GObject *obj) :
			weakRef(obj) {
		g_object_weak_ref(weakRef, WeakNotify, this);
	}

	virtual ~GObjectWatcher() {
		if (weakRef) {
			g_object_weak_unref(weakRef, WeakNotify, this);
		}
	}

	virtual void Destroyed() {}

	bool IsDestroyed() const {
		return weakRef != 0;
	}
};

std::string ConvertText(const char *s, size_t len, const char *charSetDest,
                        const char *charSetSource, bool transliterations, bool silent=false);

#ifdef SCI_NAMESPACE
}
#endif

#endif
