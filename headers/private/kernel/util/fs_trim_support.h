/*
 * Copyright 2013, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT license.
 */
#ifndef _FS_TRIM_SUPPORT_H
#define _FS_TRIM_SUPPORT_H


#include <Drivers.h>

#include <kernel.h>


static inline status_t
copy_trim_data_from_user(void* buffer, size_t size, fs_trim_data*& _trimData)
{
	if (!IS_USER_ADDRESS(buffer))
		return B_BAD_ADDRESS;

	uint32 count;
	if (user_memcpy(&count, buffer, sizeof(count)) != B_OK)
		return B_BAD_ADDRESS;

	size_t bytes = (count - 1) * sizeof(uint64) * 2 + sizeof(fs_trim_data);
	if (bytes > size)
		return B_BAD_VALUE;

	void* trimBuffer = malloc(bytes);
	if (trimBuffer == NULL)
		return B_NO_MEMORY;

	if (user_memcpy(trimBuffer, buffer, bytes) != B_OK)
		return B_BAD_ADDRESS;

	_trimData = (fs_trim_data*)trimBuffer;
	return B_OK;
}


static inline status_t
copy_trim_data_to_user(void* buffer, fs_trim_data* trimData)
{
	// Do not copy any ranges
	return user_memcpy(buffer, trimData, sizeof(uint64) * 2);
}


#endif	// _FS_TRIM_SUPPORT_H