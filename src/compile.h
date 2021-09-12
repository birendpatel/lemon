/**
 * @file compile.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Lemon-to-bytecode Compiler API.
 */

#pragma once

#include "options.h"
#include "xerror.h"

/*******************************************************************************
 * @fn compile
 * @brief Transform source code into a program that can be loaded onto the C
 * Lemon virtual machine.
 ******************************************************************************/
xerror compile(char *src, options *opt, char *fname);
