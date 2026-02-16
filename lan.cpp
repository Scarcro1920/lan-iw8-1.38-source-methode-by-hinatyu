#include <std_include.hpp>
#include "game/game.hpp"
#include "loader/component_loader.hpp"

#include <utilities/hook.hpp>
#include <utilities/string.hpp>

#include "dvars.hpp"
#include "demonware.hpp"

#include "cmds.hpp"


struct XNADDR
{
	char addrBuff[84];
};


struct bdSecurityID
{
	char ab[8];
};


struct bdSecurityKey
{
	char ab[16];
};


struct XSECURITY_INFO
{
	bdSecurityID m_id;
	bdSecurityKey m_key;
};


struct XSESSION_INFO
{
	XNADDR m_address;
	XSECURITY_INFO m_security;
};


enum PartyType
{
	GAME_LOBBY_ID = 0x0,
	PRIVATE_PARTY_ID = 0x1,
	ANY_PARTY_ID = 0x2,
};

enum LocalClientNum_t
{
	LOCAL_CLIENT_INVALID = 0xFFFFFFFF,
	LOCAL_CLIENT_0 = 0x0,
	LOCAL_CLIENT_1 = 0x1,
	LOCAL_CLIENT_LAST = 0x1,
	LOCAL_CLIENT_COUNT = 0x2,
};


namespace lan
{
	
	void Party_StartLANServerJoin_f(const XSESSION_INFO* hostInfo, PartyType partyTypeNum, LocalClientNum_t localClientNum)
	{
		auto func = reinterpret_cast<void (*)(const XSESSION_INFO * hostInfo, PartyType partyTypeNum, LocalClientNum_t localClientNum)>(0x3608C80_b);
		func(hostInfo, partyTypeNum, localClientNum);
	}

	utilities::hook::detour NET_CreateSession_h;
	void NET_CreateSession_d(XSESSION_INFO* outSessionInfo)
	{
		NET_CreateSession_h.invoke<void>(outSessionInfo);

		std::string logLine = "[ \x1b[34m Debug \x1b[39m ] <NET_CreateSession> LAN data: ";
		char temp[8];

		// address buffer
		for (int i = 0; i < 84; i++)
		{
			sprintf(temp, "%02X", (unsigned char)outSessionInfo->m_address.addrBuff[i]);
			logLine += temp;
		}

		//  security ID
		for (int i = 0; i < 8; i++)
		{
			sprintf(temp, "%02X", (unsigned char)outSessionInfo->m_security.m_id.ab[i]);
			logLine += temp;
		}

		// security key
		for (int i = 0; i < 16; i++)
		{
			sprintf(temp, "%02X", (unsigned char)outSessionInfo->m_security.m_key.ab[i]);
			logLine += temp;
		}

		logger::write(logger::LOG_TYPE_TEST, logLine.c_str());
	}


	bool ParseHexStringToSessionInfo(const char* hexData, XSESSION_INFO* sessionInfo)
	{
		std::string data = hexData;

		if (data.length() != 216)
		{
			printf("[ \x1b[31m Error \x1b[39m ] The length is different %zu\n", data.length());
			return false;
		}

		std::vector<unsigned char> bytes;

		for (size_t i = 0; i < data.length(); i += 2)
		{
			std::string hexByte = data.substr(i, 2);
			unsigned int byte;
			std::stringstream ss;
			ss << std::hex << hexByte;
			ss >> byte;
			bytes.push_back(static_cast<unsigned char>(byte));
		}

		memcpy(sessionInfo->m_address.addrBuff, bytes.data(), 84);
		memcpy(sessionInfo->m_security.m_id.ab, bytes.data() + 84, 8);
		memcpy(sessionInfo->m_security.m_key.ab, bytes.data() + 92, 16);

		return true;
	}

	void connect_ip_f()
	{
		if (cmds::Cmd_Argc() < 2)
		{
			logger::write(logger::LOG_TYPE_ERROR, "Invalid command. Missing IP/session data.\n");
			return;
		}

		char ip_buf[256];
		cmds::Cmd_ArgvBuffer(1, ip_buf, sizeof(ip_buf));

		logger::write(logger::LOG_TYPE_INFO, "[ \x1b[34m Debug \x1b[39m ] <ConnectIP> Attempting to connect to: %s\n", ip_buf);

		XSESSION_INFO hostInfo;
		memset(&hostInfo, 0, sizeof(XSESSION_INFO));

		if (!ParseHexStringToSessionInfo(ip_buf, &hostInfo))
		{
			logger::write(logger::LOG_TYPE_INFO, "[ \x1b[31m Error \x1b[39m ] <ConnectIP> Failed to parse LAN data\n");
			return;
		}

		Party_StartLANServerJoin_f(
			&hostInfo,
			PartyType::PRIVATE_PARTY_ID,
			LocalClientNum_t::LOCAL_CLIENT_0
		);

		logger::write(logger::LOG_TYPE_INFO, "[ \x1b[32m Success \x1b[39m ] <ConnectIP> function called\n");
	}



	class component final : public component_interface
	{
	public:
		void pre_start() override
		{

		}

		void post_unpack() override
		{
			NET_CreateSession_h.create(0x2AB16A0_b, NET_CreateSession_d);

			static game::ClCommandInfo join_lan_cmd;
			join_lan_cmd.name = "lan_join";
			join_lan_cmd.function = connect_ip_f;
			cmds::Cmd_AddClientCommand(&join_lan_cmd);


		}
	};
}

//REGISTER_COMPONENT(lan::component)