/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#include "../StdAfx.h"


#include "EPosixClientSocketPlatform.h"
#include "EClientSocket.h"

#include "TwsSocketClientErrors.h"
#include "EWrapper.h"
#include "EDecoder.h"
#include "EReaderSignal.h"
#include "EReader.h"
#include "EMessage.h"

#include <string.h>
#include <assert.h>
#include <ostream>


const int MIN_SERVER_VER_SUPPORTED    = 38; //all supported server versions are defined in EDecoder.h


//*******************************************************************************************************************
// member funcs
EClientSocket::EClientSocket(	EWrapper *ptr,  EReaderSignal  *pSignal   ) 
			  				  : EClient(  ptr,  new ESocket()  )
{

	m_fd 				= 	SocketsInit() ? -1 : -2;
    m_allowRedirect 	= 	false;
    m_asyncEConnect 	= 	false;
    m_pSignal 			= 	pSignal;
    m_redirectCount 	= 	0;

}

//*******************************************************************************************************************

EClientSocket::~EClientSocket()
{

	if( m_fd != -2 )
		SocketsDestroy();

}

//*******************************************************************************************************************

bool EClientSocket::allowRedirect() const 
{

    return m_allowRedirect;

}

//*******************************************************************************************************************

void EClientSocket::allowRedirect( bool v ) 
{

    m_allowRedirect = v;

}

//*******************************************************************************************************************

bool EClientSocket::asyncEConnect() const 
{

    return m_asyncEConnect;

}

//*******************************************************************************************************************

void EClientSocket::asyncEConnect( 	bool  val ) 
{

    m_asyncEConnect = val;

}

//*******************************************************************************************************************

bool EClientSocket::eConnect(			const char*		host, 
										int 			port, 
										int 			clientId, 
										bool 			extraAuth				)
{


	if( m_fd == -2 ) 
	{

		getWrapper()->error( 			NO_VALID_ID, 
										FAIL_CREATE_SOCK.code(), 
										FAIL_CREATE_SOCK.msg()					);
		
		return false;
	
	}

	// reset errno
	errno = 0;

	// already connected?
	if( m_fd >= 0 ) 
	{

		errno = EISCONN;

		getWrapper()->error( 				NO_VALID_ID, 
											ALREADY_CONNECTED.code(), 
											ALREADY_CONNECTED.msg()					);
		
		return false;
	
	}

	// normalize host
	const char* hostNorm = ( host && *host ) ? host : "127.0.0.1";

	// initialize host and port
	setHost( 	hostNorm 	);
	setPort( 	port	  	);

	// try to connect to specified host and port
	ConnState resState = CS_DISCONNECTED;

    return eConnectImpl( 				clientId, 
										extraAuth, 
										&resState									);

}

//*******************************************************************************************************************

bool EClientSocket::eConnect(				const char*		host, 
											unsigned int 	port, 
											int 			clientId				) 
{

    return eConnect(			host, 
								static_cast< int >( port ), 
								clientId											);

}

//*******************************************************************************************************************

ESocket *EClientSocket::getTransport() 
{

    assert( dynamic_cast<ESocket*>( m_transport.get() ) != 0 );

    return static_cast<ESocket*>( m_transport.get() );

}

//*******************************************************************************************************************

bool EClientSocket::eConnectImpl(			int 			clientId, 
											bool 			extraAuth, 
											ConnState* 		stateOutPt				)
{

	/*

		#include <netdb.h>

       	struct hostent *gethostbyname(  const char  *name  );

		The gethostbyname() function returns a structure of type hostent
       	for the given host name.  Here name is either a hostname or an
       	IPv4 address in standard dot notation ( as for inet_addr(3) )

	*/

	struct hostent* hostEnt = gethostbyname(  	host().c_str() 	  );


	if ( !hostEnt ) 
	{

		getWrapper()->error( 				NO_VALID_ID, 
											CONNECT_FAIL.code(), 
											CONNECT_FAIL.msg()						);
		
		return false;
	
	}


	//************************************************************
	// creates socket
	//************************************************************

	/*
	    #include <sys/socket.h>

       	int socket(				int 		domain, 
		   						int 		type, 
								int 		protocol			);

		creates an endpoint for communication and returns a file
       	descriptor that refers to that endpoint.

		RETURN VALUE         
       	On success, a file descriptor for the new socket is returned. 
		On error, -1 is returned

	*/

	m_fd = socket(				AF_INET, 		// =  IPv4 protocol
								SOCK_STREAM, 	// =  TCP
								0				// =  IP				
																);

	//************************************************************
	//************************************************************


	// failed to create socket
	if( m_fd < 0 ) 
	{

		getWrapper()->error( 				NO_VALID_ID, 
											FAIL_CREATE_SOCK.code(), 
											FAIL_CREATE_SOCK.msg()					);
		
		return false;
	
	}

	// starting to connect to server
	
	/*

		struct sockaddr_in 
		{
        	sa_family_t    	sin_family; 	// address family: AF_INET 
            in_port_t      	sin_port;   	// port in network byte order
            struct in_addr 	sin_addr;   	// internet address 
       	};

        // Internet address 
        struct in_addr 
		{
			uint32_t       	s_addr;     	// address in network byte order 
        };

	*/

	struct sockaddr_in sa;

	memset( 			&sa, 
						0, 
						sizeof (sa ) 				);
	
	sa.sin_family 		= 	AF_INET;
	sa.sin_port 		= 	htons( port() );
	sa.sin_addr.s_addr 	= 	( (in_addr*)hostEnt->h_addr )->s_addr;


	/*
	
		#include <sys/socket.h>

       	int connect(		int 						sockfd, 
		   					const struct sockaddr 	   *addr,
                   			socklen_t 					addrlen			);

		The connect() system call connects the socket referred to by the
       	file descriptor sockfd to the address specified by addr
	
	*/

	//*********************************************************************
	// connects to server
	//*********************************************************************

	if( ( connect( 	m_fd, (struct sockaddr *) &sa, sizeof( sa ) ) ) < 0   ) 
	{

		// error connecting
		SocketClose( m_fd );

		m_fd = -1;

		getWrapper()->error( 				NO_VALID_ID, 
											CONNECT_FAIL.code(), 
											CONNECT_FAIL.msg()						);
		
		return false;
	
	}


    getTransport()->fd( m_fd );

	// set client id
	setClientId ( 	clientId  	);
	setExtraAuth( 	extraAuth 	);


    int res = sendConnectRequest();


	if ( res < 0 && !handleSocketError() )
		return false;

	if( !isConnected() ) 
	{

		if( connState() != CS_DISCONNECTED ) 
		{
	
			assert( connState() == CS_REDIRECT );
	
			if( stateOutPt ) 
			{
				*stateOutPt = connState();
			}

			eDisconnect();
	
		}

		return false;
	
	}

	// set socket to non-blocking state
	if ( !SetSocketNonBlocking( m_fd ) ) 
	{

		// error setting socket to non-blocking
	
		eDisconnect();
	
		getWrapper()->error( 				NO_VALID_ID, 
											CONNECT_FAIL.code(), 
											CONNECT_FAIL.msg()						);
	
		return false;

	}

	assert( connState() == CS_CONNECTED );
	
	if(  stateOutPt ) 
	{
		*stateOutPt = connState();
	}

    if ( !m_asyncEConnect ) 
	{

        EReader reader( this, m_pSignal );

		reader.putMessageToQueue();

        while ( m_pSignal && !m_serverVersion && isSocketOK() ) 
		{

            m_pSignal->waitForSignal();
            
			reader.processMsgs();
        
		}

    }

	// successfully connected
	return isSocketOK();

}

//****************************************************************************************************************

void EClientSocket::encodeMsgLen(  			std::string& 		msg,  
											unsigned 			offset  		) const
{

	assert( 		!msg.empty() 								);
	assert( 		m_useV100Plus 								);

	assert( 		sizeof( unsigned ) == HEADER_LEN 			);
	assert( 		msg.size() > offset + HEADER_LEN 			);

	unsigned len =  msg.size() - HEADER_LEN - offset;
	

	if( len > MAX_MSG_LEN ) 
	{

		m_pEWrapper->error( 			NO_VALID_ID, 
										BAD_LENGTH.code(), 
										BAD_LENGTH.msg()					);
	
		return;
	
	}

	/*

		#include <arpa/inet.h>

		uint32_t 	htonl(		uint32_t hostlong		);
		uint16_t 	htons(		uint16_t hostshort		);
		uint32_t 	ntohl(		uint32_t netlong		);
		uint16_t   	ntohs(		uint16_t netshort		);

		htonl, htons, ntohl, ntohs - convert values between host and network byte order

		The htonl() function converts the unsigned integer hostlong from host byte order 
		to network byte order. 

	*/

	unsigned netlen = htonl( len );

	memcpy( 			&msg[ offset ], 
						&netlen, 
						HEADER_LEN				);


}

//****************************************************************************************************************

bool EClientSocket::closeAndSend(				std::string 	msg, 
												unsigned 		offset				)
{
	/*
		void assert ( int expression );

		If the argument expression of this macro with functional form compares equal to zero 
		(i.e., the expression is false), a message is written to the standard error device and 
		abort is called, terminating the program execution.
	*/

	assert( !msg.empty() );

	if( m_useV100Plus ) 
	{

		encodeMsgLen( 			msg, 
								offset 				);
	
	}


	//******************************
	//******************************

	if ( bufferedSend( msg ) == -1 )
        return handleSocketError();

	//******************************
	//******************************

    return true;

}

//****************************************************************************************************************

void EClientSocket::prepareBufferImpl(  std::ostream&   buf  ) const
{

	assert( 		m_useV100Plus 								);

	assert( 		sizeof( unsigned ) == HEADER_LEN 			);

	char header[ HEADER_LEN ] = { 0 };

	/*

		ostream& write (	const char* 		s, 
							      streamsize 	n		);

		Write block of data
		Inserts the first n characters of the array pointed by s into the stream.

		This function simply copies a block of data, without checking its contents: 
		The array may contain null characters, which are also copied without stopping 
		the copying process.

	*/	
	
	//************************************************************
	// insert 4 bytes of data pointed by header into buf
	//************************************************************

	buf.write( 				header, 
							sizeof( header ) 					);

	//************************************************************
							
}

//*******************************************************************************************************************

void EClientSocket::prepareBuffer( std::ostream&  buf ) const
{

	if( !m_useV100Plus )
		return;

	prepareBufferImpl( buf );

}

//*******************************************************************************************************************

void EClientSocket::eDisconnect(bool resetState)
{

	if ( m_fd >= 0 )
		SocketClose( m_fd );	// close socket
	
	m_fd = -1;

    if ( resetState ) 
	{
	    eDisconnectBase();
    }

}

//*******************************************************************************************************************

bool EClientSocket::isSocketOK() const
{

	return ( m_fd >= 0 );

}

//*******************************************************************************************************************

int EClientSocket::fd() const
{
	return m_fd;
}

//*******************************************************************************************************************

int EClientSocket::receive(			char* 		buf, 
									size_t 		sz			)
{


	if( sz <= 0 )
		return 0;


	/*	

		"SYSTEM CALL"

	 	reads sz bytes into buf from socket FD
	
		#include <sys/socket.h>
	
    	ssize_t recv(			int 		sockfd, 
		   						void 		*buf, 
								size_t 		len, 
								int 		flags			);

		The recv() calls are used to receive messages from a socket.							
		
		The only difference between recv() and read() is the presence of flags.  
		With a zero flags argument, recv() is generally equivalent to read()

		RETURN VALUE   
		returns the number of bytes received, or -1 if an error occurred.
	
	*/


	int nResult  =  ::recv( 			m_fd, 
										buf, 
										sz, 
										0					);


	if( nResult == -1 && !handleSocketError() ) 
	{
		return -1;
	}
	
	if( nResult == 0 ) 
	{
		onClose();
	}

	if( nResult <= 0 ) 
	{
		return 0;
	}

	return nResult;

}

//*******************************************************************************************************************

void EClientSocket::serverVersion(			int 			version, 
											const char*		time				) 
{

    m_serverVersion = version;
    m_TwsTime 		= time;
    m_redirectCount = 0;

    if( usingV100Plus() ? ( m_serverVersion < MIN_CLIENT_VER || m_serverVersion > MAX_CLIENT_VER ) : m_serverVersion < MIN_SERVER_VER_SUPPORTED ) 
	{

        eDisconnect();
        
		getWrapper()->error( 				NO_VALID_ID, 
											UNSUPPORTED_VERSION.code(), 
											UNSUPPORTED_VERSION.msg() 					);
    
	    return;
    
	}

	if ( !m_asyncEConnect )
		startApi();

}

//*******************************************************************************************************************

void EClientSocket::redirect( 		  const char 	*host, 
											int 	 port 			) 
{

	const char* hostNorm = ( host && *host ) ? host : "127.0.0.1";

	if( ( hostNorm != this->host() || (port > 0 && port != this->port() ) ) ) 
	{

        if ( !m_allowRedirect ) 
		{

            getWrapper()->error(			NO_VALID_ID, 
											CONNECT_FAIL.code(), 
											CONNECT_FAIL.msg()						);

            return;
        
		}

        this->setHost( hostNorm );

        if ( port > 0 ) 
		{
            this->setPort( port );
        }

        ++m_redirectCount;

        if ( m_redirectCount > REDIRECT_COUNT_MAX ) 
		{

            eDisconnect();
            
			getWrapper()->error(			NO_VALID_ID, 
											CONNECT_FAIL.code(), 
											"Redirect count exceeded" 				);
            
			return;
        
		}

        eDisconnect( false );
        
		eConnectImpl( 			clientId(), 
								extraAuth(), 
								0							);
    
	}

}

//*******************************************************************************************************************

bool EClientSocket::handleSocketError()
{

	// no error
	if( errno == 0 )
		return true;

	// Socket is already connected
	if( errno == EISCONN ) 
	{
		return true;
	}

	if( errno == EWOULDBLOCK )
		return false;

	if( errno == ECONNREFUSED ) 
	{
	
		getWrapper()->error( 				NO_VALID_ID, 
											CONNECT_FAIL.code(), 
											CONNECT_FAIL.msg()								);
	
	}
	else 
	{

		getWrapper()->error( 				NO_VALID_ID, 
											SOCKET_EXCEPTION.code(),
											SOCKET_EXCEPTION.msg() + strerror( errno )		);
	
	}
	
	// reset errno
	errno = 0;

	eDisconnect();
	
	return false;

}

//*******************************************************************************************************************
// callbacks from socket

void EClientSocket::onSend()
{

	if ( getTransport()->sendBufferedData() < 0 )
		handleSocketError();

}

//*******************************************************************************************************************

void EClientSocket::onClose()
{
	eDisconnect();
	getWrapper()->connectionClosed();
}

//*******************************************************************************************************************

void EClientSocket::onError()
{
	handleSocketError();
}

//*******************************************************************************************************************
