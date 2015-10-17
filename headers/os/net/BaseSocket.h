/*
 * Copyright 2015, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BASE_SOCKET_H
#define _BASE_SOCKET_H

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <system_error>


namespace io {


/*!
	\brief Base class for sockets.

	This provides the socket operations common to all socket types. The
	semantics of the operations are similar to their POSIX counterparts.

	All functions return 0 on success, or a POSIX error number otherwise.

	The Protocol template parameter must provide AddressType and SocketType
	typedefs suitable for the concrete socket type, along with methods
	Family(), Type() and Protocol() to provide parameters for the open() call.
*/
template<typename Protocol>
class BaseSocket {
public:
	typedef typename Protocol::AddressType	AddressType;

	/*!
		Create an unopened socket.
	*/
					BaseSocket();

	/*!
		Create and open a socket using the provided protocol.
	*/
	explicit		BaseSocket(Protocol protocol);

	/*!
		Create a socket, adopting the provided socket descriptor.
	*/
	explicit		BaseSocket(int socket);

	/*!
		Destroy the socket, closing the underlying descriptor if one is
		open.
	*/
	virtual			~BaseSocket();

	/*!
		Adopt the provided socket descriptor. The behaviour is undefined if
		an invalid or non-socket file descriptor is passed.
	*/
			void	Adopt(int socket);
	/*!
		Open a socket descriptor for the given protocol.
	*/

			int		Open(Protocol protocol);
	/*!
		Close the underlying socket descriptor.
	*/
			int		Close();

	/*!
		Bind the socket to the given address.
	*/
			int		Bind(const AddressType& address);

	/*!
		Set or unset non-blocking mode on the socket.
	*/
			int		SetNonBlocking(bool nonBlocking);

	/*!
		Get the error of the most recent socket operation. The error code
		will be reset after this call.
	*/
			int		Error();

protected:
			int		fSocket;
};


template<typename Protocol>
inline
BaseSocket<Protocol>::BaseSocket()
	:
	fSocket(-1)
{
}


template<typename Protocol>
inline
BaseSocket<Protocol>::BaseSocket(Protocol protocol)
{
	int result = Open(protocol);
	if (result != 0) {
		throw std::system_error(result, std::system_category());
	}
}


template<typename Protocol>
inline
BaseSocket<Protocol>::BaseSocket(int socket)
	:
	fSocket(socket)
{
}


template<typename Protocol>
BaseSocket<Protocol>::~BaseSocket()
{
	if (fSocket != -1) {
		Close();
	}
}


template<typename Protocol>
inline void
BaseSocket<Protocol>::Adopt(int socket)
{
	fSocket = socket;
}


template<typename Protocol>
inline int
BaseSocket<Protocol>::Open(Protocol protocol)
{
	fSocket = socket(protocol.Family(), protocol.Type(), protocol.Protocol());
	if (fSocket == -1) {
		return errno;
	}
	return 0;
}


template<typename Protocol>
inline int
BaseSocket<Protocol>::Close()
{
	int result = close(fSocket);
	if (result != 0) {
		return errno;
	}
	return 0;
}


template<typename Protocol>
inline int
BaseSocket<Protocol>::SetNonBlocking(bool nonBlocking)
{
	int option = nonBlocking;
	int result = ioctl(fSocket, FIONBIO, &option);
	if (result != 0) {
		return errno;
	}
	return 0;
}


template<typename Protocol>
inline int
BaseSocket<Protocol>::Bind(const AddressType& address)
{
	int result = bind(fSocket, &address.SockAddr(), address.Length());
	if (result == -1) {
		return errno;
	}
	return 0;
}


template<typename Protocol>
inline int
BaseSocket<Protocol>::Error()
{
	int error;
	socklen_t length = sizeof(error);
	int result = getsockopt(fSocket, SOL_SOCKET, SO_ERROR, &error, &length);

	if (result == 0) {
		return error;
	}
	return errno;
}


}


#endif	// _BASE_SOCKET_H
