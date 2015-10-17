/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _SERVER_PROTOCOLS_H
#define _SERVER_PROTOCOLS_H

#include <NetworkAddress.h>
#include <StreamSocket.h>


namespace io {


class TCPServerProtocol {
public:
	typedef BNetworkAddress	AddressType;
	typedef TCPSocket		SocketType;

	int Family() const { return AF_INET; }
	int Type() const { return SOCK_STREAM; }
	int Protocol() const { return 0; }
};


class UNIXServerProtocol {
public:
	typedef BNetworkAddress	AddressType;
	typedef UNIXSocket		SocketType;

	int Family() const { return AF_UNIX; }
	int Type() const { return SOCK_STREAM; }
	int Protocol() const { return 0; }
};


}


#endif  // _SERVER_PROTOCOLS_H
