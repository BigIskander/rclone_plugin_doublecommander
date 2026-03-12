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
#include "fsutils.hpp"

#define _plugin_name "Rclone"

int DCPCALL FsInitW(
    int PluginNr, tProgressProcW pProgressProc, tLogProcW pLogProc, tRequestProcW pRequestProc
) {
    gProgressProc = pProgressProc;
    gLogProc = pLogProc;
    gRequestProc = pRequestProc;
    gPluginNumber = PluginNr;

    return 0;
}

bool isInit = false;
bool isPut = false;

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
        if(!executeCommandAndReturnVector(commandString, resultVector)) return (HANDLE)-1;

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
        if(!executeCommandAndReturnString(commandString, resultString)) return (HANDLE)-1;

        try
        {
            // parse Json string and convert to vector of individual items
            nlohmann::json resultJson = nlohmann::json::parse(resultString);
            std::vector<nlohmann::json> resultJsonVector;
            std::for_each(
                resultJson.begin(), 
                resultJson.end(), 
                [&resultJsonVector](nlohmann::json &resultLine) {
                    resultJsonVector.push_back(resultLine);
                }
            );

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
                    pRes->resource_array[i].nFileSizeLow = 
                        static_cast<DWORD>((uint64_t)resultJsonVector[i]["Size"] & 0xFFFFFFFF);
                    pRes->resource_array[i].nFileSizeHigh = 
                        static_cast<DWORD>((uint64_t)resultJsonVector[i]["Size"] >> 32);
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

    wcharstring wPath(RemoteName), fileName, wLocal(LocalName);
    if(wPath.length() == 0 || wLocal.length() == 0) return FS_FILE_OK; // just ignore this case
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    if(wPath == (WCHAR*)u"/")
        return FS_FILE_NOTSUPPORTED;

    getFileName(wPath, fileName);
    if(wPath == wcharstring((WCHAR*)u"/").append(fileName))
        return FS_FILE_NOTSUPPORTED; // cannot copy file from root folder of plugin

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
    if (!pipe) return FS_FILE_READERROR; // popen failed
    // close popen
    if(popenClose(&pipe, commandString) != 0) return FS_FILE_WRITEERROR;

    gProgressProc(gPluginNumber, RemoteName, LocalName, 100);
    return FS_FILE_OK;
}

int DCPCALL FsPutFileW(WCHAR* LocalName, WCHAR* RemoteName, int CopyFlags) {
    if (CopyFlags & FS_COPYFLAGS_RESUME)
        return FS_FILE_NOTSUPPORTED;

    // this hint is never sent
    if(((CopyFlags & FS_COPYFLAGS_EXISTS_SAMECASE) || (CopyFlags & FS_COPYFLAGS_EXISTS_DIFFERENTCASE)) 
            && !(CopyFlags & FS_COPYFLAGS_OVERWRITE))  
        return FS_FILE_EXISTS;
    
    wcharstring wPath(RemoteName), folderPath, fileName, wLocal(LocalName);
    if(wPath.length() == 0 || wLocal.length() == 0) return FS_FILE_OK; // just ignore this case
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    if(wPath == (WCHAR*)u"/")
        return FS_FILE_NOTSUPPORTED;

    getFolderPath(wPath, folderPath);
    if(folderPath == (WCHAR*)u"/")
        return FS_FILE_NOTSUPPORTED; // cannot copy to root folder of plugin 

    getFileName(wPath, fileName);

    if(gProgressProc(gPluginNumber, LocalName, RemoteName, 0) != 0) 
        return FS_FILE_USERABORT;

    // get cache if cache exists otherwise make cache
    PathFolderElement* cache = getFolderCache(folderPath);
    if(cache == NULL) 
        cache = addFolderToCache(folderPath);
    
    if(isItemInCache(cache, fileName))
    {
        if(!(CopyFlags & FS_COPYFLAGS_OVERWRITE))
            return FS_FILE_EXISTS;
    }

    // copy file from to (replaces file if already exists)
    std::string commandString = UTF16toUTF8(
            wcharstring((WCHAR*)u"rclone copyto ") // copy
                .append(sanitize(wLocal)) // from
                .append((WCHAR*)u" ")
                .append(sanitize(wPath.substr(1))).data() // to
        );
    std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString); 
    if (!pipe) return FS_FILE_READERROR; // popen failed
    // close popen
    if(popenClose(&pipe, commandString) != 0) return FS_FILE_WRITEERROR;

    gProgressProc(gPluginNumber, LocalName, RemoteName, 100); 
    return FS_FILE_OK;
}

// managing cache in this function
void DCPCALL FsStatusInfoW(WCHAR* RemoteDir, int InfoStartEnd, int InfoOperation)
{
    wcharstring wPath(RemoteDir);
    // fix for wierd issue when Double Commander sends RemoteDir as empty (basic_string)
    if(wPath.length() == 0) return; 
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');
    if(wPath.size() > 1 && wPath.substr(wPath.size() - 1) == (WCHAR*)u"/")
    {
        wPath = wPath.substr(0, wPath.size() - 1);
    }
    // put file or files
    if(InfoOperation == FS_STATUS_OP_PUT_SINGLE || InfoOperation == FS_STATUS_OP_PUT_MULTI)
    {
        if(InfoStartEnd == FS_STATUS_START) 
        {
            isPut = true;
            cacheOfFolders.clear();
            addFolderToCache(wPath);
        }  
        if(InfoStartEnd == FS_STATUS_END) 
        {
            isPut = false;
        }
    }
}