#pragma once

#include <list>
#include <string>

struct DeathMessageStruct
{
	std::string playerName;
	std::string killerName;
	uint8_t reason;
};

class CDeathMessage
{
public:
	CDeathMessage();
	~CDeathMessage();

	void MakeRecord(const char* playerId, const char* killerId, uint8_t reason);
	void Render();

private:
	std::list<DeathMessageStruct*> m_pDeathMessage;

};