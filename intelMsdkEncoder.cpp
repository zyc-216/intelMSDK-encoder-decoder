#include "intelMsdkEncoder.h"
#include <vector>
#include "common_utils.h"
#include <cstdio>
//#include <opencv2/core/core.hpp>
//#include <opencv2/highgui/highgui.hpp>
#include "TimeDiffer.h"
#include "omp.h"
#include "GWAVI.h"
#include "addAVIHeader.h"
#include "mfxjpeg.h"
#include "mfxvideo++.h"
//#include "thread"

#pragma comment(lib,".\\3rdLib\\libmfx_vs2015.lib")
//#pragma comment(lib,".\\3rdLib\\libmfx.lib")
//#pragma comment(lib,".\\3rdLib\\opencv_world455.lib")

intelMSDKencoder::~intelMSDKencoder()
{
	if (m_pSession != nullptr)
		unInit();
}


bool intelMSDKencoder::Init(const EncoderParam& param)
{
	bool bRet = false;

	//m_param.Clone(param);
	m_pSession = new MFXVideoSession;
	m_pEncParams = new mfxVideoParam;
	m_videoMemHandle.m_pFrameAllocator = new mfxFrameAllocator;
	m_videoMemHandle.m_pResponse = new mfxFrameAllocResponse;
	m_videoMemHandle.m_imgChannelCount = param.m_imgChannelCount;
	CreateEncoder(param);
	bRet = InitVideoMemHandle();

	return bRet;
}

bool intelMSDKencoder::unInit()
{
	return CloseEncoder();
}


bool intelMSDKencoder::CreateEncoder(const EncoderParam& param)
{
	bool bRet = false;

	mfxStatus sts = MFX_ERR_NONE;
	mfxIMPL impl = MFX_IMPL_HARDWARE;//MFX_IMPL_VIA_D3D11;
	mfxVersion ver = { { 0, 1 } };
	mfxFrameAllocator* pFrameAllocator =(mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;
	sts = Initialize(impl, ver, (MFXVideoSession*)m_pSession, pFrameAllocator);
	memset(pEncParam, 0, sizeof(mfxVideoParam));

	pEncParam->mfx.FrameInfo.FourCC = param.m_uiSrcType;
	pEncParam->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	pEncParam->mfx.FrameInfo.CropX = 0;
	pEncParam->mfx.FrameInfo.CropY = 0;
	pEncParam->mfx.FrameInfo.CropW = param.m_usImageSrcWidth;
	pEncParam->mfx.FrameInfo.CropH = param.m_usImageSrcHeight;
	pEncParam->mfx.CodecId = param.m_uiDstType;

	if (MFX_CODEC_JPEG == param.m_uiDstType)
	{
		pEncParam->mfx.Quality = param.m_usQuality;
		pEncParam->mfx.Interleaved = MFX_SCANTYPE_INTERLEAVED;
		pEncParam->mfx.RestartInterval = 0;
	}
	else if (MFX_CODEC_AVC == param.m_uiDstType)
	{
		pEncParam->mfx.IdrInterval = 1;
		pEncParam->mfx.GopPicSize = 20;
		pEncParam->mfx.GopRefDist = 1; //不用B帧
		pEncParam->mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;// MFX_PROFILE_AVC_HIGH;//MFX_PROFILE_AVC_CONSTRAINED_HIGH;
		pEncParam->mfx.CodecLevel = MFX_LEVEL_AVC_3;//MFX_LEVEL_AVC_4;
		pEncParam->mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;// MFX_TARGETUSAGE_BALANCED;//MFX_TARGETUSAGE_BEST_QUALITY;
		pEncParam->mfx.GopOptFlag = MFX_GOP_STRICT;//MFX_GOP_CLOSED;
		pEncParam->mfx.RateControlMethod = MFX_RATECONTROL_VBR;//MFX_RATECONTROL_AVBR;//MFX_RATECONTROL_LA;//MFX_RATECONTROL_CQP;
		pEncParam->mfx.FrameInfo.FrameRateExtN = param.m_uiFrameRate;
		pEncParam->mfx.FrameInfo.FrameRateExtD = 1;
		pEncParam->mfx.BRCParamMultiplier = 20;
		pEncParam->mfx.TargetKbps = param.m_usKbps / pEncParam->mfx.BRCParamMultiplier;  //TargetKbps*=BRCParamMultiplier

	}
	pEncParam->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;


	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	pEncParam->mfx.FrameInfo.Width = MSDK_ALIGN16(param.m_usImageSrcWidth);
	pEncParam->mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == pEncParam->mfx.FrameInfo.PicStruct) ?
											MSDK_ALIGN16(param.m_usImageSrcHeight) : MSDK_ALIGN32(param.m_usImageSrcHeight);
	pEncParam->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
	pEncParam->AsyncDepth = 1;// m_iMemSurfaceCount;
										

	//sts = MFXVideoENCODE_Query(*((MFXVideoSession*)m_pSession), pEncParam, pEncParam);
	sts = MFXVideoENCODE_Init(*((MFXVideoSession*)m_pSession), pEncParam);


	if (MFX_CODEC_AVC == param.m_uiDstType)
	{
		mfxVideoParam par;
		memset(&par, 0, sizeof(par));
		//sts = m_encoder.GetVideoParam(&par);
		sts = MFXVideoENCODE_GetVideoParam(*((MFXVideoSession*)m_pSession), &par);
		pEncParam->mfx.BufferSizeInKB = par.mfx.BufferSizeInKB;
	}

	bRet = true;
	return bRet;
}




//
//bool intelMSDKencoder::EncodeBufferFile(void* pSrc, const char* pDstFileName, int& iDstLength)
//{
//	bool bRet = false;
//
//	//FILE* fSource = NULL;
//	FILE* fSink = NULL;
//	//fopen_s(&fSource, pSrcFileName, "rb");
//	fopen_s(&fSink, pDstFileName, "wb");
//
//	//LoadRawFrameToSysMem(m_pSrcBuffer, fSource);
//
//	//默认使用第一个内存块作为输出
//	int sysMemID = 0;
//	EncodeBuffer(pSrc, m_vecPtrDstBuffer[sysMemID], iDstLength);
//	WriteEncodedStreamToDisk(m_vecPtrDstBuffer[sysMemID], iDstLength, fSink);
//	
//	//if (fSource) fclose(fSource);
//	if (fSink) fclose(fSink);
//
//	bRet = true;
//	return bRet;
//}

bool intelMSDKencoder::EncodeBufferLoop(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength)
{
	bool bRet = false;

	mfxStatus sts = MFX_ERR_NONE;
	int ibpp = 12;
	int iMemSurfaceCount = m_videoMemHandle.m_vecpSurfaces.size();
	int vecpSrcBufferIdx = 0;
	int nFirstSyncTask = 0;
	int encodedFrameIdx = 0;
	int tatalFrameCount = vecpSrcBuffer.size();
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
			if (NULL == vecSync[i])   //找到空闲的显存块
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
			//sts = m_encoder.EncodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[freeSurfaceIdx], &vecBitStream[freeSurfaceIdx], &vecSync[freeSurfaceIdx]);
			//	if (MFX_WRN_DEVICE_BUSY == sts)
			//	{
			//		MSDK_SLEEP(1);
			//	}
			//	else
			//	{
			//		break;
			//	}
			//} while (1);
		}
		else
		{
			((MFXVideoSession*)m_pSession)->SyncOperation(vecSync[nFirstSyncTask], 100000000);
			vecSync[nFirstSyncTask] = NULL;
			vecBitStream[nFirstSyncTask].Data = NULL;
			veciDstLength[encodedFrameIdx] = vecBitStream[nFirstSyncTask].DataLength;
			++encodedFrameIdx;
			vecBitStream[nFirstSyncTask].DataLength = 0;
			nFirstSyncTask = (nFirstSyncTask + 1) % iMemSurfaceCount;
		}
	}

	//编完loop中剩余帧
	for (int i = 0; i < iMemSurfaceCount; ++i)
	{
		((MFXVideoSession*)m_pSession)->SyncOperation(vecSync[i], 100000000);
		veciDstLength[encodedFrameIdx] = vecBitStream[i].DataLength;
		++encodedFrameIdx;
	}

	if (encodedFrameIdx == tatalFrameCount)
		bRet = true;

	bRet = true;
	return bRet;
}

bool intelMSDKencoder::EncodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength)
{
	//mfxTime tStart, tEnd;
	//mfxGetTime(&tStart);
	bool bRet = false;

	int itotalFrameCount = vecpDstBuffer.size();
	int ibpp = 12;
	std::vector<Task> vecTasks(itotalFrameCount);
	USHORT BufferSizeInKB;
	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;
	mfxStatus sts = MFX_ERR_NONE;
	//sts = MFXVideoENCODE_Init(*((MFXVideoSession*)m_pSession), pEncParam);
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
		//iEncSurfIdx = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);   // Find free frame surface
		if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iEncSurfIdx)
		{
			// No more free tasks, need to sync
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(vecTasks[iFirstSyncTask].syncp, 60000000);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
			//vecTasks[iFirstSyncTask].mfxBS.DataLength = 0;

			//vecTasks[iFirstSyncTask].syncp = NULL;
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
						MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
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
		//iEncSurfIdx = GetFreeSurfaceIndex(m_VideoMemHandle.pmfxSurfaces);   // Find free frame surface
		if (MFX_ERR_NOT_FOUND == iTaskIdx || MFX_ERR_NOT_FOUND == iEncSurfIdx)
		{
			sts = ((MFXVideoSession*)m_pSession)->SyncOperation(vecTasks[iFirstSyncTask].syncp, 60000000);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
			veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
			//vecTasks[iFirstSyncTask].mfxBS.DataLength = 0;
			//vecTasks[iFirstSyncTask].syncp = NULL;
			//iFirstSyncTask = (iFirstSyncTask + 1) % itotalFrameCount;
			++iFirstSyncTask;
			++iFrameCount;
		}
		else
		{
			for (;;)
			{
				//sts = m_encoder.EncodeFrameAsync(NULL, NULL, &vecTasks[iTaskIdx].mfxBS, &vecTasks[iTaskIdx].syncp);
				sts = MFXVideoENCODE_EncodeFrameAsync(*((MFXVideoSession*)m_pSession), NULL, NULL, &vecTasks[iTaskIdx].mfxBS, &vecTasks[iTaskIdx].syncp);
				if (MFX_ERR_NONE < sts && !vecTasks[iTaskIdx].syncp)
				{    // Repeat the call if warning and no output
					if (MFX_WRN_DEVICE_BUSY == sts)
						MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
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

	//// Stage 3: Sync all remaining tasks in task pool
	//while (vecTasks[iFirstSyncTask].syncp)
	//{
	//	sts = m_session.SyncOperation(vecTasks[iFirstSyncTask].syncp, 60000000);
	//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	//	veciDstLength[iFrameCount] = vecTasks[iFirstSyncTask].mfxBS.DataLength;
	//	//vecTasks[iFirstSyncTask].mfxBS.DataLength = 0;
	//	//vecTasks[iFirstSyncTask].syncp = NULL;
	//	iFirstSyncTask = (iFirstSyncTask + 1) % itotalFrameCount;
	//	++iFrameCount;
	//}
	//MFXVideoENCODE_Close(*((MFXVideoSession*)m_pSession));
	bRet = true;
	//mfxGetTime(&tEnd);
	//printf("encode函数内：%.3f\n", TimeDiffMsec(tEnd, tStart) / vecpSrcBuffer.size());
	return bRet;
}

bool intelMSDKencoder::JpegsToFile(std::vector<void*>& vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName)
{
	bool bRet = false;
	for (int i = 0; i < vecpBuffer.size(); ++i)
	{
		FILE* fSink = nullptr;
		string jpegPath = string(pstrFileName) + "\\" + to_string(i) + ".jpeg";
		fopen_s(&fSink, jpegPath.c_str(), "wb+");
		fwrite(vecpBuffer[i], 1, veciLength[i], fSink);
		fclose(fSink);
	}
	bRet = true;
	return bRet;
}

bool intelMSDKencoder::AviToFile(std::vector<void*> vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName)
{
	bool bRet = false;

	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;

	GWAVI aviWriter(pstrFileName, pEncParam->mfx.FrameInfo.CropW, pEncParam->mfx.FrameInfo.CropH, 24, "H264",
		pEncParam->mfx.FrameInfo.FrameRateExtN / pEncParam->mfx.FrameInfo.FrameRateExtD, NULL);
	for (int i = 0; i < vecpBuffer.size(); ++i)
	{
		aviWriter.AddVideoFrame((unsigned char*)vecpBuffer[i], veciLength[i]);
	}
	aviWriter.Finalize();


	bRet = true;
	return bRet;
}



//bool Encoder::LockVideoMem()
//{
//	bool bRet = false;
//
//	mfxStatus sts = MFX_ERR_NONE;
//
//	sts = m_VideoMemHandle.mfxFrameAllocator.Lock(m_VideoMemHandle.mfxFrameAllocator.pthis,
//		m_VideoMemHandle.pmfxSurface.Data.MemId,
//		&(m_VideoMemHandle.pmfxSurface.Data));
//
//	bRet = true;
//	return bRet;
//}
//
//bool Encoder::UnlockVideoMem()
//{
//	bool bRet = false;
//
//	mfxStatus sts = MFX_ERR_NONE;
//
//	sts = m_VideoMemHandle.mfxFrameAllocator.Unlock(m_VideoMemHandle.mfxFrameAllocator.pthis,
//		m_VideoMemHandle.pmfxSurface.Data.MemId,
//		&(m_VideoMemHandle.pmfxSurface.Data));
//
//	bRet = true;
//	return bRet;
//}


//bool intelMSDKencoder::EncodeBuffer(void* pSrc, void* pDst, int& iDstLength)
//{
//	bool bRet = false;
//	mfxStatus sts = MFX_ERR_NONE;
//	bool flag = true;
//
//	mfxBitstream mfxBS;
//	memset(&mfxBS, 0, sizeof(mfxBS));
//	mfxBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
//	USHORT BufferSizeInKB = 4 + (m_param.m_usImageSrcWidth * m_param.m_usImageSrcHeight * m_usBitsPerPixel / 8 + 1023) / 1024;
//	mfxBS.MaxLength = BufferSizeInKB * 1024;
//	mfxBS.Data = (unsigned char*)pDst;
//
//	mfxSyncPoint syncp;
//	//默认使用第一个显存块
//	int videoMemID = 0;
//	do {
//		flag = LockVideoMem(videoMemID);
//		if (false == flag)
//			break;
//
//		flag = SysMemToVideoMem((unsigned char*)pSrc, &m_VideoMemHandle.pmfxSurfaces[videoMemID]);
//		if (false == flag)
//			break;
//
//		flag = UnlockVideoMem(videoMemID);
//		if (false == flag)
//			break;
//
//		do {
//			sts = m_encoder.EncodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[videoMemID], &mfxBS, &syncp);
//			if (MFX_WRN_DEVICE_BUSY == sts)
//			{
//				MSDK_SLEEP(1);
//			}
//			else
//			{
//				break;
//			}
//		} while (1);
//		if (MFX_ERR_NONE != sts)
//			break;
//
//		sts = m_session.SyncOperation(syncp, 10000000000000);
//		if (MFX_ERR_NONE != sts)
//			break;
//	} while (0);
//
//
//	if(MFX_ERR_NONE ==sts&&flag)
//	{
//		iDstLength = mfxBS.DataLength;
//		bRet = true;
//	}
//
//	return bRet;
//}

//bool intelMSDKencoder::EncodeMatBufferArray(std::vector<cv::Mat>& vecMats, int iFirstMatIndex, std::vector<unsigned char*>& vecPtrDstBuffer, std::vector<int>& veciDstLength)
//{
//	bool bRet = false;
//	mfxStatus sts = MFX_ERR_NONE;
//
//	std::vector<mfxSyncPoint> vecSyncp(m_iMemSurfaceCount, NULL);
//	mfxBitstream mfxBS;
//	memset(&mfxBS, 0, sizeof(mfxBS));
//	mfxBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
//	USHORT BufferSizeInKB = 4 + (m_param.m_usImageSrcWidth *m_param.m_usImageSrcWidth * m_usBitsPerPixel / 8 + 1023) / 1024;
//	mfxBS.MaxLength = BufferSizeInKB * 1024;
//	mfxBS.DataLength = 0;
//	std::vector<mfxBitstream> vecBitStream(m_iMemSurfaceCount,mfxBS);
//	for (int i = 0; i < m_iMemSurfaceCount; ++i) 
//	{
//		vecBitStream[i].Data = vecPtrDstBuffer[i];
//	}
//
//	#pragma omp parallel
//	{
//		#pragma omp for
//		
//		for (int i = iFirstMatIndex; i < iFirstMatIndex+m_iMemSurfaceCount; ++i)
//		{
//			LockVideoMem(i - iFirstMatIndex);
//			MatImageToVideoMem(vecMats[i], &m_VideoMemHandle.pmfxSurfaces[i - iFirstMatIndex]);
//			UnlockVideoMem(i - iFirstMatIndex);
//			//do {
//			sts = m_encoder.EncodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[i - iFirstMatIndex], &vecBitStream[i - iFirstMatIndex], &vecSyncp[i - iFirstMatIndex]);
//				//if (MFX_WRN_DEVICE_BUSY == sts)
//				//{
//				//	MSDK_SLEEP(1);
//				//}
//				//else 
//				//{
//				//	break;
//				//}
//			//} while (1);
//			m_session.SyncOperation(vecSyncp[i - iFirstMatIndex], 10000000000000);
//			veciDstLength[i-iFirstMatIndex] = vecBitStream[i - iFirstMatIndex].DataLength;
//		}
//	}
//
//	bRet = true;
//	return bRet;
//}

//bool intelMSDKencoder::EncodeMatBufferLoop(std::vector<cv::Mat>& vecMats, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength)
//{
//	bool bRet = true;
//
//	mfxStatus sts = MFX_ERR_NONE;
//	int vecMatsIdx = 0;
//	int nFirstSyncTask = 0;
//	int encodedFrameIdx = 0;
//	UINT tatalFrameCount = vecMats.size();
//	std::vector<mfxSyncPoint> vecSync(m_iMemSurfaceCount, NULL);
//
//	mfxBitstream mfxBS;
//	memset(&mfxBS, 0, sizeof(mfxBS));
//	mfxBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
//	USHORT BufferSizeInKB = 4 + (m_param.m_usImageSrcWidth * m_param.m_usImageSrcHeight * m_usBitsPerPixel / 8 + 1023) / 1024;
//	mfxBS.MaxLength = BufferSizeInKB * 1024;
//	mfxBS.DataLength = 0;
//	mfxBS.Data = NULL;
//	std::vector<mfxBitstream> vecBitStream(m_iMemSurfaceCount, mfxBS);
//
//	//loop
//	while (vecMatsIdx < tatalFrameCount)
//	{
//		int freeSurfaceIdx = -1;
//		for (int i = 0; i < m_iMemSurfaceCount; ++i)
//		{
//			if (NULL == vecSync[i])   //找到空闲的显存块
//			{
//				freeSurfaceIdx = i;
//				break;
//			}
//		}
//
//		if (freeSurfaceIdx != -1)
//		{
//			LockVideoMem(freeSurfaceIdx);
//			MatImageToVideoMem(vecMats[vecMatsIdx], &m_VideoMemHandle.pmfxSurfaces[freeSurfaceIdx]);
//			UnlockVideoMem(freeSurfaceIdx);
//			vecBitStream[freeSurfaceIdx].Data = (unsigned char*)vecpDstBuffer[vecMatsIdx];
//			vecMatsIdx++;
//			do {
//				sts = m_encoder.EncodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[freeSurfaceIdx], &vecBitStream[freeSurfaceIdx], &vecSync[freeSurfaceIdx]);
//				if (MFX_WRN_DEVICE_BUSY == sts)
//				{
//					MSDK_SLEEP(1);
//				}
//				else
//				{
//					break;
//				}
//			} while (1);
//		}
//		else
//		{
//			m_session.SyncOperation(vecSync[nFirstSyncTask], 100000000);
//			vecSync[nFirstSyncTask] = NULL;
//			vecBitStream[nFirstSyncTask].Data = NULL;
//			veciDstLength[encodedFrameIdx] = vecBitStream[nFirstSyncTask].DataLength;
//			++encodedFrameIdx;
//			vecBitStream[nFirstSyncTask].DataLength = 0;
//			nFirstSyncTask = (nFirstSyncTask + 1) % m_iMemSurfaceCount;
//		}
//	}
//
//	//编完loop中剩余帧
//	for (int i = 0; i < m_iMemSurfaceCount; ++i) 
//	{
//		m_session.SyncOperation(vecSync[i], 100000000);
//		veciDstLength[encodedFrameIdx] = vecBitStream[i].DataLength;
//		++encodedFrameIdx;
//	}
//
//	if (encodedFrameIdx == tatalFrameCount) 
//		bRet = true;
//	
//	return bRet;
//}
//
//
//bool intelMSDKencoder::EncodeMatBufferLoop(std::vector<cv::Mat>& vecMats, std::vector<std::string>& vecpDstBuffer, std::vector<int>& veciDstLength)
//{
//	bool bRet = true;
//
//	mfxStatus sts = MFX_ERR_NONE;
//	int vecMatsIdx = 0;
//	int nFirstSyncTask = 0;
//	int encodedFrameIdx = 0;
//	UINT tatalFrameCount = vecMats.size();
//	std::vector<mfxSyncPoint> vecSync(m_iMemSurfaceCount, NULL);
//
//	mfxBitstream mfxBS;
//	memset(&mfxBS, 0, sizeof(mfxBS));
//	mfxBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
//	USHORT BufferSizeInKB = 4 + (m_param.m_usImageSrcWidth * m_param.m_usImageSrcHeight * m_usBitsPerPixel / 8 + 1023) / 1024;
//	mfxBS.MaxLength = BufferSizeInKB * 1024;
//	mfxBS.DataLength = 0;
//	mfxBS.Data = NULL;
//	std::vector<mfxBitstream> vecBitStream(m_iMemSurfaceCount, mfxBS);
//
//	//loop
//	while (vecMatsIdx < tatalFrameCount)
//	{
//		int freeSurfaceIdx = -1;
//		for (int i = 0; i < m_iMemSurfaceCount; ++i)
//		{
//			if (NULL == vecSync[i])   //找到空闲的显存块
//			{
//				freeSurfaceIdx = i;
//				break;
//			}
//		}
//
//		if (freeSurfaceIdx != -1)
//		{
//			LockVideoMem(freeSurfaceIdx);
//			MatImageToVideoMem(vecMats[vecMatsIdx], &m_VideoMemHandle.pmfxSurfaces[freeSurfaceIdx]);
//			UnlockVideoMem(freeSurfaceIdx);
//			vecBitStream[freeSurfaceIdx].Data = (unsigned char*)vecpDstBuffer[vecMatsIdx].data();
//			vecMatsIdx++;
//			//do {
//			sts = m_encoder.EncodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[freeSurfaceIdx], &vecBitStream[freeSurfaceIdx], &vecSync[freeSurfaceIdx]);
//				//if (MFX_WRN_DEVICE_BUSY == sts)
//				//{
//				//	MSDK_SLEEP(1);
//				//}
//				//else
//				//{
//				//	break;
//				//}
//			//} while (1);
//		}
//		else
//		{
//			m_session.SyncOperation(vecSync[nFirstSyncTask], 100000000);
//			vecSync[nFirstSyncTask] = NULL;
//			vecBitStream[nFirstSyncTask].Data = NULL;
//			veciDstLength[encodedFrameIdx] = vecBitStream[nFirstSyncTask].DataLength;
//			encodedFrameIdx++;
//			vecBitStream[nFirstSyncTask].DataLength = 0;
//			nFirstSyncTask = (nFirstSyncTask + 1) % m_iMemSurfaceCount;
//		}
//	}
//
//	//编完loop中剩余帧
//	for (int i = 0; i < m_iMemSurfaceCount; ++i)
//	{
//		m_session.SyncOperation(vecSync[i], 100000000);
//		vecSync[i] = NULL;
//		veciDstLength[encodedFrameIdx] = vecBitStream[i].DataLength;
//		++encodedFrameIdx;
//		vecBitStream[nFirstSyncTask].Data = NULL;
//		vecBitStream[nFirstSyncTask].DataLength = 0;
//	}
//
//	if (encodedFrameIdx == tatalFrameCount)
//		bRet = true;
//
//	return bRet;
//}

bool intelMSDKencoder::CloseEncoder()
{
	bool bRet = false;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
	((MFXVideoSession*)m_pSession)->Close();
	pFrameAllocator->Free(pFrameAllocator->pthis, pResponse);
	Release();
	delete (MFXVideoSession*)m_pSession;
	delete (mfxVideoParam*)m_pEncParams;
	delete pFrameAllocator;
	delete pResponse;
	m_pSession = nullptr;
	m_pEncParams = nullptr;
	m_videoMemHandle.m_pFrameAllocator = nullptr;
	m_videoMemHandle.m_pResponse = nullptr;
	for (int i = 0; i < m_videoMemHandle.m_vecpSurfaces.size(); ++i)
	{
		delete (mfxFrameSurface1*)(m_videoMemHandle.m_vecpSurfaces[i]);
		m_videoMemHandle.m_vecpSurfaces[i] = nullptr;
	}

	bRet = true;
	return bRet;
}

bool intelMSDKencoder::EncodeBufferH264(std::vector<void*>& vecpSrcBuffer, const char* pstrFileName)
{
	bool bRet = false;

	int iFrameCount = vecpSrcBuffer.size();
	mfxVideoParam* pEncParam = (mfxVideoParam*)m_pEncParams;
	USHORT BufferSizeInKB;
	BufferSizeInKB = pEncParam->mfx.BufferSizeInKB;

	std::vector<void*> vecpDstBuffer(iFrameCount);
	std::vector<int> veciDstLength(iFrameCount);

	unsigned char*	pDst = new unsigned char[BufferSizeInKB * 1024 * iFrameCount];
	memset((void*)pDst, 128, BufferSizeInKB * 1024 * iFrameCount);
	unsigned char* pHeadDst = pDst;
	for (int i = 0; i < iFrameCount; ++i)
	{
		vecpDstBuffer[i] = pDst;
		pDst += BufferSizeInKB * 1024;
	}

	mfxTime tStart, tEnd;
	mfxGetTime(&tStart);
	EncodeBuffer(vecpSrcBuffer, vecpDstBuffer, veciDstLength);
	mfxGetTime(&tEnd);
	printf("encode per frame %.3f\n ms", TimeDiffMsec(tEnd, tStart) / veciDstLength.size());

	GWAVI aviWriter(pstrFileName, pEncParam->mfx.FrameInfo.CropW, pEncParam->mfx.FrameInfo.CropH, 24, "H264",
		pEncParam->mfx.FrameInfo.FrameRateExtN / pEncParam->mfx.FrameInfo.FrameRateExtD, NULL);
	for (int i = 0; i < vecpDstBuffer.size(); ++i)
	{
		aviWriter.AddVideoFrame((unsigned char*)vecpDstBuffer[i], veciDstLength[i]);
	}
	aviWriter.Finalize();

	//addAVIHeader addAVIHeader1(m_mfxEncParams.mfx.FrameInfo.CropW, m_mfxEncParams.mfx.FrameInfo.CropH, 24, "H264",
	//	m_mfxEncParams.mfx.FrameInfo.FrameRateExtN / m_mfxEncParams.mfx.FrameInfo.FrameRateExtD, NULL);
	//addAVIHeader1.AddVideoFrameHeader(vecpDstBuffer, veciDstLength);
	//std::vector<unsigned char>& fullFrames = addAVIHeader1.GetDstBuffer();
	//FILE* fSink = NULL;
	//fopen_s(&fSink, pstrFileName, "wb+");
	//fwrite(fullFrames.data(), 1, addAVIHeader1.GetNowIndex(), fSink);
	//fclose(fSink);

	delete[] pHeadDst;
	bRet = true;
	return bRet;
}

//bool intelMSDKencoder::EncodeMatBuffer(std::vector<cv::Mat>& vecMatImage, void* pDst, int& iDstLength, int iPos)
//{
//	bool bRet = false;
//	if (iPos < 0 || iPos >= vecMatImage.size())
//		return bRet;
//
//	mfxStatus sts = MFX_ERR_NONE;
//	mfxBitstream mfxBS;
//	memset(&mfxBS, 0, sizeof(mfxBS));
//	mfxBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
//	USHORT BufferSizeInKB = 4 + (m_param.m_usImageSrcWidth * m_param.m_usImageSrcHeight * m_usBitsPerPixel / 8 + 1023) / 1024;
//	mfxBS.MaxLength = BufferSizeInKB * 1024;
//	mfxBS.Data = (unsigned char*)pDst;
//
//	mfxSyncPoint syncp;
//
//	//默认用第1个显存块编码单帧
//	int VideoMemID = 0;
//	LockVideoMem(VideoMemID);
//	do
//	{
//		if (false == MatImageToVideoMem(vecMatImage[iPos], &m_VideoMemHandle.pmfxSurfaces[VideoMemID]))
//			break;
//		mfxBS.DataLength = 0;
//		syncp = NULL;
//		iDstLength = 0;
//
//		do {
//			sts = m_encoder.EncodeFrameAsync(NULL, &m_VideoMemHandle.pmfxSurfaces[VideoMemID], &mfxBS, &syncp);
//			if (MFX_WRN_DEVICE_BUSY == sts)
//			{
//				MSDK_SLEEP(1);
//			}
//			else
//			{
//				break;
//			}
//		} while (1);
//		if (MFX_ERR_NONE != sts)
//			break;
//
//		sts = m_session.SyncOperation(syncp, 10000000000000);
//		if (MFX_ERR_NONE != sts)
//			break;
//
//		if (MFX_ERR_NONE == sts)
//		{
//			iDstLength = mfxBS.DataLength;
//			bRet = true;
//		}
//	} while (0);
//	UnlockVideoMem(VideoMemID);
//
//	
//	return bRet;
//}
//
//bool intelMSDKencoder::EncodeMatBufferFile(std::vector<cv::Mat>& vecMatImage, const char* pstrFileName, int& iDstLength, int iPos)
//{
//	bool bRet = false;
//
//
//	//默认用第1个内存块存放编码后单帧
//	int SysMemID = 0;
//	EncodeMatBuffer(vecMatImage, m_vecPtrDstBuffer[SysMemID], iDstLength, iPos);
//
//	FILE* fSink = nullptr;
//	fopen_s(&fSink, pstrFileName, "wb+");
//	if (nullptr == fSink)
//		return bRet;
//	WriteEncodedStreamToDisk(m_vecPtrDstBuffer[SysMemID], iDstLength, fSink);
//
//	if(fSink)
//		fclose(fSink);
//
//	bRet = true;
//	return bRet;
//}


bool intelMSDKencoder::InitVideoMemHandle()
{
	bool bRet = false;

	mfxStatus sts = MFX_ERR_NONE;
	mfxFrameAllocator* pFrameAllocator = (mfxFrameAllocator*)(m_videoMemHandle.m_pFrameAllocator);
	mfxFrameAllocResponse* pResponse = (mfxFrameAllocResponse*)(m_videoMemHandle.m_pResponse);
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

	bRet = true;
	return bRet;
}

//bool intelMSDKencoder::AllocSysMem()
//{
//	bool bRet = false;
//	USHORT BufferSizeInKB;
//	if (MFX_CODEC_AVC == m_param.m_uiDstType)
//	{
//		BufferSizeInKB = m_mfxEncParams.mfx.BufferSizeInKB;
//	}
//	else
//	{
//		BufferSizeInKB = 4 + (m_param.m_usImageSrcHeight * m_param.m_usImageSrcWidth * m_usBitsPerPixel / 8 + 1023) / 1024;
//	}
//	//USHORT BufferSizeInKB = 4 + (m_param.m_usImageSrcWidth * m_param.m_usImageSrcHeight * m_usBitsPerPixel / 8 + 1023) / 1024;
//	//m_pDstBuffer = new mfxU8[BufferSizeInKB * 1024];
//
//	for (int i = 0; i < m_iMemSurfaceCount; ++i) {
//		m_vecPtrDstBuffer.push_back(new mfxU8[BufferSizeInKB * 1024]);
//	}
//
//	USHORT width = (USHORT)MSDK_ALIGN32(m_mfxEncParams.mfx.FrameInfo.Width);
//	USHORT height = (USHORT)MSDK_ALIGN32(m_mfxEncParams.mfx.FrameInfo.Height);
//	UINT surfaceSize = width * height * 
// 
//  / 8;
//	m_pSrcBuffer = new mfxU8[surfaceSize];
//	memset((void*)m_pSrcBuffer, 128, surfaceSize);
//
//
//	bRet = true;
//	return bRet;
//}

//bool intelMSDKencoder::LoadRawFrameToSysMem(unsigned char* pSrcBuffer, FILE* fSource)
//{
//	bool bRet = false;
//	if (nullptr == fSource)
//		return bRet;
//
//	//默认使用第一个内存块
//	int videoMemID = 0;
//
//	USHORT h = m_mfxEncParams.mfx.FrameInfo.Height;
//	USHORT w = m_mfxEncParams.mfx.FrameInfo.Width;
//
//	mfxFrameInfo* pInfo = &m_VideoMemHandle.pmfxSurfaces[videoMemID].Info;
//	mfxFrameData* pData = &m_VideoMemHandle.pmfxSurfaces[videoMemID].Data;
//
//	if (pInfo->CropH > 0 && pInfo->CropW > 0) 
//	{
//		w = pInfo->CropW;
//		h = pInfo->CropH;
//	}
//	else 
//	{
//		w = pInfo->Width;
//		h = pInfo->Height;
//	}
//
//	USHORT pitch = pData->Pitch;
//	mfxU8* ptr = pSrcBuffer +pInfo->CropX + pInfo->CropY * pitch;
//	//y
//	for (int i = 0; i < h; ++i)
//	{
//		fread(ptr + i * pitch, 1, w, fSource);
//	}
//
//
//	bRet = true;
//	return bRet;
//}

bool intelMSDKencoder::SysMemToVideoMem(unsigned char* pSrcBuffer, void* pmfxSurface)
{
	bool bRet = false;

	mfxFrameData* pData = &(((mfxFrameSurface1*)pmfxSurface)->Data);
	USHORT pitch = pData->Pitch;
	mfxVideoParam* pVideoParam = (mfxVideoParam*)m_pEncParams; 

	if(GRAY == m_videoMemHandle.m_imgChannelCount)
	{
		//y
		for (int i = 0; i < pVideoParam->mfx.FrameInfo.CropH; i++)
		{
			memcpy(pData->Y + i * pitch, pSrcBuffer + i * pVideoParam->mfx.FrameInfo.CropW, pVideoParam->mfx.FrameInfo.CropW);
		}
	}
	else if (COLOR == m_videoMemHandle.m_imgChannelCount)
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

//bool intelMSDKencoder::MatImageToVideoMem(cv::Mat& matImage, void* pmfxSurface)
//{
//	bool bRet = false;
//	mfxFrameInfo* pInfo = &(((mfxFrameSurface1*)pmfxSurface)->Info);
//	mfxFrameData* pData = &(((mfxFrameSurface1*)pmfxSurface)->Data);
//	USHORT pitch = pData->Pitch;
//	int iCols = matImage.cols;
//	mfxU8* ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;
//	int pixelNum = matImage.total();
//	for (int i = 0; i < matImage.rows; i++)
//	{
//		memcpy(pData->Y + i * pitch, matImage.data + i * iCols, iCols);
//	}
//
//	//for test
//	/*char	strFileName[10240] = { 0 };
//	sprintf(strFileName, "d:\\matdata\\%d.jpg", m_iTest++);
//	cv::imwrite(strFileName, matImage);*/
//
//	bRet = true;
//	return bRet;
//}

//bool intelMSDKencoder::WriteEncodedStreamToDisk(unsigned char* pDstBuffer, int iDstLength, FILE* fSink)
//{
//	bool bRet = false;
//
//	if (NULL == fSink)
//		return bRet;
//	fwrite(pDstBuffer, 1, iDstLength, fSink);
//
//	bRet = true;
//	return bRet;
//}



int intelMSDKencoder::GetMemSurfaceCount()
{
	return m_videoMemHandle.m_vecpSurfaces.size();
}

int intelMSDKencoder::GetBufferSizeInKB()
{
	return ((mfxVideoParam*)m_pEncParams)->mfx.BufferSizeInKB;
}

//std::vector<unsigned char*>& intelMSDKencoder::GetVecPtrDstBuffer()
//{
//	return m_vecPtrDstBuffer;
//}

bool intelMSDKencoder::LockVideoMem(int iPos)
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

bool intelMSDKencoder::UnlockVideoMem(int iPos)
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

int intelMSDKencoder::GetFreeSurface(std::vector<void*> vecpSurfaces)
{
	auto it = std::find_if(vecpSurfaces.begin(), vecpSurfaces.end(), [](const void* pSurface) {
		return 0 ==((mfxFrameSurface1*)pSurface)->Data.Locked;
		});

	if (it == vecpSurfaces.end())
		return MFX_ERR_NOT_FOUND;
	else return it - vecpSurfaces.begin();
}


bool EncoderParam::SetJpegParam(USHORT usImageSrcWidth, USHORT usImageSrcHeight, USHORT usQuality, int imgChannelCount)
{
	bool bRet = false;

	m_uiDstType = MFX_CODEC_JPEG;
	m_uiSrcType = MFX_FOURCC_NV12;
	m_usImageSrcWidth = usImageSrcWidth;
	m_usImageSrcHeight = usImageSrcHeight;
	m_usQuality = usQuality;
	m_imgChannelCount = imgChannelCount;

	m_uiFrameRate = 0;
	m_usKbps = 0;

	bRet = true;
	return bRet;
}

bool EncoderParam::SetH264Param(USHORT usImageSrcWidth, USHORT usImageSrcHeight,  USHORT usKbps, UINT uiFrameRate, int imgChannelCount)
{
	bool bRet = false;

	m_uiDstType = MFX_CODEC_AVC;
	m_uiSrcType = MFX_FOURCC_NV12;
	m_usImageSrcWidth = usImageSrcWidth;
	m_usImageSrcHeight = usImageSrcHeight;
	m_uiFrameRate = uiFrameRate;
	m_usKbps = usKbps;
	m_imgChannelCount = imgChannelCount;

	bRet = true;
	return bRet;
}

bool EncoderParam::Clone(const EncoderParam& encoderParam)
{
	bool bRet = false;


	m_usImageSrcWidth = encoderParam.m_usImageSrcWidth;
	m_usImageSrcHeight = encoderParam.m_usImageSrcHeight;
	m_uiSrcType = encoderParam.m_uiSrcType;
	m_uiDstType = encoderParam.m_uiDstType;
	m_usQuality = encoderParam.m_usQuality;
	m_uiFrameRate = encoderParam.m_uiFrameRate;;
	m_usKbps = encoderParam.m_usKbps;
	m_imgChannelCount = encoderParam.m_imgChannelCount;

	bRet = true;
	return bRet;
}
