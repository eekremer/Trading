/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#include "StdAfx.h"
#include "EMessage.h"


//***************************************************************************************

EMessage::EMessage(  const std::vector< char > &data  ) 
{

    this->data = data;

}

//***************************************************************************************

const char* EMessage::begin( void ) const
{

    /*

        value_type*         data() noexcept;
        const value_type*   data() const noexcept;

        Access data
        Returns a direct pointer to the memory array used internally by the vector to store 
        its owned elements.

        Because elements in the vector are guaranteed to be stored in contiguous storage 
        locations in the same order as represented by the vector, the pointer retrieved 
        can be offset to access any element in the array.

        Return value
        A pointer to the first element in the array used internally by the vector.

    */


    return data.data();

}

//***************************************************************************************

const char* EMessage::end( void ) const
{

    return data.data() + data.size();

}

//***************************************************************************************