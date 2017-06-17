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

    // try to use one shot mode
    nErr = dc1394_video_set_one_shot(m_ptDcCamera, DC1394_ON);
    if(nErr)
        return ERR_COMMANDNOTSUPPORTED;


    return nErr;
}

void CCameraIIDC::Disconnect()
{
    if(m_ptDcCamera) {
        dc1394_capture_stop(m_ptDcCamera);
        dc1394_camera_reset(m_ptDcCamera);
        dc1394_reset_bus(m_ptDcCamera);
    }

    if(m_pframeBuffer) {
        free(m_pframeBuffer);
        m_pframeBuffer = NULL;
    }
}

void CCameraIIDC::setCameraGuid(uint64_t tGuid)
{
    m_cameraGuid = tGuid;
    printf("[CCameraIIDC::setCameraGuid] m_cameraGuid = %llx\n", m_cameraGuid);
}

int CCameraIIDC::listCamera(std::vector<uint64_t>  &cameraIdList)
{
    int nErr = SB_OK;
    dc1394camera_list_t *pList = NULL;
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
        cameraIdList.push_back(pList->ids[0].guid);
        printf("Found camera %llx\n", pList->ids[0].guid);
    }
    dc1394_camera_free_list (pList);

    return nErr;
}


void CCameraIIDC::updateFrame(dc1394video_frame_t *frame)
{
    dc1394error_t err;
    uint32_t w,h;
    uint32_t image_bytes;
    uint16_t pixel_data;
    unsigned char *old_buffer=NULL;
    unsigned char *new_buffer=NULL;
    w = frame->size[0];
    h = frame->size[1];
    image_bytes = frame->image_bytes;

    if(m_pframeBuffer) {
        free(m_pframeBuffer);
        m_pframeBuffer = NULL;
    }

    m_pframeBuffer = (unsigned char *)malloc(image_bytes);

    if(m_bNeedSwap) {
        new_buffer = (unsigned char *)malloc(image_bytes);
        //      do some byte swapping using swab
        swab(frame->image, m_pframeBuffer, image_bytes);
        old_buffer = frame->image;
        frame->image = m_pframeBuffer;
    }

    // may be we should check the depth in the frame instead of having a variable (set via the GUI).
    // do we need to fix the bit depth ? -> yes I'm looking at you Sony...

    if(m_bNeedDepthFix && m_bBodeIs16bits) {
        // we need to cast to a 16bit int to be able to do the right bit shift
        uint16_t *pixBuffer = (uint16_t *)frame->image;
        for(int i=0; i<image_bytes/2; i++){
            pixel_data = pixBuffer[i];
            pixel_data <<= m_nDepthBitLeftShift;
            pixBuffer[i] = pixel_data;
        }
        frame->data_depth = 16;
    }

    // copy image frame buffer to m_frameBuffer;
    memcpy(m_pframeBuffer, frame->image, frame->image_bytes);
    m_bFrameAvailable = true;
    
    if(new_buffer) {
        frame->image = old_buffer;
        free(new_buffer);
    }

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
        if(nErr)
            return ERR_CMDFAILED;
        dTEmp = (((double)nCurTemp)/10.0) - 273.15;
    }
    return nErr;
}

int CCameraIIDC::getWidth(int &nWidth)
{
    int nErr = SB_OK;

    if (m_ptDcCamera && m_bConnected) {
        nWidth = m_nWidth;
    }
    return nErr;
}

int CCameraIIDC::getHeight(int &nHeight)
{
    int nErr = SB_OK;

    if (m_ptDcCamera && m_bConnected) {
        nHeight = m_nHeight;
    }

    return nErr;
}

int CCameraIIDC::setROI(int nLeft, int nTop, int nRight, int nBottom)
{
    int nErr = ERR_COMMANDNOTSUPPORTED;

    if(bIsVideoFormat7(m_tCurrentMode)) {
        nErr = dc1394_format7_get_recommended_packet_size(m_ptDcCamera, m_tCurrentMode, &m_nPacketSize);
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
    
    return m_bFrameAvailable;
}

void CCameraIIDC::clearFrameMemory()
{
    if(m_pframeBuffer) {
        free(m_pframeBuffer);
        m_pframeBuffer = NULL;
    }
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


