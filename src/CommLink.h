#ifndef _COMM_LINK_H_
#define _COMM_LINK_H_

#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "ICallbackHandler.h"

class CommLink
{
public:
	CommLink(std::string& recvlink, std::string& sendlink);
	~CommLink();
	void init(ICallbackHandler* cbh);
	int WaitForEvent(int timeout = 5); // 5second default timeout
	int readdata(char* buf, size_t buflen);
	int senddata(const char* buf, size_t buflen);
	void CloseLink();
private:
	CommLink();
	CommLink(const CommLink&);
	void operator=(const CommLink&);
	void openLink(const char* linkname, mode_t mode, int& fd, bool& flag);
	bool m_ReadOk;
	bool m_WriteOk;
	int m_readfd;
	int m_writefd;
	ICallbackHandler* m_pcbh;
	std::string m_recvlink;
	std::string m_sendlink;
};

#endif


