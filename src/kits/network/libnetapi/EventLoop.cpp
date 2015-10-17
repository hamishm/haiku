/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <EventLoop.h>

#include <OS.h>

#include <algorithm>
#include <system_error>


namespace io {


EventLoop::EventLoop()
{
	fEventQueue = event_queue_create(O_CLOEXEC);
	if (fEventQueue < 0) {
		throw std::system_error(fEventQueue, std::system_category());
	}
}


EventLoop::~EventLoop()
{
	close(fEventQueue);
}


void
EventLoop::_DispatchWork()
{
	for (auto const& work : fWorkQueue) {
		work();
	}
	fWorkQueue.clear();
}


void
EventLoop::_DispatchTimers()
{
	bigtime_t current = real_time_clock_usecs();

	while (!fTimers.empty()) {
		if (fTimers[0].expiration > current)
			break;

		fTimers[0].function();

		std::pop_heap(fTimers.begin(), fTimers.end());
		fTimers.pop_back();
	}
}


bigtime_t
EventLoop::_DetermineTimeout()
{
	if (fTimers.empty())
		return B_INFINITE_TIMEOUT;

	return fTimers[0].expiration;
}


status_t
EventLoop::RunOnce()
{
	static const int EVENTS_TO_READ = 50;

	_DispatchWork();
	_DispatchTimers();

	bigtime_t timeout = _DetermineTimeout();

	event_wait_info infos[EVENTS_TO_READ];

	ssize_t result = event_queue_wait(fEventQueue, infos, EVENTS_TO_READ,
		B_ABSOLUTE_REAL_TIME_TIMEOUT, timeout);

	if (result < B_OK)
		return result;

	for (ssize_t i = 0; i < result; i++) {
		int32 events = infos[i].events;

		EventCallback* wrapper = (EventCallback*)infos[i].user_data;
		(*wrapper)(events);
	}

	return result;
}


status_t
EventLoop::_WaitForObject(int32 object, uint16 type, uint16 events,
	EventCallback& callback, bool oneShot)
{
	event_wait_info info;
	info.object = object;
	info.type = type;
	info.events = events | B_EVENT_SELECT | (oneShot ? B_EVENT_ONE_SHOT : 0);
	info.user_data = (void*)&callback;

	status_t result = event_queue_select(fEventQueue, &info, 1);

	// Somewhat ugly: if the error is 'B_ERROR' then the error is stored in the
	// events field of the event_wait_info.
	if (result == B_ERROR)
		return info.events;
	else if (result != B_OK)
		return result;

	return B_OK;
}


void
EventLoop::_ExecuteAt(Function&& function, bigtime_t time)
{
	fTimers.push_back({ time, function });
	std::push_heap(fTimers.begin(), fTimers.end());
}


}
