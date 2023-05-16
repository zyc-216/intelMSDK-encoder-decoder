#include <dxgi1_2.h>
#include "intelMSDKCommon.h"
#include <stdio.h>
#include <memory>
#include "mfxjpeg.h"
#include <d3d11.h>
#include <atlbase.h>
#include <iostream>
//CComPtr<ID3D11Device>                   g_pD3D11Device;
//CComPtr<ID3D11DeviceContext>            g_pD3D11Ctx;
//CComPtr<IDXGIFactory2>                  g_pDXGIFactory;
//IDXGIAdapter* g_pAdapter;

//std::map<mfxMemId*, mfxHDL>             allocResponses;
//std::map<mfxHDL, mfxFrameAllocResponse> allocDecodeResponses;
//std::map<mfxHDL, int>                   allocDecodeRefCount;

CDeviceManagerD3D11 D3D11Manager;

typedef struct {
    mfxMemId    memId;
    mfxMemId    memIdStage;
    mfxU16      rw;
} CustomMemId;

const struct {
    mfxIMPL impl;       // actual implementation
    mfxU32  adapterID;  // device adapter number
} implTypes[] = {
    {MFX_IMPL_HARDWARE, 0},
    {MFX_IMPL_HARDWARE2, 1},
    {MFX_IMPL_HARDWARE3, 2},
    {MFX_IMPL_HARDWARE4, 3}
};

mfxStatus Initialize(int impl, mfxVersion ver, MFXVideoSession* pSession, mfxFrameAllocator* pmfxAllocator, bool bCreateSharedHandles)
{
    mfxStatus sts = MFX_ERR_NONE;

#ifdef DX11_D3D
    impl |= MFX_IMPL_VIA_D3D11;
#endif

    // Initialize Intel Media SDK Session
    sts = pSession->Init(impl, &ver);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

#if defined(DX9_D3D) || defined(DX11_D3D)
    // If mfxFrameAllocator is provided it means we need to setup DirectX device and memory allocator
    if (pmfxAllocator) {
        // Create DirectX device context
        mfxHDL deviceHandle;
        sts = CreateHWDevice(*pSession, &deviceHandle, NULL, bCreateSharedHandles);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // Provide device manager to Media SDK
        sts = pSession->SetHandle(MFX_HANDLE_D3D11_DEVICE, deviceHandle);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        /*SimpleAlloc simpleAlloc;
        simpleAlloc.simple_alloc_four__param = simple_alloc;*/

        pmfxAllocator->pthis = *pSession; // We use Media SDK session ID as the allocation identifier
		pmfxAllocator->Alloc = simple_alloc;//(mfxStatus(void* , mfxFrameAllocRequest * ,mfxFrameAllocResponse *))simple_alloc;
        pmfxAllocator->Free = simple_free;
        pmfxAllocator->Lock = simple_lock;
        pmfxAllocator->Unlock = simple_unlock;
        pmfxAllocator->GetHDL = simple_gethdl;

        // Since we are using video memory we must provide Media SDK with an external allocator
        sts = pSession->SetFrameAllocator(pmfxAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
#else
    (void)bCreateSharedHandles;
    (void)pmfxAllocator;
#endif

    return sts;
}

void Release()
{
#if defined(DX9_D3D) || defined(DX11_D3D)
    CleanupHWDevice();
#endif
}

void* GetIntelDeviceAdapterHandle(mfxSession session)
{
	mfxU32  adapterNum = 0;
	mfxIMPL impl;

	MFXQueryIMPL(session, &impl);

	mfxIMPL baseImpl = MFX_IMPL_BASETYPE(impl); // Extract Media SDK base implementation type

	// get corresponding adapter number
	for (mfxU8 i = 0; i < sizeof(implTypes) / sizeof(implTypes[0]); i++) {
		if (implTypes[i].impl == baseImpl) {
			adapterNum = implTypes[i].adapterID;
			break;
		}
	}

	HRESULT hres = CreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)(&D3D11Manager.m_GpDXGIFactory));
	if (FAILED(hres)) return NULL;

	IDXGIAdapter* adapter;
	hres = ((IDXGIFactory2*)D3D11Manager.m_GpDXGIFactory)->EnumAdapters(adapterNum, &adapter);
	if (FAILED(hres)) return NULL;

	return adapter;
}

// Create HW device context
mfxStatus CreateHWDevice( mfxSession session, mfxHDL* deviceHandle, void* /*hWnd*/, bool /*bCreateSharedHandles*/)
{
	// Window handle not required by DX11 since we do not showcase rendering.

	HRESULT hres = S_OK;

	static D3D_FEATURE_LEVEL FeatureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};
	D3D_FEATURE_LEVEL pFeatureLevelsOut;
	D3D11Manager.m_GpAdapter = (IDXGIAdapter*)GetIntelDeviceAdapterHandle(session);
	if (NULL == D3D11Manager.m_GpAdapter)
		return MFX_ERR_DEVICE_FAILED;

	UINT dxFlags = 0;
	//UINT dxFlags = D3D11_CREATE_DEVICE_DEBUG;

	hres = D3D11CreateDevice((IDXGIAdapter*)D3D11Manager.m_GpAdapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		dxFlags,
		FeatureLevels,
		(sizeof(FeatureLevels) / sizeof(FeatureLevels[0])),
		D3D11_SDK_VERSION,
		(ID3D11Device**)&D3D11Manager.m_GpD3D11Device,
		&pFeatureLevelsOut,
		(ID3D11DeviceContext**)&D3D11Manager.m_GpD3D11Ctx);
	if (FAILED(hres))
		return MFX_ERR_DEVICE_FAILED;

	// turn on multithreading for the DX11 context
	CComQIPtr<ID3D10Multithread> p_mt((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx);
	if (p_mt)
		p_mt->SetMultithreadProtected(true);
	else
		return MFX_ERR_DEVICE_FAILED;

	*deviceHandle = (mfxHDL)D3D11Manager.m_GpD3D11Device;

	return MFX_ERR_NONE;
}




// Free HW device context
void CleanupHWDevice()
{
	if(D3D11Manager.m_GpAdapter)
	{
		((IDXGIAdapter*)D3D11Manager.m_GpAdapter)->Release();
		D3D11Manager.m_GpAdapter = nullptr;
	}
	if (D3D11Manager.m_GpD3D11Device)
	{
		((ID3D11Device*)D3D11Manager.m_GpD3D11Device)->Release();
		D3D11Manager.m_GpD3D11Device = nullptr;
	}
	if (D3D11Manager.m_GpD3D11Ctx)
	{
		((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx)->Release();
		D3D11Manager.m_GpD3D11Ctx = nullptr;
	}
	if (D3D11Manager.m_GpDXGIFactory)
	{
		((IDXGIFactory2*)D3D11Manager.m_GpDXGIFactory)->Release();
		D3D11Manager.m_GpDXGIFactory = nullptr;
	}
}



//
// Intel Media SDK memory allocator entrypoints....
//
mfxStatus _simple_alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
{
	HRESULT hRes;

	// Determine surface format
	DXGI_FORMAT format;
	if (MFX_FOURCC_NV12 == request->Info.FourCC)
		format = DXGI_FORMAT_NV12;
	else if (MFX_FOURCC_RGB4 == request->Info.FourCC)
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
	else if (MFX_FOURCC_YUY2 == request->Info.FourCC)
		format = DXGI_FORMAT_YUY2;
	else if (MFX_FOURCC_P8 == request->Info.FourCC) //|| MFX_FOURCC_P8_TEXTURE == request->Info.FourCC
		format = DXGI_FORMAT_P8;
	else
		format = DXGI_FORMAT_UNKNOWN;

	if (DXGI_FORMAT_UNKNOWN == format)
		return MFX_ERR_UNSUPPORTED;


	// Allocate custom container to keep texture and stage buffers for each surface
	// Container also stores the intended read and/or write operation.
	CustomMemId** mids = (CustomMemId**)calloc(request->NumFrameSuggested, sizeof(CustomMemId*));
	if (!mids) return MFX_ERR_MEMORY_ALLOC;

	for (int i = 0; i < request->NumFrameSuggested; i++) {
		mids[i] = (CustomMemId*)calloc(1, sizeof(CustomMemId));
		if (!mids[i]) {
			return MFX_ERR_MEMORY_ALLOC;
		}
		mids[i]->rw = request->Type & 0xF000; // Set intended read/write operation
	}

	request->Type = request->Type & 0x0FFF;

	// because P8 data (bitstream) for h264 encoder should be allocated by CreateBuffer()
	// but P8 data (MBData) for MPEG2 encoder should be allocated by CreateTexture2D()
	if (request->Info.FourCC == MFX_FOURCC_P8) {
		D3D11_BUFFER_DESC desc = { 0 };

		if (!request->NumFrameSuggested) return MFX_ERR_MEMORY_ALLOC;

		desc.ByteWidth = request->Info.Width * request->Info.Height;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		ID3D11Buffer* buffer = 0;
		hRes = ((ID3D11Device*)D3D11Manager.m_GpD3D11Device)->CreateBuffer(&desc, 0, &buffer);
		if (FAILED(hRes))
			return MFX_ERR_MEMORY_ALLOC;

		mids[0]->memId = reinterpret_cast<ID3D11Texture2D*>(buffer);
	}
	else {
		D3D11_TEXTURE2D_DESC desc = { 0 };

		desc.Width = request->Info.Width;
		desc.Height = request->Info.Height;
		desc.MipLevels = 1;
		desc.ArraySize = 1; // number of subresources is 1 in this case
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_DECODER;
		desc.MiscFlags = 0;
		//desc.MiscFlags            = D3D11_RESOURCE_MISC_SHARED;

		if ((MFX_MEMTYPE_FROM_VPPIN & request->Type) &&
			(DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format)) {
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			if (desc.ArraySize > 2)
				return MFX_ERR_MEMORY_ALLOC;
		}

		if ((MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
			(MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type)) {
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			if (desc.ArraySize > 2)
				return MFX_ERR_MEMORY_ALLOC;
		}

		if (DXGI_FORMAT_P8 == desc.Format)
			desc.BindFlags = 0;

		ID3D11Texture2D* pTexture2D;

		// Create surface textures
		for (size_t i = 0; i < request->NumFrameSuggested / desc.ArraySize; i++) {
			hRes = ((ID3D11Device*)D3D11Manager.m_GpD3D11Device)->CreateTexture2D(&desc, NULL, &pTexture2D);

			if (FAILED(hRes))
				return MFX_ERR_MEMORY_ALLOC;

			mids[i]->memId = pTexture2D;
		}

		desc.ArraySize = 1;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;// | D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		//desc.MiscFlags        = D3D11_RESOURCE_MISC_SHARED;

		// Create surface staging textures
		for (size_t i = 0; i < request->NumFrameSuggested; i++) {
			hRes = ((ID3D11Device*)D3D11Manager.m_GpD3D11Device)->CreateTexture2D(&desc, NULL, &pTexture2D);

			if (FAILED(hRes))
				return MFX_ERR_MEMORY_ALLOC;

			mids[i]->memIdStage = pTexture2D;
		}
	}


	response->mids = (mfxMemId*)mids;
	response->NumFrameActual = request->NumFrameSuggested;

	return MFX_ERR_NONE;
}

mfxStatus simple_alloc(void* pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
{
	mfxStatus sts = MFX_ERR_NONE;

	if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
		return MFX_ERR_UNSUPPORTED;
		

	if (D3D11Manager.m_allocDecodeResponses.find(pthis) != D3D11Manager.m_allocDecodeResponses.end() &&
		MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
		MFX_MEMTYPE_FROM_DECODE & request->Type) {
		// Memory for this request was already allocated during manual allocation stage. Return saved response
		//   When decode acceleration device (DXVA) is created it requires a list of d3d surfaces to be passed.
		//   Therefore Media SDK will ask for the surface info/mids again at Init() stage, thus requiring us to return the saved response
		//   (No such restriction applies to Encode or VPP)
		*response = D3D11Manager.m_allocDecodeResponses[pthis];
		D3D11Manager.m_allocDecodeRefCount[pthis]++;
	}
	else {
		sts = _simple_alloc(request, response);

		if (MFX_ERR_NONE == sts) {
			if (MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
				MFX_MEMTYPE_FROM_DECODE & request->Type) {
				// Decode alloc response handling
				D3D11Manager.m_allocDecodeResponses[pthis] = *response;
				D3D11Manager.m_allocDecodeRefCount[pthis]++;
			}
			else {
				// Encode and VPP alloc response handling
				D3D11Manager.m_allocResponses[response->mids] = pthis;
			}
		}
	}
	return sts;
}

mfxStatus simple_lock(void* pthis, mfxMemId mid, mfxFrameData* ptr)
{
	pthis; // To suppress warning for this unused parameter

	HRESULT hRes = S_OK;

	D3D11_TEXTURE2D_DESC        desc = { 0 };
	D3D11_MAPPED_SUBRESOURCE    lockedRect = { 0 };

	CustomMemId* memId = (CustomMemId*)mid;
	ID3D11Texture2D* pSurface = (ID3D11Texture2D*)memId->memId;
	ID3D11Texture2D* pStage = (ID3D11Texture2D*)memId->memIdStage;

	D3D11_MAP   mapType = D3D11_MAP_READ;
	UINT        mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;

	if (NULL == pStage) {
		hRes = ((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx)->Map(pSurface, 0, mapType, mapFlags, &lockedRect);
		desc.Format = DXGI_FORMAT_P8;
	}
	else {
		pSurface->GetDesc(&desc);

		// copy data only in case of user wants o read from stored surface
		if (memId->rw & WILL_READ)
			((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx)->CopySubresourceRegion(pStage, 0, 0, 0, 0, pSurface, 0, NULL);

		do {
			hRes = ((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx)->Map(pStage, 0, mapType, mapFlags, &lockedRect);
			if (S_OK != hRes && DXGI_ERROR_WAS_STILL_DRAWING != hRes)
				return MFX_ERR_LOCK_MEMORY;
		} while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);
	}

	if (FAILED(hRes))
		return MFX_ERR_LOCK_MEMORY;

	switch (desc.Format) {
	case DXGI_FORMAT_NV12:
		ptr->Pitch = (mfxU16)lockedRect.RowPitch;
		ptr->Y = (mfxU8*)lockedRect.pData;
		ptr->U = (mfxU8*)lockedRect.pData + desc.Height * lockedRect.RowPitch;
		ptr->V = ptr->U + 1;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		ptr->Pitch = (mfxU16)lockedRect.RowPitch;
		ptr->B = (mfxU8*)lockedRect.pData;
		ptr->G = ptr->B + 1;
		ptr->R = ptr->B + 2;
		ptr->A = ptr->B + 3;
		break;
	case DXGI_FORMAT_YUY2:
		ptr->Pitch = (mfxU16)lockedRect.RowPitch;
		ptr->Y = (mfxU8*)lockedRect.pData;
		ptr->U = ptr->Y + 1;
		ptr->V = ptr->Y + 3;
		break;
	case DXGI_FORMAT_P8:
		ptr->Pitch = (mfxU16)lockedRect.RowPitch;
		ptr->Y = (mfxU8*)lockedRect.pData;
		ptr->U = 0;
		ptr->V = 0;
		break;
	default:
		return MFX_ERR_LOCK_MEMORY;
	}

	return MFX_ERR_NONE;
}

mfxStatus simple_unlock(void* pthis, mfxMemId mid, mfxFrameData* ptr)
{
	pthis; // To suppress warning for this unused parameter

	CustomMemId* memId = (CustomMemId*)mid;
	ID3D11Texture2D* pSurface = (ID3D11Texture2D*)memId->memId;
	ID3D11Texture2D* pStage = (ID3D11Texture2D*)memId->memIdStage;

	if (NULL == pStage) {
		((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx)->Unmap(pSurface, 0);
	}
	else {
		((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx)->Unmap(pStage, 0);
		// copy data only in case of user wants to write to stored surface
		if (memId->rw & WILL_WRITE)
			((ID3D11DeviceContext*)D3D11Manager.m_GpD3D11Ctx)->CopySubresourceRegion(pSurface, 0, 0, 0, 0, pStage, 0, NULL);
	}

	if (ptr) {
		ptr->Pitch = 0;
		ptr->U = ptr->V = ptr->Y = 0;
		ptr->A = ptr->R = ptr->G = ptr->B = 0;
	}

	return MFX_ERR_NONE;
}

mfxStatus simple_gethdl(void* pthis, mfxMemId mid, mfxHDL* handle)
{
    pthis; // To suppress warning for this unused parameter

    if (NULL == handle)
        return MFX_ERR_INVALID_HANDLE;

    mfxHDLPair* pPair = (mfxHDLPair*)handle;
    CustomMemId* memId = (CustomMemId*)mid;

    pPair->first = memId->memId; // surface texture
    pPair->second = 0;

    return MFX_ERR_NONE;
}


mfxStatus _simple_free(mfxFrameAllocResponse* response)
{
	if (response->mids) {
		for (mfxU32 i = 0; i < response->NumFrameActual; i++) {
			if (response->mids[i]) {
				CustomMemId* mid = (CustomMemId*)response->mids[i];
				ID3D11Texture2D* pSurface = (ID3D11Texture2D*)mid->memId;
				ID3D11Texture2D* pStage = (ID3D11Texture2D*)mid->memIdStage;

				if (pSurface)
					pSurface->Release();
				if (pStage)
					pStage->Release();

				free(mid);
			}
		}
		free(response->mids);
		response->mids = NULL;
	}

	return MFX_ERR_NONE;
}

mfxStatus simple_free(void* pthis, mfxFrameAllocResponse* response)
{
	if (NULL == response)
		return MFX_ERR_NULL_PTR;

	if (D3D11Manager.m_allocResponses.find(response->mids) == D3D11Manager.m_allocResponses.end()) {
		// Decode free response handling
		if (--D3D11Manager.m_allocDecodeRefCount[pthis] == 0) {
			_simple_free(response);
			D3D11Manager.m_allocDecodeResponses.erase(pthis);
			D3D11Manager.m_allocDecodeRefCount.erase(pthis);
		}
	}
	else {
		// Encode and VPP free response handling
		D3D11Manager.m_allocResponses.erase(response->mids);
		_simple_free(response);
	}

	return MFX_ERR_NONE;
}



void PrintErrString(int err, const char* filestr, int line)
{
    switch (err) {
    case   0:
        printf("\n No error.\n");
        break;
    case  -1:
        printf("\n Unknown error: %s %d\n", filestr, line);
        break;
    case  -2:
        printf("\n Null pointer.  Check filename/path + permissions? %s %d\n", filestr, line);
        break;
    case  -3:
        printf("\n Unsupported feature/library load error. %s %d\n", filestr, line);
        break;
    case  -4:
        printf("\n Could not allocate memory. %s %d\n", filestr, line);
        break;
    case  -5:
        printf("\n Insufficient IO buffers. %s %d\n", filestr, line);
        break;
    case  -6:
        printf("\n Invalid handle. %s %d\n", filestr, line);
        break;
    case  -7:
        printf("\n Memory lock failure. %s %d\n", filestr, line);
        break;
    case  -8:
        printf("\n Function called before initialization. %s %d\n", filestr, line);
        break;
    case  -9:
        printf("\n Specified object not found. %s %d\n", filestr, line);
        break;
    case -10:
        printf("\n More input data expected. %s %d\n", filestr, line);
        break;
    case -11:
        printf("\n More output surfaces expected. %s %d\n", filestr, line);
        break;
    case -12:
        printf("\n Operation aborted. %s %d\n", filestr, line);
        break;
    case -13:
        printf("\n HW device lost. %s %d\n", filestr, line);
        break;
    case -14:
        printf("\n Incompatible video parameters. %s %d\n", filestr, line);
        break;
    case -15:
        printf("\n Invalid video parameters. %s %d\n", filestr, line);
        break;
    case -16:
        printf("\n Undefined behavior. %s %d\n", filestr, line);
        break;
    case -17:
        printf("\n Device operation failure. %s %d\n", filestr, line);
        break;
    case -18:
        printf("\n More bitstream data expected. %s %d\n", filestr, line);
        break;
    case -19:
        printf("\n Incompatible audio parameters. %s %d\n", filestr, line);
        break;
    case -20:
        printf("\n Invalid audio parameters. %s %d\n", filestr, line);
        break;
    default:
        printf("\nError code %d,\t%s\t%d\n\n", err, filestr, line);
    }
}

CDeviceManagerD3D11::CDeviceManagerD3D11()
{
    m_GpD3D11Device = nullptr;
    m_GpD3D11Ctx = nullptr;
    m_GpDXGIFactory = nullptr;
    m_GpAdapter = nullptr;
    //m_allocResponses = nullptr;
   // m_allocDecodeResponses = nullptr;
    //m_allocDecodeRefCount = nullptr;
    
    fnInit();
}

CDeviceManagerD3D11::~CDeviceManagerD3D11()
{
    fnUnInit();
}


bool CDeviceManagerD3D11::fnInit()
{
    bool bRet = false;
    if (
         m_GpD3D11Ctx 
        || m_GpDXGIFactory
        || m_GpD3D11Device
        )
        return bRet;

    //m_pGpD3D11Device = new CComPtr<ID3D11Device>();
    //m_GpD3D11Ctx = new CComPtr<ID3D11DeviceContext>();
    //m_GpDXGIFactory = new CComPtr<IDXGIFactory2>();

   /* m_allocResponses = new std::map<mfxMemId*, mfxHDL>();
    m_allocDecodeResponses = new std::map<mfxHDL, mfxFrameAllocResponse>();
    m_allocDecodeRefCount = new std::map<mfxHDL, int>();*/
    

    bRet = true;
    return bRet;
}

bool CDeviceManagerD3D11::fnUnInit()
{
    bool bRet = false;

    if (m_GpAdapter)
        m_GpAdapter = nullptr;
    if (m_GpD3D11Device)
    {   
        ((ID3D11Device*)m_GpD3D11Device)->Release();
        m_GpD3D11Device = nullptr;
    }
    if (m_GpD3D11Ctx)
    {
        ((ID3D11DeviceContext*)m_GpD3D11Ctx)->Release();
        m_GpD3D11Ctx = nullptr;
    }
    if (m_GpDXGIFactory)
    {
        ((IDXGIFactory2*)m_GpDXGIFactory)->Release();
        m_GpDXGIFactory = nullptr;
    }
    bRet = true;
    return bRet;
}


CAddAVIHeader::CAddAVIHeader()
{
	size_t InitBufferSize = 1024 * 1024 * 100; //100MB
	m_dstBuffer.resize(InitBufferSize);

}


CAddAVIHeader::~CAddAVIHeader()
{
	m_dstBuffer.clear();
}

void CAddAVIHeader::Initialize(unsigned width, unsigned height, unsigned bpp, const char* fourcc, unsigned fps, gwavi_audio_t* audio, long lStartIndex)
{
	memset(&avi_header, 0, sizeof(avi_header));
	memset(&stream_header_v, 0, sizeof(stream_header_v));
	memset(&stream_format_v, 0, sizeof(stream_format_v));
	memset(&stream_header_a, 0, sizeof(stream_header_a));
	memset(&stream_format_a, 0, sizeof(stream_format_a));
	m_iNowIndex = 0;
	marker = 0;
	offsets_ptr = 0;
	offsets_len = 0;
	offsets_start = 0;
	offsets = NULL;
	offset_count = 0;
	m_iNowIndex = lStartIndex;
	/* set avi header */
	avi_header.time_delay = 1000000 / fps;
	avi_header.data_rate = width * height * bpp / 8;
	//avi_header.data_rate = 2000000;
	avi_header.flags = 0x10;

	if (audio)
		avi_header.data_streams = 2;
	else
		avi_header.data_streams = 1;

	/* this field gets updated when calling gwavi_close() */
	avi_header.number_of_frames = 0;
	avi_header.width = width;
	avi_header.height = height;
	avi_header.buffer_size = (width * height * bpp / 8);

	/* set stream header */
	(void)strcpy(stream_header_v.data_type, "vids");
	(void)memcpy(stream_header_v.codec, fourcc, 4);
	stream_header_v.time_scale = 1;
	stream_header_v.data_rate = fps;
	stream_header_v.buffer_size = (width * height * bpp / 8);
	stream_header_v.data_length = 0;

	/* set stream format */
	stream_format_v.header_size = 40;
	stream_format_v.width = width;
	stream_format_v.height = height;
	stream_format_v.num_planes = 1;
	stream_format_v.bits_per_pixel = bpp;
	stream_format_v.compression_type = ((unsigned int)fourcc[3] << 24) + ((unsigned int)fourcc[2] << 16)
		+ ((unsigned int)fourcc[1] << 8) + ((unsigned int)fourcc[0]);
	stream_format_v.image_size = width * height * 3;
	stream_format_v.colors_used = 0;
	stream_format_v.colors_important = 0;

	stream_format_v.palette = NULL;
	stream_format_v.palette_count = 0;

	if (audio) {
		/* set stream header */
		memcpy(stream_header_a.data_type, "auds", 4);
		stream_header_a.codec[0] = 1;
		stream_header_a.codec[1] = 0;
		stream_header_a.codec[2] = 0;
		stream_header_a.codec[3] = 0;
		stream_header_a.time_scale = 1;
		stream_header_a.data_rate = audio->samples_per_second;
		stream_header_a.buffer_size = audio->channels * (audio->bits / 8) * audio->samples_per_second;
		/* when set to -1, drivers use default quality value */
		stream_header_a.audio_quality = -1;
		stream_header_a.sample_size = (audio->bits / 8) * audio->channels;

		/* set stream format */
		stream_format_a.format_type = 1;
		stream_format_a.channels = audio->channels;
		stream_format_a.sample_rate = audio->samples_per_second;
		stream_format_a.bytes_per_second = audio->channels * (audio->bits / 8) * audio->samples_per_second;
		stream_format_a.block_align = audio->channels * (audio->bits / 8);
		stream_format_a.bits_per_sample = audio->bits;
		stream_format_a.size = 0;
	}

	write_chars_bin("RIFF", 4, m_iNowIndex);
	write_int(0, m_iNowIndex);
	write_chars_bin("AVI ", 4, m_iNowIndex);

	write_avi_header_chunk(m_iNowIndex);

	write_chars_bin("LIST", 4, m_iNowIndex);

	//markerIndex = m_dstBuffer.size();
	marker = m_iNowIndex;
	//marker = outFile.tellp();


	write_int(0, m_iNowIndex);
	write_chars_bin("movi", 4, m_iNowIndex);

	offsets_len = 102400;
	offsets = new unsigned int[offsets_len];
	offsets_ptr = 0;
}

int CAddAVIHeader::AddVideoFrameHeader(std::vector<void*>& vecpSrcBuffer, std::vector<int>& veciSrcLength)
{
	int iRet = 0;
	for (int i = 0; i < vecpSrcBuffer.size(); ++i) {
		AddSingleVideoFrameHeader(vecpSrcBuffer[i], veciSrcLength[i], m_iNowIndex);
	}

	Finalize(m_iNowIndex);

	return iRet;
}

int CAddAVIHeader::Finalize(long lStartIndex)
{
	int ret = 0;
	long t;

	try {
		m_iNowIndex = lStartIndex;
		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = marker;
		//outFile.seekp(marker, ios_base::beg);
		write_int((unsigned int)(t - marker - 4), m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);

		write_index(offset_count, offsets, m_iNowIndex);

		delete[] offsets;
		offsets = NULL;

		/* reset some avi header fields */
		avi_header.number_of_frames = stream_header_v.data_length;

		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = 12;
		//outFile.seekp(12, ios_base::beg);
		write_avi_header_chunk(m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);

		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = 4;
		//outFile.seekp(4, ios_base::beg);
		write_int((unsigned int)(t - 8), m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);

		if (stream_format_v.palette) // TODO check
			delete[] stream_format_v.palette;

		//outFile.close();
	}
	catch (std::system_error& e) {
		std::cerr << e.code().message() << "\n";
		ret = -1;
	}

	delete[] offsets;
	return ret;
}

std::vector<unsigned char>& CAddAVIHeader::GetDstBuffer()
{
	return m_dstBuffer;
}

long CAddAVIHeader::GetNowIndex()
{
	return m_iNowIndex;
}


void CAddAVIHeader::write_int(unsigned int n, long lStartIndex)
{
	unsigned char buffer[4];

	buffer[0] = n;
	buffer[1] = n >> 8;
	buffer[2] = n >> 16;
	buffer[3] = n >> 24;

	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, &buffer[0], &buffer[3] + 1);
	}
	else {
		for (int i = 0; i < 4; ++i) {
			m_dstBuffer.at(lStartIndex + i) = buffer[i];
		}
	}
	m_iNowIndex += 4;
}


void CAddAVIHeader::write_short(unsigned int n, long lStartIndex)
{
	unsigned char buffer[2];

	buffer[0] = n;
	buffer[1] = n >> 8;

	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, &buffer[0], &buffer[1] + 1);
	}
	else {
		for (int i = 0; i < 2; ++i) {
			m_dstBuffer.at(lStartIndex + i) = buffer[i];
		}
	}
	m_iNowIndex += 2;

}



void CAddAVIHeader::write_chars(const char* s, long lStartIndex)
{
	int count = strlen(s);
	if (count > 255)
		count = 255;
	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, (unsigned char*)&s[0], (unsigned char*)&s[count - 1] + 1);
	}
	else
	{
		for (int i = 0; i < count; ++i) {
			m_dstBuffer.at(lStartIndex + i) = s[i];
		}
	}
	m_iNowIndex += count;
}


void CAddAVIHeader::write_chars_bin(const char* s, size_t len, long lStartIndex)
{
	//unsigned char ºÍ char?
	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, (unsigned char*)&s[0], (unsigned char*)&s[len - 1] + 1);
	}
	else
	{
		for (int i = 0; i < len; ++i) {
			m_dstBuffer.at(lStartIndex + i) = s[i];
		}
	}
	m_iNowIndex += len;
}


int CAddAVIHeader::AddSingleVideoFrameHeader(void* pSrcBuffer, int iSrcLength, long lStartIndex)
{
	int ret = 0;
	size_t maxi_pad; /* if your frame is raggin, give it some paddin' */
	size_t t;

	if (!pSrcBuffer)
	{
		fputs("gwavi and/or buffer argument cannot be NULL", stderr);
		return -1;
	}
	if (iSrcLength < 256){}
		//fprintf(stderr, "WARNING: specified buffer len seems rather small: %d. Are you sure about this??????\n", (int)iSrcLength);

	m_iNowIndex = lStartIndex;
	try
	{
		offset_count++;
		stream_header_v.data_length++;

		maxi_pad = iSrcLength % 4;
		if (maxi_pad > 0)
			maxi_pad = 4 - maxi_pad;

		if (offset_count >= offsets_len)
		{
			/*offsets_len += 1024;
			delete[] offsets;
			offsets = new unsigned int[offsets_len];*/
			int tmp_offsets_len = offsets_len;
			offsets_len += 1024;
			unsigned int* tmp_offsets = new unsigned int[offsets_len];
			memcpy(tmp_offsets, offsets, tmp_offsets_len * sizeof(unsigned int));
			delete offsets;
			offsets = tmp_offsets;
		}

		offsets[offsets_ptr++] = (unsigned int)(iSrcLength + maxi_pad);

		write_chars_bin("00dc", 4, m_iNowIndex);

		write_int((unsigned int)(iSrcLength + maxi_pad), m_iNowIndex);

		m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)pSrcBuffer, (char*)pSrcBuffer + iSrcLength);
		m_iNowIndex += iSrcLength;
		//outFile.write((char*)buffer, len);

		const char* s = "\0";
		for (t = 0; t < maxi_pad; t++)
		{
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)s, (char*)s + 1);
			m_iNowIndex += 1;
		}
	}
	catch (std::system_error& e)
	{
		std::cerr << e.code().message() << "\n";
		ret = -1;
	}

	return ret;
}

void CAddAVIHeader::write_avi_header(gwavi_header_t* avi_header, long lStartIndex)
{
	long marker, t;
	m_iNowIndex = lStartIndex;
	write_chars_bin("avih", 4, m_iNowIndex);
	//marker = outFile.tellp();
	marker = m_iNowIndex;
	write_int(0, m_iNowIndex);

	write_int(avi_header->time_delay, m_iNowIndex);
	write_int(avi_header->data_rate, m_iNowIndex);
	write_int(avi_header->reserved, m_iNowIndex);
	/* dwFlags */
	write_int(avi_header->flags, m_iNowIndex);
	/* dwTotalFrames */
	write_int(avi_header->number_of_frames, m_iNowIndex);
	write_int(avi_header->initial_frames, m_iNowIndex);
	write_int(avi_header->data_streams, m_iNowIndex);
	write_int(avi_header->buffer_size, m_iNowIndex);
	write_int(avi_header->width, m_iNowIndex);
	write_int(avi_header->height, m_iNowIndex);
	write_int(avi_header->time_scale, m_iNowIndex);
	write_int(avi_header->playback_data_rate, m_iNowIndex);
	write_int(avi_header->starting_time, m_iNowIndex);
	write_int(avi_header->data_length, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();
	//outFile.seekp(marker, ios_base::beg);
	m_iNowIndex = marker;
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	//outFile.seekp(t, ios_base::beg);
	m_iNowIndex = t;
}

void CAddAVIHeader::write_stream_header(gwavi_stream_header_t* stream_header, long lStartIndex)
{
	long marker, t;
	m_iNowIndex = lStartIndex;
	write_chars_bin("strh", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);

	write_chars_bin(stream_header->data_type, 4, m_iNowIndex);
	write_chars_bin(stream_header->codec, 4, m_iNowIndex);
	write_int(stream_header->flags, m_iNowIndex);
	write_int(stream_header->priority, m_iNowIndex);
	write_int(stream_header->initial_frames, m_iNowIndex);
	write_int(stream_header->time_scale, m_iNowIndex);
	write_int(stream_header->data_rate, m_iNowIndex);
	write_int(stream_header->start_time, m_iNowIndex);
	write_int(stream_header->data_length, m_iNowIndex);
	write_int(stream_header->buffer_size, m_iNowIndex);
	write_int(stream_header->video_quality, m_iNowIndex);
	write_int(stream_header->sample_size, m_iNowIndex);
	write_int(0, m_iNowIndex);
	write_int(0, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();
	//outFile.seekp(marker, ios_base::beg);
	m_iNowIndex = marker;
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	//outFile.seekp(t, ios_base::beg);
	m_iNowIndex = t;
}

void CAddAVIHeader::write_stream_format_v(gwavi_stream_format_v_t* stream_format_v, long lStartIndex)
{
	long marker, t;
	unsigned int i;
	m_iNowIndex = lStartIndex;
	write_chars_bin("strf", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_int(stream_format_v->header_size, m_iNowIndex);
	write_int(stream_format_v->width, m_iNowIndex);
	write_int(stream_format_v->height, m_iNowIndex);
	write_short(stream_format_v->num_planes, m_iNowIndex);
	write_short(stream_format_v->bits_per_pixel, m_iNowIndex);
	write_int(stream_format_v->compression_type, m_iNowIndex);
	write_int(stream_format_v->image_size, m_iNowIndex);
	write_int(stream_format_v->x_pels_per_meter, m_iNowIndex);
	write_int(stream_format_v->y_pels_per_meter, m_iNowIndex);
	write_int(stream_format_v->colors_used, m_iNowIndex);
	write_int(stream_format_v->colors_important, m_iNowIndex);

	if (stream_format_v->colors_used != 0) {
		for (i = 0; i < stream_format_v->colors_used; i++) {
			unsigned char c = stream_format_v->palette[i] & 255;
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&c, (char*)&c + 1);
			m_iNowIndex += 1;
			//outFile.write((char*)&c, 1);
			c = (stream_format_v->palette[i] >> 8) & 255;
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&c, (char*)&c + 1);
			m_iNowIndex += 1;
			//outFile.write((char*)&c, 1);
			c = (stream_format_v->palette[i] >> 16) & 255;
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&c, (char*)&c + 1);
			m_iNowIndex += 1;
			//outFile.write((char*)&c, 1);
			const char* s = "\0";
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&s, (char*)&s + 1);
			m_iNowIndex += 1;
			//outFile.write("\0", 1);
		}
	}

	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}

void CAddAVIHeader::write_stream_format_a(gwavi_stream_format_a_t* stream_format_a, long lStartIndex)
{
	long marker, t;
	m_iNowIndex = lStartIndex;
	write_chars_bin("strf", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_short(stream_format_a->format_type, m_iNowIndex);
	write_short(stream_format_a->channels, m_iNowIndex);
	write_int(stream_format_a->sample_rate, m_iNowIndex);
	write_int(stream_format_a->bytes_per_second, m_iNowIndex);
	write_short(stream_format_a->block_align, m_iNowIndex);
	write_short(stream_format_a->bits_per_sample, m_iNowIndex);
	write_short(stream_format_a->size, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}

void CAddAVIHeader::write_avi_header_chunk(long lStartIndex)
{
	long marker, t;
	long sub_marker;
	m_iNowIndex = lStartIndex;

	write_chars_bin("LIST", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_chars_bin("hdrl", 4, m_iNowIndex);
	write_avi_header(&avi_header, m_iNowIndex);

	write_chars_bin("LIST", 4, m_iNowIndex);
	sub_marker = m_iNowIndex;
	//sub_marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_chars_bin("strl", 4, m_iNowIndex);
	write_stream_header(&stream_header_v, m_iNowIndex);
	write_stream_format_v(&stream_format_v, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();

	m_iNowIndex = sub_marker;
	//outFile.seekp(sub_marker, ios_base::beg);
	write_int((unsigned int)(t - sub_marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);

	if (avi_header.data_streams == 2) {
		write_chars_bin("LIST", 4, m_iNowIndex);
		sub_marker = m_iNowIndex;
		//sub_marker = outFile.tellp();
		write_int(0, m_iNowIndex);
		write_chars_bin("strl", 4, m_iNowIndex);
		write_stream_header(&stream_header_a, m_iNowIndex);
		write_stream_format_a(&stream_format_a, m_iNowIndex);

		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = sub_marker;
		//outFile.seekp(sub_marker, ios_base::beg);
		write_int((unsigned int)(t - sub_marker - 4), m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);
	}
	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}



void CAddAVIHeader::write_index(int count, unsigned int* offsets, long lStartIndex)
{
	long marker, t;
	unsigned int offset = 4;

	if (!offsets)
		throw 1;
	m_iNowIndex = lStartIndex;
	write_chars_bin("idx1", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);

	for (t = 0; t < count; t++) {
		if ((offsets[t] & 0x80000000) == 0)
			write_chars("00dc", m_iNowIndex);
		else {
			write_chars("01wb", m_iNowIndex);
			offsets[t] &= 0x7fffffff;
		}
		write_int(0x10, m_iNowIndex);
		write_int(offset, m_iNowIndex);
		write_int(offsets[t], m_iNowIndex);

		offset = offset + offsets[t] + 8;
	}

	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}

int CAddAVIHeader::check_fourcc(const char* fourcc)
{
	int ret = 0;
	/* list of fourccs from http://fourcc.org/codecs.php */
	const char valid_fourcc[] = "3IV1 3IV2 8BPS"
		"AASC ABYR ADV1 ADVJ AEMI AFLC AFLI AJPG AMPG ANIM AP41 ASLC"
		"ASV1 ASV2 ASVX AUR2 AURA AVC1 AVRN"
		"BA81 BINK BLZ0 BT20 BTCV BW10 BYR1 BYR2"
		"CC12 CDVC CFCC CGDI CHAM CJPG CMYK CPLA CRAM CSCD CTRX CVID"
		"CWLT CXY1 CXY2 CYUV CYUY"
		"D261 D263 DAVC DCL1 DCL2 DCL3 DCL4 DCL5 DIV3 DIV4 DIV5 DIVX"
		"DM4V DMB1 DMB2 DMK2 DSVD DUCK DV25 DV50 DVAN DVCS DVE2 DVH1"
		"DVHD DVSD DVSL DVX1 DVX2 DVX3 DX50 DXGM DXTC DXTN"
		"EKQ0 ELK0 EM2V ES07 ESCP ETV1 ETV2 ETVC"
		"FFV1 FLJP FMP4 FMVC FPS1 FRWA FRWD FVF1"
		"GEOX GJPG GLZW GPEG GWLT"
		"H260 H261 H262 H263 H264 H265 H266 H267 H268 H269"
		"HDYC HFYU HMCR HMRR"
		"I263 ICLB IGOR IJPG ILVC ILVR IPDV IR21 IRAW ISME"
		"IV30 IV31 IV32 IV33 IV34 IV35 IV36 IV37 IV38 IV39 IV40 IV41"
		"IV41 IV43 IV44 IV45 IV46 IV47 IV48 IV49 IV50"
		"JBYR JPEG JPGL"
		"KMVC"
		"L261 L263 LBYR LCMW LCW2 LEAD LGRY LJ11 LJ22 LJ2K LJ44 LJPG"
		"LMP2 LMP4 LSVC LSVM LSVX LZO1"
		"M261 M263 M4CC M4S2 MC12 MCAM MJ2C MJPG MMES MP2A MP2T MP2V"
		"MP42 MP43 MP4A MP4S MP4T MP4V MPEG MPNG MPG4 MPGI MR16 MRCA MRLE"
		"MSVC MSZH"
		"MTX1 MTX2 MTX3 MTX4 MTX5 MTX6 MTX7 MTX8 MTX9"
		"MVI1 MVI2 MWV1"
		"NAVI NDSC NDSM NDSP NDSS NDXC NDXH NDXP NDXS NHVU NTN1 NTN2"
		"NVDS NVHS"
		"NVS0 NVS1 NVS2 NVS3 NVS4 NVS5"
		"NVT0 NVT1 NVT2 NVT3 NVT4 NVT5"
		"PDVC PGVV PHMO PIM1 PIM2 PIMJ PIXL PJPG PVEZ PVMM PVW2"
		"QPEG QPEQ"
		"RGBT RLE RLE4 RLE8 RMP4 RPZA RT21 RV20 RV30 RV40 S422 SAN3"
		"SDCC SEDG SFMC SMP4 SMSC SMSD SMSV SP40 SP44 SP54 SPIG SQZ2"
		"STVA STVB STVC STVX STVY SV10 SVQ1 SVQ3"
		"TLMS TLST TM20 TM2X TMIC TMOT TR20 TSCC TV10 TVJP TVMJ TY0N"
		"TY2C TY2N"
		"UCOD ULTI"
		"V210 V261 V655 VCR1 VCR2 VCR3 VCR4 VCR5 VCR6 VCR7 VCR8 VCR9"
		"VDCT VDOM VDTZ VGPX VIDS VIFP VIVO VIXL VLV1 VP30 VP31 VP40"
		"VP50 VP60 VP61 VP62 VP70 VP80 VQC1 VQC2 VQJC VSSV VUUU VX1K"
		"VX2K VXSP VYU9 VYUY"
		"WBVC WHAM WINX WJPG WMV1 WMV2 WMV3 WMVA WNV1 WVC1"
		"X263 X264 XLV0 XMPG XVID"
		"XWV0 XWV1 XWV2 XWV3 XWV4 XWV5 XWV6 XWV7 XWV8 XWV9"
		"XXAN"
		"Y16 Y411 Y41P Y444 Y8 YC12 YUV8 YUV9 YUVP YUY2 YUYV YV12 YV16"
		"YV92"
		"ZLIB ZMBV ZPEG ZYGO ZYYY";

	if (!fourcc) {
		(void)fputs("fourcc cannot be NULL", stderr);
		return -1;
	}
	if (strchr(fourcc, ' ') || !strstr(valid_fourcc, fourcc))
		ret = 1;

	return ret;
}
