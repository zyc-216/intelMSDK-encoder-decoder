#pragma once
#ifndef _DECODER_H_
#define _DECODER_H_


#include "intelMsdkEncoder.h"

class DecoderParam 
{
public:
	DecoderParam() = default;

	bool SetJpegParam(USHORT imageDstWidth, USHORT imageDstHeight, int imgChannelCount = GRAY);
	bool SetH264Param(USHORT imageDstWidth, USHORT imageDstHeight, int imgChannelCount = GRAY);
	bool Clone(const DecoderParam& decoderParam);

public:
	USHORT		m_usImageDstWidth;
	USHORT		m_usImageDstHeight;
	UINT		m_uiSrcType;
	UINT		m_uiDstType;
	int			m_imgChannelCount;
};



class intelMSDKdecoder
{
public:
	intelMSDKdecoder() = default;
	~intelMSDKdecoder();

	bool Init(const DecoderParam& param);
	bool unInit();
	bool DecodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);

private:
	bool DecodeBufferJpeg(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	bool DecodeBufferH264(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	bool CreateDecoder(const DecoderParam& param);
	bool CloseDecoder();
	bool InitVideoMemHandle();
	bool VideoMemToSysMem(void* pmfxSurface, void* pDstBuffer);
	bool LockVideoMem(int iPos);
	bool UnlockVideoMem(int iPos);
	int  GetFreeSurface(std::vector<void*> vecpSurfaces);
	//int  GetFreeWorkingSurfaceIndex(const std::vector<mfxFrameSurface1*>& pSurfacesPool);
	//int  GetFreeSyncPoint(const std::vector<mfxSyncPoint>& vecSyncPoints);
	//bool AllocSysMem();
	//bool ReadBitStreamData(void* pBitStream, std::vector<void*>& vecpSrcBuffer, void* pBufferPos);
	//bool DecodeBufferSingleArray(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer);
	//bool SysMemToVideoMem(void* pSrcBuffer, void* pmfxSurface);
	//mfxStatus ReadBitStream(void* pBitStream, void* pSrcbuffer, int& iReadBytesCount, int iTotalBytes);
	//bool DecodeBufferLoop2(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	//bool DecodeBuffer2(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	//bool DecodeBufferJpeg(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	//bool DecodeSingleArray(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength,
		//int iStartIndex, int iEndIndex, int iIndexSurface);
	//bool DecodeBuffer(void* pSrc, void* pDst);
	//bool DecodeFile(void* pSrc, const char* pDstFileName);
	//bool DecodeBufferArray(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer);
	//bool DecodeBufferLoop(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	//bool DecodeBufferLoopH264(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
public:
	void*							m_pSession;
	void*							m_pDecParams;
	VideoMemHandle					m_videoMemHandle;
};

#endif // !_DECODER_H_
