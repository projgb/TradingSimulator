#include <unistd.h>
#include "CommLink.h"
#include "OrderTracker.h"
#include "utils.h"

using namespace std;

CommLink::CommLink(std::string& recvlink, std::string& sendlink)
					:m_ReadOk(false)
					,m_WriteOk(false)
					,m_readfd(0)
					,m_writefd(0)
					,m_pcbh(NULL)
					,m_recvlink(recvlink)
					,m_sendlink(sendlink)
{
}

void CommLink::init(ICallbackHandler* cbh)
{
	m_pcbh = cbh;
	// open fifo in nonblocking mode allowing other end to open
	openLink(m_recvlink.c_str(), O_RDONLY, m_readfd, m_ReadOk);
	openLink(m_sendlink.c_str(), O_WRONLY, m_writefd, m_WriteOk);
}

CommLink::~CommLink()
{
	close(m_readfd);
	close(m_writefd);
	m_readfd = 0;
	m_writefd = 0;
	m_ReadOk = false;
	m_WriteOk = false;
	m_pcbh = NULL; // dont delete this pointer here
}

int CommLink::readdata(char* buf, size_t buflen)
{
}

int CommLink::senddata(const char* buf, size_t buflen)
{
	//cout<<"m_writefd="<<m_writefd<<", m_WriteOk="<<m_WriteOk<<"\n";
	openLink(m_sendlink.c_str(), O_WRONLY, m_writefd, m_WriteOk);
	if (m_writefd > 0)
	{
		errno = 0;
		ssize_t nwritten = write(m_writefd, buf, buflen);
		//cout<<"your buf="<<buf<<", len="<<buflen<<", nwritten="<<nwritten<<", errno="<<errno<<", strerr:"<<strerror(errno)<<"\n";
		if (nwritten == -1)
		{
			std::string strerrmesg;
			formatErrorString(errno, "X-write:", strerrmesg);
			cout<<"write failed:"<<strerrmesg<<"\n";
		}
		return nwritten;
	}
	return -1;
}

int CommLink::WaitForEvent(int timeout)
{
	int nfds = m_readfd + 1;
	struct timeval tv = {timeout, 0}; // default timeout of 5 seconds
	errno = 0;
	bool bRun = true;
	while(bRun == true)
	{
		openLink(m_recvlink.c_str(), O_RDONLY, m_readfd, m_ReadOk);
		openLink(m_sendlink.c_str(), O_WRONLY, m_writefd, m_WriteOk);
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_readfd, &readfds);
		int ret = select(nfds, &readfds, NULL, NULL, &tv);
		if (ret == 0)  // timeout, no events
		{
			//printf("WaitForEvent:timeout:\n");
			bRun = false;
			break;
		}
		else if (ret == -1)  // error occured 
		{
			string strerrmesg;
			formatErrorString(errno, "X-WaitForEvent:", strerrmesg);
			cout << strerrmesg;
			bRun = false;
			break;
		}
		else // got read fds actionable
		{
			if(FD_ISSET(m_readfd, &readfds))
			{
				char buffer[RDBUFLEN] = {'\0'};
				ssize_t nread = read(m_readfd, buffer, RDBUFLEN);
				if (nread == 0)
				{
					close(m_readfd);
					m_ReadOk = false;
					bRun = false;
					break;
				}
				//cout<<"readfd ready to read, bytesread="<<nread<<", ["<<buffer<<"]\n";
				string strmesg = buffer;
				(*m_pcbh)(strmesg);
				bRun = false;
				break;
			}
		}
	}
	return 0;
}

void CommLink::CloseLink()
{
	if (m_readfd > 0)
	{
		close(m_readfd);
		m_readfd = -1;
	}
	if (m_writefd > 0)
	{
		close(m_writefd);
		m_writefd = -1;
	}
	m_ReadOk = false;
	m_WriteOk = false;
}

void CommLink::openLink(const char* linkname, mode_t mode, int& fd, bool& flag)
{
	if (flag == false)
	{
		fd = open(linkname, O_NONBLOCK | mode);
		//printf("open returned %d for %s\n", fd, linkname);
		if(fd > -1)
		{
			flag = true;
		}
	}
}

