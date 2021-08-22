/**
 * @file parser.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Parser API.
 */

#pragma once

#include "xerror.h"

/*******************************************************************************
 * @fn parse
 * @brief Transform raw source code to an abstract syntax tree.
 * @param src Null terminated char array
 ******************************************************************************/
xerror parse(char *src);
