/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#include "StdAfx.h"

#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <thread>

#include "TestCppClient.h"


const unsigned 	MAX_ATTEMPTS  =  50;
const unsigned 	SLEEP_TIME    =  10;


/* IMPORTANT: always use your paper trading account. The code below will submit orders as part of the demonstration. */
/* IB will not be responsible for accidental executions on your live account. */
/* Any stock or option symbols displayed are for illustrative purposes only and are not intended to portray a recommendation. */
/* Before contacting our API support team please refer to the available documentation. */



int main( 	int argc, char** argv 	)
{

	const char* host = argc > 1 ? argv[ 1 ] : "";
	
	/*
		int  atoi(  const char   *str  )

		Convert a string to an integer. 
	*/	
	int port = argc > 2 ? atoi( argv[ 2 ] ) : 0;

	if( port <= 0 )
		port = 7496;  // production account ( paper account 7497 )
	
	const char* connectOptions = argc > 3 ? argv[ 3 ] : "";
	
	int 		clientId 	= 0;
	unsigned 	attempt 	= 0;

	printf( 			"Start of C++ Socket Client Test %u\n", 
						attempt 										);


	for (;;) 
	{

		++attempt;

		printf( 		   "Attempt %u of %u \n", 
							attempt, 
							MAX_ATTEMPTS 								);

														
		//******************************************
		//
		// create EClientSocket and EReader objects
		//
		//******************************************

		TestCppClient client;

		//******************************************
		//******************************************



		// Run time error will occur (here) if TestCppClient.exe is compiled in debug mode but TwsSocketClient.dll is compiled in Release mode
		// TwsSocketClient.dll (in Release Mode) is copied by API installer into SysWOW64 folder within Windows directory 
		
		if( connectOptions ) 
		{

			client.setConnectOptions( connectOptions );
		
		}
		
		client.connect( 		host, 
								port, 
								clientId 		);
								
		
		int trial = 0;

		while( client.isConnected() ) 
		{


			// stuff goes here !!!!	

			if ( trial == 1 )
			{
				// contractDetails
				client.setState(  	ST_CONTRACTOPERATION  		); 
			}


			if ( trial == 2 )
			{
				// reqMktDepth and reqMktData
 				client.setState(  	ST_REROUTECFD				);
			}

			
			if ( trial == 3 )
			{
				// reqMktDepth and reqMktData
 				client.setState(  	ST_REQMKTDEPTHEXCHANGES		);
			}

			if ( trial == 4 )
			{
	
				// reqMktDepth and reqMktData
 				client.setState(  	ST_TICKDATAOPERATION		);
			}		


			//****************************************
			//****************************************

			client.processMessages();

			//****************************************
			//****************************************

			trial++;
		
		}

		if( attempt >= MAX_ATTEMPTS ) 
		{
			break;
		}

		printf( 				"Sleeping %u seconds before next attempt\n", 
								SLEEP_TIME 											);

		std::this_thread::sleep_for( std::chrono::seconds( SLEEP_TIME ) 			);
	
	}

	printf ( "End of C++ Socket Client Test\n");

}