#include "nzpch.hpp"

#include <algorithm>
#include <functional>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>
#include <stack>

#include "ImTextEdit.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool Equals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p)
{
	for (; first1 != last1 && first2 != last2; ++first1, ++first2) 
	{
		if (!p(*first1, *first2))
			return false;
	}

	return first1 == last1 && first2 == last2;
}

int IsBracket(char ch)
{
	if (ch == '(' || ch == '[' || ch == '{')
		return 1;
	if (ch == ')' || ch == ']' || ch == '}')
		return 2;
	return 0;
}

bool IsClosingBracket(char open, char actual)
{
	return (open == '{' && actual == '}') || (open == '[' && actual == ']') || (open == '(' && actual == ')');
}

bool IsOpeningBracket(char close, char actual)
{
	return (close == '}' && actual == '{') || (close == ']' && actual == '[') || (close == ')' && actual == '(');
}

ImTextEdit::ImTextEdit()
	: m_LineSpacing(1.0f), m_UndoIndex(0), m_InsertSpaces(false), m_TabSize(4), m_HighlightBrackets(false), m_Autocomplete(true), m_ACOpened(false), m_HighlightLine(true), m_HorizontalScroll(true), m_CompleteBraces(true), m_ShowLineNumbers(true),
	  m_SmartIndent(true), m_Overwrite(false), m_ReadOnly(false), m_WithinRender(false), m_ScrollToCursor(false), m_ScrollToTop(false), m_TextChanged(false), m_ColorizerEnabled(true), m_TextStart(20.0f), m_LeftMargin(s_DebugDataSpace + s_LineNumberSpace),
	  m_CursorPositionChanged(false), m_ColorRangeMin(0), m_ColorRangeMax(0), m_SelectionMode(SelectionMode::Normal), m_CheckComments(true), m_LastClick(-1.0f), m_HandleKeyboardInputs(true), m_HandleMouseInputs(true),
	  m_IgnoreImGuiChild(false), m_ShowWhitespaces(false), m_DebugBar(false), m_DebugCurrentLineUpdated(false), m_DebugCurrentLine(-1), m_Path(""), OnContentUpdate(nullptr), m_FuncTooltips(true), m_UIScale(1.0f), m_UIFontSize(18.0f),
	  m_EditorFontSize(18.0f), m_ActiveAutocomplete(false), m_ReadyForAutocomplete(false), m_RequestAutocomplete(false), m_ScrollbarMarkers(false), m_AutoindentOnPaste(false), m_FunctionDeclarationTooltip(false), m_FunctionDeclarationTooltipEnabled(false),
	  m_IsSnippet(false), m_SnippetTagSelected(0), m_Sidebar(true), m_HasSearch(true), m_ReplaceIndex(0), m_FoldEnabled(true), m_FoldLastIteration(0), m_FoldSorted(false), m_LastScroll(0.0f),
	  m_StartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
{
	memset(m_FindWord, 0, 256 * sizeof(char));
	memset(m_ReplaceWord, 0, 256 * sizeof(char));
	m_FindOpened = false;
	m_ReplaceOpened = false;
	m_FindJustOpened = false;
	m_FindFocused = false;
	m_ReplaceFocused = false;

	OnDebuggerJump = nullptr;
	OnDebuggerAction = nullptr;
	OnBreakpointRemove = nullptr;
	OnBreakpointUpdate = nullptr;
	OnIdentifierHover = nullptr;
	HasIdentifierHover = nullptr;
	OnExpressionHover = nullptr;
	HasExpressionHover = nullptr;

	RequestOpen = nullptr;

	m_DebugBarWidth = 0.0f;
	m_DebugBarHeight = 0.0f;

	SetPalette(GetDarkPalette());
	SetLanguageDefinition(LanguageDefinition::HLSL());
	m_Lines.push_back(Line());

	m_Shortcuts = GetDefaultShortcuts();
}

const std::vector<ImTextEdit::Shortcut> ImTextEdit::GetDefaultShortcuts()
{
	std::vector<ImTextEdit::Shortcut> ret;
	ret.resize((int)ImTextEdit::ShortcutID::Count);

	ret[(int)ImTextEdit::ShortcutID::Undo] = ImTextEdit::Shortcut(0x5A, -1, 0, 1, 0);      // CTRL+Z
	ret[(int)ImTextEdit::ShortcutID::Redo] = ImTextEdit::Shortcut(0x59, -1, 0, 1, 0); // CTRL+Y
	ret[(int)ImTextEdit::ShortcutID::MoveUp] = ImTextEdit::Shortcut(0x26, -1, 0, 0, 0);   // UP ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectUp] = ImTextEdit::Shortcut(0x26, -1, 0, 0, 1);       // SHIFT + UP ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveDown] = ImTextEdit::Shortcut(0x28, -1, 0, 0, 0);     // DOWN ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectDown] = ImTextEdit::Shortcut(0x28, -1, 0, 0, 1);   // SHIFT + DOWN ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveLeft] = ImTextEdit::Shortcut(0x25, -1, 0, 0, 0);     // LEFT ARROW (+ SHIFT/CTRL)
	ret[(int)ImTextEdit::ShortcutID::SelectLeft] = ImTextEdit::Shortcut(0x25, -1, 0, 0, 1);   // SHIFT + LEFT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveWordLeft] = ImTextEdit::Shortcut(0x25, -1, 0, 1, 0); // CTRL + LEFT ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectWordLeft] = ImTextEdit::Shortcut(0x25, -1, 0, 1, 1); // CTRL + SHIFT + LEFT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveRight] = ImTextEdit::Shortcut(0x27, -1, 0, 0, 0);       // RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectRight] = ImTextEdit::Shortcut(0x27, -1, 0, 0, 1);   // SHIFT + RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveWordRight] = ImTextEdit::Shortcut(0x27, -1, 0, 1, 0);   // CTRL + RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectWordRight] = ImTextEdit::Shortcut(0x27, -1, 0, 1, 1);  // CTRL + SHIFT + RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveUpBlock] = ImTextEdit::Shortcut(0x21, -1, 0, 0, 0);     // PAGE UP
	ret[(int)ImTextEdit::ShortcutID::SelectUpBlock] = ImTextEdit::Shortcut(0x21, -1, 0, 0, 1);    // SHIFT + PAGE UP
	ret[(int)ImTextEdit::ShortcutID::MoveDownBlock] = ImTextEdit::Shortcut(0x22, -1, 0, 0, 0);    // PAGE DOWN
	ret[(int)ImTextEdit::ShortcutID::SelectDownBlock] = ImTextEdit::Shortcut(0x22, -1, 0, 0, 1);   // SHIFT + PAGE DOWN
	ret[(int)ImTextEdit::ShortcutID::MoveTop] = ImTextEdit::Shortcut(0x24, -1, 0, 1, 0);          // CTRL + HOME
	ret[(int)ImTextEdit::ShortcutID::SelectTop] = ImTextEdit::Shortcut(0x24, -1, 0, 1, 1);         // CTRL + SHIFT + HOME
	ret[(int)ImTextEdit::ShortcutID::MoveBottom] = ImTextEdit::Shortcut(0x23, -1, 0, 1, 0);        // CTRL + END
	ret[(int)ImTextEdit::ShortcutID::SelectBottom] = ImTextEdit::Shortcut(0x23, -1, 0, 1, 1);       // CTRL + SHIFT + END
	ret[(int)ImTextEdit::ShortcutID::MoveStartLine] = ImTextEdit::Shortcut(0x24, -1, 0, 0, 0);     // HOME
	ret[(int)ImTextEdit::ShortcutID::SelectStartLine] = ImTextEdit::Shortcut(0x24, -1, 0, 0, 1); // SHIFT + HOME
	ret[(int)ImTextEdit::ShortcutID::MoveEndLine] = ImTextEdit::Shortcut(0x23, -1, 0, 0, 0);        // END
	ret[(int)ImTextEdit::ShortcutID::SelectEndLine] = ImTextEdit::Shortcut(0x23, -1, 0, 0, 1);    // SHIFT + END
	ret[(int)ImTextEdit::ShortcutID::ForwardDelete] = ImTextEdit::Shortcut(0x2E, -1, 0, 0, 0);   // DELETE
	ret[(int)ImTextEdit::ShortcutID::ForwardDeleteWord] = ImTextEdit::Shortcut(0x2E, -1, 0, 1, 0);   // CTRL + DELETE
	ret[(int)ImTextEdit::ShortcutID::DeleteRight] = ImTextEdit::Shortcut(0x2E, -1, 0, 0, 1);             // SHIFT+BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::BackwardDelete] = ImTextEdit::Shortcut(0x08, -1, 0, 0, 0);        // BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::BackwardDeleteWord] = ImTextEdit::Shortcut(0x08, -1, 0, 1, 0);        // CTRL + BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::DeleteLeft] = ImTextEdit::Shortcut(0x08, -1, 0, 0, 1);            // SHIFT+BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::OverwriteCursor] = ImTextEdit::Shortcut(0x2D, -1, 0, 0, 0);         // INSERT
	ret[(int)ImTextEdit::ShortcutID::Copy] = ImTextEdit::Shortcut(0x43, -1, 0, 1, 0);                     // CTRL+C
	ret[(int)ImTextEdit::ShortcutID::Paste] = ImTextEdit::Shortcut(0x56, -1, 0, 1, 0);                        // CTRL+V
	ret[(int)ImTextEdit::ShortcutID::Cut] = ImTextEdit::Shortcut(0x58, -1, 0, 1, 0);                      // CTRL+X
	ret[(int)ImTextEdit::ShortcutID::SelectAll] = ImTextEdit::Shortcut(0x41, -1, 0, 1, 0);                    // CTRL+A
	ret[(int)ImTextEdit::ShortcutID::AutocompleteOpen] = ImTextEdit::Shortcut(0x20, -1, 0, 1, 0);     // CTRL+SPACE
	ret[(int)ImTextEdit::ShortcutID::AutocompleteSelect] = ImTextEdit::Shortcut(0x09, -1, 0, 0, 0);         // TAB
	ret[(int)ImTextEdit::ShortcutID::AutocompleteSelectActive] = ImTextEdit::Shortcut(0x0D, -1, 0, 0, 0); // RETURN
	ret[(int)ImTextEdit::ShortcutID::AutocompleteUp] = ImTextEdit::Shortcut(0x26, -1, 0, 0, 0);               // UP ARROW
	ret[(int)ImTextEdit::ShortcutID::AutocompleteDown] = ImTextEdit::Shortcut(0x28, -1, 0, 0, 0);           // DOWN ARROW
	ret[(int)ImTextEdit::ShortcutID::NewLine] = ImTextEdit::Shortcut(0x0D, -1, 0, 0, 0);                  // RETURN
	ret[(int)ImTextEdit::ShortcutID::Indent] = ImTextEdit::Shortcut(0x09, -1, 0, 0, 0);                      // TAB
	ret[(int)ImTextEdit::ShortcutID::Unindent] = ImTextEdit::Shortcut(0x09, -1, 0, 0, 1);                    // SHIFT + TAB
	ret[(int)ImTextEdit::ShortcutID::Find] = ImTextEdit::Shortcut(0x46, -1, 0, 1, 0);                          // CTRL+F
	ret[(int)ImTextEdit::ShortcutID::Replace] = ImTextEdit::Shortcut(0x48, -1, 0, 1, 0);                       // CTRL+H
	ret[(int)ImTextEdit::ShortcutID::FindNext] = ImTextEdit::Shortcut(0x72, -1, 0, 0, 0);                      // F3
	ret[(int)ImTextEdit::ShortcutID::DebugStep] = ImTextEdit::Shortcut(0x79, -1, 0, 0, 0);                     // F10
	ret[(int)ImTextEdit::ShortcutID::DebugStepInto] = ImTextEdit::Shortcut(0x7A, -1, 0, 0, 0);                 // F11
	ret[(int)ImTextEdit::ShortcutID::DebugStepOut] = ImTextEdit::Shortcut(0x7A, -1, 0, 0, 1);                  // SHIFT+F11
	ret[(int)ImTextEdit::ShortcutID::DebugContinue] = ImTextEdit::Shortcut(0x74, -1, 0, 0, 0);                 // F5
	ret[(int)ImTextEdit::ShortcutID::DebugStop] = ImTextEdit::Shortcut(0x74, -1, 0, 0, 1);                     // SHIFT+F5
	ret[(int)ImTextEdit::ShortcutID::DebugBreakpoint] = ImTextEdit::Shortcut(0x78, -1, 0, 0, 0);               // F9
	ret[(int)ImTextEdit::ShortcutID::DebugJumpHere] = ImTextEdit::Shortcut(0x48, -1, 1, 1, 0);                 // CTRL+ALT+H
	ret[(int)ImTextEdit::ShortcutID::DuplicateLine] = ImTextEdit::Shortcut(0x44, -1, 0, 1, 0);                 // CTRL+D
	ret[(int)ImTextEdit::ShortcutID::CommentLines] = ImTextEdit::Shortcut(0x4B, -1, 0, 1, 1);                  // CTRL+SHIFT+K
	ret[(int)ImTextEdit::ShortcutID::UncommentLines] = ImTextEdit::Shortcut(0x55, -1, 0, 1, 1);                // CTRL+SHIFT+U

	return ret;
}

ImTextEdit::~ImTextEdit()
{
}

void ImTextEdit::SetLanguageDefinition(const LanguageDefinition & aLanguageDef)
{
	m_LanguageDefinition = aLanguageDef;
	m_RegexList.clear();

	for (auto& r : m_LanguageDefinition.TokenRegexStrings)
		m_RegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

	Colorize();
}

void ImTextEdit::SetPalette(const td_Palette & aValue)
{
	m_PaletteBase = aValue;
}

std::string ImTextEdit::GetText(const Coordinates & aStart, const Coordinates & aEnd) const
{
	std::string result;

	auto lstart = aStart.Line;
	auto lend = aEnd.Line;
	auto istart = GetCharacterIndex(aStart);
	auto iend = GetCharacterIndex(aEnd);
	size_t s = 0;

	for (size_t i = lstart; i < lend; i++)
		s += m_Lines[i].size();

	result.reserve(s + s / 8);

	while (istart < iend || lstart < lend)
	{
		if (lstart >= (int)m_Lines.size())
			break;

		auto& line = m_Lines[lstart];

		if (istart < (int)line.size())
		{
			result += line[istart].Character;
			istart++;
		}
		else
		{
			istart = 0;
			if (!(lstart == lend-1 && iend == -1))
				result += '\n';
			++lstart;
		}
	}

	return result;
}

ImTextEdit::Coordinates ImTextEdit::GetActualCursorCoordinates() const
{
	return SanitizeCoordinates(m_State.CursorPosition);
}

ImTextEdit::Coordinates ImTextEdit::SanitizeCoordinates(const Coordinates & aValue) const
{
	auto line = aValue.Line;
	auto column = aValue.Column;

	if (line >= (int)m_Lines.size())
	{
		if (m_Lines.empty())
		{
			line = 0;
			column = 0;
		}
		else
		{
			line = (int)m_Lines.size() - 1;
			column = GetLineMaxColumn(line);
		}

		return Coordinates(line, column);
	}
	else
	{
		column = std::max<int>(0, m_Lines.empty() ? 0 : std::min(column, GetLineMaxColumn(line)));
		return Coordinates(line, column);
	}
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(ImTextEdit::td_Char c)
{
	if ((c & 0xFE) == 0xFC)
		return 6;
	if ((c & 0xFC) == 0xF8)
		return 5;
	if ((c & 0xF8) == 0xF0)
		return 4;
	else if ((c & 0xF0) == 0xE0)
		return 3;
	else if ((c & 0xE0) == 0xC0)
		return 2;

	return 1;
}

// "Borrowed" from ImGui source
static inline int ImTextCharToUtf8(char* buf, int buf_size, unsigned int c)
{
	if (c < 0x80)
	{
		buf[0] = (char)c;

		return 1;
	}

	if (c < 0x800)
	{
		if (buf_size < 2)
			return 0;

		buf[0] = (char)(0xc0 + (c >> 6));
		buf[1] = (char)(0x80 + (c & 0x3f));
		
		return 2;
	}
	
	if (c >= 0xdc00 && c < 0xe000)
	{
		return 0;
	}
	
	if (c >= 0xd800 && c < 0xdc00)
	{
		if (buf_size < 4)
			return 0;

		buf[0] = (char)(0xf0 + (c >> 18));
		buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
		buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[3] = (char)(0x80 + ((c) & 0x3f));
		
		return 4;
	}

	//else if (c < 0x10000)
	{
		if (buf_size < 3)
			return 0;

		buf[0] = (char)(0xe0 + (c >> 12));
		buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[2] = (char)(0x80 + ((c) & 0x3f));
		
		return 3;
	}
}

void ImTextEdit::Advance(Coordinates & aCoordinates) const
{
	if (aCoordinates.Line < (int)m_Lines.size())
	{
		auto& line = m_Lines[aCoordinates.Line];
		auto cindex = GetCharacterIndex(aCoordinates);

		if (cindex + 1 < (int)line.size())
		{
			auto delta = UTF8CharLength(line[cindex].Character);
			cindex = std::min(cindex + delta, (int)line.size() - 1);
		}
		else
		{
			++aCoordinates.Line;
			cindex = 0;
		}
		aCoordinates.Column = GetCharacterColumn(aCoordinates.Line, cindex);
	}
}

void ImTextEdit::DeleteRange(const Coordinates& aStart, const Coordinates& aEnd)
{
	assert(aEnd >= aStart);
	assert(!m_ReadOnly);

	if (aEnd == aStart)
		return;

	auto start = GetCharacterIndex(aStart);
	auto end = GetCharacterIndex(aEnd);

	if (aStart.Line == aEnd.Line)
	{
		auto& line = m_Lines[aStart.Line];
		auto n = GetLineMaxColumn(aStart.Line);
		
		if (aEnd.Column >= n)
			line.erase(line.begin() + start, line.end());
		else
			line.erase(line.begin() + start, line.begin() + end);

		RemoveFolds(aStart, aEnd);
	}
	else
	{
		auto& firstLine = m_Lines[aStart.Line];
		auto& lastLine = m_Lines[aEnd.Line];

		// remove the folds
		RemoveFolds(aStart, aEnd);

		firstLine.erase(firstLine.begin() + start, firstLine.end());
		lastLine.erase(lastLine.begin(), lastLine.begin() + end);
		
		if (aStart.Line < aEnd.Line)
			firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

		if (aStart.Line < aEnd.Line)
		{
			// remove the actual lines
			RemoveLine(aStart.Line + 1, aEnd.Line + 1);
		}
	}

	if (m_ScrollbarMarkers)
	{
		for (int i = 0; i < m_ChangedLines.size(); i++)
		{
			if (m_ChangedLines[i] > aEnd.Line)
			{
				m_ChangedLines[i] -= (aEnd.Line - aStart.Line);
			}
			else if (m_ChangedLines[i] > aStart.Line && m_ChangedLines[i] < aEnd.Line)
			{
				m_ChangedLines.erase(m_ChangedLines.begin() + i);
				i--;
			}
		}
	}

	m_TextChanged = true;

	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);
}

int ImTextEdit::InsertTextAt(Coordinates& /* inout */ aWhere, const char * aValue, bool indent)
{
	assert(!m_ReadOnly);

	int autoIndentStart = 0;
	for (int i = 0; i < m_Lines[aWhere.Line].size() && indent; i++)
	{
		td_Char ch = m_Lines[aWhere.Line][i].Character;
		
		if (ch == ' ')
			autoIndentStart++;
		else if (ch == '\t')
			autoIndentStart += m_TabSize;
		else
			break;
	}

	int cindex = GetCharacterIndex(aWhere);
	int totalLines = 0;
	int autoIndent = autoIndentStart;
	
	while (*aValue != '\0')
	{
		assert(!m_Lines.empty());

		if (*aValue == '\r')
		{
			// skip
			++aValue;
		}
		else if (*aValue == '\n')
		{
			if (cindex < (int)m_Lines[aWhere.Line].size() && cindex >= 0)
			{
				// normal stuff
				auto& newLine = InsertLine(aWhere.Line + 1, aWhere.Column);
				auto& line = m_Lines[aWhere.Line];
				newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
				line.erase(line.begin() + cindex, line.end());

				// folding
				for (int b = 0; b < m_FoldBegin.size(); b++)
				{
					if (m_FoldBegin[b].Line == aWhere.Line + 1 && m_FoldBegin[b].Column >= aWhere.Column)
						m_FoldBegin[b].Column = std::max<int>(0, m_FoldBegin[b].Column - aWhere.Column);
				}

				for (int b = 0; b < m_FoldEnd.size(); b++)
				{
					if (m_FoldEnd[b].Line == aWhere.Line + 1 && m_FoldEnd[b].Column >= aWhere.Column)
						m_FoldEnd[b].Column = std::max<int>(0, m_FoldEnd[b].Column - aWhere.Column);
				}
			}
			else
			{
				InsertLine(aWhere.Line + 1, aWhere.Column);
			}

			++aWhere.Line;
			cindex = 0;
			aWhere.Column = 0;
			++totalLines;
			++aValue;

			if (indent)
			{
				bool lineIsAlreadyIndent = (isspace(*aValue) && *aValue != '\n' && *aValue != '\r');

				// first check if we need to "unindent"
				const char* bracketSearch = aValue;
				
				while (*bracketSearch != '\0' && isspace(*bracketSearch) && *bracketSearch != '\n')
					bracketSearch++;
				
				if (*bracketSearch == '}')
					autoIndent = std::max(0, autoIndent - m_TabSize);

				int actualAutoIndent = autoIndent;
				
				if (lineIsAlreadyIndent)
				{
					actualAutoIndent = autoIndentStart;

					const char* aValueCopy = aValue;
					
					while (isspace(*aValueCopy) && *aValueCopy != '\n' && *aValueCopy != '\r' && *aValueCopy != 0)
					{
						actualAutoIndent = std::max(0, actualAutoIndent - m_TabSize);
						aValueCopy++;
					}
				}

				// add tabs
				int tabCount = actualAutoIndent / m_TabSize;
				int spaceCount = actualAutoIndent - tabCount * m_TabSize;
				
				if (m_InsertSpaces)
				{
					tabCount = 0;
					spaceCount = actualAutoIndent;
				}

				cindex = tabCount + spaceCount;
				aWhere.Column = actualAutoIndent;

				// folding
				for (int b = 0; b < m_FoldBegin.size(); b++)
				{
					if (m_FoldBegin[b].Line == aWhere.Line && m_FoldBegin[b].Column >= aWhere.Column)
						m_FoldBegin[b].Column += spaceCount + tabCount * m_TabSize;
				}

				for (int b = 0; b < m_FoldEnd.size(); b++)
				{
					if (m_FoldEnd[b].Line == aWhere.Line && m_FoldEnd[b].Column >= aWhere.Column)
						m_FoldEnd[b].Column += spaceCount + tabCount * m_TabSize;
				}

				// insert the spaces/tabs
				while (spaceCount-- > 0)
				{
					m_Lines[aWhere.Line].insert(m_Lines[aWhere.Line].begin(), Glyph(' ', PaletteIndex::Default));
					
					for (int i = 0; i < m_SnippetTagStart.size(); i++)
					{
						if (m_SnippetTagStart[i].Line == aWhere.Line)
						{
							m_SnippetTagStart[i].Column++;
							m_SnippetTagEnd[i].Column++;
						}
					}
				}
				
				while (tabCount-- > 0)
				{
					m_Lines[aWhere.Line].insert(m_Lines[aWhere.Line].begin(), Glyph('\t', PaletteIndex::Default));
					
					for (int i = 0; i < m_SnippetTagStart.size(); i++)
					{
						if (m_SnippetTagStart[i].Line == aWhere.Line)
						{
							m_SnippetTagStart[i].Column += m_TabSize;
							m_SnippetTagEnd[i].Column += m_TabSize;
						}
					}
				}
			}
		}
		else
		{
			char aValueOld = *aValue;
			bool isTab = (aValueOld == '\t');
			auto& line = m_Lines[aWhere.Line];
			auto d = UTF8CharLength(aValueOld);
			int foldOffset = 0;
			
			while (d-- > 0 && *aValue != '\0')
			{
				foldOffset += (*aValue == '\t') ? m_TabSize : 1;
				line.insert(line.begin() + cindex++, Glyph(*aValue++, PaletteIndex::Default));
			}

			// shift old fold info
			for (int i = 0; i < m_FoldBegin.size(); i++)
			{
				if (m_FoldBegin[i].Line == aWhere.Line && m_FoldBegin[i].Column >= aWhere.Column)
					m_FoldBegin[i].Column += foldOffset;
			}

			for (int i = 0; i < m_FoldEnd.size(); i++)
			{
				if (m_FoldEnd[i].Line == aWhere.Line && m_FoldEnd[i].Column >= aWhere.Column)
					m_FoldEnd[i].Column += foldOffset;
			}

			// insert new fold info
			if (aValueOld == '{')
			{
				autoIndent += m_TabSize;

				m_FoldBegin.push_back(aWhere);
				m_FoldSorted = false;
			}
			else if (aValueOld == '}')
			{
				autoIndent = std::max(0, autoIndent - m_TabSize);

				m_FoldEnd.push_back(aWhere);
				m_FoldSorted = false;
			}

			aWhere.Column += (isTab ? m_TabSize : 1);
		}
	}

	if (m_ScrollbarMarkers)
	{
		bool changeExists = false;
		
		for (int i = 0; i < m_ChangedLines.size(); i++)
		{
			if (m_ChangedLines[i] == aWhere.Line)
			{
				changeExists = true;
				break;
			}
		}

		if (!changeExists)
			m_ChangedLines.push_back(aWhere.Line);
	}

	m_TextChanged = true;

	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);

	return totalLines;
}

void ImTextEdit::AddUndo(UndoRecord& aValue)
{
	assert(!m_ReadOnly);
	//printf("AddUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
	//	aValue.mBefore.mCursorPosition.mLine, aValue.mBefore.mCursorPosition.mColumn,
	//	aValue.mAdded.c_str(), aValue.mAddedStart.mLine, aValue.mAddedStart.mColumn, aValue.mAddedEnd.mLine, aValue.mAddedEnd.mColumn,
	//	aValue.mRemoved.c_str(), aValue.mRemovedStart.mLine, aValue.mRemovedStart.mColumn, aValue.mRemovedEnd.mLine, aValue.mRemovedEnd.mColumn,
	//	aValue.mAfter.mCursorPosition.mLine, aValue.mAfter.mCursorPosition.mColumn
	//	);

	m_UndoBuffer.resize((size_t)(m_UndoIndex + 1));
	m_UndoBuffer.back() = aValue;
	++m_UndoIndex;
}

ImTextEdit::Coordinates ImTextEdit::ScreenPosToCoordinates(const ImVec2& aPosition) const
{
	ImVec2 origin = m_UICursorPos;
	ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

	int lineNo = std::max(0, (int)floor(local.y / m_CharAdvance.y));
	int columnCoord = 0;

	// check for folds
	if (m_FoldEnabled)
	{
		auto foldLineStart = 0;
		auto foldLineEnd = std::min<int>((int)m_Lines.size() - 1, lineNo);
	
		while (foldLineStart < foldLineEnd)
		{
			// check if line is folded
			for (int i = 0; i < m_FoldBegin.size(); i++)
			{
				if (m_FoldBegin[i].Line == foldLineStart)
				{
					if (i < m_Fold.size() && m_Fold[i])
					{
						int foldCon = m_FoldConnection[i];

						if (foldCon != -1 && foldCon < m_FoldEnd.size())
						{
							int diff = m_FoldEnd[foldCon].Line - m_FoldBegin[i].Line;
							lineNo += diff;
							foldLineEnd = std::min<int>((int)m_Lines.size() - 1, foldLineEnd + diff);
						}

						break;
					}
				}
			}

			foldLineStart++;
		}
	}

	if (lineNo >= 0 && lineNo < (int)m_Lines.size())
	{
		auto& line = m_Lines.at(lineNo);

		int columnIndex = 0;
		float columnX = 0.0f;

		while ((size_t)columnIndex < line.size())
		{
			float columnWidth = 0.0f;

			if (line[columnIndex].Character == '\t')
			{
				float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
				float oldX = columnX;
				float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(m_TabSize) * spaceSize))) * (float(m_TabSize) * spaceSize);
				columnWidth = newColumnX - oldX;
				
				if (m_TextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				columnX = newColumnX;
				columnCoord = (columnCoord / m_TabSize) * m_TabSize + m_TabSize;
				columnIndex++;
			}
			else
			{
				char buf[7];
				auto d = UTF8CharLength(line[columnIndex].Character);
				int i = 0;
				while (i < 6 && d-- > 0)
					buf[i++] = line[columnIndex++].Character;
				buf[i] = '\0';
				columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
				
				if (m_TextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				
				columnX += columnWidth;
				columnCoord++;
			}
		}
	}

	return SanitizeCoordinates(Coordinates(lineNo, columnCoord));
}
ImTextEdit::Coordinates ImTextEdit::MousePosToCoordinates(const ImVec2& aPosition) const
{
	ImVec2 origin = m_UICursorPos;
	ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

	int lineNo = std::max(0, (int)floor(local.y / m_CharAdvance.y));
	int columnCoord = 0;
	int modifier = 0;

	// check for folds
	if (m_FoldEnabled)
	{
		auto foldLineStart = 0;
		auto foldLineEnd = std::min<int>((int)m_Lines.size() - 1, lineNo);
	
		while (foldLineStart < foldLineEnd)
		{
			// check if line is folded
			for (int i = 0; i < m_FoldBegin.size(); i++)
			{
				if (m_FoldBegin[i].Line == foldLineStart)
				{
					if (i < m_Fold.size() && m_Fold[i])
					{
						int foldCon = m_FoldConnection[i];

						if (foldCon != -1 && foldCon < m_FoldEnd.size())
						{
							int diff = m_FoldEnd[foldCon].Line - m_FoldBegin[i].Line;
							lineNo += diff;
							foldLineEnd = std::min<int>((int)m_Lines.size() - 1, foldLineEnd + diff);
						}

						break;
					}
				}
			}

			foldLineStart++;
		}
	}

	if (lineNo >= 0 && lineNo < (int)m_Lines.size())
	{
		auto& line = m_Lines.at(lineNo);

		int columnIndex = 0;
		float columnX = 0.0f;

		while ((size_t)columnIndex < line.size())
		{
			float columnWidth = 0.0f;

			if (line[columnIndex].Character == '\t')
			{
				float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
				float oldX = columnX;
				float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(m_TabSize) * spaceSize))) * (float(m_TabSize) * spaceSize);
				columnWidth = newColumnX - oldX;
				
				if (m_TextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				
				columnX = newColumnX;
				columnCoord = (columnCoord / m_TabSize) * m_TabSize + m_TabSize;
				columnIndex++;
				modifier += 3;
			}
			else
			{
				char buf[7];
				auto d = UTF8CharLength(line[columnIndex].Character);
				int i = 0;

				while (i < 6 && d-- > 0)
					buf[i++] = line[columnIndex++].Character;
				
				buf[i] = '\0';
				columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
				
				if (m_TextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				
				columnX += columnWidth;
				columnCoord++;
			}
		}
	}

	return SanitizeCoordinates(Coordinates(lineNo, columnCoord - modifier));
}

ImTextEdit::Coordinates ImTextEdit::FindWordStart(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	
	if (at.Line >= (int)m_Lines.size())
		return at;

	auto& line = m_Lines[at.Line];
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	while (cindex > 0 && isspace(line[cindex].Character))
		--cindex;

	auto cstart = (PaletteIndex)line[cindex].ColorIndex;
	while (cindex > 0)
	{
		auto c = line[cindex].Character;

		if ((c & 0xC0) != 0x80)	// not UTF code sequence 10xxxxxx
		{
			if (c <= 32 && isspace(c))
			{
				cindex++;
				break;
			}
		
			if (cstart != (PaletteIndex)line[size_t(cindex - 1)].ColorIndex)
				break;
		}
		--cindex;
	}
	return Coordinates(at.Line, GetCharacterColumn(at.Line, cindex));
}

ImTextEdit::Coordinates ImTextEdit::FindWordEnd(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	
	if (at.Line >= (int)m_Lines.size())
		return at;

	auto& line = m_Lines[at.Line];
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	bool prevspace = (bool)isspace(line[cindex].Character);
	auto cstart = (PaletteIndex)line[cindex].ColorIndex;
	while (cindex < (int)line.size())
	{
		auto c = line[cindex].Character;
		auto d = UTF8CharLength(c);

		if (cstart != (PaletteIndex)line[cindex].ColorIndex)
			break;

		if (prevspace != !!isspace(c))
		{
			if (isspace(c))
				while (cindex < (int)line.size() && isspace(line[cindex].Character))
					++cindex;
			break;
		}

		cindex += d;
	}
	return Coordinates(aFrom.Line, GetCharacterColumn(aFrom.Line, cindex));
}

ImTextEdit::Coordinates ImTextEdit::FindNextWord(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	
	if (at.Line >= (int)m_Lines.size())
		return at;

	// skip to the next non-word character
	auto cindex = GetCharacterIndex(aFrom);
	bool isword = false;
	bool skip = false;

	if (cindex < (int)m_Lines[at.Line].size())
	{
		auto& line = m_Lines[at.Line];
		isword = isalnum(line[cindex].Character);
		skip = isword;
	}

	while (!isword || skip)
	{
		if (at.Line >= m_Lines.size())
		{
			auto l = std::max(0, (int) m_Lines.size() - 1);
			return Coordinates(l, GetLineMaxColumn(l));
		}

		auto& line = m_Lines[at.Line];

		if (cindex < (int)line.size())
		{
			isword = isalnum(line[cindex].Character);

			if (isword && !skip)
				return Coordinates(at.Line, GetCharacterColumn(at.Line, cindex));

			if (!isword)
				skip = false;

			cindex++;
		}
		else
		{
			cindex = 0;
			++at.Line;
			skip = false;
			isword = false;
		}
	}

	return at;
}

int ImTextEdit::GetCharacterIndex(const Coordinates& aCoordinates) const
{
	if (aCoordinates.Line >= m_Lines.size())
		return -1;

	auto& line = m_Lines[aCoordinates.Line];
	int c = 0;
	int i = 0;
	
	for (; i < line.size() && c < aCoordinates.Column;)
	{
		if (line[i].Character == '\t')
			c = (c / m_TabSize) * m_TabSize + m_TabSize;
		else
			++c;

		i += UTF8CharLength(line[i].Character);
	}

	return i;
}

int ImTextEdit::GetCharacterColumn(int aLine, int aIndex) const
{
	if (aLine >= m_Lines.size())
		return 0;

	auto& line = m_Lines[aLine];
	int col = 0;
	int i = 0;
	
	while (i < aIndex && i < (int)line.size())
	{
		auto c = line[i].Character;
		i += UTF8CharLength(c);
	
		if (c == '\t')
			col = (col / m_TabSize) * m_TabSize + m_TabSize;
		else
			col++;
	}
	
	return col;
}

int ImTextEdit::GetLineCharacterCount(int aLine) const
{
	if (aLine >= m_Lines.size())
		return 0;

	auto& line = m_Lines[aLine];
	int c = 0;
	
	for (unsigned i = 0; i < line.size(); c++)
		i += UTF8CharLength(line[i].Character);
	
	return c;
}

int ImTextEdit::GetLineMaxColumn(int aLine) const
{
	if (aLine >= m_Lines.size())
		return 0;

	auto& line = m_Lines[aLine];
	int col = 0;
	
	for (unsigned i = 0; i < line.size();)
	{
		auto c = line[i].Character;

		if (c == '\t')
			col = (col / m_TabSize) * m_TabSize + m_TabSize;
		else
			col++;
		
		i += UTF8CharLength(c);
	}
	return col;
}

bool ImTextEdit::IsOnWordBoundary(const Coordinates & aAt) const
{
	if (aAt.Line >= (int)m_Lines.size() || aAt.Column == 0)
		return true;

	auto& line = m_Lines[aAt.Line];
	auto cindex = GetCharacterIndex(aAt);
	
	if (cindex >= (int)line.size())
		return true;

	if (m_ColorizerEnabled)
		return line[cindex].ColorIndex != line[size_t(cindex - 1)].ColorIndex;

	return isspace(line[cindex].Character) != isspace(line[cindex - 1].Character);
}

void ImTextEdit::RemoveLine(int aStart, int aEnd)
{
	assert(!m_ReadOnly);
	assert(aEnd >= aStart);
	assert(m_Lines.size() > (size_t)(aEnd - aStart));

	td_ErrorMarkers etmp;

	for (auto& i : m_ErrorMarkers)
	{
		td_ErrorMarkers::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
		
		if (e.first >= aStart && e.first <= aEnd)
			continue;
		
		etmp.insert(e);
	}
	
	m_ErrorMarkers = std::move(etmp);

	auto btmp = m_Breakpoints;
	m_Breakpoints.clear();
	
	for (auto i : btmp)
	{
		if (i.Line >= aStart && i.Line <= aEnd)
		{
			RemoveBreakpoint(i.Line);
			continue;
		}
		
		AddBreakpoint(i.Line >= aStart ? i.Line - 1 : i.Line, i.UseCondition, i.Condition, i.Enabled);
	}

	m_Lines.erase(m_Lines.begin() + aStart, m_Lines.begin() + aEnd);
	assert(!m_Lines.empty());

	// remove scrollbar markers
	if (m_ScrollbarMarkers)
	{
		for (int i = 0; i < m_ChangedLines.size(); i++)
		{
			if (m_ChangedLines[i] > aEnd)
			{
				m_ChangedLines[i] -= (aEnd - aStart);
			}
			else if (m_ChangedLines[i] >= aStart && m_ChangedLines[i] <= aEnd)
			{
				m_ChangedLines.erase(m_ChangedLines.begin() + i);
				i--;
			}
		}
	}

	m_TextChanged = true;

	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);
}

void ImTextEdit::RemoveLine(int aIndex)
{
	assert(!m_ReadOnly);
	assert(m_Lines.size() > 1);

	td_ErrorMarkers etmp;

	for (auto& i : m_ErrorMarkers)
	{
		td_ErrorMarkers::value_type e(i.first > aIndex ? i.first - 1 : i.first, i.second);
		
		if (e.first - 1 == aIndex)
			continue;
		
		etmp.insert(e);
	}

	m_ErrorMarkers = std::move(etmp);

	auto btmp = m_Breakpoints;
	m_Breakpoints.clear();

	for (auto i : btmp)
	{
		if (i.Line == aIndex)
		{
			RemoveBreakpoint(i.Line);
			continue;
		}

		AddBreakpoint(i.Line >= aIndex ? i.Line - 1 : i.Line, i.UseCondition, i.Condition, i.Enabled);
	}

	m_Lines.erase(m_Lines.begin() + aIndex);
	assert(!m_Lines.empty());

	// remove folds
	RemoveFolds(Coordinates(aIndex, 0), Coordinates(aIndex, 100000));

	// move/remove scrollbar markers
	if (m_ScrollbarMarkers)
	{
		for (int i = 0; i < m_ChangedLines.size(); i++)
		{
			if (m_ChangedLines[i] > aIndex)
			{
				m_ChangedLines[i]--;
			}
			else if (m_ChangedLines[i] == aIndex)
			{
				m_ChangedLines.erase(m_ChangedLines.begin() + i);
				i--;
			}

		}
	}

	m_TextChanged = true;

	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);
}

ImTextEdit::Line& ImTextEdit::InsertLine(int aIndex, int column)
{
	assert(!m_ReadOnly);

	auto& result = *m_Lines.insert(m_Lines.begin() + aIndex, Line());

	// folding
	for (int b = 0; b < m_FoldBegin.size(); b++)
	{
		if (m_FoldBegin[b].Line > aIndex - 1 || (m_FoldBegin[b].Line == aIndex - 1 && m_FoldBegin[b].Column >= column))
			m_FoldBegin[b].Line++;
	}

	for (int b = 0; b < m_FoldEnd.size(); b++)
	{
		if (m_FoldEnd[b].Line > aIndex - 1 || (m_FoldEnd[b].Line == aIndex - 1 && m_FoldEnd[b].Column >= column))
			m_FoldEnd[b].Line++;
	}

	// error markers
	td_ErrorMarkers etmp;

	for (auto& i : m_ErrorMarkers)
		etmp.insert(td_ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
	
	m_ErrorMarkers = std::move(etmp);

	// breakpoints
	auto btmp = m_Breakpoints;
	m_Breakpoints.clear();

	for (auto i : btmp)
		RemoveBreakpoint(i.Line);
	
	for (auto i : btmp)
		AddBreakpoint(i.Line >= aIndex ? i.Line + 1 : i.Line, i.UseCondition, i.Condition, i.Enabled);

	return result;
}

std::string ImTextEdit::GetWordUnderCursor() const
{
	auto c = GetCursorPosition();
	c.Column = std::max(c.Column - 1, 0);

	return GetWordAt(c);
}

std::string ImTextEdit::GetWordAt(const Coordinates & aCoords) const
{
	auto start = FindWordStart(aCoords);
	auto end = FindWordEnd(aCoords);

	std::string r;

	auto istart = GetCharacterIndex(start);
	auto iend = GetCharacterIndex(end);

	for (auto it = istart; it < iend; ++it)
		r.push_back(m_Lines[aCoords.Line][it].Character);

	return r;
}

ImU32 ImTextEdit::GetGlyphColor(const Glyph & aGlyph) const
{
	if (!m_ColorizerEnabled)
		return m_Palette[(int)PaletteIndex::Default];

	if (aGlyph.Comment)
		return m_Palette[(int)PaletteIndex::Comment];
	
	if (aGlyph.MultiLineComment)
		return m_Palette[(int)PaletteIndex::MultiLineComment];
	
	auto const color = m_Palette[(int)aGlyph.ColorIndex];
	
	if (aGlyph.Preprocessor)
	{
		const auto ppcolor = m_Palette[(int)PaletteIndex::Preprocessor];
		const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
		const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
		const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
		const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;

		return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
	}

	return color;
}

ImTextEdit::Coordinates ImTextEdit::FindFirst(const std::string& what, const Coordinates& fromWhere)
{
	if (fromWhere.Line < 0 || fromWhere.Line >= m_Lines.size())
		return Coordinates(m_Lines.size(), 0);

	std::string textSrc = GetText(fromWhere, Coordinates((int)m_Lines.size(), 0));

	size_t index = 0;
	size_t loc = textSrc.find(what);
	Coordinates ret = fromWhere;

	while (loc != std::string::npos)
	{
		for (; index < loc; index++)
		{
			if (textSrc[index] == '\n')
			{
				ret.Column = 0;
				ret.Line++;
			}
			else
			{
				ret.Column++;
			}
		}

		ret.Column = GetCharacterColumn(ret.Line, ret.Column); // ret.Column is currently character index, convert it to character column

		if (GetWordAt(ret) == what)
			return ret;
		else
			loc = textSrc.find(what, loc + 1);
	}

	return Coordinates(m_Lines.size(), 0);
}

void ImTextEdit::HandleKeyboardInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

	if (ImGui::IsWindowFocused())
	{
		if (ImGui::IsWindowHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
		//ImGui::CaptureKeyboardFromApp(true);

		io.WantCaptureKeyboard = true;
		io.WantTextInput = true;

		ShortcutID actionID = ShortcutID::Count;
		for (int i = 0; i < m_Shortcuts.size(); i++)
		{
			auto sct = m_Shortcuts[i];

			if (sct.Key1 == -1)
				continue;
				
			ShortcutID curActionID = ShortcutID::Count;
			bool additionalChecks = true;

			int sc1 = ::GetAsyncKeyState(sct.Key1) & 0x8000 ? sct.Key1 : -1;

			if ((ImGui::IsKeyPressed(sc1) || (sc1 == VK_RETURN && ImGui::IsKeyPressed(VK_RETURN))) && ((sct.Key2 != -1 && ImGui::IsKeyPressed(sct.Key2)) || sct.Key2 == -1))
			{
				if ((sct.Ctrl == ctrl) && (sct.Alt == alt) && (sct.Shift == shift))
				{
					// PRESSED:
					curActionID = (ImTextEdit::ShortcutID)i;
					
					switch (curActionID)
					{
						case ShortcutID::Paste:
						case ShortcutID::Cut:
						case ShortcutID::Redo: 
						case ShortcutID::Undo:
						case ShortcutID::ForwardDelete:
						case ShortcutID::BackwardDelete:
						case ShortcutID::DeleteLeft:
						case ShortcutID::DeleteRight:
						case ShortcutID::ForwardDeleteWord:
						case ShortcutID::BackwardDeleteWord:
							additionalChecks = !IsReadOnly();
							break;
						default:
							break;
					}
				}
			}

			if (additionalChecks && curActionID != ShortcutID::Count)
				actionID = curActionID;
		}

		int keyCount = 0;
		bool keepACOpened = false, functionTooltipState = m_FunctionDeclarationTooltip;
		bool hasWrittenALetter = false;
		if (actionID != ShortcutID::Count)
		{
			if (actionID != ShortcutID::Indent)
				m_IsSnippet = false;
			
			switch (actionID)
			{
				case ShortcutID::Undo:
					Undo();
					break;
				case ShortcutID::Redo:
					Redo();
					break;
				case ShortcutID::MoveUp:
					MoveUp(1, false);
					break;
				case ShortcutID::SelectUp:
					MoveUp(1, true);
					break;
				case ShortcutID::MoveDown:
					MoveDown(1, false);
					break;
				case ShortcutID::SelectDown:
					MoveDown(1, true);
					break;
				case ShortcutID::MoveLeft:
					MoveLeft(1, false, false);
					break;
				case ShortcutID::SelectLeft:
					MoveLeft(1, true, false);
					break;
				case ShortcutID::MoveWordLeft:
					MoveLeft(1, false, true);
					break;
				case ShortcutID::SelectWordLeft:
					MoveLeft(1, true, true);
					break;
				case ShortcutID::MoveRight:
					MoveRight(1, false, false);
					break;
				case ShortcutID::SelectRight:
					MoveRight(1, true, false);
					break;
				case ShortcutID::MoveWordRight:
					MoveRight(1, false, true);
					break;
				case ShortcutID::SelectWordRight:
					MoveRight(1, true, true);
					break;
				case ShortcutID::MoveTop:
					MoveTop(false);
					break;
				case ShortcutID::SelectTop:
					MoveTop(true);
					break;
				case ShortcutID::MoveBottom:
					MoveBottom(false);
					break;
				case ShortcutID::SelectBottom:
					MoveBottom(true);
					break;
				case ShortcutID::MoveUpBlock:
					MoveUp(GetPageSize() - 4, false);
					break;
				case ShortcutID::MoveDownBlock:
					MoveDown(GetPageSize() - 4, false);
					break;
				case ShortcutID::SelectUpBlock:
					MoveUp(GetPageSize() - 4, true);
					break;
				case ShortcutID::SelectDownBlock:
					MoveDown(GetPageSize() - 4, true);
					break;
				case ShortcutID::MoveEndLine:
					MoveEnd(false);
					break;
				case ShortcutID::SelectEndLine:
					MoveEnd(true);
					break;
				case ShortcutID::MoveStartLine:
					MoveHome(false);
					break;
				case ShortcutID::SelectStartLine:
					MoveHome(true);
					break;
				case ShortcutID::DeleteRight:
				case ShortcutID::ForwardDelete:
					Delete();
					break;
				case ShortcutID::ForwardDeleteWord:
					if (ctrl && m_State.SelectionStart == m_State.SelectionEnd)
						MoveRight(1, true, true);
					Delete();
					break;
				case ShortcutID::DeleteLeft:
				case ShortcutID::BackwardDelete:
					Backspace();
					break;
				case ShortcutID::BackwardDeleteWord:
					if (ctrl && (m_State.SelectionStart == m_State.SelectionEnd || m_State.SelectionStart == m_State.CursorPosition))
						MoveLeft(1, true, true);
					Backspace();
					break;
				case ShortcutID::OverwriteCursor:
					m_Overwrite ^= true;
					break;
				case ShortcutID::Copy:
					Copy();
					break;
				case ShortcutID::Paste:
					Paste();
					break;
				case ShortcutID::Cut:
					Cut();
					break;
				case ShortcutID::SelectAll:
					SelectAll();
					break;
				case ShortcutID::AutocompleteOpen:
					if (m_Autocomplete && !m_IsSnippet)
						BuildSuggestions(&keepACOpened);
					break;
				case ShortcutID::AutocompleteSelect:
				case ShortcutID::AutocompleteSelectActive:
					AutocompleteSelect();
					break;
				case ShortcutID::AutocompleteUp:
					m_ACIndex = std::max<int>(m_ACIndex - 1, 0), m_ACSwitched = true;
					keepACOpened = true;
					break;
				case ShortcutID::AutocompleteDown:
					m_ACIndex = std::min<int>(m_ACIndex + 1, (int)m_ACSuggestions.size() - 1), m_ACSwitched = true;
					keepACOpened = true;
					break;
				case ShortcutID::NewLine:
					EnterCharacter('\n', false);
					break;
				case ShortcutID::Indent:
					if (m_IsSnippet)
					{
						do
						{
							m_SnippetTagSelected++;
							if (m_SnippetTagSelected >= m_SnippetTagStart.size())
								m_SnippetTagSelected = 0;
						}
						while (!m_SnippetTagHighlight[m_SnippetTagSelected]);

						m_SnippetTagLength = 0;
						m_SnippetTagPreviousLength = m_SnippetTagEnd[m_SnippetTagSelected].Column - m_SnippetTagStart[m_SnippetTagSelected].Column;

						SetSelection(m_SnippetTagStart[m_SnippetTagSelected], m_SnippetTagEnd[m_SnippetTagSelected]);
						SetCursorPosition(m_SnippetTagEnd[m_SnippetTagSelected]);
					}
					else
					{
						EnterCharacter('\t', false);
					}
					break;
				case ShortcutID::Unindent:
					EnterCharacter('\t', true);
					break;
				case ShortcutID::Find:
					m_FindOpened = m_HasSearch;
					m_FindJustOpened = m_HasSearch;
					m_ReplaceOpened = false;
					break;
				case ShortcutID::Replace:
					m_FindOpened = m_HasSearch;
					m_FindJustOpened = m_HasSearch;
					m_ReplaceOpened = m_HasSearch;
					break;
				case ShortcutID::DebugStep:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::Step);
					break;
				case ShortcutID::DebugStepInto:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::StepInto);
					break;
				case ShortcutID::DebugStepOut:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::StepOut);
					break;
				case ShortcutID::DebugContinue:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::Continue);
					break;
				case ShortcutID::DebugStop:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::Stop);
					break;
				case ShortcutID::DebugJumpHere:
					if (OnDebuggerJump)
						OnDebuggerJump(this, GetCursorPosition().Line);
					break;
				case ShortcutID::DebugBreakpoint:
					if (OnBreakpointUpdate)
					{
						int line = GetCursorPosition().Line + 1;

						if (HasBreakpoint(line))
							RemoveBreakpoint(line);
						else AddBreakpoint(line);
					}
					break;
				case ShortcutID::DuplicateLine:
				{
					/*ImTextEdit::UndoRecord undo;
					undo.Before = m_State;

					auto oldLine = m_Lines[m_State.CursorPosition.Line];
					auto& line = InsertLine(m_State.CursorPosition.Line, m_State.CursorPosition.Column);

					undo.Added += '\n';

					for (auto& glyph : oldLine)
					{
						line.push_back(glyph);
						undo.Added += glyph.Character;
					}
					
					m_State.CursorPosition.Line++;

					undo.AddedStart = ImTextEdit::Coordinates(m_State.CursorPosition.Line - 1, m_State.CursorPosition.Column);
					undo.AddedEnd = m_State.CursorPosition;

					undo.After = m_State;

					AddUndo(undo);*/
					break;
				}
				case ShortcutID::CommentLines:
				{
					for (int l = m_State.SelectionStart.Line; l <= m_State.SelectionEnd.Line && l < m_Lines.size(); l++)
					{
						m_Lines[l].insert(m_Lines[l].begin(), ImTextEdit::Glyph('/', ImTextEdit::PaletteIndex::Comment));
						m_Lines[l].insert(m_Lines[l].begin(), ImTextEdit::Glyph('/', ImTextEdit::PaletteIndex::Comment));
					}
				
					Colorize(m_State.SelectionStart.Line, m_State.SelectionEnd.Line);
					break;
				}
				case ShortcutID::UncommentLines:
				{
					for (int l = m_State.SelectionStart.Line; l <= m_State.SelectionEnd.Line && l < m_Lines.size(); l++)
					{
						if (m_Lines[l].size() >= 2)
						{
							if (m_Lines[l][0].Character == '/' && m_Lines[l][1].Character == '/')
								m_Lines[l].erase(m_Lines[l].begin(), m_Lines[l].begin() + 2);
						}
					}

					Colorize(m_State.SelectionStart.Line, m_State.SelectionEnd.Line);
					break;
				}
			}
		}
		else if (!IsReadOnly())
		{
			for (int i = 0; i < io.InputQueueCharacters.Size; i++)
			{
				auto c = (unsigned char)io.InputQueueCharacters[i];

				if (c != 0 && (c == '\n' || c >= 32))
				{
					EnterCharacter((char)c, shift);

					if (c == '.')
						BuildMemberSuggestions(&keepACOpened);

					if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
						hasWrittenALetter = true;

					if (m_IsSnippet)
					{
						m_SnippetTagLength++;
						m_SnippetTagEnd[m_SnippetTagSelected].Column = m_SnippetTagStart[m_SnippetTagSelected].Column + m_SnippetTagLength;
					
						Coordinates curCursor = GetCursorPosition();
						SetSelection(m_SnippetTagStart[m_SnippetTagSelected], m_SnippetTagEnd[m_SnippetTagSelected]);
						std::string curWord = GetSelectedText();
						std::unordered_map<int, int> modif;
						modif[curCursor.Line] = 0;

						for (int j = 0; j < m_SnippetTagStart.size(); j++)
						{
							if (j != m_SnippetTagSelected)
							{
								m_SnippetTagStart[j].Column += modif[m_SnippetTagStart[j].Line];
								m_SnippetTagEnd[j].Column += modif[m_SnippetTagStart[j].Line];
							}

							if (m_SnippetTagID[j] == m_SnippetTagID[m_SnippetTagSelected])
							{
								modif[m_SnippetTagStart[j].Line] += m_SnippetTagLength - m_SnippetTagPreviousLength;

								if (j != m_SnippetTagSelected)
								{
									SetSelection(m_SnippetTagStart[j], m_SnippetTagEnd[j]);
									Backspace();
									AppendText(curWord);
									m_SnippetTagEnd[j].Column = m_SnippetTagStart[j].Column + m_SnippetTagLength;
								}
							}
						}

						SetSelection(curCursor, curCursor);
						SetCursorPosition(curCursor);
						EnsureCursorVisible();
						m_SnippetTagPreviousLength = m_SnippetTagLength;
					}

					keyCount++;
				}
			}
			io.InputQueueCharacters.resize(0);
		}

		// active auto-complete
		if (m_RequestAutocomplete && m_ReadyForAutocomplete && !m_IsSnippet)
		{
			BuildSuggestions(&keepACOpened);
			m_RequestAutocomplete = false;
			m_ReadyForAutocomplete = false;
		}

		if ((m_ACOpened && !keepACOpened) || m_FunctionDeclarationTooltip)
		{
			for (size_t i = 0; i < ImGuiKey_COUNT; i++)
				keyCount += ImGui::IsKeyPressed(ImGui::GetKeyIndex(i));

			if (keyCount != 0)
			{
				if (functionTooltipState == m_FunctionDeclarationTooltip)
					m_FunctionDeclarationTooltip = false;

				m_ACOpened = false;

				if (!hasWrittenALetter)
					m_ACObject = "";
			}
		}
	}
}

void ImTextEdit::HandleMouseInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

	if (ImGui::IsWindowHovered())
	{
		auto click = ImGui::IsMouseClicked(0);
	
		if ((!shift || (shift && click)) && !alt)
		{
			auto doubleClick = ImGui::IsMouseDoubleClicked(0);
			auto t = ImGui::GetTime();
			auto tripleClick = click && !doubleClick && (m_LastClick != -1.0f && (t - m_LastClick) < io.MouseDoubleClickTime);

			if (click || doubleClick || tripleClick)
			{
				m_IsSnippet = false;
				m_FunctionDeclarationTooltip = false;
			}

			if (tripleClick) // Left mouse button triple click
			{
				if (!ctrl)
				{
					m_State.CursorPosition = m_InteractiveStart = m_InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
					m_SelectionMode = SelectionMode::Line;
					SetSelection(m_InteractiveStart, m_InteractiveEnd, m_SelectionMode);
				}

				m_LastClick = -1.0f;
			}
			else if (doubleClick) // Left mouse button double click
			{
				if (!ctrl)
				{
					m_State.CursorPosition = m_InteractiveStart = m_InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
					if (m_SelectionMode == SelectionMode::Line)
						m_SelectionMode = SelectionMode::Normal;
					else
						m_SelectionMode = SelectionMode::Word;
					SetSelection(m_InteractiveStart, m_InteractiveEnd, m_SelectionMode);
					m_State.CursorPosition = m_State.SelectionEnd;
				}

				m_LastClick = (float)ImGui::GetTime();
			}
			else if (click)  // Mouse button click
			{
				ImVec2 pos = ImGui::GetMousePos();

				if (pos.x - m_UICursorPos.x < ImGui::GetStyle().WindowPadding.x + EditorCalculateSize(s_DebugDataSpace))
				{
					Coordinates lineInfo = ScreenPosToCoordinates(ImGui::GetMousePos());
					lineInfo.Line += 1;

					if (HasBreakpoint(lineInfo.Line))
						RemoveBreakpoint(lineInfo.Line);
					else
						AddBreakpoint(lineInfo.Line);
				}
				else
				{
					m_ACOpened = false;
					m_ACObject = "";

					auto tcoords = ScreenPosToCoordinates(ImGui::GetMousePos());
					
					if (!shift)
						m_InteractiveStart = tcoords;
					
					m_State.CursorPosition = m_InteractiveEnd = tcoords;

					if (ctrl && !shift)
						m_SelectionMode = SelectionMode::Word;
					else
						m_SelectionMode = SelectionMode::Normal;

					SetSelection(m_InteractiveStart, m_InteractiveEnd, m_SelectionMode);

					m_LastClick = (float)ImGui::GetTime();
				}
			}
			else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) // Mouse left button dragging (=> update selection)
			{
				io.WantCaptureMouse = true;
				m_State.CursorPosition = m_InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
				SetSelection(m_InteractiveStart, m_InteractiveEnd, m_SelectionMode);

				float mx = ImGui::GetMousePos().x;

				if (mx > m_FindOrigin.x + m_WindowWidth - 50 && mx < m_FindOrigin.x + m_WindowWidth)
					ImGui::SetScrollX(ImGui::GetScrollX() + 1.0f);
				else if (mx > m_FindOrigin.x && mx < m_FindOrigin.x + m_TextStart + 50)
					ImGui::SetScrollX(ImGui::GetScrollX() - 1.0f);
			}

		}
	}
}

bool ImTextEdit::HasBreakpoint(int line)
{
	for (const auto& bkpt : m_Breakpoints)
		if (bkpt.Line == line)
			return true;

	return false;
}

void ImTextEdit::AddBreakpoint(int line, bool useCondition, std::string condition, bool enabled)
{
	RemoveBreakpoint(line);

	Breakpoint bkpt;
	bkpt.Line = line;
	bkpt.Condition = condition;
	bkpt.Enabled = enabled;
	bkpt.UseCondition = useCondition;

	if (OnBreakpointUpdate)
		OnBreakpointUpdate(this, line, useCondition, condition, enabled);

	m_Breakpoints.push_back(bkpt);
}

void ImTextEdit::RemoveBreakpoint(int line)
{
	for (int i = 0; i < m_Breakpoints.size(); i++)
	{
		if (m_Breakpoints[i].Line == line)
		{
			m_Breakpoints.erase(m_Breakpoints.begin() + i);
			break;
		}
	}

	if (OnBreakpointRemove)
		OnBreakpointRemove(this, line);
}

void ImTextEdit::SetBreakpointEnabled(int line, bool enable)
{
	for (int i = 0; i < m_Breakpoints.size(); i++)
	{
		if (m_Breakpoints[i].Line == line)
		{
			m_Breakpoints[i].Enabled = enable;
		
			if (OnBreakpointUpdate)
				OnBreakpointUpdate(this, line, m_Breakpoints[i].UseCondition, m_Breakpoints[i].Condition, enable);
			
			break;
		}
	}
}

ImTextEdit::Breakpoint& ImTextEdit::GetBreakpoint(int line)
{
	for (int i = 0; i < m_Breakpoints.size(); i++)
	{
		if (m_Breakpoints[i].Line == line)
			return m_Breakpoints[i];
	}
}

void ImTextEdit::RenderInternal(const char* aTitle)
{
	/* Compute m_CharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
	const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
	m_CharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * m_LineSpacing);

	/* Update palette with the current alpha from style */
	for (int i = 0; i < (int)PaletteIndex::Max; ++i)
	{
		auto color = ImGui::ColorConvertU32ToFloat4(m_PaletteBase[i]);
		color.w *= ImGui::GetStyle().Alpha;
		m_Palette[i] = ImGui::ColorConvertFloat4ToU32(color);
	}

	assert(m_LineBuffer.empty());
	m_Focused = ImGui::IsWindowFocused() || m_FindFocused || m_ReplaceFocused;

	auto contentSize = ImGui::GetWindowContentRegionMax();
	auto drawList = ImGui::GetWindowDrawList();
	float longest(m_TextStart);

	if (m_ScrollToTop)
	{
		m_ScrollToTop = false;
		ImGui::SetScrollY(0.f);
	}

	ImVec2 cursorScreenPos = m_UICursorPos = ImGui::GetCursorScreenPos();
	auto scrollX = ImGui::GetScrollX();
	auto scrollY = m_LastScroll = ImGui::GetScrollY();

	int pageSize = (int)floor((scrollY + contentSize.y) / m_CharAdvance.y);
	auto lineNo = (int)floor(scrollY / m_CharAdvance.y);
	auto globalLineMax = (int)m_Lines.size();
	auto lineMax = std::max<int>(0, std::min<int>((int)m_Lines.size() - 1, lineNo + pageSize));
	int totalLinesFolded = 0;

	// Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
	char buf[16];
	snprintf(buf, 16, " %3d ", globalLineMax);
	m_TextStart = (ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + m_LeftMargin) * m_Sidebar;

	// render
	GetPageSize();
	if (!m_Lines.empty())
	{
		float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
		
		// find bracket pairs to highlight
		bool highlightBrackets = false;
		Coordinates highlightBracketCoord = m_State.CursorPosition, highlightBracketCursor = m_State.CursorPosition;
		if (m_HighlightBrackets && m_State.SelectionStart == m_State.SelectionEnd)
		{
			if (m_State.CursorPosition.Line <= m_Lines.size())
			{
				int lineSize = m_Lines[m_State.CursorPosition.Line].size();
				Coordinates start = GetCorrectCursorPosition();
				start.Column = std::min<int>(start.Column, m_Lines[start.Line].size()-1);
				highlightBracketCursor = start;

				if (lineSize != 0 && start.Line >= 0 && start.Line < m_Lines.size() && start.Column >= 0 && start.Column < m_Lines[start.Line].size())
				{
					char c1 = m_Lines[start.Line][start.Column].Character;
					char c2 = m_Lines[start.Line][std::max<int>(0, start.Column - 1)].Character;
					int b1 = IsBracket(c1);
					int b2 = IsBracket(c2);

					if (b1 || b2)
					{
						char search = c1;
						int dir = b1;
						
						if (b2)
						{
							search = c2;
							dir = b2;
							start.Column = std::max<int>(0, start.Column - 1);
							highlightBracketCursor = start;
						}

						int weight = 0;

						// go forward
						if (dir == 1)
						{
							while (start.Line < m_Lines.size())
							{
								for (; start.Column < m_Lines[start.Line].size(); start.Column++)
								{
									char curChar = m_Lines[start.Line][start.Column].Character;
								
									if (curChar == search)
									{
										weight++;
									}
									else if (IsClosingBracket(search, curChar))
									{
										weight--;
									
										if (weight <= 0)
										{
											highlightBrackets = true;
											highlightBracketCoord = start;
											break;
										}
									}
								}
								
								if (highlightBrackets)
									break;

								start.Line++;
								start.Column = 0;
							}
						}
						else // Go backwards
						{
							while (start.Line >= 0)
							{
								for (; start.Column >= 0; start.Column--)
								{
									char curChar = m_Lines[start.Line][start.Column].Character;

									if (curChar == search)
									{
										weight++;
									}
									else if (IsOpeningBracket(search, curChar))
									{
										weight--;

										if (weight <= 0) {
											highlightBrackets = true;
											highlightBracketCoord = start;
											break;
										}
									}
								}

								if (highlightBrackets)
									break;

								start.Line--;

								if (start.Line >= 0)
									start.Column = m_Lines[start.Line].size() - 1;
							}
						}
					}
				}
			}
		}

		// fold info
		int hoverFoldWeight = 0;
		int linesFolded = 0;
		uint64_t curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		
		if (m_FoldEnabled)
		{
			if (curTime - m_FoldLastIteration > 3000)
			{
				// sort if needed
				if (!m_FoldSorted)
				{
					std::sort(m_FoldBegin.begin(), m_FoldBegin.end());
					std::sort(m_FoldEnd.begin(), m_FoldEnd.end());
					m_FoldSorted = true;
				}

				// resize if needed
				if (m_Fold.size() != m_FoldBegin.size())
				{
					m_Fold.resize(m_FoldBegin.size(), false);
					m_FoldConnection.resize(m_FoldBegin.size(), -1);
				}

				// reconnect every fold BEGIN with END (TODO: any better way to do this?)
				std::vector<bool> foldUsed(m_FoldEnd.size(), false);
				for (int i = m_FoldBegin.size() - 1; i >= 0; i--)
				{
					int j = m_FoldEnd.size() - 1;
					int lastUnused = j;

					for (; j >= 0; j--)
					{
						if (m_FoldEnd[j] < m_FoldBegin[i])
							break;

						if (!foldUsed[j])
							lastUnused = j;
					}

					if (lastUnused < m_FoldEnd.size())
					{
						foldUsed[lastUnused] = true;
						m_FoldConnection[i] = lastUnused;
					}
				}

				m_FoldLastIteration = curTime;
			}

			auto foldLineStart = 0;
			auto foldLineEnd = std::min<int>((int)m_Lines.size() - 1, lineNo);
			
			while (foldLineStart < m_Lines.size())
			{
				// check if line is folded
				for (int i = 0; i < m_FoldBegin.size(); i++)
				{
					if (m_FoldBegin[i].Line == foldLineStart)
					{
						if (i < m_Fold.size() && m_Fold[i])
						{
							int foldCon = m_FoldConnection[i];

							if (foldCon != -1 && foldCon < m_FoldEnd.size())
							{
								int diff = m_FoldEnd[foldCon].Line - m_FoldBegin[i].Line;

								if (foldLineStart < foldLineEnd)
								{
									linesFolded += diff;
									foldLineEnd = std::min<int>((int)m_Lines.size() - 1, foldLineEnd + diff);
								}

								totalLinesFolded += diff;
							}
							break;
						}
					}
				}

				foldLineStart++;
			}

			lineNo += linesFolded;
			lineMax = std::max<int>(0, std::min<int>((int)m_Lines.size() - 1, lineNo + pageSize));
		}

		// render
		while (lineNo <= lineMax)
		{
			ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + (lineNo - linesFolded) * m_CharAdvance.y);
			ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + m_TextStart, lineStartScreenPos.y);

			auto* line = &m_Lines[lineNo];
			longest = std::max(m_TextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);
			Coordinates lineStartCoord(lineNo, 0);
			Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

			// check if line is folded
			bool lineFolded = false;
			int lineNew = 0;
			Coordinates lineFoldStart, lineFoldEnd;
			int lineFoldStartCIndex = 0;

			if (m_FoldEnabled)
			{
				for (int i = 0; i < m_FoldBegin.size(); i++)
				{
					if (m_FoldBegin[i].Line == lineNo)
					{
						if (i < m_Fold.size())
						{
							lineFolded = m_Fold[i];
							lineFoldStart = m_FoldBegin[i];
							lineFoldStartCIndex = GetCharacterIndex(lineFoldStart);

							int foldCon = m_FoldConnection[i];

							if (lineFolded && foldCon != -1 && foldCon < m_FoldEnd.size())
								lineFoldEnd = m_FoldEnd[foldCon];
						}

						break;
					}
				}
			}

			// Draw selection for the current line
			float sstart = -1.0f;
			float ssend = -1.0f;
			assert(m_State.SelectionStart <= m_State.SelectionEnd);
			
			if (m_State.SelectionStart <= lineEndCoord)
				sstart = m_State.SelectionStart > lineStartCoord ? TextDistanceToLineStart(m_State.SelectionStart) : 0.0f;
			
			if (m_State.SelectionEnd > lineStartCoord)
				ssend = TextDistanceToLineStart(m_State.SelectionEnd < lineEndCoord ? m_State.SelectionEnd : lineEndCoord);
			
			if (m_State.SelectionEnd.Line > lineNo)
				ssend += m_CharAdvance.x;
			
			if (sstart != -1 && ssend != -1 && sstart < ssend)
			{
				ImVec2 vstart(lineStartScreenPos.x + m_TextStart + sstart, lineStartScreenPos.y);
				ImVec2 vend(lineStartScreenPos.x + m_TextStart + ssend, lineStartScreenPos.y + m_CharAdvance.y);
				drawList->AddRectFilled(vstart, vend, m_Palette[(int)PaletteIndex::Selection]);
			}

			// draw snippet stuff
			if (m_IsSnippet)
			{
				unsigned int oldColor = m_Palette[(int)PaletteIndex::Selection];
				unsigned int alpha = (oldColor & 0xFF000000) >> 25;
				unsigned int newColor = (oldColor & 0x00FFFFFF) | (alpha << 24);

				for (int i = 0; i < m_SnippetTagStart.size(); i++)
				{
					if (m_SnippetTagStart[i].Line == lineNo && m_SnippetTagHighlight[i])
					{
						float tstart = TextDistanceToLineStart(m_SnippetTagStart[i]);
						float tend = TextDistanceToLineStart(m_SnippetTagEnd[i]);

						ImVec2 vstart(lineStartScreenPos.x + m_TextStart + tstart, lineStartScreenPos.y);
						ImVec2 vend(lineStartScreenPos.x + m_TextStart + tend, lineStartScreenPos.y + m_CharAdvance.y);
						drawList->AddRectFilled(vstart, vend, newColor);
					}
				}
			}

			auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

			// Draw error markers
			auto errorIt = m_ErrorMarkers.find(lineNo + 1);

			if (errorIt != m_ErrorMarkers.end())
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + m_CharAdvance.y);
				drawList->AddRectFilled(start, end, m_Palette[(int)PaletteIndex::ErrorMarker]);

				if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end))
				{
					ImGui::BeginTooltip();
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(m_Palette[(int)PaletteIndex::ErrorMessage]));
					ImGui::Text("Error at line %d:", errorIt->first);
					ImGui::PopStyleColor();
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(m_Palette[(int)PaletteIndex::ErrorMessage]));
					ImGui::Text("%s", errorIt->second.c_str());
					ImGui::PopStyleColor();
					ImGui::EndTooltip();
				}
			}

			// Highlight the current line (where the cursor is)
			if (m_State.CursorPosition.Line == lineNo)
			{
				auto focused = ImGui::IsWindowFocused();

				// Highlight the current line (where the cursor is)
				if (m_HighlightLine && !HasSelection())
				{
					auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + m_CharAdvance.y + 2.0f);
					drawList->AddRectFilled(start, end, m_Palette[(int)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
					drawList->AddRect(start, end, m_Palette[(int)PaletteIndex::CurrentLineEdge], 1.0f);
				}

				// Render the cursor
				if (focused)
				{
					auto elapsed = curTime - m_StartTime;
					if (elapsed > 400)
					{
						float width = 1.0f;
						auto cindex = GetCharacterIndex(m_State.CursorPosition);
						float cx = TextDistanceToLineStart(m_State.CursorPosition);

						if (m_Overwrite && cindex < (int)line->size())
						{
							auto c = (*line)[cindex].Character;

							if (c == '\t')
							{
								auto x = (1.0f + std::floor((1.0f + cx) / (float(m_TabSize) * spaceSize))) * (float(m_TabSize) * spaceSize);
								width = x - cx;
							}
							else
							{
								char buf2[2];
								buf2[0] = (*line)[cindex].Character;
								buf2[1] = '\0';
								width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
							}
						}

						ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
						ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + m_CharAdvance.y);
						drawList->AddRectFilled(cstart, cend, m_Palette[(int)PaletteIndex::Cursor]);

						if (elapsed > 800)
							m_StartTime = curTime;
					}
				}
			}

			// highlight the user defined lines
			if (m_HighlightLine)
			{
				if (std::find(m_HighlightedLines.begin(), m_HighlightedLines.end(), lineNo) != m_HighlightedLines.end())
				{
					auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + m_CharAdvance.y);
					drawList->AddRectFilled(start, end, m_Palette[(int)(PaletteIndex::CurrentLineFill)]);
				}
			}

			// text
			auto prevColor = line->empty() ? m_Palette[(int)PaletteIndex::Default] : GetGlyphColor((*line)[0]);
			ImVec2 bufferOffset;
			
			for (int i = 0; i < line->size();)
			{
				auto& glyph = (*line)[i];
				auto color = GetGlyphColor(glyph);

				if ((color != prevColor || glyph.Character == '\t' || glyph.Character == ' ') && !m_LineBuffer.empty())
				{
					const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
					drawList->AddText(newOffset, prevColor, m_LineBuffer.c_str());
					auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, m_LineBuffer.c_str(), nullptr, nullptr);
					bufferOffset.x += textSize.x;
					m_LineBuffer.clear();
				}

				prevColor = color;

				// highlight brackets
				if (highlightBrackets)
				{
					if ((lineNo == highlightBracketCoord.Line && i == highlightBracketCoord.Column) || (lineNo == highlightBracketCursor.Line && i == highlightBracketCursor.Column))
					{
						const ImVec2 p1(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
						const ImVec2 p2(textScreenPos.x + bufferOffset.x + ImGui::GetFont()->GetCharAdvance(m_Lines[highlightBracketCoord.Line][highlightBracketCoord.Column].Character), textScreenPos.y + bufferOffset.y + ImGui::GetFontSize());
						drawList->AddRectFilled(p1, p2, m_Palette[(int)PaletteIndex::Selection]);
					}
				}

				// tab, space, etc...
				if (glyph.Character == '\t')
				{
					auto oldX = bufferOffset.x;
					bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(m_TabSize) * spaceSize))) * (float(m_TabSize) * spaceSize);
					++i;

					if (m_ShowWhitespaces)
					{
						const auto s = ImGui::GetFontSize();
						const auto x1 = textScreenPos.x + oldX + 1.0f;
						const auto x2 = textScreenPos.x + bufferOffset.x - 1.0f;
						const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
						const ImVec2 p1(x1, y);
						const ImVec2 p2(x2, y);
						const ImVec2 p3(x2 - s * 0.2f, y - s * 0.2f);
						const ImVec2 p4(x2 - s * 0.2f, y + s * 0.2f);
						drawList->AddLine(p1, p2, 0x90909090);
						drawList->AddLine(p2, p3, 0x90909090);
						drawList->AddLine(p2, p4, 0x90909090);
					}
				}
				else if (glyph.Character == ' ')
				{
					if (m_ShowWhitespaces)
					{
						const auto s = ImGui::GetFontSize();
						const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
						const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
						drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
					}

					bufferOffset.x += spaceSize;
					i++;
				}
				else
				{
					auto l = UTF8CharLength(glyph.Character);
					
					while (l-- > 0)
						m_LineBuffer.push_back((*line)[i++].Character);
				}

				// skip if folded
				if (lineFolded && lineFoldStartCIndex == i - 1)
				{
					i = GetCharacterIndex(lineFoldEnd);
					lineNew = lineFoldEnd.Line;
					line = &m_Lines[lineNew];
					lineFolded = false;

					if (!m_LineBuffer.empty())
					{
						// render the actual text
						const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
						auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, m_LineBuffer.c_str(), nullptr, nullptr);
						drawList->AddText(newOffset, prevColor, m_LineBuffer.c_str());
						m_LineBuffer.clear();
						bufferOffset.x += textSize.x;

						// render the [...] when folded
						const ImVec2 offsetFoldBox(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
						drawList->AddText(offsetFoldBox, m_Palette[(int)PaletteIndex::Default], " ... ");
						textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ... ", nullptr, nullptr);
						drawList->AddRect(ImVec2(textScreenPos.x + bufferOffset.x + m_CharAdvance.x / 2.0f, textScreenPos.y + bufferOffset.y),
										  ImVec2(textScreenPos.x + bufferOffset.x + textSize.x - m_CharAdvance.x / 2.0f, textScreenPos.y + bufferOffset.y + m_CharAdvance.y),
										  m_Palette[(int)PaletteIndex::Default]);
						bufferOffset.x += textSize.x;
					}
				}

			}

			if (!m_LineBuffer.empty())
			{
				const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
				drawList->AddText(newOffset, prevColor, m_LineBuffer.c_str());
				m_LineBuffer.clear();
			}

			// side bar
			if (m_Sidebar)
			{
				drawList->AddRectFilled(ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y), ImVec2(lineStartScreenPos.x + scrollX + m_TextStart - 5.0f, lineStartScreenPos.y + m_CharAdvance.y), ImGui::GetColorU32(ImGuiCol_WindowBg));

				// Draw breakpoints
				if (HasBreakpoint(lineNo + 1) != 0)
				{
					float radius = ImGui::GetFontSize() * 1.0f / 3.0f;
					float startX = lineStartScreenPos.x + scrollX + radius + 2.0f;
					float startY = lineStartScreenPos.y + radius + 4.0f;

					drawList->AddCircle(ImVec2(startX, startY), radius + 1, m_Palette[(int)PaletteIndex::BreakpointOutline]);
					drawList->AddCircleFilled(ImVec2(startX, startY), radius, m_Palette[(int)PaletteIndex::Breakpoint]);

					Breakpoint bkpt = GetBreakpoint(lineNo + 1);

					if (!bkpt.Enabled)
					{
						drawList->AddCircleFilled(ImVec2(startX, startY), radius - 1, m_Palette[(int)PaletteIndex::BreakpointDisabled]);
					}
					else
					{
						if (bkpt.UseCondition)
							drawList->AddRectFilled(ImVec2(startX - radius + 3, startY - radius / 4), ImVec2(startX + radius - 3, startY + radius / 4), m_Palette[(int)PaletteIndex::BreakpointOutline]);
					}
				}

				// Draw current line indicator
				if (lineNo + 1 == m_DebugCurrentLine)
				{
					float radius = ImGui::GetFontSize() * 1.0f / 3.0f;
					float startX = lineStartScreenPos.x + scrollX + radius + 2.0f;
					float startY = lineStartScreenPos.y + 4.0f;

					// outline
					drawList->AddRect(ImVec2(startX - radius, startY + radius / 2), ImVec2(startX, startY + radius * 3.0f / 2.0f),
						m_Palette[(int)PaletteIndex::CurrentLineIndicatorOutline]);
					drawList->AddTriangle(ImVec2(startX - 1, startY - 2), ImVec2(startX - 1, startY + radius * 2 + 1), ImVec2(startX + radius, startY + radius),
						m_Palette[(int)PaletteIndex::CurrentLineIndicatorOutline]);

					// fill
					drawList->AddRectFilled(ImVec2(startX - radius + 1, startY + 1 + radius / 2), ImVec2(startX + 1, startY - 1 + radius * 3.0f / 2.0f),
						m_Palette[(int)PaletteIndex::CurrentLineIndicator]);
					drawList->AddTriangleFilled(ImVec2(startX, startY + 1), ImVec2(startX, startY - 1 + radius * 2), ImVec2(startX - 1 + radius, startY + radius),
						m_Palette[(int)PaletteIndex::CurrentLineIndicator]);
				}

				// Draw line number (right aligned)
				if (m_ShowLineNumbers)
				{
					snprintf(buf, 16, "%3d  ", lineNo + 1);

					auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
					drawList->AddText(ImVec2(lineStartScreenPos.x + scrollX + m_TextStart - lineNoWidth, lineStartScreenPos.y), m_Palette[(int)PaletteIndex::LineNumber], buf);
				}

				// fold +/- icon
				if (m_FoldEnabled)
				{
					int foldID = 0;
					int foldWeight = 0;
					bool hasFold = false;
					bool hasFoldEnd = false;
					bool isFolded = false;
					float foldBtnSize = spaceSize;
					float foldStartX = lineStartScreenPos.x + scrollX + m_TextStart - spaceSize * 2.0f + 4;
					float foldStartY = lineStartScreenPos.y + (ImGui::GetFontSize() - foldBtnSize) / 2.0f;

					// calculate current weight + find if here ends or starts another "fold"
					for (int i = 0; i < m_FoldBegin.size(); i++)
					{
						if (m_FoldBegin[i].Line == lineNo)
						{
							hasFold = true;
							foldID = i;
							
							if (i < m_Fold.size())
								isFolded = m_Fold[i];
							
							break;
						}
						else if (m_FoldBegin[i].Line < lineNo)
						{
							foldWeight++;
						}
					}

					for (int i = 0; i < m_FoldEnd.size(); i++)
					{
						if (m_FoldEnd[i].Line == lineNo)
						{
							hasFoldEnd = true;
							break;
						}
						else if (m_FoldEnd[i].Line < lineNo)
						{
							foldWeight--;
						}
					}

					bool isHovered = (hoverFoldWeight && foldWeight >= hoverFoldWeight);

					// line
					if (foldWeight > 0 && !hasFold)
					{
						ImVec2 p1(foldStartX + foldBtnSize / 2, lineStartScreenPos.y);
						ImVec2 p2(p1.x, p1.y + m_CharAdvance.y + 1.0f);

						drawList->AddLine(p1, p2, m_Palette[(int)PaletteIndex::Default]);
					}

					if (hasFold) // render the +/- box
					{
						ImVec2 fmin(foldStartX, foldStartY + 2);
						ImVec2 fmax(fmin.x + foldBtnSize, fmin.y + foldBtnSize);

						// line up
						if (foldWeight > 0)
						{
							ImVec2 p1(foldStartX + foldBtnSize / 2, lineStartScreenPos.y);
							ImVec2 p2(p1.x, fmin.y);
							drawList->AddLine(p1, p2, m_Palette[(int)PaletteIndex::Default]);
						}

						// fold button
						foldWeight++;
						drawList->AddRect(fmin, fmax, m_Palette[(int)PaletteIndex::Default]);

						// check if mouse is over this fold button
						ImVec2 mpos = ImGui::GetMousePos();
						if (mpos.x >= fmin.x && mpos.x <= fmax.x && mpos.y >= fmin.y && mpos.y <= fmax.y)
						{
							isHovered = true;
							hoverFoldWeight = foldWeight;

							if (isFolded)
								hoverFoldWeight = 0;

							ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
							{
								if (foldID < m_Fold.size())
								{
									isFolded = !isFolded;
									m_Fold[foldID] = isFolded;
								}
							}
						}

						// minus
						drawList->AddLine(ImVec2(fmin.x + 3, (fmin.y + fmax.y) / 2.0f), ImVec2(fmax.x - 4, (fmin.y + fmax.y) / 2.0f), m_Palette[(int)PaletteIndex::Default]);

						// plus
						if (isFolded)
							drawList->AddLine(ImVec2((fmin.x + fmax.x) / 2.0f, fmin.y + 3), ImVec2((fmin.x + fmax.x) / 2.0f, fmax.y - 4), m_Palette[(int)PaletteIndex::Default]);

						// line down
						if (!isFolded || foldWeight > 1)
						{
							float lineContinueY = (lineStartScreenPos.y + m_CharAdvance.y) - fmax.y;
							ImVec2 p1(foldStartX + foldBtnSize / 2, fmax.y);
							ImVec2 p2(p1.x, fmax.y + lineContinueY);
							drawList->AddLine(p1, p2, m_Palette[(int)PaletteIndex::Default]);
						}
					}
					else if (hasFoldEnd) // horizontal line
					{
						foldWeight--;
						if (hoverFoldWeight && foldWeight < hoverFoldWeight)
							hoverFoldWeight = 0;

						ImVec2 p1(foldStartX + foldBtnSize / 2, lineStartScreenPos.y + m_CharAdvance.y - 1.0f);
						ImVec2 p2(p1.x + m_CharAdvance.x / 2.0f, p1.y);
						drawList->AddLine(p1, p2, m_Palette[(int)PaletteIndex::Default]);
					}

					// hover background
					if (isHovered)
					{
						// sidebar bg
						ImVec2 pmin(foldStartX - 4, lineStartScreenPos.y);
						ImVec2 pmax(pmin.x + foldBtnSize + 8, pmin.y + m_CharAdvance.y);
						drawList->AddRectFilled(pmin, pmax, 0x40000000 | (0x00FFFFFF & m_Palette[(int)PaletteIndex::Default]));

						// line bg
						pmin.x = pmax.x;
						pmax.x = lineStartScreenPos.x + scrollX + m_TextStart + contentSize.x;
						drawList->AddRectFilled(pmin, pmax, 0x20000000 | (0x00FFFFFF & m_Palette[(int)PaletteIndex::Default]));
					}
				}
			}

			if (lineNew)
			{
				linesFolded += lineNew - lineNo;
				lineMax = std::min<int>((int)m_Lines.size() - 1, lineMax + lineNew - lineNo);
				lineNo = lineNew;
			}

			++lineNo;
		}

		// Draw a tooltip on known identifiers/preprocessor symbols
		if (ImGui::IsMousePosValid() && (IsDebugging() || m_FuncTooltips || ImGui::GetIO().KeyCtrl))
		{
			Coordinates hoverPosition = MousePosToCoordinates(ImGui::GetMousePos());
			
			if (hoverPosition != m_LastHoverPosition)
			{
				m_LastHoverPosition = hoverPosition;
				m_LastHoverTime = std::chrono::steady_clock::now();
			}
			
			td_Char hoverChar = 0;

			if (hoverPosition.Line < m_Lines.size() && hoverPosition.Column < m_Lines[hoverPosition.Line].size())
				hoverChar = m_Lines[hoverPosition.Line][hoverPosition.Column].Character;

			double hoverTime = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - m_LastHoverTime).count();
			
			if (hoverTime > 0.5 && (hoverChar == '(' || hoverChar == ')') && IsDebugging())
			{
				std::string expr = "";

				int colStart = 0, rowStart = hoverPosition.Line;
				int bracketMatch = 0;

				if (hoverChar == ')')
				{
					int colIndex = hoverPosition.Column;

					for (; rowStart >= 0; rowStart--)
					{
						for (int i = colIndex; i >= 0; i--)
						{
							char curChar = m_Lines[rowStart][i].Character;

							if (curChar == '(')
								bracketMatch++;
							else if (curChar == ')')
								bracketMatch--;

							if (!isspace(curChar) || curChar == ' ')
								expr += curChar;

							if (bracketMatch == 0)
							{
								colStart = i - 1;
								break;
							}
						}

						if (bracketMatch == 0)
							break;

						if (rowStart != 0)
							colIndex = m_Lines[rowStart - 1].size() - 1;
					}
					std::reverse(expr.begin(), expr.end());

					if (rowStart <= 0)
						colStart = -1;

				}
				else if (hoverChar == '(')
				{
					int colIndex = hoverPosition.Column;
					colStart = hoverPosition.Column - 1;

					for (int j = rowStart; j < m_Lines.size(); j++)
					{
						for (int i = colIndex; i < m_Lines[j].size(); i++)
						{
							char curChar = m_Lines[j][i].Character;

							if (curChar == '(')
								bracketMatch++;
							else if (curChar == ')')
								bracketMatch--;

							if (!isspace(curChar) || curChar == ' ')
								expr += curChar;

							if (bracketMatch == 0)
								break;
						}

						if (bracketMatch == 0)
							break;
						if (j != 0)
							colIndex = 0;
					}

					if (rowStart >= m_Lines.size())
						colStart = -1;
				}

				while (colStart >= 0 && isalnum(m_Lines[rowStart][colStart].Character))
				{
					expr.insert(expr.begin(), m_Lines[rowStart][colStart].Character);
					colStart--;
				}

				if (OnExpressionHover && HasExpressionHover)
				{
					if (HasExpressionHover(this, expr))
					{
						ImGui::BeginTooltip();
						OnExpressionHover(this, expr);
						ImGui::EndTooltip();
					}
				}
			}
			else if (hoverTime > 0.2)
			{
				Coordinates wordCoords = ScreenPosToCoordinates(ImGui::GetMousePos());
				auto id = GetWordAt(wordCoords);
				bool isCtrlDown = ImGui::GetIO().KeyCtrl;

				if (!id.empty())
				{
					// function/value tooltips
					if (!isCtrlDown)
					{
						auto it = m_LanguageDefinition.Identifiers.find(id);

						if (it != m_LanguageDefinition.Identifiers.end() && m_FuncTooltips)
						{
							ImGui::BeginTooltip();
							ImGui::TextUnformatted(it->second.Declaration.c_str());
							ImGui::EndTooltip();
						}
						else
						{
							auto pi = m_LanguageDefinition.PreprocIdentifiers.find(id);

							if (pi != m_LanguageDefinition.PreprocIdentifiers.end() && m_FuncTooltips)
							{
								ImGui::BeginTooltip();
								ImGui::TextUnformatted(pi->second.Declaration.c_str());
								ImGui::EndTooltip();
							}
							else if (IsDebugging() && OnIdentifierHover && HasIdentifierHover)
							{
								if (HasIdentifierHover(this, id))
								{
									ImGui::BeginTooltip();
									OnIdentifierHover(this, id);
									ImGui::EndTooltip();
								}
							}
						}
					}
					else // CTRL + click functionality
					{
						bool hasUnderline = false;
						bool isHeader = false;
						bool isFindFirst = false;
						Coordinates findStart(0, 0);
						std::string header = "";
						Coordinates wordStart = FindWordStart(wordCoords);
						Coordinates wordEnd = FindWordEnd(wordCoords);

						// check if header
						if (wordStart.Line >= 0 && wordStart.Line < m_Lines.size())
						{
							// get the contents of the line in std::string
							const auto& line = m_Lines[wordStart.Line];
							std::string text;
							text.resize(line.size());

							for (size_t i = 0; i < line.size(); ++i)
								text[i] = line[i].Character;

							// find #include
							size_t includeLoc = text.find("#include");
							size_t includeStart = std::string::npos, includeEnd = std::string::npos;
							
							if (text.size() > 0 && includeLoc != std::string::npos)
							{
								for (size_t f = includeLoc; f < text.size(); f++)
								{
									if (text[f] == '<' || text[f] == '\"')
									{
										includeStart = f + 1;
									}
									else if (text[f] == '>' || text[f] == '\"')
									{
										includeEnd = f - 1;
										break;
									}
								}
							}

							if (includeStart != std::string::npos && includeEnd != std::string::npos)
							{
								hasUnderline = true;
								isHeader = true;
								
								header = text.substr(includeStart, includeEnd-includeStart + 1);
								wordStart.Column = 0;
								wordEnd.Column = GetCharacterColumn(wordEnd.Line, line.size());
							}
						}

						// check if mul, sin, cos, etc...
						if (!hasUnderline && m_LanguageDefinition.Identifiers.find(id) != m_LanguageDefinition.Identifiers.end())
							hasUnderline = true;

						// draw the underline
						if (hasUnderline)
						{
							ImVec2 ulinePos = ImVec2(cursorScreenPos.x + m_TextStart, cursorScreenPos.y + wordStart.Line * m_CharAdvance.y);
							drawList->AddLine(ImVec2(ulinePos.x + wordStart.Column * m_CharAdvance.x, ulinePos.y + m_CharAdvance.y),
								ImVec2(ulinePos.x + wordEnd.Column * m_CharAdvance.x, ulinePos.y + m_CharAdvance.y),
								ImGui::GetColorU32(ImGuiCol_HeaderHovered));
							ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
							{
								if (isHeader)
								{
									if (RequestOpen)
										RequestOpen(this, this->m_Path, header);
								}
								else
								{
									// go to type definition
									if (isFindFirst)
									{
										if (!ImGui::GetIO().KeyAlt)
										{
											Coordinates userTypeLoc = FindFirst(id, findStart);
										
											if (userTypeLoc.Line >= 0 && userTypeLoc.Line < m_Lines.size())
											{
												m_State.CursorPosition = userTypeLoc;
												SetSelectionStart(userTypeLoc);
												SetSelectionEnd(FindWordEnd(userTypeLoc));
												EnsureCursorVisible();
											}
										}
										else
										{
											if (OnCtrlAltClick) 
												this->OnCtrlAltClick(this, id, wordCoords);
										}
									}

									// TODO: identifier documentation
								}
							}
						}
					}
				}
			}
		}
	}

	// function tooltip
	if (m_FunctionDeclarationTooltip)
	{
		const float ttWidth = 350, ttHeight = 50;
		ImVec2 ttPos = CoordinatesToScreenPos(m_FunctionDeclarationCoord);
		ttPos.y += m_CharAdvance.y;
		ttPos.x += ImGui::GetScrollX();

		drawList->AddRectFilled(ttPos, ImVec2(ttPos.x + UICalculateSize(ttWidth), ttPos.y + UICalculateSize(ttHeight)), ImGui::GetColorU32(ImGuiCol_FrameBg));

		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImGui::SetNextWindowPos(ttPos, ImGuiCond_Always);
		ImGui::BeginChild("##texteditor_functooltip", ImVec2(UICalculateSize(ttWidth), UICalculateSize(ttHeight)), true);

		ImGui::TextWrapped("%s", m_FunctionDeclaration.c_str());

		ImGui::EndChild();

		ImGui::PushFont(font);

		ImGui::SetWindowFocus();

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
			m_FunctionDeclarationTooltip = false;
	}

	// suggestions window
	if (m_ACOpened)
	{
		auto acCoord = FindWordStart(m_ACPosition);
		ImVec2 acPos = CoordinatesToScreenPos(acCoord);
		acPos.y += m_CharAdvance.y;
		acPos.x += ImGui::GetScrollX();

		drawList->AddRectFilled(acPos, ImVec2(acPos.x + UICalculateSize(150), acPos.y + UICalculateSize(100)), ImGui::GetColorU32(ImGuiCol_FrameBg));

		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImGui::SetNextWindowPos(acPos, ImGuiCond_Always);
		ImGui::BeginChild("##texteditor_autocompl", ImVec2(UICalculateSize(150), UICalculateSize(100)), true);

		for (int i = 0; i < m_ACSuggestions.size(); i++)
		{
			ImGui::Selectable(m_ACSuggestions[i].first.c_str(), i == m_ACIndex);
			
			if (i == m_ACIndex)
				ImGui::SetScrollHereY();
		}

		ImGui::EndChild();

		ImGui::PushFont(font);

		ImGui::SetWindowFocus();

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
		{
			m_ACOpened = false;
			m_ACObject = "";
		}
	}

	ImGui::Dummy(ImVec2(longest + EditorCalculateSize(100), (m_Lines.size() - totalLinesFolded) * m_CharAdvance.y));

	if (m_DebugCurrentLineUpdated)
	{
		float scrollX = ImGui::GetScrollX();
		float scrollY = ImGui::GetScrollY();

		auto height = ImGui::GetWindowHeight();
		auto width = ImGui::GetWindowWidth();

		auto top = 1 + (int)ceil(scrollY / m_CharAdvance.y);
		auto bottom = (int)ceil((scrollY + height) / m_CharAdvance.y);

		auto left = (int)ceil(scrollX / m_CharAdvance.x);
		auto right = (int)ceil((scrollX + width) / m_CharAdvance.x);

		ImTextEdit::Coordinates pos(m_DebugCurrentLine, 0);

		if (pos.Line < top)
			ImGui::SetScrollY(std::max(0.0f, (pos.Line - 1) * m_CharAdvance.y));

		if (pos.Line > bottom - 4)
			ImGui::SetScrollY(std::max(0.0f, (pos.Line + 4) * m_CharAdvance.y - height));

		m_DebugCurrentLineUpdated = false;
	}

	if (m_ScrollToCursor)
	{
		EnsureCursorVisible();
		ImGui::SetWindowFocus();
		m_ScrollToCursor = false;
	}

	
	// hacky way to get the bg working
	if (m_FindOpened)
	{
		ImVec2 findPos = ImVec2(m_UICursorPos.x + scrollX + m_WindowWidth - UICalculateSize(250), m_UICursorPos.y + ImGui::GetScrollY() + UICalculateSize(50) * (IsDebugging() && m_DebugBar));
		drawList->AddRectFilled(findPos, ImVec2(findPos.x + UICalculateSize(220), findPos.y + UICalculateSize(m_ReplaceOpened ? 90 : 40)), ImGui::GetColorU32(ImGuiCol_WindowBg));
	}

	if (IsDebugging() && m_DebugBar)
	{
		ImVec2 dbgPos = ImVec2(m_UICursorPos.x + scrollX + m_WindowWidth / 2 - m_DebugBarWidth / 2, m_UICursorPos.y + ImGui::GetScrollY());
		drawList->AddRectFilled(dbgPos, ImVec2(dbgPos.x + m_DebugBarWidth, dbgPos.y + m_DebugBarHeight), ImGui::GetColorU32(ImGuiCol_FrameBg));
	}
}

void ImTextEdit::OpenFunctionDeclarationTooltip(const std::string& obj, ImTextEdit::Coordinates coord)
{
	if (m_ACFunctions.count(obj)) {
		m_FunctionDeclarationTooltip = true;
		m_FunctionDeclarationCoord = FindWordStart(coord);
		m_FunctionDeclaration = BuildFunctionDef(obj, m_LanguageDefinition.Name);
	}
}

std::string ImTextEdit::BuildFunctionDef(const std::string& func, const std::string& lang)
{
	if (m_ACFunctions.count(func) == 0)
		return "";

	const auto& funcDef = m_ACFunctions[func];

	std::string ret = BuildVariableType(funcDef.ReturnType, lang) + " " + func + "(";

	for (size_t i = 0; i < funcDef.Arguments.size(); i++) {
		ret += BuildVariableType(funcDef.Arguments[i], lang) + " " + funcDef.Arguments[i].Name;

		if (i != funcDef.Arguments.size() - 1)
			ret += ", ";
	}

	return ret + ")";
}
std::string ImTextEdit::BuildVariableType(const ed::SPIRVParser::Variable& var, const std::string& lang)
{
	switch (var.Type) {
	case ed::SPIRVParser::ValueType::Bool:
		return "bool";

	case ed::SPIRVParser::ValueType::Float:
		return "float";

	case ed::SPIRVParser::ValueType::Int:
		return "int";

	case ed::SPIRVParser::ValueType::Void:
		return "void";

	case ed::SPIRVParser::ValueType::Struct:
		return var.TypeName;

	case ed::SPIRVParser::ValueType::Vector: {
		std::string count = std::string(1, var.TypeComponentCount + '0');
		if (lang == "HLSL") {
			switch (var.BaseType) {
			case ed::SPIRVParser::ValueType::Bool:
				return "bool" + count;

			case ed::SPIRVParser::ValueType::Float:
				return "float" + count;

			case ed::SPIRVParser::ValueType::Int:
				return "int" + count;
			}
		}
		else {
			switch (var.BaseType) {
			case ed::SPIRVParser::ValueType::Bool:
				return "bvec" + count;

			case ed::SPIRVParser::ValueType::Float:
				return "vec" + count;

			case ed::SPIRVParser::ValueType::Int:
				return "ivec" + count;
			}
		}
	} break;

	case ed::SPIRVParser::ValueType::Matrix: {
		std::string count = std::string(1, var.TypeComponentCount + '0');
		if (lang == "HLSL") {
			return "float" + count + "x" + count;
		}
		else {
			return "mat" + count;
		}
	} break;
	}

	return "";
}

void ImTextEdit::RemoveFolds(const Coordinates& aStart, const Coordinates& aEnd)
{
	RemoveFolds(m_FoldBegin, aStart, aEnd);
	RemoveFolds(m_FoldEnd, aStart, aEnd);
}
void ImTextEdit::RemoveFolds(std::vector<Coordinates>& folds, const Coordinates& aStart, const Coordinates& aEnd)
{
	bool deleteFullyLastLine = false;

	if (aEnd.Line >= m_Lines.size() || aEnd.Column >= 100000)
		deleteFullyLastLine = true;

	for (int i = 0; i < folds.size(); i++)
	{
		if (folds[i].Line >= aStart.Line && folds[i].Line <= aEnd.Line)
		{
			if (folds[i].Line == aStart.Line && aStart.Line != aEnd.Line)
			{
				if (folds[i].Column >= aStart.Column)
				{
					folds.erase(folds.begin() + i);
					m_FoldSorted = false;
					i--;
				}
			}
			else if (folds[i].Line == aEnd.Line)
			{
				if (folds[i].Column < aEnd.Column)
				{
					if (aEnd.Line != aStart.Line || folds[i].Column >= aStart.Column)
					{
						folds.erase(folds.begin() + i);
						m_FoldSorted = false;
						i--;
					}
				}
				else
				{
					if (aEnd.Line == aStart.Line)
					{
						folds[i].Column = std::max<int>(0, folds[i].Column - (aEnd.Column - aStart.Column));
					}
					else
					{
						// calculate new
						if (aStart.Line < m_Lines.size())
						{
							auto* line = &m_Lines[aStart.Line];
							int colOffset = 0;
							int chi = 0;
							bool skipped = false;
							int bracketEndChIndex = GetCharacterIndex(m_FoldEnd[i]);

							while (chi < (int)line->size() && (!skipped || (skipped && chi < bracketEndChIndex)))
							{
								auto c = (*line)[chi].Character;
								chi += UTF8CharLength(c);

								if (c == '\t')
									colOffset = (colOffset / m_TabSize) * m_TabSize + m_TabSize;
								else
									colOffset++;

								// go to the last line
								if (chi == line->size() && aEnd.Line < m_Lines.size() && !skipped)
								{
									chi = GetCharacterIndex(aEnd);
									line = &m_Lines[aEnd.Line];
									skipped = true;
								}
							}

							folds[i].Column = colOffset;
						}

						folds[i].Line -= (aEnd.Line - aStart.Line);
					}
				}
			}
			else
			{
				folds.erase(folds.begin() + i);
				m_FoldSorted = false;
				i--;
			}
		}
		else if (folds[i].Line > aEnd.Line)
		{
			folds[i].Line -= (aEnd.Line - aStart.Line) + deleteFullyLastLine;
		}
	}
}

std::string ImTextEdit::AutcompleteParse(const std::string& str, const Coordinates& start)
{
	const char* buffer = str.c_str();
	const char* tagPlaceholderStart = buffer;
	const char* tagStart = buffer;

	bool parsingTag = false;
	bool parsingTagPlaceholder = false;

	std::vector<int> tagIds, tagLocations, tagLengths;
	std::unordered_map<int, std::string> tagPlaceholders;

	m_SnippetTagStart.clear();
	m_SnippetTagEnd.clear();
	m_SnippetTagID.clear();
	m_SnippetTagHighlight.clear();

	Coordinates cursor = start, tagStartCoord, tagEndCoord;

	int tagId = -1;
	int modif = 0;

	while (*buffer != '\0')
	{
		if (*buffer == '{' && *(buffer + 1) == '$')
		{
			parsingTagPlaceholder = false;
			parsingTag = true;
			tagId = -1;
			tagStart = buffer;

			tagStartCoord = cursor;

			const char* skipBuffer = buffer;
			char** endLoc = const_cast<char**>(&buffer); // oops
			tagId = strtol(buffer + 2, endLoc, 10);

			cursor.Column += *endLoc - skipBuffer;

			if (*buffer == ':')
			{
				tagPlaceholderStart = buffer + 1;
				parsingTagPlaceholder = true;
			}
		}

		if (*buffer == '}' && parsingTag)
		{
			std::string tagPlaceholder = "";

			if (parsingTagPlaceholder)
				tagPlaceholder = std::string(tagPlaceholderStart, buffer - tagPlaceholderStart);

			tagIds.push_back(tagId);
			tagLocations.push_back(tagStart - str.c_str());
			tagLengths.push_back(buffer - tagStart + 1);
			
			if (!tagPlaceholder.empty() || tagPlaceholders.count(tagId) == 0)
			{
				if (tagPlaceholder.empty())
					tagPlaceholder = " ";

				tagStartCoord.Column = std::max<int>(0, tagStartCoord.Column - modif);
				tagEndCoord = tagStartCoord;
				tagEndCoord.Column += tagPlaceholder.size();

				m_SnippetTagStart.push_back(tagStartCoord);
				m_SnippetTagEnd.push_back(tagEndCoord);
				m_SnippetTagID.push_back(tagId);
				m_SnippetTagHighlight.push_back(true);

				tagPlaceholders[tagId] = tagPlaceholder;
			}
			else
			{
				tagStartCoord.Column = std::max<int>(0, tagStartCoord.Column - modif);
				tagEndCoord = tagStartCoord;
				tagEndCoord.Column += tagPlaceholders[tagId].size();

				m_SnippetTagStart.push_back(tagStartCoord);
				m_SnippetTagEnd.push_back(tagEndCoord);
				m_SnippetTagID.push_back(tagId);
				m_SnippetTagHighlight.push_back(false);
			}

			modif += (tagLengths.back() - tagPlaceholders[tagId].size());

			parsingTagPlaceholder = false;
			parsingTag = false;
			tagId = -1;
		}

		if (*buffer == '\n')
		{
			cursor.Line++;
			cursor.Column = 0;
			modif = 0;
		}
		else
		{
			cursor.Column++;
		}

		buffer++;
	}

	m_IsSnippet = !tagIds.empty();

	std::string ret = str;
	
	for (int i = tagLocations.size() - 1; i >= 0; i--)
	{
		ret.erase(tagLocations[i], tagLengths[i]);
		ret.insert(tagLocations[i], tagPlaceholders[tagIds[i]]);
	}

	return ret;
}
void ImTextEdit::AutocompleteSelect()
{
	UndoRecord undo;
	undo.Before = m_State;

	auto curCoord = GetCursorPosition();
	curCoord.Column = std::max<int>(curCoord.Column - 1, 0);

	auto acStart = FindWordStart(curCoord);
	auto acEnd = FindWordEnd(curCoord);

	if (!m_ACObject.empty())
		acStart = m_ACPosition;

	undo.AddedStart = acStart;
	int undoPopCount = std::max(0, acEnd.Column - acStart.Column) + 1;

	if (!m_ACObject.empty() && m_ACWord.empty())
		undoPopCount = 0;

	const auto& acEntry = m_ACSuggestions[m_ACIndex];

	std::string entryText = AutcompleteParse(acEntry.second, acStart);

	if (acStart.Column != acEnd.Column)
	{
		SetSelection(acStart, acEnd);
		Backspace();
	}
	
	AppendText(entryText, true);

	undo.Added = entryText;
	undo.AddedEnd = GetActualCursorCoordinates();

	if (m_IsSnippet && m_SnippetTagStart.size() > 0)
	{
		SetSelection(m_SnippetTagStart[0], m_SnippetTagEnd[0]);
		SetCursorPosition(m_SnippetTagEnd[0]);
		m_SnippetTagSelected = 0;
		m_SnippetTagLength = 0;
		m_SnippetTagPreviousLength = m_SnippetTagEnd[m_SnippetTagSelected].Column - m_SnippetTagStart[m_SnippetTagSelected].Column;
	}

	m_RequestAutocomplete = false;
	m_ACOpened = false;
	m_ACObject = "";

	undo.After = m_State;

	while (undoPopCount-- != 0)
	{
		m_UndoIndex--;
		m_UndoBuffer.pop_back();
	}
	
	AddUndo(undo);
}

void ImTextEdit::BuildMemberSuggestions(bool* keepACOpened)
{
	m_ACSuggestions.clear();

	auto curPos = GetCorrectCursorPosition();
	std::string obj = GetWordAt(curPos);

	ed::SPIRVParser::Variable* var = nullptr;

	for (auto& func : m_ACFunctions)
	{
		// suggest arguments and locals
		if (m_State.CursorPosition.Line >= func.second.LineStart - 2 && m_State.CursorPosition.Line <= func.second.LineEnd + 1)
		{
			// locals
			for (auto& loc : func.second.Locals)
			{
				if (strcmp(loc.Name.c_str(), obj.c_str()) == 0)
				{
					var = &loc;
					break;
				}
			}

			// arguments
			if (var == nullptr)
			{
				for (auto& arg : func.second.Arguments)
				{
					if (strcmp(arg.Name.c_str(), obj.c_str()) == 0)
					{
						var = &arg;
						break;
					}
				}
			}
		}
	}

	if (var == nullptr)
	{
		for (auto& uni : m_ACUniforms)
		{
			if (strcmp(uni.Name.c_str(), obj.c_str()) == 0)
			{
				var = &uni;
				break;
			}
		}
	}

	if (var == nullptr)
	{
		for (auto& glob : m_ACGlobals)
		{
			if (strcmp(glob.Name.c_str(), obj.c_str()) == 0)
			{
				var = &glob;
				break;
			}
		}
	}

	if (var != nullptr)
	{
		m_ACIndex = 0;
		m_ACSwitched = false;

		if (var->TypeName.size() > 0 && var->TypeName[0] != 0)
		{
			for (const auto& uType : m_ACUserTypes)
			{
				if (uType.first == var->TypeName)
				{
					for (const auto& uMember : uType.second)
						m_ACSuggestions.push_back(std::make_pair(uMember.Name, uMember.Name));
				}
			}

			if (m_ACSuggestions.size() > 0)
				m_ACObject = var->TypeName;
		}
	}

	if (m_ACSuggestions.size() > 0)
	{
		m_ACOpened = true;
		m_ACWord = "";

		if (keepACOpened != nullptr)
			*keepACOpened = true;

		Coordinates curCursor = GetCursorPosition();

		m_ACPosition = FindWordStart(curCursor);
	}
}
void ImTextEdit::BuildSuggestions(bool* keepACOpened)
{
	m_ACWord = GetWordUnderCursor();

	bool isValid = false;
	
	for (int i = 0; i < m_ACWord.size(); i++)
	{
		if ((m_ACWord[i] >= 'a' && m_ACWord[i] <= 'z') || (m_ACWord[i] >= 'A' && m_ACWord[i] <= 'Z'))
		{
			isValid = true;
			break;
		}
	}

	if (isValid)
	{
		m_ACSuggestions.clear();
		m_ACIndex = 0;
		m_ACSwitched = false;

		std::string acWord = m_ACWord;
		std::transform(acWord.begin(), acWord.end(), acWord.begin(), tolower);

		struct ACEntry
		{
			ACEntry(const std::string& str, const std::string& val, int loc)
			{
				DisplayString = str;
				Value = val;
				Location = loc;
			}

			std::string DisplayString;
			std::string Value;
			int Location;
		};

		std::vector<ACEntry> weights;

		if (m_ACObject.empty())
		{
			// get the words
			for (int i = 0; i < m_ACEntrySearch.size(); i++)
			{
				std::string lwrStr = m_ACEntrySearch[i];
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);

				if (loc != std::string::npos)
					weights.push_back(ACEntry(m_ACEntries[i].first, m_ACEntries[i].second, loc));
			}

			for (auto& func : m_ACFunctions)
			{
				std::string lwrStr = func.first;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				// suggest arguments and locals
				if (m_State.CursorPosition.Line >= func.second.LineStart - 2 && m_State.CursorPosition.Line <= func.second.LineEnd + 1)
				{
					// locals
					for (auto& loc : func.second.Locals)
					{
						std::string lwrLoc = loc.Name;
						std::transform(lwrLoc.begin(), lwrLoc.end(), lwrLoc.begin(), tolower);

						size_t location = lwrLoc.find(acWord);
						if (location != std::string::npos)
							weights.push_back(ACEntry(loc.Name, loc.Name, location));
					}

					// arguments
					for (auto& arg : func.second.Arguments)
					{
						std::string lwrLoc = arg.Name;
						std::transform(lwrLoc.begin(), lwrLoc.end(), lwrLoc.begin(), tolower);

						size_t loc = lwrLoc.find(acWord);
						if (loc != std::string::npos)
							weights.push_back(ACEntry(arg.Name, arg.Name, loc));
					}
				}

				size_t loc = lwrStr.find(acWord);

				if (loc != std::string::npos)
				{
					std::string val = func.first;
					if (m_CompleteBraces) val += "()";
					weights.push_back(ACEntry(func.first, val, loc));
				}
			}

			for (auto& uni : m_ACUniforms)
			{
				std::string lwrStr = uni.Name;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				
				if (loc != std::string::npos)
					weights.push_back(ACEntry(uni.Name, uni.Name, loc));
			}
			for (auto& glob : m_ACGlobals)
			{
				std::string lwrStr = glob.Name;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);

				if (loc != std::string::npos)
					weights.push_back(ACEntry(glob.Name, glob.Name, loc));
			}
			
			for (auto& utype : m_ACUserTypes)
			{
				std::string lwrStr = utype.first;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
			
				if (loc != std::string::npos)
					weights.push_back(ACEntry(utype.first, utype.first, loc));
			}

			for (auto& str : m_LanguageDefinition.Keywords)
			{
				std::string lwrStr = str;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				
				if (loc != std::string::npos)
					weights.push_back(ACEntry(str, str, loc));
			}

			for (auto& str : m_LanguageDefinition.Identifiers)
			{
				std::string lwrStr = str.first;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				
				if (loc != std::string::npos)
				{
					std::string val = str.first;
					
					if (m_CompleteBraces) val += "()";
					weights.push_back(ACEntry(str.first, val, loc));
				}
			}
		}
		else
		{
			for (const auto& uType : m_ACUserTypes)
			{
				if (uType.first == m_ACObject)
				{
					for (const auto& uMember : uType.second)
					{

						std::string lwrStr = uMember.Name;
						std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

						size_t loc = lwrStr.find(acWord);
						
						if (loc != std::string::npos)
							weights.push_back(ACEntry(uMember.Name, uMember.Name, loc));
					}
				}
			}
		}

		// build the actual list
		for (const auto& entry : weights)
		{
			if (entry.Location == 0)
				m_ACSuggestions.push_back(std::make_pair(entry.DisplayString, entry.Value));
		}

		for (const auto& entry : weights)
		{
			if (entry.Location != 0)
				m_ACSuggestions.push_back(std::make_pair(entry.DisplayString, entry.Value));
		}

		if (m_ACSuggestions.size() > 0)
		{
			m_ACOpened = true;

			if (keepACOpened != nullptr)
				*keepACOpened = true;

			Coordinates curCursor = GetCursorPosition();
			curCursor.Column--;

			m_ACPosition = FindWordStart(curCursor);
		}
	}
}

ImVec2 ImTextEdit::CoordinatesToScreenPos(const ImTextEdit::Coordinates& aPosition) const
{
	ImVec2 origin = m_UICursorPos;
	int dist = aPosition.Column;

	int retY = origin.y + aPosition.Line * m_CharAdvance.y;
	int retX = origin.x + GetTextStart() * m_CharAdvance.x + dist * m_CharAdvance.x - ImGui::GetScrollX();

	return ImVec2(retX, retY);
}

void ImTextEdit::Render(const char* aTitle, const ImVec2& aSize, bool aBorder)
{
	m_WithinRender = true;
	m_CursorPositionChanged = false;

	m_FindOrigin = ImGui::GetCursorScreenPos();
	float windowWidth = m_WindowWidth = ImGui::GetWindowWidth();
	
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(m_Palette[(int)PaletteIndex::Background]));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	
	if (!m_IgnoreImGuiChild)
		ImGui::BeginChild(aTitle, aSize, aBorder, (ImGuiWindowFlags_AlwaysHorizontalScrollbar * m_HorizontalScroll) | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoNav);
	
	if (m_HandleKeyboardInputs)
	{
		HandleKeyboardInputs();
		ImGui::PushAllowKeyboardFocus(true);
	}

	if (m_HandleMouseInputs)
		HandleMouseInputs();

	ColorizeInternal();
	RenderInternal(aTitle);

	// markers
	if (m_ScrollbarMarkers)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindowRead();
	
		if (window->ScrollbarY)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImRect scrollBarRect = ImGui::GetWindowScrollbarRect(window, ImGuiAxis_Y);
			ImGui::PushClipRect(scrollBarRect.Min, scrollBarRect.Max, false);
			int mSelectedLine = m_State.CursorPosition.Line;

			// current line marker
			if (mSelectedLine != 0)
			{
				float lineStartY = std::round(scrollBarRect.Min.y + (mSelectedLine - 0.5f) / m_Lines.size() * scrollBarRect.GetHeight());
				drawList->AddLine(ImVec2(scrollBarRect.Min.x, lineStartY), ImVec2(scrollBarRect.Max.x, lineStartY), (m_Palette[(int)PaletteIndex::Default] & 0x00FFFFFFu) | 0x83000000u, 3);
			}

			// changed lines marker
			for (int line : m_ChangedLines)
			{
				float lineStartY = std::round(scrollBarRect.Min.y + (float(line) - 0.5f) / m_Lines.size() * scrollBarRect.GetHeight());
				float lineEndY = std::round(scrollBarRect.Min.y + (float(line+1) - 0.5f) / m_Lines.size() * scrollBarRect.GetHeight());
				drawList->AddRectFilled(ImVec2(scrollBarRect.Min.x + scrollBarRect.GetWidth() * 0.6f, lineStartY), ImVec2(scrollBarRect.Min.x + scrollBarRect.GetWidth(), lineEndY), 0xFF8CE6F0);
			}

			// error markers
			for (auto& error : m_ErrorMarkers)
			{
				float lineStartY = std::round(scrollBarRect.Min.y + (float(error.first) - 0.5f) / m_Lines.size() * scrollBarRect.GetHeight());
				drawList->AddRectFilled(ImVec2(scrollBarRect.Min.x, lineStartY), ImVec2(scrollBarRect.Min.x + scrollBarRect.GetWidth() * 0.4f, lineStartY + 6.0f), m_Palette[(int)PaletteIndex::ErrorMarker]);
			}

			ImGui::PopClipRect();
		}
	}

	if (ImGui::IsMouseClicked(1))
	{
		m_RightClickPos = ImGui::GetMousePos();
		
		if (ImGui::IsWindowHovered())
			SetCursorPosition(ScreenPosToCoordinates(m_RightClickPos));
	}

	bool openBkptConditionWindow = false;

	if (ImGui::BeginPopupContextItem(("##edcontext" + std::string(aTitle)).c_str()))
	{
		if (m_RightClickPos.x - m_UICursorPos.x > ImGui::GetStyle().WindowPadding.x + s_DebugDataSpace)
		{
			if (ImGui::Selectable("Cut"))
				Cut();

			if (ImGui::Selectable("Copy"))
				Copy();

			if (ImGui::Selectable("Paste"))
				Paste();
		}
		else
		{
			int line = ScreenPosToCoordinates(m_RightClickPos).Line + 1;

			if (IsDebugging() && ImGui::Selectable("Jump") && OnDebuggerJump)
				OnDebuggerJump(this, line);

			if (ImGui::Selectable("Breakpoint"))
				AddBreakpoint(line);

			if (HasBreakpoint(line))
			{
				const auto& bkpt = GetBreakpoint(line);
				bool isEnabled = bkpt.Enabled;

				if (ImGui::Selectable("Condition"))
				{
					m_PopupCondition_Line = line;
					m_PopupCondition_Use = bkpt.UseCondition;
					memcpy(m_PopupCondition_Condition, bkpt.Condition.c_str(), bkpt.Condition.size());
					m_PopupCondition_Condition[std::min<size_t>(511, bkpt.Condition.size())] = 0;
					openBkptConditionWindow = true;
				}

				if (ImGui::Selectable(isEnabled ? "Disable" : "Enable"))
					SetBreakpointEnabled(line, !isEnabled);

				if (ImGui::Selectable("Delete"))
					RemoveBreakpoint(line);
			}
		}

		ImGui::EndPopup();
	}

	/* FIND TEXT WINDOW */
	if (m_FindOpened)
	{
		ImFont* cascadiaMono = new ImFont();
		ImFont* font = ImGui::GetFont();
		ImGui::PushFont(font);

		ImGui::SetNextWindowPos(ImVec2(m_FindOrigin.x + windowWidth - UICalculateSize(250), m_FindOrigin.y + UICalculateSize(50) * (IsDebugging() && m_DebugBar)), ImGuiCond_Always);
		ImGui::BeginChild(("##ted_findwnd" + std::string(aTitle)).c_str(), ImVec2(UICalculateSize(220), UICalculateSize(m_ReplaceOpened ? 90 : 40)), true, ImGuiWindowFlags_NoScrollbar);

		// check for findnext shortcut here...
		ShortcutID curActionID = ShortcutID::Count;
		ImGuiIO& io = ImGui::GetIO();
		auto shift = io.KeyShift;
		auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
		auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
		
		for (int i = 0; i < m_Shortcuts.size(); i++)
		{
			auto sct = m_Shortcuts[i];

			if (sct.Key1 == -1)
				continue;

			int sc1 = ::GetAsyncKeyState(sct.Key1) & 0x8000 ? sct.Key1 : -1;

			if (ImGui::IsKeyPressed(sc1) && ((sct.Key2 != -1 && ImGui::IsKeyPressed(sct.Key2))) || sct.Key2 == -1)
			{
				if (((sct.Ctrl == 0 && !ctrl) || (sct.Ctrl == 1 && ctrl) || (sct.Ctrl == 2)) &&		// ctrl check
					((sct.Alt == 0 && !alt) || (sct.Alt == 1 && alt) || (sct.Alt == 2)) &&			// alt check
					((sct.Shift == 0 && !shift) || (sct.Shift == 1 && shift) || (sct.Shift == 2)))  // shift check
				{
				
					curActionID = (ImTextEdit::ShortcutID)i;
				}
			}
		}

		m_FindNext = curActionID == ImTextEdit::ShortcutID::FindNext;

		if (m_FindJustOpened)
		{
			std::string txt = GetSelectedText();
			if (txt.size() > 0)
				strcpy(m_FindWord, txt.c_str());
		}

		ImGui::PushItemWidth(UICalculateSize(-45));

		if (ImGui::InputText(("##ted_findtextbox" + std::string(aTitle)).c_str(), m_FindWord, 256, ImGuiInputTextFlags_EnterReturnsTrue) || m_FindNext)
		{
			auto curPos = m_State.CursorPosition;
			size_t cindex = 0;

			for (size_t ln = 0; ln < curPos.Line; ln++)
				cindex += GetLineCharacterCount(ln) + 1;
			
			cindex += curPos.Column;

			std::string wordLower = m_FindWord;
			std::transform(wordLower.begin(), wordLower.end(), wordLower.begin(), ::tolower);

			std::string textSrc = GetText();
			std::transform(textSrc.begin(), textSrc.end(), textSrc.begin(), ::tolower);

			size_t textLoc = textSrc.find(wordLower, cindex);

			if (textLoc == std::string::npos)
				textLoc = textSrc.find(wordLower, 0);


			if (textLoc != std::string::npos)
			{
				curPos.Line = curPos.Column = 0;
				cindex = 0;

				for (size_t ln = 0; ln < m_Lines.size(); ln++)
				{
					int charCount = GetLineCharacterCount(ln) + 1;
				
					if (cindex + charCount > textLoc)
					{
						curPos.Line = ln;
						curPos.Column = textLoc - cindex;

						auto& line = m_Lines[curPos.Line];

						for (int i = 0; i < line.size(); i++)
						{
							if (line[i].Character == '\t')
								curPos.Column += (m_TabSize - 1);
						}

						break;
					}
					else // just keep adding
					{
						cindex += charCount;
					}
				}


				auto selStart = curPos, selEnd = curPos;
				selEnd.Column += strlen(m_FindWord);
				SetSelection(curPos, selEnd);
				SetCursorPosition(selEnd);
				m_ScrollToCursor = true;

				if (!m_FindNext)
					ImGui::SetKeyboardFocusHere(0);
			}

			m_FindNext = false;
		}
		
		if (ImGui::IsItemActive())
			m_FindFocused = true;
		else
			m_FindFocused = false;

		if (m_FindJustOpened)
		{
			ImGui::SetKeyboardFocusHere(0);
			m_FindJustOpened = false;
		}

		ImGui::PopItemWidth();

		if (!m_ReadOnly)
		{
			ImGui::SameLine();
			
			if (ImGui::ArrowButton(("##expandFind" + std::string(aTitle)).c_str(), m_ReplaceOpened ? ImGuiDir_Up : ImGuiDir_Down))
				m_ReplaceOpened = !m_ReplaceOpened;
		}

		ImGui::SameLine();

		if (ImGui::Button(("X##" + std::string(aTitle)).c_str()))
			m_FindOpened = false;

		if (m_ReplaceOpened && !m_ReadOnly)
		{
			ImGui::PushItemWidth(UICalculateSize(-45));
			ImGui::NewLine();
			bool shouldReplace = false;

			if (ImGui::InputText(("##ted_replacetb" + std::string(aTitle)).c_str(), m_ReplaceWord, 256, ImGuiInputTextFlags_EnterReturnsTrue))
				shouldReplace = true;
			
			if (ImGui::IsItemActive())
				m_ReplaceFocused = true;
			else
				m_ReplaceFocused = false;
			ImGui::PopItemWidth();

			ImGui::SameLine();

			if (ImGui::Button((">##replaceOne" + std::string(aTitle)).c_str()) || shouldReplace)
			{
				if (strlen(m_FindWord) > 0)
				{
					auto curPos = m_State.CursorPosition;
					
					std::string textSrc = GetText();
				
					if (m_ReplaceIndex >= textSrc.size())
						m_ReplaceIndex = 0;

					size_t textLoc = textSrc.find(m_FindWord, m_ReplaceIndex);
					
					if (textLoc == std::string::npos)
					{
						m_ReplaceIndex = 0;
						textLoc = textSrc.find(m_FindWord, 0);
					}


					if (textLoc != std::string::npos)
					{
						curPos.Line = curPos.Column = 0;
						int totalCount = 0;

						for (size_t ln = 0; ln < m_Lines.size(); ln++)
						{
							int lineCharCount = GetLineCharacterCount(ln) + 1;

							if (textLoc >= totalCount && textLoc < totalCount + lineCharCount)
							{
								curPos.Line = ln;
								curPos.Column = textLoc - totalCount;

								auto& line = m_Lines[curPos.Line];

								for (int i = 0; i < line.size(); i++)
								{
									if (line[i].Character == '\t')
										curPos.Column += (m_TabSize - 1);
								}

								break;
							}

							totalCount += lineCharCount;
						}

						auto selStart = curPos, selEnd = curPos;
						selEnd.Column += strlen(m_FindWord);
						SetSelection(curPos, selEnd);
						DeleteSelection();
						AppendText(m_ReplaceWord);
						SetCursorPosition(selEnd);
						m_ScrollToCursor = true;

						ImGui::SetKeyboardFocusHere(0);

						m_ReplaceIndex = textLoc + strlen(m_ReplaceWord);
					}
				}
			}

			ImGui::SameLine();

			if (ImGui::Button((">>##replaceAll" + std::string(aTitle)).c_str()))
			{
				if (strlen(m_FindWord) > 0)
				{
					auto curPos = m_State.CursorPosition;

					std::string textSrc = GetText();
					size_t textLoc = textSrc.find(m_FindWord, 0);

					do
					{
						if (textLoc != std::string::npos)
						{
							curPos.Line = curPos.Column = 0;
							int totalCount = 0;

							for (size_t ln = 0; ln < m_Lines.size(); ln++)
							{
								int lineCharCount = GetLineCharacterCount(ln) + 1;

								if (textLoc >= totalCount && textLoc < totalCount + lineCharCount)
								{
									curPos.Line = ln;
									curPos.Column = textLoc - totalCount;

									auto& line = m_Lines[curPos.Line];

									for (int i = 0; i < line.size(); i++)
									{
										if (line[i].Character == '\t')
											curPos.Column += (m_TabSize - 1);
									}

									break;
								}
								totalCount += lineCharCount;
							}


							auto selStart = curPos, selEnd = curPos;
							selEnd.Column += strlen(m_FindWord);
							SetSelection(curPos, selEnd);
							DeleteSelection();
							AppendText(m_ReplaceWord);
							SetCursorPosition(selEnd);
							m_ScrollToCursor = true;

							ImGui::SetKeyboardFocusHere(0);

							// find next occurance
							textSrc = GetText();
							textLoc += strlen(m_ReplaceWord);
							textLoc = textSrc.find(m_FindWord, textLoc);
						}
					}
					while (textLoc != std::string::npos);
				}
			}
		}

		ImGui::EndChild();

		ImGui::PopFont();

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
			m_FindOpened = false;
	}

	/* DEBUGGER CONTROLS */
	if (IsDebugging() && m_DebugBar)
	{
		ImFont* font = ImGui::GetFont();
		ImGui::PushFont(font);


		ImGui::SetNextWindowPos(ImVec2(m_FindOrigin.x + windowWidth / 2 - m_DebugBarWidth / 2, m_FindOrigin.y), ImGuiCond_Always);
		ImGui::BeginChild(("##ted_dbgcontrols" + std::string(aTitle)).c_str(), ImVec2(m_DebugBarWidth, m_DebugBarHeight), true, ImGuiWindowFlags_NoScrollbar);

		ImVec2 dbBarStart = ImGui::GetCursorPos();

		if (ImGui::Button(("Step##ted_dbgstep" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::Step);

		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Step In##ted_dbgstepin" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::StepInto);
		
		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Step Out##ted_dbgstepout" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::StepOut);
		
		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Continue##ted_dbgcontinue" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::Continue);
		
		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Stop##ted_dbgstop" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::Stop);

		ImVec2 dbBarEnd = ImGui::GetCursorPos();
		m_DebugBarHeight = dbBarEnd.y - dbBarStart.y + ImGui::GetStyle().WindowPadding.y * 2.0f;

		ImGui::SameLine(0, 6);

		dbBarEnd = ImGui::GetCursorPos();
		m_DebugBarWidth = dbBarEnd.x - dbBarStart.x + ImGui::GetStyle().WindowPadding.x * 2.0f;

		ImGui::EndChild();

		ImGui::PopFont();
	}

	if (m_HandleKeyboardInputs)
		ImGui::PopAllowKeyboardFocus();

	if (!m_IgnoreImGuiChild)
		ImGui::EndChild();
	
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	// breakpoint condition popup
	if (openBkptConditionWindow)
		ImGui::OpenPopup("Condition##condition");

	ImFont* font = ImGui::GetFont();
	////TODO! 
	ImGui::PushFont(font);

	ImGui::SetNextWindowSize(ImVec2(430, 175), ImGuiCond_Once);

	if (ImGui::BeginPopupModal("Condition##condition"))
	{
		ImGui::Checkbox("Use condition", &m_PopupCondition_Use);

		if (!m_PopupCondition_Use)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		
		ImGui::InputText("Condition", m_PopupCondition_Condition, 512);
		
		if (!m_PopupCondition_Use)
		{
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();

		ImGui::SameLine();
		
		if (ImGui::Button("OK"))
		{
			size_t clen = strlen(m_PopupCondition_Condition);
			bool isEmpty = true;
			
			for (size_t i = 0; i < clen; i++)
			{
				if (!isspace(m_PopupCondition_Condition[i]))
				{
					isEmpty = false;
					break;
				}
			}
			Breakpoint& bkpt = GetBreakpoint(m_PopupCondition_Line);
			bkpt.Condition = m_PopupCondition_Condition;
			bkpt.UseCondition = (m_PopupCondition_Use && !isEmpty);

			if (OnBreakpointUpdate)
				OnBreakpointUpdate(this, bkpt.Line, bkpt.UseCondition, bkpt.Condition, bkpt.Enabled);

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	////TODO! 
	ImGui::PopFont();

	m_WithinRender = false;
}

void ImTextEdit::SetText(const std::string & aText)
{
	m_Lines.clear();
	m_FoldBegin.clear();
	m_FoldEnd.clear();
	m_FoldSorted = false;

	m_Lines.emplace_back(Line());

	for (auto chr : aText)
	{
		if (chr == '\r') {} // ignore the carriage return character
		else if (chr == '\n')
		{
			m_Lines.emplace_back(Line());
		}
		else
		{
			if (chr == '{')
				m_FoldBegin.push_back(Coordinates(m_Lines.size() - 1, m_Lines.back().size()));
			else if (chr == '}')
				m_FoldEnd.push_back(Coordinates(m_Lines.size() - 1, m_Lines.back().size()));

			m_Lines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
		}
	}
	
	m_TextChanged = true;
	m_ScrollToTop = true;

	m_UndoBuffer.clear();
	m_UndoIndex = 0;

	Colorize();
}

void ImTextEdit::SetTextLines(const std::vector<std::string> & aLines)
{
	m_Lines.clear();
	m_FoldBegin.clear();
	m_FoldEnd.clear();
	m_FoldSorted = false;

	if (aLines.empty())
	{
		m_Lines.emplace_back(Line());
	}
	else
	{
		m_Lines.resize(aLines.size());

		for (size_t i = 0; i < aLines.size(); ++i)
		{
			const std::string & aLine = aLines[i];

			m_Lines[i].reserve(aLine.size());

			for (size_t j = 0; j < aLine.size(); ++j)
			{
				if (aLine[j] == '{')
					m_FoldBegin.push_back(Coordinates(i, j));
				else if (aLine[j] == '}')
					m_FoldEnd.push_back(Coordinates(i, j));

				m_Lines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
			}
		}
	}

	m_TextChanged = true;
	m_ScrollToTop = true;

	m_UndoBuffer.clear();
	m_UndoIndex = 0;

	Colorize();
}

void ImTextEdit::EnterCharacter(ImWchar aChar, bool aShift)
{
	assert(!m_ReadOnly);

	UndoRecord u;

	u.Before = m_State;

	if (HasSelection())
	{
		if (aChar == '\t' && m_State.SelectionStart.Line != m_State.SelectionEnd.Line)
		{
			auto start = m_State.SelectionStart;
			auto end = m_State.SelectionEnd;
			auto originalEnd = end;

			if (start > end)
				std::swap(start, end);

			start.Column = 0;
			//			end.mColumn = end.mLine < mLines.size() ? mLines[end.mLine].size() : 0;
			if (end.Column == 0 && end.Line > 0)
				--end.Line;

			if (end.Line >= (int)m_Lines.size())
				end.Line = m_Lines.empty() ? 0 : (int)m_Lines.size() - 1;
			
			end.Column = GetLineMaxColumn(end.Line);

			//if (end.mColumn >= GetLineMaxColumn(end.mLine))
			//	end.mColumn = GetLineMaxColumn(end.mLine) - 1;

			u.RemovedStart = start;
			u.RemovedEnd = end;
			u.Removed = GetText(start, end);

			bool modified = false;

			for (int i = start.Line; i <= end.Line; i++)
			{
				auto& line = m_Lines[i];

				if (aShift)
				{
					if (!line.empty())
					{
						if (line.front().Character == '\t')
						{
							line.erase(line.begin());
							modified = true;
						}
						else
						{
							for (int j = 0; j < m_TabSize && !line.empty() && line.front().Character == ' '; j++)
							{
								line.erase(line.begin());
								modified = true;
							}
						}
					}
				}
				else
				{
					if (m_InsertSpaces)
					{
						for (int i = 0; i < m_TabSize; i++)
							line.insert(line.begin(), Glyph(' ', ImTextEdit::PaletteIndex::Background));
					}
					else
					{
						line.insert(line.begin(), Glyph('\t', ImTextEdit::PaletteIndex::Background));
					}

					modified = true;
				}
			}

			if (modified)
			{
				start = Coordinates(start.Line, GetCharacterColumn(start.Line, 0));
				Coordinates rangeEnd;
				if (originalEnd.Column != 0)
				{
					end = Coordinates(end.Line, GetLineMaxColumn(end.Line));
					rangeEnd = end;
					u.Added = GetText(start, end);
				}
				else
				{
					end = Coordinates(originalEnd.Line, 0);
					rangeEnd = Coordinates(end.Line - 1, GetLineMaxColumn(end.Line - 1));
					u.Added = GetText(start, rangeEnd);
				}

				u.AddedStart = start;
				u.AddedEnd = rangeEnd;
				u.After = m_State;

				m_State.SelectionStart = start;
				m_State.SelectionEnd = end;
				AddUndo(u);

				m_TextChanged = true;

				if (OnContentUpdate != nullptr)
					OnContentUpdate(this);

				EnsureCursorVisible();
			}

			return;
		}
		else
		{
			u.Removed = GetSelectedText();
			u.RemovedStart = m_State.SelectionStart;
			u.RemovedEnd = m_State.SelectionEnd;
			DeleteSelection();
		}
	}

	auto coord = GetActualCursorCoordinates();
	u.AddedStart = coord;

	if (m_Lines.empty())
		m_Lines.push_back(Line());

	if (aChar == '\n')
	{
		InsertLine(coord.Line + 1, coord.Column);
		auto& line = m_Lines[coord.Line];
		auto& newLine = m_Lines[coord.Line + 1];
		auto cindex = GetCharacterIndex(coord);

		int foldOffset = 0;

		for (int i = 0; i < cindex; i++)
			foldOffset -= 1 + (line[i].Character == '\t') * 3;

		if (m_LanguageDefinition.AutoIndentation && m_SmartIndent)
		{
			for (size_t it = 0; it < line.size() && isascii(line[it].Character) && isblank(line[it].Character); ++it)
			{
				newLine.push_back(line[it]);
				foldOffset += 1 + (line[it].Character == '\t') * 3;
			}
		}

		const size_t whitespaceSize = newLine.size();
		newLine.insert(newLine.end(), line.begin() + cindex, line.end());
		line.erase(line.begin() + cindex, line.begin() + line.size());
		SetCursorPosition(Coordinates(coord.Line + 1, GetCharacterColumn(coord.Line + 1, (int)whitespaceSize)));
		u.Added = (char)aChar;

		// shift folds
		for (int b = 0; b < m_FoldBegin.size(); b++)
			if (m_FoldBegin[b].Line == coord.Line + 1)
				m_FoldBegin[b].Column = std::max<int>(0, (m_FoldBegin[b].Column + foldOffset) + (m_FoldBegin[b].Column != coord.Column));
		for (int b = 0; b < m_FoldEnd.size(); b++)
			if (m_FoldEnd[b].Line == coord.Line + 1)
				m_FoldEnd[b].Column = std::max<int>(0, (m_FoldEnd[b].Column + foldOffset) + (m_FoldEnd[b].Column != coord.Column));
	}
	else
	{
		char buf[7];
		int e = ImTextCharToUtf8(buf, 7, aChar);

		if (e > 0)
		{
			if (m_InsertSpaces && e == 1 && buf[0] == '\t')
			{
				for (int i = 0; i < m_TabSize; i++)
					buf[i] = ' ';
				e = m_TabSize;
			}

			buf[e] = '\0';

			auto& line = m_Lines[coord.Line];
			auto cindex = GetCharacterIndex(coord);

			if (m_Overwrite && cindex < (int)line.size())
			{
				auto d = UTF8CharLength(line[cindex].Character);

				u.RemovedStart = m_State.CursorPosition;
				u.RemovedEnd = Coordinates(coord.Line, GetCharacterColumn(coord.Line, cindex + d));

				while (d-- > 0 && cindex < (int)line.size())
				{
					u.Removed += line[cindex].Character;


					// remove fold information if needed
					if (line[cindex].Character == '{')
					{
						for (int fl = 0; fl < m_FoldBegin.size(); fl++)
						{
							if (m_FoldBegin[fl].Line == coord.Line && m_FoldBegin[fl].Column == coord.Column)
							{
								m_FoldBegin.erase(m_FoldBegin.begin() + fl);
								m_FoldSorted = false;
								break;
							}
						}
					}

					if (line[cindex].Character == '}')
					{
						for (int fl = 0; fl < m_FoldEnd.size(); fl++)
						{
							if (m_FoldEnd[fl].Line == coord.Line && m_FoldEnd[fl].Column == coord.Column)
							{
								m_FoldEnd.erase(m_FoldEnd.begin() + fl);
								m_FoldSorted = false;
								break;
							}
						}
					}

					line.erase(line.begin() + cindex);
				}
			}

			// move the folds if necessary
			int foldOffset = 0;

			if (buf[0] == '\t')
				foldOffset = m_TabSize - (coord.Column - (coord.Column / m_TabSize) * m_TabSize);
			else
				foldOffset = strlen(buf);

			int foldColumn = GetCharacterColumn(coord.Line, cindex);

			for (int b = 0; b < m_FoldBegin.size(); b++)
			{
				if (m_FoldBegin[b].Line == coord.Line && m_FoldBegin[b].Column >= foldColumn)
					m_FoldBegin[b].Column += foldOffset;
			}

			for (int b = 0; b < m_FoldEnd.size(); b++)
			{
				if (m_FoldEnd[b].Line == coord.Line && m_FoldEnd[b].Column >= foldColumn)
					m_FoldEnd[b].Column += foldOffset;
			}

			// insert text
			for (auto p = buf; *p != '\0'; p++, ++cindex)
			{
				if (*p == '{')
				{
					m_FoldBegin.push_back(Coordinates(coord.Line, foldColumn));
					m_FoldSorted = false;
				}
				else if (*p == '}')
				{
					m_FoldEnd.push_back(Coordinates(coord.Line, foldColumn));
					m_FoldSorted = false;
				}

				line.insert(line.begin() + cindex, Glyph(*p, PaletteIndex::Default));
			}

			u.Added = buf;

			SetCursorPosition(Coordinates(coord.Line, GetCharacterColumn(coord.Line, cindex)));
		}
		else
		{
			return;
		}
	}

	// active suggestions
	if (m_ActiveAutocomplete && aChar <= 127 && (isalpha(aChar) || aChar == '_'))
	{
		m_RequestAutocomplete = true;
		m_ReadyForAutocomplete = false;
	}

	if (m_ScrollbarMarkers)
	{
		bool changeExists = false;

		for (int i = 0; i < m_ChangedLines.size(); i++)
		{
			if (m_ChangedLines[i] == m_State.CursorPosition.Line)
			{
				changeExists = true;
				break;
			}
		}

		if (!changeExists)
			m_ChangedLines.push_back(m_State.CursorPosition.Line);
	}

	m_TextChanged = true;
	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);

	u.AddedEnd = GetActualCursorCoordinates();
	u.After = m_State;

	AddUndo(u);

	Colorize(coord.Line - 1, 3);
	EnsureCursorVisible();

	// function tooltip
	if (m_FunctionDeclarationTooltipEnabled)
	{
		if (aChar == '(')
		{
			auto curPos = GetCorrectCursorPosition();
			std::string obj = GetWordAt(curPos);
		}
		else if (aChar == ',')
		{
			auto curPos = GetCorrectCursorPosition();
			curPos.Column--;

			const auto& line = m_Lines[curPos.Line];
			std::string obj = "";
			int weight = 0;

			for (; curPos.Column > 0; curPos.Column--)
			{
				if (line[curPos.Column].Character == '(')
				{
					if (weight == 0)
					{
						obj = GetWordAt(curPos);
						break;
					}

					weight--;
				}

				if (line[curPos.Column].Character == ')')
					weight++;
			}
		}
	}

	// auto brace completion
	if (m_CompleteBraces)
	{
		if (aChar == '{')
		{
			EnterCharacter('\n', false);
			EnterCharacter('}', false);
		}
		else if (aChar == '(')
		{
			EnterCharacter(')', false);
		}
		else if (aChar == '[')
		{
			EnterCharacter(']', false);
		}

		if (aChar == '{' || aChar == '(' || aChar == '[')
			m_State.CursorPosition.Column--;
	}
}

void ImTextEdit::SetReadOnly(bool aValue)
{
	m_ReadOnly = aValue;
}

void ImTextEdit::SetColorizerEnable(bool aValue)
{
	m_ColorizerEnabled = aValue;
}

ImTextEdit::Coordinates ImTextEdit::GetCorrectCursorPosition()
{
	auto curPos = GetCursorPosition();

	if (curPos.Line >= 0 && curPos.Line <= GetCursorPosition().Line)
	{
		for (int c = 0; c < std::min<int>(curPos.Line, m_Lines[curPos.Line].size()); c++)
		{
			if (m_Lines[curPos.Line][c].Character == '\t')
				curPos.Column -= (GetTabSize() - 1);
		}
	}

	return curPos;
}

void ImTextEdit::SetCursorPosition(const Coordinates & aPosition)
{
	if (m_State.CursorPosition != aPosition)
	{
		m_State.CursorPosition = aPosition;
		m_CursorPositionChanged = true;
		EnsureCursorVisible();
	}
}

void ImTextEdit::SetSelectionStart(const Coordinates & aPosition)
{
	m_State.SelectionStart = SanitizeCoordinates(aPosition);

	if (m_State.SelectionStart > m_State.SelectionEnd)
		std::swap(m_State.SelectionStart, m_State.SelectionEnd);
}

void ImTextEdit::SetSelectionEnd(const Coordinates & aPosition)
{
	m_State.SelectionEnd = SanitizeCoordinates(aPosition);
	
	if (m_State.SelectionStart > m_State.SelectionEnd)
		std::swap(m_State.SelectionStart, m_State.SelectionEnd);
}

void ImTextEdit::SetSelection(const Coordinates & aStart, const Coordinates & aEnd, SelectionMode aMode)
{
	auto oldSelStart = m_State.SelectionStart;
	auto oldSelEnd = m_State.SelectionEnd;

	m_State.SelectionStart = SanitizeCoordinates(aStart);
	m_State.SelectionEnd = SanitizeCoordinates(aEnd);
	
	if (m_State.SelectionStart > m_State.SelectionEnd)
		std::swap(m_State.SelectionStart, m_State.SelectionEnd);

	switch (aMode)
	{
	case ImTextEdit::SelectionMode::Normal:
		break;
	case ImTextEdit::SelectionMode::Word:
	{
		m_State.SelectionStart = FindWordStart(m_State.SelectionStart);
		
		if (!IsOnWordBoundary(m_State.SelectionEnd))
			m_State.SelectionEnd = FindWordEnd(FindWordStart(m_State.SelectionEnd));

		break;
	}
	case ImTextEdit::SelectionMode::Line:
	{
		const auto lineNo = m_State.SelectionEnd.Line;
		const auto lineSize = (size_t)lineNo < m_Lines.size() ? m_Lines[lineNo].size() : 0;
		m_State.SelectionStart = Coordinates(m_State.SelectionStart.Line, 0);
		m_State.SelectionEnd = Coordinates(lineNo, GetLineMaxColumn(lineNo));
		break;
	}
	default:
		break;
	}

	if (m_State.SelectionStart != oldSelStart || m_State.SelectionEnd != oldSelEnd)
		m_CursorPositionChanged = true;

	// update mReplaceIndex
	m_ReplaceIndex = 0;

	for (size_t ln = 0; ln < m_State.CursorPosition.Line; ln++)
		m_ReplaceIndex += GetLineCharacterCount(ln) + 1;

	m_ReplaceIndex += m_State.CursorPosition.Column;
}

void ImTextEdit::AppendText(const std::string& aValue, bool indent)
{
	AppendText(aValue.c_str(), indent);
}

void ImTextEdit::AppendText(const char* aValue, bool indent)
{
	if (aValue == nullptr)
		return;

	auto pos = GetActualCursorCoordinates();
	auto start = std::min<Coordinates>(pos, m_State.SelectionStart);
	int totalLines = pos.Line - start.Line;

	totalLines += InsertTextAt(pos, aValue, indent);

	SetSelection(pos, pos);
	SetCursorPosition(pos);
	Colorize(start.Line - 1, totalLines + 2);
}

void ImTextEdit::DeleteSelection()
{
	assert(m_State.SelectionEnd >= m_State.SelectionStart);

	if (m_State.SelectionEnd == m_State.SelectionStart)
		return;

	DeleteRange(m_State.SelectionStart, m_State.SelectionEnd);

	SetSelection(m_State.SelectionStart, m_State.SelectionStart);
	SetCursorPosition(m_State.SelectionStart);
	Colorize(m_State.SelectionStart.Line, 1);
}

void ImTextEdit::MoveUp(int aAmount, bool aSelect)
{
	auto oldPos = m_State.CursorPosition;
	m_State.CursorPosition.Line = std::max<int>(0, m_State.CursorPosition.Line - aAmount);

	if (oldPos != m_State.CursorPosition)
	{
		if (aSelect)
		{
			if (oldPos == m_InteractiveStart)
			{
				m_InteractiveStart = m_State.CursorPosition;
			}
			else if (oldPos == m_InteractiveEnd)
			{
				m_InteractiveEnd = m_State.CursorPosition;
			}
			else
			{
				m_InteractiveStart = m_State.CursorPosition;
				m_InteractiveEnd = oldPos;
			}
		}
		else
		{
			m_InteractiveStart = m_InteractiveEnd = m_State.CursorPosition;
		}

		SetSelection(m_InteractiveStart, m_InteractiveEnd);
		EnsureCursorVisible();
	}
}

void ImTextEdit::MoveDown(int aAmount, bool aSelect)
{
	assert(m_State.CursorPosition.Column >= 0);
	auto oldPos = m_State.CursorPosition;
	m_State.CursorPosition.Line = std::max<int>(0, std::min<int>((int)m_Lines.size() - 1, m_State.CursorPosition.Line + aAmount));

	if (m_State.CursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == m_InteractiveEnd)
			{
				m_InteractiveEnd = m_State.CursorPosition;
			}
			else if (oldPos == m_InteractiveStart)
			{
				m_InteractiveStart = m_State.CursorPosition;
			}
			else
			{
				m_InteractiveStart = oldPos;
				m_InteractiveEnd = m_State.CursorPosition;
			}
		}
		else
		{
			m_InteractiveStart = m_InteractiveEnd = m_State.CursorPosition;
		}

		SetSelection(m_InteractiveStart, m_InteractiveEnd);
		EnsureCursorVisible();
	}
}

static bool IsUTFSequence(char c)
{
	return (c & 0xC0) == 0x80;
}

void ImTextEdit::MoveLeft(int aAmount, bool aSelect, bool aWordMode)
{
	if (m_Lines.empty())
		return;

	auto oldPos = m_State.CursorPosition;
	m_State.CursorPosition = GetActualCursorCoordinates();
	auto line = m_State.CursorPosition.Line;
	auto cindex = GetCharacterIndex(m_State.CursorPosition);

	while (aAmount-- > 0)
	{
		if (cindex == 0)
		{
			if (line > 0)
			{
				--line;

				if ((int)m_Lines.size() > line)
					cindex = (int)m_Lines[line].size();
				else
					cindex = 0;
			}
		}
		else
		{
			--cindex;

			if (cindex > 0)
			{
				if ((int)m_Lines.size() > line)
				{
					while (cindex > 0 && IsUTFSequence(m_Lines[line][cindex].Character))
						--cindex;
				}
			}
		}

		m_State.CursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));

		if (aWordMode)
		{
			m_State.CursorPosition = FindWordStart(m_State.CursorPosition);
			cindex = GetCharacterIndex(m_State.CursorPosition);
		}
	}

	m_State.CursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));

	assert(m_State.CursorPosition.Column >= 0);

	if (aSelect)
	{
		m_InteractiveStart = m_State.SelectionStart;
		m_InteractiveEnd = m_State.SelectionEnd;

		if (oldPos == m_InteractiveStart)
		{
			m_InteractiveStart = m_State.CursorPosition;
		}
		else if (oldPos == m_InteractiveEnd)
		{
			m_InteractiveEnd = m_State.CursorPosition;
		}
		else
		{
			m_InteractiveStart = m_State.CursorPosition;
			m_InteractiveEnd = oldPos;
		}
	}
	else
	{
		m_InteractiveStart = m_InteractiveEnd = m_State.CursorPosition;
	}

	SetSelection(m_InteractiveStart, m_InteractiveEnd, SelectionMode::Normal);
	EnsureCursorVisible();
}

void ImTextEdit::MoveRight(int aAmount, bool aSelect, bool aWordMode)
{
	auto oldPos = m_State.CursorPosition;

	if (m_Lines.empty() || oldPos.Line >= m_Lines.size())
		return;
		
	m_State.CursorPosition = GetActualCursorCoordinates();
	auto line = m_State.CursorPosition.Line;
	auto cindex = GetCharacterIndex(m_State.CursorPosition);

	while (aAmount-- > 0)
	{
		auto lindex = m_State.CursorPosition.Line;
		auto& line = m_Lines[lindex];

		if (cindex >= line.size())
		{
			if (m_State.CursorPosition.Line < m_Lines.size() - 1)
			{
				m_State.CursorPosition.Line = std::max(0, std::min((int)m_Lines.size() - 1, m_State.CursorPosition.Line + 1));
				m_State.CursorPosition.Column = 0;
			}
			else
			{
				return;
			}
		}
		else
		{
			cindex += UTF8CharLength(line[cindex].Character);
			m_State.CursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));

			if (aWordMode)
				m_State.CursorPosition = FindWordEnd(m_State.CursorPosition);
		}
	}

	if (aSelect)
	{
		m_InteractiveStart = m_State.SelectionStart;
		m_InteractiveEnd = m_State.SelectionEnd;

		if (oldPos == m_InteractiveEnd)
		{
			m_InteractiveEnd = m_State.CursorPosition;
		}
		else if (oldPos == m_InteractiveStart)
		{
			m_InteractiveStart = m_State.CursorPosition;
		}
		else
		{
			m_InteractiveStart = oldPos;
			m_InteractiveEnd = m_State.CursorPosition;
		}
	}
	else
	{
		m_InteractiveStart = m_InteractiveEnd = m_State.CursorPosition;
	}

	SetSelection(m_InteractiveStart, m_InteractiveEnd, SelectionMode::Normal);
	EnsureCursorVisible();
}

void ImTextEdit::MoveTop(bool aSelect)
{
	auto oldPos = m_State.CursorPosition;
	SetCursorPosition(Coordinates(0, 0));

	if (m_State.CursorPosition != oldPos)
	{
		if (aSelect)
		{
			m_InteractiveEnd = oldPos;
			m_InteractiveStart = m_State.CursorPosition;
		}
		else
		{
			m_InteractiveStart = m_InteractiveEnd = m_State.CursorPosition;
		}

		SetSelection(m_InteractiveStart, m_InteractiveEnd);
	}
}

void ImTextEdit::MoveBottom(bool aSelect)
{
	auto oldPos = GetCursorPosition();
	auto newPos = Coordinates((int)m_Lines.size() - 1, 0);
	SetCursorPosition(newPos);

	if (aSelect)
	{
		m_InteractiveStart = oldPos;
		m_InteractiveEnd = newPos;
	}
	else
	{
		m_InteractiveStart = m_InteractiveEnd = newPos;
	}

	SetSelection(m_InteractiveStart, m_InteractiveEnd);
}

void ImTextEdit::MoveHome(bool aSelect)
{
	auto oldPos = m_State.CursorPosition;
	SetCursorPosition(Coordinates(m_State.CursorPosition.Line, 0));

	if (m_State.CursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == m_InteractiveStart)
			{
				m_InteractiveStart = m_State.CursorPosition;
			}
			else if (oldPos == m_InteractiveEnd)
			{
				m_InteractiveEnd = m_State.CursorPosition;
			}
			else
			{
				m_InteractiveStart = m_State.CursorPosition;
				m_InteractiveEnd = oldPos;
			}
		}
		else
		{
			m_InteractiveStart = m_InteractiveEnd = m_State.CursorPosition;
		}

		SetSelection(m_InteractiveStart, m_InteractiveEnd);
	}
}

void ImTextEdit::MoveEnd(bool aSelect)
{
	auto oldPos = m_State.CursorPosition;
	SetCursorPosition(Coordinates(m_State.CursorPosition.Line, GetLineMaxColumn(oldPos.Line)));

	if (m_State.CursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == m_InteractiveEnd)
			{
				m_InteractiveEnd = m_State.CursorPosition;
			}
			else if (oldPos == m_InteractiveStart)
			{
				m_InteractiveStart = m_State.CursorPosition;
			}
			else
			{
				m_InteractiveStart = oldPos;
				m_InteractiveEnd = m_State.CursorPosition;
			}
		}
		else
		{
			m_InteractiveStart = m_InteractiveEnd = m_State.CursorPosition;
		}

		SetSelection(m_InteractiveStart, m_InteractiveEnd);
	}
}

void ImTextEdit::Delete()
{
	assert(!m_ReadOnly);

	if (m_Lines.empty())
		return;

	UndoRecord u;
	u.Before = m_State;

	if (HasSelection())
	{
		u.Removed = GetSelectedText();
		u.RemovedStart = m_State.SelectionStart;
		u.RemovedEnd = m_State.SelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);
		auto& line = m_Lines[pos.Line];

		if (pos.Column == GetLineMaxColumn(pos.Line))
		{
			if (pos.Line == (int)m_Lines.size() - 1)
				return;

			u.Removed = '\n';
			u.RemovedStart = u.RemovedEnd = GetActualCursorCoordinates();
			Advance(u.RemovedEnd);

			// move folds
			for (int i = 0; i < m_FoldBegin.size(); i++)
			{
				if (m_FoldBegin[i].Line == pos.Line + 1)
				{
					m_FoldBegin[i].Line = std::max<int>(0, m_FoldBegin[i].Line - 1);
					m_FoldBegin[i].Column += GetCharacterColumn(pos.Line, line.size());
				}
			}

			for (int i = 0; i < m_FoldEnd.size(); i++)
			{
				if (m_FoldEnd[i].Line == pos.Line + 1)
				{
					m_FoldEnd[i].Line = std::max<int>(0, m_FoldEnd[i].Line - 1);
					m_FoldEnd[i].Column += GetCharacterColumn(pos.Line, line.size());
				}
			}

			auto& nextLine = m_Lines[pos.Line + 1];
			line.insert(line.end(), nextLine.begin(), nextLine.end());

			RemoveLine(pos.Line + 1);
		}
		else
		{
			auto cindex = GetCharacterIndex(pos);
			u.RemovedStart = u.RemovedEnd = GetActualCursorCoordinates();
			u.RemovedEnd.Column++;
			u.Removed = GetText(u.RemovedStart, u.RemovedEnd);

			RemoveFolds(u.RemovedStart, u.RemovedEnd);

			auto d = UTF8CharLength(line[cindex].Character);

			while (d-- > 0 && cindex < (int)line.size())
				line.erase(line.begin() + cindex);
		}

		if (m_ScrollbarMarkers)
		{
			bool changeExists = false;

			for (int i = 0; i < m_ChangedLines.size(); i++)
			{
				if (m_ChangedLines[i] == m_State.CursorPosition.Line)
				{
					changeExists = true;
					break;
				}
			}

			if (!changeExists)
				m_ChangedLines.push_back(m_State.CursorPosition.Line);
		}

		m_TextChanged = true;

		if (OnContentUpdate != nullptr)
			OnContentUpdate(this);

		Colorize(pos.Line, 1);
	}

	u.After = m_State;
	AddUndo(u);
}

void ImTextEdit::DuplicateLine()
{
	ImTextEdit::UndoRecord undo;
	undo.Before = m_State;

	auto oldLine = m_Lines[m_State.CursorPosition.Line];
	auto& line = InsertLine(m_State.CursorPosition.Line, m_State.CursorPosition.Column);

	undo.Added += '\n';

	for (auto& glyph : oldLine)
	{
		line.push_back(glyph);
		undo.Added += glyph.Character;
	}

	m_State.CursorPosition.Line++;

	undo.AddedStart = ImTextEdit::Coordinates(m_State.CursorPosition.Line - 1, m_State.CursorPosition.Column);
	undo.AddedEnd = m_State.CursorPosition;

	undo.After = m_State;

	AddUndo(undo);
}

void ImTextEdit::Backspace()
{
	assert(!m_ReadOnly);

	if (m_Lines.empty())
		return;

	UndoRecord u;
	u.Before = m_State;

	if (HasSelection())
	{
		u.Removed = GetSelectedText();
		u.RemovedStart = m_State.SelectionStart;
		u.RemovedEnd = m_State.SelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);

		if (m_State.CursorPosition.Column == 0)
		{
			if (m_State.CursorPosition.Line == 0)
				return;

			u.Removed = '\n';
			u.RemovedStart = u.RemovedEnd = Coordinates(pos.Line - 1, GetLineMaxColumn(pos.Line - 1));
			Advance(u.RemovedEnd);

			auto& line = m_Lines[m_State.CursorPosition.Line];
			auto& prevLine = m_Lines[m_State.CursorPosition.Line - 1];
			auto prevSize = GetLineMaxColumn(m_State.CursorPosition.Line - 1);
			prevLine.insert(prevLine.end(), line.begin(), line.end());

			// error markers
			td_ErrorMarkers etmp;
			for (auto& i : m_ErrorMarkers)
				etmp.insert(td_ErrorMarkers::value_type(i.first - 1 == m_State.CursorPosition.Line ? i.first - 1 : i.first, i.second));
			m_ErrorMarkers = std::move(etmp);

			// shift folds
			for (int b = 0; b < m_FoldBegin.size(); b++)
			{
				if (m_FoldBegin[b].Line == m_State.CursorPosition.Line)
				{
					m_FoldBegin[b].Line = std::max<int>(0, m_FoldBegin[b].Line - 1);
					m_FoldBegin[b].Column = m_FoldBegin[b].Column + prevSize;
				}
			}

			for (int b = 0; b < m_FoldEnd.size(); b++)
			{
				if (m_FoldEnd[b].Line == m_State.CursorPosition.Line)
				{
					m_FoldEnd[b].Line = std::max<int>(0, m_FoldEnd[b].Line - 1);
					m_FoldEnd[b].Column = m_FoldEnd[b].Column + prevSize;
				}
			}

			RemoveLine(m_State.CursorPosition.Line);
			--m_State.CursorPosition.Line;
			m_State.CursorPosition.Column = prevSize;

		}
		else
		{
			auto& line = m_Lines[m_State.CursorPosition.Line];
			auto cindex = GetCharacterIndex(pos) - 1;
			auto cend = cindex + 1;

			while (cindex > 0 && IsUTFSequence(line[cindex].Character))
				--cindex;

			//if (cindex > 0 && UTF8CharLength(line[cindex].Character) > 1)
			//	--cindex;

			int actualLoc = pos.Column;

			for (int i = 0; i < line.size(); i++)
			{
				if (line[i].Character == '\t')
					actualLoc -= GetTabSize() - 1;
			}

			if (m_CompleteBraces && actualLoc > 0 && actualLoc < line.size()) {
				if ((line[actualLoc - 1].Character == '(' && line[actualLoc].Character == ')') || (line[actualLoc - 1].Character == '{' && line[actualLoc].Character == '}') || (line[actualLoc - 1].Character == '[' && line[actualLoc].Character == ']'))
					Delete();
			}

			u.RemovedStart = u.RemovedEnd = GetActualCursorCoordinates();

			while (cindex < line.size() && cend-- > cindex)
			{
				uint8_t chVal = line[cindex].Character;

				int remColumn = 0;

				for (int i = 0; i < cindex && i < line.size(); i++)
				{
					if (line[i].Character == '\t')
					{
						int tabSize = remColumn - (remColumn / m_TabSize) * m_TabSize;
						remColumn += m_TabSize - tabSize;
					}
					else
					{
						remColumn++;
					}
				}

				int remSize = m_State.CursorPosition.Column - remColumn;

				u.Removed += line[cindex].Character;
				u.RemovedStart.Column -= remSize;

				line.erase(line.begin() + cindex);

				m_State.CursorPosition.Column -= remSize;
			}

			RemoveFolds(u.RemovedStart, u.RemovedEnd);
		}

		if (m_ScrollbarMarkers)
		{
			bool changeExists = false;

			for (int i = 0; i < m_ChangedLines.size(); i++)
			{
				if (m_ChangedLines[i] == m_State.CursorPosition.Line)
				{
					changeExists = true;
					break;
				}
			}

			if (!changeExists)
				m_ChangedLines.push_back(m_State.CursorPosition.Line);
		}

		m_TextChanged = true;

		if (OnContentUpdate != nullptr)
			OnContentUpdate(this);

		EnsureCursorVisible();
		Colorize(m_State.CursorPosition.Line, 1);
	}

	u.After = m_State;
	AddUndo(u);

	// auto-complete
	if (m_ActiveAutocomplete && m_ACOpened)
	{
		m_RequestAutocomplete = true;
		m_ReadyForAutocomplete = false;
	}
}

void ImTextEdit::SelectWordUnderCursor()
{
	auto c = GetCursorPosition();
	SetSelection(FindWordStart(c), FindWordEnd(c));
}

void ImTextEdit::SelectAll()
{
	SetSelection(Coordinates(0, 0), Coordinates((int)m_Lines.size(), 0));
}

bool ImTextEdit::HasSelection() const
{
	return m_State.SelectionEnd > m_State.SelectionStart;
}

void ImTextEdit::SetShortcut(ImTextEdit::ShortcutID id, Shortcut s)
{
	m_Shortcuts[(int)id].Key1 = s.Key1;
	m_Shortcuts[(int)id].Key2 = s.Key2;

	if (m_Shortcuts[(int)id].Ctrl != 2)
		m_Shortcuts[(int)id].Ctrl = s.Ctrl;

	if (m_Shortcuts[(int)id].Shift != 2)
		m_Shortcuts[(int)id].Shift = s.Shift;

	if (m_Shortcuts[(int)id].Alt != 2)
		m_Shortcuts[(int)id].Alt = s.Alt;
}

void ImTextEdit::Copy()
{
	if (HasSelection())
	{
		ImGui::SetClipboardText(GetSelectedText().c_str());
	}
	else
	{
		if (!m_Lines.empty())
		{
			std::string str;
			auto& line = m_Lines[GetActualCursorCoordinates().Line];

			for (auto& g : line)
				str.push_back(g.Character);

			ImGui::SetClipboardText(str.c_str());
		}
	}
}

void ImTextEdit::Cut()
{
	if (IsReadOnly())
	{
		Copy();
	}
	else if (HasSelection())
	{
		UndoRecord u;
		u.Before = m_State;
		u.Removed = GetSelectedText();
		u.RemovedStart = m_State.SelectionStart;
		u.RemovedEnd = m_State.SelectionEnd;

		Copy();
		DeleteSelection();

		u.After = m_State;
		AddUndo(u);
	}
}

void ImTextEdit::Paste()
{
	if (IsReadOnly())
		return;

	auto clipText = ImGui::GetClipboardText();

	if (clipText != nullptr && strlen(clipText) > 0)
	{
		UndoRecord u;
		u.Before = m_State;

		if (HasSelection())
		{
			u.Removed = GetSelectedText();
			u.RemovedStart = m_State.SelectionStart;
			u.RemovedEnd = m_State.SelectionEnd;
			DeleteSelection();
		}

		u.Added = clipText;
		u.AddedStart = GetActualCursorCoordinates();

		AppendText(clipText, m_AutoindentOnPaste);

		u.AddedEnd = GetActualCursorCoordinates();
		u.After = m_State;
		AddUndo(u);
	}
}

bool ImTextEdit::CanUndo()
{
	return !IsReadOnly() && m_UndoIndex > 0;
}

bool ImTextEdit::CanRedo()
{
	return !IsReadOnly() && m_UndoIndex < (int)m_UndoBuffer.size();
}

void ImTextEdit::Undo(int aSteps)
{
	while (CanUndo() && aSteps-- > 0)
		m_UndoBuffer[--m_UndoIndex].Undo(this);
}

void ImTextEdit::Redo(int aSteps)
{
	while (CanRedo() && aSteps-- > 0)
		m_UndoBuffer[m_UndoIndex++].Redo(this);
}

void ImTextEdit::ResetUndos()
{
	m_UndoIndex = 0;
	m_UndoBuffer.clear();
	m_UndoBuffer.shrink_to_fit();
}

std::vector<std::string> ImTextEdit::GetRelevantExpressions(int line)
{
	std::vector<std::string> ret;
	line--;

	if (line < 0 || line >= m_Lines.size() || (m_LanguageDefinition.Name != "HLSL" && m_LanguageDefinition.Name != "GLSL"))
		return ret;

	std::string expr = "";

	for (int i = 0; i < m_Lines[line].size(); i++)
		expr += m_Lines[line][i].Character;

	enum class TokenType
	{
		Identifier,
		Operator,
		Number,
		Parenthesis,
		Comma,
		Semicolon
	};
	
	struct Token
	{
		TokenType Type;
		std::string Content;
	};

	char buffer[512] = { 0 };
	int bufferLoc = 0;
	std::vector<Token> tokens;

	// convert expression into list of tokens
	const char* e = expr.c_str();

	while (*e != 0)
	{
		if (*e == '*' || *e == '/' || *e == '+' || *e == '-' || *e == '%' || *e == '&' || *e == '|' || *e == '=' || *e == '(' || *e == ')' || *e == ',' || *e == ';' || *e == '<' || *e == '>')
		{
			if (bufferLoc != 0)
				tokens.push_back({ TokenType::Identifier, std::string(buffer) });

			memset(buffer, 0, 512);
			bufferLoc = 0;

			if (*e == '(' || *e == ')')
				tokens.push_back({ TokenType::Parenthesis, std::string(e, 1) });
			else if (*e == ',')
				tokens.push_back({ TokenType::Comma, "," });
			else if (*e == ';')
				tokens.push_back({ TokenType::Semicolon, ";" });
			else
				tokens.push_back({ TokenType::Operator, std::string(e, 1) });
		}
		else if (*e == '{' || *e == '}')
		{
			break;
		}
		else if (*e == '\n' || *e == '\r' || *e == ' ' || *e == '\t')
		{
			// empty the buffer if needed
			if (bufferLoc != 0)
			{
				tokens.push_back({ TokenType::Identifier, std::string(buffer) });

				memset(buffer, 0, 512);
				bufferLoc = 0;
			}
		}
		else
		{
			buffer[bufferLoc] = *e;
			bufferLoc++;
		}

		e++;
	}

	// empty the buffer
	if (bufferLoc != 0)
		tokens.push_back({ TokenType::Identifier, std::string(buffer) });

	// some "post processing"
	int multilineComment = 0;
	for (int i = 0; i < tokens.size(); i++)
	{
		if (tokens[i].Type == TokenType::Identifier)
		{
			if (tokens[i].Content.size() > 0)
			{
				if (tokens[i].Content[0] == '.' || isdigit(tokens[i].Content[0]))
					tokens[i].Type = TokenType::Number;
				else if (tokens[i].Content == "true" || tokens[i].Content == "false")
					tokens[i].Type = TokenType::Number;
			}
		}
		else if (i != 0 && tokens[i].Type == TokenType::Operator)
		{
			if (tokens[i - 1].Type == TokenType::Operator)
			{
				// comment
				if (tokens[i].Content == "/" && tokens[i - 1].Content == "/")
				{
					tokens.erase(tokens.begin() + i - 1, tokens.end());
					break;
				}
				else if (tokens[i - 1].Content == "/" && tokens[i].Content == "*")
				{
					multilineComment = i - 1;
				}
				else if (tokens[i - 1].Content == "*" && tokens[i].Content == "/")
				{
					tokens.erase(tokens.begin() + multilineComment, tokens.begin() + i + 1);
					i -= (i - multilineComment) - 1;
					multilineComment = 0;
					continue;
				}
				else
				{
					// &&, <=, ...
					tokens[i - 1].Content += tokens[i].Content;
					tokens.erase(tokens.begin() + i);
					i--;
					continue;
				}
			}
		}
	}

	// 1. get all the identifiers (highest priority)
	for (int i = 0; i < tokens.size(); i++)
	{
		if (tokens[i].Type == TokenType::Identifier)
		{
			if (i == tokens.size() - 1 || tokens[i + 1].Content != "(")
			{
				if (std::count(ret.begin(), ret.end(), tokens[i].Content) == 0)
					ret.push_back(tokens[i].Content);
			}
		}
	}

	// 2. get all the function calls, their arguments and expressions
	std::stack<std::string> funcStack;
	std::stack<std::string> argStack;
	std::string exprBuffer = "";
	int exprParenthesis = 0;
	int forSection = -1;
	for (int i = 0; i < tokens.size(); i++)
	{
		if ((forSection == -1 || forSection == 1) && tokens[i].Type == TokenType::Identifier && i + 1 < tokens.size() && tokens[i + 1].Content == "(")
		{
			if (tokens[i].Content == "if" || tokens[i].Content == "for" || tokens[i].Content == "while")
			{
				if (tokens[i].Content == "for")
					forSection = 0;
				else
					i++; // skip '('

				continue;
			}

			funcStack.push(tokens[i].Content + "(");
			argStack.push("");
			i++;

			continue;
		}
		else if ((forSection == -1 || forSection == 1) && (tokens[i].Type == TokenType::Comma || tokens[i].Content == ")") && !argStack.empty() && !funcStack.empty())
		{
			funcStack.top() += argStack.top().substr(0, argStack.top().size() - 1) + tokens[i].Content;

			if (tokens[i].Content == ")")
			{
				std::string topFunc = funcStack.top();
				funcStack.pop();

				if (!argStack.top().empty())
					ret.push_back(argStack.top().substr(0, argStack.top().size() - 1));

				argStack.pop();
				
				if (!argStack.empty())
					argStack.top() += topFunc + " ";

				ret.push_back(topFunc);

				if (funcStack.empty())
					exprBuffer += topFunc + " ";
			}
			else if (tokens[i].Type == TokenType::Comma)
			{
				funcStack.top() += " ";
				ret.push_back(argStack.top().substr(0, argStack.top().size() - 1));
				argStack.top() = "";
			}
		}
		else if (tokens[i].Type == TokenType::Semicolon)
		{
			if (forSection != -1)
			{
				if (forSection == 1 && !exprBuffer.empty())
				{
					ret.push_back(exprBuffer);
					exprBuffer.clear();
					exprParenthesis = 0;
				}

				forSection++;
			}
		}
		else if (forSection == -1 || forSection == 1)
		{
			if (tokens[i].Content == "(")
				exprParenthesis++;
			else if (tokens[i].Content == ")")
				exprParenthesis--;

			if (!argStack.empty())
			{
				argStack.top() += tokens[i].Content + " ";
			}
			else if (exprParenthesis < 0)
			{
				if (!exprBuffer.empty())
					ret.push_back(exprBuffer.substr(0, exprBuffer.size() - 1));
				exprBuffer.clear();
				exprParenthesis = 0;
			}
			else if (tokens[i].Type == TokenType::Operator && (tokens[i].Content.size() >= 2 || tokens[i].Content == "="))
			{
				if (!exprBuffer.empty())
					ret.push_back(exprBuffer.substr(0, exprBuffer.size() - 1));

				exprBuffer.clear();
				exprParenthesis = 0;
			}
			else
			{
				bool isKeyword = false;

				for (const auto& kwd : m_LanguageDefinition.Keywords)
				{
					if (kwd == tokens[i].Content)
					{
						isKeyword = true;
						break;
					}
				}
				
				if (!isKeyword)
					exprBuffer += tokens[i].Content + " ";
			}
		}
	}

	if (!exprBuffer.empty())
		ret.push_back(exprBuffer.substr(0, exprBuffer.size() - 1));

	// remove duplicates, numbers & keywords
	for (int i = 0; i < ret.size(); i++)
	{
		std::string r = ret[i];
		bool eraseR = false;

		// empty or duplicate
		if (ret.empty() || std::count(ret.begin(), ret.begin() + i, r) > 0)
			eraseR = true;

		// boolean
		if (r == "true" || r == "false")
			eraseR = true;

		// number
		bool isNumber = true;

		for (int i = 0; i < r.size(); i++)
		{
			if (!isdigit(r[i]) && r[i] != '.' && r[i] != 'f')
			{
				isNumber = false;
				break;
			}
		}

		if (isNumber)
			eraseR = true;

		// keyword
		if (!eraseR)
		{
			for (const auto& ident : m_LanguageDefinition.Identifiers)
			{
				if (ident.first == r)
				{
					eraseR = true;
					break;
				}
			}

			for (const auto& kwd : m_LanguageDefinition.Keywords)
			{
				if (kwd == r)
				{
					eraseR = true;
					break;
				}
			}
		}

		// delete it from the array
		if (eraseR)
		{
			ret.erase(ret.begin() + i);
			i--;

			continue;
		}
	}

	return ret;
}

const ImTextEdit::td_Palette & ImTextEdit::GetDarkPalette()
{
	const static td_Palette p =
	{
		{
			0xff7f7f7f,	// Default
			0xffd69c56,	// Keyword	
			0xff00ff00,	// Number
			0xff7070e0,	// String
			0xff70a0e0, // Char literal
			0xffffffff, // Punctuation
			0xff408080,	// Preprocessor
			0xffaaaaaa, // Identifier
			0xff9bc64d, // Known identifier
			0xffc040a0, // Preproc identifier
			0xff206020, // Comment (single line)
			0xff406020, // Comment (multi line)
			0xff101010, // Background
			0xffe0e0e0, // Cursor
			0x80a06020, // Selection
			0x800020ff, // ErrorMarker
			0xff0000ff, // Breakpoint
			0xffffffff, // Breakpoint outline
			0xFF1DD8FF, // Current line indicator
			0xFF696969, // Current line indicator outline
			0xff707000, // Line number
			0x40000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40a0a0a0, // Current line edge
			0xff33ffff, // Error message
			0xffffffff, // BreakpointDisabled
			0xffaaaaaa, // UserFunction
			0xffb0c94e, // UserType
			0xffaaaaaa, // UniformType
			0xffaaaaaa, // GlobalVariable
			0xffaaaaaa, // LocalVariable
			0xff888888	// FunctionArgument
		}
	};

	return p;
}

const ImTextEdit::td_Palette & ImTextEdit::GetLightPalette()
{
	const static td_Palette p =
	{
		{
			0xff7f7f7f,	// None
			0xffff0c06,	// Keyword	
			0xff008000,	// Number
			0xff2020a0,	// String
			0xff304070, // Char literal
			0xff000000, // Punctuation
			0xff406060,	// Preprocessor
			0xff404040, // Identifier
			0xff606010, // Known identifier
			0xffc040a0, // Preproc identifier
			0xff205020, // Comment (single line)
			0xff405020, // Comment (multi line)
			0xffffffff, // Background
			0xff000000, // Cursor
			0x80DFBF80, // Selection
			0xa00010ff, // ErrorMarker
			0xff0000ff, // Breakpoint
			0xff000000, // Breakpoint outline
			0xFF1DD8FF, // Current line indicator
			0xFF696969, // Current line indicator outline
			0xff505000, // Line number
			0x20000000, // Current line fill
			0x20808080, // Current line fill (inactive)
			0x30000000, // Current line edge
			0xff3333ff, // Error message
			0xffffffff, // BreakpointDisabled
			0xff404040, // UserFunction
			0xffb0912b, // UserType
			0xff404040, // UniformType
			0xff404040, // GlobalVariable
			0xff404040, // LocalVariable
			0xff606060	// FunctionArgument
		}
	};

	return p;
}

const ImTextEdit::td_Palette & ImTextEdit::GetRetroBluePalette()
{
	const static td_Palette p =
	{
		{
			0xff00ffff,	// None
			0xffffff00,	// Keyword	
			0xff00ff00,	// Number
			0xff808000,	// String
			0xff808000, // Char literal
			0xffffffff, // Punctuation
			0xff008000,	// Preprocessor
			0xff00ffff, // Identifier
			0xffffffff, // Known identifier
			0xffff00ff, // Preproc identifier
			0xff808080, // Comment (single line)
			0xff404040, // Comment (multi line)
			0xff800000, // Background
			0xff0080ff, // Cursor
			0x80ffff00, // Selection
			0xa00000ff, // ErrorMarker
			0xff0000ff, // Breakpoint
			0xffffffff, // Breakpoint outline
			0xFF1DD8FF, // Current line indicator
			0xff808000, // Line number
			0x40000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40000000, // Current line edge
			0xffffff00, // Error message
			0xffffffff, // BreakpointDisabled
			0xff00ffff, // UserFunction
			0xff00ffff, // UserType
			0xff00ffff, // UniformType
			0xff00ffff, // GlobalVariable
			0xff00ffff, // LocalVariable
			0xff00ffff	// FunctionArgument
		}
	};

	return p;
}


std::string ImTextEdit::GetText() const
{
	return GetText(Coordinates(), Coordinates((int)m_Lines.size(), 0));
}

void ImTextEdit::GetTextLines(std::vector<std::string>& result) const
{
	result.reserve(m_Lines.size());

	for (auto & line : m_Lines)
	{
		std::string text;
		text.resize(line.size());

		for (size_t i = 0; i < line.size(); ++i)
			text[i] = line[i].Character;

		result.emplace_back(std::move(text));
	}
}

std::string ImTextEdit::GetSelectedText() const
{
	return GetText(m_State.SelectionStart, m_State.SelectionEnd);
}

std::string ImTextEdit::GetCurrentLineText()const
{
	auto lineLength = GetLineMaxColumn(m_State.CursorPosition.Line);
	
	return GetText(Coordinates(m_State.CursorPosition.Line, 0), Coordinates(m_State.CursorPosition.Line, lineLength));
}

void ImTextEdit::ProcessInputs()
{
}

void ImTextEdit::Colorize(int aFromLine, int aLines)
{
	int toLine = aLines == -1 ? (int)m_Lines.size() : std::min<int>((int)m_Lines.size(), aFromLine + aLines);
	m_ColorRangeMin = std::min<int>(m_ColorRangeMin, aFromLine);
	m_ColorRangeMax = std::max<int>(m_ColorRangeMax, toLine);
	m_ColorRangeMin = std::max<int>(0, m_ColorRangeMin);
	m_ColorRangeMax = std::max<int>(m_ColorRangeMin, m_ColorRangeMax);
	m_CheckComments = true;
}

void ImTextEdit::ColorizeRange(int aFromLine, int aToLine)
{
	if (m_Lines.empty() || !m_ColorizerEnabled)
		return;

	std::string buffer;
	std::cmatch results;
	std::string id;

	int endLine = std::max(0, std::min((int)m_Lines.size(), aToLine));

	for (int i = aFromLine; i < endLine; ++i)
	{
		auto& line = m_Lines[i];

		if (line.empty())
			continue;

		buffer.resize(line.size());

		for (size_t j = 0; j < line.size(); ++j)
		{
			auto& col = line[j];
			buffer[j] = col.Character;
			col.ColorIndex = PaletteIndex::Default;
		}

		const char* bufferBegin = &buffer.front();
		const char* bufferEnd = bufferBegin + buffer.size();

		auto last = bufferEnd;

		for (auto first = bufferBegin; first != last;)
		{
			const char* token_begin = nullptr;
			const char* token_end = nullptr;
			PaletteIndex token_color = PaletteIndex::Default;

			bool hasTokenizeResult = false;

			if (m_LanguageDefinition.Tokenize != nullptr)
			{
				if (m_LanguageDefinition.Tokenize(first, last, token_begin, token_end, token_color))
					hasTokenizeResult = true;
			}

			if (hasTokenizeResult == false)
			{
				// todo : remove
					//printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

				for (auto& p : m_RegexList)
				{
					if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous))
					{
						hasTokenizeResult = true;

						auto& v = *results.begin();
						token_begin = v.first;
						token_end = v.second;
						token_color = p.second;
						break;
					}
				}
			}

			if (hasTokenizeResult == false)
			{
				first++;
			}
			else
			{
				const size_t token_length = token_end - token_begin;

				if (token_color == PaletteIndex::Identifier)
				{
					id.assign(token_begin, token_end);

					// todo : almost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
					if (!m_LanguageDefinition.CaseSensitive)
						std::transform(id.begin(), id.end(), id.begin(), ::toupper);

					if (!line[first - bufferBegin].Preprocessor)
					{
						if (m_LanguageDefinition.Keywords.count(id) != 0)
							token_color = PaletteIndex::Keyword;
						else if (m_LanguageDefinition.Identifiers.count(id) != 0)
							token_color = PaletteIndex::KnownIdentifier;
						else if (m_LanguageDefinition.PreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
					else
					{
						if (m_LanguageDefinition.PreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
				}

				for (size_t j = 0; j < token_length; ++j)
					line[(token_begin - bufferBegin) + j].ColorIndex = token_color;

				first = token_end;
			}
		}
	}
}

void ImTextEdit::ColorizeInternal()
{
	if (m_Lines.empty() || !m_ColorizerEnabled)
		return;

	if (m_CheckComments)
	{
		auto endLine = m_Lines.size();
		auto endIndex = 0;
		auto commentStartLine = endLine;
		auto commentStartIndex = endIndex;
		auto withinString = false;
		auto withinSingleLineComment = false;
		auto withinPreproc = false;
		auto firstChar = true;			// there is no other non-whitespace characters in the line before
		auto concatenate = false;		// '\' on the very end of the line
		auto currentLine = 0;
		auto currentIndex = 0;

		while (currentLine < endLine || currentIndex < endIndex)
		{
			auto& line = m_Lines[currentLine];

			if (currentIndex == 0 && !concatenate)
			{
				withinSingleLineComment = false;
				withinPreproc = false;
				firstChar = true;
			}

			concatenate = false;

			if (!line.empty())
			{
				auto& g = line[currentIndex];
				auto c = g.Character;

				if (c != m_LanguageDefinition.PreprocChar && !isspace(c))
					firstChar = false;

				if (currentIndex == (int)line.size() - 1 && line[line.size() - 1].Character == '\\')
					concatenate = true;

				bool inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

				if (withinString)
				{
					line[currentIndex].MultiLineComment = inComment;

					if (c == '\"')
					{
						if (currentIndex + 1 < (int)line.size() && line[currentIndex + 1].Character == '\"')
						{
							currentIndex += 1;
							if (currentIndex < (int)line.size())
								line[currentIndex].MultiLineComment = inComment;
						}
						else
						{
							withinString = false;
						}
					}
					else if (c == '\\')
					{
						currentIndex += 1;

						if (currentIndex < (int)line.size())
							line[currentIndex].MultiLineComment = inComment;
					}
				}
				else
				{
					if (firstChar && c == m_LanguageDefinition.PreprocChar)
						withinPreproc = true;

					if (c == '\"')
					{
						withinString = true;
						line[currentIndex].MultiLineComment = inComment;
					}
					else
					{
						auto pred = [](const char& a, const Glyph& b) { return a == b.Character; };
						auto from = line.begin() + currentIndex;
						auto& startStr = m_LanguageDefinition.CommentStart;
						auto& singleStartStr = m_LanguageDefinition.SingleLineComment;

						if (singleStartStr.size() > 0 && currentIndex + singleStartStr.size() <= line.size() && Equals(singleStartStr.begin(), singleStartStr.end(), from, from + singleStartStr.size(), pred))
						{
							withinSingleLineComment = true;
						}
						else if (!withinSingleLineComment && currentIndex + startStr.size() <= line.size() && Equals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred))
						{
							commentStartLine = currentLine;
							commentStartIndex = currentIndex;
						}

						inComment = inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

						line[currentIndex].MultiLineComment = inComment;
						line[currentIndex].Comment = withinSingleLineComment;

						auto& endStr = m_LanguageDefinition.CommentEnd;

						if (currentIndex + 1 >= (int)endStr.size() && Equals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred))
						{
							commentStartIndex = endIndex;
							commentStartLine = endLine;
						}
					}
				}
				line[currentIndex].Preprocessor = withinPreproc;
				currentIndex += UTF8CharLength(c);

				if (currentIndex >= (int)line.size())
				{
					currentIndex = 0;
					++currentLine;
				}
			}
			else
			{
				currentIndex = 0;
				++currentLine;
			}
		}

		m_CheckComments = false;
	}

	if (m_ColorRangeMin < m_ColorRangeMax)
	{
		const int increment = (m_LanguageDefinition.Tokenize == nullptr) ? 10 : 10000;
		const int to = std::min<int>(m_ColorRangeMin + increment, m_ColorRangeMax);
		ColorizeRange(m_ColorRangeMin, to);
		m_ColorRangeMin = to;

		if (m_ColorRangeMax == m_ColorRangeMin)
		{
			m_ColorRangeMin = std::numeric_limits<int>::max();
			m_ColorRangeMax = 0;
		}

		return;
	}
}

float ImTextEdit::TextDistanceToLineStart(const Coordinates& aFrom) const
{
	auto& line = m_Lines[aFrom.Line];
	float distance = 0.0f;
	float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
	int colIndex = GetCharacterIndex(aFrom);

	for (size_t it = 0u; it < line.size() && it < colIndex;)
	{
		if (line[it].Character == '\t')
		{
			distance = (1.0f + std::floor((1.0f + distance) / (float(m_TabSize) * spaceSize))) * (float(m_TabSize) * spaceSize);
			++it;
		}
		else
		{
			auto d = UTF8CharLength(line[it].Character);
			char tempCString[7];
			int i = 0;

			for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++)
				tempCString[i] = line[it].Character;

			tempCString[i] = '\0';
			distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
		}
	}

	return distance;
}

void ImTextEdit::EnsureCursorVisible()
{
	if (!m_WithinRender)
	{
		m_ScrollToCursor = true;
		return;
	}

	float scrollX = ImGui::GetScrollX();
	float scrollY = ImGui::GetScrollY();

	auto height = ImGui::GetWindowHeight();
	auto width = m_WindowWidth;
	
	auto top = 1 + (int)ceil(scrollY / m_CharAdvance.y);
	auto bottom = (int)ceil((scrollY + height) / m_CharAdvance.y);

	auto left = (int)ceil(scrollX / m_CharAdvance.x);
	auto right = (int)ceil((scrollX + width) / m_CharAdvance.x);

	auto pos = GetActualCursorCoordinates();
	auto len = TextDistanceToLineStart(pos);

	if (pos.Line < top)
		ImGui::SetScrollY(std::max(0.0f, (pos.Line - 1) * m_CharAdvance.y));

	if (pos.Line > bottom - 4)
		ImGui::SetScrollY(std::max(0.0f, (pos.Line + 4) * m_CharAdvance.y - height));
	
	if (pos.Column < left)
		ImGui::SetScrollX(std::max(0.0f, len + m_TextStart - 11 * m_CharAdvance.x));
	
	if (len + m_TextStart > (right - 4) * m_CharAdvance.x)
		ImGui::SetScrollX(std::max(0.0f, len + m_TextStart + 4 * m_CharAdvance.x - width));
}
void ImTextEdit::SetCurrentLineIndicator(int line, bool displayBar)
{
	m_DebugCurrentLine = line;
	m_DebugCurrentLineUpdated = line > 0;
	m_DebugBar = displayBar;
}

int ImTextEdit::GetPageSize() const
{
	auto height = ImGui::GetWindowHeight() - 20.0f;
	return (int)floor(height / m_CharAdvance.y);
}

ImTextEdit::UndoRecord::UndoRecord(const std::string& aAdded, const ImTextEdit::Coordinates aAddedStart, const ImTextEdit::Coordinates aAddedEnd, const std::string& aRemoved,
	const ImTextEdit::Coordinates aRemovedStart, const ImTextEdit::Coordinates aRemovedEnd, ImTextEdit::EditorState& aBefore, ImTextEdit::EditorState& aAfter)
	: Added(aAdded), AddedStart(aAddedStart), AddedEnd(aAddedEnd), Removed(aRemoved), RemovedStart(aRemovedStart), RemovedEnd(aRemovedEnd), Before(aBefore), After(aAfter)
{
	assert(AddedStart <= AddedEnd);
	assert(RemovedStart <= RemovedEnd);
}

void ImTextEdit::UndoRecord::Undo(ImTextEdit * aEditor)
{
	if (!Added.empty())
	{
		aEditor->DeleteRange(AddedStart, AddedEnd);
		aEditor->Colorize(AddedStart.Line - 1, AddedEnd.Line - AddedStart.Line + 2);
	}

	if (!Removed.empty())
	{
		auto start = RemovedStart;
		aEditor->InsertTextAt(start, Removed.c_str());
		aEditor->Colorize(RemovedStart.Line - 1, RemovedEnd.Line - RemovedStart.Line + 2);
	}

	aEditor->m_State = Before;
	aEditor->EnsureCursorVisible();

}

void ImTextEdit::UndoRecord::Redo(ImTextEdit * aEditor)
{
	if (!Removed.empty())
	{
		aEditor->DeleteRange(RemovedStart, RemovedEnd);
		aEditor->Colorize(RemovedStart.Line - 1, RemovedEnd.Line - RemovedStart.Line + 1);
	}

	if (!Added.empty())
	{
		auto start = AddedStart;
		aEditor->InsertTextAt(start, Added.c_str());
		aEditor->Colorize(AddedStart.Line - 1, AddedEnd.Line - AddedStart.Line + 1);
	}

	aEditor->m_State = After;
	aEditor->EnsureCursorVisible();
}

static bool TokenizeCStyleString(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	if (*p == '"')
	{
		p++;

		while (p < in_end)
		{
			// handle end of string
			if (*p == '"')
			{
				out_begin = in_begin;
				out_end = p + 1;

				return true;
			}

			// handle escape character for "
			if (*p == '\\' && p + 1 < in_end && p[1] == '"')
				p++;

			p++;
		}
	}

	return false;
}

static bool TokenizeCStyleCharacterLiteral(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	if (*p == '\'')
	{
		p++;

		// handle escape characters
		if (p < in_end && *p == '\\')
			p++;

		if (p < in_end)
			p++;

		// handle end of character literal
		if (p < in_end && *p == '\'')
		{
			out_begin = in_begin;
			out_end = p + 1;

			return true;
		}
	}

	return false;
}

static bool TokenizeCStyleIdentifier(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
	{
		p++;

		while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;

		out_begin = in_begin;
		out_end = p;

		return true;
	}

	return false;
}

static bool TokenizeCStyleNumber(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	const bool startsWithNumber = *p >= '0' && *p <= '9';

	if (*p != '+' && *p != '-' && !startsWithNumber)
		return false;

	p++;

	bool hasNumber = startsWithNumber;

	while (p < in_end && (*p >= '0' && *p <= '9'))
	{
		hasNumber = true;

		p++;
	}

	if (hasNumber == false)
		return false;

	bool isFloat = false;
	bool isHex = false;
	bool isBinary = false;

	if (p < in_end)
	{
		if (*p == '.')
		{
			isFloat = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '9'))
				p++;
		}
		else if (*p == 'x' || *p == 'X')
		{
			// hex formatted integer of the type 0xef80

			isHex = true;

			p++;

			while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
				p++;
		}
		else if (*p == 'b' || *p == 'B')
		{
			// binary formatted integer of the type 0b01011101

			isBinary = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '1'))
				p++;
		}
	}

	if (isHex == false && isBinary == false)
	{
		// floating point exponent
		if (p < in_end && (*p == 'e' || *p == 'E'))
		{
			isFloat = true;

			p++;

			if (p < in_end && (*p == '+' || *p == '-'))
				p++;

			bool hasDigits = false;

			while (p < in_end && (*p >= '0' && *p <= '9'))
			{
				hasDigits = true;

				p++;
			}

			if (hasDigits == false)
				return false;
		}

		// single precision floating point type
		if (p < in_end && *p == 'f')
			p++;
	}

	if (isFloat == false)
	{
		// integer size type
		while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L'))
			p++;
	}

	out_begin = in_begin;
	out_end = p;

	return true;
}

static bool TokenizeCStylePunctuation(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	(void)in_end;

	switch (*in_begin)
	{
	case '[':
	case ']':
	case '{':
	case '}':
	case '!':
	case '%':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '+':
	case '=':
	case '~':
	case '|':
	case '<':
	case '>':
	case '?':
	case ':':
	case '/':
	case ';':
	case ',':
	case '.':
		out_begin = in_begin;
		out_end = in_begin + 1;
		return true;
	}

	return false;
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::CPlusPlus()
{
	static bool inited = false;
	static LanguageDefinition langDef;

	if (!inited)
	{
		static const char* const cppKeywords[] = {
			"alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
			"compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
			"for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
			"register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local",
			"throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
		};

		for (auto& k : cppKeywords)
			langDef.Keywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper",
			"std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
		};
		
		for (auto& k : identifiers)
		{
			Identifier id;
			id.Declaration = "Built-in function";
			langDef.Identifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.Tokenize = [](const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end, PaletteIndex & paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::String;
			}
			else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::CharLiteral;
			}
			else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::Identifier;
			}
			else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::Number;
			}
			else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::Punctuation;
			}

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.CommentStart = "/*";
		langDef.CommentEnd = "*/";
		langDef.SingleLineComment = "//";

		langDef.CaseSensitive = true;
		langDef.AutoIndentation = true;

		langDef.Name = "C++";

		inited = true;
	}

	return langDef;
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::HLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	
	if (!inited)
	{
		static const char* const keywords[] = {
			"AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment",
			"CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else",
			"export", "extern", "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj",
			"linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset",
			"pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",
			"RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state",
			"static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS",
			"Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment",
			"VertexShader", "void", "volatile", "while",
			"bool1","bool2","bool3","bool4","double1","double2","double3","double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout",
			"uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
			"float1x1","float2x1","float3x1","float4x1","float1x2","float2x2","float3x2","float4x2",
			"float1x3","float2x3","float3x3","float4x3","float1x4","float2x4","float3x4","float4x4",
			"half1x1","half2x1","half3x1","half4x1","half1x2","half2x2","half3x2","half4x2",
			"half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4",
			"SHADERED_WEB", "SHADERED_DESKTOP", "SHADERED_VERSION"
		};

		for (auto& k : keywords)
			langDef.Keywords.insert(k);

		HLSLDocumentation(langDef.Identifiers);

		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.CommentStart = "/*";
		langDef.CommentEnd = "*/";
		langDef.SingleLineComment = "//";

		langDef.CaseSensitive = true;
		langDef.AutoIndentation = true;

		langDef.Name = "HLSL";

		inited = true;
	}

	return langDef;
}
void ImTextEdit::LanguageDefinition::HLSLDocumentation(td_Identifiers& idents)
{
	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("abort", Identifier("Terminates the current draw or dispatch call being executed.")));
	idents.insert(std::make_pair("abs", Identifier("Absolute value (per component).")));
	idents.insert(std::make_pair("acos", Identifier("Returns the arccosine of each component of x.")));
	idents.insert(std::make_pair("all", Identifier("Test if all components of x are nonzero.")));
	idents.insert(std::make_pair("AllMemoryBarrier", Identifier("Blocks execution of all threads in a group until all memory accesses have been completed.")));
	idents.insert(std::make_pair("AllMemoryBarrierWithGroupSync", Identifier("Blocks execution of all threads in a group until all memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("any", Identifier("Test if any component of x is nonzero.")));
	idents.insert(std::make_pair("asdouble", Identifier("Reinterprets a cast value into a double.")));
	idents.insert(std::make_pair("asfloat", Identifier("Convert the input type to a float.")));
	idents.insert(std::make_pair("asin", Identifier("Returns the arcsine of each component of x.")));
	idents.insert(std::make_pair("asint", Identifier("Convert the input type to an integer.")));
	idents.insert(std::make_pair("asuint", Identifier("Convert the input type to an unsigned integer.")));
	idents.insert(std::make_pair("atan", Identifier("Returns the arctangent of x.")));
	idents.insert(std::make_pair("atan2", Identifier("Returns the arctangent of of two values (x,y).")));
	idents.insert(std::make_pair("ceil", Identifier("Returns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("CheckAccessFullyMapped", Identifier("Determines whether all values from a Sample or Load operation accessed mapped tiles in a tiled resource.")));
	idents.insert(std::make_pair("clamp", Identifier("Clamps x to the range [min, max].")));
	idents.insert(std::make_pair("clip", Identifier("Discards the current pixel, if any component of x is less than zero.")));
	idents.insert(std::make_pair("cos", Identifier("Returns the cosine of x.")));
	idents.insert(std::make_pair("cosh", Identifier("Returns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("countbits", Identifier("Counts the number of bits (per component) in the input integer.")));
	idents.insert(std::make_pair("cross", Identifier("Returns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("D3DCOLORtoUBYTE4", Identifier("Swizzles and scales components of the 4D vector x to compensate for the lack of UBYTE4 support in some hardware.")));
	idents.insert(std::make_pair("ddx", Identifier("Returns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_coarse", Identifier("Computes a low precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_fine", Identifier("Computes a high precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddy", Identifier("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_coarse", Identifier("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_fine", Identifier("Computes a high precision partial derivative with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("degrees", Identifier("Converts x from radians to degrees.")));
	idents.insert(std::make_pair("determinant", Identifier("Returns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("DeviceMemoryBarrier", Identifier("Blocks execution of all threads in a group until all device memory accesses have been completed.")));
	idents.insert(std::make_pair("DeviceMemoryBarrierWithGroupSync", Identifier("Blocks execution of all threads in a group until all device memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("distance", Identifier("Returns the distance between two points.")));
	idents.insert(std::make_pair("dot", Identifier("Returns the dot product of two vectors.")));
	idents.insert(std::make_pair("dst", Identifier("Calculates a distance vector.")));
	idents.insert(std::make_pair("errorf", Identifier("Submits an error message to the information queue.")));
	idents.insert(std::make_pair("EvaluateAttributeAtCentroid", Identifier("Evaluates at the pixel centroid.")));
	idents.insert(std::make_pair("EvaluateAttributeAtSample", Identifier("Evaluates at the indexed sample location.")));
	idents.insert(std::make_pair("EvaluateAttributeSnapped", Identifier("Evaluates at the pixel centroid with an offset.")));
	idents.insert(std::make_pair("exp", Identifier("Returns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", Identifier("Base 2 exponent(per component).")));
	idents.insert(std::make_pair("f16tof32", Identifier("Converts the float16 stored in the low-half of the uint to a float.")));
	idents.insert(std::make_pair("f32tof16", Identifier("Converts an input into a float16 type.")));
	idents.insert(std::make_pair("faceforward", Identifier("Returns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("firstbithigh", Identifier("Gets the location of the first set bit starting from the highest order bit and working downward, per component.")));
	idents.insert(std::make_pair("firstbitlow", Identifier("Returns the location of the first set bit starting from the lowest order bit and working upward, per component.")));
	idents.insert(std::make_pair("floor", Identifier("Returns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("fma", Identifier("Returns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("fmod", Identifier("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("frac", Identifier("Returns the fractional part of x.")));
	idents.insert(std::make_pair("frexp", Identifier("Returns the mantissa and exponent of x.")));
	idents.insert(std::make_pair("fwidth", Identifier("Returns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("GetRenderTargetSampleCount", Identifier("Returns the number of render-target samples.")));
	idents.insert(std::make_pair("GetRenderTargetSamplePosition", Identifier("Returns a sample position (x,y) for a given sample index.")));
	idents.insert(std::make_pair("GroupMemoryBarrier", Identifier("Blocks execution of all threads in a group until all group shared accesses have been completed.")));
	idents.insert(std::make_pair("GroupMemoryBarrierWithGroupSync", Identifier("Blocks execution of all threads in a group until all group shared accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("InterlockedAdd", Identifier("Performs a guaranteed atomic add of value to the dest resource variable.")));
	idents.insert(std::make_pair("InterlockedAnd", Identifier("Performs a guaranteed atomic and.")));
	idents.insert(std::make_pair("InterlockedCompareExchange", Identifier("Atomically compares the input to the comparison value and exchanges the result.")));
	idents.insert(std::make_pair("InterlockedCompareStore", Identifier("Atomically compares the input to the comparison value.")));
	idents.insert(std::make_pair("InterlockedExchange", Identifier("Assigns value to dest and returns the original value.")));
	idents.insert(std::make_pair("InterlockedMax", Identifier("Performs a guaranteed atomic max.")));
	idents.insert(std::make_pair("InterlockedMin", Identifier("Performs a guaranteed atomic min.")));
	idents.insert(std::make_pair("InterlockedOr", Identifier("Performs a guaranteed atomic or.")));
	idents.insert(std::make_pair("InterlockedXor", Identifier("Performs a guaranteed atomic xor.")));
	idents.insert(std::make_pair("isfinite", Identifier("Returns true if x is finite, false otherwise.")));
	idents.insert(std::make_pair("isinf", Identifier("Returns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", Identifier("Returns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("ldexp", Identifier("Returns x * 2exp")));
	idents.insert(std::make_pair("length", Identifier("Returns the length of the vector v.")));
	idents.insert(std::make_pair("lerp", Identifier("Returns x + s(y - x).")));
	idents.insert(std::make_pair("lit", Identifier("Returns a lighting vector (ambient, diffuse, specular, 1)")));
	idents.insert(std::make_pair("log", Identifier("Returns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log10", Identifier("Returns the base-10 logarithm of x.")));
	idents.insert(std::make_pair("log2", Identifier("Returns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("mad", Identifier("Performs an arithmetic multiply/add operation on three values.")));
	idents.insert(std::make_pair("max", Identifier("Selects the greater of x and y.")));
	idents.insert(std::make_pair("min", Identifier("Selects the lesser of x and y.")));
	idents.insert(std::make_pair("modf", Identifier("Splits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("msad4", Identifier("Compares a 4-byte reference value and an 8-byte source value and accumulates a vector of 4 sums.")));
	idents.insert(std::make_pair("mul", Identifier("Performs matrix multiplication using x and y.")));
	idents.insert(std::make_pair("noise", Identifier("Generates a random value using the Perlin-noise algorithm.")));
	idents.insert(std::make_pair("normalize", Identifier("Returns a normalized vector.")));
	idents.insert(std::make_pair("pow", Identifier("Returns x^n.")));
	idents.insert(std::make_pair("printf", Identifier("Submits a custom shader message to the information queue.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsAvg", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMax", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMin", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessIsolineTessFactors", Identifier("Generates the rounded tessellation factors for an isoline.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsAvg", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMax", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMin", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsAvg", Identifier("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMax", Identifier("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMin", Identifier("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("radians", Identifier("Converts x from degrees to radians.")));
	idents.insert(std::make_pair("rcp", Identifier("Calculates a fast, approximate, per-component reciprocal.")));
	idents.insert(std::make_pair("reflect", Identifier("Returns a reflection vector.")));
	idents.insert(std::make_pair("refract", Identifier("Returns the refraction vector.")));
	idents.insert(std::make_pair("reversebits", Identifier("Reverses the order of the bits, per component.")));
	idents.insert(std::make_pair("round", Identifier("Rounds x to the nearest integer")));
	idents.insert(std::make_pair("rsqrt", Identifier("Returns 1 / sqrt(x)")));
	idents.insert(std::make_pair("saturate", Identifier("Clamps x to the range [0, 1]")));
	idents.insert(std::make_pair("sign", Identifier("Computes the sign of x.")));
	idents.insert(std::make_pair("sin", Identifier("Returns the sine of x")));
	idents.insert(std::make_pair("sincos", Identifier("Returns the sineand cosine of x.")));
	idents.insert(std::make_pair("sinh", Identifier("Returns the hyperbolic sine of x")));
	idents.insert(std::make_pair("smoothstep", Identifier("Returns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("sqrt", Identifier("Square root (per component)")));
	idents.insert(std::make_pair("step", Identifier("Returns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("tan", Identifier("Returns the tangent of x")));
	idents.insert(std::make_pair("tanh", Identifier("Returns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("tex1D", Identifier("1D texture lookup.")));
	idents.insert(std::make_pair("tex1Dbias", Identifier("1D texture lookup with bias.")));
	idents.insert(std::make_pair("tex1Dgrad", Identifier("1D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex1Dlod", Identifier("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex1Dproj", Identifier("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex2D", Identifier("2D texture lookup.")));
	idents.insert(std::make_pair("tex2Dbias", Identifier("2D texture lookup with bias.")));
	idents.insert(std::make_pair("tex2Dgrad", Identifier("2D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex2Dlod", Identifier("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex2Dproj", Identifier("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex3D", Identifier("3D texture lookup.")));
	idents.insert(std::make_pair("tex3Dbias", Identifier("3D texture lookup with bias.")));
	idents.insert(std::make_pair("tex3Dgrad", Identifier("3D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex3Dlod", Identifier("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex3Dproj", Identifier("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texCUBE", Identifier("Cube texture lookup.")));
	idents.insert(std::make_pair("texCUBEbias", Identifier("Cube texture lookup with bias.")));
	idents.insert(std::make_pair("texCUBEgrad", Identifier("Cube texture lookup with a gradient.")));
	idents.insert(std::make_pair("texCUBElod", Identifier("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("texCUBEproj", Identifier("Cube texture lookup with projective divide.")));
	idents.insert(std::make_pair("transpose", Identifier("Returns the transpose of the matrix m.")));
	idents.insert(std::make_pair("trunc", Identifier("Truncates floating-point value(s) to integer value(s)")));
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::GLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;

	if (!inited)
	{
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local", "attribute", "uniform", "varying", "layout", "centroid", "flat", "smooth", "noperspective", "patch", "sample", "subroutine", "in", "out", "inout",
			"bool", "true", "false", "invariant", "mat2", "mat3", "mat4", "dmat2", "dmat3", "dmat4", "mat2x2", "mat2x3", "mat2x4", "dmat2x2", "dmat2x3", "dmat2x4", "mat3x2", "mat3x3", "mat3x4", "dmat3x2", "dmat3x3", "dmat3x4",
			"mat4x2", "mat4x3", "mat4x4", "dmat4x2", "dmat4x3", "dmat4x4", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "dvec2", "dvec3", "dvec4", "uint", "uvec2", "uvec3", "uvec4",
			"lowp", "mediump", "highp", "precision", "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler1DShadow", "sampler2DShadow", "samplerCubeShadow", "sampler1DArray", "sampler2DArray", "sampler1DArrayShadow",
			"sampler2DArrayShadow", "isampler1D", "isampler2D", "isampler3D", "isamplerCube", "isampler1DArray", "isampler2DArray", "usampler1D", "usampler2D", "usampler3D", "usamplerCube", "usampler1DArray", "usampler2DArray",
			"sampler2DRect", "sampler2DRectShadow", "isampler2DRect", "usampler2DRect", "samplerBuffer", "isamplerBuffer", "usamplerBuffer", "sampler2DMS", "isampler2DMS", "usampler2DMS", "sampler2DMSArray", "isampler2DMSArray",
			"usampler2DMSArray", "samplerCubeArray", "samplerCubeArrayShadow", "isamplerCubeArray", "usamplerCubeArray",
			"SHADERED_WEB", "SHADERED_DESKTOP", "SHADERED_VERSION", "shared", "writeonly", "readonly", "image2D", "image1D", "image3D"
		};

		for (auto& k : keywords)
			langDef.Keywords.insert(k);

		GLSLDocumentation(langDef.Identifiers);

		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.CommentStart = "/*";
		langDef.CommentEnd = "*/";
		langDef.SingleLineComment = "//";

		langDef.CaseSensitive = true;
		langDef.AutoIndentation = true;

		langDef.Name = "GLSL";

		inited = true;
	}

	return langDef;
}
void ImTextEdit::LanguageDefinition::GLSLDocumentation(td_Identifiers& idents)
{
	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("radians", Identifier("genType radians(genType degrees)\nConverts x from degrees to radians.")));
	idents.insert(std::make_pair("degrees", Identifier("genType degrees(genType radians)\nConverts x from radians to degrees.")));
	idents.insert(std::make_pair("sin", Identifier("genType sin(genType angle)\nReturns the sine of x")));
	idents.insert(std::make_pair("cos", Identifier("genType cos(genType angle)\nReturns the cosine of x.")));
	idents.insert(std::make_pair("tan", Identifier("genType tan(genType angle)\nReturns the tangent of x")));
	idents.insert(std::make_pair("asin", Identifier("genType asin(genType x)\nReturns the arcsine of each component of x.")));
	idents.insert(std::make_pair("acos", Identifier("genType acos(genType x)\nReturns the arccosine of each component of x.")));
	idents.insert(std::make_pair("atan", Identifier("genType atan(genType y, genType x)\ngenType atan(genType y_over_x)\nReturns the arctangent of x.")));
	idents.insert(std::make_pair("sinh", Identifier("genType sinh(genType x)\nReturns the hyperbolic sine of x")));
	idents.insert(std::make_pair("cosh", Identifier("genType cosh(genType x)\nReturns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("tanh", Identifier("genType tanh(genType x)\nReturns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("asinh", Identifier("genType asinh(genType x)\nReturns the arc hyperbolic sine of x")));
	idents.insert(std::make_pair("acosh", Identifier("genType acosh(genType x)\nReturns the arc hyperbolic cosine of x.")));
	idents.insert(std::make_pair("atanh", Identifier("genType atanh(genType x)\nReturns the arc hyperbolic tangent of x")));
	idents.insert(std::make_pair("pow", Identifier("genType pow(genType x, genType n)\nReturns x^n.")));
	idents.insert(std::make_pair("exp", Identifier("genType exp(genType x)\nReturns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", Identifier("genType exp2(genType x)\nBase 2 exponent(per component).")));
	idents.insert(std::make_pair("log", Identifier("genType log(genType x)\nReturns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log2", Identifier("genType log2(genType x)\nReturns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("sqrt", Identifier("genType sqrt(genType x)\ngenDType sqrt(genDType x)\nSquare root (per component).")));
	idents.insert(std::make_pair("inversesqrt", Identifier("genType inversesqrt(genType x)\ngenDType inversesqrt(genDType x)\nReturns rcp(sqrt(x)).")));
	idents.insert(std::make_pair("abs", Identifier("genType abs(genType x)\ngenIType abs(genIType x)\ngenDType abs(genDType x)\nAbsolute value (per component).")));
	idents.insert(std::make_pair("sign", Identifier("genType sign(genType x)\ngenIType sign(genIType x)\ngenDType sign(genDType x)\nComputes the sign of x.")));
	idents.insert(std::make_pair("floor", Identifier("genType floor(genType x)\ngenDType floor(genDType x)\nReturns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("trunc", Identifier("genType trunc(genType x)\ngenDType trunc(genDType x)\nTruncates floating-point value(s) to integer value(s)")));
	idents.insert(std::make_pair("round", Identifier("genType round(genType x)\ngenDType round(genDType x)\nRounds x to the nearest integer")));
	idents.insert(std::make_pair("roundEven", Identifier("genType roundEven(genType x)\ngenDType roundEven(genDType x)\nReturns a value equal to the nearest integer to x. A fractional part of 0.5 will round toward the nearest even integer.")));
	idents.insert(std::make_pair("ceil", Identifier("genType ceil(genType x)\ngenDType ceil(genDType x)\nReturns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("fract", Identifier("genType fract(genType x)\ngenDType fract(genDType x)\nReturns the fractional part of x.")));
	idents.insert(std::make_pair("mod", Identifier("genType mod(genType x, float y)\ngenType mod(genType x, genType y)\ngenDType mod(genDType x, double y)\ngenDType mod(genDType x, genDType y)\nModulus. Returns x – y ∗ floor (x/y).")));
	idents.insert(std::make_pair("modf", Identifier("genType modf(genType x, out genType i)\ngenDType modf(genDType x, out genDType i)\nSplits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("max", Identifier("genType max(genType x, genType y)\ngenType max(genType x, float y)\nSelects the greater of x and y.")));
	idents.insert(std::make_pair("min", Identifier("genType min(genType x, genType y)\ngenType min(genType x, float y)\nSelects the lesser of x and y.")));
	idents.insert(std::make_pair("clamp", Identifier("genType clamp(genType x, genType minVal, genType maxVal)\ngenType clamp(genType x, float minVal, float maxVal)\nClamps x to the range [min, max].")));
	idents.insert(std::make_pair("mix", Identifier("genType mix(genType x, genType y, genType a)\ngenType mix(genType x, genType y, float a)\nReturns x*(1-a)+y*a.")));
	idents.insert(std::make_pair("isinf", Identifier("genBType isinf(genType x)\ngenBType isinf(genDType x)\nReturns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", Identifier("genBType isnan(genType x)\ngenBType isnan(genDType x)\nReturns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("smoothstep", Identifier("genType smoothstep(genType edge0, genType edge1, genType x)\ngenType smoothstep(float edge0, float edge1, genType x)\nReturns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("step", Identifier("genType step(genType edge, genType x)\ngenType step(float edge, genType x)\nReturns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("floatBitsToInt", Identifier("genIType floatBitsToInt(genType x)\nReturns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("floatBitsToUint", Identifier("genUType floatBitsToUint(genType x)\nReturns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("intBitsToFloat", Identifier("genType intBitsToFloat(genIType x)\nReturns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("uintBitsToFloat", Identifier("genType uintBitsToFloat(genUType x)\nReturns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("fmod", Identifier("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("fma", Identifier("genType fma(genType a, genType b, genType c)\nReturns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("ldexp", Identifier("genType ldexp(genType x, genIType exp)\nReturns x * 2exp")));
	idents.insert(std::make_pair("packUnorm2x16", Identifier("uint packUnorm2x16(vec2 v)\nFirst, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packUnorm4x8", Identifier("uint packUnorm4x8(vec4 v)\nFirst, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packSnorm4x8", Identifier("uint packUnorm4x8(vec4 v)\nFirst, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("unpackUnorm2x16", Identifier("vec2 unpackUnorm2x16(uint p)\nFirst, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackUnorm4x8", Identifier("vec4 unpackUnorm4x8(uint p)\nFirst, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackSnorm4x8", Identifier("vec4 unpackSnorm4x8(uint p)\nFirst, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("packDouble2x32", Identifier("double packDouble2x32(uvec2 v)\nReturns a double-precision value obtained by packing the components of v into a 64-bit value.")));
	idents.insert(std::make_pair("unpackDouble2x32", Identifier("uvec2 unpackDouble2x32(double d)\nReturns a two-component unsigned integer vector representation of v.")));
	idents.insert(std::make_pair("length", Identifier("float length(genType x)\nReturns the length of the vector v.")));
	idents.insert(std::make_pair("distance", Identifier("float distance(genType p0, genType p1)\nReturns the distance between two points.")));
	idents.insert(std::make_pair("dot", Identifier("float dot(genType x, genType y)\nReturns the dot product of two vectors.")));
	idents.insert(std::make_pair("cross", Identifier("vec3 cross(vec3 x, vec3 y)\nReturns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("normalize", Identifier("genType normalize(genType v)\nReturns a normalized vector.")));
	idents.insert(std::make_pair("faceforward", Identifier("genType faceforward(genType N, genType I, genType Nref)\nReturns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("reflect", Identifier("genType reflect(genType I, genType N)\nReturns a reflection vector.")));
	idents.insert(std::make_pair("refract", Identifier("genType refract(genType I, genType N, float eta)\nReturns the refraction vector.")));
	idents.insert(std::make_pair("matrixCompMult", Identifier("mat matrixCompMult(mat x, mat y)\nMultiply matrix x by matrix y component-wise.")));
	idents.insert(std::make_pair("outerProduct", Identifier("Linear algebraic matrix multiply c * r.")));
	idents.insert(std::make_pair("transpose", Identifier("mat transpose(mat m)\nReturns the transpose of the matrix m.")));
	idents.insert(std::make_pair("determinant", Identifier("float determinant(mat m)\nReturns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("inverse", Identifier("mat inverse(mat m)\nReturns a matrix that is the inverse of m.")));
	idents.insert(std::make_pair("lessThan", Identifier("bvec lessThan(vec x, vec y)\nReturns the component-wise compare of x < y")));
	idents.insert(std::make_pair("lessThanEqual", Identifier("bvec lessThanEqual(vec x, vec y)\nReturns the component-wise compare of x <= y")));
	idents.insert(std::make_pair("greaterThan", Identifier("bvec greaterThan(vec x, vec y)\nReturns the component-wise compare of x > y")));
	idents.insert(std::make_pair("greaterThanEqual", Identifier("bvec greaterThanEqual(vec x, vec y)\nReturns the component-wise compare of x >= y")));
	idents.insert(std::make_pair("equal", Identifier("bvec equal(vec x, vec y)\nReturns the component-wise compare of x == y")));
	idents.insert(std::make_pair("notEqual", Identifier("bvec notEqual(vec x, vec y)\nReturns the component-wise compare of x != y")));
	idents.insert(std::make_pair("any", Identifier("bool any(bvec x)\nTest if any component of x is nonzero.")));
	idents.insert(std::make_pair("all", Identifier("bool all(bvec x)\nTest if all components of x are nonzero.")));
	idents.insert(std::make_pair("not", Identifier("bvec not(bvec x)\nReturns the component-wise logical complement of x.")));
	idents.insert(std::make_pair("uaddCarry", Identifier("genUType uaddCarry(genUType x, genUType y, out genUType carry)\nAdds 32bit unsigned integer x and y, returning the sum modulo 2^32.")));
	idents.insert(std::make_pair("usubBorrow", Identifier("genUType usubBorrow(genUType x, genUType y, out genUType borrow)\nSubtracts the 32bit unsigned integer y from x, returning the difference if non-negatice, or 2^32 plus the difference otherwise.")));
	idents.insert(std::make_pair("umulExtended", Identifier("void umulExtended(genUType x, genUType y, out genUType msb, out genUType lsb)\nMultiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("imulExtended", Identifier("void imulExtended(genIType x, genIType y, out genIType msb, out genIType lsb)\nMultiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("bitfieldExtract", Identifier("genIType bitfieldExtract(genIType value, int offset, int bits)\ngenUType bitfieldExtract(genUType value, int offset, int bits)\nExtracts bits [offset, offset + bits - 1] from value, returning them in the least significant bits of the result.")));
	idents.insert(std::make_pair("bitfieldInsert", Identifier("genIType bitfieldInsert(genIType base, genIType insert, int offset, int bits)\ngenUType bitfieldInsert(genUType base, genUType insert, int offset, int bits)\nReturns the insertion the bits leas-significant bits of insert into base")));
	idents.insert(std::make_pair("bitfieldReverse", Identifier("genIType bitfieldReverse(genIType value)\ngenUType bitfieldReverse(genUType value)\nReturns the reversal of the bits of value.")));
	idents.insert(std::make_pair("bitCount", Identifier("genIType bitCount(genIType value)\ngenUType bitCount(genUType value)\nReturns the number of bits set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findLSB", Identifier("genIType findLSB(genIType value)\ngenUType findLSB(genUType value)\nReturns the bit number of the least significant bit set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findMSB", Identifier("genIType findMSB(genIType value)\ngenUType findMSB(genUType value)\nReturns the bit number of the most significant bit in the binary representation of value.")));
	idents.insert(std::make_pair("textureSize", Identifier("ivecX textureSize(gsamplerXD sampler, int lod)\nReturns the dimensions of level lod  (if present) for the texture bound to sample.")));
	idents.insert(std::make_pair("textureQueryLod", Identifier("vec2 textureQueryLod(gsamplerXD sampler, vecX P)\nReturns the mipmap array(s) that would be accessed in the x component of the return value.")));
	idents.insert(std::make_pair("texture", Identifier("gvec4 texture(gsamplerXD sampler, vecX P, [float bias])\nUse the texture coordinate P to do a texture lookup in the texture currently bound to sampler.")));
	idents.insert(std::make_pair("textureProj", Identifier("Do a texture lookup with projection.")));
	idents.insert(std::make_pair("textureLod", Identifier("gvec4 textureLod(gsamplerXD sampler, vecX P, float lod)\nDo a texture lookup as in texture but with explicit LOD.")));
	idents.insert(std::make_pair("textureOffset", Identifier("gvec4 textureOffset(gsamplerXD sampler, vecX P, ivecX offset, [float bias])\nDo a texture lookup as in texture but with offset added to the (u,v,w) texel coordinates before looking up each texel.")));
	idents.insert(std::make_pair("texelFetch", Identifier("gvec4 texelFetch(gsamplerXD sampler, ivecX P, int lod)\nUse integer texture coordinate P to lookup a single texel from sampler.")));
	idents.insert(std::make_pair("texelFetchOffset", Identifier("gvec4 texelFetchOffset(gsamplerXD sampler, ivecX P, int lod, int offset)\nFetch a single texel as in texelFetch offset by offset.")));
	idents.insert(std::make_pair("textureProjLod", Identifier("Do a projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureLodOffset", Identifier("gvec4 textureLodOffset(gsamplerXD sampler, vecX P, float lod, ivecX offset)\nDo an offset texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureProjLodOffset", Identifier("Do an offset projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureGrad", Identifier("gvec4 textureGrad(gsamplerXD sampler, vecX P, vecX dPdx, vecX dPdy)\nDo a texture lookup as in texture but with explicit gradients.")));
	idents.insert(std::make_pair("textureGradOffset", Identifier("gvec4 textureGradOffset(gsamplerXD sampler, vecX P, vecX dPdx, vecX dPdy, ivecX offset)\nDo a texture lookup with both explicit gradient and offset, as described in textureGrad and textureOffset.")));
	idents.insert(std::make_pair("textureProjGrad", Identifier("Do a texture lookup both projectively and with explicit gradient.")));
	idents.insert(std::make_pair("textureProjGradOffset", Identifier("Do a texture lookup both projectively and with explicit gradient as well as with offset.")));
	idents.insert(std::make_pair("textureGather", Identifier("gvec4 textureGather(gsampler2D sampler, vec2 P, [int comp])\nGathers four texels from a texture")));
	idents.insert(std::make_pair("textureGatherOffset", Identifier("gvec4 textureGatherOffset(gsampler2D sampler, vec2 P, ivec2 offset, [int comp])\nGathers four texels from a texture with offset.")));
	idents.insert(std::make_pair("textureGatherOffsets", Identifier("gvec4 textureGatherOffsets(gsampler2D sampler, vec2 P, ivec2 offsets[4], [int comp])\nGathers four texels from a texture with an array of offsets.")));
	idents.insert(std::make_pair("texture1D", Identifier("1D texture lookup.")));
	idents.insert(std::make_pair("texture1DLod", Identifier("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture1DProj", Identifier("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture1DProjLod", Identifier("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture2D", Identifier("2D texture lookup.")));
	idents.insert(std::make_pair("texture2DLod", Identifier("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture2DProj", Identifier("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture2DProjLod", Identifier("2D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture3D", Identifier("3D texture lookup.")));
	idents.insert(std::make_pair("texture3DLod", Identifier("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture3DProj", Identifier("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture3DProjLod", Identifier("3D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("textureCube", Identifier("Cube texture lookup.")));
	idents.insert(std::make_pair("textureCubeLod", Identifier("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1D", Identifier("1D texture lookup.")));
	idents.insert(std::make_pair("shadow1DLod", Identifier("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1DProj", Identifier("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow1DProjLod", Identifier("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("shadow2D", Identifier("2D texture lookup.")));
	idents.insert(std::make_pair("shadow2DLod", Identifier("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow2DProj", Identifier("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow2DProjLod", Identifier("2D texture lookup with projective divide and with LOD.")));

	idents.insert(std::make_pair("dFdx", Identifier("genType dFdx(genType p)\nReturns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("dFdy", Identifier("genType dFdy(genType p)\nReturns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("fwidth", Identifier("genType fwidth(genType p)\nReturns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("interpolateAtCentroid", Identifier("Return the value of the input varying interpolant sampled at a location inside the both the pixel and the primitive being processed.")));
	idents.insert(std::make_pair("interpolateAtSample", Identifier("Return the value of the input varying interpolant at the location of sample number sample.")));
	idents.insert(std::make_pair("interpolateAtOffset", Identifier("Return the value of the input varying interpolant sampled at an offset from the center of the pixel specified by offset.")));
	idents.insert(std::make_pair("noise1", Identifier("Generates a random value")));
	idents.insert(std::make_pair("noise2", Identifier("Generates a random value")));
	idents.insert(std::make_pair("noise3", Identifier("Generates a random value")));
	idents.insert(std::make_pair("noise4", Identifier("Generates a random value")));
	idents.insert(std::make_pair("EmitStreamVertex", Identifier("void EmitStreamVertex(int stream)\nEmit the current values of output variables to the current output primitive on stream stream.")));
	idents.insert(std::make_pair("EndStreamPrimitive", Identifier("void EndStreamPrimitive(int stream)\nCompletes the current output primitive on stream stream and starts a new one.")));
	idents.insert(std::make_pair("EmitVertex", Identifier("void EmitVertex()\nEmit the current values to the current output primitive.")));
	idents.insert(std::make_pair("EndPrimitive", Identifier("void EndPrimitive()\nCompletes the current output primitive and starts a new one.")));
	idents.insert(std::make_pair("barrier", Identifier("void barrier()\nSynchronize execution of multiple shader invocations")));
	idents.insert(std::make_pair("groupMemoryBarrier", Identifier("void groupMemoryBarrier()\nControls the ordering of memory transaction issued shader invocation relative to a work group")));
	idents.insert(std::make_pair("memoryBarrier", Identifier("uint memoryBarrier()\nControls the ordering of memory transactions issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierAtomicCounter", Identifier("void memoryBarrierAtomicCounter()\nControls the ordering of operations on atomic counters issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierBuffer", Identifier("void memoryBarrierBuffer()\nControls the ordering of operations on buffer variables issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierImage", Identifier("void memoryBarrierImage()\nControls the ordering of operations on image variables issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierShared", Identifier("void memoryBarrierShared()\nControls the ordering of operations on shared variables issued by a single shader invocation")));

	idents.insert(std::make_pair("atomicAdd", Identifier("int atomicAdd(inout int mem, int data)\nuint atomicAdd(inout uint mem, uint data)\nPerform an atomic addition to a variable")));
	idents.insert(std::make_pair("atomicAnd", Identifier("int atomicAnd(inout int mem, int data)\nuint atomicAnd(inout uint mem, uint data)\nPerform an atomic logical AND operation to a variable")));
	idents.insert(std::make_pair("atomicCompSwap", Identifier("int atomicCompSwap(inout int mem, uint compare, uint data)\nuint atomicCompSwap(inout uint mem, uint compare, uint data)\nPerform an atomic compare-exchange operation to a variable")));
	idents.insert(std::make_pair("atomicCounter", Identifier("uint atomicCounter(atomic_uint c)\nReturn the current value of an atomic counter")));
	idents.insert(std::make_pair("atomicCounterDecrement", Identifier("uint atomicCounterDecrement(atomic_uint c)\nAtomically decrement a counter and return its new value")));
	idents.insert(std::make_pair("atomicCounterIncrement", Identifier("uint atomicCounterIncrement(atomic_uint c)\nAtomically increment a counter and return the prior value")));
	idents.insert(std::make_pair("atomicExchange", Identifier("int atomicExchange(inout int mem, int data)\nuint atomicExchange(inout uint mem, uint data)\nPerform an atomic exchange operation to a variable ")));
	idents.insert(std::make_pair("atomicMax", Identifier("int atomicMax(inout int mem, int data)\nuint atomicMax(inout uint mem, uint data)\nPerform an atomic max operation to a variable")));
	idents.insert(std::make_pair("atomicMin", Identifier("int atomicMin(inout int mem, int data)\nuint atomicMin(inout uint mem, uint data)\nPerform an atomic min operation to a variable ")));
	idents.insert(std::make_pair("atomicOr", Identifier("int atomicOr(inout int mem, int data)\nuint atomicOr(inout uint mem, uint data)\nPerform an atomic logical OR operation to a variable")));
	idents.insert(std::make_pair("atomicXor", Identifier("int atomicXor(inout int mem, int data)\nuint atomicXor(inout uint mem, uint data)\nPerform an atomic logical exclusive OR operation to a variable")));
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::SPIRV()
{
	static bool inited = false;
	static LanguageDefinition langDef;

	if (!inited)
	{
		/*
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));
		*/

		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ =\\t]Op[a-zA-Z]*", PaletteIndex::Keyword));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("%[_a-zA-Z0-9]*", PaletteIndex::Identifier));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		
		langDef.CommentStart = "/*";
		langDef.CommentEnd = "*/";
		langDef.SingleLineComment = ";";

		langDef.CaseSensitive = true;
		langDef.AutoIndentation = false;

		langDef.Name = "SPIR-V";

		inited = true;
	}

	return langDef;
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::C()
{
	static bool inited = false;
	static LanguageDefinition langDef;

	if (!inited)
	{
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local"
		};

		for (auto& k : keywords)
			langDef.Keywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
		};

		for (auto& k : identifiers)
		{
			Identifier id;
			id.Declaration = "Built-in function";
			langDef.Identifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.Tokenize = [](const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, PaletteIndex & paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::String;
			}
			else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::CharLiteral;
			}
			else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::Identifier;
			}
			else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::Number;
			}
			else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
			{
				paletteIndex = PaletteIndex::Punctuation;
			}

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.CommentStart = "/*";
		langDef.CommentEnd = "*/";
		langDef.SingleLineComment = "//";
		
		langDef.CaseSensitive = true;
		langDef.AutoIndentation = true;

		langDef.Name = "C";

		inited = true;
	}

	return langDef;
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::SQL()
{
	static bool inited = false;
	static LanguageDefinition langDef;

	if (!inited)
	{
		static const char* const keywords[] = {
			"ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS", "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
			"AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ", "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE",
			"BROWSE", "FROM", "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO", "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE",
			"CHECKPOINT", "HAVING", "RIGHT", "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT", "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE",
			"COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA", "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET", "CONTAINSTABLE", "INTERSECT", "SETUSER",
			"CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME", "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE", "CURRENT_DATE", "LEFT", "TEXTSIZE",
			"CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO", "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK", "TRANSACTION",
			"DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE", "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
			"DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED", "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING","DROP", "OPENROWSET", "VIEW",
			"DUMMY", "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE", "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
		};

		for (auto& k : keywords)
			langDef.Keywords.insert(k);

		static const char* const identifiers[] = {
			"ABS",  "ACOS",  "ADD_MONTHS",  "ASCII",  "ASCIISTR",  "ASIN",  "ATAN",  "ATAN2",  "AVG",  "BFILENAME",  "BIN_TO_NUM",  "BITAND",  "CARDINALITY",  "CASE",  "CAST",  "CEIL",
			"CHARTOROWID",  "CHR",  "COALESCE",  "COMPOSE",  "CONCAT",  "CONVERT",  "CORR",  "COS",  "COSH",  "COUNT",  "COVAR_POP",  "COVAR_SAMP",  "CUME_DIST",  "CURRENT_DATE",
			"CURRENT_TIMESTAMP",  "DBTIMEZONE",  "DECODE",  "DECOMPOSE",  "DENSE_RANK",  "DUMP",  "EMPTY_BLOB",  "EMPTY_CLOB",  "EXP",  "EXTRACT",  "FIRST_VALUE",  "FLOOR",  "FROM_TZ",  "GREATEST",
			"GROUP_ID",  "HEXTORAW",  "INITCAP",  "INSTR",  "INSTR2",  "INSTR4",  "INSTRB",  "INSTRC",  "LAG",  "LAST_DAY",  "LAST_VALUE",  "LEAD",  "LEAST",  "LENGTH",  "LENGTH2",  "LENGTH4",
			"LENGTHB",  "LENGTHC",  "LISTAGG",  "LN",  "LNNVL",  "LOCALTIMESTAMP",  "LOG",  "LOWER",  "LPAD",  "LTRIM",  "MAX",  "MEDIAN",  "MIN",  "MOD",  "MONTHS_BETWEEN",  "NANVL",  "NCHR",
			"NEW_TIME",  "NEXT_DAY",  "NTH_VALUE",  "NULLIF",  "NUMTODSINTERVAL",  "NUMTOYMINTERVAL",  "NVL",  "NVL2",  "POWER",  "RANK",  "RAWTOHEX",  "REGEXP_COUNT",  "REGEXP_INSTR",
			"REGEXP_REPLACE",  "REGEXP_SUBSTR",  "REMAINDER",  "REPLACE",  "ROUND",  "ROWNUM",  "RPAD",  "RTRIM",  "SESSIONTIMEZONE",  "SIGN",  "SIN",  "SINH",
			"SOUNDEX",  "SQRT",  "STDDEV",  "SUBSTR",  "SUM",  "SYS_CONTEXT",  "SYSDATE",  "SYSTIMESTAMP",  "TAN",  "TANH",  "TO_CHAR",  "TO_CLOB",  "TO_DATE",  "TO_DSINTERVAL",  "TO_LOB",
			"TO_MULTI_BYTE",  "TO_NCLOB",  "TO_NUMBER",  "TO_SINGLE_BYTE",  "TO_TIMESTAMP",  "TO_TIMESTAMP_TZ",  "TO_YMINTERVAL",  "TRANSLATE",  "TRIM",  "TRUNC", "TZ_OFFSET",  "UID",  "UPPER",
			"USER",  "USERENV",  "VAR_POP",  "VAR_SAMP",  "VARIANCE",  "VSIZE "
		};

		for (auto& k : identifiers)
		{
			Identifier id;
			id.Declaration = "Built-in function";
			langDef.Identifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.CommentStart = "/*";
		langDef.CommentEnd = "*/";
		langDef.SingleLineComment = "//";

		langDef.CaseSensitive = false;
		langDef.AutoIndentation = false;

		langDef.Name = "SQL";

		inited = true;
	}

	return langDef;
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::AngelScript()
{
	static bool inited = false;
	static LanguageDefinition langDef;

	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default", "do", "double", "else", "enum", "false", "final", "float", "for",
			"from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is", "mixin", "namespace", "not",
			"null", "or", "out", "override", "private", "protected", "return", "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
			"uint64", "void", "while", "xor"
		};

		for (auto& k : keywords)
			langDef.Keywords.insert(k);

		static const char* const identifiers[] = {
			"cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE",
			"complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"
		};

		for (auto& k : identifiers)
		{
			Identifier id;
			id.Declaration = "Built-in function";
			langDef.Identifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.CommentStart = "/*";
		langDef.CommentEnd = "*/";
		langDef.SingleLineComment = "//";

		langDef.CaseSensitive = true;
		langDef.AutoIndentation = true;

		langDef.Name = "AngelScript";

		inited = true;
	}

	return langDef;
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::Lua()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "break", "do", "", "else", "elseif", "end", "false", "for", "function", "if", "in", "", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
		};

		for (auto& k : keywords)
			langDef.Keywords.insert(k);

		static const char* const identifiers[] = {
			"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring",  "next",  "pairs",  "pcall",  "print",  "rawequal",  "rawlen",  "rawget",  "rawset",
			"select",  "setmetatable",  "tonumber",  "tostring",  "type",  "xpcall",  "_G",  "_VERSION","arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", 
			"rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug","getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", 
			"getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen", 
			"read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger",
			"floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh",
			 "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock",
			 "date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep",
			 "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern",
			 "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
		};

		for (auto& k : identifiers)
		{
			Identifier id;
			id.Declaration = "Built-in function";
			langDef.Identifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.TokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.CommentStart = "--[[";
		langDef.CommentEnd = "]]";
		langDef.SingleLineComment = "--";

		langDef.CaseSensitive = true;
		langDef.AutoIndentation = false;

		langDef.Name = "Lua";

		inited = true;
	}

	return langDef;
}
