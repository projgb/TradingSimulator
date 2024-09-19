#ifndef _ORDER_TRACKER_H_
#define _ORDER_TRACKER_H_

#include <iostream>
#include <stdio.h>
#include "OrderInfo.h"
#include "Listener.h"
#include "CommLink.h"

#define LINKMODE 0666

class OrderTracker : public Listener
{
	public:
		OrderTracker();
		~OrderTracker();
		int TrackOrders();
		static void* RunThread(void *arg);
		// Call backs
		void OnInsertOrderRequest(int id,char side, double price,int quantity);
		void OnReplaceOrderRequest(int oldId, int newId, int deltaQuantity);
		void OnRequestAcknowledged(int id);
		void OnRequestRejected(int id);
		void OnOrderFilled(int id, int quantityFilled);
	private:
		// Dont allow copying or assignments
		OrderTracker(const OrderTracker&);
		void operator=(const OrderTracker&);
		// 
		void initLink(const char* linkname);
		int HandleOrders();
		std::string m_OrdRequestLink;
		std::string m_OrdReplyLink;
		std::string m_ExchRequstLink;
		std::string m_ExchReplyLink;
		CommLink m_clientlink;
		CommLink m_exchlink;
        OrdersDB m_OrdersDb;
        PositionsBook m_PosBook;
};

#endif 

