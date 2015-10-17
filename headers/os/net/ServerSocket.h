/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _SERVER_SOCKET_H
#define _SERVER_SOCKET_H

#include <BaseSocket.h>
#include <EventLoop.h>
#include <ServerProtocols.h>

#include <functional>

#include <assert.h>


namespace io {


class EventLoop;


typedef std::function<void (ssize_t)> IOCallback;


/*!
	\brief A socket for listening on an address and accepting connections.

	ServerSocket provides an interface for asynchronously accepting sockets.
	Each ServerSocket is associated with an EventLoop, which provides the
	mechanism for waiting on events.

	The semantics of the operations are the same as their BSD socket
	counterparts, except a callback is also provided, which will be called on
	completion.

	The callbacks are type-erased for storage, which may incur heap allocation
	if the callback object is sufficiently large. To avoid this, you can pass a
	std::ref or std::cref to your callback. If you do this, you must ensure the
	callback remains valid for the duration of the asynchronous operation.
*/
template<typename Protocol>
class ServerSocket : public BaseSocket<Protocol> {
public:
	typedef typename Protocol::SocketType SocketType;

	/*!
		Create a ServerSocket associated with the given event loop using the
		given protocol.
		This will open an underlying socket handle. A std::system_error will be
		thrown if this fails.
	*/
							ServerSocket(EventLoop& eventLoop,
								Protocol protocol);

	/*!
		Create a ServerSocket associated with the given event loop, and adopt
		the existing socket handle.
	*/
							ServerSocket(EventLoop& eventLoop, int socket);

	/*!
		Close the socket and cancel any pending asynchronous operations.
	*/
	virtual					~ServerSocket();

			int				Listen(int backlog);

	/*!
		Asynchronously accept a connection. The callback will be invoked with
		the result of the connection. If successful, the provided socket will
		refer to the accepted connection. Otherwise, it will not be modified.

		The provided socket must remain valid until the callback is called.
	*/
	template<typename Callback>
			void			AsyncAccept(SocketType& socket,
								Callback&& callback);
private:
			void			_HandleEvents(int events);
			void			_WaitForRead();

private:
			EventLoop&		fEventLoop;
			EventCallback	fEventCallback;

			SocketType*		fAcceptSocket;
			EventCallback	fAcceptCallback;
};


extern template class ServerSocket<TCPServerProtocol>;
extern template class ServerSocket<UNIXServerProtocol>;

typedef ServerSocket<TCPServerProtocol>		TCPServerSocket;
typedef ServerSocket<UNIXServerProtocol>	UNIXServerSocket;


template<typename Protocol>
inline
ServerSocket<Protocol>::ServerSocket(EventLoop& eventLoop,
	Protocol protocol)
	:
	BaseSocket<Protocol>(protocol),
	fEventLoop(eventLoop),
	fEventCallback(std::bind(&ServerSocket<Protocol>::_HandleEvents, this,
		std::placeholders::_1))
{
}


template<typename Protocol>
inline
ServerSocket<Protocol>::ServerSocket(EventLoop& eventLoop, int socket)
	:
	BaseSocket<Protocol>(socket),
	fEventLoop(eventLoop),
	fEventCallback(std::bind(&ServerSocket<Protocol>::_HandleEvents, this,
		std::placeholders::_1))
{
}


template<typename Protocol>
inline
ServerSocket<Protocol>::~ServerSocket()
{
}


template<typename Protocol>
inline int
ServerSocket<Protocol>::Listen(int backlog)
{
	int result = listen(this->fSocket, backlog);
	if (result != 0) {
		return errno;
	}
	return 0;
}


template<typename Protocol>
template<typename Callback>
inline void
ServerSocket<Protocol>::AsyncAccept(SocketType& socket, Callback&& callback)
{
	int result = accept(this->fSocket, nullptr, nullptr);
	if (result == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		callback(result);
	} else if (result >= 0) {
		socket.Adopt(result);
		callback(0);
	} else {
		fAcceptSocket = &socket;
		fAcceptCallback = callback;
		_WaitForRead();
	}
}


template<typename Protocol>
inline void
ServerSocket<Protocol>::_WaitForRead()
{
	status_t result = fEventLoop.WaitForFD(this->fSocket, B_EVENT_READ,
		fEventCallback);
	if (result != B_OK) {
		throw std::system_error(result, std::system_category());
	}
}


template<typename Protocol>
inline void
ServerSocket<Protocol>::_HandleEvents(int events)
{
	if ((events & B_EVENT_ERROR) != 0) {
		fAcceptCallback(this->Error());
		return;
	}

	assert((events & B_EVENT_READ) != 0);

	int result = accept(this->fSocket, nullptr, nullptr);

	if (result == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		fAcceptCallback(result);
	} else if (result >= 0) {
		fAcceptSocket->Adopt(result);
		fAcceptCallback(0);

		fAcceptSocket = nullptr;
		fAcceptCallback = nullptr;
	} else {
		_WaitForRead();
	}
}


}


#endif	// _SERVER_SOCKET_H
