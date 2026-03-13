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

#ifndef _FSUTILS_HPP
#define _FSUTILS_HPP

int gPluginNumber;
tProgressProcW gProgressProc = NULL;
tLogProcW gLogProc = NULL;
tRequestProcW gRequestProc = NULL;
tCryptProcW gCryptProc = NULL;

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
    if(!pipe) gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"C++ popen() failed.");
    return pipe;
}

// close shell
bool popenClose(std::unique_ptr<FILE, decltype(&pclose)> *pipe, std::string commandString)
{
    // close popen
    int status = pclose(pipe->release());
    if(status != 0) {
        // show message if error occured
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
            (WCHAR*) wcharstring((WCHAR*)u"Command (")
                .append(UTF8toUTF16(commandString))
                .append((WCHAR*)u") exited with status ")
                .append(int_to_wcharstring(status)).data()
        );
    }
    return status;
}

// execute shell command and return stdout as Vector
bool executeCommandAndReturnVector(std::string commandString, std::vector<wcharstring> &resultVector) 
{
    std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString);
    if (!pipe) return false; // popen failed
    // Read the output line by line into the buffer
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        wcharstring itemName = UTF8toUTF16(buffer.data());
        if(std::isspace(itemName.at(itemName.size() - 1))) // delete last element if it is space
            itemName = itemName.substr(0, itemName.size() - 1);
        resultVector.push_back(itemName);
    }
    if(popenClose(&pipe, commandString) != 0) return false;
    return true;
}

// execute shell command and return stdout as text
bool executeCommandAndReturnString(std::string commandString, std::string &resultString)
{
    std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString);
    if (!pipe) return false; // popen failed
    // Read the output line by line into the buffer
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        resultString += buffer.data();
    }
    if(popenClose(&pipe, commandString) != 0) return false;
    return true;
}

// set env variables if necessary
void setEnvVariables()
{
#ifdef __APPLE__
    // source from ~/.zprofile file and get $PATH env variable
    std::string commandString = "source ~/.zprofile > /dev/null 2>&1; echo $PATH";
    std::string resultString;
    if(!executeCommandAndReturnString(commandString, resultString)) return;
    // set $PATH env variable
    if(setenv("PATH", resultString.c_str(), 1) != 0) {
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"Failed to set PATH env variable.");
        return;
    }
    gLogProc(gPluginNumber, MSGTYPE_DETAILS, (WCHAR*)u"Set PATH env variable.");
#endif
    return;
}

// variables and function to manage cache
std::vector<wcharstring> busyFolders;

struct PathFolderElement
{
    wcharstring path;
    std::vector<wcharstring> elementsCache;
};

std::vector<PathFolderElement> cacheOfFolders;

void getFolderPath(wcharstring wPath, wcharstring& folderPath)
{
    if(wPath.length() == 0) {
        folderPath = (WCHAR*)u"/";
        return;
    }
    size_t nPos = wPath.find_last_of((WCHAR*)u"/");
    folderPath = wPath.substr(0, nPos);
    if(folderPath == (WCHAR*)u"") folderPath = (WCHAR*)u"/";
}

void getFileName(wcharstring wPath, wcharstring& fileName)
{
    if(wPath.length() == 0) {
        fileName = (WCHAR*)u"";
        return;
    }
    size_t nPos = wPath.find_last_of((WCHAR*)u"/");
    if(nPos == std::string::npos) nPos = -1; // can crash the app if unchecked
    fileName = wPath.substr(nPos + 1);
}

PathFolderElement* getFolderCache(wcharstring folder) 
{
    if(folder.length() == 0) return NULL;
    auto it = std::find_if(
        cacheOfFolders.begin(),
        cacheOfFolders.end(),
        [folder](const PathFolderElement& item)
        {
            return item.path == folder;
        }
    );
    if(it == cacheOfFolders.end()) return NULL;
    return &(*it);
}

PathFolderElement* addFolderToCache(wcharstring folderPath) {
    if(folderPath.length() == 0) return NULL;
    if(folderPath == (WCHAR*)u"/") return NULL;
    // request list of items in folder as vector
    std::string commandString = UTF16toUTF8(
            wcharstring((WCHAR*)u"rclone lsf ").append(sanitize(folderPath.substr(1))).data()
        );
    std::vector<wcharstring> resultVector;
    if(!executeCommandAndReturnVector(commandString, resultVector)) return NULL;

    // add new element to cache and return reference
    PathFolderElement newCache;
    newCache.path = folderPath;
    newCache.elementsCache = resultVector;
    cacheOfFolders.push_back(newCache);
    return &(cacheOfFolders.back());
}

bool isItemInCache(PathFolderElement *cache, wcharstring itemPath)
{
    if(cache == NULL) return false;
    auto it = std::find_if(
        cache->elementsCache.begin(),
        cache->elementsCache.end(),
        [itemPath](wcharstring &path) {
            return itemPath == path;
        }
    );
    if(it == cache->elementsCache.end()) return false;
    else return true;
}

#endif