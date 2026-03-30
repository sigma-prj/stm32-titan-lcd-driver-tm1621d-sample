STM32 F030R8 NUCLEO Board Sample Application - How to drive LCD using Titan TM1621D driver
==============

This project represents sample STM32 F030R8 NUCLEO board application,
showing how to drive 7-Segments LCD using Titan TM1621D driver

The following methods are implemented within main application's source code:
 - ***submit_bits_array*** - data transfer to TM1621D LCD driver using pre-defined output PINs
 - ***submit_cmd*** - command submission to TM1621D LCD driver
 - ***submit_data*** - data submission to TM1621D LCD driver
 - ***put_matrix_symbol*** - specific symbol placement into matrix buffer
 - ***display_ methods*** - ability to display specific symbols on LCD

Requirements and Dependencies
-----------------------------

This application is targeted to be built and executed using STM32 CubeIDE.
New CubeIDE project needs to be created and configured as F030R8 NUCLEO board app.
Corresponding pins needs to be setup in .ioc file using CubeMX application.
Besides F030R8 NUCLEO pins set by default, the following should be configured:
 - ***PA8*** - GPIO Output (will be used as TM1621D Write signal)
 - ***PA15*** - GPIO Output (will be used as TM1621D Data signal)
 - ***PA11*** - GPIO Output (will be used as TM1621D CS signal)

Once the project is created, the main file at Core/Src/ subfolder can be substituted by
main.c file from repo.

KiCad Design Files
-----------------------------

In order to run this application with specified Titan LCD Driver
and presented LCD chip - special adapter PCB can be used (pluggable into STM32 F030R8 NUCLEO board)
KiCad schema and PCB layout design files can be found under 'kicad' subfolder.

More Details
-----------------------------

More information can be found at corresponding YouTube video:
https://www.youtube.com/watch?v=M4Ayx5ZT4EI

