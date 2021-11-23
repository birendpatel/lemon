// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Multi-producer multi-consumer thread-safe FIFO blocking queue with a fixed
// buffer length. 

#pragma once

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "xerror.h"

#ifdef CHANNEL_TRACE
	#define ChannelTrace(msg, ...) xerror_trace(msg, ##__VA_ARGS__)
#else
	#define ChannelTrace(msg, ...)
#endif

//channel functions may return any integer error code s.t. INT_MIN < x < INT_MAX
#ifndef CHANNEL_ESUCCESS
	#error "channel.h requires user to implement CHANNEL_ESUCCESS int code"
#endif

//one or more threads are waiting
#ifndef CHANNEL_EBUSY
	#error "channel.h requires user to implement CHANNEL_EBUSY int code"
#endif

//attempted to send on a closed channel or recv on a closed empty channel
#ifndef CHANNEL_ECLOSED
	#error "channel.h requires user to implement CHANNEL_ECLOSED int code"
#endif

#define alias_channel(pfix)						       \
typedef struct pfix##_channel pfix##_channel;

//the producer or consumer wishing to perform an action on the channel must
//first acquire the top-level mutex.
//
//senders wait on cond_full if the queue.len == queue.cap and consumer wait on 
//cond_empty if queue.len == 0.
//
//data is the internal buffer with maximum capacity queue.cap and current size
//queue.len. data[head] is the first element to be dequeued, data[tail] is
//the last element to be dequeued.
#define declare_channel(T, pfix)					       \
struct pfix##_channel {							       \
	pthread_mutex_t mutex;						       \
	pthread_cond_t cond_full;					       \
	pthread_cond_t cond_empty;					       \
	size_t len;							       \
	size_t cap;							       \
	size_t head;							       \
	size_t tail;							       \
	T *data;							       \
	unsigned char flags;						       \
};

//struct flags
#define CHANNEL_OPEN	1 << 0
#define CHANNEL_CLOSED	1 << 1

//API
//cls is the storage class and an optional inline directive
#define api_channel(T, pfix, cls)					       \
cls void pfix##ChannelInit(pfix##_channel *self, const size_t n);	       \
cls int pfix##ChannelShutdown(pfix##_channel *self);	                       \
cls void pfix##ChannelClose(pfix##_channel *self);			       \
cls int pfix##ChannelSend(pfix##_channel *self, const T datum);	               \
cls int pfix##ChannelRecv(pfix##_channel *self, T *datum);

//must be invoked before any other channel function
#define impl_channel_init(T, pfix, cls)					       \
cls void pfix##ChannelInit(pfix##_channel *self, const size_t n)	       \
{									       \
	assert(self);							       \
	assert(n);							       \
									       \
	self->data = ArenaAllocate(sizeof(T) * n);			       \
									       \
	self->cap = n;							       \
	self->len = 0;							       \
	self->head = 0;							       \
	self->tail = 0;							       \
									       \
	pthread_mutex_init(&self->mutex, NULL);				       \
	pthread_cond_init(&self->cond_full, NULL);			       \
	pthread_cond_init(&self->cond_empty, NULL);			       \
									       \
	self->flags = CHANNEL_OPEN;					       \
									       \
	ChannelTrace("initialized");		                               \
}

//cfree is invoked on each element in the channel before the channel is
//destroyed, unless cfree is NULL.
//
//If any thread are waiting for a signal, then CHANNEL_EBUSY is returned and
//the channel is not destroyed. Otherwise, CHANNEL_ESUCCESS is returned and the
//CHANNEL_CLOSED flag is set.
#define impl_channel_shutdown(T, pfix, cls)				       \
cls int pfix##ChannelShutdown(pfix##_channel *self)                    	       \
{									       \
	assert(self);							       \
									       \
	int err = CHANNEL_ESUCCESS;					       \
									       \
	pthread_mutex_lock(&self->mutex);				       \
									       \
	err = pthread_cond_destroy(&self->cond_full);			       \
									       \
	if (err) {							       \
		err = CHANNEL_EBUSY;					       \
		goto unlock;						       \
	}								       \
									       \
	err = pthread_cond_destroy(&self->cond_empty);			       \
									       \
	if (err) {							       \
		err = CHANNEL_EBUSY;					       \
		goto unlock;						       \
	}								       \
									       \
	self->flags = CHANNEL_CLOSED;					       \
							                       \
	ChannelTrace("cond variables removed");		                       \
									       \
unlock:									       \
	pthread_mutex_unlock(&self->mutex);				       \
	return err;							       \
}

//the CHANNEL_CLOSED flag does not imply that the channel is empty. It only
//indicates that the channel will no longer enqueue data from producers.
#define impl_channel_close(T, pfix, cls)				       \
cls void pfix##ChannelClose(pfix##_channel *self)			       \
{									       \
	assert(self);							       \
									       \
	pthread_mutex_lock(&self->mutex);				       \
									       \
	self->flags = CHANNEL_CLOSED;					       \
									       \
	ChannelTrace("closed");				                       \
									       \
	pthread_mutex_unlock(&self->mutex);				       \
}

//calling thread will suspend without timeout if the channel is full.
#define impl_channel_send(T, pfix, cls)					       \
cls int pfix##ChannelSend(pfix##_channel *self, const T datum)	               \
{									       \
	assert(self);							       \
									       \
	int err = CHANNEL_ESUCCESS;					       \
									       \
	pthread_mutex_lock(&self->mutex);				       \
									       \
	if (self->flags & CHANNEL_CLOSED) {				       \
		err = CHANNEL_ECLOSED;					       \
		ChannelTrace("attempted send on closed queue");                \
		goto unlock;						       \
	}								       \
									       \
	while (self->len == self->cap) {				       \
		ChannelTrace("full; suspending thread");	               \
		pthread_cond_wait(&self->cond_full, &self->mutex);	       \
	}								       \
									       \
	ChannelTrace("sending data");			                       \
	self->data[self->tail] = datum;					       \
	self->tail = (self->tail + 1) % self->cap;			       \
	self->len++;							       \
									       \
	if (self->len == 1) {						       \
		ChannelTrace("signal; now non-empty");	                       \
		pthread_cond_signal(&self->cond_empty);			       \
	}								       \
									       \
unlock:									       \
	pthread_mutex_unlock(&self->mutex);				       \
	return err;							       \
}

//calling thread will suspend without timeout if the channel is empty.
#define impl_channel_recv(T, pfix, cls)					       \
cls int pfix##ChannelRecv(pfix##_channel *self, T *datum)		       \
{									       \
	assert(self);						               \
	assert(datum);							       \
									       \
	int err = CHANNEL_ESUCCESS;					       \
									       \
	pthread_mutex_lock(&self->mutex);				       \
									       \
	if (self->flags & CHANNEL_CLOSED && self->len == 0) {		       \
		err = CHANNEL_ECLOSED;					       \
		ChannelTrace("recv fail; closed empty queue");                 \
		goto unlock;						       \
	}							               \
									       \
	while (self->len == 0) {					       \
		ChannelTrace("empty; suspending thread");	               \
		pthread_cond_wait(&self->cond_empty, &self->mutex);	       \
	}								       \
									       \
	ChannelTrace("receiving data");				       	       \
	*datum = self->data[self->head];			               \
	self->head = (self->head + 1) % self->cap;			       \
	self->len--;							       \
									       \
	if (self->len == self->cap - 1) {				       \
		ChannelTrace("signal; now non-full");	                       \
		pthread_cond_signal(&self->cond_full);			       \
	}								       \
									       \
unlock:									       \
	pthread_mutex_unlock(&self->mutex);				       \
	return err;							       \
}

//create a generic channel named pfix_channel which contains elements of type T
//and calls methods with storage class cls.
#define make_channel(T, pfix, cls)					       \
	alias_channel(pfix)						       \
	declare_channel(T, pfix)					       \
	api_channel(T, pfix, cls)					       \
	impl_channel_init(T, pfix, cls)					       \
	impl_channel_shutdown(T, pfix, cls)				       \
	impl_channel_close(T, pfix, cls)				       \
	impl_channel_send(T, pfix, cls)					       \
	impl_channel_recv(T, pfix, cls)

#define channel(pfix) pfix##_channel
