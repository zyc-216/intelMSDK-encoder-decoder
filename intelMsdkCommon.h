#pragma once
#ifndef _INTELMSDKCOMMON_H
#define	_INTELMSDKCOMMON_H

#include <cstdio>
#include <vector>

typedef unsigned short USHORT;
typedef unsigned int   UINT;
typedef unsigned char  UCHAR;

#define WAIT_60000MS	60000


enum ChannelCount
{
	GRAY = 1,
	COLOR = 3,
};

struct VideoMemHandle
{
	void* m_pFrameAllocator;   // allocate and free Video Mem
	void* m_pResponse;
	std::vector<void*>		m_vecpSurfaces;
	int						m_imgChannelCount; //gray or color
};
#endif // !_INTELMSDKCOMMON_H
