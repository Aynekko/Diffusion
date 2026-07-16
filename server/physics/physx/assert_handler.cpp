/*
assert_handler.cpp - class for handling PhysX asserts
Copyright (C) 2023 SNMetamorph
Copyright (C) 2026 rickomax

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include "assert_handler.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include <string>

/*
=================
operator()

reports a failed PhysX assert (stripped to the bare file name) and decides whether to ignore it: ignored in debug builds, fatal otherwise
=================
*/
void AssertHandler::operator()(const char *expr, const char *file, int line, bool &ignore)
{
	std::string fileName = file;
	fileName.erase(0, fileName.find_last_of("/\\") + 1);
	ALERT(at_error, "PhysX Assert failed: %s(%d): %s\n", fileName.c_str(), line, expr);

#ifdef _DEBUG
	ignore = true;
#else
	ignore = false;
#endif
}
