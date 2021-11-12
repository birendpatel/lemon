// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Compiler phase 1: lexical analysis: source code -> scanner -> tokens

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "token.h"
#include "xerror.h"
#include "channel.h"
#include "str.h"

//------------------------------------------------------------------------------
//Tokens are sent on the channel in the order that they are found. On completion
//a final _EOF token is sent and the channel is closed.

make_channel(token, Token, static)

//------------------------------------------------------------------------------
//Execute lexical analysis in a new detached thread. The input channel must be
//initialised prior to this call and not freed until the final _EOF is received.
//The scanner has the exclusive right to close the channel. On failure XETHREAD
//is returned.

xerror ScannerInit(const cstring *src, Token_channel *chan);
