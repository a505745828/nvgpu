/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __NVGPU_POSIX_COND_H__
#define __NVGPU_POSIX_COND_H__

#include <nvgpu/bug.h>
#include <nvgpu/lock.h>
#include <nvgpu/posix/thread.h>

struct nvgpu_cond {
	bool initialized;
	struct nvgpu_mutex mutex;
	pthread_cond_t cond;
	pthread_condattr_t attr;
};

/**
 * NVGPU_COND_WAIT - Wait for a condition to be true
 *
 * @c - The condition variable to sleep on
 * @condition - The condition that needs to be true
 * @timeout_ms - Timeout in milliseconds, or 0 for infinite wait
 *
 * Wait for a condition to become true. Returns -ETIMEOUT if
 * the wait timed out with condition false.
 */
#define NVGPU_COND_WAIT(c, condition, timeout_ms)			\
({									\
	int ret = 0;							\
	struct timespec ts;						\
	nvgpu_mutex_acquire(&(c)->mutex);				\
	if (timeout_ms == 0) {						\
		ret = pthread_cond_wait(&(c)->cond,			\
			&(c)->mutex.lock.mutex);			\
	} else {							\
		clock_gettime(CLOCK_REALTIME, &ts);			\
		ts.tv_sec += timeout_ms / 1000;				\
		ts.tv_nsec += (timeout_ms % 1000) * 1000000;		\
		if (ts.tv_nsec >= 1000000000) {				\
			ts.tv_sec += 1;					\
			ts.tv_nsec %= 1000000000;			\
		}							\
		while (!(condition) && ret == 0) {			\
			ret = pthread_cond_timedwait(&(c)->cond,	\
				&(c)->mutex.lock.mutex, &ts);		\
		}							\
	}								\
	nvgpu_mutex_release(&(c)->mutex);				\
	ret; 								\
})

/**
 * NVGPU_COND_WAIT_INTERRUPTIBLE - Wait for a condition to be true
 *
 * @c - The condition variable to sleep on
 * @condition - The condition that needs to be true
 * @timeout_ms - Timeout in milliseconds, or 0 for infinite wait
 *
 * Wait for a condition to become true. Returns -ETIMEOUT if
 * the wait timed out with condition false or -ERESTARTSYS on
 * signal.
 */
#define NVGPU_COND_WAIT_INTERRUPTIBLE(c, condition, timeout_ms) \
				NVGPU_COND_WAIT(c, condition, timeout_ms)

#endif
