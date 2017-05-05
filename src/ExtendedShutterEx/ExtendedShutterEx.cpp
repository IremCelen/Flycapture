//=============================================================================
// Copyright � 2008 Point Grey Research, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of
// Point Grey Research, Inc. ("Confidential Information"). You shall not
// disclose such Confidential Information and shall use it only in
// accordance with the terms of the "License Agreement" that you
// entered into with PGR in connection with this software.
//
// UNLESS OTHERWISE SET OUT IN THE LICENSE AGREEMENT, THIS SOFTWARE IS
// PROVIDED ON AN �AS-IS� BASIS AND POINT GREY RESEARCH INC. MAKES NO
// REPRESENTATIONS OR WARRANTIES ABOUT THE SOFTWARE, EITHER EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY IMPLIED WARRANTIES OR
// CONDITIONS OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
// NON-INFRINGEMENT. POINT GREY RESEARCH INC. SHALL NOT BE LIABLE FOR ANY
// DAMAGES, INCLUDING BUT NOT LIMITED TO ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES, OR ANY LOSS OF PROFITS,
// REVENUE, DATA OR DATA USE, ARISING OUT OF OR IN CONNECTION WITH THIS
// SOFTWARE OR OTHERWISE SUFFERED BY YOU AS A RESULT OF USING, MODIFYING
// OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================
//=============================================================================
// $Id: ExtendedShutterEx.cpp 300855 2016-09-30 22:48:39Z erich $
//=============================================================================

#include "stdafx.h"

#include "FlyCapture2.h"
#include <bitset>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace FlyCapture2;
using namespace std;

enum ExtendedShutterType
{
    NO_EXTENDED_SHUTTER,
    DRAGONFLY_EXTENDED_SHUTTER,
    GENERAL_EXTENDED_SHUTTER
};

void PrintBuildInfo()
{
    FC2Version fc2Version;
    Utilities::GetLibraryVersion(&fc2Version);

    ostringstream version;
    version << "FlyCapture2 library version: " << fc2Version.major << "."
            << fc2Version.minor << "." << fc2Version.type << "."
            << fc2Version.build;
    cout << version.str() << endl;

    ostringstream timeStamp;
    timeStamp << "Application build date: " << __DATE__ << " " << __TIME__;
    cout << timeStamp.str() << endl << endl;
}

void PrintCameraInfo(CameraInfo *pCamInfo)
{
    cout << endl;
    cout << "*** CAMERA INFORMATION ***" << endl;
    cout << "Serial number - " << pCamInfo->serialNumber << endl;
    cout << "Camera model - " << pCamInfo->modelName << endl;
    cout << "Camera vendor - " << pCamInfo->vendorName << endl;
    cout << "Sensor - " << pCamInfo->sensorInfo << endl;
    cout << "Resolution - " << pCamInfo->sensorResolution << endl;
    cout << "Firmware version - " << pCamInfo->firmwareVersion << endl;
    cout << "Firmware build time - " << pCamInfo->firmwareBuildTime << endl
         << endl;
}

void PrintError(Error error) { error.PrintErrorTrace(); }

int main(int /*argc*/, char ** /*argv*/)
{
    PrintBuildInfo();

    const int k_numImages = 5;

    Error error;

    BusManager busMgr;
    unsigned int numCameras;
    error = busMgr.GetNumOfCameras(&numCameras);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    cout << "Number of cameras detected: " << numCameras << endl;

    if (numCameras < 1)
    {
        cout << "Insufficient number of cameras... exiting" << endl;
        return -1;
    }

    PGRGuid guid;
    error = busMgr.GetCameraFromIndex(0, &guid);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }    

    // Connect to a camera
	Camera cam;
    error = cam.Connect(&guid);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    // Get the camera information
    CameraInfo camInfo;
    error = cam.GetCameraInfo(&camInfo);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    PrintCameraInfo(&camInfo);

    // Check if the camera supports the FRAME_RATE property
    PropertyInfo propInfo;
    propInfo.type = FRAME_RATE;
    error = cam.GetPropertyInfo(&propInfo);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    ExtendedShutterType shutterType = NO_EXTENDED_SHUTTER;

    if (propInfo.present == true)
    {
        // Turn off frame rate
        Property prop;
        prop.type = FRAME_RATE;
        error = cam.GetProperty(&prop);
        if (error != PGRERROR_OK)
        {
            PrintError(error);
            return -1;
        }

        prop.autoManualMode = false;
        prop.onOff = false;

        error = cam.SetProperty(&prop);
        if (error != PGRERROR_OK)
        {
            PrintError(error);
            return -1;
        }

        shutterType = GENERAL_EXTENDED_SHUTTER;
    }
    else
    {
        // Frame rate property does not appear to be supported.
        // Disable the extended shutter register instead.
        // This is only applicable for Dragonfly.

        const unsigned int k_extendedShutter = 0x1028;
        unsigned int extendedShutterRegVal = 0;

        error = cam.ReadRegister(k_extendedShutter, &extendedShutterRegVal);
        if (error != PGRERROR_OK)
        {
            PrintError(error);
            return -1;
        }

        std::bitset<32> extendedShutterBS((int)extendedShutterRegVal);
        if (extendedShutterBS[31] == true)
        {
            // Set the camera into extended shutter mode
            error = cam.WriteRegister(k_extendedShutter, 0x80020000);
            if (error != PGRERROR_OK)
            {
                PrintError(error);
                return -1;
            }
        }
        else
        {
            cout << "Frame rate and extended shutter are not supported... "
                    "exiting"
                 << endl;
            return -1;
        }

        shutterType = DRAGONFLY_EXTENDED_SHUTTER;
    }

    // Set the shutter property of the camera
    Property prop;
    prop.type = SHUTTER;
    error = cam.GetProperty(&prop);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    prop.autoManualMode = false;
    prop.absControl = true;

    const float k_shutterVal = 3000.0;
    prop.absValue = k_shutterVal;

    error = cam.SetProperty(&prop);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    cout << "Shutter time set to " << fixed << setprecision(2) << k_shutterVal
         << "ms" << endl;

    // Enable timestamping
    EmbeddedImageInfo embeddedInfo;

    error = cam.GetEmbeddedImageInfo(&embeddedInfo);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    if (embeddedInfo.timestamp.available != 0)
    {
        embeddedInfo.timestamp.onOff = true;
    }

    error = cam.SetEmbeddedImageInfo(&embeddedInfo);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    // Start the camera
    error = cam.StartCapture();
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    for (int i = 0; i < k_numImages; i++)
    {
        Image image;
        error = cam.RetrieveBuffer(&image);
        if (error != PGRERROR_OK)
        {
            PrintError(error);
            return -1;
        }

        TimeStamp timestamp = image.GetTimeStamp();
        cout << "TimeStamp [" << timestamp.cycleSeconds << " "
             << timestamp.cycleCount << "]" << endl;
    }

    // Stop capturing images
    error = cam.StopCapture();
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    // Set the camera back to its original state

    prop.type = SHUTTER;
    error = cam.GetProperty(&prop);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    prop.autoManualMode = true;

    error = cam.SetProperty(&prop);
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    if (shutterType == GENERAL_EXTENDED_SHUTTER)
    {
        Property prop;
        prop.type = FRAME_RATE;
        error = cam.GetProperty(&prop);
        if (error != PGRERROR_OK)
        {
            PrintError(error);
            return -1;
        }

        prop.autoManualMode = true;
        prop.onOff = true;

        error = cam.SetProperty(&prop);
        if (error != PGRERROR_OK)
        {
            PrintError(error);
            return -1;
        }
    }
    else if (shutterType == DRAGONFLY_EXTENDED_SHUTTER)
    {
        const unsigned int k_extendedShutter = 0x1028;
        unsigned int extendedShutterRegVal = 0;

        error = cam.ReadRegister(k_extendedShutter, &extendedShutterRegVal);
        if (error != PGRERROR_OK)
        {
            PrintError(error);
            return -1;
        }

        std::bitset<32> extendedShutterBS((int)extendedShutterRegVal);
        if (extendedShutterBS[31] == true)
        {
            // Set the camera into extended shutter mode
            error = cam.WriteRegister(k_extendedShutter, 0x80000000);
            if (error != PGRERROR_OK)
            {
                PrintError(error);
                return -1;
            }
        }
    }

    // Disconnect the camera
    error = cam.Disconnect();
    if (error != PGRERROR_OK)
    {
        PrintError(error);
        return -1;
    }

    cout << "Done! Press Enter to exit..." << endl;
    cin.ignore();

    return 0;
}