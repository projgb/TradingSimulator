#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "OrderTracker.h"
#include "utils.h"
#include "ICallbackHandler.h"

using namespace std;

struct ThreadArg
{
	OrderTracker* m_ThisPtr;
};

// call back handler classes for handling client's and exch's inputs
template<class T>
class ClientInputListener : public ICallbackHandler
{
public:
	ClientInputListener(T* ptr):ICallbackHandler(),m_pTarget(ptr){}
	~ClientInputListener(){}
	int operator()(const std::string& strmesg)
	{
		auto strvec = tokenizesting(strmesg, ",");
		//cout<<"Num tokens "<< strvec.size()<<", tokens\n"; // N,1,B,12,3
		char cOrdType = strvec[0][0];
		switch(cOrdType)
		{
		case 'N':
			{
				int ordid = stringtoint(strvec[1]);
				char side = strvec[2][0];
				int qty = stringtoint(strvec[3]);
				double price = stringtodouble(strvec[4]);
				m_pTarget->OnInsertOrderRequest(ordid, side, price, qty);
			}
			break;
		case 'R':
			{
				std::string::size_type sz;
				int oldId = stringtoint(strvec[1]);
				int newId = stringtoint(strvec[2]);
				int deltaQuantity = stringtoint(strvec[3]);
				m_pTarget->OnReplaceOrderRequest(oldId, newId, deltaQuantity);
			}
			break;
		default:
			break;
		}
	}
private:
	ClientInputListener();
	ClientInputListener(const ClientInputListener&);
	void operator=(const ClientInputListener&);
	T* m_pTarget;
};

template<class T>
class ExchInputListener: public ICallbackHandler
{
public:
	ExchInputListener(T* ptr):ICallbackHandler(),m_pTarget(ptr){}
	~ExchInputListener(){}
	void processMesg(const std::string& strmesg)
	{
		auto strvec = tokenizesting(strmesg, ",");
		int numtokens = strvec.size();
		//cout<<"Num tokens "<< numtokens<<", tokens:"<<strmesg<<"\n"; // A,Id / R,Id / F,Id,Qtyfilled
		char cOrdType = strvec[0][0];
		switch(cOrdType)
		{
		case 'A': // ack'd
			{
				int id = stringtoint(strvec[1]);
				m_pTarget->OnRequestAcknowledged(id);
			}
			break;
		case 'F': // partilly or fully filled
			{
				int id = stringtoint(strvec[1]);
				int qty = stringtoint(strvec[2]);
				m_pTarget->OnOrderFilled(id, qty);
			}
			break;
		case 'R': // order rejected
			{
				int id = stringtoint(strvec[1]);
				m_pTarget->OnRequestRejected(id);
			}
			break;
		default:
			break;
		}
	}
	int operator ()(const std::string& strmesg)
	{
		auto vecmesgs = tokenizesting(strmesg, "|");
		int nummesgs = vecmesgs.size();
		//cout<<"ExchInputListener recd mesg="<<strmesg<<", num tokens="<<nummesgs<<"\n";
		if (nummesgs > 0)
		{
			for (int ctr = 0; ctr < nummesgs; ctr++)
			{
				processMesg(vecmesgs[ctr]);
			}
		}
		else
		{
			processMesg(strmesg);
		}
	}
private:
	ExchInputListener();
	ExchInputListener(const ExchInputListener&);
	void operator=(const ExchInputListener&);
	T* m_pTarget;
};


OrderTracker::OrderTracker()
			:m_OrdRequestLink("/tmp/CLIENT_to_OH")
			,m_OrdReplyLink("/tmp/OH_to_CLIENT")
			,m_ExchRequstLink("/tmp/OH_to_EXCH")
			,m_ExchReplyLink("/tmp/EXCH_to_OH")
			,m_clientlink(m_OrdRequestLink, m_OrdReplyLink)
			,m_exchlink(m_ExchReplyLink, m_ExchRequstLink)
			,m_OrdersDb()
			,m_PosBook()
{
}

OrderTracker::~OrderTracker()
{
}

int OrderTracker::HandleOrders()
{
	ClientInputListener<Listener> clientListener(this);
	ExchInputListener<Listener> exchListener(this);
	m_clientlink.init(&clientListener);
	m_exchlink.init(&exchListener);
	bool bRun = true;
	while(bRun == true)
	{
		m_clientlink.WaitForEvent();
		m_exchlink.WaitForEvent();
	}
	int myret = 5;
	return myret;
}

void* OrderTracker::RunThread(void *arg)
{
	ThreadArg *ptrThrArg = (ThreadArg*)arg;
	OrderTracker& ordTracker = *ptrThrArg->m_ThisPtr;
	int ret = ordTracker.HandleOrders();
	pthread_exit((void*)&ret);
}

int OrderTracker::TrackOrders()
{
	try
	{
		// set communications links
		initLink(m_OrdRequestLink.c_str());
		initLink(m_OrdReplyLink.c_str());
		initLink(m_ExchRequstLink.c_str());
		initLink(m_ExchReplyLink.c_str());

		// start order tracking loop
		pthread_t ordTid;
		pthread_attr_t attr;
		void *status;
		/* Initialize and set thread detached attribute */
		pthread_attr_init(&attr);
		ThreadArg targ;
		targ.m_ThisPtr = this;
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		int ret = pthread_create(&ordTid, &attr, RunThread, (void*)&targ);
		if (ret) // thread create failed report error
		{
			string strerrmesg;
			formatErrorString(ret, "X-TrackOrders:pthr_create:", strerrmesg);
			cout <<"pthread_Create failed\n";
			throw runtime_error(strerrmesg.c_str());
		}
		/* Free attribute and wait for the other threads */
		pthread_attr_destroy(&attr);
		while(1)
		{
			int ret = pthread_join(ordTid, NULL);
			if (ret == 0)
			{
				break;
			}
		}
	}
	catch(exception& ex)
	{
		cout << ex.what();
	}
	catch(...)
	{
		cout<<"X-TrackOrders::TrackOrders:Unknown exception\n";
	}
	return 0;
}

void OrderTracker::initLink(const char* linkname)
{
		assert(linkname);
		struct stat sb;
        if (stat(linkname, &sb) == 0) // fifo exists, delete it & create fresh
		{
			unlink(linkname);
        }
		mode_t mode = LINKMODE;
		errno = 0;
		int ret = mkfifo(linkname, mode);
		if (ret != 0)
		{
			char buf[ERR_LEN] = {'\0'};
			sprintf(buf, "X-initLink:%s:", linkname);
			string strerrmesg;
			formatErrorString(ret, buf, strerrmesg);
			throw runtime_error(strerrmesg.c_str());
		}
		return;
}

//--- client call backs
void OrderTracker::OnInsertOrderRequest(int id,char side, double price,int qty)
{
	OrderInfo neworder(id, qty, side, price);
	m_OrdersDb.emplace(id, neworder);
	string strneword = "N|"+to_string(id)+"|"+side+"|"+to_string(qty)+"|"+to_string(price);
	int ret = m_exchlink.senddata(strneword.c_str(), strneword.length());
	//cout<<"New order to exch=["<<strneword<<"], Wrote to exch: ret="<<ret<<"\n";
	string strpov;
	switch (side)
	{
	case 'B':
			m_PosBook.m_BidMaxPOV += price * qty;
			break;
	case 'O':
			m_PosBook.m_OfferMaxPOV += price * qty;
			break;
	default: // assumption: client checks for valid sides before sendng, so this case wont b hit
			break;
	}
	// respond to client order is queued (pending) + nfq-cov-pov:Q,Id,Nfq,COVb,COVo,POVminb,POVmaxb,POVmino,POVmaxo
	strneword = "Q,"+to_string(id)+","+to_string(m_PosBook.m_NFQ)+","+to_string(m_PosBook.m_BidCOV)+","+to_string(m_PosBook.m_OfferCOV)+",";
	strneword += to_string(m_PosBook.m_BidMaxPOV)+","+to_string(m_PosBook.m_BidMinPOV)+","+to_string(m_PosBook.m_OfferMaxPOV)+","+to_string(m_PosBook.m_OfferMinPOV)+"|";
	ret = m_clientlink.senddata(strneword.c_str(), strneword.length());
	//cout<<"reply neword queued to client=["<<strneword<<"], Wrote to client: ret="<<ret<<"\n";
}

void OrderTracker::OnReplaceOrderRequest(int oldId, int newId, int deltaQuantity)
{
	OrdersDB::iterator itr = m_OrdersDb.find(oldId);
	// assumption: client validates old-id before sending, no checks for if order exists or is not 'Filled/Rejected' or eff qty is not zero
	if (itr != m_OrdersDb.end())
	{
		OrderInfo oldOrder = itr->second;
		m_OrdersDb.erase(itr); // assumption: remove old order from store as no more actions allowed on this
		oldOrder.m_prevId = oldId;
		oldOrder.m_Id = newId;
		oldOrder.m_Qty += deltaQuantity; // store effective qty, send delta to exch
		oldOrder.m_BalQty += deltaQuantity;
		m_OrdersDb.emplace(newId, oldOrder);
		string strreplord = "R|"+to_string(oldId)+"|"+to_string(newId)+"|"+to_string(deltaQuantity);
		int ret = m_exchlink.senddata(strreplord.c_str(), strreplord.length());
		//cout<<"New order to exch=["<<strreplord<<"], Wrote to exch: ret="<<ret<<"\n";
		double repldeltaval = oldOrder.m_Price * deltaQuantity;
		switch (oldOrder.m_Side)
		{
		case 'B':
				{
					double effmaxpov = m_PosBook.m_BidMaxPOV + repldeltaval;
					m_PosBook.m_BidMaxPOV = max(effmaxpov, m_PosBook.m_BidMaxPOV);
				}
				break;
		case 'O':
				{
					double effmaxpov = m_PosBook.m_OfferMaxPOV + repldeltaval;
					m_PosBook.m_BidMaxPOV = max(effmaxpov, m_PosBook.m_OfferMaxPOV);
				}
				break;
		default: // assumption: client checks for valid sides before sendng, so this case wont b hit
				break;
		}
		// respond to client order is queued (pending) + nfq-cov-pov:Q,Id,Nfq,COVb,COVo,POVminb,POVmaxb,POVmino,POVmaxo
		strreplord = "Q,"+to_string(newId)+","+to_string(m_PosBook.m_NFQ)+","+to_string(m_PosBook.m_BidCOV)+","+to_string(m_PosBook.m_OfferCOV)+",";
		strreplord += to_string(m_PosBook.m_BidMaxPOV)+","+to_string(m_PosBook.m_BidMinPOV)+","+to_string(m_PosBook.m_OfferMaxPOV)+","+to_string(m_PosBook.m_OfferMinPOV)+"|";
		ret = m_clientlink.senddata(strreplord.c_str(), strreplord.length());
		//cout<<"reply replaceorder queued to client=["<<strreplord<<"], Wrote to client: ret="<<ret<<"\n";
	}
}

// Exchange callbacks
void OrderTracker::OnRequestAcknowledged(int id)
{
	OrdersDB::iterator itr = m_OrdersDb.find(id);
	if (itr != m_OrdersDb.end())
	{
		itr->second.m_State = 'A';
		double ackordval = itr->second.m_Price * itr->second.m_Qty;
		switch (itr->second.m_Side)
		{
		case 'B':
				m_PosBook.m_BidMinPOV = m_PosBook.m_BidMaxPOV;
				m_PosBook.m_BidCOV += ackordval;
				break;
		case 'O':
				m_PosBook.m_OfferMinPOV = m_PosBook.m_OfferMaxPOV;
				m_PosBook.m_OfferCOV += ackordval;
				break;
		default: // assumption: client checks for valid sides before sendng, so this case wont b hit
				break;
		}
		// respond to client order is acknow + nfq-cov-pov:A,Id,Nfq,COVb,COVo,POVminb,POVmaxb,POVmino,POVmaxo
		string strordack = "A,"+to_string(id)+","+to_string(m_PosBook.m_NFQ)+","+to_string(m_PosBook.m_BidCOV)+","+to_string(m_PosBook.m_OfferCOV)+",";
		strordack += to_string(m_PosBook.m_BidMaxPOV)+","+to_string(m_PosBook.m_BidMinPOV)+","+to_string(m_PosBook.m_OfferMaxPOV)+","+to_string(m_PosBook.m_OfferMinPOV)+"|";
		int ret = m_clientlink.senddata(strordack.c_str(), strordack.length());
		//cout<<"reply ack-from-exch to client=["<<strordack<<"], Wrote to client: ret="<<ret<<"\n";
		return;
	}
	//cout<<"Rec'd ack for non-existent order "<<id<<" from exch!!!, ignore\n";
}

void OrderTracker::OnOrderFilled(int id, int quantityFilled)
{
	OrdersDB::iterator itr = m_OrdersDb.find(id);
	if (itr != m_OrdersDb.end())
	{
		int ordqty = itr->second.m_Qty;
		int balqty = itr->second.m_BalQty;
		balqty -= quantityFilled;
		string strordfill = "F,";
		char ordstate = 'F';
		if (quantityFilled < ordqty && balqty > 0) // partial fill
		{
			ordstate = 'P';
			strordfill = "P,";
		}
		itr->second.m_BalQty = balqty;
		itr->second.m_State = ordstate;
		char side = itr->second.m_Side;
		string strpov;
		double fillordval = itr->second.m_Price * quantityFilled;
		switch (side)
		{
		case 'B':
				m_PosBook.m_NFQ += quantityFilled;
				m_PosBook.m_BidMinPOV -= fillordval;
				m_PosBook.m_BidMaxPOV -= fillordval;
				strpov = to_string(m_PosBook.m_BidCOV)+","+to_string(m_PosBook.m_BidMaxPOV)+","+to_string(m_PosBook.m_BidMinPOV);
				break;
		case 'O':
				m_PosBook.m_NFQ -= quantityFilled;
				m_PosBook.m_OfferMinPOV -= fillordval;
				m_PosBook.m_OfferMaxPOV -= fillordval;
				strpov = to_string(m_PosBook.m_OfferCOV)+","+to_string(m_PosBook.m_OfferMaxPOV)+","+to_string(m_PosBook.m_OfferMinPOV);
				break;
		default: // assumption: client checks for valid sides before sendng, so this case wont b hit
				break;
		}
		// respond to client order is acknow + nfq-cov-pov:P/F,Id,Nfq,COVb,COVo,POVminb,POVmaxb,POVmino,POVmaxo
		strordfill += to_string(id)+","+to_string(m_PosBook.m_NFQ)+","+to_string(m_PosBook.m_BidCOV)+","+to_string(m_PosBook.m_OfferCOV)+",";
		strordfill += to_string(m_PosBook.m_BidMaxPOV)+","+to_string(m_PosBook.m_BidMinPOV)+","+to_string(m_PosBook.m_OfferMaxPOV)+","+to_string(m_PosBook.m_OfferMinPOV)+"|";
		int ret = m_clientlink.senddata(strordfill.c_str(), strordfill.length());
		//cout<<"reply fill-from-exch to client=["<<strordfill<<"], Wrote to client: ret="<<ret<<"\n";
		return;
	}
	//cout<<"Rec'd fill for non-existent order "<<id<<" from exch!!!, ignore\n";
}

void OrderTracker::OnRequestRejected(int id)
{
	OrdersDB::iterator itr = m_OrdersDb.find(id);
	if (itr != m_OrdersDb.end())
	{
		itr->second.m_State = 'R';
		char side = itr->second.m_Side;
		string strpov;
		double fillordval = itr->second.m_Price * itr->second.m_BalQty;
		switch (side)
		{
		case 'B':
				m_PosBook.m_BidMinPOV -= fillordval;
				m_PosBook.m_BidMaxPOV -= fillordval;
				strpov = to_string(m_PosBook.m_BidCOV)+","+to_string(m_PosBook.m_BidMaxPOV)+","+to_string(m_PosBook.m_BidMinPOV);
				break;
		case 'O':
				m_PosBook.m_OfferMinPOV -= fillordval;
				m_PosBook.m_OfferMaxPOV -= fillordval;
				strpov = to_string(m_PosBook.m_OfferCOV)+","+to_string(m_PosBook.m_OfferMaxPOV)+","+to_string(m_PosBook.m_OfferMinPOV);
				break;
		default: // assumption: client checks for valid sides before sendng, so this case wont b hit
				break;
		}
		// respond to client order is acknow + nfq-cov-pov:R,Id,Nfq,COVb,COVo,POVminb,POVmaxb,POVmino,POVmaxo
		string strordrej = "R,"+to_string(id)+","+to_string(m_PosBook.m_NFQ)+","+to_string(m_PosBook.m_BidCOV)+","+to_string(m_PosBook.m_OfferCOV)+",";
		strordrej += to_string(m_PosBook.m_BidMaxPOV)+","+to_string(m_PosBook.m_BidMinPOV)+","+to_string(m_PosBook.m_OfferMaxPOV)+","+to_string(m_PosBook.m_OfferMinPOV)+"|";
		int ret = m_clientlink.senddata(strordrej.c_str(), strordrej.length());
		//cout<<"reply rej-from-exch to client=["<<strordrej<<"], Wrote to client: ret="<<ret<<"\n";
		return;
	}
	//cout<<"Rec'd reject for non-existent order "<<id<<" from exch!!!, ignore\n";
}



