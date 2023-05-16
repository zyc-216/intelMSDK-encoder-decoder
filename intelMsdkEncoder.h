/* 
encode nv12 to JPEG / h264
 hardware (using intel GPU) (d3d11)
*/
#pragma once
#ifndef _INTELMSDK_ENCODER_H_
#define	_INTELMSDK_ENCODER_H_

#include "intelMSDKCommon.h"

class CIntelMSDKEncoder
{
public:
	CIntelMSDKEncoder();
	~CIntelMSDKEncoder();
	bool Init(USHORT usImageSrcWidth, USHORT usImageSrcHeight, UINT	uiSrcType, UINT	uiDstType, USHORT usQuality=85, 
				UINT uiFrameRate=30, USHORT	usKbps=4000, int imgChannelCount=1);
	bool UnInit();
	bool EncodeBuffer(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength);
	//bool JpegsToFile(std::vector<void*>& vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName);
	//bool AviToFile(std::vector<void*> vecpBuffer, std::vector<int>& veciLength, const char* pstrFileName);

public:
	int  GetMemSurfaceCount();
	int	 GetBufferSizeInKB();
private:
	bool CreateEncoder();
	bool CloseEncoder();
	bool InitVideoMemHandle();
	bool SysMemToVideoMem(unsigned char* pSrcBuffer, void* pmfxSurface);
	bool LockVideoMem(int iPos);
	bool UnlockVideoMem(int iPos);
	int  GetFreeSurface(std::vector<void*> vecpSurfaces);
	int  GetFreeTaskIndex(void* pTaskPool, USHORT usTaskCount);
	bool EncodeBufferLoop(std::vector<void*>& vecpSrcBuffer, std::vector<void*>& vecpDstBuffer, std::vector<int>& veciDstLength);
private:
	void*							m_pSession;
	void*							m_pEncParams;
	VideoMemHandle					m_videoMemHandle;

	USHORT		m_usImageSrcWidth;
	USHORT		m_usImageSrcHeight;
	UINT		m_uiSrcType;
	UINT		m_uiDstType;
	USHORT		m_usQuality;
	UINT		m_uiFrameRate;
	USHORT		m_usKbps;
	int			m_imgChannelCount;

};

#endif // !_INTELMSDK_ENCODER_H_
