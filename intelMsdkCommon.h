#pragma once
#ifndef _INTEL_MSDK_COMMON_H_
#define _INTEL_MSDK_COMMON_H_

#include <vector>
#include <map>
#include "mfxvideo++.h"

#define MSDK_PRINT_RET_MSG(ERR)         {PrintErrString(ERR, __FILE__, __LINE__);}
#define MSDK_CHECK_RESULT(P, X, ERR)    {if ((X) > (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_ERROR(P, X, ERR)     {if ((X) == (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_IGNORE_MFX_STS(P, X)       {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define MSDK_BREAK_ON_ERROR(P)          {if (MFX_ERR_NONE != (P)) break;}
#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define MSDK_ALIGN16(value)             (((value + 15) >> 4) << 4)
//#define MSDK_SAFE_RELEASE(X)            {if (X) { X->Release(); X = NULL; }}
//#define MSDK_SAFE_DELETE_ARRAY(P)       {if (P) {delete[] P; P = NULL;}}
//#define MSDK_CHECK_POINTER(P, ERR)      {if (!(P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define WAIT_60000MS	60000
#define WILL_READ		0x1000
#define WILL_WRITE		0x2000

typedef unsigned short USHORT;
typedef unsigned int   UINT;
typedef unsigned char  UCHAR;

typedef struct
{
	void* m_pFrameAllocator;   // allocate and free Video Mem
	void* m_pResponse;
	std::vector<void*>		m_vecpSurfaces;
}VideoMemHandle;

typedef struct {
    mfxBitstream mfxBS;
    mfxSyncPoint syncp;
} Task;


/*d3d11 manager*/
class CDeviceManagerD3D11
{
public:
	CDeviceManagerD3D11();
	~CDeviceManagerD3D11();
	friend mfxStatus simple_alloc(void* pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);
	friend mfxStatus simple_lock(void* pthis, mfxMemId mid, mfxFrameData* ptr);
	friend mfxStatus simple_unlock(void* pthis, mfxMemId mid, mfxFrameData* ptr);
	friend mfxStatus simple_free(void* pthis, mfxFrameAllocResponse* response);
	friend mfxStatus _simple_alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);
	friend mfxStatus Initialize(int impl, mfxVersion ver, MFXVideoSession* pSession, mfxFrameAllocator* pmfxAllocator, bool bCreateSharedHandles);
	friend void Release();
	friend mfxStatus CreateHWDevice(mfxSession session, mfxHDL* deviceHandle, void* hWnd, bool bCreateSharedHandles);
	friend void CleanupHWDevice();
	friend void* GetIntelDeviceAdapterHandle(mfxSession session); 
private:
	bool fnInit();
	bool fnUnInit();
private:
	void*									m_GpAdapter;
	void*									m_GpD3D11Device;
	void*									m_GpD3D11Ctx;
	void*									m_GpDXGIFactory;
	std::map<mfxMemId*, void*>				m_allocResponses;
	std::map<void*, mfxFrameAllocResponse>	m_allocDecodeResponses;
	std::map<void*, int>					m_allocDecodeRefCount;
};

extern mfxStatus simple_alloc(void* pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);
extern mfxStatus simple_lock(void* pthis, mfxMemId mid, mfxFrameData* ptr);
extern mfxStatus simple_unlock(void* pthis, mfxMemId mid, mfxFrameData* ptr);
extern mfxStatus simple_free(void* pthis, mfxFrameAllocResponse* response);
extern mfxStatus simple_gethdl(void* pthis, mfxMemId mid, mfxHDL* handle);
extern mfxStatus Initialize(int impl, mfxVersion ver, MFXVideoSession* pSession, mfxFrameAllocator* pmfxAllocator, bool bCreateSharedHandles = false);
extern void Release();
extern mfxStatus CreateHWDevice(mfxSession session, mfxHDL* deviceHandle, void* hWnd, bool bCreateSharedHandles );
extern void CleanupHWDevice();
extern void PrintErrString(int err, const char* filestr, int line);


/* write avi to system memory, only need when decode h264*/
class CAddAVIHeader
{
	struct gwavi_header_t
	{
		unsigned int time_delay; /* dwMicroSecPerFrame */
		unsigned int data_rate; /* dwMaxBytesPerSec */
		unsigned int reserved;
		unsigned int flags; /* dwFlags */
		unsigned int number_of_frames; /* dwTotalFrames */
		unsigned int initial_frames; /* dwInitialFrames */
		unsigned int data_streams; /* dwStreams */
		unsigned int buffer_size; /* dwSuggestedBufferSize */
		unsigned int width; /* dwWidth */
		unsigned int height; /* dwHeight */
		unsigned int time_scale;
		unsigned int playback_data_rate;
		unsigned int starting_time;
		unsigned int data_length;
	};
	struct gwavi_stream_header_t
	{
		char data_type[5]; /* fccType */
		char codec[5]; /* fccHandler */
		unsigned int flags; /* dwFlags */
		unsigned int priority;
		unsigned int initial_frames;/* dwInitialFrames */
		unsigned int time_scale; /* dwScale */
		unsigned int data_rate; /* dwRate */
		unsigned int start_time; /* dwStart */
		unsigned int data_length; /* dwLength */
		unsigned int buffer_size; /* dwSuggestedBufferSize */
		unsigned int video_quality; /* dwQuality */
		/**
		 * Value between 0-10000. If set to -1, drivers use default quality
		 * value.
		 */
		int audio_quality;
		unsigned int sample_size; /* dwSampleSize */
	};
	struct gwavi_stream_format_v_t
	{
		unsigned int header_size;
		unsigned int width;
		unsigned int height;
		unsigned short int num_planes;
		unsigned short int bits_per_pixel;
		unsigned int compression_type;
		unsigned int image_size;
		unsigned int x_pels_per_meter;
		unsigned int y_pels_per_meter;
		unsigned int colors_used;
		unsigned int colors_important;
		unsigned int* palette;
		unsigned int palette_count;
	};
	struct gwavi_stream_format_a_t
	{
		unsigned short format_type;
		unsigned int channels;
		unsigned int sample_rate;
		unsigned int bytes_per_second;
		unsigned int block_align;
		unsigned int bits_per_sample;
		unsigned short size;
	};
	typedef struct
	{
		unsigned int channels;
		unsigned int bits;
		unsigned int samples_per_second;
	} gwavi_audio_t;
public:
	CAddAVIHeader();
	~CAddAVIHeader();
	void Initialize(unsigned width, unsigned height, unsigned bpp, const char* fourcc, unsigned fps,
		gwavi_audio_t* audio, long lStartIndex = 0);
	int AddVideoFrameHeader(std::vector<void*>& vecpSrcBuffer, std::vector<int>& veciSrcLength);
	int Finalize(long lStartIndex);

	std::vector<unsigned char>& GetDstBuffer();
	long GetNowIndex();

private:
	struct gwavi_header_t avi_header;
	struct gwavi_stream_header_t stream_header_v;
	struct gwavi_stream_format_v_t stream_format_v;
	struct gwavi_stream_header_t stream_header_a;
	struct gwavi_stream_format_a_t stream_format_a;
	long marker;
	int offsets_ptr;
	int offsets_len;
	long offsets_start;
	unsigned int* offsets;
	int offset_count;
	long m_iNowIndex;
	std::vector<unsigned char> m_dstBuffer;

private:
	int  AddSingleVideoFrameHeader(void* pSrcBuffer, int iSrcLength, long lStartIndex);
	void write_avi_header(struct gwavi_header_t* avi_header, long lStartIndex);
	void write_stream_header(struct gwavi_stream_header_t* stream_header, long lStartIndex);
	void write_stream_format_v(struct gwavi_stream_format_v_t* stream_format_v, long lStartIndex);
	void write_stream_format_a(struct gwavi_stream_format_a_t* stream_format_a, long lStartIndex);
	void write_avi_header_chunk(long lStartIndex);
	void write_index(int count, unsigned int* offsets, long lStartIndex);
	int check_fourcc(const char* fourcc);

	void write_int(unsigned int n, long lStartIndex);
	void write_short(unsigned int n, long lStartIndex);
	void write_chars(const char* s, long lStartIndex);
	void write_chars_bin(const char* s, size_t len, long lStartIndex);
};
#endif // !_INTEL_MSDK_COMMON_H_

