/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _STREAM_SOCKET_H
#define _STREAM_SOCKET_H

#include <BaseSocket.h>
#include <EventLoop.h>
#include <Protocols.h>

#include <sys/socket.h>

#include <functional>


namespace io {


class EventLoop;


typedef std::function<void (ssize_t)> IOCallback;


/*!
	\brief Interface for executing asynchronous socket operations.

	AsyncStreamSocket provides an interface for asynchronous socket I/O on
	stream sockets. Each AsyncStreamSocket is associated with an EventLoop,
	which provides the mechanism for waiting on events.

	The semantics of the operations are the same as their BSD socket
	counterparts, except a callback is also provided, which will be called on
	completion.

	The callbacks are type-erased for storage, which may incur heap allocation
	if the callback object is sufficiently large. To avoid this, you can pass a
	std::ref or std::cref to your callback. If you do this, you must ensure the
	callback remains valid for the duration of the asynchronous operation.
*/
template<typename Protocol>
class StreamSocket : public BaseSocket<Protocol> {
public:
	typedef typename Protocol::AddressType AddressType;

	/*!
		Create an unopened StreamSocket associated with the given event loop.
	*/
							StreamSocket(EventLoop& eventLoop);

	/*!
		Create a StreamSocket associated with the given event loop. This will
		open an underlying socket handle. A std::system_error will be thrown if
		this fails.
	*/
							StreamSocket(EventLoop& eventLoop,
									Protocol protocol);

	/*!
		Create a StreamSocket associated with the given event loop, and adopt
		the existing socket handle.
	*/
							StreamSocket(EventLoop& eventLoop, int socket);

	/*!
		Close the socket and cancel any pending asynchronous operations.
	*/
	virtual					~StreamSocket();

	/*!
		Connect the socket to the given peer. The callback will be invoked with
		the result of the operation. The callback may be invoked before the
		method returns.
	*/
	template<typename Callback>
			void			AsyncConnect(const AddressType& peer,
								Callback&& callback);

	/*!
		Receive into the provided buffer. The callback will be invoked with the
		result of the receive. The callback may be invoked before the method
		returns.
	*/
	template<typename Callback>
			void			AsyncRecv(void* buffer, size_t size, int flags,
								Callback&& callback);

	/*!
		Send up to size bytes from the buffer. The callback will be invoked
		with the result of the send. The callback may be invoked before the
		method returns.
	*/
	template<typename Callback>
			void			AsyncSend(const void* buffer, size_t size,
								int flags, Callback&& callback);

private:
			void			_HandleEvents(int events);
			void			_HandleConnect();
			void			_HandleSend();
			void			_HandleRecv();

			void			_WaitForRead();
			void			_WaitForWrite();

private:
			EventLoop&		fEventLoop;
			EventCallback	fEventCallback;

			struct IORequest {
				void*		buffer;
				size_t		size;
				int			flags;
				IOCallback	callback;
			};

			IORequest		fSendRequest;
			IORequest		fRecvRequest;
			IOCallback		fConnectCallback;

			bool			fWaitingRead : 1;
			bool			fWaitingWrite : 1;
			bool			fWaitingConnect : 1;
};

extern template class StreamSocket<TCPProtocol>;
extern template class StreamSocket<UNIXProtocol>;

typedef StreamSocket<TCPProtocol>	TCPSocket;
typedef StreamSocket<UNIXProtocol>	UNIXSocket;


template<typename Protocol>
inline
StreamSocket<Protocol>::StreamSocket(EventLoop& eventLoop)
	:
	fEventLoop(eventLoop),
	fEventCallback(std::bind(&StreamSocket<Protocol>::_HandleEvents, this,
		std::placeholders::_1)),
	fWaitingRead(false),
	fWaitingWrite(false),
	fWaitingConnect(false)
{
}


template<typename Protocol>
inline
StreamSocket<Protocol>::StreamSocket(EventLoop& eventLoop,
	Protocol protocol)
	:
	BaseSocket<Protocol>(protocol),
	fEventLoop(eventLoop),
	fEventCallback(std::bind(&StreamSocket<Protocol>::_HandleEvents, this,
		std::placeholders::_1)),
	fWaitingRead(false),
	fWaitingWrite(false),
	fWaitingConnect(false)
{
}


template<typename Protocol>
inline
StreamSocket<Protocol>::StreamSocket(EventLoop& eventLoop, int socket)
	:
	BaseSocket<Protocol>(socket),
	fEventLoop(eventLoop),
	fEventCallback(std::bind(&StreamSocket<Protocol>::_HandleEvents, this,
		std::placeholders::_1)),
	fWaitingRead(false),
	fWaitingWrite(false),
	fWaitingConnect(false)
{
}


template<typename Protocol>
inline
StreamSocket<Protocol>::~StreamSocket()
{
}


template<typename Protocol>
template<typename Callback>
inline void
StreamSocket<Protocol>::AsyncConnect(const AddressType& address,
	Callback&& callback)
{
	int result = connect(this->fSocket, &address.SockAddr(), address.Length());

	if (result == 0 || errno != EINPROGRESS) {
		callback(result == 0 ? 0 : errno);
	} else {
		fWaitingConnect = true;
		fConnectCallback = callback;
		_WaitForWrite();
	}
}


template<typename Protocol>
template<typename Callback>
inline void
StreamSocket<Protocol>::AsyncRecv(void* buffer, size_t size, int flags,
	Callback&& callback)
{
	ssize_t received = recv(this->fSocket, buffer, size, flags);

	if (received >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
		callback(received >= 0 ? received : errno);
	} else {
		fRecvRequest.buffer = buffer;
		fRecvRequest.size = size;
		fRecvRequest.flags = flags;
		fRecvRequest.callback = callback;
		fWaitingRead = true;
		_WaitForRead();
	}
}


template<typename Protocol>
template<typename Callback>
inline void
StreamSocket<Protocol>::AsyncSend(const void* buffer, size_t size, int flags,
	Callback&& callback)
{
	ssize_t sent = send(this->fSocket, buffer, size, flags);

	if (sent >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
		callback(sent >= 0 ? sent : errno);
	} else {
		fSendRequest.buffer = const_cast<void*>(buffer);
		fSendRequest.size = size;
		fSendRequest.flags = flags;
		fSendRequest.callback = callback;
		fWaitingWrite = true;
		_WaitForWrite();
	}
}


template<typename Protocol>
void
StreamSocket<Protocol>::_WaitForRead()
{
	status_t result = fEventLoop.WaitForFD(this->fSocket, B_EVENT_READ,
		fEventCallback);
	if (result != B_OK) {
		throw std::system_error(result, std::system_category());
	}
}


template<typename Protocol>
void
StreamSocket<Protocol>::_WaitForWrite()
{
	status_t result = fEventLoop.WaitForFD(this->fSocket, B_EVENT_WRITE,
		fEventCallback);
	if (result != B_OK) {
		throw std::system_error(result, std::system_category());
	}
}


template<typename Protocol>
void
StreamSocket<Protocol>::_HandleEvents(int events)
{
	if ((events & B_EVENT_READ) != 0 && fWaitingRead)
		_HandleRecv();

	if ((events & B_EVENT_WRITE) != 0) {
		if (fWaitingConnect)
			_HandleConnect();
		if (fWaitingWrite)
			_HandleSend();
	}
}


template<typename Protocol>
void
StreamSocket<Protocol>::_HandleRecv()
{
	ssize_t received = recv(this->fSocket, fRecvRequest.buffer,
		fRecvRequest.size, fRecvRequest.flags);

	if (received >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
		fWaitingRead = false;
		fRecvRequest.callback(received >= 0 ? received : errno);
		fRecvRequest.callback = nullptr;
	} else {
		_WaitForRead();
	}
}


template<typename Protocol>
void
StreamSocket<Protocol>::_HandleSend()
{
	ssize_t sent = send(this->fSocket, fRecvRequest.buffer, fRecvRequest.size,
		fRecvRequest.flags);

	if (sent >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
		fWaitingWrite = false;
		fSendRequest.callback(sent >= 0 ? sent : errno);
		fSendRequest.callback = nullptr;
	} else {
		_WaitForWrite();
	}
}


template<typename Protocol>
void
StreamSocket<Protocol>::_HandleConnect()
{
	fWaitingConnect = false;
	fConnectCallback(0);
	fConnectCallback = nullptr;
}


}


#endif	// _STREAM_SOCKET_H
