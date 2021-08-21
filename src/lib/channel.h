/**
 * @file channel.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 *
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
#include <stdbool.h>
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

//one or more threads are waiting
#ifndef CHANNEL_EBUSY
	#error "channel.h requires user to implement CHANNEL_EBUSY int code"
#endif

//attempted to send on a closed channel or recv on a closed empty channel
#ifndef CHANNEL_ECLOSED
	#error "channel.h requires user to implement CHANNEL_ECLOSED int code"
#endif

//struct flags
#define CHANNEL_OPEN	1 << 0
#define CHANNEL_CLOSED	1 << 1

//typedef and forward declaration
#define alias_channel(pfix)						       \
typedef struct pfix##_channel pfix##_channel;

//declares a channel struct with a pfixed tag and elements of type T
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

//prototypes
//cls is the storage class and an optional inline directive
#define api_channel(T, pfix, cls)					       \
cls int pfix##_channel_init(pfix##_channel *self, const size_t n);	       \
cls int pfix##_channel_free(pfix##_channel *self, void (*cfree) (T));	       \
cls void pfix##_channel_close(pfix##_channel *self);			       \
cls int pfix##_channel_send(pfix##_channel *self, const T datum);	       \
cls int pfix##_channel_recv(pfix##_channel *self, T *datum);

/*******************************************************************************
 * @def impl_channel_init
 * @brief Initialize a channel with a fixed capacity n > 0.
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
	return CHANNEL_ESUCCESS;					       \
}

/*******************************************************************************
 * @def impl_channel_free
 * @brief Destroy a channel and release system resources.
 * @param cfree Invoked on each element in the channel unless NULL.
 * @retruns If any threads are waiting for a signal, then CHANNEL_EBUSY is
 * returned and the channel is not destroyed. Otherwise, CHANNEL_ESUCCESS is
 * returned and the CHANNEL_CLOSED flag is set.
 ******************************************************************************/
#define impl_channel_free(T, pfix, cls)					       \
cls int pfix##_channel_free(pfix##_channel *self, void (*cfree) (T))	       \
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
	if (cfree) {							       \
		for (size_t i = self->head; i < self->len; i++) {	       \
			cfree(self->data[i]);				       \
		}							       \
	}								       \
									       \
	free(self->data);						       \
	memset(self, 0, sizeof(struct pfix##_channel));			       \
	self->flags = CHANNEL_CLOSED;					       \
									       \
unlock:									       \
	pthread_mutex_unlock(&self->mutex);				       \
	return err;							       \
}

/*******************************************************************************
 * @def impl_channel_close
 * @brief Set a flag on the channel which indicates that it will no longer
 * receive data from producers. This does not imply that the channel is empty.
 ******************************************************************************/
#define impl_channel_close(T, pfix, cls)				       \
cls void pfix##_channel_close(pfix##_channel *self)			       \
{									       \
	assert(self);							       \
									       \
	pthread_mutex_lock(&self->mutex);				       \
									       \
	self->flags = CHANNEL_CLOSED;					       \
									       \
	pthread_mutex_unlock(&self->mutex);				       \
}

/*******************************************************************************
 * @def impl_channel_send
 * @brief Place an item at the back of the channel.
 * @details Thread will suspend without timeout if the channel is full.
 ******************************************************************************/
#define impl_channel_send(T, pfix, cls)					       \
cls int pfix##_channel_send(pfix##_channel *self, const T datum)	       \
{									       \
	assert(self);							       \
									       \
	int err = CHANNEL_ESUCCESS;					       \
									       \
	pthread_mutex_lock(&self->mutex);				       \
									       \
	if (self->flags & CHANNEL_CLOSED) {				       \
		err = CHANNEL_ECLOSED;					       \
		goto unlock;						       \
	}								       \
									       \
	while (self->len == self->cap) {				       \
		pthread_cond_wait(&self->cond_full, &self->mutex);	       \
	}								       \
									       \
	self->data[self->tail] = datum;					       \
	self->tail = (self->tail + 1) % self->cap;			       \
	self->len++;							       \
									       \
	if (self->len == 1) {						       \
		pthread_cond_signal(&self->cond_empty);			       \
	}								       \
									       \
unlock:									       \
	pthread_mutex_unlock(&self->mutex);				       \
	return err;							       \
}

/*******************************************************************************
 * @def impl_channel_recv
 * @brief Receive an item from the front of the channel.
 * @details Thread will suspend without timeout if the channel is empty.
 ******************************************************************************/
#define impl_channel_recv(T, pfix, cls)					       \
cls int pfix##_channel_recv(pfix##_channel *self, T *datum)		       \
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
		goto unlock;						       \
	}							               \
									       \
	while (self->len == 0) {					       \
		pthread_cond_wait(&self->cond_empty, &self->mutex);	       \
	}								       \
									       \
	*datum = self->data[self->head];			               \
	self->head = (self->head + 1) % self->cap;			       \
	self->len--;							       \
									       \
	if (self->len == self->cap - 1) {				       \
		pthread_cond_signal(&self->cond_full);			       \
	}								       \
									       \
unlock:									       \
	pthread_mutex_unlock(&self->mutex);				       \
	return err;							       \
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
