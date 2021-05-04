/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */


#include "../StdAfx.h"
#include "Contract.h"
#include "EDecoder.h"
#include "EMutex.h"
#include "EReader.h"
#include "EClientSocket.h"
#include "EPosixClientSocketPlatform.h"
#include "EReaderSignal.h"
#include "EMessage.h"
#include "DefaultEWrapper.h"



#define IN_BUF_SIZE_DEFAULT 8192

static DefaultEWrapper defaultWrapper;


//***************************************************************************************************


EReader::EReader(			EClientSocket*		clientSocket, 
							EReaderSignal*		signal					)

							:  processMsgsDecoder_( 		clientSocket->EClient::serverVersion(), 
															clientSocket->getWrapper(), 
															clientSocket 							)

#if defined(IB_POSIX)

    , m_hReadThread( pthread_self() )

#elif defined(IB_WIN32)

    , m_hReadThread(0)

#endif
{

		m_isAlive 			= true;
        m_pClientSocket 	= clientSocket;       
		m_pEReaderSignal 	= signal;
		m_nMaxBufSize 		= IN_BUF_SIZE_DEFAULT;

		m_buf.reserve( IN_BUF_SIZE_DEFAULT );

}

//***************************************************************************************************

EReader::~EReader( void ) 
{

#if defined(IB_POSIX)
    
	if ( !pthread_equal( pthread_self(), m_hReadThread ) ) 
	{

        m_isAlive = false;

        m_pClientSocket->eDisconnect();
		
		pthread_join( m_hReadThread, NULL );
    
	}

#elif defined(IB_WIN32)
    
	if (m_hReadThread) 
	{
        m_isAlive = false;
        m_pClientSocket->eDisconnect();
        WaitForSingleObject(m_hReadThread, INFINITE);
    }

#endif

}

//***************************************************************************************************

void EReader::start() 
{


#if defined(IB_POSIX)

//********************************************************************
// create thread
//********************************************************************  

	pthread_create( 			&m_hReadThread, 
								NULL, 
								readToQueueThread, 
								this 								);

//********************************************************************
//********************************************************************


#elif defined(IB_WIN32)

    m_hReadThread = CreateThread(0, 0, readToQueueThread, this, 0, 0);

#else

#   error "Not implemented on this platform"

#endif

}

//***************************************************************************************************


#if defined(IB_POSIX)

void * EReader::readToQueueThread( void* lpParam )

#elif defined(IB_WIN32)

DWORD WINAPI EReader::readToQueueThread(LPVOID lpParam)

#else

#   error "Not implemented on this platform"

#endif

{

	EReader *pThis = reinterpret_cast< EReader* >( lpParam );

	pThis->readToQueue();

	return 0;

}


//***************************************************************************************************

void EReader::readToQueue() 
{

	// EMessage *msg = 0;

	while ( m_isAlive )  // true as long as it's not invoked EReader::~EReader
	{


		// STEP I:
		// read a message from Socket
		// if there's data in outgoing buffer, it signals the other thread   
		if (  m_buf.size() == 0   &&  !processNonBlockingSelect()  &&  m_pClientSocket->isSocketOK() )
			continue;


		// STEP II:
		// insert message into the queue
        if ( !putMessageToQueue() )
			break;


	}

	// if error 

	m_pClientSocket->handleSocketError();

	m_pEReaderSignal->issueSignal(); // letting client know that socket was closed

}

//***************************************************************************************************

bool EReader::putMessageToQueue() 
{

	EMessage *msg = 0;


	if ( m_pClientSocket->isSocketOK() )
		msg = readSingleMsg();

	if ( msg == 0 )
		return false;

	{

		EMutexGuard lock( m_csMsgQueue );
		
		//************************************************************
		// Inserts element into the queue
		//************************************************************

		m_msgQueue.push_back(  std::shared_ptr< EMessage >(  msg  )  );

		//************************************************************
		//************************************************************
	
	}

	m_pEReaderSignal->issueSignal();

	return true;

}

//***************************************************************************************************

bool EReader::processNonBlockingSelect() 
{


	fd_set 	readSet ;
	fd_set	writeSet;
	fd_set	errorSet;

	struct timeval  tval;

	tval.tv_usec = 100 * 1000; // timeout 100ms
	tval.tv_sec  = 0;


	if( m_pClientSocket->fd() >= 0 ) 
	{
		/*
			void FD_ZERO(	fd_set  *set	);

			This macro removes all FDs from set. Used during initialization.
		*/

		FD_ZERO( 			&readSet								);
		FD_ZERO( 			&writeSet 								);
		FD_ZERO( 			&errorSet 								);

		/*

			void FD_SET(		int 		fd, 
								fd_set 		*set		);

			This macro adds the file descriptor fd to set.
		
		*/

		FD_SET( 			m_pClientSocket->fd(), 
							&readSet								);


		if ( !m_pClientSocket->getTransport()->isOutBufferEmpty() )
		{

			FD_SET( 			m_pClientSocket->fd(), 
								&writeSet 							);
		
		}


		FD_SET( 			m_pClientSocket->fd(), 
							&errorSet 								);

		/*

		int select(			int 							nfds, 
							fd_set 			*restrict 		readfds,
                  			fd_set 			*restrict 		writefds, 
							fd_set 			*restrict 		exceptfds,
                  			struct timeval 	*restrict 		timeout				);

		select() allows a program to monitor multiple file descriptors, waiting one or more become
		"ready" for reading, writing, or exceptions.

		nfds: highest-numbered FD in any of the 3 sets, plus 1

		timeout:  this argument is a timeval structure that specifies the interval that select() 
				  should block waiting for a file descriptor to become ready. The call will block 
				  until either:

					• a file descriptor becomes ready;

					• the call is interrupted by a signal handler; or

					• the timeout expires.

		RETURN VALUE

		On success, select() returns the number of file descriptors contained in the three 
		returned descriptor sets ( that is, the total number of bits that are set in readfds, 
		writefds, exceptfds ).  
		   
		The return value may be zero if the timeout expired before any file descriptors 
		became ready.

       	On error, -1 is returned, and errno is set to indicate the error; the file descriptor 
		sets are unmodified, and timeout becomes undefined.

		*/

		//***********************************************************************************
		//***********************************************************************************

		int ret = select( 				 m_pClientSocket->fd() + 1, 
										&readSet, 
										&writeSet, 
										&errorSet, 
										&tval									);

		//***********************************************************************************
		//***********************************************************************************


		if( ret == 0 ) // timeout expired
		{ 

			return false;
		
		}

		if( ret < 0 ) // error
		{	

			m_pClientSocket->eDisconnect();
		
			return false;
		
		}

		if( m_pClientSocket->fd() < 0 ) 
			return false;

		if( FD_ISSET( m_pClientSocket->fd(), &errorSet ) ) 
		{
			
			m_pClientSocket->onError();	// error on socket
		
		}

		if( m_pClientSocket->fd() < 0 )  
			return false;

		/*

			int   FD_ISSET( 		int 	 	 fd, 
									fd_set 	 	*set			);

			select() modifies the contents of the sets according to
            the rules established. After calling select(), the
			FD_ISSET() macro can be used to test if a file descriptor
			is still present in a set.  
			
			FD_ISSET() returns nonzero if the file descriptor fd is present 
			in set, and zero if it is not.

		*/

		if( FD_ISSET( m_pClientSocket->fd(), &writeSet ) ) 
		{

			//************************************
			//************************************

			 onSend(); // socket ready for writing
		
			//************************************
			//************************************

		}

		if( m_pClientSocket->fd() < 0 )
			return false;


		if( FD_ISSET( m_pClientSocket->fd(), &readSet ) ) 
		{

			//*****************************************
			//*****************************************

			 onReceive(); // socket is ready for reading

			//*****************************************
			//*****************************************			
		
		}

		return true;
	
	}

	return false;

}

//***************************************************************************************************

void EReader::onSend() 
{

	m_pEReaderSignal->issueSignal();

}

//***************************************************************************************************

void EReader::onReceive() 
{

	int nOffset = m_buf.size();  	// m_buf is a vector of char

	m_buf.resize( m_nMaxBufSize ); 	// 8192 bytes

	/*
		data()
		Returns a pointer such that [ data(), data() + size() is a valid range. 
	*/

	int nRes = m_pClientSocket->receive( 				m_buf.data() + nOffset, 
														m_buf.size() - nOffset 				);

	if ( nRes <= 0 )
		return;

 	m_buf.resize( nRes + nOffset );	

}

//***************************************************************************************************

bool EReader::bufferedRead( 		char*			buf, 
									unsigned int 	size			) 
{

	while ( size > 0 ) 
	{
	
		while ( 		m_buf.size() < size 			&& 
						m_buf.size() < m_nMaxBufSize 				) 
		{

			if ( !processNonBlockingSelect() && !m_pClientSocket->isSocketOK() )
				return false;

		}

		/*

			template <class T> const T& min ( const T& a, const T& b );

			Returns the smallest of a and b. If both are equivalent, a is returned.				

		*/

		int nBytes = ( std::min<unsigned int> )(  	m_nMaxBufSize,  size  	);


		/*

			template <class InputIterator, class OutputIterator>
  			OutputIterator copy (		InputIterator 		first, 
			  							InputIterator 		last, 
										OutputIterator 		result			);

			Copies the elements in the range [first,last) into the range beginning at result.

		*/


		std::copy(			m_buf.begin(), 
							m_buf.begin() + nBytes, 
							buf												);
		
		std::copy(			m_buf.begin() + nBytes, 
							m_buf.end(), 
							m_buf.begin()									);

		m_buf.resize( 		m_buf.size() - nBytes 							);

		size -= nBytes;
		buf  += nBytes;

	}

	return true;

}

//***************************************************************************************************


EMessage*  EReader::readSingleMsg() 
{

	if ( m_pClientSocket->usingV100Plus() ) 
	{

		int msgSize;


		if ( !bufferedRead( 	(char*)&msgSize, 	sizeof( msgSize ) 	) )
			return 0;

		msgSize = ntohl( msgSize );

		if ( msgSize <= 0 || msgSize > MAX_MSG_LEN )
			return 0;

		std::vector< char > buf = std::vector< char >( msgSize );

		if ( !bufferedRead( 	buf.data(), 	buf.size() 		) 	)
			return 0;

		return new EMessage( buf );
	
	}
	else 
	{

		const char *pBegin = 0;
		const char *pEnd   = 0;

		int msgSize        = 0;

		while ( msgSize == 0 )
		{

			if ( m_buf.size() >= m_nMaxBufSize * 3/4 ) 
				m_nMaxBufSize *= 2;

			if ( !processNonBlockingSelect() && !m_pClientSocket->isSocketOK() )
				return 0;
		
			pBegin 	= m_buf.data();
			pEnd 	= pBegin + m_buf.size();
			
			msgSize = EDecoder(	m_pClientSocket->EClient::serverVersion(), &defaultWrapper ).parseAndProcessMsg( pBegin, pEnd );
		
		}
	
		std::vector<char> msgData( msgSize );

		if ( !bufferedRead( msgData.data(), msgSize ) )
			return 0;

		if ( m_buf.size() < IN_BUF_SIZE_DEFAULT && m_buf.capacity() > IN_BUF_SIZE_DEFAULT )
		{

			m_buf.resize( m_nMaxBufSize = IN_BUF_SIZE_DEFAULT );
		
			m_buf.shrink_to_fit();
		
		}

		EMessage * msg = new EMessage( msgData );

		return msg;

	}

}

//***************************************************************************************************

std::shared_ptr<EMessage> EReader::getMsg( void ) 
{

	EMutexGuard lock( m_csMsgQueue );

	// checks whether there is any element in the queue
	if ( m_msgQueue.size() == 0 )  
	{

		return std::shared_ptr< EMessage >();
	
	}


	//***************************************************
	//***************************************************

	// it takes queue's first element 
	std::shared_ptr< EMessage > msg = m_msgQueue.front();

	// it deletes queue's first element
	m_msgQueue.pop_front();

	//**************************************************
	//**************************************************


	return msg;

}

//***************************************************************************************************


void EReader::processMsgs( void ) 
{


	//**********************************
	// send bytes on buffer to socket FD
	//**********************************

	m_pClientSocket->onSend();

	//**********************************
	//**********************************



	//*****************************************
	// get the first message from the queue 
	//*****************************************

	std::shared_ptr< EMessage > msg = getMsg();

	//*****************************************
	//*****************************************


	if ( !msg.get() )
		return;

	const char *pBegin = msg->begin();


	//*****************************************
	// loop goes processing messages one by one
	//*****************************************

	while ( processMsgsDecoder_.parseAndProcessMsg(  pBegin,  msg->end()  ) > 0 ) 
	{


		//**********************************
		// get the next message in the queue
		//**********************************

		msg = getMsg();

		//**********************************
		//**********************************


		if ( !msg.get() )  // if no more messages, breaks the loop !
			break;

		pBegin = msg->begin();

	} 

	//*****************************************
	//*****************************************

}

//***************************************************************************************
