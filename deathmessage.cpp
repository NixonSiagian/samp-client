#include "main.h"
#include "game/game.h"
#include "game/util.h"
#include "net/netgame.h"
#include "deathmessage.h"
#include "gui/gui.h"

extern CGUI* pGUI;
extern CGame* pGame;

CDeathMessage::CDeathMessage()
{
	m_pDeathMessage.clear();
	Log("Deathmessage initialized.");
}

CDeathMessage::~CDeathMessage()
{
	m_pDeathMessage.clear();
}

void CDeathMessage::MakeRecord(const char* playername, const char *killername, uint8_t reason)
{
	DeathMessageStruct* pPlayerKill = new DeathMessageStruct;
	pPlayerKill->playerName = playername;
	pPlayerKill->killerName = killername;
	pPlayerKill->reason = reason;

	if(m_pDeathMessage.size() >= 5)
		m_pDeathMessage.pop_front();

	m_pDeathMessage.push_back(pPlayerKill);
}

void CDeathMessage::Render()
{
	if(m_pDeathMessage.empty() == false)
	{
		ImVec2 vecPos;
		vecPos.x = RsGlobal->maximumWidth / 2 + pGUI->ScaleX(350.0f);
		vecPos.y = RsGlobal->maximumHeight / 2 - pGUI->ScaleY(170.0f);

		for(auto& playerkill : m_pDeathMessage)
		{
			if(playerkill)
			{
				char szText[256], playerName[24], killerName[24];
				cp1251_to_utf8(playerName, playerkill->playerName.c_str());
				cp1251_to_utf8(killerName, playerkill->killerName.c_str());

				if(playerkill->reason == 200 || playerkill->reason == 201)
				{
					sprintf(szText, "%s %s.", playerName, playerkill->reason == 200 ? "connected" : playerkill->reason == 201 ? "disconnected" : GetWeaponName(playerkill->reason));
				}
				else
				{
					if(!playerkill->killerName.empty())
						sprintf(szText, "%s kill %s (%s)", killerName, playerName, GetWeaponName(playerkill->reason));
					else sprintf(szText, "%s died. (%s)", playerName, GetWeaponName(playerkill->reason));
				}

				pGUI->RenderText(vecPos, 0xFFFFFFFF, true, szText);
				vecPos.y += pGUI->GetFontSize() + ImGui::GetStyle().ItemSpacing.y;
			}
		}
	}
}