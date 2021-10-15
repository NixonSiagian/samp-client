#include "main.h"
#include "gui/gui.h"
#include "game/game.h"
#include "net/netgame.h"
#include "settings.h"
#include "dialog.h"
#include "spawnscreen.h"
#include "extrakeyboard.h"
#include "scoreboard.h"
#include "settings.h"

extern CGUI *pGUI;
extern CGame *pGame;
extern CNetGame *pNetGame;
extern CSettings *pSettings;
extern CScoreBoard *pScoreBoard;
extern CDialogWindow *pDialogWindow;
extern CSpawnScreen *pSpawnScreen;
extern CSettings *pSettings;

CExtraKeyBoard::CExtraKeyBoard() 
{
	m_bIsActive = false;
	m_bIsExtraShow = false;
	m_passengerUseTexture = pSettings->Get().bPassengerUseTexture;
	if(m_passengerUseTexture)
	{
		m_passengerButtonTexture[0] = (RwTexture*)LoadTextureFromDB("samp", "passengertexture"); // passenger button
		m_passengerButtonTexture[1] = (RwTexture*)LoadTextureFromDB("samp", "passengertexturehover"); // passenger button hover
		// the texture not load? okay using imgui button
		if(!m_passengerButtonTexture[0] || !m_passengerButtonTexture[1]) m_passengerUseTexture = false;
		m_dwLastTickPassengerHover = GetTickCount();
	}
	Log("Extrakeyboard initialized.");
}

CExtraKeyBoard::~CExtraKeyBoard() 
{ 
	m_bIsActive = false;
	m_bIsExtraShow = false;
	if(m_passengerUseTexture)
	{
		for(int i = 0; i < 2; i++)
			m_passengerButtonTexture[i] = nullptr;

		m_dwLastTickPassengerHover = GetTickCount();
	}
}

void CExtraKeyBoard::Show(bool bShow) 
{
	m_bIsActive = bShow;
}

void CExtraKeyBoard::Render() 
{
	if(!m_bIsActive)
		return;

	bool bDontExpand = false;

	CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
	if(pPlayerPed) 
	{
		ImGuiIO &io = ImGui::GetIO();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.5f, 6.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

		ImGui::Begin("Extrakeyboard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

		float m_fButWidth = ImGui::CalcTextSize("QWERTY").x;
		float m_fButHeight = ImGui::CalcTextSize("QWER").x;

		CTextDrawPool *pTextDrawPool = pNetGame->GetTextDrawPool();
		if(pTextDrawPool)
		{
			if(pTextDrawPool->GetState())
			{
				if(ImGui::Button("ESC", ImVec2(m_fButWidth, m_fButHeight)))
				{
					if(pTextDrawPool->GetState())
						pTextDrawPool->SetSelectState(false, 0);
				}
				
				ImGui::SameLine(0, 10);
			}
		}
		if(!bDontExpand)
		{
			if(ImGui::Button(m_bIsExtraShow ? "<<<" : ">>>", ImVec2(m_fButWidth, m_fButHeight)))
				m_bIsExtraShow = !m_bIsExtraShow;

			if(m_bIsExtraShow) 
			{
				ImGui::SameLine(0, 10);
				if(ImGui::Button("TAB", ImVec2(m_fButWidth, m_fButHeight)))
				{
						pScoreBoard->Toggle();
				}
				ImGui::SameLine(0, 10);
				if(ImGui::Button("ALT", ImVec2(m_fButWidth, m_fButHeight)))
				{
					if(pPlayerPed->IsInVehicle()) LocalPlayerKeys.bKeys[ePadKeys::KEY_FIRE] = true;
					else LocalPlayerKeys.bKeys[ePadKeys::KEY_WALK] = true;
				}

				ImGui::SameLine(0, 10);
				if(ImGui::Button("F", ImVec2(m_fButWidth, m_fButHeight)))
					LocalPlayerKeys.bKeys[ePadKeys::KEY_SECONDARY_ATTACK] = true;
				
				ImGui::SameLine(0, 10);
				if(ImGui::Button("Y", ImVec2(m_fButWidth, m_fButHeight)))
					LocalPlayerKeys.bKeys[ePadKeys::KEY_YES] = true;
				
				ImGui::SameLine(0, 10);
				if(ImGui::Button("H", ImVec2(m_fButWidth, m_fButHeight)))
				{
					LocalPlayerKeys.bKeys[ePadKeys::KEY_CTRL_BACK] = true;
					if(pPlayerPed->IsAPassenger()) 
						pGame->FindPlayerPed()->StartPassengerDriveByMode();
				}

				ImGui::SameLine(0, 10);
				if(ImGui::Button("N", ImVec2(m_fButWidth, m_fButHeight)))
					LocalPlayerKeys.bKeys[ePadKeys::KEY_NO] = true;
				
				ImGui::SameLine();
				if(ImGui::Button("L.CTRL", ImVec2(m_fButWidth, m_fButHeight)))
          	   {
               pNetGame->SendChatCommand("/en");
           	   }

				ImGui::SameLine(0, 10);
				if(ImGui::Button("2", ImVec2(m_fButWidth, m_fButHeight)))
					LocalPlayerKeys.bKeys[ePadKeys::KEY_SUBMISSION] = true;
			}
		}

		if(!m_passengerUseTexture)
		{
			CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
			if(pVehiclePool) 
			{
				uint16_t sNearestVehicleID = pVehiclePool->FindNearestToLocalPlayerPed();
				CVehicle *pVehicle = pVehiclePool->GetAt(sNearestVehicleID);
				if(pVehicle)
				{
					if(pVehicle->GetDistanceFromLocalPlayerPed() < 4.0f) 
					{
						CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
						if(pPlayerPool) 
						{
							CLocalPlayer *pLocalPlayer = pPlayerPool->GetLocalPlayer();
							if(pLocalPlayer)
							{
								if(!pLocalPlayer->IsSpectating() && !pPlayerPed->IsInVehicle())
								{
									ImGui::SameLine(0, 10);
									if(ImGui::Button("G", ImVec2(m_fButWidth, m_fButHeight)))
									{
										pPlayerPed->EnterVehicle(pVehicle->m_dwGTAId, true);
										pLocalPlayer->SendEnterVehicleNotification(sNearestVehicleID, true);
									}
								}
							}
						}
					}
				}
			}
		}

		ImGui::SetWindowSize(ImVec2(-1, -1));
		ImVec2 size = ImGui::GetWindowSize();
		ImGui::SetWindowPos(ImVec2(pGUI->ScaleX(10), pGUI->ScaleY(365)));
		ImGui::End();
			
		ImGui::PopStyleVar(2);
	
		if(m_passengerUseTexture)
		{
			ImGuiIO &io = ImGui::GetIO();
		
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(0x00, 0x00, 0x00, 0x00).Value);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(0x00, 0x00, 0x00, 0x00).Value);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(0x00, 0x00, 0x00, 0x00).Value);
			
			ImGuiStyle style;
			style.FrameBorderSize = ImGui::GetStyle().FrameBorderSize;
			ImGui::GetStyle().FrameBorderSize = 0.0f;
			
			ImGui::Begin("PassengerTexture", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
			
			ImVec2 vecButSize = ImVec2(pSettings->Get().fPassengerTextureSize * 5 + 5.0f, pSettings->Get().fPassengerTextureSize * 5);
			
			CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
			if(pVehiclePool) 
			{
				uint16_t sNearestVehicleID = pVehiclePool->FindNearestToLocalPlayerPed();
				CVehicle *pVehicle = pVehiclePool->GetAt(sNearestVehicleID);
				if(pVehicle)
				{
					if(pVehicle->GetDistanceFromLocalPlayerPed() < 4.0f) 
					{
						CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
						if(pPlayerPool) 
						{
							CLocalPlayer *pLocalPlayer = pPlayerPool->GetLocalPlayer();
							if(pLocalPlayer)
							{
								if(!pLocalPlayer->IsSpectating() && !pPlayerPed->IsInVehicle())
								{
									if(ImGui::ImageButton(GetTickCount() - m_dwLastTickPassengerHover >= 150 ? (ImTextureID)m_passengerButtonTexture[0]->raster : (ImTextureID)m_passengerButtonTexture[1]->raster, vecButSize))
									{
										m_dwLastTickPassengerHover = GetTickCount();
										pPlayerPed->EnterVehicle(pVehicle->m_dwGTAId, true);
										pLocalPlayer->SendEnterVehicleNotification(sNearestVehicleID, true);
									}
								}
							}
						}
					}
				}
			}
			
			ImGui::SetWindowSize(ImVec2(-1, -1));
			
			ImGui::SetWindowPos(ImVec2(pGUI->ScaleX(pSettings->Get().fPassengerTextureX), pGUI->ScaleY(pSettings->Get().fPassengerTextureY)));
			ImGui::End();
			
			ImGui::PopStyleColor(3);
			ImGui::GetStyle().FrameBorderSize = style.FrameBorderSize;
		}
	}
	return;
}
