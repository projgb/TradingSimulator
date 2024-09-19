#ifndef _LISTENER_H_
#define _LISTENER_H_

#include <iostream>
#include <stdio.h>

class Listener
{
     public:
	// These two callbacks represent client requests.
	// Indicates the client has sent a new order request to the market. Exactly one
	// callback will follow:
	// * 'OnOrderRequestAcknowledged', in which case order 'id' is active in the market; or
	// * 'OnRequestRejected', in which case the order was never active in the market.
	virtual void OnInsertOrderRequest(int id,char side, // 'B' for bid, 'O' for offer
	double price,int quantity) = 0;

	// Indicates the client has sent a request to change the quantity of an order.
	// Exactly one callback will follow:
	// * 'OnRequestAcknowledged', in which case the order quantity was modified and the
	// order is now tracked by ID 'newId'; or
	// * 'OnRequestRejected', in which case the order was not modified and remains
	// tracked by ID 'oldId'.
	virtual void OnReplaceOrderRequest(int oldId, // The existing order to modify
	int newId, // The new order ID to use if the modification succeeds
	int deltaQuantity) = 0; // How much the quantity should be increased/decreased

	// These three callbacks represent market confirmations.
	// Indicates the insert or modify request was accepted.
	virtual void OnRequestAcknowledged(int id) = 0;

	// Indicates the insert or modify request was rejected.
	virtual void OnRequestRejected(int id) = 0;

	// Indicates that the order quantity was reduced (and filled) by 'quantityFilled'.
	virtual void OnOrderFilled(int id, int quantityFilled) = 0;
};

#endif

