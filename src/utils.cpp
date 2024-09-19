#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <string>
#include <string.h>
#include "utils.h"

using namespace std;

int stringtoint(const string& strnum)
{
	string::size_type sz;
	return stoi(strnum, &sz);
}

double stringtodouble(const string& strnum)
{
	string::size_type sz;
	return stod(strnum, &sz);
}

static inline bool check(size_t pos)
{
    return pos != string::npos;
}

StrVec tokenizesting(const string& instring, const string& delim)
{
    vector<string> tokens;
    size_t start_pos;
    size_t end_pos = 0;
    while (check(start_pos = instring.find_first_not_of(delim, end_pos)) )
    {
        end_pos = instring.find_first_of(delim, start_pos);
        tokens.emplace_back(instring.substr(start_pos, end_pos - start_pos));
    }
    return tokens;
}

void formatErrorString(int errnum, const char* mesg, string& strerrmesg)
{
	char buf[ERR_LEN_2] = {'\0'};
	sprintf(buf, "%s:Err=%s", mesg, strerror(errnum));
	strerrmesg.assign(buf);
}


