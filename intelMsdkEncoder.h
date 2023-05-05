/* 
encode nv12 to JPEG / h264
 hardware (using intel GPU) (d3d11)
*/

#pragma once
#ifndef _ENCODER_H_
#define	_ENCODER_H_


#include <cstdio>
#include <vector>
#include "intelMsdkCommon.h"


class EncoderParam
{
public:
	EncoderParam() = default;
	bool SetJpegParam(USHORT usImageSrcWidth, USHORT usImageSrcHeight, USHORT usQuality = 75, int imgChannelCount = GRAY);
	bool SetH264Param(USHORT usImageSrcWidth, USHORT usImageSrcHeight, USHORT usKbps, UINT uiFrameRate = 30, int imgChannelCount = GRAY);
	bool Clone(const EncoderParam& encoderParam);

public:
	USHORT		m_usImageSrcWidth;
	USHORT		m_usImageSrcHeight;
	UINT		m_uiSrcType;
	UINT		m_uiDstType;
	USHORT		m_usQuality;
	UINT		m_uiFrameRate;
	USHORT		m_usKbps;
	int			m_imgChannelCount;
};

struct VideoMemHandle
{
	void*					m_pFrameAllocator;   // allocate and free Video Mem
	void*					m_pResponse;
	std::vector<void*>		m_vecpSurfaces;
	int						m_imgChannelCount; //gray or color
};

class intelMSDKencoder
{
public:
	intelMSDKencoder() = default;
	~intelMSDKencoder();
	bool Init(const EncoderParam& param);
	bool unInit();
	bool EncodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength);

	bool JpegsToFile(std::vector<void*>& vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName);
	bool AviToFile(std::vector<void*> vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName);

public:
	int  GetMemSurfaceCount();
	int	 GetBufferSizeInKB();
	//bool WriteEncodedStreamToDisk(unsigned char* pDstBuffer, int iDstLength, FILE* fSink);
	//	int GetSrcBufferSize();

private:
	bool CreateEncoder(const EncoderParam& param);
	bool CloseEncoder();
	bool InitVideoMemHandle();
	bool SysMemToVideoMem(unsigned char* pSrcBuffer, void* pmfxSurface);
	bool LockVideoMem(int iPos);
	bool UnlockVideoMem(int iPos);
	int  GetFreeSurface(std::vector<void*> vecpSurfaces);
	bool EncodeBufferLoop(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength);
	bool EncodeBufferH264(std::vector<void*>& vecpSrcBuffer, const char* pstrFileName);
	//	bool AllocSysMem();
	//	bool MatImageToVideoMem(cv::Mat& matImage, void* pmfxSurface);
	//  bool LoadRawFrameToSysMem(unsigned char* pSrcBuffer, FILE* fSource);
	//	std::vector<unsigned char*>& GetVecPtrDstBuffer();
	//	bool EncodeMatBufferArray(std::vector<cv::Mat>& vecMats, int iFirstMatIndex, std::vector<unsigned char*>& vecPtrDstBuffer, std::vector<int>& veciDstLengthArray);
	//	bool EncodeBuffer(void* pSrc, void* pDst, int& iDstLength);
	//	bool EncodeBufferFile(void* pSrc, const char* pDstFileName, int& iDstLength);
private:
	void*							m_pSession;
	void*							m_pEncParams;
	VideoMemHandle					m_videoMemHandle;
};

#endif // !_ENCODER_H_
