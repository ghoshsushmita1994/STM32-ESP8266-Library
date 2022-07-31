#pragma once

//#include "string.h"
#include "string"
#include "cstring"
# include "stdexcept"

class IPAddress{
	uint8_t addr[4];
	int16_t error = 0;
public:
	IPAddress() {};

	IPAddress(const char * ip)
	{
		const char * p = ip;
		for (uint8_t i = 0; i < 4; i++)
		{
			char tempOctet[4];
			memset(tempOctet, 0, 4); // Clear tempOctet

			size_t octetLength = strspn(p, "0123456789"); // Find length of numerical string:
			if (octetLength >= 4) // If it's too big, return an error
				throw std::out_of_range("Invalid IP Address.");

			strncpy(tempOctet, p, octetLength); // Copy string to temp char array:
			addr[i] = atoi(tempOctet); // Move the temp char into IP Address octet

			p += (octetLength + 1); // Increment p to next octet
		}
	}

	IPAddress(int16_t err)
	{
		error = err;
	}

	uint8_t& operator[](int idx)
	{
		return addr[idx];
	}

	operator std::string()
	{
		if(error)
			return "";
		std::string str = "";
		for(uint8_t i=0; i<3; i++)
			str += std::to_string(addr[i]) + ".";
		str += std::to_string(addr[3]);
		return str;
	}
};
