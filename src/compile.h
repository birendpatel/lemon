/**
 * @file compile.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Lemon-to-bytecode Compiler API.
 */

#pragma once

#include "lemon.h"
#include "options.h"

/*******************************************************************************
 * @fn compile
 * @brief Transform source code into a program that can be loaded onto the C
 * Lemon virtual machine.
 ******************************************************************************/
lemon_error compile(options *opt, char *src);
