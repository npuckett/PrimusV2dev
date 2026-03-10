# Plan Overview
Inside V1Tool there is both processing and arduino code for a system that sends realtime color data from processing to arduino via artnet wifi. The tool works, but is limited. I would like outline a multi-stage plan for updating the tools to work with the new V2 hardware that is shown in hardwareTest.ino and outlined in the repo

# Hardware changes from V1
## New Flexibility
V1 had a hardwired setup for a grid and an led strip. V2 makes this a modular system where strips or grids can be plugged into 1 of the 3 outputs. Yes the board can do more than three, but we are limiting it to 3 outputs. So the number of outputs must be adjustable, but also the type.
### Output Types
For simplicity we are limiting the outputs to 3 type
- Short LED Strip 33 pixels
- Long LED Strip 72 pixels
- LED Grid 4X8

These types may be changed later, so make it easy to update. also provide and option to note if the strip is RGB or RGBW. Assume the long strip default is RGBW

This will require that the interface can target a specific port 0,1,2 on the device.

## New Hardware
### NeoPXL8
The new setup uses teh neopxl8 hardware and library for control

### TFT Screen
The TFT Screen is very useful when setting up, testing and operating the device as it can give clear feedback about connection status, IP address etc. plan a method to implement it thoughtfully

### Hardware buttons
There are several ways that these buttons could be used for input and interaction. Consider the best functionality. 

# Things to Keep from V1
## Stick with ArtNet Wifi
We want to keep working with the opend Artnet Standard for this project. However, investigate how the code can be improved for greater reliability and efficiency. Also potentially looking into how it can provide feedback from the board to the controller.

## New Software to consider
I created an arduino library webgui https://github.com/npuckett/WebGUI that provides a lightweight interface for setting up board properties. Consider how this could be used in conjunction with local control on each device


## Short Term Goal
Update the controller so it can send data to the outputs on the current control setup without making major changes

## Medium Term Goal
Create a clear and standardized API for this setup based around Artnet that would allow color data to be easily sent from any programming language that can send artnet.

## Long Term Goal 
Update the sending tool. This may involve moving away from Processing. I can see the potential in making this a web-based tool with a simple local relay to send the data.
### New Features Needed
#### Better Support for Multiple Devices
Cleaner way to send data to multiple devices at once. Currently this requires opening multiple instances of the interface, which isn't ideal. Find a UI scheme for managing multiple.

#### Clip Library / Editor / orchestrator
The overall goal would be to create a look that is tested in realtime that is then saved into a look library. From there they can be combined with others for a scene and then be easily sent to a particular device. The assumption is that in a performance this could be triggered by an operator.