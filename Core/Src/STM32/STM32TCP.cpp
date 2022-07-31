#include <chrono>
#include <iostream>
#include <iomanip>
#include <../CMSIS_RTOS/cmsis_os.h>
#include <cstring>
#include <timers.h>
#include <climits>

#include "STM32Debug.h"
#include "CircularBuffer.h"

// Himanshu
#include "STM32TCP.h"

extern "C" EPRI::STM32SerialSocket** pg_pSocket;

static WiFiBuffer TxVector;
static char MAC[18]{0};
static IPAddress IP;

namespace EPRI
{
	//
	// STM32TCP
	//
	STM32TCP::STM32TCP()
	{
	}

	STM32TCP::~STM32TCP()
	{
	}

	//
	// STM32TCPSocket
	//
	STM32TCPSocket::STM32TCPSocket(const STM32Serial::Options& SerialOpt, const STM32TCP::Options& IPOpt, WiFiDevice* wifi)
		: STM32SerialSocket(SerialOpt)
		, m_WiFi(wifi)
		, m_IPOptions(IPOpt)
	{
		this->SetPortOptions();
		if (this->STM32SerialSocket::Open("") != SUCCESSFUL)		// m_Socket should not get called in STM32SerialSocket::Open. Hence, passed "" instead of nullptr.
		{
			printf("ERROR : Failed to open Serial Socket.\r\n");
			throw std::runtime_error("error");
		}
		else
		{
			*pg_pSocket = this;
RETRY_BEGIN:
			if(m_WiFi->Begin(this))
			{
				printf("Init\r\n");
				if(m_WiFi->WiFiLocalMAC(MAC) > 0)
					printf("MAC Address : %s\r\n", MAC);
			}
			else
			{
				printf("ERROR : Failed to communicate to WiFi Device.\r\n");
				osDelay(1000);
				goto RETRY_BEGIN;
				throw std::runtime_error("error");
			}
		}
	}

	STM32TCPSocket::~STM32TCPSocket()
	{
		delete m_WiFi;
	}

	ERROR_TYPE STM32TCPSocket::Open(const char * DestinationAddress /*= nullptr*/, int Port /*= DEFAULT_WiFi_PORT*/,
			const char* AccessPoint /*= DEFAULT_ACCESSPOINT*/, const char* PassPhrase /*= DEFAULT_PASSPHRASE*/)
	{
RETRY_CONNECT:
		if (m_WiFi->Test() and
			m_WiFi->WiFiConnect(AccessPoint, PassPhrase) > 0)
		{
			printf("Connected to %s.\r\n", AccessPoint);
			if(std::string(IP = m_WiFi->WiFiLocalIP()) != "")
				printf("IP Address : %s\r\n", std::string(IP).c_str());
			if (m_IPOptions.m_Mode == STM32TCP::Options::MODE_SERVER)
			{
				if (m_WiFi->TCPSetMux(0) > 0 and
					m_WiFi->TCPSetTransferMode(m_IPOptions.m_Mode) > 0 and
					m_WiFi->TCPSetMux(1) > 0 and
					m_WiFi->TCPConfigureServer(Port, 1) > 0)			// TODO - keepAlive
				{
					printf("Server started on %s:%d\r\n", std::string(IP).c_str(), Port);

					if (m_Connect)
					{
						m_Connect(SUCCESSFUL);
					}

					m_Connected = true;
					return SUCCESSFUL;
				}
			}
			else if (m_IPOptions.m_Mode == STM32TCP::Options::MODE_CLIENT)
			{
				//! TODO
			}

			printf("Failed to create server.\r\n");
		}
		else
		{
			printf("Failed to connect to %s.\r\nRetrying after 5s...\r\n", AccessPoint);
			osDelay(5000);
			goto RETRY_CONNECT;
		}
		return not SUCCESSFUL;
	}

//	STM32TCPSocket::ConnectCallbackFunction STM32TCPSocket::RegisterConnectHandler(ConnectCallbackFunction Callback)
//	{
//		ConnectCallbackFunction RetVal = m_Connect;
//		m_Connect = Callback;
//		return RetVal;
//	}

	STM32Serial::Options STM32TCPSocket::GetSerialOptions()
	{
		return m_Options;
	}

	STM32TCP::Options STM32TCPSocket::GetIPOptions()
	{
		return m_IPOptions;
	}

	ERROR_TYPE STM32TCPSocket::Write(const char * Data, size_t Count /*= 0*/, bool Asynchronous /*= false*/)	// raw-write {mainly} for sending AT commands.
	{
		if(!Count)
		{
			if(strlen(Data) == 0)
				return SUCCESSFUL;
			Count = strlen(Data);
		}
		TxVector.Clear();
		TxVector.AppendBuffer(Data, Count);
		return this->STM32SerialSocket::Write(TxVector, Asynchronous);
	}

	ERROR_TYPE STM32TCPSocket::Write(const WiFiBuffer& Data, bool Asynchronous /*= false*/)						// {planned} for sending data over TCP.
	{
		ERROR_TYPE RetVal = SUCCESSFUL;
		m_WiFi->TCPSend(m_SocketID, Data);

		if(m_Write)
			m_Write(RetVal, Data.Size());
		return RetVal;
	}

//	STM32TCPSocket::WriteCallbackFunction STM32TCPSocket::RegisterWriteHandler(WriteCallbackFunction Callback)
//	{
//		WriteCallbackFunction RetVal = m_Write;
//		m_Write = Callback;
//		return RetVal;
//	}

	/*
	 * @param pData : (WiFiBuffer *)
	 * @param ReadAtLeast : (size_t) The assumed number of bytes that must've been read after the function call finishes or the read handler interrupt completes, unless a timeout occurs. Keep below a max value specified by WiFi device library or 0xffff (UINT16_MAX) otherwise.
	 */
//	ERROR_TYPE STM32TCPSocket::Read(WiFiBuffer * pData,
//		size_t ReadAtLeast /*= 0*/,
//		uint32_t TimeOutPeriodInMS /*= 0*/,
//		size_t * pActualBytes /*= nullptr*/)
//	{
////		if(ReadAtLeast <= (size_t)std::min(UINT16_MAX, WIFI_MAX_TCP_LEN));
//			return this->STM32SerialSocket::Read(pData, ReadAtLeast, TimeOutPeriodInMS, pActualBytes);
//	}

//	bool STM32TCPSocket::AppendAsyncReadResult(WiFiBuffer * pData, size_t ReadAtLeast /*= 0*/)
//	{
//		if(not this->STM32SerialSocket::AppendAsyncReadResult(pData, ReadAtLeast))
//			return false;
//		// TODO - process received pData result
//		return true;
//	}

//	STM32TCPSocket::ReadCallbackFunction STM32TCPSocket::RegisterReadHandler(ReadCallbackFunction Callback)
//	{
//		ReadCallbackFunction RetVal = m_Read;
//		m_Read = Callback;
//		return RetVal;
//	}

	ERROR_TYPE STM32TCPSocket::Close()
	{
		m_Connected = false;
		if(m_WiFi->TCPClose(m_SocketID))
		{
			return SUCCESSFUL;
		}
		return not SUCCESSFUL;
	}

//	STM32TCPSocket::CloseCallbackFunction STM32TCPSocket::RegisterCloseHandler(CloseCallbackFunction Callback)
//	{
//		CloseCallbackFunction RetVal = m_Close;
//		m_Close = Callback;
//		return RetVal;
//	}

	bool STM32TCPSocket::IsConnected()
	{
		return m_Connected;
	}

	void STM32TCPSocket::Connected()	// Himanshu
	{
		m_Connected = true;
	}

	void STM32TCPSocket::Disconnect()	// Himanshu
	{
		m_Connected = false;
	}

	ERROR_TYPE STM32TCPSocket::Accept(const char * DestinationAddress /*= nullptr*/, int Port /*= DEFAULT_WiFi_PORT*/)	// Himanshu
	{
		// TODO - accept TCP client connection - AT command
		return SUCCESSFUL;
	}

	ERROR_TYPE STM32TCPSocket::Flush(FlushDirection Direction)
	{
		WiFiBuffer vec;
		this->AppendAsyncReadResult(&vec, 0);
		this->Read(&vec, WIFI_RX_BUFFER_LEN, 1000);
		Base()->GetDebug()->TRACE_VECTOR("FLUSH", vec);
		this->STM32SerialSocket::Flush(Direction);
		return SUCCESSFUL;
	}

	void STM32TCPSocket::RegisterDevice(WiFiDevice * wifi)		// Himanshu
	{
		m_WiFi = wifi;
	}

	ERROR_TYPE STM32TCPSocket::SetOptions(const STM32Serial::Options& SerialOpt, const STM32TCP::Options& IPOpt)
	{
		m_Options = SerialOpt;
		m_IPOptions = IPOpt;
		return SUCCESSFUL;
	}

	void STM32TCPSocket::SetPortOptions()
	{
		this->STM32SerialSocket::SetPortOptions();
		// TODO - implement for TCP Options.
	}

}

