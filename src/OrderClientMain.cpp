#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <list>
#include "utils.h"
#include "CommLink.h"
#include "OrderInfo.h"

using namespace std;

pthread_mutex_t listmutex;
OrdersDB orderBook;
list<string> OrdersToOH;

// call back handler class for handling order tracker's inputs
class OHInputListener : public ICallbackHandler
{
public:
	OHInputListener():ICallbackHandler(){}
	~OHInputListener(){}
	void processMesg(const std::string& strmesg)
	{
		auto strvec = tokenizesting(strmesg, ",");
		//cout<<"Num tokens "<< strvec.size()<<", tokens:"<<strmesg<<"\n"; // A,Id,Nfq,COVb,COVo,POVminb,POVmaxb,POVmino,POVmaxo
		char cReply = strvec[0][0];
		switch(cReply)
		{
		case 'A': // ack from exch
			{
				int id = stringtoint(strvec[1]);
				OrdersDB::iterator itr = orderBook.find(id);
				if (itr != orderBook.end())
				{
					itr->second.m_State = cReply;
					cout<<"Order "<<id<<" accpeted on exch:Positions details\n";
					showPositions(strvec);
				}
			}
			break;
		case 'F': // fill from exch
			{
				int id = stringtoint(strvec[1]);
				OrdersDB::iterator itr = orderBook.find(id);
				if (itr != orderBook.end())
				{
					itr->second.m_State = cReply;
					cout<<"Order "<<id<<" fully-filled on exch:Positions details\n";
					showPositions(strvec);
				}
			}
			break;
		case 'P': // partial fill from exch
			{
				int id = stringtoint(strvec[1]);
				OrdersDB::iterator itr = orderBook.find(id);
				if (itr != orderBook.end())
				{
					itr->second.m_State = cReply;
					cout<<"Order "<<id<<" partially-filled on exch:Positions details\n";
					showPositions(strvec);
				}
			}
			break;
		case 'Q': // order queued to - pending at exch
			{
				int id = stringtoint(strvec[1]);
				OrdersDB::iterator itr = orderBook.find(id);
				if (itr != orderBook.end())
				{
					itr->second.m_State = cReply;
					cout<<"Order "<<id<<" pending on (queued to) exch:Positions details\n";
					showPositions(strvec);
				}
			}
			break;
		case 'R': // order reject from exch
			{
				int id = stringtoint(strvec[1]);
				OrdersDB::iterator itr = orderBook.find(id);
				if (itr != orderBook.end())
				{
					itr->second.m_State = cReply;
					cout<<"Order "<<id<<" rejected from exch:Positions details\n";
					showPositions(strvec);
				}
			}
			break;
		default:
			break;
		}
	}
	int operator()(const std::string& strmesg)
	{
		auto vecmesgs = tokenizesting(strmesg, "|");
		int nummesgs = vecmesgs.size();
		//cout<<"OHInputListener recd mesg="<<strmesg<<", num tokens="<<nummesgs<<"\n";
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
	OHInputListener(const OHInputListener&);
	void operator=(const OHInputListener&);
	void showPositions(const StrVec& strvec) // 2Nfq,3COVb,4COVo,5POVminb,6POVmaxb,7POVmino,8POVmaxo
	{
		cout<<"NFQ	|COVbid	|COVoffer|POVbidmin	|POVbidmax	|POVoffmin	|POVoffmax\n";
		int nfq = stringtoint(strvec[2]);
		double COVbid = stringtodouble(strvec[3]);
		double COVoffer = stringtodouble(strvec[4]);
		double POVbidmin = stringtodouble(strvec[5]);
		double POVbidmax = stringtodouble(strvec[6]);
		double POVoffmin = stringtodouble(strvec[7]);
		double POVoffmax = stringtodouble(strvec[8]);
		printf("%d	|%.2F	|%.2F	|%.2F	|%.2F	|%.2F	|%.2F	\n",nfq, COVbid, COVoffer, POVbidmin, POVbidmax, POVoffmin, POVoffmax);
	}
};

// thread to handle com with Order tracker
static void* RunThread(void *arg)
{
	int iOrderId = 0;
	string strLinkFromOH = "/tmp/OH_to_CLIENT";
	string strLinktoOH = "/tmp/CLIENT_to_OH";
	OHInputListener clientListener;
	CommLink linktotracker(strLinkFromOH, strLinktoOH);
	linktotracker.init(&clientListener);
	bool bStop = false;
	while(bStop == false)
	{
		struct timespec tv = {10, 0};
		int ret = -1;
		string userinput = "";
		pthread_mutex_lock(&listmutex);
		if (!OrdersToOH.empty())
		{
			userinput = OrdersToOH.front();
			OrdersToOH.pop_front();
		}
		pthread_mutex_unlock(&listmutex);
		if (!userinput.empty())
		{
			string strorderInfo;
			OrderInfo orderInfo;
			char userChoice = userinput[0];
			//cout<<"userinput=["<<userinput<<"]\n";
			string strord = userinput.erase(0, 2);
			switch(userChoice)
			{
				case 'N': // new order
					{
						auto strvec = tokenizesting(strord, ",");
						int numtokens = strvec.size();
						if (numtokens < 3)
						{
							//cout<<"   WARN: User entered only "<<numtokens<<" parameters, enter correct order.\n";
							continue;
						}
						orderInfo.m_Qty = stringtoint(strvec[1]);
						if (orderInfo.m_Qty <= 0)
						{
							cout<<"Entered invalid quantity "<<orderInfo.m_Qty<<", enter only > 0\n";
							continue;
						}
						orderInfo.m_Price = stringtodouble(strvec[2]);
						if (orderInfo.m_Price <= 0)
						{
							cout<<"ntered invalid price "<<orderInfo.m_Price<<", enter only > 0\n";
							continue;
						}
						orderInfo.m_Side = strvec[0][0];
						iOrderId++;
						orderInfo.m_Id = iOrderId;
						//cout<<"   INF: User's new order="<<strord<<", at orderid="<<iOrderId<<":";
						strorderInfo = "N,"+to_string(iOrderId)+","+strord;
						orderBook.emplace(orderInfo.m_Id,orderInfo);
						ret = linktotracker.senddata(strorderInfo.c_str(), strorderInfo.length());
						//cout<<"   INF: Wrote to ordertracker: ret="<<ret<<", wait OH response\n";
					}
					break;
				case 'R':// replace order
					{
						auto strvec = tokenizesting(strord, ",");
						int numtokens = strvec.size();
						if (numtokens < 2)
						{
							//cout<<"   WARN: User entered only "<<numtokens<<" parameters, enter correct order.\n";
							continue;
						}
						int previd = stringtoint(strvec[0]);
						OrdersDB::iterator itr = orderBook.find(previd);
						if (itr == orderBook.end()) // invalid id
						{
							cout<<"Old order id "<<previd<<" to replace does not exist, try again\n";
							continue;
						}
						orderInfo = itr->second;
						if (orderInfo.m_State == 'F' || orderInfo.m_State == 'R')
						{
							cout<<"Old order id "<<previd<<" filled/rejected, cannot replace\n";
							continue;
						}
						int deltaqty = stringtoint(strvec[1]); // assumption:store delta-qty here, send delta to OH & exch
						if (deltaqty == 0)
						{
							cout<<"Entered invalid quantity "<<deltaqty<<", enter only other than 0\n";
							continue;
						}
						int effqty = orderInfo.m_Qty + deltaqty;
						if (effqty == 0) // order qty goes to zero, same as order cancel, reject this 
						{
							cout<<"Old order id "<<previd<<" qty "<<orderInfo.m_Qty<<" will be zero if replaced with this delta "<<deltaqty<<", cannot replace.\n";
							continue;
						}
						iOrderId++;
						orderBook.erase(itr);
						orderInfo.m_prevId = orderInfo.m_Id; // old order id
						orderInfo.m_Id = iOrderId;  // new order id
						orderInfo.m_Qty = deltaqty;
						//cout<<"   INF: User's replace order="<<strord<<", at orderid="<<iOrderId<<":";
						strorderInfo = "R,"+to_string(previd)+","+to_string(iOrderId)+","+to_string(orderInfo.m_Qty);
						orderBook.emplace(orderInfo.m_Id,orderInfo);
						ret = linktotracker.senddata(strorderInfo.c_str(), strorderInfo.length());
						//cout<<"   INF: Wrote to ordertracker: ret="<<ret<<", wait OH response\n";
					}
					break;
				case 'S': // queue exit signal to thread & wait for it to join
					//cout <<"User requesting stopping\n";
					linktotracker.CloseLink();
					pthread_exit(NULL);
					break;
			}
		} // pthreadcondwait return
		linktotracker.WaitForEvent(10);
	} // main while loop
	pthread_exit(NULL);
}

//------------------
void showOrdersforReplace()
{
	if (orderBook.empty())
	{
		cout<<"No orders to replace, try again after entering new orders\n";
		return;
	}
	OrdersDB::iterator itr = orderBook.begin();
	OrdersDB::iterator itrEnd = orderBook.end();
	for (; itr != itrEnd; ++itr)
	{
		OrderInfo ord = itr->second;
		cout<<"ID:"<<itr->first<<", Side:"<<ord.m_Side<<", State:"<<ord.m_State<<", Price:"<<ord.m_Price<<", Qty:"<<ord.m_Qty<<"\n";
	}
}

int getUserInput(string& userinput)
{
	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	nread = getline(&line, &len, stdin);
	if (nread < 0)
	{
		//cout<<"  Warn:No input rec'd or input invalid , continue\n";
		free(line);
		return 0;
	}
	line[nread-1] = '\0';
	userinput.assign(line);
	free(line);
	return 1;
}

int main(int argc, char** argv)
{
	try
	{
		pthread_t ordTid;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		pthread_mutex_init(&listmutex, NULL);
		int ret = pthread_create(&ordTid, &attr, RunThread, NULL);
		if (ret) // thread create failed report error
		{
			string strerrmesg;
			formatErrorString(ret, "X-Client:pthr_create:", strerrmesg);
			throw runtime_error(strerrmesg.c_str());
		}
		bool bStop = false;
		while(bStop == false)
		{
			string userinput;
			cout << "Choose action:1: New order, 2: Replace Order, 3: STOP:\n";
			if (getUserInput(userinput) > 0)
			{
				if (isdigit(userinput[0]) == 0)
				{
					//cout<<"   Warn:Enter a number between 1 and 3 :\n";
					continue;
				}
				int choice = stringtoint(userinput);
				switch(choice)
				{
					case 0: userinput.clear(); cout<<"Unknown choice,try again: \n";
						break;
					case 1: cout<<"Enter new order in format:Side,Quantity,Price: B for Bid, O for Offer: \n";
						if (getUserInput(userinput) == 0)
						{
							continue;
						}
						if (userinput.find("B,")==string::npos && userinput.find("O,")==string::npos)
						{
							cout<<"Entered invalid order side ["<<userinput<<", enter only 'B'id or 'O'ffer\n";
							continue;
						}
						{
							string ordtooh = "N,"+userinput;
							pthread_mutex_lock(&listmutex);
							OrdersToOH.push_back(ordtooh);
							//cout<<"New order into queue:"<<ordtooh<<", size="<<OrdersToOH.size()<<"\n";
							pthread_mutex_unlock(&listmutex);
							pthread_yield();
						}
						break;
					case 2: cout<<"Enter replace order in format:OldId,DeltaQuantity (+ to add, - to reduce): \n";
						showOrdersforReplace();
						if (getUserInput(userinput) == 0)
						{
							continue;
						}
						{
							string ordtooh = "R,"+userinput;
							pthread_mutex_lock(&listmutex);
							OrdersToOH.push_back(ordtooh);
							//cout<<"Repl order into queue:"<<ordtooh<<", size="<<OrdersToOH.size()<<"\n";
							pthread_mutex_unlock(&listmutex);
							pthread_yield();
						}
						break;
					case 3: cout<<"User input STOP: exit order entry\n";
						{
							string ordtooh = "S,T,O,P";
							pthread_mutex_lock(&listmutex);
							OrdersToOH.push_back(ordtooh);
							pthread_mutex_unlock(&listmutex);
							pthread_yield();
							bStop = true;
						}
						break;
					default:continue;
				} // switch
			}
		} // while loop foruser input
		/* Free attribute and wait for the other threads */
		while(1)
		{
			int ret = pthread_join(ordTid, NULL);
			if (ret == 0)
			{
				cout<<"pthread_join returned, order thread exited\n";
				break;
			}
		}
		return 0;
	}
	catch(exception& ex)
	{
		cout << ex.what();
	}
	catch(...)
	{
		cout<<"Warn:Caught unknown exception!\n";
	}
	return 0;
}

