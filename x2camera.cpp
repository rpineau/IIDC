// x2camera.cpp  
//
#include "x2camera.h"



X2Camera::X2Camera( const char* pszSelection, 
					const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface*	pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*				pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*						pIOMutex,
					TickCountInterface*					pTickCount)
{   
	m_nPrivateISIndex				= nISIndex;
	m_pSerX							= pSerX;
	m_pTheSkyXForMounts				= pTheSkyXForMounts;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;	
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

    char szTmpBuf[128];
	setCameraId(CI_PLUGIN);

	m_dwFin = 0;
	m_dCurTemp = 0;
	m_dCurPower = 0;
	m_CachedBinX = 1;
	m_CachedBinY = 1; 
	m_CachedCam = 0;
    m_cameraGuid = 0;

    // Read in settings
    if (m_pIniUtil) {
        m_cameraGuid = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_GUID, 0);
        m_pIniUtil->readString(KEY_X2CAM_ROOT, KEY_X2CAM_GUID, "0", szTmpBuf, 128);
        //  convert uuid to uint64_t
        m_cameraGuid = std::strtoull(szTmpBuf, NULL, 0);
        printf("[X2Camera::X2Camera] m_cameraGuid = %llx\n", m_cameraGuid);
        m_Camera.setCameraGuid(m_cameraGuid);
        // read other camera settngs
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_BRIGHTNESS, 0);
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_SHARPNESS, 0);
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_WHITE_BALANCE_B, 0);
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_WHITE_BALANCE_R, 0);
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_HUE, 0);
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_SATURATION, 0);
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_GAMA, 0);
        m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_X2CAM_GAIN, 0);

    }

}

X2Camera::~X2Camera()
{
	//Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetBasicIniUtil())
		delete GetBasicIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();
	if (GetTickCountInterface())
		delete GetTickCountInterface();

}

#pragma mark DriverRootInterface
int	X2Camera::queryAbstraction(const char* pszName, void** ppVal)			
{
	X2MutexLocker ml(GetMutex());

	if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
		*ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
	else
		if (!strcmp(pszName, X2GUIEventInterface_Name))
			*ppVal = dynamic_cast<X2GUIEventInterface*>(this);

	return SB_OK;
}

#pragma mark DriverInfoInterface
void X2Camera::driverInfoDetailedInfo(BasicStringInterface& str) const		
{
	X2MutexLocker ml(GetMutex());

	str = DEVICE_AND_DRIVER_INFO_STRING;
}
double X2Camera::driverInfoVersion(void) const								
{
	X2MutexLocker ml(GetMutex());

	return 1.0;
}

#pragma mark HardwareInfoInterface
void X2Camera::deviceInfoNameShort(BasicStringInterface& str) const										
{
	X2MutexLocker ml(GetMutex());

	str = DEVICE_AND_DRIVER_INFO_STRING;
}
void X2Camera::deviceInfoNameLong(BasicStringInterface& str) const										
{
	X2MutexLocker ml(GetMutex());

	str = DEVICE_AND_DRIVER_INFO_STRING;
}
void X2Camera::deviceInfoDetailedDescription(BasicStringInterface& str) const								
{
	X2MutexLocker ml(GetMutex());

	str = DEVICE_AND_DRIVER_INFO_STRING;
}
void X2Camera::deviceInfoFirmwareVersion(BasicStringInterface& str)										
{
	X2MutexLocker ml(GetMutex());

	str = DEVICE_AND_DRIVER_INFO_STRING;
}
void X2Camera::deviceInfoModel(BasicStringInterface& str)													
{
	X2MutexLocker ml(GetMutex());

	str = DEVICE_AND_DRIVER_INFO_STRING;
}

#pragma mark Device Access
int X2Camera::CCEstablishLink(const enumLPTPort portLPT, const enumWhichCCD& CCD, enumCameraIndex DesiredCamera, enumCameraIndex& CameraFound, const int nDesiredCFW, int& nFoundCFW)
{
    int nErr = SB_OK;

    m_bLinked = false;
    if(!m_cameraGuid)
        return ERR_NODEVICESELECTED;

    printf("[X2Camera::CCEstablishLink] m_cameraGuid = %llx\n", m_cameraGuid);
    nErr = m_Camera.Connect(m_cameraGuid);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    return nErr;
}


int X2Camera::CCQueryTemperature(double& dCurTemp, double& dCurPower, char* lpszPower, const int nMaxLen, bool& bCurEnabled, double& dCurSetPoint)
{   
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    nErr = m_Camera.getTemperture(m_dCurTemp);
    // no temp controll on IIDC cam
    dCurTemp = m_dCurTemp;
	dCurPower = 0;
    dCurSetPoint = 0;
    bCurEnabled = false;

    return nErr;
}

int X2Camera::CCRegulateTemp(const bool& bOn, const double& dTemp)
{ 
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	return SB_OK;
}

int X2Camera::CCGetRecommendedSetpoint(double& RecTemp)
{
	X2MutexLocker ml(GetMutex());

	RecTemp = 100;//Set to 100 if you cannot recommend a setpoint

	return SB_OK;
}  


int X2Camera::CCStartExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const double& dTime, enumPictureType Type, const int& nABGState, const bool& bLeaveShutterAlone)
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	bool bLight = true;
    int nErr = SB_OK;

	switch (Type)
	{
		case PT_FLAT:
		case PT_LIGHT:			bLight = true;	break;
		case PT_DARK:	
		case PT_AUTODARK:	
		case PT_BIAS:			bLight = false;	break;
		default:				return ERR_CMDFAILED;
	}

	if (m_pTickCount)
		m_dwFin = (unsigned long)(dTime*1000)+m_pTickCount->elapsed();
	else
		m_dwFin = 0;
    // TBD I should try to use dTime to set the IIDC framerate and capture shutter
    nErr = m_Camera.startCaputure();
	return nErr;
}   



int X2Camera::CCIsExposureComplete(const enumCameraIndex& Cam, const enumWhichCCD CCD, bool* pbComplete, unsigned int* pStatus)
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    *pbComplete = false;

	if (m_pTickCount)
	{
        if (m_pTickCount->elapsed()>m_dwFin) {
            if(m_Camera.isFameAvailable())
                *pbComplete = true;
        }
	}
    else {
        if(m_Camera.isFameAvailable())
            *pbComplete = true;

    }

	return SB_OK;
}

int X2Camera::CCEndExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const bool& bWasAborted, const bool& bLeaveShutterAlone)           
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	int nErr = SB_OK;

	if (bWasAborted)
    {
        m_Camera.abortCapture();
        // cleanup ?
	}

    return nErr;
}

int X2Camera::CCGetChipSize(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nXBin, const int& nYBin, const bool& bOffChipBinning, int& nW, int& nH, int& nReadOut)
{
	X2MutexLocker ml(GetMutex());

	m_CachedCam = CCD;
	m_CachedBinX = nXBin;
	m_CachedBinY = nYBin;

	nW = width()/nXBin;
	nH = height()/nYBin;
    nReadOut = CameraDriverInterface::rm_Image;

    printf("nXBin = %d\n", nXBin);
    printf("nYBin = %d\n", nYBin);
    printf("nW = %d\n", nW);
    printf("nH = %d\n",nH );

    return SB_OK;
}

int X2Camera::CCGetNumBins(const enumCameraIndex& Camera, const enumWhichCCD& CCD, int& nNumBins)
{
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		nNumBins = 1;
	else
	{
		nNumBins = 1;
	}

	return SB_OK;
}

int X2Camera::CCGetBinSizeFromIndex(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nIndex, long& nBincx, long& nBincy)
{
	X2MutexLocker ml(GetMutex());

	switch (nIndex)
	{
		case 0:		nBincx=nBincy=1;break;
		case 1:		nBincx=nBincy=2;break;
		case 2:		nBincx=nBincy=3;break;
		case 3:		nBincx=nBincy=4;break;
		case 4:		nBincx=nBincy=5;break;
		default:	nBincx=nBincy=1;break;
	}

	return SB_OK;
}

int X2Camera::CCUpdateClock(void)
{   
	X2MutexLocker ml(GetMutex());

	return SB_OK;
}

int X2Camera::CCSetShutter(bool bOpen)           
{   
	X2MutexLocker ml(GetMutex());
    // IIDC camera don't have physical shutter.

	return SB_OK;;
}

int X2Camera::CCActivateRelays(const int& nXPlus, const int& nXMinus, const int& nYPlus, const int& nYMinus, const bool& bSynchronous, const bool& bAbort, const bool& bEndThread)
{   
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}

int X2Camera::CCPulseOut(unsigned int nPulse, bool bAdjust, const enumCameraIndex& Cam)
{   
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}

void X2Camera::CCBeforeDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());
}

void X2Camera::CCAfterDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());
    m_Camera.clearFrameMemory();
	return;
}

int X2Camera::CCReadoutLine(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& pixelStart, const int& pixelLength, const int& nReadoutMode, unsigned char* pMem)
{   
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}           

int X2Camera::CCDumpLines(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nReadoutMode, const unsigned int& lines)
{                                     
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}           


int X2Camera::CCReadoutImage(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nWidth, const int& nHeight, const int& nMemWidth, unsigned char* pMem)
{
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    m_Camera.getFrame(nHeight, nMemWidth, pMem);
    printf("nWidth = %d\n",nWidth);
    printf("nHeight = %d\n",nHeight);
    printf("nMemWidth = %d\n",nMemWidth);
    printf("pMem = %px\n",pMem);

    return SB_OK;
}

int X2Camera::CCDisconnect(const bool bShutDownTemp)
{
	X2MutexLocker ml(GetMutex());

	if (m_bLinked)
	{
        m_Camera.Disconnect();
		setLinked(false);
	}

	return SB_OK;
}

int X2Camera::CCSetImageProps(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nReadOut, void* pImage)
{
	X2MutexLocker ml(GetMutex());

	return SB_OK;
}

int X2Camera::CCGetFullDynamicRange(const enumCameraIndex& Camera, const enumWhichCCD& CCD, unsigned long& dwDynRg)
{
	X2MutexLocker ml(GetMutex());

    uint32_t nBitDepth;

    nBitDepth = m_Camera.getBitDepth();
    dwDynRg = (unsigned long)2^nBitDepth;
    printf("Full dynamic range is : %lu\n", dwDynRg);

	return SB_OK;
}

void X2Camera::CCMakeExposureState(int* pnState, enumCameraIndex Cam, int nXBin, int nYBin, int abg, bool bRapidReadout)
{
	X2MutexLocker ml(GetMutex());

	return;
}

int X2Camera::CCSetBinnedSubFrame(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nLeft, const int& nTop, const int& nRight, const int& nBottom)
{
	X2MutexLocker ml(GetMutex());

    m_Camera.setROI(nLeft, nTop, nRight, nBottom);
	return SB_OK;
}


int X2Camera::CCSettings(const enumCameraIndex& Camera, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());

	return ERR_NOT_IMPL;
}

int X2Camera::CCSetFan(const bool& bOn)
{
	X2MutexLocker ml(GetMutex());

	return SB_OK;
}

int	X2Camera::pathTo_rm_FitsOnDisk(char* lpszPath, const int& nPathSize)
{
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	//Just give a file path to a FITS and TheSkyX will load it
		
	return SB_OK;
}

CameraDriverInterface::ReadOutMode X2Camera::readoutMode(void)		
{
	X2MutexLocker ml(GetMutex());

	return CameraDriverInterface::rm_Image;
}


enumCameraIndex	X2Camera::cameraId()
{
	X2MutexLocker ml(GetMutex());
	return m_CameraIdx;
}

void X2Camera::setCameraId(enumCameraIndex Cam)	
{
    m_CameraIdx = Cam;
}

int X2Camera::width()
{
	int nDef = 0;

    if(m_bLinked) {
        m_Camera.getWidth(nDef);
    }
    return nDef;
}
int X2Camera::height()
{
    int nDef = 0;

    if(m_bLinked) {
        m_Camera.getHeight(nDef);

    }
    return nDef;
}

#pragma mark UI bindings
int X2Camera::execModalSettingsDialog()
{
	int nErr = SB_OK;
	X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
	X2GUIInterface*					ui = uiutil.X2UI();
	X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
	bool bPressedOK = false;
    std::vector<uint64_t>           cameraIdList;
    int i;
    char tmpBuffer[128];

	if (NULL == ui)
		return ERR_POINTER;
    nErr = ui->loadUserInterface("IIDCCamSelect.ui", deviceType(), m_nPrivateISIndex);
	if (nErr)
		return nErr;

	if (NULL == (dx = uiutil.X2DX()))
		return ERR_POINTER;

	//Intialize the user interface
    m_Camera.listCamera(cameraIdList);
    for(i=0; i< cameraIdList.size(); i++) {
	//Populate the camera combo box and set the current index (selection)
    snprintf(tmpBuffer, 128, "%llx",cameraIdList[i]);
	dx->comboBoxAppendString("comboBox",tmpBuffer);
	dx->setCurrentIndex("comboBox",0);
    }
	//Display the user interface
	if (nErr = ui->exec(bPressedOK))
		return nErr;

	//Retreive values from the user interface
	if (bPressedOK)
	{
        int nCamera;
		//Camera
		nCamera = dx->currentIndex("comboBox");
        m_Camera.setCameraGuid(cameraIdList[nCamera]);
        m_cameraGuid = cameraIdList[nCamera];
        // store GUID (64 bits) as a string
        snprintf(tmpBuffer, 128, "%llu", m_cameraGuid);
        m_pIniUtil->writeString(KEY_X2CAM_ROOT, KEY_X2CAM_GUID, tmpBuffer);
	}

	return nErr;
}

int X2Camera::doAddPixelDialogExample(int& xCoord, int& yCoord, bool& bPressedOK)
{
	int nErr = SB_OK;
	X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
	X2GUIInterface*					ui = uiutil.X2UI();
	X2GUIExchangeInterface*			dx = NULL;
	bPressedOK = false;

	if (NULL == ui)
		return ERR_POINTER;

	if (nErr = ui->loadUserInterface("x2addpixel.ui", deviceType(), m_nPrivateISIndex))
		return nErr;

	if (NULL == (dx = uiutil.X2DX()))
		return ERR_POINTER;

	dx->setPropertyInt("spinBox","value", xCoord);
	dx->setPropertyInt("spinBox_2","value", yCoord);

	if (nErr = ui->exec(bPressedOK))
		return nErr;

	dx->propertyInt("spinBox","value", xCoord);
	dx->propertyInt("spinBox_2","value", yCoord);

	return nErr;
}


void X2Camera::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
	char szEvt[DRIVER_MAX_STRING];


	//An example of showing another modal dialog
	if (!strcmp(szEvt, "on_pushButton_3_clicked"))
	{
		int nErr=SB_OK;
		bool bPressedOK = false;
		int xCoord=10;
		int yCoord=20;

		doAddPixelDialogExample(xCoord, yCoord, bPressedOK);

		if (bPressedOK)
		{
			int nRow=0;
			char szX[DRIVER_MAX_STRING];
			char szY[DRIVER_MAX_STRING];

			sprintf(szX,"%d",xCoord);
			sprintf(szY,"%d",yCoord);

			//Get the number of rows
			uiex->propertyInt("tableWidget","rowCount",nRow);

			//Increase rows by one
			uiex->setPropertyInt("tableWidget","rowCount",nRow+1);

			//Set the new pixel
			uiex->tableWidgetSetItem("tableWidget", nRow,0,szX);
			uiex->tableWidgetSetItem("tableWidget", nRow,1,szY);
		}
	}
	else
		if (!strcmp(szEvt, "on_pushButton_4_clicked"))
		{
			int nCurRow=-1;

			uiex->tableWidgetCurrentRow("tableWidget",nCurRow);

			if (-1!=nCurRow)
				uiex->tableWidgetRemoveRow("tableWidget",nCurRow);

		}
	else
		if (!strcmp(szEvt, "on_timer"))
		{
			static int nCount;
			char szCount[DRIVER_MAX_STRING];

			++nCount;
			sprintf(szCount,"Elapsed seconds = %d",nCount);

			uiex->setText("label_7",szCount);
		}

}
