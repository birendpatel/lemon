/*
 * @file main.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon compiler
 */

#include "lemon.h"

#include "assets/vector.h"

make_vector(uint8_t, u8, extern)

int main(void) {
	int err = 0;

	u8_vector v;

	err = u8_vector_init(&v);

	assert(err == LEMON_ESUCCESS);

	u8_vector_free(&v);

	return 0;
}
