/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#include "StdAfx.h"
#include "EReaderOSSignal.h"

#if defined(IB_POSIX)
#if defined(IBAPI_MONOTONIC_TIME)
#include <time.h>
#else
#include <sys/time.h>
#endif
#endif



//******************************************************************************************

EReaderOSSignal::EReaderOSSignal( unsigned long  waitTimeout )
{

    bool ok = true;

    m_waitTimeout = waitTimeout;

#if defined(IB_POSIX)

    /*

        #include <pthread.h>

        int pthread_mutex_init(               pthread_mutex_t       *mutex, 
                                        const pthread_mutexattr_t   *attr           );

        The pthread_mutex_init() function initialises the mutex referenced by mutex with attributes 
        specified by attr. If attr is NULL, the default mutex attributes are used; the effect is 
        the same as passing the address of a default mutex attributes object.

        RETURN VALUE
        If successful, the pthread_mutex_init function returns zero. 

    */

    ok = ok && !pthread_mutex_init      (       &m_mutex,   NULL        );

    ok = ok && !pthread_condattr_init   (       &m_condattr             );


#if defined(IBAPI_MONOTONIC_TIME)

    ok = ok && !pthread_condattr_setclock(&m_condattr, CLOCK_MONOTONIC);

#endif

    ok = ok && !pthread_cond_init(              &m_evMsgs, 
                                                &m_condattr                 );

    open = false; 

#elif defined(IB_WIN32)

	m_evMsgs = CreateEvent(0, false, false, 0);
    ok = (NULL != m_evMsgs);

#else

#   error "Not implemented on this platform"

#endif

	if ( !ok )
		throw std::runtime_error( "Failed to create event" );

}

//******************************************************************************************

EReaderOSSignal::~EReaderOSSignal(  void  )
{

#if defined(IB_POSIX)
  
    pthread_cond_destroy    (   &m_evMsgs     );
    pthread_condattr_destroy(   &m_condattr   );
    pthread_mutex_destroy   (   &m_mutex      );

#elif defined(IB_WIN32)
	
    CloseHandle(m_evMsgs);

#else

#   error "Not implemented on this platform"

#endif

}

//******************************************************************************************

void EReaderOSSignal::issueSignal() 
{

    #if defined(IB_POSIX)   


        //************************************************
        // Entering critical section
        //************************************************


        pthread_mutex_lock      (       &m_mutex        );

            open = true;

            pthread_cond_signal (       &m_evMsgs       );

        pthread_mutex_unlock    (       &m_mutex        );


        //************************************************
        //************************************************


    #elif defined(IB_WIN32)

        SetEvent(m_evMsgs);

    #else

    #   error "Not implemented on this platform"

    #endif

}

//******************************************************************************************

void EReaderOSSignal::waitForSignal() 
{

#if defined(IB_POSIX)


    //*********************************
    // Entering critical 
    //*********************************

    pthread_mutex_lock(   &m_mutex   );

    //*********************************


    if ( !open ) 
    {

        if ( m_waitTimeout == INFINITE ) 
        {

            /*

                #include <pthread.h>

                int pthread_cond_timedwait(         pthread_cond_t  *restrict cond,
                                                    pthread_mutex_t *restrict mutex,
                                              const struct timespec *restrict abstime       );


                The pthread_cond_timedwait() and pthread_cond_wait() functions shall block 
                on a condition variable. They shall be called with mutex locked by the calling 
                thread or undefined behavior results.

                These functions atomically release mutex and cause the calling thread to 
                block on the condition variable cond; atomically here means "atomically 
                with respect to access by another thread to the mutex and then the condition 
                variable". That is, if another thread is able to acquire the mutex after 
                the about-to-block thread has released it, then a subsequent call to 
                pthread_cond_broadcast() or pthread_cond_signal() in that thread shall behave 
                as if it were issued after the about-to-block thread has blocked.

                Upon successful return, the mutex shall have been locked and shall be 
                owned by the calling thread.

            */    


            //********************************************************
            //********************************************************

            pthread_cond_wait(              &m_evMsgs, 
                                            &m_mutex                );
        
            //********************************************************
            //********************************************************


        }
        else 
        {

            struct timespec ts;

#if defined(IBAPI_MONOTONIC_TIME)

            clock_gettime(CLOCK_MONOTONIC, &ts);

#else

            // on Mac OS X, clock_gettime is not available, stick to gettimeofday for the moment
            struct timeval tv;

            /*

                #include <sys/time.h>

                int gettimeofday(       struct timeval      *restrict tv,
                                        struct timezone     *restrict tz        );

                This function gets time. The use of the timezone structure is obsolete; the tz argument
                should normally be specified as NULL

                RETURN VALUE 
                gettimeofday() returns 0 for success.  On error, -1 is returned.

            */
            

            gettimeofday(           &tv, 
                                    NULL            );

            /*

                #include <time.h>

                struct timespec 
                {
                    time_t   tv_sec;
                    long     tv_nsec;
                }

                The timespec structure specifies a time in seconds and nanoseconds. The 
                members include:
                tv_sec    The number of seconds. If specifying an absolute time, this member is the number of seconds since 1970. 
                tv_nsec   The number of nanoseconds. 

            */


            ts.tv_sec   =   tv.tv_sec;
            ts.tv_nsec  =   tv.tv_usec * 1000;

#endif

            ts.tv_sec   +=  m_waitTimeout / 1000; 
            ts.tv_nsec  +=  1000 * 1000 * ( m_waitTimeout % 1000 );
            ts.tv_sec   +=  ts.tv_nsec / ( 1000 * 1000 * 1000 );
            ts.tv_nsec  %=  ( 1000 * 1000 * 1000 );


            /*

                #include <pthread.h>

                int pthread_cond_timedwait(         pthread_cond_t  *restrict cond,
                                                    pthread_mutex_t *restrict mutex,
                                              const struct timespec *restrict abstime       );


                The pthread_cond_timedwait() and pthread_cond_wait() functions shall block 
                on a condition variable. They shall be called with mutex locked by the calling 
                thread or undefined behavior results.

                These functions atomically release mutex and cause the calling thread to 
                block on the condition variable cond; atomically here means "atomically 
                with respect to access by another thread to the mutex and then the condition 
                variable". That is, if another thread is able to acquire the mutex after 
                the about-to-block thread has released it, then a subsequent call to 
                pthread_cond_broadcast() or pthread_cond_signal() in that thread shall behave 
                as if it were issued after the about-to-block thread has blocked.

                Upon successful return, the mutex shall have been locked and shall be 
                owned by the calling thread.

            */

            //***********************************************************
            //***********************************************************
        
            pthread_cond_timedwait(             &m_evMsgs, 
                                                &m_mutex, 
                                                &ts                    );

            //***********************************************************
            //***********************************************************

        }

    }



    open = false;


    //************************************
    // Leaving critical 
    //************************************
    
    pthread_mutex_unlock(   &m_mutex    );

    //************************************
    //************************************



#elif defined(IB_WIN32)
	
    WaitForSingleObject(m_evMsgs, m_waitTimeout);

#else

#   error "Not implemented on this platform"

#endif

}

//******************************************************************************************
