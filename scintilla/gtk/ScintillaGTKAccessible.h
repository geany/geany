/* Scintilla source code edit control */
/* ScintillaGTKAccessible.h - GTK+ accessibility for ScintillaGTK */
/* Copyright 2016 by Colomban Wendling <colomban@geany.org>
 * The License.txt file describes the conditions under which this software may be distributed. */

#ifndef SCINTILLAGTKACCESSIBLE_H
#define SCINTILLAGTKACCESSIBLE_H

#ifdef SCI_NAMESPACE
namespace Scintilla {
#endif

#ifndef ATK_CHECK_VERSION
# define ATK_CHECK_VERSION(x, y, z) 0
#endif

class ScintillaGTKAccessible {
private:
	// weak references to related objects
	GtkAccessible *accessible;
	ScintillaGTK *sci;

	// cache holding character offset for each line start, see CharacterOffsetFromByteOffset()
	std::vector<Position> character_offsets;

	// cached length of the deletion, in characters (see Notify())
	int deletionLengthChar;
	// local state for comparing
	Position old_pos;
	std::vector<SelectionRange> old_sels;

	void UpdateCursor();
	void Notify(GtkWidget *widget, gint code, SCNotification *nt);
	static void SciNotify(GtkWidget *widget, gint code, SCNotification *nt, gpointer data) {
		try {
			reinterpret_cast<ScintillaGTKAccessible*>(data)->Notify(widget, code, nt);
		} catch (...) {}
	}

	Position ByteOffsetFromCharacterOffset(Position startByte, int characterOffset) {
		Position pos = sci->pdoc->GetRelativePosition(startByte, characterOffset);
		if (pos == INVALID_POSITION) {
			// clamp invalid positions inside the document
			if (characterOffset > 0) {
				return sci->pdoc->Length();
			} else {
				return 0;
			}
		}
		return pos;
	}

	Position ByteOffsetFromCharacterOffset(int characterOffset) {
		return ByteOffsetFromCharacterOffset(0, characterOffset);
	}

	int CharacterOffsetFromByteOffset(Position byteOffset) {
		const Position line = sci->pdoc->LineFromPosition(byteOffset);
		if (character_offsets.size() <= static_cast<size_t>(line)) {
			if (character_offsets.empty())
				character_offsets.push_back(0);
			for (Position i = character_offsets.size(); i <= line; i++) {
				const Position start = sci->pdoc->LineStart(i - 1);
				const Position end = sci->pdoc->LineStart(i);
				character_offsets.push_back(character_offsets[i - 1] + sci->pdoc->CountCharacters(start, end));
			}
		}
		const Position lineStart = sci->pdoc->LineStart(line);
		return character_offsets[line] + sci->pdoc->CountCharacters(lineStart, byteOffset);
	}

	void CharacterRangeFromByteRange(Position startByte, Position endByte, int *startChar, int *endChar) {
		*startChar = CharacterOffsetFromByteOffset(startByte);
		*endChar = *startChar + sci->pdoc->CountCharacters(startByte, endByte);
	}

	void ByteRangeFromCharacterRange(int startChar, int endChar, Position& startByte, Position& endByte) {
		startByte = ByteOffsetFromCharacterOffset(startChar);
		endByte = ByteOffsetFromCharacterOffset(startByte, endChar - startChar);
	}

	Position PositionBefore(Position pos) {
		return sci->pdoc->MovePositionOutsideChar(pos - 1, -1, true);
	}

	Position PositionAfter(Position pos) {
		return sci->pdoc->MovePositionOutsideChar(pos + 1, 1, true);
	}

	int StyleAt(Position position, bool ensureStyle = false) {
		if (ensureStyle)
			sci->pdoc->EnsureStyledTo(position);
		return sci->pdoc->StyleAt(position);
	}

	// For AtkText
	gchar *GetTextRangeUTF8(Position startByte, Position endByte);
	gchar *GetText(int startChar, int endChar);
	gchar *GetTextAfterOffset(int charOffset, AtkTextBoundary boundaryType, int *startChar, int *endChar);
	gchar *GetTextBeforeOffset(int charOffset, AtkTextBoundary boundaryType, int *startChar, int *endChar);
	gchar *GetTextAtOffset(int charOffset, AtkTextBoundary boundaryType, int *startChar, int *endChar);
#if ATK_CHECK_VERSION(2, 10, 0)
	gchar *GetStringAtOffset(int charOffset, AtkTextGranularity granularity, int *startChar, int *endChar);
#endif
	gunichar GetCharacterAtOffset(int charOffset);
	gint GetCharacterCount();
	gint GetCaretOffset();
	gboolean SetCaretOffset(int charOffset);
	gint GetOffsetAtPoint(gint x, gint y, AtkCoordType coords);
	void GetCharacterExtents(int charOffset, gint *x, gint *y, gint *width, gint *height, AtkCoordType coords);
	AtkAttributeSet *GetAttributesForStyle(unsigned int style);
	AtkAttributeSet *GetRunAttributes(int charOffset, int *startChar, int *endChar);
	AtkAttributeSet *GetDefaultAttributes();
	gint GetNSelections();
	gchar *GetSelection(gint selection_num, int *startChar, int *endChar);
	gboolean AddSelection(int startChar, int endChar);
	gboolean RemoveSelection(int selection_num);
	gboolean SetSelection(gint selection_num, int startChar, int endChar);
	// for AtkEditableText
	bool InsertStringUTF8(Position bytePos, const gchar *utf8, int lengthBytes);
	void SetTextContents(const gchar *contents);
	void InsertText(const gchar *contents, int lengthBytes, int *charPosition);
	void CopyText(int startChar, int endChar);
	void CutText(int startChar, int endChar);
	void DeleteText(int startChar, int endChar);
	void PasteText(int charPosition);

public:
	ScintillaGTKAccessible(GtkAccessible *accessible, GtkWidget *widget);
	~ScintillaGTKAccessible();

	static ScintillaGTKAccessible *FromAccessible(GtkAccessible *accessible);
	static ScintillaGTKAccessible *FromAccessible(AtkObject *accessible) {
		return FromAccessible(GTK_ACCESSIBLE(accessible));
	}
	// So ScintillaGTK can notify us
	void ChangeDocument(Document *oldDoc, Document *newDoc);
	void NotifyReadOnly();

	// Helper GtkWidget methods
	static AtkObject *WidgetGetAccessibleImpl(GtkWidget *widget, AtkObject **cache, gpointer widget_parent_class);

	// ATK methods

	class AtkTextIface {
	public:
		static void init(::AtkTextIface *iface);

	private:
		AtkTextIface();

		static gchar *GetText(AtkText *text, int start_offset, int end_offset);
		static gchar *GetTextAfterOffset(AtkText *text, int offset, AtkTextBoundary boundary_type, int *start_offset, int *end_offset);
		static gchar *GetTextBeforeOffset(AtkText *text, int offset, AtkTextBoundary boundary_type, int *start_offset, int *end_offset);
		static gchar *GetTextAtOffset(AtkText *text, gint offset, AtkTextBoundary boundary_type, gint *start_offset, gint *end_offset);
#if ATK_CHECK_VERSION(2, 10, 0)
		static gchar *GetStringAtOffset(AtkText *text, gint offset, AtkTextGranularity granularity, gint *start_offset, gint *end_offset);
#endif
		static gunichar GetCharacterAtOffset(AtkText *text, gint offset);
		static gint GetCharacterCount(AtkText *text);
		static gint GetCaretOffset(AtkText *text);
		static gboolean SetCaretOffset(AtkText *text, gint offset);
		static gint GetOffsetAtPoint(AtkText *text, gint x, gint y, AtkCoordType coords);
		static void GetCharacterExtents(AtkText *text, gint offset, gint *x, gint *y, gint *width, gint *height, AtkCoordType coords);
		static AtkAttributeSet *GetRunAttributes(AtkText *text, gint offset, gint *start_offset, gint *end_offset);
		static AtkAttributeSet *GetDefaultAttributes(AtkText *text);
		static gint GetNSelections(AtkText *text);
		static gchar *GetSelection(AtkText *text, gint selection_num, gint *start_pos, gint *end_pos);
		static gboolean AddSelection(AtkText *text, gint start, gint end);
		static gboolean RemoveSelection(AtkText *text, gint selection_num);
		static gboolean SetSelection(AtkText *text, gint selection_num, gint start, gint end);
	};
	class AtkEditableTextIface {
	public:
		static void init(::AtkEditableTextIface *iface);

	private:
		AtkEditableTextIface();

		static void SetTextContents(AtkEditableText *text, const gchar *contents);
		static void InsertText(AtkEditableText *text, const gchar *contents, gint length, gint *position);
		static void CopyText(AtkEditableText *text, gint start, gint end);
		static void CutText(AtkEditableText *text, gint start, gint end);
		static void DeleteText(AtkEditableText *text, gint start, gint end);
		static void PasteText(AtkEditableText *text, gint position);
	};
};

#ifdef SCI_NAMESPACE
}
#endif


#endif /* SCINTILLAGTKACCESSIBLE_H */
