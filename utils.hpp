/*
    This is modified version of original code copied from (accessed 22 nov 2025):
    https://github.com/ivanenko/cloud_storage/blob/master/plugin_utils.h

Copyright (C) 2019 Ivanenko Danil (ivanenko.danil@gmail.com)
Copyright (C) 2026 Iskander Sultanov (BigIskander@gmail.com)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
        License as published by the Free Software Foundation; either
        version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
        Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
        Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#ifndef _UTILS_HPP
#define _UTILS_HPP

#include "common.h"
#include <codecvt>
#include <locale>
#include <vector>
#include <sys/stat.h>
#include <iomanip>
#include <sstream>

typedef struct {
    int nCount;
    std::vector<WIN32_FIND_DATAW> resource_array;
} tResources, *pResources;

typedef std::basic_string<WCHAR> wcharstring;

// function to get current time (this function is not used)
// FILETIME get_now_time()
// {
//     time_t t2 = time(0);
//     int64_t ft = (int64_t) t2 * 10000000 + 116444736000000000;
//     FILETIME file_time;
//     // deleted: &0xffff - it causes time shift to ~5 min
//     file_time.dwLowDateTime = ft;
//     file_time.dwHighDateTime = ft >> 32;
//     return file_time;
// }

// add function converting RFC3339 standard time sting to FILETIME
FILETIME get_file_time(std::string tm)
{
    // convert std::string to time_t
    std::tm t;
    std::istringstream ss(tm.substr(0, 19)); // parse time part (without timezone)
    ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
    t.tm_isdst = 0; // turn off Daylight Saving Time flag because it causes timeshift to 1 hour im macOS
    time_t ttime = std::mktime(const_cast<std::tm*>(&t));
    // adjust time_t for timezone if necessary
    // get current timezone
    // https://codereview.stackexchange.com/questions/175353/getting-current-timezone
    time_t timeShiftLocal = 0, timeShiftRemote = 0;
    std::time_t now = std::time(NULL);
    std::time_t local = std::mktime(std::localtime(&now));
    std::time_t gmt = std::mktime(std::gmtime(&now));
    timeShiftLocal = static_cast<long> (local - gmt);
    // get timezone of the file or folder
    int pos = tm.find_last_of("Z+-");
    if(pos != tm.npos) {
        if(tm.at(pos) != 'Z') {
            if(tm.length() >= pos + 3) {
                int hours = std::stoi(tm.substr(pos + 1, 2));
                if(tm.at(pos) == '+') timeShiftRemote = hours * 3600; // i.e. 3600 - seconds in 1 hour
                else timeShiftRemote = hours * 3600;
            }
        }
    }
    // adjust time_t to differences in timezones
    ttime += (timeShiftLocal - timeShiftRemote);
    // convert time_t to FILETIME
    int64_t ft = (int64_t) ttime * 10000000 + 116444736000000000;
    FILETIME file_time;
    // 
    file_time.dwLowDateTime = ft;
    file_time.dwHighDateTime = ft >> 32;
    return file_time;
}

std::string UTF16toUTF8(const WCHAR *p)
{
    // replaced codecvt_utf8 with codecvt_utf8_utf16 to fix an conversion error
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert2;
    return convert2.to_bytes(std::u16string((char16_t*) p));
}

wcharstring UTF8toUTF16(const std::string &str)
{
    // replaced codecvt_utf8 with codecvt_utf8_utf16 to fix an conversion error
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert2;
    std::u16string utf16 = convert2.from_bytes(str);
    return wcharstring((WCHAR*)utf16.data());
}

BOOL file_exists(const std::string& filename)
{
#if defined(_WIN32) || defined(_WIN64)
    struct _stat buf;
    return _wstat(UTF8toUTF16(filename).c_str(), &buf) == 0;
#else
    struct stat buf;
    return stat(filename.c_str(), &buf) == 0;
#endif
}

// add function to gel local file size (this function is not used)
// BOOL get_file_size(const std::string& filename, uint64_t& fileSize)
// {
//     struct stat buf;
//     if(stat(filename.c_str(), &buf) != 0) return false;
//     fileSize = buf.st_size;
//     return true;
// }

// add function to convert int to wcharstring
wcharstring int_to_wcharstring(int num) 
{
    return UTF8toUTF16(std::to_string(num));
}

#endif