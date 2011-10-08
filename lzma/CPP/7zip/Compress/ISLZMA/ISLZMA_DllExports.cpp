// ISLZMA_DLLExports.cpp, by Jordan Russell for Inno Setup
// This file may be used under the same license terms as the LZMA SDK.
// $jrsoftware: iscompress/lzma/CPP/7zip/Compress/ISLZMA/ISLZMA_DllExports.cpp,v 1.4 2007/08/02 20:20:10 jr Exp $

#include "StdAfx.h"

// #include <initguid.h>
#define INITGUID

#include "../LZMA/LZMAEncoder.h"
#include "../../../Common/NewHandler.h"

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
	return TRUE;
}

HRESULT __stdcall LZMA_Init(void **handle)
{
	try
	{
		NCompress::NLZMA::CEncoder *encoder = new NCompress::NLZMA::CEncoder();
		encoder->SetWriteEndMarkerMode(true);
		*handle = encoder;
		return S_OK;
	}
	catch(CNewException)
	{
		return E_OUTOFMEMORY;
	}
	catch(...)
	{
		return E_FAIL;
	}
}

HRESULT __stdcall LZMA_SetProps(void *handle, unsigned algorithm, unsigned dicSize, unsigned numFastBytes,
	const LPCWSTR matchFinder, unsigned numThreads)
{
	NCompress::NLZMA::CEncoder *encoder = reinterpret_cast<NCompress::NLZMA::CEncoder *>(handle);

	PROPID propdIDs[] =
	{
		NCoderPropID::kAlgorithm,
		NCoderPropID::kDictionarySize,
		NCoderPropID::kNumFastBytes,
		NCoderPropID::kMatchFinder,
		NCoderPropID::kNumThreads
	};
	const int kNumProps = sizeof(propdIDs) / sizeof(propdIDs[0]);
	PROPVARIANT props[kNumProps];

	// NCoderPropID::kAlgorithm
	props[0].vt = VT_UI4;
	props[0].ulVal = algorithm;
	// NCoderPropID::kDictionarySize
	props[1].vt = VT_UI4;
	props[1].ulVal = dicSize;
	// NCoderPropID::kNumFastBytes
	props[2].vt = VT_UI4;
	props[2].ulVal = numFastBytes;
	// NCoderPropID::kMatchFinder
	props[3].vt = VT_BSTR;
	if (!(props[3].bstrVal = ::SysAllocString(matchFinder)))
	{
		return E_OUTOFMEMORY;
	}
	// NCoderPropID::kNumThreads
	props[4].vt = VT_UI4;
	props[4].ulVal = numThreads;

	HRESULT res = encoder->SetCoderProperties(propdIDs, props, kNumProps);
	::SysFreeString(props[3].bstrVal);  // matchFinder
	return res;
}

HRESULT __stdcall LZMA_Encode(void *handle, ISequentialInStream *in_stream, ISequentialOutStream *out_stream,
	ICompressProgressInfo *progress)
{
	try
	{
		NCompress::NLZMA::CEncoder *encoder = reinterpret_cast<NCompress::NLZMA::CEncoder *>(handle);
		HRESULT res;

		res = encoder->WriteCoderProperties(out_stream);
		if (res == S_OK)
		{
			res = encoder->Code(in_stream, out_stream, NULL, NULL, progress);
		}
		return res;
	}
	catch(...)
	{
		return E_FAIL;
	}
}

HRESULT __stdcall LZMA_End(void *handle)
{
	try
	{
		NCompress::NLZMA::CEncoder *encoder = reinterpret_cast<NCompress::NLZMA::CEncoder *>(handle);
		delete encoder;
		return S_OK;
	}
	catch(...)
	{
		return E_FAIL;
	}
}