#pragma once
#ifndef _INTELMSDK_DECODER_H_
#define _INTELMSDK_DECODER_H_

#include "intelMSDKCommon.h"

class CIntelMSDKDecoder
{
public:
	CIntelMSDKDecoder();
	~CIntelMSDKDecoder();

	bool Init(USHORT iDstWidth,USHORT iDstHeight,UINT uiSrcType,UINT uiDstType,UINT uiFrameRate=25,int iImageChannelCount=1);
	bool UnInit();
	bool DecodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);

private:
	bool DecodeBufferJpeg(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	bool DecodeBufferJpeg2(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	bool DecodeBufferH264(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciSrcLength);
	bool CreateDecoder();
	bool CloseDecoder();
	bool InitVideoMemHandle();
	bool VideoMemToSysMem(void* pmfxSurface, void* pDstBuffer);
	bool LockVideoMem(int iPos);
	bool UnlockVideoMem(int iPos);
	int  GetFreeSurface(std::vector<void*> vecpSurfaces);
public:
	void*							m_pSession;
	void*							m_pDecParams;
	void*							m_pAddAVIHeader;
	VideoMemHandle					m_videoMemHandle;

	USHORT							m_usImageDstWidth;
	USHORT							m_usImageDstHeight;
	UINT							m_uiSrcType;
	UINT							m_uiDstType;
	UINT							m_uiFrameRate;
	int								m_imgChannelCount;

};

#endif // !_INTELMSDK_DECODER_H_
