#pragma once

#include "ESP8266_AT.h"
#include "WiFiDevice.h"
#include "WiFiBuffer.h"

using namespace EPRI;

///////////////////////
//// Pin Definitions //
///////////////////////
//#define WIFI_SW_RX	9	// WIFI UART0 RXI goes to Arduino pin 9
//#define WIFI_SW_TX	8	// WIFI UART0 TXO goes to Arduino pin 8

class ESP8266Server;
class ESP8266Client;

class ESP8266Device : public WiFiDevice
{
	friend class ESP8266Server;
	friend class ESP8266Client;
public:
	ESP8266Device(WiFi_GPIO_Pin Reset = {0,0}, WiFi_GPIO_Pin Enable = {0,0});
	
	bool Begin(STM32TCPSocket * pSocket);
	
	///////////////////////
	// Basic AT Commands //
	///////////////////////
	bool Test();
	bool Reset();
	bool Enable();
	bool Disable();
	int16_t GetVersion(char * ATversion, char * SDKversion, char * compileTime);
	bool Echo(bool enable);
	bool SetBaud(unsigned long baud);
	
	////////////////////
	// WiFi Functions //
	////////////////////
	int16_t WiFiGetMode();
	int16_t WiFiSetMode(wifi_mode mode);
	int16_t WiFiConnect(const char * ssid, const char * pwd);
	int16_t WiFiGetAP(char * ssid);
	int16_t WiFiLocalMAC(char * mac);
	int16_t WiFiDisconnect();
	IPAddress WiFiLocalIP();
	
	/////////////////////
	// TCP/IP Commands //
	/////////////////////
	int16_t TCPStatus();
	int16_t TCPUpdateStatus();
	int16_t TCPConnect(uint8_t linkID, const char * destination, uint16_t port, uint16_t keepAlive);
	int16_t TCPSend(uint8_t linkID, WiFiBuffer Data);
	int16_t TCPSend(uint8_t linkID, const uint8_t *buf, size_t size);
	int16_t TCPClose(uint8_t linkID);
	int16_t TCPSetTransferMode(uint8_t mode);
	int16_t TCPSetMux(uint8_t mux);
	int16_t TCPConfigureServer(uint16_t port, uint8_t create = 1);
	int16_t TCPPing(IPAddress ip);
	int16_t TCPPing(char * server);
	bool TCPIsConnected(uint8_t linkID);

	//////////////////////////
	// Custom GPIO Commands //
	//////////////////////////
	int16_t pinMode(uint8_t pin, uint8_t mode);
	int16_t digitalWrite(uint8_t pin, uint8_t state);
	int8_t digitalRead(uint8_t pin);
	
	///////////////////////////////////
	// Virtual Functions from Stream //
	///////////////////////////////////
	inline size_t Write(const char * buffer, size_t length = 0);
	inline size_t Write(WiFiBuffer Data);
	inline int Read(unsigned int timeoutInMS = 1000, size_t readLen = WIFI_RX_BUFFER_LEN, bool asynchronous = false);
	void Flush();

private:
	//////////////////////////
	// Command Send/Receive //
	//////////////////////////
	void sendCommand(const char * cmd, enum wifi_command_type type = WIFI_CMD_EXECUTE, WiFiBuffer params = {});		// const char * params = NULL
	int16_t readForResponse(const char * rsp, unsigned int timeoutInMS, size_t readLen = WIFI_RX_BUFFER_LEN);
	int16_t readForResponses(const char * pass, const char * fail, unsigned int timeout, size_t readLen = WIFI_RX_BUFFER_LEN);
	
	//////////////////
	// Buffer Stuff //
	//////////////////
	void clearBuffer();

	/// searchBuffer([test]) - Search buffer for string [test]
	/// Success: Returns pointer to beginning of string
	/// Fail: returns NULL
	char * searchBuffer(const char * test);
	
	uint8_t sync();

	//////////////////
	// Control pins //
	//////////////////
	WiFi_GPIO_Pin m_Reset;
	WiFi_GPIO_Pin m_Enable;
};
