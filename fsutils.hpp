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

CSimpleIniA ini;

wcharstring sanitize(wcharstring value) 
{
    wcharstring sanitizedValue = (WCHAR*)u"\"";
    for(int i = 0; i < value.size(); i++) {
#if defined(_WIN32) || defined(_WIN64)
        // escape special characters for Windows shell
        // https://zenn.dev/tryjsky/articles/0610b2f32453e7?locale=en#escaping-symbols
        if(value.at(i) == (WCHAR)u'%') {
            sanitizedValue.push_back((WCHAR)u'"');
            sanitizedValue.push_back((WCHAR)u'^');
            sanitizedValue.push_back(value.at(i));
            sanitizedValue.push_back((WCHAR)u'"');
        } else {
            sanitizedValue.push_back(value.at(i));
        }
#else
        // escape special characters for POSIX compatible shell
        // https://www.gnu.org/software/bash/manual/html_node/Double-Quotes.html
        if(
            value.at(i) == (WCHAR)u'$' || value.at(i) == (WCHAR)u'`' || value.at(i) == (WCHAR)u'"' ||
            value.at(i) == (WCHAR)u'\\' || value.at(i) == (WCHAR)u'!'
        ) {
            sanitizedValue.push_back((WCHAR)u'\\');
        }
        sanitizedValue.push_back(value.at(i));
#endif  
    }
    sanitizedValue.push_back((WCHAR)u'"');
    return sanitizedValue;
}

#if  defined(_WIN32) || defined(_WIN64)
// Windows version of functions used to interact with shell

#define BUFSIZE 4096 

wchar_t rcloneExePath[MAX_PATH];

    struct commandOutput
    {
        HANDLE stdoutPipe;
        HANDLE process;
        HANDLE thread;
        commandOutput() {
            this->stdoutPipe = NULL;
            this->process = NULL;
            this->thread = NULL;
        }
        commandOutput(HANDLE stdoutPipe, HANDLE process, HANDLE thread) {
            this->stdoutPipe = stdoutPipe;
            this->process = process;
            this->thread = thread;
        }
    };

    // resourses used as examples
    // https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes
    // https://stackoverflow.com/questions/7063859/c-popen-command-without-console
    // https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
    // https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags
    

    bool executeCommand(std::string command, commandOutput &output, bool isCmd)
    { 
        // Set the bInheritHandle flag so pipe handles are inherited.
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        // create stdout pipe and set handle
        HANDLE StdOutHandles[2]; 
        if(CreatePipe(&StdOutHandles[0], &StdOutHandles[1], &sa, 0) == 0) 
        {
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"Failed to create pipe.");
            return false;
        }
        if (!SetHandleInformation(StdOutHandles[0], HANDLE_FLAG_INHERIT, 0))
        {
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"Failed to set handle.");
            return false;
        }

        // attach stdout pipe to startup info
        STARTUPINFOW siStartInfo;
        memset(&siStartInfo, 0, sizeof(siStartInfo));
        siStartInfo.dwFlags = STARTF_USESTDHANDLES;
        siStartInfo.hStdOutput = StdOutHandles[1];

        wchar_t* exePath = rcloneExePath;
        if(isCmd || wcslen(rcloneExePath) == 0) exePath = NULL;
        // create process
        PROCESS_INFORMATION piProcInfo;
        memset(&piProcInfo, 0, sizeof(piProcInfo));
        std::wstring commandW(UTF8toUTF16(command).c_str()); // make copy of the value
        BOOL isProcessCreated =  CreateProcessW(
            exePath,                                // rclone.exe or just shell
            const_cast<wchar_t*>(commandW.data()),                               // rclone command
            NULL,                                   // Process handle not inheritable
            NULL,                                   // Thread handle not inheritable
            TRUE,                                   // Set handle inheritance to ???
            CREATE_PRESERVE_CODE_AUTHZ_LEVEL | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,  // Creation flags
            NULL,                                   // Use parent's environment block
            NULL,                                   // Use parent's starting directory 
            &siStartInfo,                           // Pointer to STARTUPINFO structure
            &piProcInfo                             // Pointer to PROCESS_INFORMATION structure
        );

        if(!isProcessCreated)
        {
            wcharstring errorString = (WCHAR*)u"Failed to create process. Exe path: ";
            if(exePath == NULL) errorString.append((WCHAR*)u"NULL, ");
            else errorString.append(exePath).append((WCHAR*)u", ");
            errorString.append((WCHAR*)u"command: ").append(commandW);
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)errorString.data());
            return false;
        }

        if(!CloseHandle(StdOutHandles[1]))
        {
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"Failed to close handle.");
            return false;
        }

        output.stdoutPipe = const_cast<HANDLE>(StdOutHandles[0]);
        output.process = const_cast<HANDLE>(piProcInfo.hProcess);
        output.thread = const_cast<HANDLE>(piProcInfo.hThread);

        return true; 
    }

    int exitProcess(commandOutput &output, std::string &commandString) {
        // wait for process exit and get exit code
        WaitForSingleObject(output.process, INFINITE);  // wait for process to end
        DWORD dwExitCode = 0;
        ::GetExitCodeProcess(output.process, &dwExitCode);
        if(dwExitCode != 0)
        {
            // show message if error occured
            gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
                (WCHAR*) wcharstring((WCHAR*)u"Command (")
                    .append(UTF8toUTF16(commandString))
                    .append((WCHAR*)u") exited with status ")
                    .append(int_to_wcharstring(dwExitCode)).data()
            );
        }
        // close handles
        CloseHandle(output.stdoutPipe); 
        CloseHandle(output.process);
        CloseHandle(output.thread);
        return dwExitCode;
    }

    // execute shell command and return stdout as Vector
    bool executeCommandAndReturnVector(std::string commandString, std::vector<wcharstring> &resultVector, bool isCmd = false) 
    {
        // execute command
        commandOutput output;
        if(!executeCommand(commandString, output, isCmd)) return false;
        // read from stdout
        CHAR chBuf[BUFSIZE];
        DWORD dwRead;
        while(ReadFile(output.stdoutPipe, chBuf, BUFSIZE, &dwRead, NULL)) 
        {
            if(dwRead == 0) break;
            std::string s(chBuf, dwRead);
            wcharstring itemName = UTF8toUTF16(s);
            if(std::isspace(itemName.at(itemName.size() - 1))) // delete last element if it is space
                itemName = itemName.substr(0, itemName.size() - 1);
            resultVector.push_back(itemName);
        }
        // exit process
        if(exitProcess(output, commandString) != 0) return false;
        return true;
    }

    // execute shell command and return stdout as text
    bool executeCommandAndReturnString(std::string commandString, std::string &resultString, bool isCmd = false)
    {
        // execute command
        commandOutput output;
        if(!executeCommand(commandString, output, isCmd)) return false;
        // read from stdout
        CHAR chBuf[BUFSIZE];
        DWORD dwRead;
        while(ReadFile(output.stdoutPipe, chBuf, BUFSIZE, &dwRead, NULL)) 
        {
            if(dwRead == 0) break;
            std::string s(chBuf, dwRead);
            resultString += s;
        }
        // exit process
        if(exitProcess(output, commandString) != 0) return false;
        return true;
    }

    // execute shell command and return nothing
    bool executeCommand2(std::string commandString, bool isCmd = false) 
    {
        // execute command
        commandOutput output;
        if(!executeCommand(commandString, output, isCmd)) return false;
        // exit process
        if(exitProcess(output, commandString) != 0) return false;
        return true;
    }
#else
// Linux and macOS version of functions used to interact with shell

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
            if (WIFEXITED(status)) {
                status = WEXITSTATUS(status);
                // show message if error occured
                gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
                    (WCHAR*) wcharstring((WCHAR*)u"Command (")
                        .append(UTF8toUTF16(commandString))
                        .append((WCHAR*)u") exited with status ")
                        .append(int_to_wcharstring(status)).data()
                );
            } else {
                // show message if error occured
                gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
                    (WCHAR*) wcharstring((WCHAR*)u"Command (")
                        .append(UTF8toUTF16(commandString))
                        .append((WCHAR*)u") was terminated").data()
                );
            }
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

    // execute shell command and return nothing
    bool executeCommand2(std::string commandString) 
    {
        std::unique_ptr<FILE, decltype(&pclose)> pipe = executeCommand(commandString); 
        if (!pipe) return false; // popen failed
        // close popen
        if(popenClose(&pipe, commandString) != 0) return false;
        return true;
    }
#endif

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
#if  defined(_WIN32) || defined(_WIN64)
    std::vector<wcharstring> resultVector;
    if(!executeCommandAndReturnVector("where rclone", resultVector, true))
    {
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, (WCHAR*)u"Failed to get rclone.exe executable path from shell.");
        return;
    }
    if(!file_exists(UTF16toUTF8(resultVector[0].data())))
    {
        // it's highly unlikely though, whatever
        gLogProc(gPluginNumber, MSGTYPE_IMPORTANTERROR, 
            (WCHAR*)wcharstring((WCHAR*)u"Failed to get rclone.exe executable path from shell. File ")
            .append(resultVector[0])
            .append((WCHAR*)u" does not exists.")
            .data()
        );
        return;
    }
    wchar_t* exePath = const_cast<wchar_t *>(resultVector[0].c_str());
    memcpy(rcloneExePath, exePath, MAX_PATH);
    gLogProc(gPluginNumber, MSGTYPE_DETAILS, (WCHAR*)u"Got path to rclone.exe executable from shell.");
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