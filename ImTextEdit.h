#pragma once

#include "SPIRVParser.h"

#include "imgui.h"

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <thread>
#include <map>
#include <regex>

class ImTextEdit
{
public:
	enum class PaletteIndex
	{
		Default,
		Keyword,
		Number,
		String,
		CharLiteral,
		Punctuation,
		Preprocessor,
		Identifier,
		KnownIdentifier,
		PreprocIdentifier,
		Comment,
		MultiLineComment,
		Background,
		Cursor,
		Selection,
		ErrorMarker,
		Breakpoint,
		BreakpointOutline,
		CurrentLineIndicator,
		CurrentLineIndicatorOutline,
		LineNumber,
		CurrentLineFill,
		CurrentLineFillInactive,
		CurrentLineEdge,
		ErrorMessage,
		BreakpointDisabled,
		UserFunction,
		UserType,
		UniformVariable,
		GlobalVariable,
		LocalVariable,
		FunctionArgument,
		Max
	};

	enum class ShortcutID
	{
		Undo,
		Redo,
		MoveUp,
		SelectUp,
		MoveDown,
		SelectDown,
		MoveLeft,
		SelectLeft,
		MoveWordLeft,
		SelectWordLeft,
		MoveRight,
		SelectRight,
		MoveWordRight,
		SelectWordRight,
		MoveUpBlock,
		SelectUpBlock,
		MoveDownBlock,
		SelectDownBlock,
		MoveTop,
		SelectTop,
		MoveBottom,
		SelectBottom,
		MoveStartLine,
		SelectStartLine,
		MoveEndLine,
		SelectEndLine,
		ForwardDelete,
		ForwardDeleteWord,
		DeleteRight,
		BackwardDelete,
		BackwardDeleteWord,
		DeleteLeft,
		OverwriteCursor,
		Copy,
		Paste,
		Cut,
		SelectAll,
		AutocompleteOpen,
		AutocompleteSelect,
		AutocompleteSelectActive,
		AutocompleteUp,
		AutocompleteDown,
		NewLine,
		Indent,
		Unindent,
		Find,
		Replace,
		FindNext,
		DebugStep,
		DebugStepInto,
		DebugStepOut,
		DebugContinue,
		DebugJumpHere,
		DebugBreakpoint,
		DebugStop,
		DuplicateLine,
		CommentLines,
		UncommentLines,
		Count
	};

	struct Shortcut
	{
		// 0 - not used, 1 - used
		bool Alt;
		bool Ctrl;
		bool Shift;

		// -1 - not used, everything else: Win32 VK_ code
		int Key1;
		int Key2;

		Shortcut(int vk1 = -1, int vk2 = -2, bool alt = 0, bool ctrl = 0, bool shift = 0)
			: Key1(vk1), Key2(vk2), Alt(alt), Ctrl(ctrl), Shift(shift) {}
	};

	enum class SelectionMode
	{
		Normal,
		Word,
		Line
	};

	struct Breakpoint
	{
		int Line;
		bool Enabled;
		bool UseCondition;
		std::string Condition;

		Breakpoint()
			: Line(-1), Enabled(false) {}
	};

	// Represents a character coordinate from the user's point of view,
	// i. e. consider an uniform grid (assuming fixed-width font) on the
	// screen as it is rendered, and each cell has its own coordinate, starting from 0.
	// Tabs are counted as [1..mTabSize] count empty spaces, depending on
	// how many space is necessary to reach the next tab stop.
	// For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when mTabSize = 4,
	// because it is rendered as "    ABC" on the screen.
	struct Coordinates
	{
		int Line, Column;
		Coordinates()
			: Line(0), Column(0) {}

		Coordinates(int aLine, int aColumn)
			: Line(aLine), Column(aColumn)
		{
			assert(aLine >= 0);
			assert(aColumn >= 0);
		}

		static Coordinates Invalid()
		{
			static Coordinates invalid(-1, -1);
			return invalid;
		}

		bool operator==(const Coordinates& o) const
		{
			return Line == o.Line && Column == o.Column;
		}

		bool operator!=(const Coordinates& o) const
		{
			return Line != o.Line || Column != o.Column;
		}

		bool operator<(const Coordinates& o) const
		{
			if (Line != o.Line)
				return Line < o.Line;
			return Column < o.Column;
		}

		bool operator>(const Coordinates& o) const
		{
			if (Line != o.Line)
				return Line > o.Line;
			return Column > o.Column;
		}

		bool operator<=(const Coordinates& o) const
		{
			if (Line != o.Line)
				return Line < o.Line;
			return Column <= o.Column;
		}

		bool operator>=(const Coordinates& o) const
		{
			if (Line != o.Line)
				return Line > o.Line;
			return Column >= o.Column;
		}
	};

	struct Identifier
	{
		Identifier() {}
		Identifier(const std::string& declr)
			: Declaration(declr) {}

		Coordinates Location;
		std::string Declaration;
	};

	typedef std::unordered_map<std::string, Identifier> td_Identifiers;
	typedef std::unordered_set<std::string> td_Keywords;
	typedef std::map<int, std::string> td_ErrorMarkers;
	typedef std::array<ImU32, (unsigned)PaletteIndex::Max> td_Palette;
	typedef uint8_t td_Char;

	struct Glyph
	{
		td_Char Character;
		PaletteIndex ColorIndex = PaletteIndex::Default;
		bool Comment : 1;
		bool MultiLineComment : 1;
		bool Preprocessor : 1;

		Glyph(td_Char aChar, PaletteIndex aColorIndex)
			: Character(aChar), ColorIndex(aColorIndex), Comment(false), MultiLineComment(false), Preprocessor(false) {}
	};

	struct LanguageDefinition
	{
		typedef std::pair<std::string, PaletteIndex> TokenRegexString;
		typedef std::vector<TokenRegexString> td_TokenRegexStrings;
		typedef bool (*TokenizeCallback)(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, PaletteIndex& paletteIndex);

		std::string Name;
		td_Keywords Keywords;
		td_Identifiers Identifiers;
		td_Identifiers PreprocIdentifiers;
		std::string CommentStart, CommentEnd, SingleLineComment;
		char PreprocChar;
		bool AutoIndentation;

		TokenizeCallback Tokenize;

		td_TokenRegexStrings TokenRegexStrings;

		bool CaseSensitive;

		LanguageDefinition()
			: PreprocChar('#'), AutoIndentation(true), Tokenize(nullptr), CaseSensitive(true) {}

		static const LanguageDefinition& CPlusPlus();
		static const LanguageDefinition& HLSL();
		static const LanguageDefinition& GLSL();
		static const LanguageDefinition& SPIRV();
		static const LanguageDefinition& C();
		static const LanguageDefinition& SQL();
		static const LanguageDefinition& AngelScript();
		static const LanguageDefinition& Lua();

	private:
		static void HLSLDocumentation(td_Identifiers& idents);
		static void GLSLDocumentation(td_Identifiers& idents);
	};

	typedef std::vector<Glyph> Line;
	typedef std::vector<Line> Lines;

public:
	ImTextEdit();
	~ImTextEdit();

	void SetLanguageDefinition(const LanguageDefinition& aLanguageDef);
	const LanguageDefinition& GetLanguageDefinition() const { return m_LanguageDefinition; }

	const td_Palette& GetPalette() const { return m_PaletteBase; }
	void SetPalette(const td_Palette& aValue);

	void SetErrorMarkers(const td_ErrorMarkers& aMarkers) { m_ErrorMarkers = aMarkers; }

	bool HasBreakpoint(int line);
	void AddBreakpoint(int line, bool useCondition = false, std::string condition = "", bool enabled = true);
	void RemoveBreakpoint(int line);
	void SetBreakpointEnabled(int line, bool enable);
	Breakpoint& GetBreakpoint(int line);
	inline const std::vector<Breakpoint>& GetBreakpoints() { return m_Breakpoints; }
	void SetCurrentLineIndicator(int line, bool displayBar = true);
	inline int GetCurrentLineIndicator() { return m_DebugCurrentLine; }

	inline bool IsDebugging() { return m_DebugCurrentLine > 0; }

	void Render(const char* aTitle, const ImVec2& aSize = ImVec2(), bool aBorder = false);
	void SetText(const std::string& aText);
	std::string GetText() const;

	void SetTextLines(const std::vector<std::string>& aLines);
	void GetTextLines(std::vector<std::string>& out) const;

	std::string GetSelectedText() const;
	std::string GetCurrentLineText() const;

	int GetTotalLines() const { return (int)m_Lines.size(); }
	bool IsOverwrite() const { return m_Overwrite; }

	bool IsFocused() const { return m_Focused; }
	void SetReadOnly(bool aValue);
	bool IsReadOnly() { return m_ReadOnly || IsDebugging(); }
	bool IsTextChanged() const { return m_TextChanged; }
	bool IsCursorPositionChanged() const { return m_CursorPositionChanged; }
	inline void ResetTextChanged()
	{
		m_TextChanged = false;
		m_ChangedLines.clear();
	}

	bool IsColorizerEnabled() const { return m_ColorizerEnabled; }
	void SetColorizerEnable(bool aValue);

	Coordinates GetCorrectCursorPosition(); // The GetCursorPosition() returns the cursor pos where \t == 4 spaces
	Coordinates GetCursorPosition() const { return GetActualCursorCoordinates(); }
	void SetCursorPosition(const Coordinates& aPosition);

	inline void SetHandleMouseInputs(bool aValue) { m_HandleMouseInputs = aValue; }
	inline bool IsHandleMouseInputsEnabled() const { return m_HandleKeyboardInputs; }

	inline void SetHandleKeyboardInputs(bool aValue) { m_HandleKeyboardInputs = aValue; }
	inline bool IsHandleKeyboardInputsEnabled() const { return m_HandleKeyboardInputs; }

	inline void SetImGuiChildIgnored(bool aValue) { m_IgnoreImGuiChild = aValue; }
	inline bool IsImGuiChildIgnored() const { return m_IgnoreImGuiChild; }

	inline void SetShowWhitespaces(bool aValue) { m_ShowWhitespaces = aValue; }
	inline bool IsShowingWhitespaces() const { return m_ShowWhitespaces; }

	void AppendText(const std::string& aValue, bool indent = false);
	void AppendText(const char* aValue, bool indent = false);

	void MoveUp(int aAmount = 1, bool aSelect = false);
	void MoveDown(int aAmount = 1, bool aSelect = false);
	void MoveLeft(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveRight(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveTop(bool aSelect = false);
	void MoveBottom(bool aSelect = false);
	void MoveHome(bool aSelect = false);
	void MoveEnd(bool aSelect = false);

	void SetSelectionStart(const Coordinates& aPosition);
	void SetSelectionEnd(const Coordinates& aPosition);
	void SetSelection(const Coordinates& aStart, const Coordinates& aEnd, SelectionMode aMode = SelectionMode::Normal);
	void SelectWordUnderCursor();
	void SelectAll();
	bool HasSelection() const;

	void Copy();
	void Cut();
	void Paste();
	void Delete();
	void DuplicateLine();

	bool CanUndo();
	bool CanRedo();
	void Undo(int aSteps = 1);
	void Redo(int aSteps = 1);
	void ResetUndos();

	std::vector<std::string> GetRelevantExpressions(int line);

	inline void SetHighlightedLines(const std::vector<int>& lines) { m_HighlightedLines = lines; }
	inline void ClearHighlightedLines() { m_HighlightedLines.clear(); }

	inline void SetTabSize(int s) { m_TabSize = std::max<int>(0, std::min<int>(32, s)); }
	inline int GetTabSize() { return m_TabSize; }

	inline void SetInsertSpaces(bool s) { m_InsertSpaces = s; }
	inline int GetInsertSpaces() { return m_InsertSpaces; }

	inline void SetSmartIndent(bool s) { m_SmartIndent = s; }
	inline void SetAutoIndentOnPaste(bool s) { m_AutoindentOnPaste = s; }
	inline void SetHighlightLine(bool s) { m_HighlightLine = s; }
	inline void SetCompleteBraces(bool s) { m_CompleteBraces = s; }
	inline void SetHorizontalScroll(bool s) { m_HorizontalScroll = s; }
	inline void SetSmartPredictions(bool s) { m_Autocomplete = s; }
	inline void SetFunctionDeclarationTooltip(bool s) { m_FunctionDeclarationTooltipEnabled = s; }
	inline void SetFunctionTooltips(bool s) { m_FuncTooltips = s; }
	inline void SetActiveAutocomplete(bool cac) { m_ActiveAutocomplete = cac; }
	inline void SetScrollbarMarkers(bool markers) { m_ScrollbarMarkers = markers; }
	inline void SetSidebarVisible(bool s) { m_Sidebar = s; }
	inline void SetSearchEnabled(bool s) { m_HasSearch = s; }
	inline void SetHiglightBrackets(bool s) { m_HighlightBrackets = s; }
	inline void SetFoldEnabled(bool s) { m_FoldEnabled = s; }

	inline void SetUIScale(float scale) { m_UIScale = scale; }
	inline void SetUIFontSize(float size) { m_UIFontSize = size; }
	inline void SetEditorFontSize(float size) { m_EditorFontSize = size; }

	void SetShortcut(ImTextEdit::ShortcutID id, Shortcut s);

	inline void SetShowLineNumbers(bool s)
	{
		m_ShowLineNumbers = s;
		m_TextStart = (s ? 20 : 6);
		m_LeftMargin = (s ? (s_DebugDataSpace + s_LineNumberSpace) : (s_DebugDataSpace - s_LineNumberSpace));
	}

	inline int GetTextStart() const { return m_ShowLineNumbers ? 7 : 3; }

	void Colorize(int aFromLine = 0, int aCount = -1);
	void ColorizeRange(int aFromLine = 0, int aToLine = 0);
	void ColorizeInternal();
	
	inline void ClearAutocompleteData()
	{
		m_ACFunctions.clear();
		m_ACUserTypes.clear();
		m_ACUniforms.clear();
		m_ACGlobals.clear();
	}

	inline void ClearAutocompleteEntries()
	{
		m_ACEntries.clear();
		m_ACEntrySearch.clear();
	}

	inline const std::unordered_map<std::string, ed::SPIRVParser::Function>& GetAutocompleteFunctions() { return m_ACFunctions; }
	inline const std::unordered_map<std::string, std::vector<ed::SPIRVParser::Variable>>& GetAutocompleteUserTypes() { return m_ACUserTypes; }
	inline const std::vector<ed::SPIRVParser::Variable>& GetAutocompleteUniforms() { return m_ACUniforms; }
	inline const std::vector<ed::SPIRVParser::Variable>& GetAutocompleteGlobals() { return m_ACGlobals; }
	
	inline void SetAutocompleteFunctions(const std::unordered_map<std::string, ed::SPIRVParser::Function>& funcs)
	{
		m_ACFunctions = funcs;
	}
	
	inline void SetAutocompleteUserTypes(const std::unordered_map<std::string, std::vector<ed::SPIRVParser::Variable>>& utypes)
	{
		m_ACUserTypes = utypes;
	}
	
	inline void SetAutocompleteUniforms(const std::vector<ed::SPIRVParser::Variable>& unis)
	{
		m_ACUniforms = unis;
	}
	
	inline void SetAutocompleteGlobals(const std::vector<ed::SPIRVParser::Variable>& globs)
	{
		m_ACGlobals = globs;
	}
	
	inline void AddAutocompleteEntry(const std::string& search, const std::string& display, const std::string& value)
	{
		m_ACEntrySearch.push_back(search);
		m_ACEntries.push_back(std::make_pair(display, value));
	}

	static const std::vector<Shortcut> GetDefaultShortcuts();
	static const td_Palette& GetDarkPalette();
	static const td_Palette& GetLightPalette();
	static const td_Palette& GetRetroBluePalette();

	void Backspace();

	enum class DebugAction
	{
		Step,
		StepInto,
		StepOut,
		Continue,
		Stop
	};

	std::function<void(ImTextEdit*, int)> OnDebuggerJump;
	std::function<void(ImTextEdit*, DebugAction)> OnDebuggerAction;
	std::function<void(ImTextEdit*, const std::string&)> OnIdentifierHover;
	std::function<bool(ImTextEdit*, const std::string&)> HasIdentifierHover;
	std::function<void(ImTextEdit*, const std::string&)> OnExpressionHover;
	std::function<bool(ImTextEdit*, const std::string&)> HasExpressionHover;
	std::function<void(ImTextEdit*, int)> OnBreakpointRemove;
	std::function<void(ImTextEdit*, int, bool, const std::string&, bool)> OnBreakpointUpdate;

	std::function<void(ImTextEdit*, const std::string&, ImTextEdit::Coordinates coords)> OnCtrlAltClick;
	std::function<void(ImTextEdit*, const std::string&, const std::string&)> RequestOpen;
	std::function<void(ImTextEdit*)> OnContentUpdate;

	inline void SetPath(const std::string& path) { m_Path = path; }
	inline const std::string& GetPath() { return m_Path; }

public:
	static const int s_LineNumberSpace = 20;
	static const int s_DebugDataSpace = 10;

private:
	std::string m_Path;

	typedef std::vector<std::pair<std::regex, PaletteIndex>> td_RegexList;
	
	struct EditorState
	{
		Coordinates SelectionStart;
		Coordinates SelectionEnd;
		Coordinates CursorPosition;
	};

	class UndoRecord
	{
	public:
		UndoRecord() {}
		~UndoRecord() {}

		UndoRecord(const std::string& aAdded, const ImTextEdit::Coordinates aAddedStart, const ImTextEdit::Coordinates aAddedEnd, const std::string& aRemoved,
			const ImTextEdit::Coordinates aRemovedStart, const ImTextEdit::Coordinates aRemovedEnd, ImTextEdit::EditorState& aBefore, ImTextEdit::EditorState& aAfter);

		void Undo(ImTextEdit* aEditor);
		void Redo(ImTextEdit* aEditor);

		std::string Added;
		Coordinates AddedStart;
		Coordinates AddedEnd;

		std::string Removed;
		Coordinates RemovedStart;
		Coordinates RemovedEnd;

		EditorState Before;
		EditorState After;
	};

	typedef std::vector<UndoRecord> td_UndoBuffer;

	float TextDistanceToLineStart(const Coordinates& aFrom) const;
	void EnsureCursorVisible();
	int GetPageSize() const;
	std::string GetText(const Coordinates& aStart, const Coordinates& aEnd) const;
	Coordinates GetActualCursorCoordinates() const;
	Coordinates SanitizeCoordinates(const Coordinates& aValue) const;
	void Advance(Coordinates& aCoordinates) const;
	void DeleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	int InsertTextAt(Coordinates& aWhere, const char* aValue, bool indent = false);
	void AddUndo(UndoRecord& aValue);
	Coordinates ScreenPosToCoordinates(const ImVec2& aPosition) const;
	Coordinates MousePosToCoordinates(const ImVec2& aPosition) const;
	ImVec2 CoordinatesToScreenPos(const ImTextEdit::Coordinates& aPosition) const;
	Coordinates FindWordStart(const Coordinates& aFrom) const;
	Coordinates FindWordEnd(const Coordinates& aFrom) const;
	Coordinates FindNextWord(const Coordinates& aFrom) const;
	int GetCharacterIndex(const Coordinates& aCoordinates) const;
	int GetCharacterColumn(int aLine, int aIndex) const;
	int GetLineCharacterCount(int aLine) const;
	int GetLineMaxColumn(int aLine) const;
	bool IsOnWordBoundary(const Coordinates& aAt) const;
	void RemoveLine(int aStart, int aEnd);
	void RemoveLine(int aIndex);
	Line& InsertLine(int aIndex, int column);
	void EnterCharacter(ImWchar aChar, bool aShift);
	void DeleteSelection();
	std::string GetWordUnderCursor() const;
	std::string GetWordAt(const Coordinates& aCoords) const;
	ImU32 GetGlyphColor(const Glyph& aGlyph) const;

	Coordinates FindFirst(const std::string& what, const Coordinates& fromWhere);

	void HandleKeyboardInputs();
	void HandleMouseInputs();
	void RenderInternal(const char* aTitle);

	bool m_FuncTooltips;

	float m_UIScale, m_UIFontSize, m_EditorFontSize;
	
	inline float UICalculateSize(float h)
	{
		return h * (m_UIScale + m_UIFontSize / 18.0f - 1.0f);
	}

	inline float EditorCalculateSize(float h)
	{
		return h * (m_UIScale + m_EditorFontSize / 18.0f - 1.0f);
	}

	bool m_FunctionDeclarationTooltipEnabled;
	ImTextEdit::Coordinates m_FunctionDeclarationCoord;
	bool m_FunctionDeclarationTooltip;
	std::string m_FunctionDeclaration;

	void OpenFunctionDeclarationTooltip(const std::string& obj, ImTextEdit::Coordinates coord);

	std::string BuildFunctionDef(const std::string& func, const std::string& lang);
	std::string BuildVariableType(const ed::SPIRVParser::Variable& var, const std::string& lang);

	void RemoveFolds(const Coordinates& start, const Coordinates& end);
	void RemoveFolds(std::vector<Coordinates>& folds, const Coordinates& start, const Coordinates& end);

	std::string AutcompleteParse(const std::string& str, const Coordinates& start);
	void AutocompleteSelect();

	void BuildMemberSuggestions(bool* keepACOpened = nullptr);
	void BuildSuggestions(bool* keepACOpened = nullptr);

	float m_LineSpacing;
	Lines m_Lines;
	EditorState m_State;
	td_UndoBuffer m_UndoBuffer;
	int m_UndoIndex;
	int m_ReplaceIndex;

	bool m_Sidebar;
	bool m_HasSearch;

	char m_FindWord[256];
	bool m_FindOpened;
	bool m_FindJustOpened;
	bool m_FindNext;
	bool m_FindFocused, m_ReplaceFocused;
	bool m_ReplaceOpened;
	char m_ReplaceWord[256];

	bool m_FoldEnabled;
	std::vector<Coordinates> m_FoldBegin, m_FoldEnd;
	std::vector<int> m_FoldConnection;
	std::vector<bool> m_Fold;
	bool m_FoldSorted;
	
	uint64_t m_FoldLastIteration;

	float m_LastScroll;

	std::vector<std::string> m_ACEntrySearch;
	std::vector<std::pair<std::string, std::string>> m_ACEntries;

	bool m_IsSnippet;
	std::vector<Coordinates> m_SnippetTagStart, m_SnippetTagEnd;
	std::vector<int> m_SnippetTagID;
	std::vector<bool> m_SnippetTagHighlight;
	int m_SnippetTagSelected, m_SnippetTagLength, m_SnippetTagPreviousLength;

	bool m_RequestAutocomplete, m_ReadyForAutocomplete;

	bool m_ActiveAutocomplete;
	bool m_Autocomplete;
	std::unordered_map<std::string, ed::SPIRVParser::Function> m_ACFunctions;
	std::unordered_map<std::string, std::vector<ed::SPIRVParser::Variable>> m_ACUserTypes;
	std::vector<ed::SPIRVParser::Variable> m_ACUniforms;
	std::vector<ed::SPIRVParser::Variable> m_ACGlobals;
	std::string m_ACWord;
	std::vector<std::pair<std::string, std::string>> m_ACSuggestions;
	int m_ACIndex;
	bool m_ACOpened;
	bool m_ACSwitched;		// if == true then allow selection with enter
	std::string m_ACObject;	// if mACObject is not empty, it means user typed '.' -> suggest struct members and methods for mACObject
	Coordinates m_ACPosition;

	std::vector<Shortcut> m_Shortcuts;

	bool m_ScrollbarMarkers;
	std::vector<int> m_ChangedLines;

	std::vector<int> m_HighlightedLines;

	bool m_HorizontalScroll;
	bool m_CompleteBraces;
	bool m_ShowLineNumbers;
	bool m_HighlightLine;
	bool m_HighlightBrackets;
	bool m_InsertSpaces;
	bool m_SmartIndent;
	bool m_Focused;
	int m_TabSize;
	bool m_Overwrite;
	bool m_ReadOnly;
	bool m_WithinRender;
	bool m_ScrollToCursor;
	bool m_ScrollToTop;
	bool m_TextChanged;
	bool m_ColorizerEnabled;
	float m_TextStart;                   // position (in pixels) where a code line starts relative to the left of the ImTextEdit.
	int  m_LeftMargin;
	bool m_CursorPositionChanged;
	int m_ColorRangeMin, m_ColorRangeMax;
	SelectionMode m_SelectionMode;
	bool m_HandleKeyboardInputs;
	bool m_HandleMouseInputs;
	bool m_IgnoreImGuiChild;
	bool m_ShowWhitespaces;
	bool m_AutoindentOnPaste;

	td_Palette m_PaletteBase;
	td_Palette m_Palette;
	LanguageDefinition m_LanguageDefinition;
	td_RegexList m_RegexList;

	float m_DebugBarWidth, m_DebugBarHeight;

	bool m_DebugBar;
	bool m_DebugCurrentLineUpdated;
	int m_DebugCurrentLine;
	ImVec2 m_UICursorPos, m_FindOrigin;
	float m_WindowWidth;
	std::vector<Breakpoint> m_Breakpoints;
	ImVec2 m_RightClickPos;

	int m_PopupCondition_Line;
	bool m_PopupCondition_Use;
	char m_PopupCondition_Condition[512];

	bool m_CheckComments;
	td_ErrorMarkers m_ErrorMarkers;
	ImVec2 m_CharAdvance;
	Coordinates m_InteractiveStart, m_InteractiveEnd;
	std::string m_LineBuffer;
	uint64_t m_StartTime;

	Coordinates m_LastHoverPosition;
	std::chrono::steady_clock::time_point m_LastHoverTime;

	float m_LastClick;
};
