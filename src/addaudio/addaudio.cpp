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
/*
addaudio - give the annotated (silent) movies their speaker audio back.

C++ equivalent of addaudio.sh (bash) and addaudio.ps1 (PowerShell).

    addaudio [options] <inputroot> <outputroot>

    <outputroot>                                    the annotated, silent .mov files
    <inputroot>/<yyyymmdd>/SeparatedSpeakersAudio   one .wav per speaker

Two passes, as in the scripts:

  1. every movie in <outputroot> is rewritten to <outputroot>/MoovAtom with the
     moov atom moved up front (-movflags +faststart), which is what makes the
     annotated movies playable;
  2. every movie in <outputroot>/MoovAtom is combined with the wav files of the
     day its name starts with - the first 8 characters, yyyymmdd - and written
     to <outputroot>/OverlayAndAudio.

By default the speakers are mixed down into a single audio track. With
--separate-tracks each speaker keeps a track of its own.

The lists the scripts leave behind (filelist.txt, movies.txt and audios.txt)
are written to <outputroot> as well.

ffmpeg must be on the PATH, or be named with --ffmpeg=<path>.
*/
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

/* Arguments are kept as paths and handed to the operating system in its own
encoding, so that spaces and non-ASCII characters in file names survive. The
scripts had to build a command line as text; here nothing is ever re-split. */
using Args = std::vector<fs::path>;

std::string u8(fs::path Path)
    {
    std::u8string Path8 = Path.u8string();
    std::string String8(Path8.cbegin(), Path8.cend());
    return String8;
    }

std::string join(const Args& args)
    {
    std::string line;
    for(const fs::path& arg : args)
        {
        if(!line.empty())
            line += ' ';
        line += u8(arg);
        }
    return line;
    }

bool sameExtension(const fs::path& Path, const char* ext)
    {
    std::string have = Path.extension().string();
    std::string want = ext;
    if(have.size() != want.size())
        return false;

    for(size_t i = 0; i < have.size(); ++i)
        if(tolower(static_cast<unsigned char>(have[i])) != want[i])
            return false;

    return true;
    }

/* All names in <Dir> with extension <ext>, sorted, as 'ls -1' and
'Get-ChildItem | Sort-Object Name' give them. An unreadable or absent
directory yields nothing, like the '2>/dev/null' in addaudio.sh. */
std::vector<fs::path> listFiles(const fs::path& Dir, const char* ext)
    {
    std::vector<fs::path> names;
    std::error_code ec;
    for(const fs::directory_entry& entry : fs::directory_iterator(Dir, ec))
        {
        if(!entry.is_regular_file(ec))
            continue;

        if(ext == nullptr || sameExtension(entry.path(), ext))
            names.push_back(entry.path().filename());
        }
    std::sort(names.begin(), names.end());
    return names;
    }

void writeList(const fs::path& File, const std::vector<fs::path>& names)
    {
    std::ofstream list(File);
    if(!list)
        {
        fprintf(stderr, "addaudio: cannot write %s\n", u8(File).c_str());
        return;
        }
    for(const fs::path& name : names)
        list << u8(name) << '\n';
    }

#ifdef _WIN32
/* CreateProcess takes one string, so the arguments have to be quoted after
all - by the rules the C runtime of the child uses to take them apart again.
See "Parsing C++ Command-Line Arguments" in the Microsoft documentation. */
void appendQuoted(std::wstring& cmd, const std::wstring& arg)
    {
    if(!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        {
        cmd += arg;
        return;
        }

    cmd += L'"';
    for(std::wstring::const_iterator it = arg.begin();; ++it)
        {
        size_t backslashes = 0;
        while(it != arg.end() && *it == L'\\')
            {
            ++it;
            ++backslashes;
            }

        if(it == arg.end())
            {
            cmd.append(2 * backslashes, L'\\'); // all of them end up before the closing quote
            break;
            }
        else if(*it == L'"')
            {
            cmd.append(2 * backslashes + 1, L'\\');
            cmd += L'"';
            }
        else
            {
            cmd.append(backslashes, L'\\');
            cmd += *it;
            }
        }
    cmd += L'"';
    }

int runProcess(const Args& args)
    {
    std::wstring cmd;
    for(const fs::path& arg : args)
        {
        if(!cmd.empty())
            cmd += L' ';
        appendQuoted(cmd, arg.wstring());
        }

    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if(!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi))
        {
        fprintf(stderr, "addaudio: cannot start %s (error %lu)\n", u8(args[0]).c_str(), GetLastError());
        return -1;
        }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD status = 0;
    if(!GetExitCodeProcess(pi.hProcess, &status))
        status = static_cast<DWORD>(-1);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(status);
    }
#else
int runProcess(const Args& args)
    {
    std::vector<std::string> strings;
    for(const fs::path& arg : args)
        strings.push_back(arg.string());

    std::vector<char*> argv;
    for(std::string& s : strings)
        argv.push_back(s.data());
    argv.push_back(nullptr);

    fflush(nullptr); // the child inherits our stdout
    pid_t pid = fork();
    if(pid < 0)
        {
        perror("addaudio: fork");
        return -1;
        }

    if(pid == 0)
        {
        execvp(argv[0], argv.data());
        perror(argv[0]);
        _exit(127);
        }

    int status = 0;
    if(waitpid(pid, &status, 0) < 0)
        {
        perror("addaudio: waitpid");
        return -1;
        }

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
#endif

struct optionStruct
    {
    fs::path inputroot;
    fs::path outputroot;
    fs::path ffmpeg{ "ffmpeg" };
    bool separateTracks = false; // one audio track per speaker instead of a mix
    bool dryRun = false;         // print the ffmpeg command lines, run nothing
    };

void usage(const char* progname)
    {
    printf("usage: %s [options] <inputroot> <outputroot>\n", progname);
    printf("  -s, --separate-tracks  give every speaker an audio track of its own\n");
    printf("                         (default: mix all speakers into one track)\n");
    printf("  -n, --dry-run          print the ffmpeg command lines, run nothing\n");
    printf("      --ffmpeg=<path>    ffmpeg to call (default: ffmpeg, from the PATH)\n");
    printf("  -h, --help             this text\n");
    }

bool readArgs(int argc, char* argv[], optionStruct& options)
    {
    std::vector<std::string> positional;
    for(int i = 1; i < argc; ++i)
        {
        std::string arg = argv[i];
        if(arg == "-s" || arg == "--separate-tracks")
            options.separateTracks = true;
        else if(arg == "-n" || arg == "--dry-run")
            options.dryRun = true;
        else if(arg.rfind("--ffmpeg=", 0) == 0)
            options.ffmpeg = fs::path(arg.substr(9));
        else if(arg == "-h" || arg == "--help")
            {
            usage(argv[0]);
            exit(0);
            }
        else if(!arg.empty() && arg[0] == '-' && arg != "-")
            {
            fprintf(stderr, "%s: unknown option %s\n", argv[0], arg.c_str());
            usage(argv[0]);
            return false;
            }
        else
            positional.push_back(arg);
        }

    if(positional.size() != 2)
        {
        usage(argv[0]);
        return false;
        }

    options.inputroot = fs::path(positional[0]);
    options.outputroot = fs::path(positional[1]);
    return true;
    }

/* Pass 1: copy every movie in <outputroot> to <outputroot>/MoovAtom with the
moov atom up front. Streams are copied, nothing is re-encoded. */
void faststart(const optionStruct& options, const fs::path& MoovAtom, const std::vector<fs::path>& movies)
    {
    for(const fs::path& f : movies)
        {
        fs::path Out = MoovAtom / f;
        std::error_code ec;
        fs::remove(Out, ec);

        Args args{ options.ffmpeg, "-i", options.outputroot / f, "-c", "copy", "-movflags", "+faststart", Out };
        printf("%s\n", join(args).c_str());
        fflush(stdout);
        if(!options.dryRun)
            {
            int status = runProcess(args);
            if(status != 0)
                fprintf(stderr, "addaudio: ffmpeg exited with %d for %s\n", status, u8(f).c_str());
            }
        }
    }

/* Pass 2: add the speaker audio of day yyyymmdd to one movie. */
void addAudio(const optionStruct& options, const fs::path& MoovAtom, const fs::path& OverlayAndAudio, const fs::path& f, const std::vector<fs::path>& audios, const fs::path& AudioDir)
    {
    Args args{ options.ffmpeg, "-i", MoovAtom / f };
    std::string filterInputs; // [1:a][2:a]... for amix
    size_t n = 0;
    for(const fs::path& g : audios)
        {
        ++n;
        args.push_back("-i");
        args.push_back(AudioDir / g);
        filterInputs += "[" + std::to_string(n) + ":a]";
        }

    Args maps{ "-map", "0:v" };
    if(options.separateTracks)
        {
        // one audio track per speaker
        for(size_t i = 1; i <= n; ++i)
            {
            maps.push_back("-map");
            maps.push_back(std::to_string(i) + ":a");
            }
        }
    else
        {
        // all speakers mixed down into a single audio track
        args.push_back("-filter_complex");
        args.push_back(filterInputs + "amix=inputs=" + std::to_string(n) + ":duration=longest[aout]");
        maps.push_back("-map");
        maps.push_back("[aout]");
        }
    printf("m %s\n", join(maps).c_str());

    fs::path Out = OverlayAndAudio / f;
    std::error_code ec;
    fs::remove(Out, ec);

    args.insert(args.end(), maps.begin(), maps.end());
    for(const char* option : { "-c:v", "copy", "-acodec", "aac", "-strict", "experimental", "-f", "mp4" })
        args.push_back(option);
    args.push_back(Out);

    printf("%s\n", join(args).c_str());
    fflush(stdout);
    if(!options.dryRun)
        {
        int status = runProcess(args);
        if(status != 0)
            fprintf(stderr, "addaudio: ffmpeg exited with %d for %s\n", status, u8(f).c_str());
        }
    }

int main(int argc, char* argv[])
    {
    optionStruct options;
    if(!readArgs(argc, argv, options))
        return 1;

    std::error_code ec;
    if(!fs::is_directory(options.outputroot, ec))
        {
        fprintf(stderr, "addaudio: %s is not a directory\n", u8(options.outputroot).c_str());
        return 1;
        }

    // define your list of files (edit text file)
    std::vector<fs::path> movies = listFiles(options.outputroot, ".mov");
    writeList(options.outputroot / "filelist.txt", movies);

    // a directory for (silent) annotated movies corrected for playability
    fs::path MoovAtom = options.outputroot / "MoovAtom";
    // and one for playable sound movies with annotations
    fs::path OverlayAndAudio = options.outputroot / "OverlayAndAudio";
    fs::create_directories(MoovAtom, ec);
    fs::create_directories(OverlayAndAudio, ec);

    faststart(options, MoovAtom, movies);

    std::vector<fs::path> made = listFiles(MoovAtom, nullptr);
    writeList(options.outputroot / "movies.txt", made);

    for(const fs::path& f : made)
        {
        std::string name = u8(f);
        if(name.size() < 8)
            continue;

        std::string yyyymmdd = name.substr(0, 8);
        printf("y %s\n", yyyymmdd.c_str());

        fs::path AudioDir = options.inputroot / yyyymmdd / "SeparatedSpeakersAudio";
        std::vector<fs::path> audios = listFiles(AudioDir, ".wav");
        writeList(options.outputroot / "audios.txt", audios);
        if(audios.empty())
            continue;

        addAudio(options, MoovAtom, OverlayAndAudio, f, audios, AudioDir);
        }

    return 0;
    }
