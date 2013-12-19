#ifndef _BASE64_H_
#define _BASE64_H_

#include <vector>
#include <string>
typedef unsigned char BYTE;

std::string b64encode(const std::vector<BYTE> & buff);
std::string b64encode(const std::string buff);
std::vector<BYTE> b64decode(const std::string & encoded_string);

#endif