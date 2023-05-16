#include "CIntelMSDKEncoder.h"
#include <cstdio>
#include <chrono>
#include <thread>
#include <mfxjpeg.h>

#pragma comment(lib,"libmfx_vs2015.lib")

CIntelMSDKEncoder::CIntelMSDKEncoder()
{
	m_usImageSrcWidth = 1920;
	m_usImageSrcHeight = 1080;

	m_usQuality = 85;
	m_uiFrameRate = 30;
	m_usKbps = 4000;
	m_imgChannelCount = 1;

	m_uiSrcType = MFX_FOURCC_NV12;
	m_uiDstType = MFX_CODEC_AVC;

	m_pSession = NULL;
	m_pEncParams = NULL;
	m_videoMemHandle.m_pFrameAllocator = NULL;
	m_videoMemHandle.m_pResponse = NULL;
	//m_pDeviceThis.pDevice = nullptr;
	//m_pDeviceThis.pthis = nullptr;
}

CIntelMSDKEncoder::~CIntelMSDKEncoder()
{
	if (m_pSession != nullptr)
		UnInit();
}

bool CIntelMSDKEncoder::Init(USHORT usImageSrcWidth, USHORT usImageSrcHeight, UINT uiSrcType, UINT uiDstType, USHORT usQuality/*=85*/, UINT uiFrameRate/*=25*/, USHORT usKbps/*=4000*/, int imgChannelCount/*=1*/)
{
	bool bRet = false;

	m_usImageSrcWidth = usImageSrcWidth;
	m_usImageSrcHeight = usImageSrcHeight;
	m_uiSrcType = uiSrcType;
	m_uiDstType = uiDstType;
	m_usQuality = usQuality;
	m_uiFrameRate = uiFrameRate;
	m_usKbps = usKbps;
	m_imgChannelCount = imgChannelCount;


	CloseEncoder();
	CreateEncoder();
	bRet = InitVideoMemHandle();

	return bRet;
}

bool CIntelMSDKEncoder::UnInit()
{
	return CloseEncoder();
}

bool CIntelMSDKEncoder::CloseEncoder()
{
	bool bRet = false;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);

	if (nullptr != m_pSession)
	{
		MFXVideoENCODE_Close(*((MFXVideoSession*)m_pSession));
		delete (MFXVideoSession*)m_pSession;
		m_pSession = nullptr;
	}
	if (nullptr != pFrameAllocator)
	{
		pFrameAllocator->Free(pFrameAllocator->pthis,pResponse);
		delete pFrameAllocator;
		pFrameAllocator = nullptr;
		m_videoMemHandle.m_pFrameAllocator = nullptr;
	}
	
	if (nullptr != m_pEncParams)
	{
		delete (mfxVideoParam*)m_pEncParams;
		m_pEncParams = nullptr;
	}
	if (nullptr != pResponse)
	{
		delete pResponse;
		pResponse = nullptr;
		m_videoMemHandle.m_pResponse = nullptr;
	}

	for (int i = 0; i < m_videoMemHandle.m_vecpSurfaces.size(); ++i)
	{
		if (nullptr != m_videoMemHandle.m_vecpSurfaces[i])
		{
			delete (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[i]);
			m_videoMemHandle.m_vecpSurfaces[i] = nullptr;
		}
	}

	Release();

	bRet = true;
	return bRet;
}


bool CIntelMSDKEncoder::CreateEncoder()
{
	bool bRet = false;

	MFXVideoSession* pSession = NULL;
	pSession = new MFXVideoSession;
	m_pSession = pSession;
	m_pEncParams = new mfxVideoParam;
	m_videoMemHandle.m_pFrameAllocator = new mfxFrameAllocator;
	m_videoMemHandle.m_pResponse = new mfxFrameAllocResponse;
	//m_pDeviceThis.pDevice = new CDeviceManagerD3D11;



	mfxStatus sts = MFX_ERR_NONE;
	mfxIMPL impl = MFX_IMPL_HARDWARE;//MFX_IMPL_VIA_D3D11; // MFX_IMPL_HARDWARE;//MFX_IMPL_VIA_D3D11;
	mfxVersion ver = { { 0, 1 } };
	mfxFrameAllocator* pFrameAllocator =(mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;
	sts = Initialize(impl, ver, (MFXVideoSession*)m_pSession, pFrameAllocator);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


	memset(pEncParam, 0, sizeof(mfxVideoParam));

	pEncParam->mfx.FrameInfo.FourCC = m_uiSrcType;
	pEncParam->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	pEncParam->mfx.FrameInfo.CropX = 0;
	pEncParam->mfx.FrameInfo.CropY = 0;
	pEncParam->mfx.FrameInfo.CropW = m_usImageSrcWidth;
	pEncParam->mfx.FrameInfo.CropH = m_usImageSrcHeight;
	pEncParam->mfx.CodecId = m_uiDstType;

	if (MFX_CODEC_JPEG == m_uiDstType)
	{
		pEncParam->mfx.Quality = m_usQuality;
		pEncParam->mfx.Interleaved = MFX_SCANTYPE_INTERLEAVED;
		pEncParam->mfx.RestartInterval = 0;
	}
	else if (MFX_CODEC_AVC == m_uiDstType)
	{
		pEncParam->mfx.IdrInterval = 1;
		pEncParam->mfx.GopPicSize = 20;
		pEncParam->mfx.GopRefDist = 1; //����B֡
		pEncParam->mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;// MFX_PROFILE_AVC_HIGH;//MFX_PROFILE_AVC_CONSTRAINED_HIGH;
		pEncParam->mfx.CodecLevel = MFX_LEVEL_AVC_3;//MFX_LEVEL_AVC_4;
		pEncParam->mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;// MFX_TARGETUSAGE_BALANCED;//MFX_TARGETUSAGE_BEST_QUALITY;
		pEncParam->mfx.GopOptFlag = MFX_GOP_STRICT;//MFX_GOP_CLOSED;
		pEncParam->mfx.RateControlMethod = MFX_RATECONTROL_VBR;//MFX_RATECONTROL_AVBR;//MFX_RATECONTROL_LA;//MFX_RATECONTROL_CQP;
		pEncParam->mfx.FrameInfo.FrameRateExtN = m_uiFrameRate;
		pEncParam->mfx.FrameInfo.FrameRateExtD = 1;
		pEncParam->mfx.BRCParamMultiplier = 20;
		pEncParam->mfx.TargetKbps = m_usKbps / pEncParam->mfx.BRCParamMultiplier;  //TargetKbps*=BRCParamMultiplier

	}
	pEncParam->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	pEncParam->mfx.FrameInfo.Width = MSDK_ALIGN16(m_usImageSrcWidth);
	pEncParam->mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == pEncParam->mfx.FrameInfo.PicStruct) ?
											MSDK_ALIGN16(m_usImageSrcHeight) : MSDK_ALIGN32(m_usImageSrcHeight);
	pEncParam->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
	pEncParam->AsyncDepth = 1;

	sts = MFXVideoENCODE_Query(*((MFXVideoSession*)m_pSession), pEncParam, pEncParam);
	//sts = MFXVideoENCODE_Init(*((MFXVideoSession*)m_pSession), pEncParam);




	bRet = true;
	return bRet;
}

bool CIntelMSDKEncoder::EncodeBufferLoop(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength)
{
	bool bRet = false;

	mfxStatus sts = MFX_ERR_NONE;
	int ibpp = 12;
	int iMemSurfaceCount = (int)m_videoMemHandle.m_vecpSurfaces.size();
	int vecpSrcBufferIdx = 0;
	int nFirstSyncTask = 0;
	int encodedFrameIdx = 0;
	int tatalFrameCount = (int)vecpSrcBuffer.size();
	std::vector<mfxSyncPoint> vecSync(iMemSurfaceCount, NULL);
	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;

	mfxBitstream mfxBS;
	memset(&mfxBS, 0, sizeof(mfxBS));
	mfxBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
	USHORT BufferSizeInKB = 4 + (pEncParam->mfx.FrameInfo.CropW * pEncParam->mfx.FrameInfo.CropH * ibpp / 8 + 1023) / 1024;
	mfxBS.MaxLength = BufferSizeInKB * 1024;
	mfxBS.DataLength = 0;
	mfxBS.Data = NULL;
	std::vector<mfxBitstream> vecBitStream(iMemSurfaceCount, mfxBS);

	//loop
	while (vecpSrcBufferIdx < tatalFrameCount)
	{
		int freeSurfaceIdx = -1;
		for (int i = 0; i < iMemSurfaceCount; ++i)
		{
			if (NULL == vecSync[i])   //�ҵ����е��Դ��
			{
				freeSurfaceIdx = i;
				break;
			}
		}
		if (freeSurfaceIdx != -1)
		{
			LockVideoMem(freeSurfaceIdx);
			SysMemToVideoMem((unsigned char*)vecpSrcBuffer[vecpSrcBufferIdx], (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[freeSurfaceIdx]));
			UnlockVideoMem(freeSurfaceIdx);
			vecBitStream[freeSurfaceIdx].Data = (unsigned char*)vecpDstBuffer[vecpSrcBufferIdx];
			vecpSrcBufferIdx++;
			//do {
			sts = MFXVideoENCODE_EncodeFrameAsync(*((MFXVideoSession*)m_pSession),NULL,
				(mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[freeSurfaceIdx]), &vecBitStream[freeSurfaceIdx], &vecSync[freeSurfaceIdx]);
		}
		else
		{
			((MFXVideoSession*)m_pSession)->SyncOperation(vecSync[nFirstSyncTask], WAIT_60000MS);
			vecSync[nFirstSyncTask] = NULL;
			vecBitStream[nFirstSyncTask].Data = NULL;
			veciDstLength[encodedFrameIdx] = vecBitStream[nFirstSyncTask].DataLength;
			++encodedFrameIdx;
			vecBitStream[nFirstSyncTask].DataLength = 0;
			nFirstSyncTask = (nFirstSyncTask + 1) % iMemSurfaceCount;
		}
	}
	//����loop��ʣ��֡
	for (int i = 0; i < iMemSurfaceCount; ++i)
	{
		((MFXVideoSession*)m_pSession)->SyncOperation(vecSync[i], WAIT_60000MS);
		veciDstLength[encodedFrameIdx] = vecBitStream[i].DataLength;
		++encodedFrameIdx;
	}

	if (encodedFrameIdx == tatalFrameCount)
		bRet = true;

	bRet = true;
	return bRet;
}




bool CIntelMSDKEncoder::EncodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength)
{
	bool bRet = false;
	int itotalFrameCount = (int)vecpDstBuffer.size();
	int ibpp = 12;
	std::vector<Task> vecTasks(itotalFrameCount);
	USHORT BufferSizeInKB;
	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;
	mfxStatus sts = MFX_ERR_NONE;

	if (MFX_CODEC_AVC == pEncParam->mfx.CodecId)
	{
		BufferSizeInKB = pEncParam->mfx.BufferSizeInKB;
	}
	else
	{
		BufferSizeInKB = 4 + (pEncParam->mfx.FrameInfo.CropH * pEncParam->mfx.FrameInfo.CropW * ibpp / 8 + 1023) / 1024;
	}
	for (int i = 0; i < itotalFrameCount; ++i)
	{
		vecTasks[i] = {};
		vecTasks[i].mfxBS.MaxLength = BufferSizeInKB * 1024;
		vecTasks[i].mfxBS.Data = (unsigned char*)vecpDstBuffer[i];
	}


	int iEncSurfIdx = 0;
	int iTaskIdx = 0;
	int iFirstSyncTask = 0;
	int iDstBufferindex = 0;
	int iFrameCount = 0;

	// Stage 1: Main encoding loop
	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
	{
		iTaskIdx = GetFreeTaskIndex(vecTasks.data(), itotalFrameCount);
		iEncSurfIdx = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
		if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iEncSurfIdx)
		{
			// No more free tasks, need to sync
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(vecTasks[iFirstSyncTask].syncp, WAIT_60000MS);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
			iFirstSyncTask = (iFirstSyncTask + 1) % itotalFrameCount;

			++iFrameCount;
		}
		else
		{
			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iEncSurfIdx, MFX_ERR_MEMORY_ALLOC);
			LockVideoMem(iEncSurfIdx);
			SysMemToVideoMem((unsigned char*)vecpSrcBuffer[iDstBufferindex++], m_videoMemHandle.m_vecpSurfaces[iEncSurfIdx]);
			UnlockVideoMem(iEncSurfIdx);

			for (;;)
			{
				// Encode a frame asychronously (returns immediately)
				sts = MFXVideoENCODE_EncodeFrameAsync(*((MFXVideoSession*)m_pSession), NULL, 
														(mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iEncSurfIdx]),
														&vecTasks[iTaskIdx].mfxBS, &vecTasks[iTaskIdx].syncp);
				if (MFX_ERR_NONE < sts && !vecTasks[iTaskIdx].syncp)
				{    // Repeat the call if warning and no output
					if (MFX_WRN_DEVICE_BUSY == sts)
						//Sleep(1);  // Wait if device is busy, then repeat the same call
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				else if (MFX_ERR_NONE < sts && vecTasks[iTaskIdx].syncp)
				{
					sts = MFX_ERR_NONE;     // Ignore warnings if output is available
					break;
				}
				else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
				{
					// Allocate more bitstream buffer memory here if needed...
					break;
				}
				else
					break;
			}
			if (iDstBufferindex == vecpSrcBuffer.size())
				break;
		}
	}

	// MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Stage 2: Retrieve the buffered encoded frames
	while (MFX_ERR_NONE <= sts && iFirstSyncTask < itotalFrameCount)
	{
		iTaskIdx = GetFreeTaskIndex(vecTasks.data(), itotalFrameCount);      // Find free task
		iEncSurfIdx = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
		if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iEncSurfIdx)
		{
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(vecTasks[iFirstSyncTask].syncp, WAIT_60000MS);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
			veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
			++iFirstSyncTask;
			++iFrameCount;
		}
		else
		{
			for (;;)
			{
				sts = MFXVideoENCODE_EncodeFrameAsync(*((MFXVideoSession*)m_pSession), NULL, NULL, &vecTasks[iTaskIdx].mfxBS, &vecTasks[iTaskIdx].syncp);
				if (MFX_ERR_NONE < sts && !vecTasks[iTaskIdx].syncp)
				{    // Repeat the call if warning and no output
					if (MFX_WRN_DEVICE_BUSY == sts)
						//Sleep(1);  // Wait if device is busy, then repeat the same call
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				else if (MFX_ERR_NONE < sts && vecTasks[iTaskIdx].syncp)
				{
					sts = MFX_ERR_NONE;     // Ignore warnings if output is available
					break;
				}
				else
					break;
			}
		}
	}

	// MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	bRet = true;
	return bRet;
}

//bool CIntelMSDKEncoder::JpegsToFile(std::vector<void*>& vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName)
//{
//	bool bRet = false;
//	char	strValue[1024] = { 0 };
//	for (int i = 0; i < vecpBuffer.size(); ++i)
//	{
//		FILE* fSink = nullptr;
//		sprintf_s(strValue,1024, "%s/%d.jpg", pstrFileName, i);
//		fopen_s(&fSink, strValue, "wb+");
//		fwrite(vecpBuffer[i], 1, veciLength[i], fSink);
//		fclose(fSink);
//	}
//	bRet = true;
//	return bRet;
//}

//bool CIntelMSDKEncoder::AviToFile(std::vector<void*> vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName)
//{
//	bool bRet = false;
//
//	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;
//
//	GWAVI aviWriter(pstrFileName, pEncParam->mfx.FrameInfo.CropW, pEncParam->mfx.FrameInfo.CropH, 24, "H264",
//		pEncParam->mfx.FrameInfo.FrameRateExtN / pEncParam->mfx.FrameInfo.FrameRateExtD, NULL);
//	for (int i = 0; i < vecpBuffer.size(); ++i)
//	{
//		aviWriter.AddVideoFrame((unsigned char*)vecpBuffer[i], veciLength[i]);
//	}
//	aviWriter.Finalize();
//
//
//	bRet = true;
//	return bRet;
//}



bool CIntelMSDKEncoder::InitVideoMemHandle()
{
	bool bRet = false;

	mfxStatus sts = MFX_ERR_NONE;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
	pResponse->NumFrameActual = 0;
	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;
	mfxFrameAllocRequest encRequest;
	memset(&encRequest, 0, sizeof(encRequest));
	sts = MFXVideoENCODE_QueryIOSurf(*((MFXVideoSession*)m_pSession), pEncParam, &encRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (encRequest.NumFrameMin < 4 && encRequest.NumFrameSuggested < 4) 
	{
		encRequest.NumFrameMin = 4;
		encRequest.NumFrameSuggested = 4;
	}
	encRequest.Type |= WILL_WRITE; // This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application
	
	//

	sts = pFrameAllocator->Alloc(pFrameAllocator->pthis, &encRequest, pResponse);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	USHORT nEncSurfNum = pResponse->NumFrameActual;

	for (int i = 0; i < nEncSurfNum; i++) {
		mfxFrameSurface1* pFrameSurface = new mfxFrameSurface1();
		m_videoMemHandle.m_vecpSurfaces.push_back(pFrameSurface);
		memset(m_videoMemHandle.m_vecpSurfaces[i], 0, sizeof(mfxFrameSurface1));
		pFrameSurface->Info = pEncParam->mfx.FrameInfo;
		pFrameSurface->Data.MemId = pResponse->mids[i];      // MID (memory id) represent one video NV12 surface
		pFrameSurface->Data.Y = (UCHAR*)pFrameSurface->Data.MemId;
		pFrameSurface->Data.Pitch = pFrameSurface->Info.Width;
	
		LockVideoMem(i);
		memset((void*)pFrameSurface->Data.Y, 128, pFrameSurface->Data.Pitch * pFrameSurface->Info.Height * 1.5);
		UnlockVideoMem(i);
	}
	sts = MFXVideoENCODE_Init(*((MFXVideoSession*)m_pSession), pEncParam);
	if (MFX_CODEC_AVC == m_uiDstType)
	{
		mfxVideoParam par;
		memset(&par, 0, sizeof(par));
		sts = MFXVideoENCODE_GetVideoParam(*((MFXVideoSession*)m_pSession), &par);
		pEncParam->mfx.BufferSizeInKB = par.mfx.BufferSizeInKB;
	}
	bRet = true;
	return bRet;
}

bool CIntelMSDKEncoder::SysMemToVideoMem(unsigned char* pSrcBuffer, void* pmfxSurface)
{
	bool bRet = false;

	mfxFrameData* pData = &(((mfxFrameSurface1*)pmfxSurface)->Data);
	USHORT pitch = pData->Pitch;
	mfxVideoParam* pVideoParam = (mfxVideoParam*)m_pEncParams; 

	if(1 == m_imgChannelCount)
	{
		//y
		for (int i = 0; i < pVideoParam->mfx.FrameInfo.CropH; i++)
		{
			memcpy(pData->Y + i * pitch, pSrcBuffer + i * pVideoParam->mfx.FrameInfo.CropW, pVideoParam->mfx.FrameInfo.CropW);
		}
	}
	else if (3 == m_imgChannelCount)
	{
		//y
		for (int i = 0; i < pVideoParam->mfx.FrameInfo.CropH; i++)
		{
			memcpy(pData->Y + i * pitch, pSrcBuffer + i * pVideoParam->mfx.FrameInfo.CropW, pVideoParam->mfx.FrameInfo.CropW);
		}
		//uv
		unsigned char* pSrcBufferUV = pSrcBuffer + pVideoParam->mfx.FrameInfo.CropW * pVideoParam->mfx.FrameInfo.CropW;
		for (int i = 0; i < pVideoParam->mfx.FrameInfo.CropH / 2; ++i)
		{
			memcpy(pData->UV + i * pitch, pSrcBufferUV + i * pVideoParam->mfx.FrameInfo.CropW, pVideoParam->mfx.FrameInfo.CropW);
		}
	}
	else
	{
		return bRet;
	}

	bRet = true;
	return bRet;
}

int CIntelMSDKEncoder::GetMemSurfaceCount()
{
	return (int)m_videoMemHandle.m_vecpSurfaces.size();
}

int CIntelMSDKEncoder::GetBufferSizeInKB()
{
	return ((mfxVideoParam*)m_pEncParams)->mfx.BufferSizeInKB;
}


bool CIntelMSDKEncoder::LockVideoMem(int iPos)
{
	bool bRet = false;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameSurface1* pSurface = (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iPos]);
	if (iPos >= m_videoMemHandle.m_vecpSurfaces.size())
		return bRet;

	mfxStatus sts = MFX_ERR_NONE;

	sts = pFrameAllocator->Lock(pFrameAllocator->pthis,
		pSurface->Data.MemId,
		&(pSurface->Data));

	if (sts == MFX_ERR_NONE)
		bRet = true;
	return bRet;
}

bool CIntelMSDKEncoder::UnlockVideoMem(int iPos)
{
	bool bRet = false;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameSurface1* pSurface = (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iPos]);
	if (iPos >= m_videoMemHandle.m_vecpSurfaces.size())
		return bRet;

	mfxStatus sts = MFX_ERR_NONE;

	sts = pFrameAllocator->Unlock(pFrameAllocator->pthis,
		pSurface->Data.MemId,
		&(pSurface->Data));

	if (sts == MFX_ERR_NONE)
		bRet = true;
	return bRet;
}

int CIntelMSDKEncoder::GetFreeSurface(std::vector<void*> vecpSurfaces)
{
	auto it = std::find_if(vecpSurfaces.begin(), vecpSurfaces.end(), [](const void* pSurface) {
		return 0 ==((mfxFrameSurface1*)pSurface)->Data.Locked;
		});

	if (it == vecpSurfaces.end())
		return MFX_ERR_NOT_FOUND;
	else return it - vecpSurfaces.begin();
}

int CIntelMSDKEncoder::GetFreeTaskIndex(void* pTaskPool, USHORT usTaskCount)
{
	if (nullptr == pTaskPool)
		return MFX_ERR_NULL_PTR;

	Task* pTasks = (Task*)pTaskPool;
	for (int i = 0; i < usTaskCount; ++i)
	{
		if (!pTasks[i].syncp)
			return i;
	}
	return MFX_ERR_NOT_FOUND;
}
