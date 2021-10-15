#include "main.h"
#include "game/game.h"
#include "net/netgame.h"
#include "gui/gui.h"
#include "dialog.h"
#include "vendor/imgui/imgui_internal.h"
#include "keyboard.h"
#include "gui/util.h"

extern CGUI *pGUI;
extern CGame *pGame;
extern CNetGame *pNetGame;
extern CKeyBoard *pKeyBoard;

char szCDialogInputBuffer[100];
char utf8CDialogInputBuffer[100*3];

CDialogWindow::CDialogWindow()
{
	m_bIsActive = false;
	m_putf8Info = nullptr;
	m_pszInfo = nullptr;
}

void CDialogWindow::Show(bool bShow)
{
	if(pGame)
		pGame->FindPlayerPed()->TogglePlayerControllable(!bShow);

	m_bIsActive = bShow;
}

void DialogWindowInputHandler(const char* str)
{
	if(!str || *str == '\0') return;
	strcpy(szCDialogInputBuffer, str);
	cp1251_to_utf8(utf8CDialogInputBuffer, str);
}
void CDialogWindow::Clear()
{
	if(m_putf8Info)
	{
		free(m_putf8Info);
		m_putf8Info = nullptr;
	}

	if(m_pszInfo)
	{
		free(m_pszInfo);
		m_pszInfo = nullptr;
	}

	m_bIsActive = false;
	m_byteDialogStyle = 0;
	m_wDialogID = -1;
	m_utf8Title[0] = 0;
	m_utf8Button1[0] = 0;
	m_utf8Button2[0] = 0;
	
	memset(szCDialogInputBuffer, 0, 100);
	memset(utf8CDialogInputBuffer, 0, 100*3);
}

void CDialogWindow::SetInfo(char* szInfo, int length)
{
	if(!szInfo || !length) return;

	if(m_putf8Info)
	{
		free(m_putf8Info);
		m_putf8Info = nullptr;
	}

	if(m_pszInfo)
	{
		free(m_pszInfo);
		m_pszInfo = nullptr;
	}

	m_putf8Info = (char*)malloc((length * 3) + 1);
	if(!m_putf8Info) return;

	m_pszInfo = (char*)malloc((length * 3) + 1);
	if(!m_pszInfo) return;

	cp1251_to_utf8(m_putf8Info, szInfo);

	// =========
	char szNonColorEmbeddedMsg[4200];
	int iNonColorEmbeddedMsgLen = 0;

	for (size_t pos = 0; pos < strlen(szInfo) && szInfo[pos] != '\0'; pos++)
	{
		if(pos+7 < strlen(szInfo))
		{
			if (szInfo[pos] == '{' && szInfo[pos+7] == '}')
			{
				pos += 7;
				continue;
			}
		}

		szNonColorEmbeddedMsg[iNonColorEmbeddedMsgLen] = szInfo[pos];
		iNonColorEmbeddedMsgLen++;
	}

	szNonColorEmbeddedMsg[iNonColorEmbeddedMsgLen] = 0;
	// ========

	cp1251_to_utf8(m_pszInfo, szNonColorEmbeddedMsg);
}

bool CDialogWindow::IsDialogList()
{
	if(m_bIsActive)
	{
		if(m_byteDialogStyle == DIALOG_STYLE_LIST || m_byteDialogStyle == DIALOG_STYLE_TABLIST || 
			m_byteDialogStyle == DIALOG_STYLE_TABLIST_HEADERS)
			return true;
	}
	return false;
}

void dialogKeyboardEvent(const char* str)
{
	if(!str || *str == '\0') return;
	strcpy(szCDialogInputBuffer, str);
	cp1251_to_utf8(utf8CDialogInputBuffer, str);
}

void CDialogWindow::Draw()
{
	if(m_bIsActive)
	{
		if(m_byteDialogStyle == DIALOG_STYLE_MSGBOX || m_byteDialogStyle == DIALOG_STYLE_INPUT ||
			m_byteDialogStyle == DIALOG_STYLE_PASSWORD)
		{
			DrawDefault();
		}
		else
		{
			DrawList();
		}
	}
}

void CDialogWindow::DrawList()
{
	std::string strUtf8 = m_putf8Info;
	int size = strUtf8.length();
	int iTabsCount = 0;
	std::vector<std::string> vLines;
	std::vector<std::string> firstTabs;
	ImVec2 fTabSize[4];

	int tmplineid;
	std::stringstream ssLine(strUtf8);
	std::string tmpLine;
	while(std::getline(ssLine, tmpLine, '\n'))
	{
		if(tmpLine[0] != 0)
		{
			vLines.push_back(tmpLine);
			int tmpTabId = 0;
			if(m_byteDialogStyle != DIALOG_STYLE_LIST)
			{
				std::stringstream ssTabLine(tmpLine);
				std::string tmpTabLine;
				while(std::getline(ssTabLine, tmpTabLine, '\t'))
				{
					if(tmpTabId > 4) continue;
					//if(vLines.size() == 1 && iTabsCount <= 4)
					if(tmpTabId >= iTabsCount && iTabsCount <= 4)
						iTabsCount++; //0 >= 0 = 1 //1 >= 1 = 2 //

					ImVec2 tabSize;
					tabSize = CalcTextSizeWithoutTags((char*)tmpTabLine.c_str());

					if(tmpTabId == 0)
					firstTabs.push_back(removeColorTags(tmpTabLine));

					if(tabSize.x > fTabSize[tmpTabId].x)
						fTabSize[tmpTabId].x = tabSize.x;

					tmpTabId++;
				}
			} 
			else 
			{
				ImVec2 tabSize;
				tabSize = CalcTextSizeWithoutTags((char*)tmpLine.c_str());

				if(tmpTabId == 0)
					firstTabs.push_back(removeColorTags(tmpLine));
				
				if(tabSize.x > fTabSize[tmpTabId].x)
					fTabSize[tmpTabId].x = tabSize.x;
			}
			tmplineid++;
		}
	}

	float buttonFactor = pGUI->ScaleX(187.5f) * 2 + pGUI->GetFontSize();

	ImVec2 vecWinSize;
	if(m_byteDialogStyle != DIALOG_STYLE_LIST)
	{
		for(uint8_t i = 0; i < iTabsCount; i++)
			vecWinSize.x += fTabSize[i].x * (i == iTabsCount - 1 ? 1.25 : 1.5);
	}
	else vecWinSize.x += fTabSize[0].x * 1.25;
	if(buttonFactor > vecWinSize.x)
	{
		vecWinSize.x = 0.0f;
		
		if(m_byteDialogStyle != DIALOG_STYLE_LIST)
		{
			buttonFactor /= iTabsCount;
			for(uint8_t i = 0; i < iTabsCount; i++)
				vecWinSize.x += fTabSize[i].x + buttonFactor;
		}
		else vecWinSize.x += fTabSize[0].x + buttonFactor;
	}
	else buttonFactor = 0;

	vecWinSize.x += 93.5f;
	vecWinSize.y = 720;
		
	ImGuiStyle style;
	ImGuiIO &io = ImGui::GetIO();
	
	ImGui::Begin("###tablist", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
	
	ImVec2 cursor = ImGui::GetCursorScreenPos();
	cursor.x -= 32;
	cursor.y -= 3;
	ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImVec2(cursor.x + ImGui::GetWindowWidth() + 32, cursor.y + ImGui::GetFontSize() + 5 + 3), ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)));
	TextWithColors(m_utf8Title);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

	ImGui::ItemSize(ImVec2(0, pGUI->GetFontSize() / 2 + 2.75f));

	ImVec2 cursonPosition;
	cursonPosition = ImGui::GetCursorPos();
	
	if(m_byteDialogStyle == DIALOG_STYLE_TABLIST_HEADERS)
	{
		style.Colors[ImGuiCol_ChildBg] = ImGui::GetStyle().Colors[ImGuiCol_ChildBg];
		ImGui::GetStyle().Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		ImGui::BeginChild("###headers", ImVec2(-1, pGUI->GetFontSize()), false);
		
		ImGui::Columns(iTabsCount, "###tablistHeader", false);
		for(uint16_t i = 0; i < iTabsCount; i++)
		{
			ImGui::SetColumnWidth(-1, fTabSize[i].x * (i == iTabsCount - 1 ? 1.25 : 1.5) + buttonFactor);
			ImGui::NextColumn();
		}
		
		std::stringstream ssTabLine(vLines[0]);
		std::string tmpTabLine;
		int tmpTabId = 0;
		while(std::getline(ssTabLine, tmpTabLine, '\t'))
		{
			if(tmpTabId > iTabsCount) continue;
			tmpTabLine.insert(0, "{a9c4e4}");
			if(tmpTabId == 0)
			{
				cursonPosition = ImGui::GetCursorPos();
				ImGui::SetCursorPosX(cursonPosition.x + pGUI->GetFontSize() / 3);
			}
			TextWithColors(tmpTabLine.c_str());
			ImGui::NextColumn();
			tmpTabId++;
		}
		ImGui::Columns(1);
		ImGui::EndChild();
		ImGui::GetStyle().Colors[ImGuiCol_ChildBg] = style.Colors[ImGuiCol_ChildBg];
	}

	ImGui::BeginChild("###tablist", ImVec2(-1, pGUI->ScaleY(vecWinSize.y)-pGUI->ScaleY(95.0f*2)), false, ImGuiWindowFlags_NoScrollbar);

	if(m_byteDialogStyle != DIALOG_STYLE_LIST)
	{
		ImGui::Columns(iTabsCount, "###tablistColumn", false);
		for(uint16_t i = 0; i < iTabsCount; i++)
		{
			ImGui::SetColumnWidth(-1, fTabSize[i].x * (i == iTabsCount - 1 ? 1.25 : 1.5) + buttonFactor);
			ImGui::NextColumn();
		}
	}
	
	for(uint32_t line = m_byteDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ? 1 : 0; line < vLines.size(); line++)
	{
		std::stringstream ssTabLine(vLines[line]);
		std::string tmpTabLine;
		int tmpTabId = 0;

		ImVec2 differentOffsets;
		if(tmpTabId == 0)
		{
			ImGui::PushID(tmpTabId+line);

			std::stringstream ss;
			ss << line+tmpTabId;
			std::string s = ss.str();

			std::string itemid = "##" + s;
			bool is_selected = (m_iSelectedItem == line);

			cursonPosition = ImGui::GetCursorPos();
			if(ImGui::Selectable(itemid.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
			{
				if(ImGui::IsMouseDoubleClicked(0))
				{
					Show(false);
					if(pNetGame) 
					{
						if(m_byteDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) m_iSelectedItem--;
						pNetGame->SendDialogResponse(m_wDialogID, 1, m_iSelectedItem, (char*)firstTabs[m_iSelectedItem].c_str());
					}
				}
			}

			if(ImGui::IsItemHovered())
				m_iSelectedItem = line;

			ss.clear();
			ss << line + tmpTabId + 1;
			s = ss.str();
			itemid = "##" + s;
			if(ImGui::Selectable(itemid.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
			{
				if(ImGui::IsMouseDoubleClicked(0))
				{
					Show(false);
					if(pNetGame) 
					{
						if(m_byteDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) m_iSelectedItem--;
						pNetGame->SendDialogResponse(m_wDialogID, 1, m_iSelectedItem, (char*)firstTabs[m_iSelectedItem].c_str());
					}
				}
			}

			if(ImGui::IsItemHovered())
				m_iSelectedItem = line;

			ImVec2 cursonPositionLast;
			cursonPositionLast = ImGui::GetCursorPos();

			differentOffsets.x = cursonPositionLast.x - cursonPosition.x;
			differentOffsets.y = cursonPositionLast.y - cursonPosition.y;

			if(is_selected) ImGui::SetItemDefaultFocus();

			if(m_byteDialogStyle != DIALOG_STYLE_LIST)
				ImGui::SameLine();
			
		}
		if(m_byteDialogStyle != DIALOG_STYLE_LIST)
		{
			while(std::getline(ssTabLine, tmpTabLine, '\t'))
			{
				if(tmpTabId > iTabsCount) continue;
				
				ImVec2 cursonPos;
				cursonPos = ImGui::GetCursorPos();
				if(tmpTabId == 0)
				{
					if(line == m_iSelectedItem)
						m_strSelectedItemText = tmpTabLine;

					ImGui::SetCursorPos(ImVec2(cursonPos.x, cursonPos.y - differentOffsets.y / 4));
				}
				else ImGui::SetCursorPos(ImVec2(cursonPos.x, cursonPos.y + differentOffsets.y / 4));
				
				tmpTabLine.insert(0, "{ffffff}");
				TextWithColors((char*)tmpTabLine.c_str());
				ImGui::SetCursorPos(ImVec2(cursonPos.x, cursonPos.y + differentOffsets.y / 2));
				
				if(m_byteDialogStyle != DIALOG_STYLE_LIST)
					ImGui::NextColumn();

				tmpTabId++;
				
			}
			while(tmpTabId < iTabsCount)
			{
				TextWithColors("{ffffff}");
				ImGui::NextColumn();
				tmpTabId++;
			}
		}
		else 
		{
			ImVec2 cursonPos;
			cursonPos = ImGui::GetCursorPos();

			if(line == m_iSelectedItem)
				m_strSelectedItemText = vLines[line];

			ImGui::SetCursorPos(ImVec2(cursonPos.x+pGUI->GetFontSize(), cursonPos.y - differentOffsets.y + pGUI->GetFontSize() / 2));
			
			vLines[line].insert(0, "{ffffff}");
			TextWithColors((char*)vLines[line].c_str());
			ImGui::SetCursorPos(ImVec2(cursonPos.x, cursonPos.y));

		}
	}
	
	if(m_byteDialogStyle != DIALOG_STYLE_LIST)
		ImGui::Columns(1);
	
	ScrollWhenDraggingOnVoid();
	ImGui::EndChild();

	if(m_utf8Button1[0] != 0 && m_utf8Button2[0] != 0)
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - pGUI->ScaleX(417.0f) + ImGui::GetStyle().ItemSpacing.x + pGUI->GetFontSize()) / 2);
	else
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - pGUI->ScaleX(230.0f) + ImGui::GetStyle().ItemSpacing.x + pGUI->GetFontSize()) / 2);
	
	if(m_utf8Button1[0] != 0)
	{
		if(ImGui::Button(m_utf8Button1, ImVec2(pGUI->ScaleX(187.5f), pGUI->ScaleY(60.0f))))
		{
			Show(false);
			if(pNetGame)
			{
				if(m_byteDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) m_iSelectedItem--;
				pNetGame->SendDialogResponse(m_wDialogID, 1, m_iSelectedItem, (char*)firstTabs[m_iSelectedItem].c_str());
			}
		}
	}

	if(m_utf8Button1[0] != 0 && m_utf8Button2[0] != 0)
		ImGui::SameLine(0, pGUI->GetFontSize());

	if(m_utf8Button2[0] != 0)
	{
		if(ImGui::Button(m_utf8Button2, ImVec2(pGUI->ScaleX(187.5f), pGUI->ScaleY(60.0f))))
		{
			Show(false);
			if(pNetGame)
			{
				if(firstTabs.size() <= m_iSelectedItem)
				{
					m_iSelectedItem = firstTabs.size();
					m_iSelectedItem--;
				}
				pNetGame->SendDialogResponse(m_wDialogID, 0, m_iSelectedItem, (char*)firstTabs[m_iSelectedItem].c_str());
			}
		}
	}

	ImGui::SetWindowSize(ImVec2(vecWinSize.x, pGUI->ScaleY(vecWinSize.y + m_byteDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ? pGUI->ScaleY(187.5f) / 2 : 0)));
	ImVec2 winsize = ImGui::GetWindowSize();
	ImGui::SetWindowPos(ImVec2(((io.DisplaySize.x - winsize.x) / 2), ((io.DisplaySize.y - winsize.y) / 2)));
	ImGui::End();
}

void CDialogWindow::DrawDefault()
{
	if(!m_bIsActive || !m_putf8Info) return;
	
	ImGuiIO &io = ImGui::GetIO();
		
	ImGui::Begin("###dialogDefault", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize | 
		ImGuiWindowFlags_NoScrollbar);
	
	// title
	ImVec2 cursor = ImGui::GetCursorScreenPos();
	cursor.x -= 32;
	cursor.y -= 3;
	ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImVec2(cursor.x + ImGui::GetWindowWidth() + 32, cursor.y + ImGui::GetFontSize() + 5 + 3), ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)));
	TextWithColors(m_utf8Title);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
	
	ImGui::ItemSize(ImVec2(0, pGUI->GetFontSize() / 2 + 2.75f));

	switch(m_byteDialogStyle)
	{
		case DIALOG_STYLE_MSGBOX:
		TextWithColors(m_putf8Info);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX()+pGUI->GetFontSize()*1.5);
		ImGui::ItemSize(ImVec2(0, pGUI->GetFontSize() / 2));
		break;

		case DIALOG_STYLE_INPUT:
		TextWithColors(m_putf8Info);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX()+pGUI->GetFontSize()*1.5);
		ImGui::ItemSize(ImVec2(0, pGUI->GetFontSize() / 2));

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if(ImGui::Button(utf8CDialogInputBuffer, ImVec2(pGUI->ScaleX(500 * 2), pGUI->ScaleY(35 * 2))))
		{
			if(!pKeyBoard->IsOpen())
				pKeyBoard->Open(&DialogWindowInputHandler, false);
		}
		ImGui::PopStyleVar(1);
		ImGui::ItemSize(ImVec2(0, pGUI->GetFontSize() / 2));
		break;

		case DIALOG_STYLE_PASSWORD:
		TextWithColors(m_putf8Info);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX()+pGUI->GetFontSize()*1.5);
		ImGui::ItemSize(ImVec2(0, pGUI->GetFontSize() / 2));

		char _utf8CDialogInputBuffer[100*3+1];
		strcpy(_utf8CDialogInputBuffer, utf8CDialogInputBuffer);

		for(int i = 0; i < strlen(_utf8CDialogInputBuffer); i++)
		{
			if(_utf8CDialogInputBuffer[i] == '\0')
				break;
			_utf8CDialogInputBuffer[i] = '*';
		}
		
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if(ImGui::Button(_utf8CDialogInputBuffer, ImVec2(ImGui::CalcTextSize(m_pszInfo).x+pGUI->GetFontSize(), pGUI->ScaleY(70.0f))))
		{
			if(!pKeyBoard->IsOpen())
				pKeyBoard->Open(&DialogWindowInputHandler, true);
		}
		ImGui::PopStyleVar(1);
		ImGui::ItemSize(ImVec2(0, pGUI->GetFontSize() / 2));
		break;
	}

	if(m_utf8Button1[0] != 0 && m_utf8Button2[0] != 0)
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - pGUI->ScaleX(417.0f) + ImGui::GetStyle().ItemSpacing.x + pGUI->GetFontSize()) / 2);
	else
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - pGUI->ScaleX(230.0f) + ImGui::GetStyle().ItemSpacing.x + pGUI->GetFontSize()) / 2);
	

	if(m_utf8Button1[0] != 0) 
	{
		if(ImGui::Button(m_utf8Button1, ImVec2(pGUI->ScaleX(187.5f), pGUI->ScaleY(60.0f))))
		{
			Show(false);
			if(pNetGame) 
				pNetGame->SendDialogResponse(m_wDialogID, 1, 0, szCDialogInputBuffer);
			memset(szCDialogInputBuffer, 0, 100);
		}
	}

	if(m_utf8Button1[0] != 0 && m_utf8Button2[0] != 0) 
		ImGui::SameLine(0, pGUI->GetFontSize());

	if(m_utf8Button2[0] != 0) 
	{
		if(ImGui::Button(m_utf8Button2, ImVec2(pGUI->ScaleX(187.5f), pGUI->ScaleY(60.0f))))
		{
			Show(false);
			if(pNetGame) 
				pNetGame->SendDialogResponse(m_wDialogID, 0, -1, szCDialogInputBuffer);
			memset(szCDialogInputBuffer, 0, 100);
		}
	}
	
	ScrollWhenDraggingOnVoid();
	ImVec2 size = ImGui::GetWindowSize();
	ImGui::SetWindowPos(ImVec2(((io.DisplaySize.x - size.x) / 2), ((io.DisplaySize.y - size.y) / 2)));
	ImGui::End();
}