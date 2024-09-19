#ifndef _ORDER_INFO_H_
#define _ORDER_INFO_H_

#include <iostream>
#include <stdio.h>
#include <map>

class OrderTracker;

/*
 * Order states: 'D'ormant(exch not up, not sent yet), 'Q'ueued(pending ack), 'A'cknowledged
 *               'P'artially filled, 'F'illed, 'R'ejected
 * Valid sides : 'B'id, 'O'ffer
 */
struct OrderInfo
{
   OrderInfo()
   :m_Id(0)
   ,m_Qty(0)
   ,m_BalQty(0)
   ,m_prevId(-1)
   ,m_Side('u')
   ,m_State('u')
   ,m_Price(0.0)
   {
   }
   OrderInfo(int id, int qty, char side, double price)
   :m_Id(id)
   ,m_Qty(qty)
   ,m_BalQty(qty)
   ,m_prevId(-1)
   ,m_Side(side)
   ,m_State('u')
   ,m_Price(price)
   {
   }
   OrderInfo(const OrderInfo& oi)
   :m_Id(oi.m_Id)
   ,m_Qty(oi.m_Qty)
   ,m_BalQty(oi.m_BalQty)
   ,m_prevId(m_prevId)
   ,m_Side(oi.m_Side)
   ,m_State(oi.m_State)
   ,m_Price(oi.m_Price)
   {
   }
   int m_Id;
   int m_Qty; // original qty, never change this
   int m_BalQty; // change this asper fill or partial fill, at end when order fully filled
   int m_prevId;
   char m_Side;
   char m_State;
   double m_Price;
};

typedef std::map<int, OrderInfo> OrdersDB;

//--- 
struct PositionsBook
{
    PositionsBook():m_NFQ(0)
				,m_BidCOV(0.0)
				,m_OfferCOV(0.0)
				,m_BidMinPOV(0.0)
				,m_BidMaxPOV(0.0)
				,m_OfferMinPOV(0.0)
				,m_OfferMaxPOV(0.0)
    {
    }
    PositionsBook(const PositionsBook&);
    void operator=(const PositionsBook&);
    int m_NFQ;
    double m_BidCOV;
    double m_OfferCOV;
    double m_BidMinPOV;
    double m_BidMaxPOV;
    double m_OfferMinPOV;
    double m_OfferMaxPOV;
};



#endif

