#include "cmsis_os.h"
#include <ESP8266_WiFi.h>

#define WIFI_DISABLE_ECHO

////////////////////////
// Buffer Definitions //
////////////////////////
WiFiBuffer wifiTxBuffer;
WiFiBuffer wifiRxBuffer(WIFI_RX_BUFFER_LEN);

////////////////////
// Initialization //
////////////////////

ESP8266Device::ESP8266Device(WiFi_GPIO_Pin Reset /*= {0,0}*/, WiFi_GPIO_Pin Enable /*= {0,0}*/)
	: WiFiDevice()
	, m_Reset(Reset)
	, m_Enable(Enable)
{
	for (int i=0; i<WIFI_MAX_SOCK_NUM; i++)
		m_State[i] = AVAILABLE;
}

bool ESP8266Device::Begin(STM32TCPSocket * pSocket)		// OK if sendCommand and readForResponse are OK
{
	m_Serial = pSocket;
	Reset();
	if(Test())
	{
		if(!WiFiSetMode(WIFI_MODE_STA))
			return false;
		return true;
	}

	return false;
}

///////////////////////
// Basic AT Commands //
///////////////////////

bool ESP8266Device::Test()
{
	sendCommand(ESP8266_TEST); // Send AT

	if (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0)
		return true;
	
	return false;
}

bool ESP8266Device::Reset()
{
	if(m_Reset.Pin)
	{
		HAL_GPIO_WritePin(m_Reset.GPIO_Port, m_Reset.Pin, GPIO_PIN_RESET);
		osDelay(1000);
		HAL_GPIO_WritePin(m_Reset.GPIO_Port, m_Reset.Pin, GPIO_PIN_SET);
		this->Read(1000);
		this->Flush();
	}

	sendCommand(ESP8266_RESET); // Send AT+RST
	
	if (readForResponse(RESPONSE_READY, COMMAND_RESET_TIMEOUT) > 0 and osOK == osDelay(2000))
		return true;
	
	return false;
}


bool ESP8266Device::Enable()
{
	if(m_Enable.Pin)
	{
		HAL_GPIO_WritePin(m_Enable.GPIO_Port, m_Enable.Pin, GPIO_PIN_SET);
	}
	return true;
}

bool ESP8266Device::Disable()
{
	if(m_Enable.Pin)
	{
		HAL_GPIO_WritePin(m_Enable.GPIO_Port, m_Enable.Pin, GPIO_PIN_RESET);
	}
	return true;
}

bool ESP8266Device::Echo(bool enable)
{
	if (enable)
		sendCommand(ESP8266_ECHO_ENABLE);
	else
		sendCommand(ESP8266_ECHO_DISABLE);
	
	if (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0)
		return true;
	
	return false;
}

bool ESP8266Device::SetBaud(unsigned long baud)
{
	char parameters[16];
	memset(parameters, 0, 16);

	// Constrain parameters:
	baud = std::max(110UL, baud);
	baud = std::min(baud, 115200UL);
	
	// Put parameters into string
	sprintf(parameters, "%lu,8,1,0,0", baud);
	
	// Send AT+UART=baud,databits,stopbits,parity,flowcontrol
	sendCommand(ESP8266_UART, WIFI_CMD_SETUP, WiFiBuffer(parameters, strlen(parameters)));
	
	if (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0)
		return true;
	
	return false;
}

int16_t ESP8266Device::GetVersion(char * ATversion, char * SDKversion, char * compileTime)
{
	sendCommand(ESP8266_VERSION); // Send AT+GMR
	// Example Response: AT version:0.30.0.0(Jul  3 2015 19:35:49)\r\n (43 chars)
	//                   SDK version:1.2.0\r\n (19 chars)
	//                   compile time:Jul  7 2015 18:34:26\r\n (36 chars)
	//                   OK\r\n
	// (~101 characters)
	// Look for "OK":
	int16_t rsp = (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0);
	if (rsp > 0)
	{
		char *p, *q;
		// Look for "AT version" in the rxBuffer
		p = strstr((const char *)wifiRxBuffer.GetData(), "AT version:");
		if (p == NULL) return WIFI_RSP_UNKNOWN;
		p += strlen("AT version:");
		q = strchr(p, '\r'); // Look for \r
		if (q == NULL) return WIFI_RSP_UNKNOWN;
		strncpy(ATversion, p, q-p);

		// Look for "SDK version:" in the rxBuffer
		p = strstr((const char *)wifiRxBuffer.GetData(), "SDK version:");
		if (p == NULL) return WIFI_RSP_UNKNOWN;
		p += strlen("SDK version:");
		q = strchr(p, '\r'); // Look for \r
		if (q == NULL) return WIFI_RSP_UNKNOWN;
		strncpy(SDKversion, p, q-p);
		
		// Look for "compile time:" in the rxBuffer
		p = strstr((const char *)wifiRxBuffer.GetData(), "compile time:");
		if (p == NULL) return WIFI_RSP_UNKNOWN;
		p += strlen("compile time:");
		q = strchr(p, '\r'); // Look for \r
		if (q == NULL) return WIFI_RSP_UNKNOWN;
		strncpy(compileTime, p, q-p);
	}
	
	return rsp;
}

////////////////////
// WiFi Functions //
////////////////////

// WiFiGetMode()
// Input: None
// Output:
//    - Success: 1, 2, 3 (WIFI_MODE_STA, WIFI_MODE_AP, WIFI_MODE_STAAP)
//    - Fail: <0 (wifi_cmd_rsp)
int16_t ESP8266Device::WiFiGetMode()
{
	sendCommand(ESP8266_WIFI_MODE, WIFI_CMD_QUERY);
	
	// Example response: \r\nAT+CWMODE_CUR?\r+CWMODE_CUR:2\r\n\r\nOK\r\n
	// Look for the OK:
	int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	if (rsp > 0)
	{
		// Then get the number after ':':
		char * p = strchr((const char *)wifiRxBuffer.GetData(), ':');
		if (p != NULL)
		{
			char mode = *(p+1);
			if ((mode >= '1') && (mode <= '3'))
				return (mode - 48); // Convert ASCII to decimal
		}
		
		return WIFI_RSP_UNKNOWN;
	}
	
	return rsp;
}

// WiFiSetMode()
// Input: 1, 2, 3 (WIFI_MODE_STA, WIFI_MODE_AP, WIFI_MODE_STAAP)
// Output:
//    - Success: >0
//    - Fail: <0 (wifi_cmd_rsp)
int16_t ESP8266Device::WiFiSetMode(wifi_mode mode)
{
	WiFiBuffer params;
	params.Append(std::to_string(mode));
	sendCommand(ESP8266_WIFI_MODE, WIFI_CMD_SETUP, params);
	
	return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

// WiFiConnect()
// Input: ssid and pwd const char's
// Output:
//    - Success: >0
//    - Fail: <0 (wifi_cmd_rsp)
int16_t ESP8266Device::WiFiConnect(const char * ssid, const char * pwd)
{
	WiFiBuffer params;
	params.AppendBuffer("\"", 1U);
	params.AppendBuffer(ssid, strlen(ssid));
	params.AppendBuffer("\"", 1U);
	if (pwd != NULL)
	{
		params.AppendBuffer(",\"", 2U);
		params.AppendBuffer(pwd, strlen(pwd));
		params.AppendBuffer("\"", 1U);
	}
	sendCommand(ESP8266_CONNECT_AP, WIFI_CMD_SETUP, params);

	return readForResponses(RESPONSE_OK, RESPONSE_FAIL, WIFI_CONNECT_TIMEOUT);
//	return readForResponses("WIFI CONNECTED", RESPONSE_FAIL, WIFI_CONNECT_TIMEOUT);
}

int16_t ESP8266Device::WiFiGetAP(char * ssid)
{
	sendCommand(ESP8266_CONNECT_AP, WIFI_CMD_QUERY); // Send "AT+CWJAP?"
	
	int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	// Example Responses: No AP\r\n\r\nOK\r\n
	// - or -
	// +CWJAP:"WiFiSSID","00:aa:bb:cc:dd:ee",6,-45\r\n\r\nOK\r\n
	if (rsp > 0)
	{
		// Look for "No AP"
		if (strstr((const char *)wifiRxBuffer.GetData(), "No AP") != NULL)
			return 0;
		
		// Look for "+CWJAP"
		char * p = strstr((const char *)wifiRxBuffer.GetData(), ESP8266_CONNECT_AP);
		if (p != NULL)
		{
			p += strlen(ESP8266_CONNECT_AP) + 2;
			char * q = strchr(p, '"');
			if (q == NULL) return WIFI_RSP_UNKNOWN;
			strncpy(ssid, p, q-p); // Copy string to temp char array:
			return 1;
		}
	}
	
	return rsp;
}

int16_t ESP8266Device::WiFiDisconnect()
{
	sendCommand(ESP8266_DISCONNECT); // Send AT+CWQAP
	// Example response: \r\n\r\nOK\r\nWIFI DISCONNECT\r\n
	// "WIFI DISCONNECT" comes up to 500ms _after_ OK. 
	int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	if (rsp > 0)
	{
		rsp = readForResponse("WIFI DISCONNECT", COMMAND_RESPONSE_TIMEOUT);
		if (rsp > 0)
			return rsp;
		return 1;
	}
	
	return rsp;
}

// TCPStatus()
// Input: none
// Output:
//    - Success: 2, 3, 4, or 5 (WIFI_STATUS_GOTIP, WIFI_STATUS_CONNECTED, WIFI_STATUS_DISCONNECTED, WIFI_STATUS_NOWIFI)
//    - Fail: <0 (wifi_cmd_rsp)
int16_t ESP8266Device::TCPStatus()
{
	int16_t statusRet = TCPUpdateStatus();
	if (statusRet > 0)
	{
		switch (m_Status.stat)
		{
		case WIFI_STATUS_GOTIP: // 3
		case WIFI_STATUS_DISCONNECTED: // 4 - "Client" disconnected, not wifi
			return 1;
			break;
		case WIFI_STATUS_CONNECTED: // Connected, but haven't gotten an IP
		case WIFI_STATUS_NOWIFI: // No WiFi configured
			return 0;
			break;
		}
	}
	return statusRet;
}

int16_t ESP8266Device::TCPUpdateStatus()
{
	sendCommand(ESP8266_TCP_STATUS); // Send AT+CIPSTATUS\r\n
	// Example response: (connected as client)
	// STATUS:3\r\n
	// +CIPSTATUS:0,"TCP","93.184.216.34",80,0\r\n\r\nOK\r\n 
	// - or - (clients connected to WIFI server)
	// STATUS:3\r\n
	// +CIPSTATUS:0,"TCP","192.168.0.100",54723,1\r\n
	// +CIPSTATUS:1,"TCP","192.168.0.101",54724,1\r\n\r\nOK\r\n 
	int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	if (rsp > 0)
	{
		char * p = searchBuffer("STATUS:");
		if (p == NULL)
			return WIFI_RSP_UNKNOWN;
		
		p += strlen("STATUS:");
		m_Status.stat = (wifi_connect_status)(*p - 48);
		
		for (int i=0; i<WIFI_MAX_SOCK_NUM; i++)
		{
			p = strstr(p, "+CIPSTATUS:");
			if (p == NULL)
			{
				// Didn't find any IPSTATUS'. Set linkID to 255.
				for (int j=i; j<WIFI_MAX_SOCK_NUM; j++)
					m_Status.ipstatus[j].linkID = 255;
				return rsp;
			}
			else
			{
				p += strlen("+CIPSTATUS:");
				// Find linkID:
				uint8_t linkId = *p - 48;
				if (linkId >= WIFI_MAX_SOCK_NUM)
					return rsp;
				m_Status.ipstatus[linkId].linkID = linkId;
				
				// Find type (p pointing at linkID):
				p += 3; // Move p to either "T" or "U"
				if (*p == 'T')
					m_Status.ipstatus[linkId].type = WIFI_TCP;
				else if (*p == 'U')
					m_Status.ipstatus[linkId].type = WIFI_UDP;
				else
					m_Status.ipstatus[linkId].type = WIFI_TYPE_UNDEFINED;
				
				// Find remoteIP (p pointing at first letter or type):
				p += 6; // Move p to first digit of first octet.
				for (uint8_t j = 0; j < 4; j++)
				{
					char tempOctet[4];
					memset(tempOctet, 0, 4); // Clear tempOctet
					
					size_t octetLength = strspn(p, "0123456789"); // Find length of numerical string:
					
					strncpy(tempOctet, p, octetLength); // Copy string to temp char array:
					m_Status.ipstatus[linkId].remoteIP[j] = atoi(tempOctet); // Move the temp char into IP Address octet
					
					p += (octetLength + 1); // Increment p to next octet
				}
				
				// Find port (p pointing at ',' between IP and port:
				p += 1; // Move p to first digit of port
				char tempPort[6];
				memset(tempPort, 0, 6);
				size_t portLen = strspn(p, "0123456789"); // Find length of numerical string:
				strncpy(tempPort, p, portLen);
				m_Status.ipstatus[linkId].port = atoi(tempPort);
				p += portLen + 1;
				
				// Find tetype (p pointing at tetype)
				if (*p == '0')
					m_Status.ipstatus[linkId].tetype = WIFI_CLIENT;
				else if (*p == '1')
					m_Status.ipstatus[linkId].tetype = WIFI_SERVER;
			}
		}
	}
	
	return rsp;
}

// WiFiLocalIP()
// Input: none
// Output:
//    - Success: Device's local IPAddress
//    - Fail: 0
IPAddress ESP8266Device::WiFiLocalIP()
{
	sendCommand(ESP8266_GET_LOCAL_IP); // Send AT+CIFSR\r\n
	// Example Response: +CIFSR:STAIP,"192.168.0.114"\r\n
	//                   +CIFSR:STAMAC,"18:fe:34:9d:b7:d9"\r\n
	//                   \r\n
	//                   OK\r\n
	// Look for the OK:
	int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	if (rsp > 0)
	{
		// Look for "STAIP" in the rxBuffer
		char * p = strstr((const char *)wifiRxBuffer.GetData(), "STAIP");
		if (p != NULL)
		{
			IPAddress returnIP;
			
			p += 7; // Move p seven places. (skip STAIP,")
			for (uint8_t i = 0; i < 4; i++)
			{
				char tempOctet[4];
				memset(tempOctet, 0, 4); // Clear tempOctet
				
				size_t octetLength = strspn(p, "0123456789"); // Find length of numerical string:
				if (octetLength >= 4) // If it's too big, return an error
					return WIFI_RSP_UNKNOWN;
				
				strncpy(tempOctet, p, octetLength); // Copy string to temp char array:
				returnIP[i] = atoi(tempOctet); // Move the temp char into IP Address octet
				
				p += (octetLength + 1); // Increment p to next octet
			}
			
			return returnIP;
		}
	}
	
	return rsp;
}

int16_t ESP8266Device::WiFiLocalMAC(char * mac)
{
	sendCommand(ESP8266_GET_STA_MAC, WIFI_CMD_QUERY); // Send "AT+CIPSTAMAC?"

	int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);

	if (rsp > 0)
	{
		// Look for "+CIPSTAMAC"
		char * p = strstr((const char *)wifiRxBuffer.GetData(), ESP8266_GET_STA_MAC);
		if (p != NULL)
		{
			p += strlen(ESP8266_GET_STA_MAC) + 2;
			char * q = strchr(p, '"');
			if (q == NULL) return WIFI_RSP_UNKNOWN;
			p = ++q;
			q = strchr(q, '"');
			strncpy(mac, p, q - p); // Copy string to temp char array:
			return 1;
		}
	}

	return rsp;
}

/////////////////////
// TCP/IP Commands //
/////////////////////

int16_t ESP8266Device::TCPConnect(uint8_t linkID, const char * destination, uint16_t port, uint16_t keepAlive)
{
	WiFiBuffer params;
	params.Append(linkID);
	params.AppendBuffer(",\"TCP\",\"", 8U);
	params.AppendBuffer(destination, strlen(destination));
	params.AppendBuffer("\",", 2U);
	params.Append(std::to_string(port));
	if (keepAlive > 0)
	{
		params.AppendBuffer(",", 1U);
		// keepAlive is in units of 500 milliseconds.
		// Max is 7200 * 500 = 3600000 ms = 60 minutes.
		params.Append(std::to_string(keepAlive / 500));
	}
	sendCommand(ESP8266_TCP_CONNECT, WIFI_CMD_SETUP, params);

	// Example good: CONNECT\r\n\r\nOK\r\n
	// Example bad: DNS Fail\r\n\r\nERROR\r\n
	// Example meh: ALREADY CONNECTED\r\n\r\nERROR\r\n
	int16_t rsp = readForResponses(RESPONSE_OK, RESPONSE_ERROR, CLIENT_CONNECT_TIMEOUT);
	
	if (rsp < 0)
	{
		// We may see "ERROR", but be "ALREADY CONNECTED".
		// Search for "ALREADY", and return success if we see it.
		char * p = searchBuffer("ALREADY");
		if (p != NULL)
			return 2;
		// Otherwise the connection failed. Return the error code:
		return rsp;
	}
	// Return 1 on successful (new) connection
	return 1;
}

int16_t ESP8266Device::TCPSend(uint8_t linkID, WiFiBuffer Data)					// Himanshu
{
	if (Data.Size() > WIFI_MAX_TCP_LEN)
		return WIFI_CMD_BAD;
	char params[8];
	sprintf(params, "%d,%d", linkID, Data.Size());
	sendCommand(ESP8266_TCP_SEND, WIFI_CMD_SETUP, WiFiBuffer(params, strlen(params)));

	int16_t rsp = readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_RESPONSE_TIMEOUT);
	if (rsp >= 0)
	{
		this->Write(Data);
		size_t readLen = strlen("\r\nRecv ") + std::to_string(Data.Size()).size() + strlen(" bytes\r\n\r\nSEND OK\r\n");
		rsp = readForResponse("SEND OK", COMMAND_RESPONSE_TIMEOUT, readLen);
		if (rsp > 0)
			return Data.Size();
	}

	return rsp;
}

int16_t ESP8266Device::TCPSend(uint8_t linkID, const uint8_t *buf, size_t size)	//! TODO - modify the Read function in Socket class.
{
	if (size > WIFI_MAX_TCP_LEN)
		return WIFI_CMD_BAD;
	WiFiBuffer params;
	params.Append(std::to_string(linkID) + "," + std::to_string(size));
	sendCommand(ESP8266_TCP_SEND, WIFI_CMD_SETUP, params);
	
	int16_t rsp = readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_RESPONSE_TIMEOUT);
	//if (rsp > 0)
	if (rsp != WIFI_RSP_FAIL)
	{
		this->Write((const char *)buf, size);
		size_t readLen = strlen("Recv ") + std::to_string(size).size() + strlen(" bytes\r\n\r\nSEND OK\r\n");
		rsp = readForResponse("SEND OK", COMMAND_RESPONSE_TIMEOUT, readLen);
		if (rsp > 0)
			return size;
	}
	
	return rsp;
}

int16_t ESP8266Device::TCPClose(uint8_t linkID)	// upto 5??
{
	WiFiBuffer params;
	params.Append(std::to_string(linkID));
	sendCommand(ESP8266_TCP_CLOSE, WIFI_CMD_SETUP, params);
	
	// Eh, client virtual function doesn't have a return value.
	// We'll wait for the OK or timeout anyway.
	return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

int16_t ESP8266Device::TCPSetTransferMode(uint8_t mode)
{
	char params[2] = {0, 0};
	params[0] = (mode > 0) ? '1' : '0';
	sendCommand(ESP8266_TRANSMISSION_MODE, WIFI_CMD_SETUP, WiFiBuffer(params, sizeof params));
	
	return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

int16_t ESP8266Device::TCPSetMux(uint8_t mux)
{
	char params[2] = {0, 0};
	params[0] = (mux > 0) ? '1' : '0';
	sendCommand(ESP8266_TCP_MULTIPLE, WIFI_CMD_SETUP, WiFiBuffer(params, sizeof params));
	
	return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

int16_t ESP8266Device::TCPConfigureServer(uint16_t port, uint8_t create)
{
	char params[10];
	if (create > 1) create = 1;
	sprintf(params, "%d,%d", create, port);
	sendCommand(ESP8266_SERVER_CONFIG, WIFI_CMD_SETUP, WiFiBuffer(params, sizeof params));
	
	return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);	
}

int16_t ESP8266Device::TCPPing(IPAddress ip)
{
	char ipStr[17];
	sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	return TCPPing(ipStr);
}

int16_t ESP8266Device::TCPPing(char * server)
{
	char params[strlen(server) + 3];
	sprintf(params, "\"%s\"", server);
	// Send AT+Ping=<server>
	sendCommand(ESP8266_PING, WIFI_CMD_SETUP, WiFiBuffer(params, sizeof params));
	// Example responses:
	//  * Good response: +12\r\n\r\nOK\r\n
	//  * Timeout response: +timeout\r\n\r\nERROR\r\n
	//  * Error response (unreachable): ERROR\r\n\r\n
	int16_t rsp = readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_PING_TIMEOUT);
	if (rsp > 0)
	{
		char * p = searchBuffer("+");
		p += 1; // Move p forward 1 space
		char * q = strchr(p, '\r'); // Find the first \r
		if (q == NULL)
			return WIFI_RSP_UNKNOWN;
		char tempRsp[10];
		strncpy(tempRsp, p, q - p);
		return atoi(tempRsp);
	}
	else
	{
		if (searchBuffer("timeout") != NULL)
			return 0;
	}
	
	return rsp;
}

bool ESP8266Device::TCPIsConnected(uint8_t linkID)
{
	return true;	// TODO - use linkID for MUXing
}

//////////////////////////
// Custom GPIO Commands //
//////////////////////////
int16_t ESP8266Device::pinMode(uint8_t pin, uint8_t mode)		// DO NOT USE THIS YET - DANGEROUS
{
//	char params[5];
//
//	char modeC = 'i'; // Default mode to input
//	if (mode == GPIO_MODE_OUTPUT_PP)
//		modeC = 'o'; // o = OUTPUT
//	else if (mode == GPIO_PULLUP)
//		modeC = 'p'; // p = INPUT_PULLUP
//
//	snprintf(params, sizeof params, "%d,%c", pin, modeC);
//	sendCommand(ESP8266_PINMODE, WIFI_CMD_SETUP, params);
//
//	return readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_RESPONSE_TIMEOUT);
	return -1;
}

int16_t ESP8266Device::digitalWrite(uint8_t pin, uint8_t state)	// DO NOT USE THIS YET - DANGEROUS
{
//	char params[5];
//
//	char stateC = 'l'; // Default state to LOW
//	if (state == SET)
//		stateC = 'h'; // h = HIGH
//
//	snprintf(params, sizeof params, "%d,%c", pin, stateC);
//	sendCommand(ESP8266_PINWRITE, WIFI_CMD_SETUP, params);
//
//	return readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_RESPONSE_TIMEOUT);
	return -1;
}

int8_t ESP8266Device::digitalRead(uint8_t pin)					// DO NOT USE THIS YET - DANGEROUS
{
//	char params[3];
//
//	snprintf(params, sizeof params, "%d", pin);
//	sendCommand(ESP8266_PINREAD, WIFI_CMD_SETUP, params); // Send AT+PINREAD=n\r\n
//	// Example response: 1\r\n\r\nOK\r\n
//	if (readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_RESPONSE_TIMEOUT) > 0)
//	{
//		if (strchr((const char *)wifiRxBuffer.GetData(), '0') != NULL)
//			return RESET;
//		else if (strchr((const char *)wifiRxBuffer.GetData(), '1') != NULL)
//			return SET;
//	}
	
	return -1;
}

//////////////////////////////
// Stream Virtual Functions //
//////////////////////////////

inline size_t ESP8266Device::Write(const char * buffer, size_t length /*= 0*/)	// for sending {commands}
{
	if(m_Serial->Write(buffer, length) == SUCCESSFUL)
	{
		if(length)
			return length;
		return strlen(buffer);
	}
	return -1;
}

inline size_t ESP8266Device::Write(WiFiBuffer Data)							// for sending {data}
{
#ifdef DEBUG
		if(WIFI_DEBUG_LVL >= LVL_HIGH)
			Base()->GetDebug()->TRACE_VECTOR("SW", wifiRxBuffer);
#endif

	if(m_Serial->STM32SerialSocket::Write(Data) == SUCCESSFUL)
		return Data.Size();
	return -1;
}

inline int ESP8266Device::Read(unsigned int timeoutInMS /*= 1000*/, size_t readLen /*= WIFI_RX_BUFFER_LEN*/, bool asynchronous /*= false*/)
{
	size_t ActualBytes = 0;
	ERROR_TYPE RetVal = not SUCCESSFUL;
	if(not asynchronous)
	{
		RetVal = m_Serial->Read(&wifiRxBuffer, readLen, timeoutInMS, &ActualBytes);
	}
	else
	{
		throw std::runtime_error("Not yet implemented.\r\n");
	}
	if(RetVal == SUCCESSFUL or RetVal == ERR_TIMEOUT)
	{
		return ActualBytes;
	}
	return 0;
}

void ESP8266Device::Flush()
{
	m_Serial->Flush(STM32SerialSocket::BOTH);
	clearBuffer();
}

//////////////////////////////////////////////////
// Private, Low-Level, Ugly, Hardware Functions //
//////////////////////////////////////////////////

void ESP8266Device::sendCommand(const char * cmd, enum wifi_command_type type, WiFiBuffer params)		// const char * params // OK
{
	wifiTxBuffer.Clear();

	wifiTxBuffer.AppendBuffer("AT", 2U);
	wifiTxBuffer.AppendBuffer(cmd, strlen(cmd));
	if (type == WIFI_CMD_QUERY)
		wifiTxBuffer.AppendBuffer("?", 1U);
	else if (type == WIFI_CMD_SETUP)
	{
		wifiTxBuffer.AppendBuffer("=", 1U);
		wifiTxBuffer.Append(&params);
	}
	wifiTxBuffer.AppendBuffer("\r\n\0", 3);

#ifdef DEBUG
		if(WIFI_DEBUG_LVL >= LVL_HIGH)
			Base()->GetDebug()->TRACE_VECTOR("SW", wifiRxBuffer);
#endif

	if(WIFI_DEBUG_LVL >= LVL_LOW)
		printf("\r\nCommand : %s\r\n", (const char *)wifiTxBuffer.GetData());

	this->Write((const char *)wifiTxBuffer.GetData(), wifiTxBuffer.Size() - 1);
}

int16_t ESP8266Device::readForResponse(const char * rsp, unsigned int timeoutInMS, size_t readLen /*= WIFI_RX_BUFFER_LEN*/)	// Not to be used in transparent communications
{
	clearBuffer();
	size_t ActualBytes = 0, TotalBytes = 0;
	do {
		ActualBytes = this->Read(timeoutInMS, readLen, false);
		TotalBytes += ActualBytes;
	} while(readLen == WIFI_RX_BUFFER_LEN and ActualBytes != 0 and ActualBytes == WIFI_RX_BUFFER_LEN);

	if(TotalBytes > 0)
	{
#ifdef DEBUG
		if(WIFI_DEBUG_LVL >= LVL_HIGH)
			Base()->GetDebug()->TRACE_VECTOR("SR", wifiRxBuffer);
#endif
		if(WIFI_DEBUG_LVL >= LVL_LOW)
			printf("Response : %s\r\n===\r\n\r\n", (const char *)wifiRxBuffer.GetData());
	}

	if (searchBuffer(rsp))
		return TotalBytes;
	
	if (TotalBytes > 0)
		return WIFI_RSP_UNKNOWN;
	else
		return WIFI_RSP_TIMEOUT;
}

int16_t ESP8266Device::readForResponses(const char * pass, const char * fail, unsigned int timeoutInMS, size_t readLen /*= WIFI_RX_BUFFER_LEN*/)
{
	clearBuffer();
	size_t ActualBytes = 0, TotalBytes = 0;
	do {
		ActualBytes = this->Read(timeoutInMS, readLen, false);
		TotalBytes += ActualBytes;
	} while(ActualBytes != 0 and ActualBytes == readLen);

	if(TotalBytes > 0)
	{
#ifdef DEBUG
		if(WIFI_DEBUG_LVL >= LVL_HIGH)
			Base()->GetDebug()->TRACE_VECTOR("SR", wifiRxBuffer);
#endif
		if(WIFI_DEBUG_LVL >= LVL_LOW)
			printf("Response : %s\r\n===\r\n\r\n", (const char *)wifiRxBuffer.GetData());
	}

	if (searchBuffer(pass))	// Search the buffer for goodRsp
		return TotalBytes;	// Return how number of chars read
	if (searchBuffer(fail))
		return WIFI_RSP_FAIL;
	
	if (TotalBytes > 0)
		return WIFI_RSP_UNKNOWN;
	else
		return WIFI_RSP_TIMEOUT;
}

//////////////////
// Buffer Stuff //
//////////////////
void ESP8266Device::clearBuffer()
{
	memset(&wifiRxBuffer[0], '\0', wifiRxBuffer.Size());
	wifiRxBuffer.Clear();
}

char * ESP8266Device::searchBuffer(const char * test)
{
	return strstr((const char *)wifiRxBuffer.GetData(), test);
}
