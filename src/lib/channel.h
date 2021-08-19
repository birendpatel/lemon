/**
 * @file channel.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Thread-safe blocking FIFO queue.
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


//declare channel struct
#define declare_channel(T, pfix)					       \
struct pfix##_channel {
	pthread_mutex_t mutex;
	pthread_cond_t cond;

};
