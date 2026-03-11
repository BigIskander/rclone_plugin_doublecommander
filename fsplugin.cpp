/*
Rclone plugin for Double Commander

Wfx plugin for working with different cloud storages using Rclone.

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

#include <stdio.h>
#include <cstring>
#include <string>
#include <array>
#include <algorithm>
#include <memory>
#include "common.h"
#include "fsplugin.h"
#include "utils.hpp"
#include "json.hpp"

#define _plugin_name "Rclone"

int gPluginNumber;
tProgressProcW gProgressProc = NULL;
tLogProcW gLogProc = NULL;
tRequestProcW gRequestProc = NULL;
tCryptProcW gCryptProc = NULL;

int DCPCALL FsInitW(int PluginNr, tProgressProcW pProgressProc, tLogProcW pLogProc, tRequestProcW pRequestProc) 
{
    gProgressProc = pProgressProc;
    gLogProc = pLogProc;
    gRequestProc = pRequestProc;
    gPluginNumber = PluginNr;

    return 0;
}

bool isInit = false;
bool isPut = false;

wcharstring sanitize(wcharstring value) 
{
    // escape special characters https://www.gnu.org/software/bash/manual/html_node/Double-Quotes.html
    wcharstring sanitizedValue = (WCHAR*)u"\"";
    for(int i = 0; i < value.size(); i++) {
        if(
            value.at(i) == (WCHAR)u'$' || value.at(i) == (WCHAR)u'`' || 
            value.at(i) == (WCHAR)u'\\' || value.at(i) == (WCHAR)u'!'
        ) {
            sanitizedValue.push_back((WCHAR)u'\\');
        }
        sanitizedValue.push_back(value.at(i));
    }
    sanitizedValue.push_back((WCHAR)u'"');
    return sanitizedValue;
}

// execute shell command and return stdout pipe
std::unique_ptr<FILE, decltype(&pclose)> executeCommand(std::string commandString)
{
    const char* command = commandString.c_str();
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    return pipe;
}

void setEnvVariables()
{
#ifdef __APPLE__
    // source from ~/.zprofile file and get $PATH env variable
    std::string commandString = "source ~/.zprofile > /dev/null 2>&1; echo $PATH";
    std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString);
    if (!pipe) {
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"C++ popen() failed.");
        return; // popen failed
    }
    // Read the output line by line into the buffer
    std::array<char, 128> buffer;
    std::string resultString;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        resultString += buffer.data();
    }
    // close popen
    int status = pclose(pipe.release());
    if(status != 0) {
        // if error occured
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
            (WCHAR*) wcharstring((WCHAR*)u"Command (")
                .append(UTF8toUTF16(commandString))
                .append((WCHAR*)u") exited with status ")
                .append(int_to_wcharstring(status)).data()
        );
        return;
    }
    // set $PATH env variable
    status = setenv("PATH", resultString.c_str(), 1);
    if(status != 0) {
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"Failed to set PATH env variable.");
    }
    gLogProc(gPluginNumber, MSGTYPE_DETAILS, (WCHAR*)u"Set PATH env variable.");
#endif
    return;
}

HANDLE DCPCALL FsFindFirstW(WCHAR* Path, WIN32_FIND_DATAW *FindData)
{
    pResources pRes = NULL;

    wcharstring wPath(Path);
    if(wPath.length() == 0) return (HANDLE)-1;
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    // fix issue with traversing files and folders
    if(wPath.size() > 1 && wPath.substr(wPath.size() - 1) == (WCHAR*)u"/")
    {
        wPath = wPath.substr(0, wPath.size() - 1);
        // ignore this kind of request when put files, for speed
        if(isPut) return (HANDLE)-1;
    } 

    if(!isInit)
    {
        setEnvVariables(); // set env variables only once
        isInit = true;
    }

    // variables used for request to rclone
    std::array<char, 128> buffer;
    std::vector<wcharstring> resultVector;
    std::string resultString;

    if(wPath.length() == 1) { // root folder of plugin
        // gLogProc(gPluginNumber, MSGTYPE_CONNECT, (WCHAR*)u"123");
        // request list of configured storages (available remotes) from rclone's config 
        std::string commandString("rclone listremotes");
        std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString);
        
        if (!pipe) {
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"C++ popen() failed.");
            return (HANDLE)-1; // popen failed
        }

        // Read the output line by line into the buffer
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            wcharstring itemName = UTF8toUTF16(buffer.data());
            if(std::isspace(itemName.at(itemName.size() - 1))) // delete last element if it is space
                itemName = itemName.substr(0, itemName.size() - 1);
            resultVector.push_back(itemName);
        }

        // close popen
        int status = pclose(pipe.release());
        if(status != 0) {
            // if error occured
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
                (WCHAR*) wcharstring((WCHAR*)u"Command (rclone listremotes) exited with status ")
                    .append(int_to_wcharstring(status)).data()
            );
            return (HANDLE)-1;
        }

        pRes = new tResources;
        pRes->nCount = 0;
        pRes->resource_array.resize(resultVector.size());

        size_t str_size;
        wcharstring itemName;
        for(int i = 0; i < resultVector.size(); i++) 
        {
            itemName = resultVector[i].substr(resultVector.size() - 1); // delete last empty value
            str_size = (MAX_PATH > resultVector[i].size()+1) ? 
                (resultVector[i].size()+1): MAX_PATH;
            memcpy(
                pRes->resource_array[i].cFileName, 
                resultVector[i].data(), 
                str_size * sizeof(WCHAR)
            );
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        }

    } else {
        // request list of items in folder of cloud storage (in json format)
        std::string commandString = UTF16toUTF8(
                wcharstring((WCHAR*)u"rclone lsjson ").append(sanitize(wPath.substr(1))).data()
            );
        std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString);
        
        if (!pipe) {
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"C++ popen() failed.");
            return (HANDLE)-1; // popen failed
        }
        
        // Read the output line by line into the buffer
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            resultString += buffer.data();
        }

        // close popen
        int status = pclose(pipe.release());
        if(status != 0) {
            // if error occured
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
                (WCHAR*) wcharstring((WCHAR*)u"Command (")
                    .append(UTF8toUTF16(commandString))
                    .append((WCHAR*)u") exited with status ")
                    .append(int_to_wcharstring(status)).data()
            );
            return (HANDLE)-1;
        }

        try
        {
            // parse Json string and convert to vector of individual items
            nlohmann::json resultJson = nlohmann::json::parse(resultString);
            std::vector<nlohmann::json> resultJsonVector;
            std::for_each(resultJson.begin(), resultJson.end(), [&resultJsonVector](nlohmann::json &resultLine) {
                resultJsonVector.push_back(resultLine);
            });

            pRes = new tResources;
            pRes->nCount = 0;
            pRes->resource_array.resize(resultJsonVector.size());

            size_t str_size;
            for(int i = 0; i < resultJsonVector.size(); i++) 
            {
                wcharstring itemName = UTF8toUTF16(resultJsonVector[i]["Name"]);
                str_size = (MAX_PATH > itemName.size()+1) ? 
                    (itemName.size()+1): MAX_PATH;
                memcpy(
                    pRes->resource_array[i].cFileName, 
                    itemName.data(), 
                    str_size * sizeof(WCHAR)
                );
                if(resultJsonVector[i]["IsDir"])
                    pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                if(resultJsonVector[i]["Size"] != -1) {
                    pRes->resource_array[i].nFileSizeLow = static_cast<DWORD>((uint64_t)resultJsonVector[i]["Size"] & 0xFFFFFFFF);
                    pRes->resource_array[i].nFileSizeHigh = static_cast<DWORD>((uint64_t)resultJsonVector[i]["Size"] >> 32);
                }
                pRes->resource_array[i].ftLastWriteTime = get_file_time(resultJsonVector[i]["ModTime"]);

            }
        }
        catch(const std::exception& e)
        {
            // got error if path does not exists usually
            gLogProc(gPluginNumber, MSGTYPE_DETAILS, (WCHAR*) UTF8toUTF16(e.what()).data());
            return (HANDLE)-1;
        }
    }

    if(!pRes || pRes->resource_array.size()==0)
        return (HANDLE)-1;

    if(pRes->resource_array.size()>0){
        memset(FindData, 0, sizeof(WIN32_FIND_DATAW));
        memcpy(FindData, &(pRes->resource_array[0]), sizeof(WIN32_FIND_DATAW));
        pRes->nCount++;
    }

    return (HANDLE) pRes;
}

BOOL DCPCALL FsFindNextW(HANDLE Hdl, WIN32_FIND_DATAW *FindData)
{
    pResources pRes = (pResources) Hdl;

    if(pRes && (pRes->nCount < pRes->resource_array.size()) ){
        memcpy(FindData, &pRes->resource_array[pRes->nCount], sizeof(WIN32_FIND_DATAW));
        pRes->nCount++;
        return true;
    } else {
        if(pRes != NULL)
        {
            pRes->resource_array.clear();
            delete pRes;
        }
        return false;
    }
}

int DCPCALL FsFindClose(HANDLE Hdl) 
{
    // this function is not even called
    return 0;
}

void DCPCALL FsGetDefRootName(char* DefRootName, int maxlen)
{
    strncpy(DefRootName, _plugin_name, maxlen);
}

int DCPCALL FsGetFileW(WCHAR* RemoteName, WCHAR* LocalName, int CopyFlags, RemoteInfoStruct* ri)
{
    if(CopyFlags & FS_COPYFLAGS_RESUME) // resume copy not supported
        return FS_FILE_NOTSUPPORTED;

    wcharstring wPath(RemoteName), deviceName, storageName, internalPath, wLocal(LocalName);
    if(wPath.length() == 0 || wLocal.length() == 0) return FS_FILE_OK; // just ignore this case
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    if(wPath == (WCHAR*)u"/")
        return FS_FILE_NOTSUPPORTED;

    // do not copy if file exists and no overwrite flag is set
    BOOL isFileExists = file_exists(UTF16toUTF8(LocalName));
    if(isFileExists && !(CopyFlags & FS_COPYFLAGS_OVERWRITE) )
        return FS_FILE_EXISTS;

    if(gProgressProc(gPluginNumber, RemoteName, LocalName, 0) != 0) 
        return FS_FILE_USERABORT;

    // copy file from to (replaces file if already exists)
    std::string commandString = UTF16toUTF8(
            wcharstring((WCHAR*)u"rclone copyto ") // copy
                .append(sanitize(wPath.substr(1))) // from
                .append((WCHAR*)u" ")
                .append(sanitize(wLocal)).data() // to
        );
    std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString); 

    if (!pipe) {
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"C++ popen() failed.");
        return FS_FILE_READERROR; // popen failed
    }

    // close popen
    int status = pclose(pipe.release());
    if(status != 0) {
        // if error occured
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
            (WCHAR*) wcharstring((WCHAR*)u"Command (")
                .append(UTF8toUTF16(commandString))
                .append((WCHAR*)u") exited with status ")
                .append(int_to_wcharstring(status)).data()
        );
        return FS_FILE_WRITEERROR;
    }

    gProgressProc(gPluginNumber, RemoteName, LocalName, 100);
    return FS_FILE_OK;
}