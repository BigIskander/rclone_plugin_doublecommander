# Rclone plugin for Double Commander

[По русски](README_RUS.md)

This is the plugin for file manager [Double Commander](https://doublecmd.sourceforge.io), plugin can work with Double Commander in Windows, macOS and Linux operating systems.

In Windows this plugin is also compatible with file manager [Total Commander](https://www.ghisler.com).

Plugin allows Double Commander (or Total Commander) to interact with files located in cloud storages by using rclone program. [Rclone](https://rclone.org) is a command-line program, allowing to work with cloud storages (such as Google disk, Yandex disk, Dropbox etc.). From techinical point of view this plugin is a wrapper around rclone program.

## How to install and setup

### 1. Install and configure Rclone:

Examples how you can install and configure Rclone in different operating systems.

#### In Windows operating system

1. Download Rclone from their official website https://rclone.org , unpack and place it in any convinient folder on the computer, for example, *C:/rclone/*
2. In folder with Rclone program open command line, execute `./rclone.exe config` command and by following suggestions configure cloud storages.

#### In macOS operating system

1. Easiest way to install Rclone is to istall it via [homebrew](https://brew.sh) : `brew install rclone` .
2. In command line execute `rclone config` command and by following suggestions configure cloud storages.

#### In Linux operating system

1. Install Rclone via your Linux version's package manager, for exampe, for distrebutions based on Debian: `sudo apt install rclone` .
2. In command line execute `rclone config` command and by following suggestions configure cloud storages.

### 2. Add plugin in Double Commander (or Total Commander)

#### Instruction for Double Commander

1. Download plugin (file *.wfx64) from Releases page of this repository or get this file by compiling plugin from source code.
2. In Double Commander open settings window: ***Configuration > Options***
3. In settings window in side menu select ***Plugins WFX***
4. Click ***Add*** button and choose path to plugin file (file with **.wfx64** extension).

#### Instruction for Total Commander

1. Download plugin (file *.wfx64) from Releases page of this repository or get this file by compiling plugin from source code.
2. In Total Commander open settings window: ***Configuration > Options***
3. In settings window in side menu select ***Plugins***
4. In ***File system pluginx(.WFX)*** menu item click ***Configure*** button, then in new window click ***Add*** button and choose path to file plugin (file with **.wfx64** extension). 

### 3. Configure plugin

1. In Double Commander open **//** (**Open VFS list**) tab. In **vfs:** tab open **Rclone** folder. Similarly in Total Commander from drop down menu choose **Network Neighborhood** item, then, open **Rclone** folder.

Next (number 2) item in this instruction is differs depending on your operating system. Examples of configuring plugin in different operating systems.

#### In Windows operating system

2. In plugin's root folder, double click (open) item with name **&lt;Settings&gt;**. Then double click item with name **&lt;0_edit_rclone_executable_binary_path&gt;** and in opened window write path to rclone program, for example `C:\rclone\rclone.exe` and click **Ok**.

#### In macOS operating system

2. In plugin's root folder, double click (open) item with name **&lt;Settings&gt;**. Then double click item with name **&lt;0_edit_rclone_executable_binary_path&gt;** and in opened window write path to rclone program, for example `/opt/homebrew/bin/rclone` (if rclone was installed via homebrew) and click **Ok**.

#### In Linux operating system

2. If you installed rclone via your package manager, you can skip this part. Otherwise, in plugin's root folder, double click (open) item with name **&lt;Settings&gt;**. Then double click item with name **&lt;0_edit_rclone_executable_binary_path&gt;** and in opened window write path to rclone program and click **Ok**.

Next (number 3) item in this istruction is the same for all operating systems.

3. Return back to plugin's root folder (by double clicking **..** item). If everything is configured correctly, then alongside with item **&lt;Settings&gt;** you should see list of cloud storages (as separate folders), which was configured in Rclone earlier.

## How to use

In Double Commander open **//** (**Open VFS list**) tab. In **vfs:** tab open **Rclone** folder. Similarly in Total Commander from drop down menu choose **Network Neighborhood** item, then, open **Rclone** folder.

If earlier everything was configured correctly, then in plugin's root folder you should see list of cloud storages (as separate folders), which was configured in Rclone earlier.

## For developers

Instructions differs depending on operating system.

### In Windows

To compile the plugin, MSVC compiler is used. To compile the plugin you need to:

1. Install [Visual Studio Build Tools]( https://visualstudio.microsoft.com/ru/downloads/?q=build+tools)
2. Run compilation script:
    ```
    ./compile.bat
    ```

### In macOS

To compile the plugin, clang++ compiler included in Xcode Command Line Developer Tools is used. To compile the plugin you need to:

1. Install Xcode Command Line Developer Tools:
    ```
    xcode-select --install
    ```
2. Mark compilation script as executable file and run it:
    ```
    chmod +x compile.sh
    ./compile.sh
    ```

### In Linux

To compile the plugin, clang++ compiler is used. To compile the plugin you need to:

1. Install clang compiler via package manager of your Linux distribution, for example, in distributions based on Debian:
    ```
    sudo apt install clang
    ```
2. Mark compilation script as executable file and run it:
    ```
    chmod +x compile.sh
    ./compile.sh
    ```

