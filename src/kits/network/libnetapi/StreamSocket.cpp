/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <Protocols.h>
#include <StreamSocket.h>


namespace io {


template class StreamSocket<TCPProtocol>;
template class StreamSocket<UNIXProtocol>;


}

