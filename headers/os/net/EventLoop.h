/*
 * Copyright 2015, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <OS.h>

#include <deque>
#include <functional>
#include <utility>
#include <vector>


namespace io {


typedef std::function<void (int)> EventCallback;


class EventLoop {
public:
						EventLoop();
	virtual				~EventLoop();

	status_t			RunOnce();

	status_t			WaitForFD(int fd, int events, EventCallback& callback,
							bool oneShot = true);

	status_t			WaitForPort(port_id port, int events,
							EventCallback& callback, bool oneShot = true);

	status_t			WaitForSemaphore(sem_id semaphore, int events,
							EventCallback& callback, bool oneShot = true);

	status_t			WaitForThread(thread_id thread, int events,
							EventCallback& callback, bool oneShot = true);

	template<typename F>
	void				ExecuteLater(F&& function);

	template<typename F>
	void				ExecuteAt(F&& function, bigtime_t time);

private:
	typedef std::function<void ()> Function;

	status_t			_WaitForObject(int32 object, uint16 type, uint16 events,
							EventCallback& callback, bool oneShot = true);

	void				_ExecuteAt(Function&& fn, bigtime_t time);

	bigtime_t			_DetermineTimeout();
	void				_DispatchTimers();
	void				_DispatchWork();

private:
	struct timer {
		bigtime_t	expiration;
		Function	function;
		bool operator<(const timer& other) const
		{
			return expiration < other.expiration;
		}
	};

	struct timer_comparator;

	std::deque<Function>	fWorkQueue;
	std::vector<timer>		fTimers;
	int						fEventQueue;
};


template<typename F>
inline void
EventLoop::ExecuteLater(F&& function)
{
	fWorkQueue.emplace_back(std::forward<F>(function));

	// TODO: should wake up event loop
}


template<typename F>
inline void
EventLoop::ExecuteAt(F&& function, bigtime_t time)
{
	Function fn{std::forward<F>(function)};
	_ExecuteAt(std::move(fn), time);
}


inline status_t
EventLoop::WaitForFD(int fd, int events, EventCallback& callback, bool oneShot)
{
	return _WaitForObject(fd, B_OBJECT_TYPE_FD, events, callback, oneShot);

}


inline status_t
EventLoop::WaitForPort(port_id port, int events, EventCallback& callback,
	bool oneShot)
{
	return _WaitForObject(port, B_OBJECT_TYPE_PORT, events, callback, oneShot);
}


inline status_t
EventLoop::WaitForSemaphore(sem_id semaphore, int events,
	EventCallback& callback, bool oneShot)
{
	return _WaitForObject(semaphore, B_OBJECT_TYPE_SEMAPHORE, events, callback,
		oneShot);
}


inline status_t
EventLoop::WaitForThread(thread_id thread, int events, EventCallback& callback,
	bool oneShot)
{
	return _WaitForObject(thread, B_OBJECT_TYPE_THREAD, events, callback,
		oneShot);
}


}


#endif	// _EVENT_LOOP_H
