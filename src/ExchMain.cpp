#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <list>
#include "utils.h"
#include "CommLink.h"
#include "OrderInfo.h"

using namespace std;

typedef list<int> OrdresList;

OrdersDB orderBook;
OrdresList BidOrders;
OrdresList OfferOrders;

// call back handler class for handling client's (via order tracker) inputs
template<class T>
class TraderInputListener : public ICallbackHandler
{
public:
	TraderInputListener(T* ptr):ICallbackHandler(),pTarget(ptr){}
	~TraderInputListener(){}
	int operator()(const std::string& strmesg)
	{
		auto strvec = tokenizesting(strmesg, "|");
		//cout<<"Num tokens "<< strvec.size()<<", tokens\n"; // N|Id|side|qty|price / R|oldid|newid|deltaqty
		char cOrdType = strvec[0][0];
		switch(cOrdType)
		{
		case 'N':
			{
				int ordid = stringtoint(strvec[1]);
				char side = strvec[2][0];
				int qty = stringtoint(strvec[3]);
				double price = stringtodouble(strvec[4]);
				OrdersDB::iterator iter = orderBook.find(ordid);
				if (iter == orderBook.end()) // new order
				{
					OrderInfo neworder(ordid, qty, side, price);
					neworder.m_State = 'A';
					auto empret = orderBook.emplace(ordid, neworder);
					// ack this order
					string ackord = "A,"+to_string(ordid)+"|";
					pTarget->senddata(ackord.c_str(), ackord.length());
					// now try to match this order & fill it
					if(side == 'B')
					{
						BidOrders.push_back(ordid);
						if (!OfferOrders.empty())
						{
							OrdresList::iterator itr = OfferOrders.begin();
							OrdresList::iterator itrend = OfferOrders.end();
							for (; itr != itrend; ++itr)
							{
								OrdersDB::iterator obiter = orderBook.find(*itr);
								if (obiter != orderBook.end() && obiter->second.m_Price == price)
								{
									if (obiter->second.m_BalQty == qty)
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty = 0;
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order also filled:
										fillord = "F,"+to_string(ordid)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										OfferOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										BidOrders.remove(ordid);
										//cout<<"Current 'B' orderid "<<ordid<<" q="<<qty<<", fully filled with '"<<obiter->second.m_Side<<"' id="<<obiter->second.m_Id<<"\n";
										break;
									}
									else if (obiter->second.m_BalQty > qty) // current order is fully filled, but other side order is partially filled
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty -= qty; // partial fill
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(ordid)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										//cout<<"Current 'B' orderid "<<ordid<<" q="<<qty<<", full-filled with '"<<obiter->second.m_Side<<"' id="<<obiter->second.m_Id<<"\n";
										BidOrders.remove(ordid);
										break;
									}
									else // obiter->second.m_BalQty < qty
									{
										obiter->second.m_State = 'F';
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										obiter->second.m_BalQty = 0; // all fill
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(ordid)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										OfferOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty -= qty; // partial fill
										//cout<<"Current 'B' orderid "<<ordid<<" q="<<qty<<", part-filled with '"<<obiter->second.m_Side<<"' id="<<obiter->second.m_Id<<"\n";
									}
								}
							}
						} // offers list not empty
					}
					else
					{
						OfferOrders.push_back(ordid);
						if (!BidOrders.empty())
						{
							OrdresList::iterator itr = BidOrders.begin();
							OrdresList::iterator itrend = BidOrders.end();
							for (; itr != itrend; ++itr)
							{
								OrdersDB::iterator obiter = orderBook.find(*itr);
								if (obiter != orderBook.end() && obiter->second.m_Price == price)
								{
									if (obiter->second.m_BalQty == qty)
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty = 0;
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order also filled:
										fillord = "F,"+to_string(ordid)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										BidOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										OfferOrders.remove(ordid);
										//cout<<"Current replace 'O' orderid "<<ordid<<" q="<<qty<<", fully filled with '"<<obiter->second.m_Side<<"' id="<<obiter->second.m_Id<<"\n";
										break;
									}
									else if (obiter->second.m_BalQty > qty) // current order is fully filled, but other side order is partially filled
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty -= qty; // partial fill
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(ordid)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										OfferOrders.remove(ordid);
										//cout<<"Current replace 'O' orderid "<<ordid<<" q="<<qty<<", fully filled with '"<<obiter->second.m_Side<<"' id="<<obiter->second.m_Id<<"\n";
										break;
									}
									else // obiter->second.m_BalQty < qty
									{
										obiter->second.m_State = 'F';
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										obiter->second.m_BalQty = 0; // all fill
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(ordid)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										BidOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty -= qty; // partial fill
										//cout<<"Current replace 'O' orderid "<<ordid<<" q="<<qty<<", part filled with '"<<obiter->second.m_Side<<"' id="<<obiter->second.m_Id<<"\n";
									}
								}
							}
						} // bids list not empty
					} // offer side
					return 1;
				}
				// reject duplicate order
				string rejord = "R,"+to_string(ordid)+"|";
				pTarget->senddata(rejord.c_str(), rejord.length());
			}
			break;
		case 'R':
			{
				std::string::size_type sz;
				int oldId = stringtoint(strvec[1]);
				int newId = stringtoint(strvec[2]);
				int deltaQuantity = stringtoint(strvec[3]);
				OrdersDB::iterator iter = orderBook.find(oldId);
				if (iter != orderBook.end()) // process order
				{
					OrderInfo oldOrder = iter->second;
					if(oldOrder.m_State == 'F' || oldOrder.m_State == 'R')
					{
						// reject replacing filled/rejected order
						string rejord = "R,"+to_string(newId)+"|";
						pTarget->senddata(rejord.c_str(), rejord.length());
						return 0;
					}
					orderBook.erase(iter);
					oldOrder.m_prevId = oldId;
					oldOrder.m_Id = newId;
					oldOrder.m_Qty += deltaQuantity;
					oldOrder.m_BalQty += deltaQuantity;
					oldOrder.m_State = 'A';
					auto empret = orderBook.emplace(newId, oldOrder);
					char side = oldOrder.m_Side;
					double price = oldOrder.m_Price;
					int qty = oldOrder.m_BalQty;
					// ack this order
					string ackord = "A,"+to_string(newId)+"|";
					pTarget->senddata(ackord.c_str(), ackord.length());
					// now try to match this order & fill it
					if(side == 'B')
					{
						BidOrders.remove(oldId);
						BidOrders.push_back(newId);
						if (!OfferOrders.empty())
						{
							OrdresList::iterator itr = OfferOrders.begin();
							OrdresList::iterator itrend = OfferOrders.end();
							for (; itr != itrend; ++itr)
							{
								OrdersDB::iterator obiter = orderBook.find(*itr);
								if (obiter != orderBook.end() && obiter->second.m_Price == price)
								{
									if (obiter->second.m_BalQty == qty)
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty = 0;
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order also filled:
										fillord = "F,"+to_string(newId)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										OfferOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										BidOrders.remove(newId);
										break;
									}
									else if (obiter->second.m_BalQty > qty) // current order is fully filled, but other side order is partially filled
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty -= qty; // partial fill
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(newId)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										BidOrders.remove(newId);
										break;
									}
									else // obiter->second.m_BalQty < qty
									{
										obiter->second.m_State = 'F';
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										obiter->second.m_BalQty = 0; // all fill
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(newId)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										OfferOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty -= qty; // partial fill
									}
								}
							}
						} // offers list not empty
					}
					else
					{
						OfferOrders.remove(oldId);
						OfferOrders.push_back(newId);
						if (!BidOrders.empty())
						{
							OrdresList::iterator itr = BidOrders.begin();
							OrdresList::iterator itrend = BidOrders.end();
							for (; itr != itrend; ++itr)
							{
								OrdersDB::iterator obiter = orderBook.find(*itr);
								if (obiter != orderBook.end() && obiter->second.m_Price == price)
								{
									if (obiter->second.m_BalQty == qty)
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty = 0;
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order also filled:
										fillord = "F,"+to_string(newId)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										BidOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										OfferOrders.remove(newId);
										break;
									}
									else if (obiter->second.m_BalQty > qty) // current order is fully filled, but other side order is partially filled
									{
										obiter->second.m_State = 'F';
										obiter->second.m_BalQty -= qty; // partial fill
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(newId)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty = 0;
										OfferOrders.remove(newId);
										break;
									}
									else // obiter->second.m_BalQty < qty
									{
										obiter->second.m_State = 'F';
										string fillord = "F,"+to_string(obiter->second.m_Id)+","+to_string(obiter->second.m_BalQty)+"|";
										obiter->second.m_BalQty = 0; // all fill
										pTarget->senddata(fillord.c_str(), fillord.length());
										// current this order fully filled:
										fillord = "F,"+to_string(newId)+","+to_string(qty)+"|";
										pTarget->senddata(fillord.c_str(), fillord.length());
										BidOrders.remove(obiter->second.m_Id);
										empret.first->second.m_State = 'F';
										empret.first->second.m_BalQty -= qty; // partial fill
									}
								}
							}
						} // bids list not empty
					} // offer side
					return 1;
				}
				// reject missing order
				string rejord = "R,"+to_string(newId);
				pTarget->senddata(rejord.c_str(), rejord.length())+"|";
			}
			break;
		default:
			break;
		}
	}
private:
	TraderInputListener(const TraderInputListener&);
	void operator=(const TraderInputListener&);
	T* pTarget;
};

int main(int argc, char** argv)
{
	try
	{
		string strohtoexch = "/tmp/OH_to_EXCH";
		string strexchtooh = "/tmp/EXCH_to_OH";
		CommLink linktoclient(strohtoexch, strexchtooh);
		TraderInputListener<CommLink> ordlistener(&linktoclient);
		linktoclient.init(&ordlistener);
		while(1)
		{
			linktoclient.WaitForEvent(10);
		}
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

