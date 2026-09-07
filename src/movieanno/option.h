/*
MOVIEANNO - movie annotator

Copyright (C) 2026  Center for Sprogteknologi, University of Copenhagen

This file is part of MOVIEANNO.

MOVIEANNO is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

MOVIEANNO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MOVIEANNO; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
enum class OptReturnTp { GoOn = 0, Leave = 1, Error = 2 };

extern char* dupl(const char* s);
enum class caseTp;

#define LOG1LINE(x) printf("%s\n",x)

struct optionStruct
    {
    // input and output
    const char* arga; // -a
    const char* argd; // -d
    const char* argf; // -f
    const char* argi; // -i
    const char* argj; // -j
    const char* argk; // -k
    const char* argm; // -m
    const char* argo; // -o
    const char* argp; // -o
    const char* args; // -s
    const char* argt; // -t

    optionStruct();
    ~optionStruct();
    OptReturnTp doSwitch(int c, char* locoptarg, char* progname);
    OptReturnTp readOptsFromFile(char* locoptarg, char* progname);
    OptReturnTp readArgs(int argc, char* argv[]);
    };
