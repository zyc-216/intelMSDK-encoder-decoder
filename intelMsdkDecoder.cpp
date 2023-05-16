#include "CIntelMSDKDecoder.h"
#include <numeric>
#include <cstdio>
#include <string>
#include <chrono>
#include <thread>
#include <mfxjpeg.h>



#pragma warning(disable:4244)
#pragma warning(disable:4305)


CIntelMSDKDecoder::CIntelMSDKDecoder()
{
	m_uiSrcType = MFX_CODEC_AVC;
	m_uiDstType = MFX_FOURCC_NV12;
	m_usImageDstHeight = 1080;
	m_usImageDstWidth = 1920;
	m_imgChannelCount = 1;
	m_uiFrameRate = 25;
	m_pSession = NULL;
	m_pDecParams = NULL;
	m_pAddAVIHeader = nullptr;
	//m_pDeviceThis.pDevice = nullptr;
	//m_pDeviceThis.pthis = nullptr;

}

CIntelMSDKDecoder::~CIntelMSDKDecoder()
{
	if (m_pSession != nullptr)
		UnInit();
}

bool CIntelMSDKDecoder::Init(USHORT iDstWidth, USHORT iDstHeight, UINT uiSrcType, UINT uiDstType, UINT uiFrameRate, int iImageChannelCount)
{
	bool bRet = false;

	m_usImageDstWidth = iDstWidth;
	m_usImageDstHeight = iDstHeight;
	m_uiSrcType = uiSrcType;
	m_uiDstType = uiDstType;
	m_uiFrameRate = uiFrameRate;
	m_imgChannelCount = iImageChannelCount;


	CloseDecoder();
	CreateDecoder();
	bRet = InitVideoMemHandle();
	
	return bRet;
}

bool CIntelMSDKDecoder::CreateDecoder()
{
	bool bRet = false;
	
	MFXVideoSession* pSession = NULL;
	pSession = new MFXVideoSession;
	m_pSession = pSession;
	m_pDecParams = new mfxVideoParam;
	m_videoMemHandle.m_pFrameAllocator = new mfxFrameAllocator;
	m_videoMemHandle.m_pResponse = new mfxFrameAllocResponse;
	//m_pDeviceThis.pDevice = new CDeviceManagerD3D11;
	m_pAddAVIHeader = new CAddAVIHeader;

	mfxStatus sts = MFX_ERR_NONE;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
	pResponse->NumFrameActual = 0;
	mfxIMPL impl = MFX_IMPL_HARDWARE;
	mfxVersion ver = { { 0, 1 } };
	sts = Initialize(impl, ver, (MFXVideoSession*)m_pSession, pFrameAllocator);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	//m_pDeviceThis.pthis = pFrameAllocator->pthis;

	memset(pDecParam, 0, sizeof(mfxVideoParam));
	pDecParam->mfx.CodecId = m_uiSrcType;
	pDecParam->mfx.FrameInfo.FourCC = m_uiDstType;
	pDecParam->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	pDecParam->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	pDecParam->mfx.FrameInfo.Width = MSDK_ALIGN16(m_usImageDstWidth);
	pDecParam->mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == pDecParam->mfx.FrameInfo.PicStruct) ?
		MSDK_ALIGN16(m_usImageDstHeight) : MSDK_ALIGN32(m_usImageDstHeight);
	pDecParam->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	pDecParam->mfx.FrameInfo.CropX = 0;
	pDecParam->mfx.FrameInfo.CropY = 0;
	pDecParam->mfx.FrameInfo.CropW = m_usImageDstWidth;
	pDecParam->mfx.FrameInfo.CropH = m_usImageDstHeight;


	if (MFX_CODEC_JPEG == m_uiSrcType)
	{
		pDecParam->mfx.CodecProfile = MFX_PROFILE_JPEG_BASELINE;
		pDecParam->AsyncDepth = 1;
		pDecParam->mfx.InterleavedDec = MFX_SCANTYPE_INTERLEAVED;
		pDecParam->mfx.JPEGChromaFormat = MFX_CHROMAFORMAT_YUV420;
		pDecParam->mfx.JPEGColorFormat = MFX_JPEG_COLORFORMAT_YCbCr;
	}
	else if (MFX_CODEC_AVC == m_uiSrcType)
	{
		pDecParam->mfx.CodecId = MFX_CODEC_AVC;
		pDecParam->mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;
		pDecParam->mfx.CodecLevel = MFX_LEVEL_AVC_3;
		pDecParam->mfx.FrameInfo.BitDepthChroma = 8;
		pDecParam->mfx.FrameInfo.BitDepthLuma = 8;
		pDecParam->mfx.FrameInfo.FrameRateExtN = m_uiFrameRate;
		pDecParam->mfx.FrameInfo.FrameRateExtD = 1;
		pDecParam->mfx.MaxDecFrameBuffering = (mfxU16)m_videoMemHandle.m_vecpSurfaces.size();
	}
	sts = MFXVideoDECODE_Query(*((MFXVideoSession*)m_pSession), pDecParam, pDecParam);
	bRet = true;
	return bRet;
}

bool CIntelMSDKDecoder::UnInit()
{
	return CloseDecoder();
}

bool CIntelMSDKDecoder::CloseDecoder()
{
	bool bRet = false;

	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
	

	//if(m_pSession )
	//	
	//	((MFXVideoSession*)m_pSession)->Close();



	if (m_pSession)
	{
		delete (MFXVideoSession*)m_pSession;
		m_pSession = nullptr;
	}	
	if (m_pDecParams)
	{
		delete (mfxVideoParam*)m_pDecParams;
		m_pDecParams = nullptr;
	}
	if (pFrameAllocator)
	{
		pFrameAllocator->Free(pFrameAllocator->pthis, pResponse);
		delete pFrameAllocator;
		pFrameAllocator = nullptr;
		m_videoMemHandle.m_pFrameAllocator = nullptr;
	}
	if (pResponse)
	{
		delete pResponse;
		pResponse = nullptr;
		m_videoMemHandle.m_pResponse = nullptr;
	}
	for (int i = 0; i < m_videoMemHandle.m_vecpSurfaces.size(); ++i)
	{
		if (NULL != m_videoMemHandle.m_vecpSurfaces[i])
		{
			delete (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[i]);
			m_videoMemHandle.m_vecpSurfaces[i] = nullptr;
		}
	}

	Release();


	if (m_pAddAVIHeader)
	{
		delete (CAddAVIHeader*)m_pAddAVIHeader;
		m_pAddAVIHeader = nullptr;
	}
	
	return bRet;
}

bool CIntelMSDKDecoder::DecodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxStatus sts = MFX_ERR_NONE;

	if (MFX_CODEC_JPEG == pDecParam->mfx.CodecId)
	{
		DecodeBufferJpeg2(vecpSrcBuffer, vecpDstBuffer, veciSrcLength);
	}
	else if (MFX_CODEC_AVC == pDecParam->mfx.CodecId)
	{
		DecodeBufferH264(vecpSrcBuffer, vecpDstBuffer, veciSrcLength);
	}
	else
	{
		return bRet;
	}
	bRet = true;
	return bRet;
}



bool CIntelMSDKDecoder::InitVideoMemHandle()
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
	mfxStatus sts = MFX_ERR_NONE;
	mfxFrameAllocRequest decRequest;

	if (NULL != pDecParam && NULL != pFrameAllocator && NULL != pResponse)
	{
		memset(&decRequest, 0, sizeof(decRequest));
		sts = MFXVideoDECODE_QueryIOSurf(*((MFXVideoSession*)m_pSession), pDecParam, &decRequest);
		MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		if (decRequest.NumFrameMin < 4 && decRequest.NumFrameSuggested < 4)
		{
			decRequest.NumFrameMin = 4;
			decRequest.NumFrameSuggested = 4;
		}
		decRequest.Type |= WILL_READ;  // This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application
		
		//sts = simple_alloc(&m_pDeviceThis, &decRequest, pResponse);
		sts = pFrameAllocator->Alloc(pFrameAllocator->pthis, &decRequest, pResponse);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		USHORT nEncSurfNum = pResponse->NumFrameActual;

		for (int i = 0; i < nEncSurfNum; ++i)
		{
			mfxFrameSurface1* pFrameSurface = new mfxFrameSurface1();
			m_videoMemHandle.m_vecpSurfaces.push_back(pFrameSurface);
			memset(m_videoMemHandle.m_vecpSurfaces[i], 0, sizeof(mfxFrameSurface1));
			pFrameSurface->Info = pDecParam->mfx.FrameInfo;
			pFrameSurface->Data.MemId = pResponse->mids[i];      // MID (memory id) represent one video NV12 surface
		}
	}

	bRet = true;
	return bRet;
}



bool CIntelMSDKDecoder::VideoMemToSysMem(void* pmfxSurface, void* pDstBuffer)
{
	bool bRet = false;
	mfxFrameInfo* pInfo = &((mfxFrameSurface1*)pmfxSurface)->Info;
	mfxFrameData* pData = &((mfxFrameSurface1*)pmfxSurface)->Data;
	USHORT i = 0;
	USHORT usHeight = 0;
	USHORT usWidth = 0;
	mfxStatus sts = MFX_ERR_NONE;

	if (pInfo->CropH > 0 && pInfo->CropW > 0)
	{
		usWidth = pInfo->CropW;
		usHeight = pInfo->CropH;
	}
	else
	{
		usWidth = pInfo->Width;
		usHeight = pInfo->Height;
	}

	if(1 == m_imgChannelCount)
	{
		//y
		for (i = 0; i < pInfo->CropH; i++) {
			memcpy((unsigned char*)pDstBuffer + pInfo->CropW * i,
				pData->Y + (pInfo->CropY * pData->Pitch / 1 + pInfo->CropX) +
				i * pData->Pitch + 0, pInfo->CropW);
		}
	}
	else if (3 == m_imgChannelCount)
	{
		//y
		for (i = 0; i < pInfo->CropH; i++) {
			memcpy((unsigned char*)pDstBuffer + pInfo->CropW * i,
				pData->Y + (pInfo->CropY * pData->Pitch / 1 + pInfo->CropX) +
				i * pData->Pitch + 0, pInfo->CropW);
		}
		//uv
		unsigned char* pDstBufferUV = (unsigned char*)pDstBuffer + pInfo->CropW * pInfo->CropH;
		for (int i = 0; i < pInfo->CropH / 2; i++) {
			memcpy(pDstBufferUV+ pInfo->CropW * i, 
				pData->UV + (pInfo->CropY * pData->Pitch / 2 + pInfo->CropX) +
				i * pData->Pitch + 0, pInfo->CropW);
		}
	}
	else
	{
		return bRet;
	}
	bRet = true;
	return bRet;
}

bool CIntelMSDKDecoder::LockVideoMem(int iPos)
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

bool CIntelMSDKDecoder::UnlockVideoMem(int iPos)
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

int CIntelMSDKDecoder::GetFreeSurface(std::vector<void*> vecpSurfaces)
{
	/*
	for (int i = 0; i < vecpSurfaces.size(); i++)
	{
		mfxFrameSurface1* pSurface = (mfxFrameSurface1*)vecpSurfaces[i];
		if(pSurface->Data.Locked)
	}
	*/
	auto it = std::find_if(vecpSurfaces.begin(), vecpSurfaces.end(), [](const void* pSurface) 
		{
		return 0 == ((mfxFrameSurface1*)pSurface)->Data.Locked;
		});

	if (it == vecpSurfaces.end())
		return MFX_ERR_NOT_FOUND;
	else 
		return it - vecpSurfaces.begin();
}


bool CIntelMSDKDecoder::DecodeBufferH264(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator); 

	CAddAVIHeader* pAddAVIHeader = (CAddAVIHeader*)m_pAddAVIHeader;
	pAddAVIHeader->Initialize(pDecParam->mfx.FrameInfo.CropW, pDecParam->mfx.FrameInfo.CropH, 24, "H264",
		pDecParam->mfx.FrameInfo.FrameRateExtN / pDecParam->mfx.FrameInfo.FrameRateExtD, NULL);
	pAddAVIHeader->AddVideoFrameHeader(vecpSrcBuffer, veciSrcLength);
	std::vector<unsigned char>& fullFrames = pAddAVIHeader->GetDstBuffer();
	mfxStatus sts = MFX_ERR_NONE;
	mfxBitstream mfxBS;
	memset(&mfxBS, 0, sizeof(mfxBS));

	mfxBS.MaxLength = pAddAVIHeader->GetNowIndex();
	mfxBS.Data = fullFrames.data(); 
	mfxBS.DataLength = mfxBS.MaxLength;
	
	sts = MFXVideoDECODE_DecodeHeader(*((MFXVideoSession*)m_pSession), &mfxBS, pDecParam);
	sts = MFXVideoDECODE_Query(*((MFXVideoSession*)m_pSession), pDecParam, pDecParam);
	sts = MFXVideoDECODE_Init(*((MFXVideoSession*)m_pSession), pDecParam);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfxSyncPoint syncp;
	mfxFrameSurface1* pmfxOutSurface = NULL;
	int iSurfaceIndex = 0;
	int iFrameIndex = 0;
	UINT uiDecFrameCount = 0;


	// Stage 1: Main decoding loop
	//
	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts)
	{
		if (MFX_WRN_DEVICE_BUSY == sts)
			//Sleep(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (MFX_ERR_MORE_DATA == sts ) {
			memmove(mfxBS.Data, mfxBS.Data + mfxBS.DataOffset, mfxBS.DataLength);
			mfxBS.DataOffset = 0;
			sts = MFX_ERR_MORE_DATA;
			MSDK_BREAK_ON_ERROR(sts);
		}

		if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
			iSurfaceIndex = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
			//iSurfaceIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);       
			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);
		}
		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
			&mfxBS, (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iSurfaceIndex]), &pmfxOutSurface, &syncp);

		// Ignore warnings if output is available,
		// if no output and no action required just repeat the DecodeFrameAsync call
		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, WAIT_60000MS);      // Synchronize. Wait until decoded frame is ready

		if (MFX_ERR_NONE == sts) {
			// Surface locking required when read/write video surfaces
			sts = pFrameAllocator->Lock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			VideoMemToSysMem(pmfxOutSurface, vecpDstBuffer[uiDecFrameCount]);

			sts = pFrameAllocator->Unlock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			++uiDecFrameCount;
		}
	}

	// MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	//
	// Stage 2: Retrieve the buffered decoded frames
	//
	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts) 
	{
		if (MFX_WRN_DEVICE_BUSY == sts)
			//Sleep(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		iSurfaceIndex = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
		//iSurfaceIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);         
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);
		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
			NULL, (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iSurfaceIndex]), &pmfxOutSurface, &syncp);

		// Ignore warnings if output is available,
		// if no output and no action required just repeat the DecodeFrameAsync call
		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, WAIT_60000MS);      // Synchronize. Waits until decoded frame is ready

		if (MFX_ERR_NONE == sts) {
			sts = pFrameAllocator->Lock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			VideoMemToSysMem(&pmfxOutSurface, vecpDstBuffer[uiDecFrameCount]);

			sts = pFrameAllocator->Unlock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			++uiDecFrameCount;
		}
	}
	// MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	MFXVideoDECODE_Close(*((MFXVideoSession*)m_pSession));
	if (uiDecFrameCount == iFrameIndex)
		bRet = true;

	return bRet;
}


bool CIntelMSDKDecoder::DecodeBufferJpeg2(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxStatus sts = MFX_ERR_NONE;
	mfxBitstream bitStream;
	memset(&bitStream, 0, sizeof(bitStream));
	bitStream.MaxLength = veciSrcLength[0];
	bitStream.Data = (unsigned char*)vecpSrcBuffer[0];
	bitStream.DataLength = bitStream.MaxLength;

	sts = MFXVideoDECODE_Init(*((MFXVideoSession*)m_pSession), pDecParam);
	sts = MFXVideoDECODE_DecodeHeader(*((MFXVideoSession*)m_pSession), &bitStream, pDecParam);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	/* decoding */
	mfxSyncPoint syncp;
	mfxFrameSurface1* pmfxOutSurface = NULL;
	int iSurfaceIndex = 0;
	UINT uiDecFrameCount = 0;

	//decoding loop(sync)
	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts)
	{
		if (MFX_WRN_DEVICE_BUSY == sts)
			//Sleep(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
			iSurfaceIndex = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);
		}

		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
			&bitStream, (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iSurfaceIndex]), &pmfxOutSurface, &syncp);

		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, WAIT_60000MS);

		if (MFX_ERR_NONE == sts) {
			sts = pFrameAllocator->Lock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			VideoMemToSysMem(pmfxOutSurface, vecpDstBuffer[uiDecFrameCount]);

			sts = pFrameAllocator->Unlock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);
			++uiDecFrameCount;
			if (uiDecFrameCount == vecpSrcBuffer.size())
				break;
			bitStream.Data = (unsigned char*)vecpSrcBuffer[uiDecFrameCount];
			bitStream.DataOffset = 0;
			bitStream.MaxLength = veciSrcLength[uiDecFrameCount];;
			bitStream.DataLength = bitStream.MaxLength;

		}
	}


	sts = MFXVideoDECODE_Close(*((MFXVideoSession*)m_pSession));

	bRet = true;
	return bRet;
}

bool CIntelMSDKDecoder::DecodeBufferJpeg(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxStatus sts = MFX_ERR_NONE;
	long lTotolBytesCount = std::accumulate(veciSrcLength.begin(), veciSrcLength.end(), 0);
	std::string strTotalBytes((char*)vecpSrcBuffer[0], (char*)vecpSrcBuffer[0] + veciSrcLength[0]);
	for (int i = 1; i < vecpSrcBuffer.size(); ++i) {
		strTotalBytes += std::string((char*)vecpSrcBuffer[i], (char*)vecpSrcBuffer[i] + veciSrcLength[i]);
	}

	//mfxGetTime(&tStart1);
	mfxBitstream bitStream;
	memset(&bitStream, 0, sizeof(bitStream));
	bitStream.MaxLength = lTotolBytesCount;
	bitStream.Data = (unsigned char*)const_cast<char*>(strTotalBytes.data());
	bitStream.DataLength = bitStream.MaxLength;

	sts = MFXVideoDECODE_Init(*((MFXVideoSession*)m_pSession), pDecParam);
	sts = MFXVideoDECODE_DecodeHeader(*((MFXVideoSession*)m_pSession), &bitStream, pDecParam);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	/* decoding */
	mfxSyncPoint syncp;
	mfxFrameSurface1* pmfxOutSurface = NULL;
	int iSurfaceIndex = 0;
	int iFrameIndex = 0;
	UINT uiDecFrameCount = 0;

	// Stage 1: Main decoding loop
	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts)
	{
		if (MFX_WRN_DEVICE_BUSY == sts)
			//Sleep(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (MFX_ERR_MORE_DATA == sts) {
			memmove(bitStream.Data, bitStream.Data + bitStream.DataOffset, bitStream.DataLength);
			bitStream.DataOffset = 0;
			sts = MFX_ERR_MORE_DATA;
			MSDK_BREAK_ON_ERROR(sts);
		}

		if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
			iSurfaceIndex = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
			//iSurfaceIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);
			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);
		}

		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
			&bitStream, (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iSurfaceIndex]), &pmfxOutSurface, &syncp);

		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, WAIT_60000MS);

		if (MFX_ERR_NONE == sts) {
			sts = pFrameAllocator->Lock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			VideoMemToSysMem(pmfxOutSurface, vecpDstBuffer[uiDecFrameCount]);

			sts = pFrameAllocator->Unlock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			++uiDecFrameCount;
		}
	}

	// MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Stage 2: Retrieve the buffered decoded frames
	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts)
	{
		if (MFX_WRN_DEVICE_BUSY == sts)
			//Sleep(1); 
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		iSurfaceIndex = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
		//iSurfaceIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);

		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
			NULL, (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iSurfaceIndex]), &pmfxOutSurface, &syncp);
		//sts = m_decoder.DecodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[iSurfaceIndex], &pmfxOutSurface, &syncp);

		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, WAIT_60000MS);

		if (MFX_ERR_NONE == sts) {
			sts = pFrameAllocator->Lock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			VideoMemToSysMem(&pmfxOutSurface, vecpDstBuffer[uiDecFrameCount]);

			sts = pFrameAllocator->Unlock(pFrameAllocator->pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
			MSDK_BREAK_ON_ERROR(sts);

			++uiDecFrameCount;
		}
	}
	// MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = MFXVideoDECODE_Close(*((MFXVideoSession*)m_pSession));
	if (uiDecFrameCount == iFrameIndex)
		bRet = true;
	return bRet;
}

