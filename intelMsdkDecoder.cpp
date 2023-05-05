#include "intelMsdkDecoder.h"
#include <vector>
#include <numeric>
#include "common_utils.h"
#include <cstdio>
#include "addAVIHeader.h"
#include <string>
#include "mfxjpeg.h"
#include "mfxvideo++.h"

bool DecoderParam::SetJpegParam(USHORT imageDstWidth, USHORT imageDstHeight, int imgChannelCount)
{
	bool bRet = false;

	m_uiSrcType = MFX_CODEC_JPEG;
	m_uiDstType = MFX_FOURCC_NV12;
	m_usImageDstHeight = imageDstHeight;
	m_usImageDstWidth = imageDstWidth;
	m_imgChannelCount = imgChannelCount;

	bRet = true;
	return bRet;
}

bool DecoderParam::SetH264Param(USHORT imageDstWidth, USHORT imageDstHeight, int imgChannelCount)
{
	bool bRet = false;

	m_uiSrcType = MFX_CODEC_AVC;
	m_uiDstType = MFX_FOURCC_NV12;
	m_usImageDstHeight = imageDstHeight;
	m_usImageDstWidth = imageDstWidth;
	m_imgChannelCount = imgChannelCount;

	bRet = true;
	return bRet;
}

bool DecoderParam::Clone(const DecoderParam& decoderParam)
{
	bool bRet = false;

	m_usImageDstWidth = decoderParam.m_usImageDstWidth;
	m_usImageDstHeight = decoderParam.m_usImageDstHeight;
	m_uiSrcType = decoderParam.m_uiSrcType;
	m_uiDstType = decoderParam.m_uiDstType;
	m_imgChannelCount = decoderParam.m_imgChannelCount;

	bRet = true;
	return bRet;
}

intelMSDKdecoder::~intelMSDKdecoder()
{
	if (m_pSession != nullptr)
		unInit();
}

bool intelMSDKdecoder::Init(const DecoderParam& param)
{
	bool bRet = false;

	//m_param.Clone(param);
	m_pSession = new MFXVideoSession;
	m_pDecParams = new mfxVideoParam;
	m_videoMemHandle.m_pFrameAllocator = new mfxFrameAllocator;
	m_videoMemHandle.m_pResponse = new mfxFrameAllocResponse;
	m_videoMemHandle.m_imgChannelCount = param.m_imgChannelCount;
	CreateDecoder(param);
	bRet = InitVideoMemHandle();
	
	return bRet;
}

bool intelMSDKdecoder::unInit()
{
	return CloseDecoder();
}

bool intelMSDKdecoder::CloseDecoder()
{
	bool bRet = false;

	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);


	((MFXVideoSession*)m_pSession)->Close();
	pFrameAllocator->Free(pFrameAllocator->pthis, pResponse);
	Release();
	delete (MFXVideoSession*)m_pSession;
	delete (mfxVideoParam*)m_pDecParams;
	delete pFrameAllocator;
	delete pResponse;
	m_pSession = nullptr;
	m_pDecParams = nullptr;
	m_videoMemHandle.m_pFrameAllocator = nullptr;
	m_videoMemHandle.m_pResponse = nullptr;
	for (int i = 0; i < m_videoMemHandle.m_vecpSurfaces.size(); ++i)
	{
		delete (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[i]);
		m_videoMemHandle.m_vecpSurfaces[i] = nullptr;
	}
	return bRet;
}

bool intelMSDKdecoder::DecodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxStatus sts = MFX_ERR_NONE;

	if (MFX_CODEC_JPEG == pDecParam->mfx.CodecId)
	{
		DecodeBufferJpeg(vecpSrcBuffer, vecpDstBuffer, veciSrcLength);
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

//bool intelMSDKdecoder::DecodeBufferJpeg(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
//{
//	bool bRet = false;
//	mfxStatus sts = MFX_ERR_NONE;
//	int ivideoSurfaceCount = m_VideoMemHandle.pmfxSurfaces.size();
//	int itotalFrameCount = vecpSrcBuffer.size();
//	int ibpp = 12; //nv12
//
//	std::vector<Task> vecTasks(itotalFrameCount);
//	USHORT BufferSizeInKB;
//	if (MFX_CODEC_AVC == m_mfxDecParams.mfx.FrameInfo.FourCC)
//	{
//		return bRet;
//	}
//	else
//	{
//		BufferSizeInKB = 4 + (m_mfxDecParams.mfx.FrameInfo.CropW * m_mfxDecParams.mfx.FrameInfo.CropH * ibpp / 8 + 1023) / 1024;
//	}
//	for (int i = 0; i < itotalFrameCount; ++i)
//	{
//		vecTasks[i] = {};
//		memset(&vecTasks[i].mfxBS, 0, sizeof(&vecTasks[i].mfxBS));
//		vecTasks[i].mfxBS.MaxLength = BufferSizeInKB * 1024;
//		vecTasks[i].mfxBS.Data = (unsigned char*)vecpSrcBuffer[i];
//		vecTasks[i].mfxBS.DataOffset = 0;
//		vecTasks[i].mfxBS.DataLength = veciSrcLength[i];
//	}
//
//	std::vector<mfxFrameSurface1*> surfaces_out(ivideoSurfaceCount, NULL);
//	for (int i = 0; i < ivideoSurfaceCount; ++i)
//	{
//		surfaces_out[i] = &m_VideoMemHandle.pmfxSurfaces[i];
//	}
//
//	mfxTime tStart, tEnd;
//	mfxGetTime(&tStart);
//
//	int iDecSurfIdx = 0;
//	int iTaskIdx = 0;
//	int iFirstSyncTask = 0;
//	int iDstBufferindex = 0;
//	int iFrameCount = 0;
//
//	//ReadBitStream(&vecTasks[0].mfxBS, vecpSrcBuffer[0], veciSrcLength[0]);
//
//	// Stage 1: Main decoding loop
//	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
//	{
//		iTaskIdx = GetFreeTaskIndex(vecTasks.data(), itotalFrameCount);
//		iDecSurfIdx = GetFreeWorkingSurfaceIndex(surfaces_out);
//		//iDecSurfIdx = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);   // Find free frame surface
//		if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iDecSurfIdx)
//		{
//			// No more free tasks, need to sync
//			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(vecTasks[iFirstSyncTask].syncp, 600000);
//			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//			LockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//			VideoMemToSysMem(surfaces_out[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//
//			//SysMemToVideoMem((unsigned char*)vecpSrcBuffer[iDstBufferindex++], &m_VideoMemHandle.pmfxSurfaces[iEncSurfIdx]);
//			//if (iDstBufferindex == vecpSrcBuffer.size())
//			//	break;
//
//			UnlockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//			//veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
//			//vecTasks[iFirstSyncTask].mfxBS.DataLength = 0;
//
//			//vecTasks[iFirstSyncTask].syncp = NULL;
//			iFirstSyncTask = (iFirstSyncTask + 1) % itotalFrameCount;
//			++iFrameCount;
//			//if (iFrameCount == itotalFrameCount)
//			//	break;
//		}
//		else
//		{
//			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iDecSurfIdx, MFX_ERR_MEMORY_ALLOC);
//			++iDstBufferindex;
//			for (;;)
//			{
//				sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession), &vecTasks[iTaskIdx].mfxBS,
//					&m_VideoMemHandle.pmfxSurfaces[iDecSurfIdx], &surfaces_out[iDecSurfIdx], &vecTasks[iTaskIdx].syncp);
//				/*sts = m_decoder.DecodeFrameAsync(&vecTasks[iTaskIdx].mfxBS,
//					&m_VideoMemHandle.pmfxSurfaces[iDecSurfIdx], &surfaces_out[iDecSurfIdx], &vecTasks[iTaskIdx].syncp);*/
//				if (MFX_ERR_NONE < sts && !vecTasks[iTaskIdx].syncp)
//				{    // Repeat the call if warning and no output
//					if (MFX_WRN_DEVICE_BUSY == sts)
//						MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
//				}
//				else if (MFX_ERR_NONE < sts && vecTasks[iTaskIdx].syncp)
//				{
//					sts = MFX_ERR_NONE;     // Ignore warnings if output is available
//					break;
//				}
//				else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
//				{
//					// Allocate more bitstream buffer memory here if needed...
//					break;
//				}
//				else
//					break;
//			}
//			if (iDstBufferindex == vecpSrcBuffer.size())
//				break;
//		}
//	}
//
//	// MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
//	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	// Stage 2: Retrieve the buffered encoded frames
//	while (MFX_ERR_NONE <= sts && iFirstSyncTask < itotalFrameCount)
//	{
//		iTaskIdx = GetFreeTaskIndex(vecTasks.data(), itotalFrameCount);
//		iDecSurfIdx = GetFreeWorkingSurfaceIndex(surfaces_out);
//		//iDecSurfIdx = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);   // Find free frame surface
//		if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iDecSurfIdx)
//		{
//			// No more free tasks, need to sync
//			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(vecTasks[iFirstSyncTask].syncp, 600000);
//			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//			LockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//			VideoMemToSysMem(surfaces_out[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//
//			UnlockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//			//veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
//			//vecTasks[iFirstSyncTask].mfxBS.DataLength = 0;
//
//			//vecTasks[iFirstSyncTask].syncp = NULL;
//			++iFirstSyncTask;
//			++iFrameCount;
//			//if (iFrameCount == itotalFrameCount)
//			//	break;
//		}
//		else
//		{
//			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iDecSurfIdx, MFX_ERR_MEMORY_ALLOC);
//
//			for (;;)
//			{
//				sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession), NULL,
//					&m_VideoMemHandle.pmfxSurfaces[iDecSurfIdx], &surfaces_out[iDecSurfIdx], &vecTasks[iTaskIdx].syncp);
//				/*sts = m_decoder.DecodeFrameAsync(NULL,
//					&m_VideoMemHandle.pmfxSurfaces[iDecSurfIdx], &surfaces_out[iDecSurfIdx], &vecTasks[iTaskIdx].syncp);*/
//				if (MFX_ERR_NONE < sts && !vecTasks[iTaskIdx].syncp)
//				{    // Repeat the call if warning and no output
//					if (MFX_WRN_DEVICE_BUSY == sts)
//						MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
//				}
//				else if (MFX_ERR_NONE < sts && vecTasks[iTaskIdx].syncp)
//				{
//					sts = MFX_ERR_NONE;     // Ignore warnings if output is available
//					break;
//				}
//				else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
//				{
//					// Allocate more bitstream buffer memory here if needed...
//					break;
//				}
//				else
//					break;
//			}
//		}
//	}
//
//	// MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
//	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//// Stage 3: Sync all remaining tasks in task pool
//	//while (vecTasks[iFirstSyncTask].syncp)
//	//{
//	//	sts = m_session.SyncOperation(vecTasks[iFirstSyncTask].syncp, 60000000);
//	//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//	LockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//	//	VideoMemToSysMem(surfaces_out[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//
//	//	UnlockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//	//	//veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
//	//	//vecTasks[iFirstSyncTask].mfxBS.DataLength = 0;
//	//	//vecTasks[iFirstSyncTask].syncp = NULL;
//	//	iFirstSyncTask = (iFirstSyncTask + 1) % itotalFrameCount;
//	//	++iFrameCount;
//	//}
//
//	mfxGetTime(&tEnd);
//	double elapsed = TimeDiffMsec(tEnd, tStart);
//	printf("%.3f\n", elapsed / vecpSrcBuffer.size());
//
//	bRet = true;
//	return bRet;
//
//	//bool bRet = false;
//
//	//int ivideoSurfaceCount = m_VideoMemHandle.pmfxSurfaces.size();
//	//int itotalFrameCount = vecpSrcBuffer.size();
//	//mfxStatus sts = MFX_ERR_NONE;
//
//	//std::vector<Task> vecTasks(itotalFrameCount);
//
//
//	//for (int i = 0; i < itotalFrameCount; ++i)
//	//{
//	//	vecTasks[i] = {};
//	//	memset(&vecTasks[i].mfxBS, 0, sizeof(&vecTasks[i].mfxBS));
//	//	vecTasks[i].mfxBS.MaxLength = veciSrcLength[i];
//	//	vecTasks[i].mfxBS.Data = (unsigned char*)vecpSrcBuffer[i];
//	//	vecTasks[i].mfxBS.DataOffset = 0;
//	//	vecTasks[i].mfxBS.DataLength = veciSrcLength[i];
//	//}
//
//	//std::vector<mfxFrameSurface1*> surfaces_out(ivideoSurfaceCount,NULL);
//
//	//mfxTime tStart, tEnd;
//	//mfxGetTime(&tStart);
//
//	//int iDecSurfIdx = 0;
//	//int iTaskIdx = 0;
//	//int iFirstSyncTask = 0;
//	//int iDstBufferindex = 0;
//	//int iFrameCount = 0;
//
//
//	//// Stage 1: Main decoding loop
//	//while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
//	//{
//	//	iTaskIdx = GetFreeTaskIndex(vecTasks.data(), itotalFrameCount);
//	//	//iDecSurfIdx = GetFreeWorkingSurfaceIndex(surfaces_out);
//	//	iDecSurfIdx = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);   // Find free frame surface
//	//	if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iDecSurfIdx)
//	//	{
//	//		sts = m_session.SyncOperation(vecTasks[iFirstSyncTask].syncp, 600000);
//	//		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//		//LockVideoMem(iDecSurfIdx);
//	//		LockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//	//		//VideoMemToSysMem(&m_VideoMemHandle.pmfxSurfaces[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//	//		VideoMemToSysMem(surfaces_out[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//
//	//		//UnlockVideoMem(iDecSurfIdx);
//	//		UnlockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//	//		++iFirstSyncTask;
//	//		++iFrameCount;
//	//	}
//	//	else
//	//	{
//	//		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iDecSurfIdx, MFX_ERR_MEMORY_ALLOC);
//	//		++iDstBufferindex;
//	//		for (;;)
//	//		{
//	//			sts = m_decoder.DecodeFrameAsync(&vecTasks[iTaskIdx].mfxBS,
//	//								&m_VideoMemHandle.pmfxSurfaces[iDecSurfIdx], &surfaces_out[iDecSurfIdx], &vecTasks[iTaskIdx].syncp);
//	//			if (MFX_ERR_NONE < sts && !vecTasks[iTaskIdx].syncp)
//	//			{    // Repeat the call if warning and no output
//	//				if (MFX_WRN_DEVICE_BUSY == sts)
//	//					MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
//	//			}
//	//			else if (MFX_ERR_NONE < sts && vecTasks[iTaskIdx].syncp)
//	//			{
//	//				sts = MFX_ERR_NONE;     // Ignore warnings if output is available
//	//				break;
//	//			}
//	//			else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
//	//			{
//	//				// Allocate more bitstream buffer memory here if needed...
//	//				break;
//	//			}
//	//			else
//	//				break;
//	//		}
//	//		if (iDstBufferindex == vecpSrcBuffer.size())
//	//			break;
//	//	}
//	//}
//
//	//// MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
//	//MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	//MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//// Stage 2: Retrieve the buffered encoded frames
//	//while (MFX_ERR_NONE <= sts && iFirstSyncTask < itotalFrameCount)
//	//{
//	//	iTaskIdx = GetFreeTaskIndex(vecTasks.data(), itotalFrameCount);
//	//	//iDecSurfIdx = GetFreeWorkingSurfaceIndex(surfaces_out);
//	//	iDecSurfIdx = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);   // Find free frame surface
//	//	if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iDecSurfIdx)
//	//	{
//	//		// No more free tasks, need to sync
//	//		sts = m_session.SyncOperation(vecTasks[iFirstSyncTask].syncp, 600000);
//	//		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//		//LockVideoMem(iDecSurfIdx);
//	//		LockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//	//		VideoMemToSysMem(&m_VideoMemHandle.pmfxSurfaces[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//	//		//VideoMemToSysMem(surfaces_out[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//
//	//		//UnlockVideoMem(iDecSurfIdx);
//	//		UnlockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//	//		++iFirstSyncTask;
//	//		++iFrameCount;
//	//	}
//	//	else
//	//	{
//	//		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iDecSurfIdx, MFX_ERR_MEMORY_ALLOC);
//
//	//		for (;;)
//	//		{
//	//			// Encode a frame asychronously (returns immediately)
//	//			sts = m_decoder.DecodeFrameAsync(NULL,
//	//				&m_VideoMemHandle.pmfxSurfaces[iDecSurfIdx], &surfaces_out[iDecSurfIdx], &vecTasks[iTaskIdx].syncp);
//	//			if (MFX_ERR_NONE < sts && !vecTasks[iTaskIdx].syncp)
//	//			{    // Repeat the call if warning and no output
//	//				if (MFX_WRN_DEVICE_BUSY == sts)
//	//					MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
//	//			}
//	//			else if (MFX_ERR_NONE < sts && vecTasks[iTaskIdx].syncp)
//	//			{
//	//				sts = MFX_ERR_NONE;     // Ignore warnings if output is available
//	//				break;
//	//			}
//	//			else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
//	//			{
//	//				// Allocate more bitstream buffer memory here if needed...
//	//				break;
//	//			}
//	//			else
//	//				break;
//	//		}
//	//	}
//	//}
//
//	//// MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
//	//MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	//MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//// Stage 3: Sync all remaining tasks in task pool
//	////while (vecTasks[iFirstSyncTask].syncp)
//	////{
//	////	sts = m_session.SyncOperation(vecTasks[iFirstSyncTask].syncp, 60000000);
//	////	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	////	LockVideoMem(iFirstSyncTask % ivideoSurfaceCount);
//
//	////	VideoMemToSysMem(surfaces_out[iFirstSyncTask % ivideoSurfaceCount], vecpDstBuffer[iFrameCount]);
//
//	////	UnlockVideoMem(iFirstSyncTask% ivideoSurfaceCount);
//
//	////	++iFirstSyncTask;
//	////	++iFrameCount;
//	////}
//
//	//mfxGetTime(&tEnd);
//	//printf("%.3f\n", TimeDiffMsec(tEnd, tStart) / vecpDstBuffer.size());
//	//bRet = true;
//	//return bRet;
//}

bool intelMSDKdecoder::CreateDecoder(const DecoderParam& param)
{
	bool bRet = false;
	mfxStatus sts = MFX_ERR_NONE;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
	mfxIMPL impl = MFX_IMPL_HARDWARE;
	mfxVersion ver = { { 0, 1 } };
	sts = Initialize(impl, ver, (MFXVideoSession*)m_pSession, pFrameAllocator);

	memset(pDecParam, 0, sizeof(mfxVideoParam));
	pDecParam->mfx.CodecId = param.m_uiSrcType;
	pDecParam->mfx.FrameInfo.FourCC = param.m_uiDstType;
	pDecParam->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	pDecParam->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	pDecParam->mfx.FrameInfo.Width = MSDK_ALIGN16(param.m_usImageDstWidth);
	pDecParam->mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == pDecParam->mfx.FrameInfo.PicStruct) ?
										MSDK_ALIGN16(param.m_usImageDstHeight) : MSDK_ALIGN32(param.m_usImageDstHeight);
	pDecParam->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	pDecParam->mfx.FrameInfo.CropX = 0;
	pDecParam->mfx.FrameInfo.CropY = 0;
	pDecParam->mfx.FrameInfo.CropW = param.m_usImageDstWidth;
	pDecParam->mfx.FrameInfo.CropH = param.m_usImageDstHeight;


	if (MFX_CODEC_JPEG == param.m_uiSrcType)
	{
		pDecParam->mfx.CodecProfile = MFX_PROFILE_JPEG_BASELINE;

		//pDecParam->mfx.NumThread = 1;
		pDecParam->AsyncDepth = 1;
		pDecParam->mfx.InterleavedDec = MFX_SCANTYPE_INTERLEAVED;
		pDecParam->mfx.JPEGChromaFormat = MFX_CHROMAFORMAT_YUV420;
		pDecParam->mfx.JPEGColorFormat = MFX_JPEG_COLORFORMAT_YCbCr;
	}
	else if (MFX_CODEC_AVC == param.m_uiSrcType)
	{
		pDecParam->mfx.CodecId = MFX_CODEC_AVC;
		pDecParam->mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;
		pDecParam->mfx.CodecLevel = MFX_LEVEL_AVC_3;
		pDecParam->mfx.FrameInfo.BitDepthChroma = 8;
		pDecParam->mfx.FrameInfo.BitDepthLuma = 8;
		pDecParam->mfx.FrameInfo.FrameRateExtN = 30;
		pDecParam->mfx.FrameInfo.FrameRateExtD = 1;
		//pDecParam->mfx.FrameInfo.AspectRatioH = 1;
		//pDecParam->mfx.FrameInfo.AspectRatioW = 1;
		pDecParam->mfx.MaxDecFrameBuffering = m_videoMemHandle.m_vecpSurfaces.size();
	}

	//sts = MFXVideoDECODE_Query(*((MFXVideoSession*)m_pSession), pDecParam, pDecParam);

	bRet = true;
	return bRet;
}

bool intelMSDKdecoder::InitVideoMemHandle()
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
	mfxStatus sts = MFX_ERR_NONE;
	mfxFrameAllocRequest decRequest;
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

	sts = pFrameAllocator->Alloc(pFrameAllocator->pthis, &decRequest, pResponse);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	USHORT nEncSurfNum = pResponse->NumFrameActual;

	for (int i = 0; i < nEncSurfNum; ++i) {
		mfxFrameSurface1* pFrameSurface = new mfxFrameSurface1();
		m_videoMemHandle.m_vecpSurfaces.push_back(pFrameSurface);
		memset(m_videoMemHandle.m_vecpSurfaces[i], 0, sizeof(mfxFrameSurface1));
		pFrameSurface->Info = pDecParam->mfx.FrameInfo;
		pFrameSurface->Data.MemId = pResponse->mids[i];      // MID (memory id) represent one video NV12 surface
		//ClearYUVSurfaceVMem(m_VideoMemHandle.pmfxSurfaces[i].Data.MemId);
		//	m_VideoMemHandle.pmfxSurfaces[i].Data.Y = (mfxU8*)m_VideoMemHandle.pmfxSurfaces[i].Data.MemId;
		//m_VideoMemHandle.pmfxSurfaces[i].Data.U = m_VideoMemHandle.pmfxSurfaces[i].Data.Y 
		//									+ m_VideoMemHandle.pmfxSurfaces[i].Info.Width * m_VideoMemHandle.pmfxSurfaces[i].Info.Height;
		//m_VideoMemHandle.pmfxSurfaces[i].Data.V = m_VideoMemHandle.pmfxSurfaces[i].Data.U + 1;
	//	m_VideoMemHandle.pmfxSurfaces[i].Data.Pitch = m_VideoMemHandle.pmfxSurfaces[i].Info.Width;
	}

	//if (MFX_CODEC_JPEG  == pDecParam->mfx.CodecId)
	//{
	//	sts = MFXVideoDECODE_Init(*((MFXVideoSession*)m_pSession), pDecParam);
	//	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	//}


	bRet = true;
	return bRet;
}



bool intelMSDKdecoder::VideoMemToSysMem(void* pmfxSurface, void* pDstBuffer)
{
	bool bRet = false;
	mfxFrameInfo* pInfo = &((mfxFrameSurface1*)pmfxSurface)->Info;
	mfxFrameData* pData = &((mfxFrameSurface1*)pmfxSurface)->Data;
	UINT nByteWrite;
	USHORT i, j, h, w, pitch;
	mfxU8* ptr;
	mfxStatus sts = MFX_ERR_NONE;

	if (pInfo->CropH > 0 && pInfo->CropW > 0)
	{
		w = pInfo->CropW;
		h = pInfo->CropH;
	}
	else
	{
		w = pInfo->Width;
		h = pInfo->Height;
	}

	if(GRAY == m_videoMemHandle.m_imgChannelCount)
	{
		//y
		for (i = 0; i < pInfo->CropH; i++) {
			memcpy((unsigned char*)pDstBuffer + pInfo->CropW * i,
				pData->Y + (pInfo->CropY * pData->Pitch / 1 + pInfo->CropX) +
				i * pData->Pitch + 0, pInfo->CropW);
		}
	}
	else if (COLOR == m_videoMemHandle.m_imgChannelCount)
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

bool intelMSDKdecoder::LockVideoMem(int iPos)
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

bool intelMSDKdecoder::UnlockVideoMem(int iPos)
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

int intelMSDKdecoder::GetFreeSurface(std::vector<void*> vecpSurfaces)
{
	auto it = std::find_if(vecpSurfaces.begin(), vecpSurfaces.end(), [](const void* pSurface) {
		return 0 == ((mfxFrameSurface1*)pSurface)->Data.Locked;
		});

	if (it == vecpSurfaces.end())
		return MFX_ERR_NOT_FOUND;
	else return it - vecpSurfaces.begin();
}

//bool intelMSDKdecoder::AllocSysMem()
//{
//	bool bRet = false;
//
//	//USHORT BufferSizeInKB = m_mfxEncParams.mfx.BufferSizeInKB;
//	USHORT BufferSizeInKB = 4 + (m_mfxDecParams.mfx.FrameInfo.CropW * m_mfxDecParams.mfx.FrameInfo.CropH * m_usBitsPerPixel / 8 + 1023) / 1024;
//	m_pSrcBuffer = new mfxU8[BufferSizeInKB * 1024];
//	memset((void*)m_pSrcBuffer, 128, BufferSizeInKB * 1024);
//
//
//	//USHORT width = (USHORT)MSDK_ALIGN32(m_mfxEncParams.mfx.FrameInfo.Width);
//	//USHORT height = (USHORT)MSDK_ALIGN32(m_mfxEncParams.mfx.FrameInfo.Height);
//	//UINT surfaceSize = width * height * m_usBitsPerPixel / 8;
//	//m_pSrcBuffer = new mfxU8[surfaceSize];
//
//
//
//	bRet = true;
//	return bRet;
//}


//mfxStatus intelMSDKdecoder::ReadBitStream(void* pBitStream, void* pSrcbuffer, int& iReadBytesCount, int iTotalBytes)
//{
//	mfxBitstream* pBS = (mfxBitstream*)pBitStream;
//
//	memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
//	pBS->DataOffset = 0;
//
//	if (iReadBytesCount == iTotalBytes)
//		return MFX_ERR_MORE_DATA;
//
//	unsigned char* pStart = (unsigned char*)pSrcbuffer + iReadBytesCount;
//	if (pBS->MaxLength - pBS->DataLength > (iTotalBytes - iReadBytesCount))
//	{
//		memcpy(pBS->Data + pBS->DataLength, pStart, iTotalBytes - iReadBytesCount);
//		iReadBytesCount += (iTotalBytes - iReadBytesCount);
//	}
//	else 
//	{
//		memcpy(pBS->Data + pBS->DataLength, pStart, pBS->MaxLength - pBS->DataLength);
//		iReadBytesCount += (pBS->MaxLength - pBS->DataLength);
//	}
//	int nBytesRead = pBS->MaxLength - pBS->DataLength;
//
//	pBS->DataLength += nBytesRead;
//
//	return MFX_ERR_NONE;
//
//}

//int intelMSDKdecoder::GetFreeWorkingSurfaceIndex(const std::vector<mfxFrameSurface1*>& pSurfacesPool)
//{
//	for (int i = 0; i < pSurfacesPool.size(); ++i) {
//		if (0 == pSurfacesPool[i]->Data.Locked)
//			return i;
//	}
//	return MFX_ERR_NOT_FOUND;;
//}

//int intelMSDKdecoder::GetFreeSyncPoint(const std::vector<mfxSyncPoint>& vecSyncPoints)
//{
//	for (int i = 0; i < vecSyncPoints.size(); ++i) {
//		if (NULL == vecSyncPoints[i])
//			return i;
//	}
//	return -1;
//}

//bool intelMSDKdecoder::DecodeSingleArray(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength, int iStartIndex, int iEndIndex, int iIndexSurface)
//{
//	bool bRet = false;
//
//	mfxStatus sts = MFX_ERR_NONE;
//	mfxBitstream mfxBS;
//	memset(&mfxBS, 0, sizeof(mfxBS));
//	USHORT BufferSizeInKB;
//
//	mfxBS.MaxLength = 1024 * 1024;
//	std::vector<mfxU8> bstData(mfxBS.MaxLength);
//	mfxBS.Data = bstData.data();
//
//
//	mfxSyncPoint syncp;
//	mfxFrameSurface1* pmfxOutSurface = NULL;
//	int iSurfaceIndex = 0;
//	int iFrameIndex = iStartIndex;
//	UINT uiDecFrameCount = iStartIndex;
//
//	mfxBS.Data = (mfxU8*)vecpSrcBuffer[iFrameIndex];
//	mfxBS.DataLength = veciSrcLength[iFrameIndex];
//	++iFrameIndex;
//	//ReadBitStream(&mfxBS, vecpSrcBuffer[iFrameIndex], veciSrcLength[iFrameIndex]);
//
//	//
//	// Stage 1: Main decoding loop
//	//
//	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts)
//	{
//		if (MFX_WRN_DEVICE_BUSY == sts)
//			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
//
//		if (MFX_ERR_MORE_DATA == sts) {
//			mfxBS.Data = (mfxU8*)vecpSrcBuffer[iFrameIndex];
//			mfxBS.DataLength = veciSrcLength[iFrameIndex];
//			++iFrameIndex;
//			//if (ReadBitStream(&mfxBS, vecpSrcBuffer[iFrameIndex], veciSrcLength[iFrameIndex])) 	 // Read more data into input bit stream
//			//	sts = MFX_ERR_NONE;
//			if (iFrameIndex == iEndIndex)
//				break;
//		}
//
//		if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
//			//iSurfaceIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);        // Find free frame surface
//			iSurfaceIndex = iIndexSurface;
//			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);
//		}
//		// Decode a frame asychronously (returns immediately)
//		//  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
//		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
//			&mfxBS, &m_VideoMemHandle.pmfxSurfaces[iSurfaceIndex], &pmfxOutSurface, &syncp);
//		//sts = m_decoder.DecodeFrameAsync(&mfxBS, &m_VideoMemHandle.pmfxSurfaces[iSurfaceIndex], &pmfxOutSurface, &syncp);
//
//		// Ignore warnings if output is available,
//		// if no output and no action required just repeat the DecodeFrameAsync call
//		if (MFX_ERR_NONE < sts && syncp)
//			sts = MFX_ERR_NONE;
//
//		if (MFX_ERR_NONE == sts)
//			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, 600000000000000000);      // Synchronize. Wait until decoded frame is ready
//
//		if (MFX_ERR_NONE == sts) {
//			// Surface locking required when read/write video surfaces
//			sts = m_VideoMemHandle.mfxFrameAllocator.Lock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//
//			VideoMemToSysMem(pmfxOutSurface, vecpDstBuffer[uiDecFrameCount]);
//
//			sts = m_VideoMemHandle.mfxFrameAllocator.Unlock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//
//			++uiDecFrameCount;
//		}
//	}
//
//	// MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
//	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//
//	// Stage 2: Retrieve the buffered decoded frames
//	//
//	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts)
//	{
//		if (MFX_WRN_DEVICE_BUSY == sts)
//			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
//
//		//iSurfaceIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);         // Find free frame surface
//		iSurfaceIndex = iIndexSurface;
//		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);
//
//		// Decode a frame asychronously (returns immediately)
//		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
//			NULL, &m_VideoMemHandle.pmfxSurfaces[iSurfaceIndex], &pmfxOutSurface, &syncp);
//		//sts = m_decoder.DecodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[iSurfaceIndex], &pmfxOutSurface, &syncp);
//
//		// Ignore warnings if output is available,
//		// if no output and no action required just repeat the DecodeFrameAsync call
//		if (MFX_ERR_NONE < sts && syncp)
//			sts = MFX_ERR_NONE;
//
//		if (MFX_ERR_NONE == sts)
//			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, 600000000);      // Synchronize. Waits until decoded frame is ready
//
//		if (MFX_ERR_NONE == sts) {
//			// Surface locking required when read/write video surfaces
//			sts = m_VideoMemHandle.mfxFrameAllocator.Lock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//
//			VideoMemToSysMem(&pmfxOutSurface, vecpDstBuffer[uiDecFrameCount]);
//
//			sts = m_VideoMemHandle.mfxFrameAllocator.Unlock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//
//			++uiDecFrameCount;
//		}
//	}
//	// MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
//	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	return bRet;
//}
//




bool intelMSDKdecoder::DecodeBufferH264(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
{
	bool bRet = false;
	mfxVideoParam* pDecParam = (mfxVideoParam*)m_pDecParams;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	addAVIHeader addAVIHeader1(pDecParam->mfx.FrameInfo.CropW, pDecParam->mfx.FrameInfo.CropH, 24, "H264",
		pDecParam->mfx.FrameInfo.FrameRateExtN / pDecParam->mfx.FrameInfo.FrameRateExtD, NULL);
	addAVIHeader1.AddVideoFrameHeader(vecpSrcBuffer, veciSrcLength);
	std::vector<unsigned char>& fullFrames = addAVIHeader1.GetDstBuffer();
	mfxStatus sts = MFX_ERR_NONE;
	mfxBitstream mfxBS;
	memset(&mfxBS, 0, sizeof(mfxBS));

	mfxBS.MaxLength = addAVIHeader1.GetNowIndex();
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
			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

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
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, 600000000000000000);      // Synchronize. Wait until decoded frame is ready

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
			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
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
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, 600000000);      // Synchronize. Waits until decoded frame is ready

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



//bool intelMSDKdecoder::DecodeBufferLoopH264(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
//{
//	bool bRet = false;
//
//	mfxStatus sts = MFX_ERR_NONE;
//	mfxBitstream mfxBS;
//	memset(&mfxBS, 0, sizeof(mfxBS));
//	mfxBS.MaxLength = 1024 * 1024;// (4 + (m_param.m_usImageDstWidth * m_param.m_usImageDstWidth * m_usBitsPerPixel / 8 + 1023)) * 1024;
//	std::vector<mfxU8> bstData(mfxBS.MaxLength);
//	mfxBS.Data = bstData.data();
//
//	mfxSyncPoint syncp;
//	mfxFrameSurface1* pmfxOutSurface = NULL;
//	int nIndex = 0;
//	UINT nFrame = 0;
//	int iIndexReadIntoBS = 0;
//
//
//	//
//	// Stage 1: Main decoding loop
//	//
//	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) {
//		if (MFX_WRN_DEVICE_BUSY == sts)
//			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
//
//		if (MFX_ERR_MORE_DATA == sts) {
//			if (ReadFrameIntoBitStream(&mfxBS, vecpSrcBuffer[iIndexReadIntoBS], veciSrcLength[iIndexReadIntoBS])) {
//				iIndexReadIntoBS++;
//				sts = MFX_ERR_NONE;
//			}
//			else {
//				sts = MFX_ERR_MORE_DATA;
//			}
//		//	sts = ReadBitStreamData(&mfxBS, fSource);       // Read more data into input bit stream
//			MSDK_BREAK_ON_ERROR(sts);
//		}
//
//		if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
//			nIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);        // Find free frame surface
//			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);
//		}
//		// Decode a frame asychronously (returns immediately)
//		//  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
//		sts = m_decoder.DecodeFrameAsync(&mfxBS, &m_VideoMemHandle.pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);
//
//		// Ignore warnings if output is available,
//		// if no output and no action required just repeat the DecodeFrameAsync call
//		if (MFX_ERR_NONE < sts && syncp)
//			sts = MFX_ERR_NONE;
//
//		if (MFX_ERR_NONE == sts)
//			sts = m_session.SyncOperation(syncp, 60000);      // Synchronize. Wait until decoded frame is ready
//
//		if (MFX_ERR_NONE == sts) {
//			sts = m_VideoMemHandle.mfxFrameAllocator.Lock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//
//			VideoMemToSysMem(pmfxOutSurface, vecpDstBuffer[nFrame]);
//			//sts = WriteRawFrame(pmfxOutSurface, fSink);
//			//MSDK_BREAK_ON_ERROR(sts);
//
//			sts = m_VideoMemHandle.mfxFrameAllocator.Unlock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//			++nFrame;	
//		}
//	}
//
//	// MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
//	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//	//
//	// Stage 2: Retrieve the buffered decoded frames
//	//
//	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts) {
//		if (MFX_WRN_DEVICE_BUSY == sts)
//			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
//
//		nIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);        // Find free frame surface
//		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);
//
//		// Decode a frame asychronously (returns immediately)
//		sts = m_decoder.DecodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);
//
//		// Ignore warnings if output is available,
//		// if no output and no action required just repeat the DecodeFrameAsync call
//		if (MFX_ERR_NONE < sts && syncp)
//			sts = MFX_ERR_NONE;
//
//		if (MFX_ERR_NONE == sts)
//			sts = m_session.SyncOperation(syncp, 60000);      // Synchronize. Waits until decoded frame is ready
//
//		if (MFX_ERR_NONE == sts) {
//			sts = m_VideoMemHandle.mfxFrameAllocator.Lock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//
//			VideoMemToSysMem(pmfxOutSurface, vecpDstBuffer[nFrame]);
//			//sts = WriteRawFrame(pmfxOutSurface, fSink);
//			//MSDK_BREAK_ON_ERROR(sts);
//
//			sts = m_VideoMemHandle.mfxFrameAllocator.Unlock(m_VideoMemHandle.mfxFrameAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
//			MSDK_BREAK_ON_ERROR(sts);
//			++nFrame;
//		}
//	}
//	// MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
//	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//}

//bool intelMSDKdecoder::DecodeBufferLoop2(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
//{
//	bool bRet = false;
//	int indexArrayLength = vecpSrcBuffer.size() / m_VideoMemHandle.pmfxSurfaces.size();
//
//#pragma omp parallel
//	{
//#pragma omp for
//
//		for (int i = 0; i < vecpSrcBuffer.size(); i += indexArrayLength)
//		{
//			int iEndIndex = i + indexArrayLength;
//			if (i + indexArrayLength > veciSrcLength.size())
//			{
//				iEndIndex = veciSrcLength.size();
//			}
//			DecodeSingleArray(vecpSrcBuffer, vecpDstBuffer, veciSrcLength, i, iEndIndex, i);
//		}
//	}
//	bRet = true;
//	return bRet;
//}

bool intelMSDKdecoder::DecodeBufferJpeg(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
{
	//mfxTime tStart, tEnd, tStart1, tEnd1;

	//mfxGetTime(&tStart);

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
			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

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
		//sts = m_decoder.DecodeFrameAsync(&bitStream, &m_VideoMemHandle.pmfxSurfaces[iSurfaceIndex], &pmfxOutSurface, &syncp);

		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, 600000000000000000);

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
			MSDK_SLEEP(1); 
		iSurfaceIndex = GetFreeSurface(m_videoMemHandle.m_vecpSurfaces);
		//iSurfaceIndex = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, iSurfaceIndex, MFX_ERR_MEMORY_ALLOC);

		sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
			NULL, (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[iSurfaceIndex]), &pmfxOutSurface, &syncp);
		//sts = m_decoder.DecodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[iSurfaceIndex], &pmfxOutSurface, &syncp);

		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(syncp, 600000000);

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
	//mfxGetTime(&tEnd);
	//printf("decode函数内：%.3f\n",TimeDiffMsec(tEnd,tStart)/vecpSrcBuffer.size());
	//printf("decode函数内：%.3f\n", TimeDiffMsec(tEnd, tStart1) / vecpSrcBuffer.size());

	return bRet;
}

bool intelMSDKdecoder::DecodeBufferJpeg2(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
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
			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

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
//bool intelMSDKdecoder::DecodeBuffer2(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength)
//{
//	mfxTime tStart, tEnd;
//	mfxGetTime(&tStart);
//
//	bool bRet = false;
//
//	mfxStatus sts = MFX_ERR_NONE;
//	mfxBitstream BS{};
//	memset(&BS, 0, sizeof(BS));
//	std::vector<mfxBitstream> vecBS(m_VideoMemHandle.pmfxSurfaces.size(), BS);
//	std::vector<mfxFrameSurface1*> vecSurfaceWorking(m_VideoMemHandle.pmfxSurfaces.size(), NULL);
//	std::vector<mfxSyncPoint> vecSyncPoints(m_VideoMemHandle.pmfxSurfaces.size(), NULL);
//
//
//	int iDecFrameIndex = 0;
//	int iAsyncFrameIndex = 0;
//	int iTotalFrameCounts = vecpSrcBuffer.size();
//	int iFreeSyncPoint = 0;
//	int iSurfaceCount = m_VideoMemHandle.pmfxSurfaces.size();
//
//	while (iDecFrameIndex < iTotalFrameCounts) {
//		iFreeSyncPoint = GetFreeSyncPoint(vecSyncPoints);
//		if (iFreeSyncPoint >= 0&&iAsyncFrameIndex<iTotalFrameCounts) {
//			vecBS[iFreeSyncPoint].Data = (mfxU8*)vecpSrcBuffer[iAsyncFrameIndex];
//			vecBS[iFreeSyncPoint].DataLength = veciSrcLength[iAsyncFrameIndex];
//			vecBS[iFreeSyncPoint].MaxLength = vecBS[iFreeSyncPoint].DataLength;
//			vecBS[iFreeSyncPoint].DataOffset = 0;
//			for (;;) {
//				sts = MFXVideoDECODE_DecodeFrameAsync(*((MFXVideoSession*)m_pSession),
//					&vecBS[iFreeSyncPoint],
//					&m_VideoMemHandle.pmfxSurfaces[iFreeSyncPoint],
//					&vecSurfaceWorking[iFreeSyncPoint], &vecSyncPoints[iFreeSyncPoint]);
//				/*sts = m_decoder.DecodeFrameAsync(&vecBS[iFreeSyncPoint],
//					&m_VideoMemHandle.pmfxSurfaces[iFreeSyncPoint], &vecSurfaceWorking[iFreeSyncPoint], &vecSyncPoints[iFreeSyncPoint]);*/
//				if (MFX_ERR_NONE < sts && !vecSyncPoints[iFreeSyncPoint]) {    // Repeat the call if warning and no output
//					if (MFX_WRN_DEVICE_BUSY == sts)
//						MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
//				}
//				else if (MFX_ERR_NONE < sts
//					&& vecSyncPoints[iFreeSyncPoint]) {
//					sts = MFX_ERR_NONE;     // Ignore warnings if output is available
//					iAsyncFrameIndex++;
//					break;
//				}
//				else if (MFX_ERR_NONE == sts) {
//					iAsyncFrameIndex++;
//					break;
//				}
//			}
//		}
//		else
//		{
//			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(vecSyncPoints[iDecFrameIndex %iSurfaceCount], 600000);
//
//			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
//
//			LockVideoMem(iDecFrameIndex % iSurfaceCount);
//
//			VideoMemToSysMem(vecSurfaceWorking[iDecFrameIndex % iSurfaceCount], vecpDstBuffer[iDecFrameIndex]);
//
//			UnlockVideoMem(iDecFrameIndex % iSurfaceCount);
//
//			vecSyncPoints[iDecFrameIndex % iSurfaceCount] = NULL;
//			iDecFrameIndex++;
//		}
//	}
//
//	if(iDecFrameIndex==iTotalFrameCounts)
//		bRet = true;
//
//	mfxGetTime(&tEnd);
//	printf("decode函数内：%.3f\n", TimeDiffMsec(tEnd, tStart) / iTotalFrameCounts);
//	return bRet;
//}
