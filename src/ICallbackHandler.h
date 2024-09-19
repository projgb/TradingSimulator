#ifndef _ICALLBACK_HANDLER_H_
#define _ICALLBACK_HANDLER_H_

#include <iostream>
#include <string>

class ICallbackHandler
{
public:
	ICallbackHandler(){}
	virtual int operator ()(const std::string& strmesg) = 0;
	virtual ~ICallbackHandler(){}
private:
	ICallbackHandler(const ICallbackHandler&);
	void operator=(const ICallbackHandler&);
};

#endif

