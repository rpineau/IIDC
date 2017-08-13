//
//  cameraIIDC.hpp
//  IIDC
//
//  Created by roro on 12/06/17.
//  Copyright © 2017 RTI-Zone. All rights reserved.
//

#ifndef cameraIIDC_hpp
#define cameraIIDC_hpp

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#ifndef SB_WIN_BUILD
#include <unistd.h>
#endif

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/loggerinterface.h"

#include <dc1394/dc1394.h>


class CCameraIIDC {
public:
    CCameraIIDC();
    ~CCameraIIDC();

    int         Connect(uint64_t cameraGuid);
    void        Disconnect(void);
    void        setCameraGuid(uint64_t tGuid);
    int         listCamera(std::vector<uint64_t>  &cameraIdList);
    void        updateFrame(dc1394video_frame_t *frame);

    int         startCaputure();
    int         stopCaputure();
    void        abortCapture(void);

    int         getTemperture(double &dTEmp);
    int         getWidth(int &nWidth);
    int         getHeight(int &nHeight);

    int         setROI(int nLeft, int nTop, int nRight, int nBottom);
    int         clearROI(void);
    bool        isFameAvailable(void);
    void        clearFrameMemory(void);

    uint32_t    getBitDepth();
    int         getFrame(int nHeight, int nMemWidth, unsigned char* frameBuffer);

protected:
    bool                    bIsVideoFormat7(dc1394video_mode_t tMode);
    bool                    bIs16bitMode(dc1394video_mode_t tMode);
    void                    setCameraFeatures(void);
    void                    calculateFormat7PacketSize(float exposureTime);

    dc1394camera_t          *m_ptDcCamera;
    dc1394_t                *m_ptDc1394Lib;
    dc1394framerate_t       m_tFramerate;
    dc1394video_modes_t     m_tVideoModes;
    dc1394video_mode_t      m_tCurrentMode;

    dc1394featureset_t      m_tFeatures;
    dc1394feature_info_t    m_tFeature_white_balance;
    uint32_t                m_nBlue;
    uint32_t                m_nRed;

    dc1394feature_info_t    m_tFeature_hue;
    uint32_t                m_nCurrentHue;

    dc1394feature_info_t    m_tFeature_brightness;
    uint32_t                m_nCurrentBrightness;

    dc1394feature_info_t    m_tFeature_gamma;
    uint32_t                m_nCurrentGama;

    dc1394framerates_t      m_tFramerates;
    dc1394framerate_t       m_tCurFrameRate;
    dc1394color_coding_t    m_tCoding;
    uint32_t                m_nPacketSize;


    uint16_t                m_nSamplesPerPixel;
    uint16_t                m_nBitsPerSample;
    uint32_t                m_nBitsPerPixel;
    bool                    m_bNeedSwap;
    bool                    m_bNeedDepthFix;
    uint8_t                 m_nDepthBitLeftShift;
    bool                    m_bModeIs16bits;
    bool                    m_bNeed8bitTo16BitExpand;


    uint32_t                m_nWidth;
    uint32_t                m_nHeight;
    bool                    m_bConnected;
    bool                    m_bFrameAvailable;

    unsigned char *         m_pframeBuffer;
    uint64_t                m_cameraGuid;
    bool                    m_bDeviceIsUSB;
    bool                    m_bAbort;

};
#endif /* cameraIIDC_hpp */
