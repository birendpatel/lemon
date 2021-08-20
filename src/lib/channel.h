/**
 * @file channel.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief This data structure is a simple multi-producer multi-consumer FIFO
 * thread-safe queue implemented via posix threads. The queue is blocking and
 * its buffer size is fixed to enable the producers to place back-pressure
 * on the system whenever the consumers begin to outpace demand.
 *
 * @details Like the vector template, the user must configure macro definitions
 * for the integer error codes. Other than that, the file is plug and play
 * with no dependencies.
 */

#pragma once

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//function returned successfully
#ifndef CHANNEL_ESUCCESS
	#error "channel.h requires user to implement CHANNEL_ESUCCESS int code"
#endif

//cannot allocate memory
#ifndef CHANNEL_ENOMEM
	#error "channel.h requires user to implement CHANNEL_ENOMEM int code"
#endif

//cannot free channel, threads are waiting
#ifndef CHANNEL_EBUSY
	#error "channel.h requires user to implement CHANNEL_EBUSY int code"
#endif

//cannot free nonempty channel without permission
#ifndef CHANNEL_EFULL
	#error "channel.h requires user to implement CHANNEL_EFULL int code"
#endif

//struct flags
#define CHANNEL_OPEN	1 << 0
#define CHANNEL_CLOSED 	1 << 1

//typedef and forward declaration
#define alias_channel(pfix)						       \
typdef struct pfix##_channel pfix##_channel;

//declares a channel struct with a pfixed tag and elements of type T
#define declare_channel(T, pfix)					       \
struct pfix##_channel {							       \
	pthread_mutex_t mutex;						       \
	pthread_cond_t cond;						       \
	size_t len;							       \
	size_t head;							       \
	size_t tail;							       \
	T *data;							       \
	unsigned char flags;						       \
};

//prototypes
//cls is the storage class and an optional inline directive
#define api_channel(T, pfix, cls)					       \
cls int pfix##_channel_init(pfix##_channel *self, const size_t n);	       \
cls int pfix##_channel_free(pfix##_channel *self, void (*cfree) (T), bool f);  \
cls int pfix##_channel_close(pfix##_channel *self);			       \
cls int pfix##_channel_send(pfix##_channel *self, const T datum);	       \
cls int pfix##_channel_recv(pfix##_channel *self, T *datum);

/*******************************************************************************
 * @def impl_channel_init
 * @brief Initialize a channel with a fixed length n > 0.
 * @details This function must be invoked before any other channel functions.
 ******************************************************************************/
#define impl_channel_init(T, pfix, cls)					       \
cls int pfix##_channel_init(pfix##_channel *self, const size_t n)	       \
{									       \
	assert(self);							       \
	assert(n);							       \
									       \
	self->data = malloc(sizeof(T) * n);				       \
									       \
	if (!self->data) {						       \
		return CHANNEL_ENOMEM;					       \
	}								       \
									       \
	self->len = n;							       \
	self->head = 0;							       \
	self->tail = 0;							       \
									       \
	pthread_mutex_init(&self->mutex, NULL);				       \
	pthread_cond_init(&self->cond, NULL);				       \
									       \
	flags = CHANNEL_OPEN;						       \
									       \
	return CHANNEL_ESUCCESS;					       \
}

/*******************************************************************************
 * @def impl_channel_free
 * @brief Destroy a channel, destroy its mutex and condition variables, and
 * release any system resources. 
 * @param f Force flag; if false then channel will not be freed if there are
 * elements currently in the channel.
 * @returns If any threads are waiting ofr a broadcast, then the function fails
 * and returns CHANNEL_EBUSY. If the force flag is set and the channel is non-
 * empty, the function returns CHANNEL_EFULL. In both situations, the channel
 * will remain operable. Otherwise, CHANNEL_ESUCCESS is returns and the flag
 * CHANNEL_CLOSED will be set.
 ******************************************************************************/
#define impl_channel_free(T, pfix, cls)					       \
cls int pfix##_channel_free(pfix##_channel *self, void (*cfree) (T), bool f)   \
{
	assert(self);

	int err = CHANNEL_ESUCCESS;

	pthread_mutex_lock(&self->mutex);

	if (f && self->len > 0) {
		err = CHANNEL_EFULL;
		goto unlock;
	}

	err = pthread_cond_destroy(&self->cond);

	if (err) {
		err = CHANNEL_EBUSY;
		goto unlock;
	}

	if (cfree) {
		for (size_t i = self->head; i < self->len; i++) {
			cfree(self->data[i]);
		}
	}

	free(self->data);
	memset(self, 0, sizeof(struct pfix##_channel));
	self->flags = CHANNEL_CLOSED;

unlock:
	pthread_mutex_unlock(&self->mutex);
	return err;
}


/*******************************************************************************
 * @def make_channel
 * @brief Create a generic channel named pfix_channel which contains elements
 * of type T and calls methods with storage class cls.
 ******************************************************************************/
#define make_channel(T, pfix, cls)					       \
	alias_channel(pfix)						       \
	declare_channel(T, pfix)					       \
	api_channel(T, pfix, cls)					       \
	impl_channel_init(T, pfix, cls)					       \
	impl_channel_free(T, pfix, cls)					       \
	impl_channel_close(T, pfix, cls)				       \
	impl_channel_send(T, pfix, cls)					       \
	impl_channel_recv(T, pfix, cls)
