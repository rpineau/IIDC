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

#define BUFFER_LEN  128

typedef struct _camere_info {
    uint64_t    uuid;
    char        model[BUFFER_LEN];
} camera_info_t;

typedef struct camResolution {
    uint32_t            nWidth;
    uint32_t            nHeight;
    dc1394video_mode_t  vidMode;
    bool                bMode7;
    uint32_t            nPacketSize;
    bool                bModeIs16bits;
    uint32_t            nBitsPerPixel;
    bool                bNeed8bitTo16BitExpand;
    uint16_t            nSamplesPerPixel;
    uint16_t            nBitsPerSample;
} camResolution_t;

class CCameraIIDC {
public:
    CCameraIIDC();
    ~CCameraIIDC();

    int         Connect(uint64_t cameraGuid);
    void        Disconnect(void);
    void        setCameraGuid(uint64_t tGuid);
    void        getCameraGuid(uint64_t &tGuid);
    void        getCameraName(char *pszName, int nMaxStrLen);
    int         listCamera(std::vector<camera_info_t>  &cameraIdList);
    void        updateFrame(dc1394video_frame_t *frame);

    int         startCaputure(double dTime);
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

    int         getResolutions(std::vector<camResolution_t> &vResList);
    int         setFeature(dc1394feature_t tFeature, uint32_t nValue, dc1394feature_mode_t tMode);
    int         getFeature(dc1394feature_t tFeature, uint32_t &nValue, uint32_t nMin, uint32_t nMax,  dc1394feature_mode_t &tMode);

protected:
    bool                    bIsVideoFormat7(dc1394video_mode_t tMode);
    bool                    bIs16bitMode(dc1394video_mode_t tMode);
    void                    setCameraFeatures(void);
    void                    calculateFormat7PacketSize(float exposureTime);

    dc1394camera_t          *m_ptDcCamera;
    dc1394_t                *m_ptDc1394Lib;
    dc1394framerate_t       m_tFramerate;
    dc1394video_modes_t     m_tVideoModes;

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

    std::vector<camResolution_t>         m_vResList;
    camResolution_t         m_tCurrentResolution;
    bool                    m_bNeedSwap;
    bool                    m_bNeedDepthFix;
    uint8_t                 m_nDepthBitLeftShift;


    bool                    m_bConnected;
    bool                    m_bFrameAvailable;

    unsigned char *         m_pframeBuffer;
    uint64_t                m_cameraGuid;
    char                    m_szCameraName[BUFFER_LEN];
    bool                    m_bDeviceIsUSB;
    bool                    m_bAbort;

};
#endif /* cameraIIDC_hpp */
