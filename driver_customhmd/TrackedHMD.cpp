#include <process.h>
#include "Serial.h"
#include "TrackedHMD.h"


CTrackedHMD::CTrackedHMD(std::string id, CServerDriver *pServer) : CTrackedDevice(id, pServer)
{

	m_Pose = {};
	m_Pose.vecDriverFromHeadTranslation[0] = 0.0;
	m_Pose.vecDriverFromHeadTranslation[1] = 0.0;
	m_Pose.vecDriverFromHeadTranslation[2] = 0.0;

	//m_Pose.vecPosition[1] = 1.0;

	m_Pose.qRotation = HmdQuaternion_Init(1, 0, 0, 0);
	m_Pose.qWorldFromDriverRotation = HmdQuaternion_Init(1, 0, 0, 0);
	m_Pose.qDriverFromHeadRotation = HmdQuaternion_Init(1, 0, 0, 0);

	m_hThread = nullptr;
	m_IsRunning = false;
	m_PIDValue = 0.05f;
	m_MonData = { 0 };
	m_MonData.HMD_POSX = 0;
	m_MonData.HMD_POSY = 0;
	m_MonData.HMD_WIDTH = 1920;
	m_MonData.HMD_HEIGHT = 1080;
	m_MonData.HMD_ASPECT = (float)m_MonData.HMD_WIDTH / (float)m_MonData.HMD_HEIGHT;
	m_MonData.HMD_FREQ = 60;
	m_MonData.HMD_FOUND = false;
	m_MonData.HMD_FAKEPACK = false;	
	EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&m_MonData);
}

CTrackedHMD::~CTrackedHMD()
{
	//Deactivate();
}

unsigned int WINAPI CTrackedHMD::ProcessThread(void *p)
{
	//	TRACE(__FUNCTIONW__);
	//MessageBox(NULL, L"Thread start", L"Info", 0);
	if (!p) return -1;
	auto device = static_cast<CTrackedHMD *>(p);
	if (device)
		device->Run();
	_endthreadex(0);
	return 0;
}

void CTrackedHMD::Run()
{
	//m_PoseUpdated = false;
	m_pDriverHost->TrackedDevicePropertiesChanged(m_unObjectId);

	//return;

	//	TRACE(__FUNCTIONW__);
	HTData htData = {};
	char *array = (char *)&htData;
	int pos = 0;

	double _yaw = 0;
	double _pitch = 0;
	double _roll = 0;

	double _yawRaw = 0;
	double _pitchRaw = 0;
	double _rollRaw = 0;

	double _yawCenter = 0;
	double _pitchCenter = 0;
	double _rollCenter = 0;

	CSerial *pSerial = new CSerial(std::wstring(L"COM3"), 115200);
	std::string incBuffer;

	char data;
	float step = 0.0001f;
	auto keydown = false;
	auto lastdown = GetTickCount();
	auto firstPacket = false;
	DWORD delay = 1000;

	long count = 0;

	//Sleep(5000);
	//if (m_MonData.HMD_FOUND && m_MonData.DisplayName[0])
	//{
	//	DEVMODE settings = {};
	//	settings.dmSize = sizeof(DEVMODE);
	//	Sleep(5000);
	//	if (EnumDisplaySettings(m_MonData.DisplayName, ENUM_CURRENT_SETTINGS, &settings))
	//	{
	//		MessageBox(nullptr, L"xx", L"yy", 0);
	//	}
	//}

	m_Pose.poseIsValid = true;
	m_Pose.result = ETrackingResult::TrackingResult_Running_OK;
	m_Pose.willDriftInYaw = false;
	m_Pose.shouldApplyHeadModel = true;


	while (m_IsRunning)
	{
		if (delay < 20)
			delay = 20;
		if (keydown && GetTickCount() - lastdown > delay)
			keydown = false;
		if ((GetAsyncKeyState(VK_HOME) & 0x8000) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000)))
		{
			if (!keydown)
			{
				//center
				_yawCenter = _yawRaw;
				_pitchCenter = _pitchRaw;
				_rollCenter = _rollRaw;
				keydown = true;
				lastdown = GetTickCount();
				delay /= 2;
			}
		}
		else if ((GetAsyncKeyState(VK_END) & 0x8000) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000)))
		{
			if (!keydown)
			{
				//center
				_yawCenter = 0;
				_pitchCenter = 0;
				_rollCenter = 0;
				keydown = true;
				firstPacket = false;
				lastdown = GetTickCount();
				delay /= 2;
			}
		}
		else if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000)))
		{
			if (!keydown)
			{
				m_PIDValue += step;
				m_pDriverHost->PhysicalIpdSet(0, m_PIDValue);
				keydown = true;
				lastdown = GetTickCount();
				delay /= 2;
			}
		}
		else if ((GetAsyncKeyState(VK_LEFT) & 0x8000) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000)))
		{
			if (!keydown)
			{
				m_PIDValue -= step;
				m_pDriverHost->PhysicalIpdSet(0, m_PIDValue);
				keydown = true;
				lastdown = GetTickCount();
				delay /= 2;
			}
		}
		else if ((GetAsyncKeyState(VK_UP) & 0x8000) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000)))
		{
			if (!keydown)
			{
				m_Pose.vecPosition[1] += 0.1;
				keydown = true;
				lastdown = GetTickCount();
				delay /= 2;
			}
		}
		else if ((GetAsyncKeyState(VK_DOWN) & 0x8000) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000)))
		{
			if (!keydown)
			{
				m_Pose.vecPosition[1] -= 0.1;
				keydown = true;
				lastdown = GetTickCount();
				delay /= 2;
			}
		}
		else
		{
			delay = 1000;
			keydown = false;
		}

		try
		{
			pos = 0;
			if (!pSerial->Read(&data, 1))
				continue;
			if (data != (char)0xff)
				continue;
			array[pos++] = data;

			if (!pSerial->Read(&data, 1))
				continue;
			if (data != (char)0xff)
				continue;
			array[pos++] = data;

			if (!pSerial->Read(&data, 1))
				continue;
			if (data != (char)0xff)
				continue;
			array[pos++] = data;

			if (!pSerial->Read(&data, 1))
				continue;
			if (data != (char)0xff)
				continue;
			array[pos++] = data;

			if (!pSerial->Read(&array[pos], sizeof(htData) - sizeof(htData.start)))
				continue;

			auto yawPrev = _yawRaw;
			auto pitchPrev = _pitchRaw;
			auto rollPrev = _rollRaw;

			//de�erler radyan *10000 geliyo seriden
			_yawRaw = (double)htData.yaw / -10000.0;
			_pitchRaw = (double)htData.pitch / -10000.0;
			_rollRaw = (double)htData.roll / 10000.0;

			//if diff is greater than 2 degrees, do minimal smoothing
			//the smaller the movement(noise), the more smoothing
			auto smooth = 0.03490658503;
			if (firstPacket)
			{
				firstPacket = false;
				yawPrev = _yawRaw;
				pitchPrev = _pitchRaw;
				rollPrev = _rollRaw;
			}

			auto yawDiff = abs(yawPrev - _yawRaw); if (yawDiff >= smooth) yawDiff = smooth * 0.99f; _yawRaw = (_yawRaw * yawDiff + yawPrev * (smooth - yawDiff)) / smooth;
			auto pitchDiff = abs(pitchPrev - _pitchRaw); if (pitchDiff >= smooth) pitchDiff = smooth * 0.99f; _pitchRaw = (_pitchRaw * pitchDiff + pitchPrev * (smooth - pitchDiff)) / smooth;
			auto rollDiff = abs(rollPrev - _rollRaw); if (rollDiff >= smooth) rollDiff = smooth * 0.99f; _rollRaw = (_rollRaw * rollDiff + rollPrev * (smooth - rollDiff)) / smooth;

			_yaw = (_yawRaw - _yawCenter);
			_pitch = (_pitchRaw - _pitchCenter);
			_roll = (_rollRaw - _rollCenter);


			//double alpha = -_roll * 0.5f;
			//double sinAlpha = sin(alpha);
			//double cosAlpha = cos(alpha);
			//double num8 = -_pitch * 0.5f;
			//double num4 = sin(num8);
			//double num3 = cos(num8);
			//double num7 = _yaw * 0.5f;
			//double num2 = sin(num7);
			//double num = cos(num7);
			//m_Pose.qRotation.x = ((num * num4) * cosAlpha) + ((num2 * num3) * sinAlpha);
			//m_Pose.qRotation.y = ((num2 * num3) * cosAlpha) - ((num * num4) * sinAlpha);
			//m_Pose.qRotation.z = ((num * num3) * sinAlpha) - ((num2 * num4) * cosAlpha);
			//m_Pose.qRotation.w = ((num * num3) * cosAlpha) + ((num2 * num4) * sinAlpha);



			double c1 = cos(_yaw / 2.0);
			double s1 = sin(_yaw / 2.0);
			double c2 = cos(_roll / 2.0);
			double s2 = sin(_roll / 2.0);
			double c3 = cos(_pitch / 2.0);
			double s3 = sin(_pitch / 2.0);
			double c1c2 = c1*c2;
			double s1s2 = s1*s2;
			m_Pose.qRotation.w = c1c2*c3 - s1s2*s3;
			m_Pose.qRotation.x = c1c2*s3 + s1s2*c3;
			m_Pose.qRotation.y = s1*c2*c3 + c1*s2*s3;
			m_Pose.qRotation.z = c1*s2*c3 - s1*c2*s3;


			//m_Pose.poseIsValid = true;
			//m_Pose.result = ETrackingResult::TrackingResult_Running_OK;
			//m_Pose.willDriftInYaw = false;
			//m_Pose.shouldApplyHeadModel = true;

			//m_PoseUpdated = true;
			m_pDriverHost->TrackedDevicePoseUpdated(m_unObjectId, m_Pose);
		}
		catch (...)
		{
			Sleep(1);
		}

	}

	delete pSerial;
}

EVRInitError CTrackedHMD::Activate(uint32_t unObjectId)
{
	//	MessageBox(NULL, L"Activate", L"Info", 0);
	m_Pose.poseIsValid = true;
	m_Pose.result = TrackingResult_Running_OK;
	m_Pose.deviceIsConnected = true;

	m_Pose.qWorldFromDriverRotation = HmdQuaternion_Init(1, 0, 0, 0);
	m_Pose.qDriverFromHeadRotation = HmdQuaternion_Init(1, 0, 0, 0);


	m_unObjectId = unObjectId;
	m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, ProcessThread, this, CREATE_SUSPENDED, nullptr));
	if (m_hThread)
	{
		m_IsRunning = true;
		ResumeThread(m_hThread);
	}

	return vr::VRInitError_None;
}

void CTrackedHMD::Deactivate()
{
	m_IsRunning = false;
	if (m_hThread)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		m_hThread = nullptr;
	}
	m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
	//	TRACE(__FUNCTIONW__);
}

void *CTrackedHMD::GetComponent(const char *pchComponentNameAndVersion)
{
	if (!_stricmp(pchComponentNameAndVersion, vr::IVRDisplayComponent_Version))
	{
		return (vr::IVRDisplayComponent*)this;
	}

	// override this to add a component to a driver
	return nullptr;
}

void CTrackedHMD::DebugRequest(const char * pchRequest, char * pchResponseBuffer, uint32_t unResponseBufferSize)
{
	//	TRACE(__FUNCTIONW__);
}

void CTrackedHMD::GetWindowBounds(int32_t * pnX, int32_t * pnY, uint32_t * pnWidth, uint32_t * pnHeight)
{
	//	TRACE(__FUNCTIONW__);
//	MessageBox(NULL, L"GetWindowBounds", L"Info", 0);
	*pnX = m_MonData.HMD_POSX;
	*pnY = m_MonData.HMD_POSY;
	*pnWidth = m_MonData.HMD_WIDTH;
	*pnHeight = m_MonData.HMD_HEIGHT;
}

bool CTrackedHMD::IsDisplayOnDesktop()
{
	//	MessageBox(NULL, L"IsDisplayOnDesktop", L"Info", 0);
	//return true;
	return true;
}

bool CTrackedHMD::IsDisplayRealDisplay()
{
	//	TRACE(__FUNCTIONW__);	
//	MessageBox(NULL, L"IsDisplayRealDisplay", L"Info", 0);
	return true;
}

void CTrackedHMD::GetRecommendedRenderTargetSize(uint32_t * pnWidth, uint32_t * pnHeight)
{
	//	TRACE(__FUNCTIONW__);
//	MessageBox(NULL, L"GetRecommendedRenderTargetSize", L"Info", 0);
	if (m_MonData.HMD_FAKEPACK)
	{
		*pnWidth = m_MonData.HMD_WIDTH;
		*pnHeight = (m_MonData.HMD_HEIGHT - 30) / 2;
	}
	else
	{
		*pnWidth = m_MonData.HMD_WIDTH;
		*pnHeight = m_MonData.HMD_HEIGHT;
	}
	*pnWidth = uint32_t(*pnWidth * HMD_SUPERSAMPLE);
	*pnHeight = uint32_t(*pnHeight * HMD_SUPERSAMPLE);
}

void CTrackedHMD::GetEyeOutputViewport(EVREye eEye, uint32_t * pnX, uint32_t * pnY, uint32_t * pnWidth, uint32_t * pnHeight)
{
	//	MessageBox(NULL, L"GetEyeOutputViewport", L"Info", 0);
	if (m_MonData.HMD_FAKEPACK)
	{
		//	TRACE(__FUNCTIONW__);
		uint32_t h = (m_MonData.HMD_HEIGHT - 30) / 2;
		switch (eEye)
		{
		case EVREye::Eye_Left:
			*pnX = 0;
			*pnY = h + 30;
			*pnWidth = m_MonData.HMD_WIDTH;
			*pnHeight = h;
			break;
		case EVREye::Eye_Right:
			*pnX = 0;
			*pnY = 0;
			*pnWidth = m_MonData.HMD_WIDTH;
			*pnHeight = h;
			break;
		}
	}
	else 
	{		
		switch (eEye)
		{
		case EVREye::Eye_Left:
			*pnX = 0;
			*pnY = 0;
			*pnWidth = m_MonData.HMD_WIDTH / 2;
			*pnHeight = m_MonData.HMD_HEIGHT;
			break;
		case EVREye::Eye_Right:
			*pnX = 0 + (m_MonData.HMD_WIDTH / 2);
			*pnY = 0;
			*pnWidth = m_MonData.HMD_WIDTH / 2;
			*pnHeight = m_MonData.HMD_HEIGHT;
			break;
		}
	}
}

void CTrackedHMD::GetProjectionRaw(EVREye eEye, float * pfLeft, float * pfRight, float * pfTop, float * pfBottom)
{
	//	MessageBox(NULL, L"GetProjectionRaw", L"Info", 0);
		////	TRACE(__FUNCTIONW__);
	if (m_MonData.HMD_FAKEPACK)
	{
		switch (eEye)
		{
		case EVREye::Eye_Left:
			*pfLeft = -1.0f;
			*pfRight = 1.0f;
			*pfTop = -1.0f * m_MonData.HMD_ASPECT;
			*pfBottom = 1.0f * m_MonData.HMD_ASPECT;
			break;
		case EVREye::Eye_Right:
			*pfLeft = -1.0f;
			*pfRight = 1.0f;
			*pfTop = -1.0f * m_MonData.HMD_ASPECT;
			*pfBottom = 1.0f * m_MonData.HMD_ASPECT;
			break;
		}
	}
	else
	{
		switch (eEye)
		{
		case EVREye::Eye_Left:
			*pfLeft = -1.0f;
			*pfRight = 1.0f;
			*pfTop = -1.0f / m_MonData.HMD_ASPECT;
			*pfBottom = 1.0f / m_MonData.HMD_ASPECT;
			break;
		case EVREye::Eye_Right:
			*pfLeft = -1.0f;
			*pfRight = 1.0f;
			*pfTop = -1.0f / m_MonData.HMD_ASPECT;
			*pfBottom = 1.0f / m_MonData.HMD_ASPECT;
			break;
		}
	}
}

DistortionCoordinates_t CTrackedHMD::ComputeDistortion(EVREye eEye, float fU, float fV)
{
	vr::DistortionCoordinates_t coords = {};
	coords.rfRed[0] = fU;
	coords.rfRed[1] = fV;
	coords.rfBlue[0] = fU;
	coords.rfBlue[1] = fV;
	coords.rfGreen[0] = fU;
	coords.rfGreen[1] = fV;
	return coords;
}

DriverPose_t CTrackedHMD::GetPose()
{
	//	TRACE(__FUNCTIONW__);
	return m_Pose;
}

bool CTrackedHMD::GetBoolTrackedDeviceProperty(ETrackedDeviceProperty prop, ETrackedPropertyError *pError)
{
	//TRACE(__FUNCTIONW__);

	switch (prop)
	{
	case vr::Prop_IsOnDesktop_Bool: 
		if (pError)
			*pError = vr::TrackedProp_Success;
		return false;

	case vr::Prop_ContainsProximitySensor_Bool:
		//if (pError)
		//	*pError = vr::TrackedProp_Success;
		//return true;
	case vr::Prop_CanUnifyCoordinateSystemWithHmd_Bool:
	case vr::Prop_DeviceProvidesBatteryStatus_Bool:
	case vr::Prop_WillDriftInYaw_Bool:
	case vr::Prop_BlockServerShutdown_Bool:
	case vr::Prop_DeviceIsWireless_Bool:
	case vr::Prop_DeviceIsCharging_Bool:
	case vr::Prop_Firmware_UpdateAvailable_Bool:
	case vr::Prop_Firmware_ManualUpdate_Bool:
	case vr::Prop_DeviceCanPowerOff_Bool:
	case vr::Prop_HasCamera_Bool:
	case vr::Prop_ReportsTimeSinceVSync_Bool:
		if (pError)
			*pError = vr::TrackedProp_ValueNotProvidedByDevice;
		return false;
	}

	if (pError)
		*pError = vr::TrackedProp_UnknownProperty;
	return false;
}

float CTrackedHMD::GetFloatTrackedDeviceProperty(ETrackedDeviceProperty prop, ETrackedPropertyError * pError)
{
	//	TRACE(__FUNCTIONW__);
	const float default_value = 0.0f;

	switch (prop)
	{
	case vr::Prop_UserHeadToEyeDepthMeters_Float:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return 0.0f;
	case vr::Prop_SecondsFromVsyncToPhotons_Float:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return 0.0f;
	case vr::Prop_DisplayFrequency_Float:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return m_MonData.HMD_FREQ;
	case vr::Prop_UserIpdMeters_Float:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return m_PIDValue;
	case vr::Prop_DeviceBatteryPercentage_Float:
	case vr::Prop_DisplayGCScale_Float:
	case vr::Prop_DisplayGCPrescale_Float:
	case vr::Prop_DisplayMCOffset_Float:
	case vr::Prop_DisplayMCScale_Float:
	case vr::Prop_DisplayGCBlackClamp_Float:
	case vr::Prop_DisplayGCOffset_Float:
	case vr::Prop_LensCenterLeftU_Float:
	case vr::Prop_LensCenterLeftV_Float:
	case vr::Prop_LensCenterRightU_Float:
	case vr::Prop_LensCenterRightV_Float:
	case vr::Prop_FieldOfViewLeftDegrees_Float:
	case vr::Prop_FieldOfViewRightDegrees_Float:
	case vr::Prop_FieldOfViewTopDegrees_Float:
	case vr::Prop_FieldOfViewBottomDegrees_Float:
	case vr::Prop_TrackingRangeMinimumMeters_Float:
	case vr::Prop_TrackingRangeMaximumMeters_Float:
		if (pError)
			*pError = vr::TrackedProp_ValueNotProvidedByDevice;
		return default_value;
	}

	if (pError)
		*pError = vr::TrackedProp_UnknownProperty;
	return default_value;
}

int32_t CTrackedHMD::GetInt32TrackedDeviceProperty(ETrackedDeviceProperty prop, ETrackedPropertyError *pError)
{
	//	TRACE(__FUNCTIONW__);
	const int32_t default_value = 0;


	switch (prop)
	{
		case vr::Prop_DeviceClass_Int32:
			if (pError)
				*pError = vr::TrackedProp_Success;
			return vr::TrackedDeviceClass_HMD;
		//case vr::Prop_EdidVendorID_Int32:
		//	if (pError)
		//		*pError = vr::TrackedProp_Success;
		//	return 1027;
		//case vr::Prop_EdidProductID_Int32:
		//	if (pError)
		//		*pError = vr::TrackedProp_Success;
		//	return 24577;
		//case vr::Prop_DisplayMCType_Int32:
		//case vr::Prop_DisplayGCType_Int32:
		//case vr::Prop_Axis0Type_Int32:
		//case vr::Prop_Axis1Type_Int32:
		//case vr::Prop_Axis2Type_Int32:
		//case vr::Prop_Axis3Type_Int32:
		//case vr::Prop_Axis4Type_Int32:
		//	if (pError)
		//		*pError = vr::TrackedProp_ValueNotProvidedByDevice;
		//	return default_value;
	}

	if (pError)
		*pError = vr::TrackedProp_UnknownProperty;
	return default_value;
}

uint64_t CTrackedHMD::GetUint64TrackedDeviceProperty(ETrackedDeviceProperty prop, ETrackedPropertyError *pError)
{
	//	TRACE(__FUNCTIONW__);
	const uint64_t default_value = 0;


	switch (prop) {
	case vr::Prop_CurrentUniverseId_Uint64:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return 2;
	case vr::Prop_PreviousUniverseId_Uint64:
	case vr::Prop_HardwareRevision_Uint64:
	case vr::Prop_FirmwareVersion_Uint64:
	case vr::Prop_FPGAVersion_Uint64:
	case vr::Prop_VRCVersion_Uint64:
	case vr::Prop_RadioVersion_Uint64:
	case vr::Prop_DongleVersion_Uint64:
	case vr::Prop_CameraFirmwareVersion_Uint64:
	case vr::Prop_DisplayFPGAVersion_Uint64:
	case vr::Prop_DisplayBootloaderVersion_Uint64:
	case vr::Prop_DisplayHardwareVersion_Uint64:
	case vr::Prop_AudioFirmwareVersion_Uint64:
	case vr::Prop_SupportedButtons_Uint64:
	case vr::Prop_DisplayFirmwareVersion_Uint64:
		if (pError)
			*pError = vr::TrackedProp_ValueNotProvidedByDevice;
		return default_value;
	}

	if (pError)
		*pError = vr::TrackedProp_UnknownProperty;
	return default_value;
}

HmdMatrix34_t CTrackedHMD::GetMatrix34TrackedDeviceProperty(ETrackedDeviceProperty prop, ETrackedPropertyError *pError)
{
	//	TRACE(__FUNCTIONW__);
	// Default value is identity matrix
	vr::HmdMatrix34_t default_value;
	HmdMatrix_SetIdentity(&default_value);

	switch (prop) {
	case vr::Prop_StatusDisplayTransform_Matrix34:
		if (pError)
			*pError = vr::TrackedProp_ValueNotProvidedByDevice;
		return default_value;
	}

	if (pError)
		*pError = vr::TrackedProp_UnknownProperty;
	return default_value;
}

uint32_t CTrackedHMD::GetStringTrackedDeviceProperty(vr::ETrackedDeviceProperty prop, char *pchValue, uint32_t unBufferSize, vr::ETrackedPropertyError *pError)
{
	std::string sValue = GetStringTrackedDeviceProperty(prop, pError);
	if (*pError == vr::TrackedProp_Success)
	{
		if (sValue.size() + 1 > unBufferSize)
		{
			*pError = vr::TrackedProp_BufferTooSmall;
		}
		else
		{
			strcpy_s(pchValue, unBufferSize, sValue.c_str());
		}
		return (uint32_t)sValue.size() + 1;
	}
	return 0;
}

std::string CTrackedHMD::GetStringTrackedDeviceProperty(vr::ETrackedDeviceProperty prop, vr::ETrackedPropertyError *pError)
{
	*pError = vr::TrackedProp_ValueNotProvidedByDevice;
	std::string sRetVal;

	switch (prop)
	{
	case vr::Prop_ManufacturerName_String:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return std::string("SONY");
	case vr::Prop_ModelNumber_String:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return std::string("HMZ-T2");
	case vr::Prop_SerialNumber_String:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return std::string("1244244");
	case vr::Prop_TrackingSystemName_String:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return std::string("Custom Aruino Tracker");
	case vr::Prop_HardwareRevision_String:
	case vr::Prop_TrackingFirmwareVersion_String:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return std::string("1.0");
	case vr::Prop_AttachedDeviceId_String:
		if (pError)
			*pError = vr::TrackedProp_Success;
		return m_Id;

	case vr::Prop_RenderModelName_String:
	case vr::Prop_AllWirelessDongleDescriptions_String:
	case vr::Prop_ConnectedWirelessDongle_String:
	case vr::Prop_Firmware_ManualUpdateURL_String:
	case vr::Prop_Firmware_ProgrammingTarget_String:
	case vr::Prop_DisplayMCImageLeft_String:
	case vr::Prop_DisplayMCImageRight_String:
	case vr::Prop_DisplayGCImage_String:
	case vr::Prop_CameraFirmwareDescription_String:
	case vr::Prop_ModeLabel_String:
	default:
		if (pError)
			*pError = vr::TrackedProp_ValueNotProvidedByDevice;
		return sRetVal;
	}
}

void CTrackedHMD::CreateSwapTextureSet(uint32_t unPid, uint32_t unFormat, uint32_t unWidth, uint32_t unHeight, void *(*pSharedTextureHandles)[2])
{
}

void CTrackedHMD::DestroySwapTextureSet(void *pSharedTextureHandle)
{
}

void CTrackedHMD::DestroyAllSwapTextureSets(uint32_t unPid)
{
}

void CTrackedHMD::SubmitLayer(void *pSharedTextureHandles[2], const vr::VRTextureBounds_t * pBounds, const vr::HmdMatrix34_t * pPose)
{
}

void CTrackedHMD::Present()
{
}

void CTrackedHMD::PowerOff()
{
}
