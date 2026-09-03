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

#include "option.h"
#include "argopt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


static char opts[] = "?@:a:d:f:k:m:o:s:" /* GNU: */ "wr";
static char *** Ppoptions = NULL;
static char ** Poptions = NULL;
static int optionSets = 0;

char * dupl(const char * s)
    {
    char * d = new char[strlen(s) + 1];
    strcpy(d,s);
    return d;
    }

optionStruct::optionStruct()
    {
    arga = NULL;
    argd = NULL;
    argf = NULL;
    argk = NULL;
    argm = NULL; 
    argo = NULL;
    argp = NULL;
    args = NULL;
    }

optionStruct::~optionStruct()
    {
    for(int i = 0;i < optionSets;++i)
        {
        delete [] Poptions[i];
        delete [] Ppoptions[i];
        }
    delete [] Poptions;
    delete [] Ppoptions;

    delete[] arga;
    delete[] argd;
    delete[] argf;
    delete[] argk;
    delete[] argm;
    delete[] argo;
    delete[] argp;
    delete[] args;
    }

OptReturnTp optionStruct::doSwitch(int c,char * locoptarg,char * progname)
    {
    switch (c)
        {
        case '@':
            readOptsFromFile(locoptarg,progname);
            break;
        case 'h':
        case '?':
            LOG1LINE("usage:\n============================");
            printf("%s -D \\\n",progname);
            LOG1LINE("===============================");
            LOG1LINE("    -a  directory where audio annotation (in columnar form) is stored. E.g. Praat (input)\n"
                     "    -d  directory relative to which other directories are defined. Default: directory where the program is started from (./).\n"
                     "    -f  windows, numbers of frames for computing velocity, acceleration and jerk. Format #-#-#. E.g. 9-11-13 \n"
                     "    -k  directory where kinetic data (velocity, acceleration, jerk) is stored. (input)\n"
                     "    -m  directory where the original movies are found in a folder (input)\n"
                     "    -s  session name, e.g. 202201120\n"
                     "    -o  output\n"
                     "    -p  participant identifier. Optional.\n"
                     "===============================");
            return OptReturnTp::Leave;
        case 'a':
            arga = dupl(locoptarg);
            break;
        case 'd':
            argd = dupl(locoptarg);
            break;
        case 'f':
            argf = dupl(locoptarg);
            break;
        case 'k':
                argk = dupl(locoptarg);
            break;
        case 'm':
            argm = dupl(locoptarg);
            break;
        case 'o':
            argo = dupl(locoptarg);
            break;
        case 'p':
            argp = dupl(locoptarg);
            break;
        case 's':
            args = dupl(locoptarg);
            break;
// GNU >>
        case 'r':
            LOG1LINE("12. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\n"
            "WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR\n"
            "REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,\n"
            "INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING\n"
            "OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED\n"
            "TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY\n"
            "YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER\n"
            "PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE\n"
            "POSSIBILITY OF SUCH DAMAGES.\n");
            return OptReturnTp::Leave;
        case 'w':
            LOG1LINE("11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY\n"
            "FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN\n"
            "OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES\n"
            "PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED\n"
            "OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\n"
            "MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS\n"
            "TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE\n"
            "PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,\n"
            "REPAIR OR CORRECTION.");
            return OptReturnTp::Leave;
// << GNU
        }
    return OptReturnTp::GoOn;
    }

OptReturnTp optionStruct::readOptsFromFile(char * locoptarg,char * progname)
    {
    char ** poptions;
    char * options;
    FILE * fpopt = fopen(locoptarg,"r");
    OptReturnTp result = OptReturnTp::GoOn;
    if(fpopt)
        {
        char * p;
        char line[1000];
        int lineno = 0;
        size_t bufsize = 0;
        while(fgets(line,sizeof(line) - 1,fpopt))
            {
            lineno++;
            size_t off = strspn(line," \t");
            if(line[off] == ';')
                continue; // comment line
            if(line[off] == '-')
                {
                off++;
                if(line[off])
                    {
                    char * optarg2 = line + off + 1;
                    size_t off2 = strspn(optarg2," \t");
                    if(!optarg2[off2])
                        optarg2 = NULL;
                    else
                        optarg2 += off2;
                    if(optarg2)
                        {
                        for(p = optarg2 + strlen(optarg2) - 1;p >= optarg2;--p)
                            {
                            if(!isspace((unsigned char)*p))
                                break;
                            *p = '\0';
                            }
                        bool string = false;
                        if(*optarg2 == '\'' || *optarg2 == '"')
                            {

                            // -x 'jhgfjhagj asdfj\' hsdjfk' ; dfaasdhfg
                            // -x 'jhgfjhagj asdfj\' hsdjfk' ; dfa ' asdhfg
                            // -x "jhgfjhagj \"asdfj hsdjfk" ; dfaasdhfg
                            // -x "jhgfjhagj \"asdfj hsdjfk" ; dfa " asdhfg
                            for(p = optarg2 + strlen(optarg2) - 1;p > optarg2;--p)
                                {
                                if(*p == *optarg2)
                                    {
                                    string = true;
                                    for(char * q = p + 1;*q;++q)
                                        {
                                        if(*q == ';')
                                            break;
                                        if(!isspace((unsigned char)*q))
                                            {
                                            string = false;
                                            }
                                        }
                                    if(string)
                                        {
                                        *p = '\0';
                                        ++optarg2;
                                        }
                                    break;
                                    }
                                }
                            }
                        if(!*optarg2 && !string)
                            optarg2 = NULL;
                        }
                    if(optarg2)
                        {
                        bufsize += strlen(optarg2) + 1;
                        }
                    else
                        {
                        ++bufsize;
                        }
                    char * optpos = strchr(opts,line[off]);
                    if(optpos)
                        {
                        if(optpos[1] != ':')
                            {
                            if(optarg2)
                                {
                                fprintf(stderr, "Option argument %s provided for option letter %c that doesn't use it on line %d in option file \"%s\"\n",optarg2,line[off],lineno,locoptarg);
                                exit(1);
                                }
                            }
                        }
                    }
                else
                    {
                    fprintf(stderr, "Missing option letter on line %d in option file \"%s\"\n",lineno,locoptarg);
                    exit(1);
                    }
                }
            }
        rewind(fpopt);

        poptions = new char * [lineno];
        options = new char[bufsize];
        // update stacks that keep pointers to the allocated arrays.
        optionSets++;
        char *** tmpPpoptions = new char **[optionSets];
        char ** tmpPoptions = new char *[optionSets];
        int g;
        for(g = 0;g < optionSets - 1;++g)
            {
            tmpPpoptions[g] = Ppoptions[g];
            tmpPoptions[g] = Poptions[g];
            }
        tmpPpoptions[g] = poptions;
        tmpPoptions[g] = options;
        delete [] Ppoptions;
        Ppoptions = tmpPpoptions;
        delete [] Poptions;
        Poptions = tmpPoptions;

        lineno = 0;
        bufsize = 0;
        while(fgets(line,sizeof(line) - 1,fpopt))
            {
            poptions[lineno] = options+bufsize;
            size_t off = strspn(line," \t");
            if(line[off] == ';')
                continue; // comment line
            if(line[off] == '-')
                {
                off++;
                if(line[off])
                    {
                    char * optarg2 = line + off + 1;
                    size_t off2 = strspn(optarg2," \t");
                    if(!optarg2[off2])
                        optarg2 = NULL;
                    else
                        optarg2 += off2;
                    if(optarg2)
                        {
                        for(p = optarg2 + strlen(optarg2) - 1;p >= optarg2;--p)
                            {
                            if(!isspace((unsigned char)*p))
                                break;
                            *p = '\0';
                            }
                        bool string = false;
                        if(*optarg2 == '\'' || *optarg2 == '"')
                            {

                            // -x 'jhgfjhagj asdfj\' hsdjfk' ; dfaasdhfg
                            // -x 'jhgfjhagj asdfj\' hsdjfk' ; dfa ' asdhfg
                            // -x "jhgfjhagj \"asdfj hsdjfk" ; dfaasdhfg
                            // -x "jhgfjhagj \"asdfj hsdjfk" ; dfa " asdhfg
                            for(p = optarg2 + strlen(optarg2) - 1;p > optarg2;--p)
                                {
                                if(*p == *optarg2)
                                    {
                                    string = true;
                                    for(char * q = p + 1;*q;++q)
                                        {
                                        if(*q == ';')
                                            break;
                                        if(!isspace((unsigned char)*q))
                                            {
                                            string = false;
                                            }
                                        }
                                    if(string)
                                        {
                                        *p = '\0';
                                        ++optarg2;
                                        }
                                    break;
                                    }
                                }
                            }
                        if(!*optarg2 && /*allow empty string for e.g. -s option*/!string)
                            optarg2 = NULL;
                        }
                    if(optarg2)
                        {
                        strcpy(poptions[lineno],optarg2);
                        bufsize += strlen(optarg2) + 1;
                        }
                    else
                        {
                        strcpy(poptions[lineno],"");
                        //++bufsize;
                        }

                    /*else
                        optarg2 = "";
                    char * optpos = strchr(opts,line[off]);*/
                    OptReturnTp res = doSwitch(line[off],poptions[lineno],progname);
                    if(res > result)
                        result = res;
                    }
                }
            lineno++;
            }
        fclose(fpopt);
        }
    else
        {
        fprintf(stderr, "Cannot open option file %s\n",locoptarg);
        }
    return result;
    }

OptReturnTp optionStruct::readArgs(int argc, char * argv[])
    {
    int c;
    OptReturnTp result = OptReturnTp::GoOn;
    while((c = getopt(argc,argv, opts)) != -1)
        {
        OptReturnTp res = doSwitch(c,myoptarg,argv[0]);
        if(res > result)
            result = res;
        }
    return result;
    }
