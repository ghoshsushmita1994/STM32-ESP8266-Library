#pragma once

#include "IPAddress.h"
#include "STM32TCP.h"

namespace EPRI{
	class STM32TCPSocket;
}

using namespace EPRI;

////////////////////////
// Buffer Definitions //
////////////////////////
#ifndef WIFI_RX_BUFFER_LEN
#define WIFI_RX_BUFFER_LEN 128 // Number of bytes in the serial receive buffer
#endif

///////////////////////////////
// Command Response Timeouts //
///////////////////////////////
#define COMMAND_RESPONSE_TIMEOUT 1000
#define COMMAND_PING_TIMEOUT 3000
#define WIFI_CONNECT_TIMEOUT 30000
#define COMMAND_RESET_TIMEOUT 5000
#define CLIENT_CONNECT_TIMEOUT 5000

#define WIFI_MAX_SOCK_NUM 5
#define WIFI_SOCK_NOT_AVAIL 255
#define WIFI_MAX_TCP_LEN 2048

enum wifi_cmd_rsp {
	WIFI_CMD_BAD = -5,
	WIFI_RSP_MEMORY_ERR = -4,
	WIFI_RSP_FAIL = -3,
	WIFI_RSP_UNKNOWN = -2,
	WIFI_RSP_TIMEOUT = -1,
	WIFI_RSP_SUCCESS = 0
};

enum wifi_mode {
	WIFI_MODE_STA = 1,
	WIFI_MODE_AP = 2,
	WIFI_MODE_STAAP = 3
};

enum wifi_command_type {
	WIFI_CMD_QUERY,
	WIFI_CMD_SETUP,
	WIFI_CMD_EXECUTE
};

enum wifi_encryption {
	WIFI_ECN_OPEN,
	WIFI_ECN_WPA_PSK,
	WIFI_ECN_WPA2_PSK,
	WIFI_ECN_WPA_WPA2_PSK
};

enum wifi_connect_status {
	WIFI_STATUS_GOTIP = 2,
	WIFI_STATUS_CONNECTED = 3,
	WIFI_STATUS_DISCONNECTED = 4,
	WIFI_STATUS_NOWIFI = 5
};

enum wifi_socketm_State {
	AVAILABLE = 0,
	TAKEN = 1,
};

enum wifi_connection_type {
	WIFI_TCP,
	WIFI_UDP,
	WIFI_TYPE_UNDEFINED
};

enum wifi_tetype {
	WIFI_CLIENT,
	WIFI_SERVER
};

struct wifi_ipstatus
{
	uint8_t linkID;
	wifi_connection_type type;
	IPAddress remoteIP;
	uint16_t port;
	wifi_tetype tetype;
};

struct wifi_status
{
	wifi_connect_status stat;
	wifi_ipstatus ipstatus[WIFI_MAX_SOCK_NUM];
};

struct WiFi_GPIO_Pin {
	GPIO_TypeDef* GPIO_Port;
	uint16_t Pin;
	WiFi_GPIO_Pin(GPIO_TypeDef* port, uint16_t pin)
		: GPIO_Port(port), Pin(pin)
	{
	}
};

class WiFiServer;
class WiFiClient;

class WiFiDevice
{
	friend class WiFiServer;
	friend class WiFiClient;

public:
	WiFiDevice() {};
	virtual ~WiFiDevice() {};

	enum wifi_debug_lvl {
		LVL_NONE = 0,
		LVL_LOW = 1,
#ifdef DEBUG
		LVL_HIGH = 2,
		LVL_ALL = 3
#endif
	} WIFI_DEBUG_LVL =
#ifdef DEBUG
		LVL_HIGH;
#else
		LVL_LOW;
#endif

	virtual bool Begin(STM32TCPSocket * pSocket) = 0;

	///////////////////////
	// Basic AT Commands //
	///////////////////////
	virtual bool Test() = 0;
	virtual bool Reset() = 0;
	virtual bool Enable() = 0;
	virtual bool Disable() = 0;
#ifdef DEBUG
	virtual int16_t GetVersion(char * ATversion, char * SDKversion, char * compileTime) = 0;
	virtual bool Echo(bool enable) = 0;
	virtual bool SetBaud(unsigned long baud) = 0;
#endif

	////////////////////
	// WiFi Functions //
	////////////////////
#ifdef DEBUG
	virtual int16_t WiFiGetMode() = 0;
#endif
	virtual int16_t WiFiSetMode(wifi_mode mode) = 0;
	virtual int16_t WiFiConnect(const char * ssid, const char * pwd) = 0;
#ifdef DEBUG
	virtual int16_t WiFiGetAP(char * ssid) = 0;
#endif
	virtual int16_t WiFiLocalMAC(char * mac) = 0;
	virtual int16_t WiFiDisconnect() = 0;
	virtual IPAddress WiFiLocalIP() = 0;

	/////////////////////
	// TCP/IP Commands //
	/////////////////////
	virtual int16_t TCPStatus() = 0;
	virtual int16_t TCPUpdateStatus() = 0;
	virtual int16_t TCPConnect(uint8_t linkID, const char * destination, uint16_t port, uint16_t keepAlive) = 0;	// Client connection
	virtual int16_t TCPSend(uint8_t linkID, WiFiBuffer Data) = 0;		// Himanshu
	virtual int16_t TCPSend(uint8_t linkID, const uint8_t *buf, size_t size) = 0;
	virtual int16_t TCPClose(uint8_t linkID) = 0;
	virtual int16_t TCPSetTransferMode(uint8_t mode) = 0;
	virtual int16_t TCPSetMux(uint8_t mux) = 0;
	virtual int16_t TCPConfigureServer(uint16_t port, uint8_t create = 1) = 0;
#ifdef DEBUG
	virtual int16_t TCPPing(IPAddress ip) = 0;
	virtual int16_t TCPPing(char * server) = 0;
#endif
	virtual bool TCPIsConnected(uint8_t linkID) = 0;

    ///////////////////////////////////
	// Virtual Functions from Stream //
	///////////////////////////////////
	virtual size_t Write(const char * buf, size_t len = 0) = 0;
	virtual size_t Write(WiFiBuffer Data) = 0;
	virtual int Read(unsigned int timeoutInMS = 1000, size_t readLen = WIFI_RX_BUFFER_LEN, bool asynchronous = false) = 0;
	virtual void Flush() = 0;

	int16_t m_State[WIFI_MAX_SOCK_NUM];
protected:
    STM32TCPSocket* m_Serial;
    wifi_status m_Status;

    //////////////////////////
	// Command Send/Receive //
	//////////////////////////
	virtual void sendCommand(const char * cmd, enum wifi_command_type type = WIFI_CMD_EXECUTE, WiFiBuffer params = {}) = 0;		// const char * params = NULL
	virtual int16_t readForResponse(const char * rsp, unsigned int timeoutInMS, size_t readLen = WIFI_RX_BUFFER_LEN) = 0;
	virtual int16_t readForResponses(const char * pass, const char * fail, unsigned int timeout, size_t readLen = WIFI_RX_BUFFER_LEN) = 0;

	//////////////////
	// Buffer Stuff //
	//////////////////
	virtual void clearBuffer() = 0;

	/// searchBuffer([test]) - Search buffer for string [test]
	/// Success: Returns pointer to beginning of string
	/// Fail: returns NULL
	virtual char * searchBuffer(const char * test) = 0;
};

class WiFiServer
{
public:
	WiFiServer(WiFiDevice * device, uint16_t port)
		: m_Device(device)
		, m_Port(port)
	{
	}

	virtual WiFiClient * Available(uint8_t wait = 0) = 0;
	virtual void Begin() = 0;
	virtual size_t Write(const uint8_t *buf, size_t size) = 0;
	virtual uint8_t Status() = 0;

	size_t s;
protected:
	WiFiDevice * m_Device;
	uint16_t m_Port;
};

class WiFiClient
{
public:
	WiFiClient() {};
	WiFiClient(WiFiDevice * device, uint16_t port)
		: m_Device(device)
		, m_Port(port)
	{
	}

	virtual uint8_t status() = 0;

	virtual int connect(IPAddress ip, uint16_t port, uint32_t keepAlive) = 0;
	virtual int connect(std::string host, uint16_t port, uint32_t keepAlive = 0) = 0;
	virtual int connect(const char *host, uint16_t port, uint32_t keepAlive) = 0;

	virtual size_t write(const uint8_t *buf, size_t size) = 0;
	virtual int read() = 0;
	virtual int read(uint8_t *buf, size_t size) = 0;
	virtual void flush() = 0;
	virtual void stop() = 0;
	virtual uint8_t connected() = 0;
	virtual operator bool() = 0;

	friend class WiFiServer;

protected:
	WiFiDevice * m_Device;
	uint16_t m_Port;
	uint16_t  m_Socket;
	bool m_IPMuxEn;

	virtual uint8_t getFirstSocket() = 0;
};
