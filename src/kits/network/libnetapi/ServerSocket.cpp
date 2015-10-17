/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All Rights Reserved. Distributed under the terms of the MIT License.
 */

#include <ServerProtocols.h>
#include <ServerSocket.h>


namespace io {


template class ServerSocket<TCPServerProtocol>;
template class ServerSocket<UNIXServerProtocol>;


}
