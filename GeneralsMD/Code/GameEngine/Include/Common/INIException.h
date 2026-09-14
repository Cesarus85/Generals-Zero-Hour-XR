/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: INIException.h ///////////////////////////////////////////////////////////////////////////
// Author: John McDonald, Jr, October 2002
// Desc:   INI Exception class. Thrown when INIs fail to read.
///////////////////////////////////////////////////////////////////////////////////////////////////

class INIException
{
	// This is a stack based exception class. It is used to output useful information
	// when thrown from an INI message

public:
	char *mFailureMessage;

	INIException(const char* errorMessage) : mFailureMessage(nullptr)
	{
		if (errorMessage) {
			mFailureMessage = new char[strlen(errorMessage) + 1];
			strcpy(mFailureMessage, errorMessage);
		}
	}

	// GeneralsX @bugfix Android port 12/07/2026 - Without these, the compiler-
	// generated copy constructor/assignment shallow-copy mFailureMessage, so
	// any code that catches this type by value (as GameEngine.cpp used to)
	// ends up with two objects pointing at the same heap block; both
	// destructors then delete[] it, a double-free that corrupts the heap
	// right as an INI parse error is being reported. Deep-copy instead.
	INIException(const INIException& other) : mFailureMessage(nullptr)
	{
		if (other.mFailureMessage) {
			mFailureMessage = new char[strlen(other.mFailureMessage) + 1];
			strcpy(mFailureMessage, other.mFailureMessage);
		}
	}

	INIException& operator=(const INIException& other)
	{
		if (this != &other) {
			delete [] mFailureMessage;
			mFailureMessage = nullptr;
			if (other.mFailureMessage) {
				mFailureMessage = new char[strlen(other.mFailureMessage) + 1];
				strcpy(mFailureMessage, other.mFailureMessage);
			}
		}
		return *this;
	}

	~INIException()
	{
		delete [] mFailureMessage;
	}
};
