#pragma once
#include <vector>
#include <string>
#include <stdint.h>
typedef uint8_t BYTE;

std::string b64encode(const std::vector<BYTE> & buff);
std::string b64encode(const std::string & buff);
std::vector<BYTE> b64encode_v(const std::vector<BYTE> & buff);

std::vector<BYTE> b64decode(const std::string & encoded_string);
std::vector<BYTE> b64decode(const std::vector<BYTE> & encoded_string);
std::string b64decode_s(const std::string & encoded_string);

