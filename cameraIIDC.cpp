//
//  cameraIIDC.cpp
//  IIDC
//
//  Created by roro on 12/06/17.
//  Copyright © 2017 RTI-Zone. All rights reserved.
//

#include "cameraIIDC.h"


CCameraIIDC::CCameraIIDC()
{

    m_pframeBuffer = NULL;
    m_ptDc1394Lib = NULL;
    m_ptDcCamera = NULL;
    m_bConnected = false;
    m_bDeviceIsUSB = false;
    m_bFrameAvailable = false;
    m_cameraGuid = 0;
    m_bAbort = false;
    m_nBitsPerPixel = 0;
    m_bNeedDepthFix = false;

    // camera features default value.
    m_nCurrentHue = 0;
    m_nBlue = 0;
    m_nRed = 0;
    m_nCurrentBrightness = 0;
    m_nCurrentGama = 0;
    memset(m_szCameraName,0,BUFFER_LEN);
}

CCameraIIDC::~CCameraIIDC()
{

    if(m_pframeBuffer)
        free(m_pframeBuffer);

    if(m_ptDcCamera)
        dc1394_camera_free(m_ptDcCamera);

    if(m_ptDc1394Lib)
        dc1394_free(m_ptDc1394Lib);

}

int CCameraIIDC::Connect(uint64_t cameraGuid)
{
    int nErr = SB_OK;
    uint32_t PGR_byte_swap_register;
    int i;
    int nFrameSize;
    uint32_t nTemp;

    if(cameraGuid)
        m_cameraGuid = cameraGuid;
    else
        return ERR_DEVICENOTSUPPORTED;

    if (!m_ptDc1394Lib) {
        m_ptDc1394Lib = dc1394_new ();
        if (!m_ptDc1394Lib)
            m_ptDc1394Lib = NULL;
    }

    if(!m_ptDcCamera) {
        m_ptDcCamera = dc1394_camera_new(m_ptDc1394Lib, m_cameraGuid);
        if(!m_ptDcCamera)
            return ERR_NORESPONSE;
    }

    // get camera name
    snprintf(m_szCameraName, BUFFER_LEN, "%s", m_ptDcCamera->model);
    printf("m_ptDcCamera->model = %s\n",m_ptDcCamera->model);
    printf("m_szCameraName = %s\n",m_szCameraName);

    // set device speed.
    if(!m_bDeviceIsUSB) {
        if (m_ptDcCamera->bmode_capable==DC1394_TRUE) {
            // switch to 1394b mode (aka 800Mbps)
            nErr =dc1394_video_set_operation_mode(m_ptDcCamera, DC1394_OPERATION_MODE_1394B);
            if(!nErr) {
                nErr = dc1394_video_set_iso_speed(m_ptDcCamera, DC1394_ISO_SPEED_800);
                if(nErr) {
                    printf("Error setting mode to 800Mbps\n");
                    return ERR_COMMANDNOTSUPPORTED;
                }
                printf("800 Mbps mode\n");
            }
            else {
                nErr = dc1394_video_set_iso_speed(m_ptDcCamera, DC1394_ISO_SPEED_400);
                printf("400 Mbps mode\n");
            }
        }
        else {
            nErr = dc1394_video_set_iso_speed(m_ptDcCamera, DC1394_ISO_SPEED_400);
        }
    }
    else {
        nErr = dc1394_video_set_iso_speed(m_ptDcCamera, DC1394_ISO_SPEED_400);
        printf("USB mode\n");
    }

    if(nErr)
        return ERR_COMMANDNOTSUPPORTED;

    m_bNeedSwap = true;
    // if vendor is PGR, set the endianness to little endian as we're on Intel CPU
    if (m_ptDcCamera->vendor_id == 45213) {
        // let's try to set the byte ordering to little endian
        //      PGR should have a register to set the endianness :)
        //      register address is 0x1048
        //      values are :
        //          0x80000000 : little endian
        //          0x80000001 : big endian
        //
        nErr = dc1394_get_control_registers (m_ptDcCamera, 0x1048, &PGR_byte_swap_register, 1);
        if (!nErr) {
            if (PGR_byte_swap_register == 0x80000001) {
                PGR_byte_swap_register = 0x80000000;
                nErr = dc1394_set_control_registers (m_ptDcCamera, 0x1048, &PGR_byte_swap_register, 1);
                printf("PGR little endian mode on\n");
            }
            m_bNeedSwap = false;
        }
        else
            return ERR_COMMANDNOTSUPPORTED;
    }

    // set video mode by chosng the biggest one for now.
    nFrameSize = 0;
    nErr = dc1394_video_get_supported_modes(m_ptDcCamera, &m_tVideoModes);
    if(nErr)
        return ERR_COMMANDNOTSUPPORTED;

    for (i=m_tVideoModes.num-1;i>=0;i--) {
        dc1394video_mode_t vidMode = m_tVideoModes.modes[i];
        dc1394_get_image_size_from_video_mode(m_ptDcCamera, vidMode, &m_nWidth, &m_nHeight);
        printf("dc1394_get_image_size_from_video_mode for mode %d : Image sise is %d x %d\n", vidMode, m_nWidth, m_nHeight);
        if (nFrameSize < (m_nWidth*m_nHeight) && !bIsVideoFormat7(vidMode)) { // let's avoid format 7 for now.
            nFrameSize = m_nWidth*m_nHeight;
            m_tCurrentMode = vidMode;
            printf("Selecting mode %d\n", vidMode);
        }
    }

    /* format 7 mode requires packet size to be set .. to be tested.
     if(bIsVideoFormat7(m_tCurrentMode)) {
         nErr = dc1394_format7_get_recommended_packet_size(m_ptDcCamera, m_tCurrentMode, &m_nPacketSize);
         nErr = dc1394_format7_set_packet_size(m_ptDcCamera, m_tCurrentMode, m_nPacketSize);
         nErr = dc1394_format7_get_data_depth(m_ptDcCamera, m_tCurrentMode, &m_nBitsPerPixel);
         if(m_nBitsPerPixel == 16) 
            m_bBodeIs16bits = true;
         else
            m_bBodeIs16bits = false;
     }
     else { // not format 7
     */
    m_bModeIs16bits = bIs16bitMode(m_tCurrentMode);
    if(m_bModeIs16bits)
        m_nBitsPerPixel = 16;
    else {
        m_nBitsPerPixel = 8;
        m_bNeed8bitTo16BitExpand = true;
    }

    printf("bit depth = %d\n", m_nBitsPerPixel);

    // } // end of not format 7
    
    nErr = dc1394_video_set_mode(m_ptDcCamera, m_tCurrentMode);
    if(nErr)
        return ERR_COMMANDNOTSUPPORTED;

    printf("Mode selected : %d\n", m_tCurrentMode);
    nErr = dc1394_get_image_size_from_video_mode(m_ptDcCamera, m_tCurrentMode, &m_nWidth, &m_nHeight);
    printf("dc1394_get_image_size_from_video_mode : Image sise is %d x %d\n", m_nWidth, m_nHeight);

    // set framerate if we're not in Format 7
    if(!bIsVideoFormat7(m_tCurrentMode)) {
        // get slowest framerate
        nErr = dc1394_video_get_supported_framerates(m_ptDcCamera, m_tCurrentMode, &m_tFramerates);
        if(nErr)
            return ERR_COMMANDNOTSUPPORTED;

        m_tCurFrameRate=m_tFramerates.framerates[m_tFramerates.num-1];
        // m_tCurFrameRate = m_tFramerates.framerates[0];

        printf("m_tCurFrameRate = %d\n", m_tCurFrameRate);
        dc1394_video_set_framerate(m_ptDcCamera, m_tCurFrameRate);
    }
    else {
        printf("We're in Format_7, not setting frame rate\n");
    }

    // we need to check for all the mode.. but this is enough for testing for now.
    switch(m_tCurrentMode) {
        case DC1394_VIDEO_MODE_640x480_MONO8:
        case DC1394_VIDEO_MODE_800x600_MONO8:
        case DC1394_VIDEO_MODE_1024x768_MONO8:
        case DC1394_VIDEO_MODE_1280x960_MONO8:
        case DC1394_VIDEO_MODE_1600x1200_MONO8:
            m_nSamplesPerPixel = 1;
            m_nBitsPerSample = 8;
            break;

        case DC1394_VIDEO_MODE_640x480_MONO16:
        case DC1394_VIDEO_MODE_800x600_MONO16:
        case DC1394_VIDEO_MODE_1024x768_MONO16:
        case DC1394_VIDEO_MODE_1280x960_MONO16:
        case DC1394_VIDEO_MODE_1600x1200_MONO16:
            m_nSamplesPerPixel = 1;
            m_nBitsPerSample = 16;
            break;

        default: // let's asume RGB8 for now
            m_nSamplesPerPixel = 3;
            m_nBitsPerSample = 8;
            break;
    }

    // Use one shot mode
    nErr = dc1394_video_set_one_shot(m_ptDcCamera, DC1394_ON);
    if(nErr)
        return ERR_COMMANDNOTSUPPORTED;


    // debug
    // print current feature values.
    /*
    nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_hue);
    nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_brightness);
    nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_gamma);
    nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_white_balance);
    printf("========== Initial values ==========\n");
    nErr = dc1394_feature_get_all(m_ptDcCamera,&m_tFeatures);
    dc1394_feature_print_all(&m_tFeatures, stdout);
    printf("====================================\n");
    printf("====================================\n");
     */



    // set some basic hardcoded features values
    nTemp = 480;
    dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_FRAME_RATE, DC1394_FEATURE_MODE_MANUAL);
    nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_FRAME_RATE, nTemp );
    if(nErr)
        printf("Error setting DC1394_FEATURE_FRAME_RATE\n");

    nTemp = 510;
    dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_BRIGHTNESS, DC1394_FEATURE_MODE_MANUAL);
    nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_BRIGHTNESS, nTemp );
    if(nErr)
        printf("Error setting DC1394_FEATURE_BRIGHTNESS\n");

    nTemp = 310;
    dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_EXPOSURE, DC1394_FEATURE_MODE_MANUAL);
    nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_EXPOSURE, nTemp );
    if(nErr)
        printf("Error setting DC1394_FEATURE_EXPOSURE\n");

    nTemp = 1529;
    dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_SHARPNESS, DC1394_FEATURE_MODE_MANUAL);
    nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_SHARPNESS, nTemp );
    if(nErr)
        printf("Error setting DC1394_FEATURE_SHARPNESS\n");

    nTemp = 1024;
    dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_GAMMA, DC1394_FEATURE_MODE_MANUAL);
    nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_GAMMA, nTemp );
    if(nErr)
        printf("Error setting DC1394_FEATURE_GAMMA\n");

    nTemp = 286;
    dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_SHUTTER, DC1394_FEATURE_MODE_MANUAL);
    nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_SHUTTER, nTemp );
    if(nErr)
        printf("Error setting DC1394_FEATURE_SHUTTER\n");

    nTemp = 65;
    dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_GAIN, DC1394_FEATURE_MODE_MANUAL);
    nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_GAIN, nTemp );
    if(nErr)
        printf("Error setting DC1394_FEATURE_GAIN\n");


    /*
    printf("========== New values ==========\n");
    nErr = dc1394_feature_get_all(m_ptDcCamera,&m_tFeatures);
    dc1394_feature_print_all(&m_tFeatures, stdout);
    printf("====================================\n");
    printf("====================================\n");
     */
    
    m_bConnected = true;
    return nErr;
}

void CCameraIIDC::Disconnect()
{
    if(m_ptDcCamera) {
        dc1394_capture_stop(m_ptDcCamera);
        dc1394_camera_reset(m_ptDcCamera);
        dc1394_reset_bus(m_ptDcCamera);
        dc1394_camera_free(m_ptDcCamera);
        m_ptDcCamera = NULL;
    }

    if(m_pframeBuffer) {
        free(m_pframeBuffer);
        m_pframeBuffer = NULL;
    }
    m_bConnected = false;

}

void CCameraIIDC::setCameraGuid(uint64_t tGuid)
{
    m_cameraGuid = tGuid;
    printf("[CCameraIIDC::setCameraGuid] m_cameraGuid = %llx\n", m_cameraGuid);
}

void CCameraIIDC::getCameraGuid(uint64_t &tGuid)
{
    tGuid = m_cameraGuid;
}

void CCameraIIDC::getCameraName(char *pszName, int nMaxStrLen)
{
    strncpy(pszName, m_szCameraName, nMaxStrLen);
}

int CCameraIIDC::listCamera(std::vector<camera_info_t>  &cameraIdList)
{
    int nErr = SB_OK;
    dc1394camera_list_t *pList = NULL;
    dc1394camera_t *camera;
    camera_info_t   tCameraInfo;
    int i = 0;

    if (!m_ptDc1394Lib) {
        m_ptDc1394Lib = dc1394_new ();
        if (!m_ptDc1394Lib) {
            m_ptDc1394Lib = NULL;
            return ERR_POINTER;
        }
    }

    cameraIdList.clear();
    // list camera connected to the syste,
    nErr = dc1394_camera_enumerate (m_ptDc1394Lib, &pList);
    if(nErr) {
        return ERR_UNKNOWNRESPONSE;
    }

    for(i=0; i<pList->num; i++) {
        tCameraInfo.uuid = pList->ids[i].guid;
        printf("Found camera with UUID %llx\n", pList->ids[i].guid);
        if(pList->ids[i].guid == m_cameraGuid && m_ptDcCamera) {
            printf("current camera %s found\n", m_szCameraName);
            strncpy(tCameraInfo.model, m_szCameraName, BUFFER_LEN);
            cameraIdList.push_back(tCameraInfo);
        }
        else {
            camera = dc1394_camera_new (m_ptDc1394Lib, pList->ids[i].guid);
            if (camera) {
                printf("camera %u : %s %s\n", i, camera->vendor, camera->model);
                strncpy(tCameraInfo.model, camera->model, BUFFER_LEN);
                dc1394_camera_free(camera);
                cameraIdList.push_back(tCameraInfo);
            }
        }
    }
    dc1394_camera_free_list (pList);

    return nErr;
}


void CCameraIIDC::updateFrame(dc1394video_frame_t *frame)
{
    uint32_t w,h;
    uint32_t image_bytes;
    uint16_t pixel_data;
    unsigned char *old_buffer=NULL;
    unsigned char *new_buffer=NULL;
    unsigned char *expandBuffer = NULL;
    uint8_t *pixBuffer = NULL;

    old_buffer = frame->image;
    w = frame->size[0];
    h = frame->size[1];
    image_bytes = frame->image_bytes;

    if(m_pframeBuffer) {
        free(m_pframeBuffer);
        m_pframeBuffer = NULL;
    }


    if(m_bNeed8bitTo16BitExpand) {
        pixBuffer = frame->image;
        // allocate new buffer
        printf("Source buffer size : %d bytes\n", image_bytes);
        image_bytes = image_bytes*2;
        printf("Allocation expandBuffer : %d bytes\n", image_bytes);
        expandBuffer = (unsigned char *)malloc(image_bytes);
        // copy data and expand to 16 bit (also takes care of byte swap)
        printf("Expending to 16 bits.\n");
        for(int i=0; i<image_bytes/2; i++){
            expandBuffer[(i*2)+1] = pixBuffer[i];
            expandBuffer[(i*2)] = 0x0;
        }
        m_bNeedSwap = false;
        frame->image = expandBuffer;
    }

    printf("Allocating buffer of %d bytes\n", image_bytes);
    m_pframeBuffer = (unsigned char *)malloc(image_bytes);

    if(m_bNeedSwap) {
        printf("Byte swapping buffer\n");
        new_buffer = (unsigned char *)malloc(image_bytes);
        //      do some byte swapping using swab
        swab(frame->image, m_pframeBuffer, image_bytes);
        frame->image = m_pframeBuffer;
    }

    // do we need to fix the bit depth ? -> yes I'm looking at you Sony...
    if(m_bNeedDepthFix && m_bModeIs16bits) {
        printf("Adjusting bit depth\n");
        // we need to cast to a 16bit int to be able to do the right bit shift
        uint16_t *pixBuffer16 = (uint16_t *)frame->image;
        for(int i=0; i<image_bytes/2; i++){
            pixel_data = pixBuffer16[i];
            pixel_data <<= m_nDepthBitLeftShift;
            pixBuffer16[i] = pixel_data;
        }
    }

    // copy image frame buffer to m_frameBuffer;
    printf("copying %d byte to internal buffer\n", image_bytes);

    memcpy(m_pframeBuffer, frame->image, image_bytes);
    m_bFrameAvailable = true;
    
    frame->image = old_buffer;

    if(new_buffer)
        free(new_buffer);

    if(expandBuffer)
        free(expandBuffer);
}

int CCameraIIDC::startCaputure()
{
    int nErr = SB_OK;

    if (m_ptDcCamera) {
        printf("Starting capture\n");
        m_bFrameAvailable = false;
        nErr = dc1394_capture_setup(m_ptDcCamera, 8, DC1394_CAPTURE_FLAGS_DEFAULT);
        if(nErr) {
            printf("Error dc1394_capture_setup\n");
            return ERR_CMDFAILED;
        }
        nErr = dc1394_video_set_transmission (m_ptDcCamera, DC1394_ON);
        if(nErr) {
            printf("Error dc1394_video_set_transmission\n");
            return ERR_CMDFAILED;
        }
    }
    m_bAbort = false;
    m_bFrameAvailable = false;
    return nErr;
}

int CCameraIIDC::stopCaputure()
{
    int nErr = SB_OK;
    if (m_ptDcCamera) {
        printf("Stopping capture\n");
        nErr = dc1394_video_set_transmission (m_ptDcCamera, DC1394_OFF);
        if(nErr)
            return ERR_CMDFAILED;
        nErr = dc1394_capture_stop(m_ptDcCamera);
        if(nErr)
            return ERR_CMDFAILED;
    }
    return nErr;
}

void CCameraIIDC::abortCapture(void)
{
    m_bAbort = true;
    stopCaputure();
}

int CCameraIIDC::getTemperture(double &dTEmp)
{
    // (Value)/10 -273.15 = degree C.
    int nErr;
    unsigned int nTempGoal;
    unsigned int nCurTemp;

    if (m_ptDcCamera) {
        nErr = dc1394_feature_temperature_get_value(m_ptDcCamera, &nTempGoal, &nCurTemp);
        if(nErr) {
            dTEmp = -100;
            return ERR_CMDFAILED;
        }
        dTEmp = (((double)nCurTemp)/10.0) - 273.15;
    }
    return nErr;
}


int CCameraIIDC::getWidth(int &nWidth)
{
    int nErr = SB_OK;

    if (m_ptDcCamera && m_bConnected) {
        nWidth = m_nWidth;
    } else
        printf("[getWidth] NOT CONNECTED\n");
    return nErr;
}

int CCameraIIDC::getHeight(int &nHeight)
{
    int nErr = SB_OK;

    if (m_ptDcCamera && m_bConnected) {
        nHeight = m_nHeight;
    } else
        printf("[getHeight] NOT CONNECTED\n");

    return nErr;
}

int CCameraIIDC::setROI(int nLeft, int nTop, int nRight, int nBottom)
{
    int nErr = ERR_COMMANDNOTSUPPORTED;

    if(bIsVideoFormat7(m_tCurrentMode)) {
        nErr = dc1394_format7_get_recommended_packet_size(m_ptDcCamera, m_tCurrentMode, &m_nPacketSize);
        nErr = dc1394_format7_set_packet_size(m_ptDcCamera, m_tCurrentMode, m_nPacketSize);
        nErr = dc1394_format7_set_roi(m_ptDcCamera, m_tCurrentMode, m_tCoding, m_nPacketSize, nLeft, nTop, nRight, nBottom);
    }
    return nErr;
}

int CCameraIIDC::clearROI()
{
    int nErr = ERR_COMMANDNOTSUPPORTED;

    if(bIsVideoFormat7(m_tCurrentMode)) {
        nErr = setROI(0,0, m_nWidth, m_nHeight);
    }

    return nErr;
}


bool CCameraIIDC::isFameAvailable()
{
    dc1394video_frame_t *frame;
    int nErr;

    if(m_bAbort)
        return false;

    // m_bFrameAvailable is set to false in startCapture
    if(!m_bFrameAvailable) {
        nErr = dc1394_capture_dequeue(m_ptDcCamera, DC1394_CAPTURE_POLICY_POLL, &frame);
        if(nErr) {
            m_bFrameAvailable = false;
            return m_bFrameAvailable;
        }
        if(frame) {
            printf("We got a frame\n");
            updateFrame(frame);
            m_bFrameAvailable = true;
            dc1394_capture_enqueue (m_ptDcCamera, frame);
            stopCaputure();
        }
    }
    return m_bFrameAvailable;
}

void CCameraIIDC::clearFrameMemory()
{
    if(m_pframeBuffer) {
        free(m_pframeBuffer);
        m_pframeBuffer = NULL;
    }
}

uint32_t CCameraIIDC::getBitDepth()
{
    return m_nBitsPerPixel;
}

int CCameraIIDC::getFrame(int nHeight, int nMemWidth, unsigned char* frameBuffer)
{
    int nErr = SB_OK;
    int sizeToCopy;

    if(!m_bFrameAvailable)
        return ERR_OBJECTNOTFOUND;

    if(!frameBuffer)
        return ERR_POINTER;

    //  copy internal m_pframeBuffer to frameBuffer
    sizeToCopy = nHeight * nMemWidth;
    printf("copying %d byte from internal buffer\n", sizeToCopy);
    memcpy(frameBuffer, m_pframeBuffer, sizeToCopy);

    return nErr;
}


#pragma mark protected methods
bool CCameraIIDC::bIsVideoFormat7(dc1394video_mode_t tMode)
{
    bool bIsFormat7 = false;

    switch (tMode) {
        case DC1394_VIDEO_MODE_FORMAT7_0:
        case DC1394_VIDEO_MODE_FORMAT7_1:
        case DC1394_VIDEO_MODE_FORMAT7_2:
        case DC1394_VIDEO_MODE_FORMAT7_3:
        case DC1394_VIDEO_MODE_FORMAT7_4:
        case DC1394_VIDEO_MODE_FORMAT7_5:
        case DC1394_VIDEO_MODE_FORMAT7_6:
        case DC1394_VIDEO_MODE_FORMAT7_7:
            bIsFormat7 = true;
            break;

        default:
            bIsFormat7 = false;
            break;
    }
    return bIsFormat7;
}

bool CCameraIIDC::bIs16bitMode(dc1394video_mode_t tMode)
{
    bool b16bitMode = false;
    int nErr;
    uint32_t    nDepth;

    switch (tMode) {
        case DC1394_VIDEO_MODE_FORMAT7_0:
        case DC1394_VIDEO_MODE_FORMAT7_1:
        case DC1394_VIDEO_MODE_FORMAT7_2:
        case DC1394_VIDEO_MODE_FORMAT7_3:
        case DC1394_VIDEO_MODE_FORMAT7_4:
        case DC1394_VIDEO_MODE_FORMAT7_5:
        case DC1394_VIDEO_MODE_FORMAT7_6:
        case DC1394_VIDEO_MODE_FORMAT7_7:
            nErr = dc1394_format7_get_data_depth(m_ptDcCamera, tMode, &nDepth);
            if(nErr)
                return false;
            if(nDepth == 16)
                b16bitMode = true;
            break;

        case DC1394_VIDEO_MODE_640x480_MONO16:
        case DC1394_VIDEO_MODE_800x600_MONO16:
        case DC1394_VIDEO_MODE_1024x768_MONO16:
        case DC1394_VIDEO_MODE_1280x960_MONO16:
        case DC1394_VIDEO_MODE_1600x1200_MONO16:
            b16bitMode = true;
            break;

        default:
            b16bitMode = false;
            break;
    }

    return b16bitMode;
}

void CCameraIIDC::setCameraFeatures()
{
    int nErr;
    uint32_t nTemp;


    if(m_nCurrentHue == 0) { // not set yet so we set some sane value
        dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_HUE, DC1394_FEATURE_MODE_MANUAL);
        printf("set the hue in the midle of min/max\n");
        m_tFeature_hue.id = DC1394_FEATURE_HUE;
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_hue);
        nTemp = m_tFeature_hue.min + (m_tFeature_hue.max - m_tFeature_hue.min)/2 -1;
        printf("nTemp = %u\n", nTemp);
        nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_HUE, nTemp );
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_hue);
        m_nCurrentHue = m_tFeature_hue.value;
    }
    else {
        nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_HUE, m_nCurrentHue );
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_hue);
    }
    printf("m_tFeature_hue.value = %u\n", m_tFeature_hue.value);
    printf("m_nCurrentHue = %u\n", m_nCurrentHue);
    printf("====================================\n");

    if(m_nCurrentBrightness == 0) { // not set yet so we set some sane value
        dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_BRIGHTNESS, DC1394_FEATURE_MODE_MANUAL);
        printf("set the brightness in the midle of min/max]\n");
        m_tFeature_brightness.id = DC1394_FEATURE_BRIGHTNESS;
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_brightness);
        nTemp = m_tFeature_brightness.min + (m_tFeature_brightness.max - m_tFeature_brightness.min)/2 -1;
        printf("nTemp = %u\n", nTemp);
        nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_BRIGHTNESS, nTemp );
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_brightness);
        m_nCurrentBrightness = m_tFeature_brightness.value;
    }
    else {
        nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_BRIGHTNESS, m_nCurrentBrightness);
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_brightness);
    }
    printf("m_tFeature_brightness.value = %u\n", m_tFeature_brightness.value);
    printf("m_nCurrentBrightness = %u\n", m_nCurrentBrightness);
    printf("====================================\n");

    if(m_nCurrentGama == 0) { // not set yet so we set some sane value
        dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_GAMMA, DC1394_FEATURE_MODE_MANUAL);
        printf("set the gama in the midle of min/max\n");
        m_tFeature_gamma.id = DC1394_FEATURE_GAMMA;
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_gamma);
        nTemp = m_tFeature_gamma.min + (m_tFeature_gamma.max - m_tFeature_gamma.min)/2 -1;
        printf("nTemp = %u\n", nTemp);
        nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_GAMMA, nTemp );
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_gamma);
        m_nCurrentGama = m_tFeature_gamma.value;
    }
    else {
        nErr = dc1394_feature_set_value(m_ptDcCamera, DC1394_FEATURE_GAMMA, m_nCurrentGama);
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_gamma);
    }
    printf("m_tFeature_gamma.value = %u\n", m_tFeature_gamma.value);
    printf("m_nCurrentGama = %u\n", m_nCurrentGama);
    printf("====================================\n");

    if(m_nBlue == 0 && m_nRed == 0) { // not set yet so we set some sane value
        dc1394_feature_set_mode(m_ptDcCamera, DC1394_FEATURE_WHITE_BALANCE, DC1394_FEATURE_MODE_MANUAL);
        printf("set the white balance in the midle of min/max\n");
        m_tFeature_white_balance.id = DC1394_FEATURE_WHITE_BALANCE;
        nErr = dc1394_feature_get(m_ptDcCamera, &m_tFeature_white_balance);
        m_nBlue = m_tFeature_white_balance.min + (m_tFeature_white_balance.max - m_tFeature_white_balance.min)/2 -1;
        m_nRed = m_tFeature_white_balance.min + (m_tFeature_white_balance.max - m_tFeature_white_balance.min)/2 -1;

        nErr = dc1394_feature_whitebalance_set_value(m_ptDcCamera, m_nBlue, m_nRed );
        nErr = dc1394_feature_whitebalance_get_value(m_ptDcCamera, &m_nBlue, &m_nRed);
        printf("m_nBlue = %u\n", m_nBlue);
        printf("m_nRed = %u\n", m_nRed);
        printf("m_tFeature_white_balance.min = %u\n", m_tFeature_white_balance.min);
        printf("m_tFeature_white_balance.max = %u\n", m_tFeature_white_balance.max);
    }
    else {
        nErr = dc1394_feature_whitebalance_set_value(m_ptDcCamera, m_nBlue, m_nRed );
        nErr = dc1394_feature_whitebalance_get_value(m_ptDcCamera, &m_nBlue, &m_nRed);
    }
    printf("m_nBlue = %u\n", m_nBlue);
    printf("m_nRed = %u\n", m_nRed);
    printf("====================================\n");
    

}


void CCameraIIDC::calculateFormat7PacketSize(float exposureTime)
{
    // frame size [bytes] times frame rate [Hz] divided by 8000 Hz determines IEEE 1394 packet payload size.
    double nFrameRate;

    nFrameRate = 1.0/exposureTime;

    m_nPacketSize = (((m_nWidth * (m_nBitsPerPixel/8)) * m_nHeight) * nFrameRate) / 8000;
}

