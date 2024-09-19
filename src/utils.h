#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>
#include <string>
#include <vector>

#define ERR_LEN 128
#define ERR_LEN_2 256
#define RDBUFLEN 512

typedef std::vector<std::string> StrVec;

int stringtoint(const std::string& strnum);
double stringtodouble(const std::string& strnum);
StrVec tokenizesting(const std::string& instring, const std::string& delim);
void formatErrorString(int errnum, const char* mesg, std::string& strerrmesg);

#endif
