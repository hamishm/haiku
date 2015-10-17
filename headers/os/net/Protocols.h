/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _PROTOCOLS_H
#define _PROTOCOLS_H

#include <NetworkAddress.h>


namespace io {


class TCPProtocol {
public:
	typedef BNetworkAddress	AddressType;

	int Family() const { return AF_INET; }
	int Type() const { return SOCK_STREAM; }
	int Protocol() const { return 0; }
};


class UNIXProtocol {
public:
	typedef BNetworkAddress	AddressType;

	int Family() const { return AF_UNIX; }
	int Type() const { return SOCK_STREAM; }
	int Protocol() const { return 0; }
};


}


#endif  // _PROTOCOLS_H
