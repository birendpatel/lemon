/*
 * @file main.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon compiler
 */

#include "lemon.h"

#include <stdio.h>
#include "assets/vector.h"

make_vector(uint8_t, u8, extern)

int main(void) {
	int err = 0;

	u8_vector v;

	err = u8_vector_init(&v, 8, 8);

	assert(err == LEMON_ESUCCESS);
	
	v.data[0] = 17;
	
	printf("len: %d, cap: %d\n", (int) v.len, (int) v.cap);
	err = u8_vector_push(&v, 42);

	assert(err == LEMON_ESUCCESS);
	printf("len: %d, cap: %d\n", (int) v.len, (int) v.cap);

	printf("%d\n", v.data[0]);
	printf("%d\n", v.data[8]);

	u8_vector_free(&v);

	return 0;
}
