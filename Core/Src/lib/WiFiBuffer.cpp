
#include "cstring"
#include "WiFiBuffer.h"

WiFiBuffer::WiFiBuffer()
{
}

WiFiBuffer::WiFiBuffer(size_t Size)
{
	m_Data.resize(Size);
}

WiFiBuffer::WiFiBuffer(const std::initializer_list<uint8_t>& Value)
{
	m_Data = Value;
}

WiFiBuffer::WiFiBuffer(const WiFiBuffer& Value)
{
	m_Data = Value.m_Data;
}

WiFiBuffer::WiFiBuffer(const std::vector<uint8_t>& Value)
{
	m_Data = Value;
}

WiFiBuffer::WiFiBuffer(const void * pBuffer, size_t Size)
{
	const uint8_t * p = static_cast<const uint8_t *>(pBuffer);
	m_Data.insert(m_Data.end(), p, p + Size);
}

WiFiBuffer::~WiFiBuffer()
{
}

size_t WiFiBuffer::Size() const
{
	return m_Data.size();
}

size_t WiFiBuffer::GetReadPosition() const
{
	return m_ReadPosition;
}

bool WiFiBuffer::SetReadPosition(size_t Value)
{
	if (Value >= m_Data.size())
		return false;
	m_ReadPosition = Value;
	return true;
}

bool WiFiBuffer::Skip(size_t Count)
{
	return SetReadPosition(m_ReadPosition + Count);
}

void WiFiBuffer::RemoveReadBytes()
{
	if (m_ReadPosition < m_Data.size())
	{
		std::vector<uint8_t>(m_Data.begin() + m_ReadPosition, m_Data.end()).swap(m_Data);
		m_ReadPosition = 0;
	}
}

bool WiFiBuffer::IsAtEnd() const
{
	return m_ReadPosition >= m_Data.size();
}

bool WiFiBuffer::Zero(size_t Position /* = 0 */, size_t Count /* = 0 */)
{
	if (0 == Count)
	{
		Count = m_Data.size() - Position;
	}
	if (Position + Count > m_Data.size())
	{
		return false;
	}
	std::memset(m_Data.data() + Position, '\0', Count);
	return true;
}

size_t WiFiBuffer::AppendFloat(float Value)
{
	size_t RetVal = m_Data.size();
	union
	{
		float   Float;
		uint8_t Bytes[sizeof(float)];
	} Convert;
	Convert.Float = Value;
	m_Data.push_back(Convert.Bytes[3]);
	m_Data.push_back(Convert.Bytes[2]);
	m_Data.push_back(Convert.Bytes[1]);
	m_Data.push_back(Convert.Bytes[0]);
	return RetVal;
}

size_t WiFiBuffer::AppendDouble(double Value)
{
	size_t RetVal = m_Data.size();
	union
	{
		double  Double;
		uint8_t Bytes[sizeof(double)];
	} Convert;
	Convert.Double = Value;
	m_Data.push_back(Convert.Bytes[7]);
	m_Data.push_back(Convert.Bytes[6]);
	m_Data.push_back(Convert.Bytes[5]);
	m_Data.push_back(Convert.Bytes[4]);
	m_Data.push_back(Convert.Bytes[3]);
	m_Data.push_back(Convert.Bytes[2]);
	m_Data.push_back(Convert.Bytes[1]);
	m_Data.push_back(Convert.Bytes[0]);
	return RetVal;
}

size_t WiFiBuffer::AppendBuffer(const void * pValue, size_t Count)
{
	size_t RetVal = m_Data.size();
	const uint8_t * p = static_cast<const uint8_t *>(pValue);
	m_Data.insert(m_Data.end(), p, p + Count);
	return RetVal;
}

ssize_t WiFiBuffer::Append(const WiFiBuffer& Value, size_t Position /* = 0 */, size_t Count /* = 0 */)
{
	ssize_t RetVal = m_Data.size();
	if (0 == Count)
	{
		Count = Value.Size() - Position;
	}
	if (Position + Count > Value.Size())
		return -1;
	AppendBuffer(Value.m_Data.data() + Position, Count);
	return RetVal;
}

ssize_t WiFiBuffer::Append(WiFiBuffer * pValue, size_t Count /* = 0 */)
{
	ssize_t RetVal = m_Data.size();
	if (0 == Count)
	{
		Count = pValue->Size() - pValue->GetReadPosition();
	}
	if (pValue->GetReadPosition() + Count > pValue->Size())
		return -1;
	size_t Position = AppendExtra(Count);
	if (!pValue->GetBuffer(&m_Data[Position], Count))
	{
		RetVal = -1;
	}
	return RetVal;
}

size_t WiFiBuffer::Append(const std::string& Value)
{
	size_t RetVal = m_Data.size();
	m_Data.insert(m_Data.end(), Value.begin(), Value.end());
	return RetVal;
}

size_t WiFiBuffer::Append(const std::vector<uint8_t>& Value)
{
	size_t RetVal = m_Data.size();
	m_Data.insert(m_Data.end(), Value.begin(), Value.end());
	return RetVal;
}

size_t WiFiBuffer::AppendExtra(size_t Count)
{
	size_t RetVal = m_Data.size();
	m_Data.resize(RetVal + Count);
	return RetVal;
}

void WiFiBuffer::Clear()
{
    m_Data.clear();
    m_ReadPosition = 0;
}

bool WiFiBuffer::Get(std::string * pValue, size_t Count, bool Append /*= false*/)
{
	if (m_ReadPosition + Count <= m_Data.size())
	{
		if (!Append)
		{
			pValue->clear();
		}
		std::string::iterator it = Append ? pValue->end() : pValue->begin();
		pValue->insert(it,
			m_Data.begin() + m_ReadPosition,
			m_Data.begin() + m_ReadPosition + Count);
		m_ReadPosition += Count;
		return true;
	}
	return false;
}

bool WiFiBuffer::GetBuffer(uint8_t * pValue, size_t Count)
{
	if (m_ReadPosition + Count <= m_Data.size())
	{
		std::memcpy(pValue, &m_Data.data()[m_ReadPosition], Count);
		m_ReadPosition += Count;
		return true;
	}
	return false;
}

const uint8_t * WiFiBuffer::GetData() const
{
	return m_Data.data();
}

int WiFiBuffer::PeekByte(size_t OffsetFromGetPosition /* = 0 */) const
{
    if (m_ReadPosition + OffsetFromGetPosition < m_Data.size())
    {
        return m_Data[m_ReadPosition + OffsetFromGetPosition];
    }
    return -1;
}

int WiFiBuffer::PeekByteAtEnd(size_t OffsetFromEndOfVector /* = 0*/) const
{
    ssize_t Index = (ssize_t) m_Data.size() - (ssize_t) OffsetFromEndOfVector - 1;
    if (Index >= 0)
    {
        return m_Data[Index];
    }
    return -1;
}

bool WiFiBuffer::PeekBuffer(uint8_t * pValue, size_t Count) const
{
    if (m_ReadPosition + Count <= m_Data.size())
    {
        std::memcpy(pValue, &m_Data[m_ReadPosition], Count);
        return true;
    }
    return false;
}

uint8_t& WiFiBuffer::operator[](size_t Index)
{
	return m_Data[Index];
}

const uint8_t& WiFiBuffer::operator[](size_t Index) const
{
	return m_Data[Index];
}
