#pragma once

#include "vector"
#include "cstdint"
#include "cstdio"
#include "string"

class WiFiBuffer
{
public:
    WiFiBuffer();
	WiFiBuffer(size_t Size);
	WiFiBuffer(const std::initializer_list<uint8_t>& Value);
	WiFiBuffer(const WiFiBuffer& Value);
	WiFiBuffer(const std::vector<uint8_t>& Value);
	WiFiBuffer(const void * pBuffer, size_t Size);
	~WiFiBuffer();

	size_t Size() const;
	size_t GetReadPosition() const;
	bool SetReadPosition(size_t value);
	bool IsAtEnd() const;
	bool Skip(size_t Count);
	bool Zero(size_t Position = 0, size_t Count = 0);
	void RemoveReadBytes();

	size_t AppendFloat(float Value);
	size_t AppendDouble(double Value);
	size_t AppendBuffer(const void * pValue, size_t Count);
	ssize_t Append(const WiFiBuffer& Value, size_t Position = 0, size_t Count = 0);
	ssize_t Append(WiFiBuffer * pValue, size_t Count = 0);
	size_t Append(const std::string& Value);
	size_t Append(const std::vector<uint8_t>& Value);
	size_t AppendExtra(size_t Count);
	void Clear();

	bool Get(std::string * pValue, size_t Count, bool Append = false);
	bool GetBuffer(uint8_t * pValue, size_t Count);
	const uint8_t * GetData() const;

	int PeekByte(size_t OffsetFromGetPosition = 0) const;
	int PeekByteAtEnd(size_t OffsetFromEndOfVector = 0) const;
	bool PeekBuffer(uint8_t * pValue, size_t Count) const;

	uint8_t& operator[](size_t Index);
	const uint8_t& operator[](size_t Index) const;

private:
	using RawData = std::vector<uint8_t>;
	RawData                 m_Data;
	size_t                  m_ReadPosition = 0;
};
