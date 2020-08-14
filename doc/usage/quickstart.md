1.  Download or clone this repository

2.  Copy or import the ado_chip_175x_6x Folder as project into your (empty/new)
    workspace.

3.  Use MCUXpresso New project wizard.

    1.  Select LPC176x as Target CPU

    2.  Do NOT choose a board for your project! (we are not planning to support
        this)

    3.  Select LPCOpen - C Project (not tested with other types yet)

    4.  After naming your project you are asked to “Select an LPCOpen Chip
        library project within the current workspace”. Click browse and select
        the ado_chip_175x_6x project iso of the suggested lpc_chip_175x_6x
        project.

    5.  Continue and finish the wizard.

4.  Your newly generated project will be build in DEBUG configuration.  
    To link against the ado_chip_175x_6x build output you have to check, that either
    1.  the ado_chip_175x_6x has and compiles also in DEBUG configuration.
    2.  at the time of this doku, there was no debug config available but 2 testconfigs named "TestConfig_1" and "TestConfig_2"
        1. to use one of these goto YourNewProject - Properties - C/C++Build - Settings - ToolSettings
        2. under MCU-Linker - Libraries edit the existing Library Search Path to:        "${workspace_loc:/ado_chip_175x_6x/TestConfig_2}" 
          
    
4.  You can start developing and debugging your project in MCUXpresso as
    normally....
