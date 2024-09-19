#include <iostream>
#include <stdio.h>
#include <signal.h>
#include "OrderTracker.h"

using namespace std;

int main(int argc, char** argv)
{
	try
	{
		OrderTracker orderTracker;
		orderTracker.TrackOrders();
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

