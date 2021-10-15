#include "../main.h"
#include "gui.h"
#include "../game/game.h"
#include "../net/netgame.h"
#include "../game/RW/RenderWare.h"
#include "../chatwindow.h"
#include "../spawnscreen.h"
#include "../playertags.h"
#include "../dialog.h"
#include "../extrakeyboard.h"
#include "../keyboard.h"
#include "../debug.h"
#include "../settings.h"
#include "../scoreboard.h"
#include "../deathmessage.h"

// voice
#include "../voice/MicroIcon.h"
#include "../voice/SpeakerList.h"
#include "../voice/include/util/Render.h"

#include <time.h>
#include <ctime>
#include <stdio.h>
#include <string.h>

extern CExtraKeyBoard *pExtraKeyBoard;
extern CSpawnScreen *pSpawnScreen;
extern CPlayerTags *pPlayerTags;
extern CDialogWindow *pDialogWindow;
extern CDebug *pDebug;
extern CChatWindow *pChatWindow;
extern CSettings *pSettings;
extern CKeyBoard *pKeyBoard;
extern CNetGame *pNetGame;
extern CGame *pGame;
extern CScoreBoard *pScoreBoard;
extern CDeathMessage *pDeathMessage;

/* imgui_impl_renderware.h */
void ImGui_ImplRenderWare_RenderDrawData(ImDrawData* draw_data);
bool ImGui_ImplRenderWare_Init();
void ImGui_ImplRenderWare_NewFrame();

/*
	Все координаты GUI-элементов задаются
	относительно разрешения 1920x1080
*/
#define MULT_X					0.00052083333f	// 1/1920
#define MULT_Y					0.00092592592f 	// 1/1080

//Yellow 						ImVec4(0.96f, 0.56f, 0.19f, 1.0f)
//Dark Red 						ImVec4(0.7f, 0.12f, 0.12f, 1.0f)

#define PRIMARY_COLOR 			ImVec4(0.96f, 0.56f, 0.19f, 1.0f)
#define SECONDARY_COLOR 		ImVec4(0.0f, 0.200f, 0.255f, 1.00f)
CGUI::CGUI()
{
	// setup ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	ImGui_ImplRenderWare_Init();

	// scale
	m_vecScale.x = io.DisplaySize.x * MULT_X;
	m_vecScale.y = io.DisplaySize.y * MULT_Y;

	// font Size
	m_fFontSize = ScaleY(pSettings->Get().fFontSize);

	// mouse/touch
	m_bMousePressed = false;
	m_vecMousePos = ImVec2(0, 0);

	Log("GUI | Scale factor: %f, %f Font size: %f", m_vecScale.x, m_vecScale.y, m_fFontSize);

	// setup style
	ImGui::StyleColorsDark();
	SetupStyleColors();

	Log("GUI | Loading font: %s", pSettings->Get().szFont);
	m_pFont = LoadFont((char*)pSettings->Get().szFont);
	Log("GUI | ImFont pointer = 0x%X", m_pFont);

	// voice
	for(const auto& deviceInitCallback : Render::deviceInitCallbacks)
    {
        if(deviceInitCallback != nullptr) 
			deviceInitCallback();
    }

	bShowDebugLabels = false;
	Log("GUI Initialized.");
}

CGUI::~CGUI()
{
	// voice 
	for(const auto& deviceFreeCallback : Render::deviceFreeCallbacks)
	{
		if(deviceFreeCallback != nullptr) 
			deviceFreeCallback();
	}

	ImGui::DestroyContext();
}

ImFont* CGUI::LoadFont(char *font, float fontsize)
{
	ImGuiIO &io = ImGui::GetIO();

	// load fonts
	char path[0xFF];
	sprintf(path, "%sSAMP/fonts/%s", g_pszStorage, font);
	
	// cp1251 ranges
	static const ImWchar ranges[] =
	{
		0x0020, 0x0080,
		0x00A0, 0x00C0,
		0x0400, 0x0460,
		0x0490, 0x04A0,
		0x2010, 0x2040,
		0x20A0, 0x20B0,
		0x2110, 0x2130,
		0
	};

	ImFont* pFont = io.Fonts->AddFontFromFileTTF(path, fontsize == 0.0 ? GetFontSize() : fontsize, nullptr, ranges);
	return pFont;
}

void CGUI::Render()
{
	//if(pNetGame && pNetGame->GetTextDrawPool()) pNetGame->GetTextDrawPool()->Draw();

	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplRenderWare_NewFrame();
	ImGui::NewFrame();

	RenderVersion();
	RenderRakNetStatistics();

	if(pDebug) pDebug->Render();
	if(pPlayerTags) pPlayerTags->Render();
	if(pNetGame && pNetGame->GetLabelPool()) pNetGame->GetLabelPool()->Render();
	if(pNetGame && pNetGame->GetChatBubblePool()) pNetGame->GetChatBubblePool()->Render();
	if(pChatWindow) pChatWindow->Render();
	if(pDialogWindow) pDialogWindow->Draw();
	if(pSpawnScreen) pSpawnScreen->Render();
	if(pKeyBoard) pKeyBoard->Render();
	if(pExtraKeyBoard) pExtraKeyBoard->Render();
	if(pScoreBoard) pScoreBoard->Render();
	if(pDeathMessage) pDeathMessage->Render();

	// voice
	if(pNetGame)
	{
		if(pDialogWindow->m_bIsActive || pScoreBoard->m_bToggle || pNetGame->GetTextDrawPool()->GetState())
		{
			SpeakerList::Hide();
			MicroIcon::Hide();
		}
		else 
		{
			if(MicroIcon::hasShowed)
			{
				SpeakerList::Show();
				MicroIcon::Show();
			}
		}

		for(const auto& renderCallback : Render::renderCallbacks)
		{
			if(renderCallback != nullptr) 
				renderCallback();
		}
	}

	if(pNetGame)
	{
		CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
		if(pPlayerPed)
		{
			PED_TYPE *pActor = pPlayerPed->GetGtaActor();
			if(pActor)
			{
				ImGui::PushFontOutline(0xFF000000, 2);
				char szCoordText[128];
				sprintf(szCoordText, "X: %.4f - Y: %.4f - Z: %.4f", pActor->entity.mat->pos.X, pActor->entity.mat->pos.Y, pActor->entity.mat->pos.Z);
				ImVec2 _ImVec2 = ImVec2(ScaleX(60), RsGlobal->maximumHeight - ImGui::GetFontSize() * 0.85);
				RenderText(_ImVec2, ImColor(IM_COL32_WHITE), false, szCoordText, nullptr, ImGui::GetFontSize() * 0.85);
				ImGui::PopFontOutline();
			}
		}
	}

	// Debug label
	if(bShowDebugLabels)
	{
		if(pNetGame)
		{
			CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
			if(pVehiclePool)
			{
				for(VEHICLEID x = 0; x < MAX_VEHICLES; x++)
				{
					CVehicle *pVehicle = pVehiclePool->GetAt(x);
					if(pVehicle)
					{
						if(pVehicle->GetDistanceFromLocalPlayerPed() <= 20.0f)
						{
							MATRIX4X4 matVehicle;
							pVehicle->GetMatrix(&matVehicle);

							RwV3d rwPosition;
							rwPosition.x = matVehicle.pos.X;
							rwPosition.y = matVehicle.pos.Y;
							rwPosition.z = matVehicle.pos.Z;

							RwV3d rwOutResult;

							// CSPrite::CalcScreenCoors(RwV3d const&, RwV3d *, float *, float *, bool, bool) - 0x54EEC0
							((void (*)(RwV3d const&, RwV3d *, float *, float *, bool, bool))(g_libGTASA + 0x54EEC0 + 1))(rwPosition, &rwOutResult, 0, 0, 0, 0);
							if(rwOutResult.z < 1.0f)
								break;

							char szTextLabel[256];
							sprintf(szTextLabel, "[id: %d | model: %d | subtype: %d | Health: %.1f | preloaded: %d]\nDistance: %.2fm\ncPos: %.3f, %.3f, %.3f\nsPos: %.3f, %.3f, %.3f",
								x, pVehicle->GetModelIndex(), pVehicle->GetVehicleSubtype(), 
								pVehicle->GetHealth(), pGame->m_bIsVehiclePreloaded[pVehicle->GetModelIndex()],
								pVehicle->GetDistanceFromLocalPlayerPed(),
								matVehicle.pos.X, matVehicle.pos.Y, matVehicle.pos.Z,
								pVehiclePool->m_vecSpawnPos[x].X, pVehiclePool->m_vecSpawnPos[x].Y, pVehiclePool->m_vecSpawnPos[x].Z 
							);

							ImVec2 vecRealPos = ImVec2(rwOutResult.x, rwOutResult.y);
							Render3DLabel(vecRealPos, szTextLabel, 0x358BD4FF);
						}
					}
				}
			}
		}
	}
	
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplRenderWare_RenderDrawData(ImGui::GetDrawData());

	if(m_bNeedClearMousePos)
	{
		io.MousePos = ImVec2(-1, -1);
		m_bNeedClearMousePos = false;
	}
}

bool CGUI::OnTouchEvent(int type, bool multi, int x, int y)
{
	ImGuiIO& io = ImGui::GetIO();

	if(!pKeyBoard->OnTouchEvent(type, multi, x, y)) return false;
	if(!pChatWindow->OnTouchEvent(type, multi, x, y)) return false;

	if(pNetGame && pNetGame->GetTextDrawPool()) pNetGame->GetTextDrawPool()->OnTouchEvent(type, multi, x, y);

	switch(type)
	{
		case TOUCH_PUSH:
		io.MousePos = ImVec2(x, y);
		io.MouseDown[0] = true;
		break;

		case TOUCH_POP:
		io.MouseDown[0] = false;
		m_bNeedClearMousePos = true;
		break;

		case TOUCH_MOVE:
		io.MousePos = ImVec2(x, y);
		break;
	}

	return true;
}

void CGUI::RenderVersion()
{
	if(GetTickCount() - dwLastFpsUpdateTick >= 100)
    {
		dwLastFpsUpdateTick = GetTickCount();
		fFps = *(float*)(g_libGTASA + 0x8C9BA7 + 1);
		if(fFps >= 90.0)
			fFps = 90.0;
	}
		
	ImGui::Begin("DebugFPS", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
	ImGui::SetWindowPos(ImVec2(ScaleX(20), ScaleY(5)));
	ImGui::SetWindowSize(ImVec2(500, 250));
	ImGui::PushFontOutline(0xFF000000, 2);
	ImGui::Text("FPS: %.2f", fFps);
	ImGui::PopFontOutline();
	ImGui::End();

	ImVec2 _ImVec2 = ImVec2(RsGlobal->maximumWidth * 0.95, ScaleY(5));
	RenderText(_ImVec2, ImColor(SECONDARY_COLOR), true, "V.1.0.5", nullptr);
}

void CGUI::RenderRakNetStatistics()
{
		// StatisticsToString(rss, message, 0);

		//ImGui::GetOverlayDrawList()->AddText(
		//	ImVec2(ScaleX(10), ScaleY(400)),
		//	ImColor(IM_COL32_BLACK), message);
}

void CGUI::RenderText(ImVec2& posCur, ImU32 col, bool bOutline, const char* text_begin, const char* text_end, float font_size, ImFont *font, bool bOutlineUseTextColor)
{
	int iOffset = bOutlineUseTextColor ? 1 : pSettings->Get().iFontOutline;
	if(bOutline)
	{
		// left
		posCur.x -= iOffset;
		ImGui::GetBackgroundDrawList()->AddText(font, font_size == 0.0f ? GetFontSize() : font_size, posCur, bOutlineUseTextColor ? ImColor(col) : ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.x += iOffset;
		// right
		posCur.x += iOffset;
		ImGui::GetBackgroundDrawList()->AddText(font, font_size == 0.0f ? GetFontSize() : font_size, posCur, bOutlineUseTextColor ? ImColor(col) : ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.x -= iOffset;
		// above
		posCur.y -= iOffset;
		ImGui::GetBackgroundDrawList()->AddText(font, font_size == 0.0f ? GetFontSize() : font_size, posCur, bOutlineUseTextColor ? ImColor(col) : ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.y += iOffset;
		// below
		posCur.y += iOffset;
		ImGui::GetBackgroundDrawList()->AddText(font, font_size == 0.0f ? GetFontSize() : font_size, posCur, bOutlineUseTextColor ? ImColor(col) : ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.y -= iOffset;
	}

	ImGui::GetBackgroundDrawList()->AddText(font, font_size == 0.0f ? GetFontSize() : font_size, posCur, col, text_begin, text_end);
}

void CGUI::RenderOverlayText(ImVec2& posCur, ImU32 col, bool bOutline, const char* text_begin, const char* text_end)
{
	int iOffset = pSettings->Get().iFontOutline;

	if(bOutline)
	{
		posCur.x -= iOffset;
		ImGui::GetOverlayDrawList()->AddText(posCur, ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.x += iOffset;
		// right
		posCur.x += iOffset;
		ImGui::GetOverlayDrawList()->AddText(posCur, ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.x -= iOffset;
		// above
		posCur.y -= iOffset;
		ImGui::GetOverlayDrawList()->AddText(posCur, ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.y += iOffset;
		// below
		posCur.y += iOffset;
		ImGui::GetOverlayDrawList()->AddText(posCur, ImColor(IM_COL32_BLACK), text_begin, text_end);
		posCur.y -= iOffset;
	}
	ImGui::GetOverlayDrawList()->AddText(posCur, col, text_begin, text_end);
}

void CGUI::SetupStyleColors()
{
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

	style->WindowPadding = ImVec2(4, 4);
	style->WindowRounding = 0.0f;
	style->FrameRounding = 1.0f;
	style->ItemSpacing = ImVec2(8, 1.3);
	style->ScrollbarSize = ScaleY(35.0f);
	style->ScrollbarRounding = 1.0f;
	style->WindowBorderSize = 0.0f;
	style->ChildBorderSize  = 3.0f;
	style->FrameBorderSize  = 3.0f;
	style->ChildRounding = 2.75f;

	colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.79f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	colors[ImGuiCol_Border] = SECONDARY_COLOR;
	colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.7f, 0.12f, 0.12f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabHovered] = colors[ImGuiCol_ScrollbarGrab];
	colors[ImGuiCol_ScrollbarGrabActive] = colors[ImGuiCol_ScrollbarGrab];
	colors[ImGuiCol_Button] = ImVec4(0.0f, 0.200f, 0.255f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.253f, 0.138f, 0.0f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.253f, 0.138f, 0.0f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.7f, 0.12f, 0.12f, 1.0f);
	colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_Header];
	colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_Header];
}