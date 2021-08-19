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

//function returned successfully
#ifndef CHANNEL_ESUCCESS
	#error "channel.h requires user to implement CHANNEL_ESUCCESS int code"
#endif

//typedef and forward declaration
#define alias_channel(pfix)					  	       \
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
cls int pfix##_channel_init(pfix##_channel *self, size_t len);		       \
cls int pfix##_channel_free(pfix##_channel *self, void (*cfree) (T));	       \
cls int pfix##_channel_close(pfix##_channel *self);			       \
cls int pfix##_channel_send(pfix##_channel *self, T datum);		       \
cls int pfix##_channel_recv(pfix##_channel *self, T *datum);
