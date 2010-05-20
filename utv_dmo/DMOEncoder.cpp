/* �����R�[�h�͂r�i�h�r ���s�R�[�h�͂b�q�k�e */
/* $Id$ */

// DMOEncoder.cpp : CDMOEncoder �̎���

#include "stdafx.h"
#include "DMOEncoder.h"

#define FCC4PRINTF(fcc) \
	(BYTE)(fcc), \
	(BYTE)(fcc >> 8), \
	(BYTE)(fcc >> 16), \
	(BYTE)(fcc >> 24)

// CDMOEncoder

CDMOEncoder::CDMOEncoder(DWORD fcc)
{
	m_pCodec = CCodec::CreateInstance(fcc, "DMO");
}

CDMOEncoder::~CDMOEncoder()
{
	delete m_pCodec;
}

static void FormatInfoToPartialMediaType(const FORMATINFO *pfi, DWORD *pcTypes, DMO_PARTIAL_MEDIATYPE **ppTypes)
{
	const FORMATINFO *p;
	DWORD n;
	DWORD i;

	p = pfi;
	n = 0;
	while (!IS_FORMATINFO_END(p++))
		n++;

	*pcTypes = n;
	*ppTypes = new DMO_PARTIAL_MEDIATYPE[n];

	for (i = 0; i < n; i++)
	{
		(*ppTypes)[i].type = MEDIATYPE_Video;
		(*ppTypes)[i].subtype = pfi[i].guidMediaSubType;
	}
}

HRESULT WINAPI CDMOEncoder::UpdateRegistry(DWORD fcc, REFCLSID clsid, BOOL bRegister)
{
	HRESULT hr;
	OLECHAR szFcc[5];
	OLECHAR szClsID[64];
	_ATL_REGMAP_ENTRY regmap[3] = {
		{ L"FCC",   szFcc   },
		{ L"CLSID", szClsID },
		{ NULL,     NULL    }
	};
	WCHAR szCodecName[128];

	wsprintfW(szFcc, L"%C%C%C%C", FCC4PRINTF(fcc));
	StringFromGUID2(clsid, szClsID, _countof(szClsID));
	hr = ATL::_pAtlModule->UpdateRegistryFromResource(IDR_DMOENCODER, bRegister, regmap);
	if (FAILED(hr))
		return hr;

	if (bRegister)
	{
		CCodec *pCodec = CCodec::CreateInstance(fcc, "DMO");
		DMO_PARTIAL_MEDIATYPE *pInTypes;
		DMO_PARTIAL_MEDIATYPE *pOutTypes;
		DWORD cInTypes, cOutTypes;

		FormatInfoToPartialMediaType(pCodec->GetEncoderInputFormat(), &cInTypes, &pInTypes);
		FormatInfoToPartialMediaType(pCodec->GetCompressedFormat(), &cOutTypes, &pOutTypes);
		pCodec->GetLongFriendlyName(szCodecName, _countof(szCodecName));
		hr = DMORegister(szCodecName, clsid, DMOCATEGORY_VIDEO_ENCODER, 0, cInTypes, pInTypes, cOutTypes, pOutTypes);
		delete pInTypes;
		delete pOutTypes;
		return hr;
	}
	else
	{
		return DMOUnregister(clsid, DMOCATEGORY_VIDEO_ENCODER);
	}
}

HRESULT CDMOEncoder::InternalGetInputStreamInfo(DWORD dwInputStreamIndex, DWORD *pdwFlags)
{
	*pdwFlags = DMO_INPUT_STREAMF_WHOLE_SAMPLES |
	            DMO_INPUT_STREAMF_SINGLE_SAMPLE_PER_BUFFER |
	            DMO_INPUT_STREAMF_FIXED_SAMPLE_SIZE |
	            DMO_INPUT_STREAMF_HOLDS_BUFFERS;

	return S_OK;
}

HRESULT CDMOEncoder::InternalGetOutputStreamInfo(DWORD dwOutputStreamIndex, DWORD *pdwFlags)
{
	*pdwFlags = DMO_OUTPUT_STREAMF_WHOLE_SAMPLES |
	            DMO_OUTPUT_STREAMF_SINGLE_SAMPLE_PER_BUFFER;

	return S_OK;
}

HRESULT CDMOEncoder::InternalCheckInputType(DWORD dwInputStreamIndex, const DMO_MEDIA_TYPE *pmt)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalCheckInputType()\n");

	const FORMATINFO *pfi;
	const VIDEOINFOHEADER *pvih;

	if (!IsEqualGUID(pmt->majortype, MEDIATYPE_Video))
		return DMO_E_INVALIDTYPE;
	if (!pmt->bFixedSizeSamples)
		return DMO_E_INVALIDTYPE;
	if (pmt->bTemporalCompression)
		return DMO_E_INVALIDTYPE;
	if (!IsEqualGUID(pmt->formattype, FORMAT_VideoInfo))
		return DMO_E_INVALIDTYPE;

	pvih = (const VIDEOINFOHEADER *)pmt->pbFormat;

	for (pfi = m_pCodec->GetEncoderInputFormat(); !IS_FORMATINFO_END(pfi); pfi++)
	{
		if (IsEqualGUID(pfi->guidMediaSubType, pmt->subtype))
		{
			if (m_pCodec->CompressQuery(&pvih->bmiHeader, NULL) == 0)
				return S_OK;
		}
	}

	return DMO_E_INVALIDTYPE;
}

HRESULT CDMOEncoder::InternalCheckOutputType(DWORD dwOutputStreamIndex, const DMO_MEDIA_TYPE *pmt)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalCheckOutputType()\n");

	const FORMATINFO *pfi;
	const DMO_MEDIA_TYPE *pmtIn;
	const VIDEOINFOHEADER *pvih;
	const VIDEOINFOHEADER *pvihIn;

	if (!InputTypeSet(0))
		return DMO_E_INVALIDTYPE;
	pmtIn = InputType(0);
	pvihIn = (const VIDEOINFOHEADER *)pmtIn->pbFormat;

	if (!IsEqualGUID(pmt->majortype, MEDIATYPE_Video))
		return DMO_E_INVALIDTYPE;
	if (!IsEqualGUID(pmt->formattype, FORMAT_VideoInfo))
		return DMO_E_INVALIDTYPE;

	pvih = (const VIDEOINFOHEADER *)pmt->pbFormat;

	pfi = m_pCodec->GetCompressedFormat();
	if (!IsEqualGUID(pfi->guidMediaSubType, pmt->subtype))
		return DMO_E_INVALIDTYPE;
	if (m_pCodec->CompressQuery(&pvihIn->bmiHeader, &pvih->bmiHeader) != 0)
		return DMO_E_INVALIDTYPE;

	return S_OK;
}

HRESULT CDMOEncoder::InternalGetInputType(DWORD dwInputStreamIndex, DWORD dwTypeIndex, DMO_MEDIA_TYPE *pmt)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalGetInputType()\n");

	const FORMATINFO *pfi = m_pCodec->GetEncoderInputFormat();

	while (dwTypeIndex > 0 && !IS_FORMATINFO_END(pfi))
	{
		pfi++;
		dwTypeIndex--;
	}

	if (IS_FORMATINFO_END(pfi))
		return DMO_E_NO_MORE_ITEMS;

	if (pmt != NULL)
	{
		memset(pmt, 0, sizeof(DMO_MEDIA_TYPE));
		pmt->majortype            = MEDIATYPE_Video;
		pmt->subtype              = pfi->guidMediaSubType;
		pmt->bFixedSizeSamples    = TRUE;
		pmt->bTemporalCompression = FALSE;
	}

	return S_OK;
}

HRESULT CDMOEncoder::InternalGetOutputType(DWORD dwOutputStreamIndex, DWORD dwTypeIndex, DMO_MEDIA_TYPE *pmt)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalGetOutputType()\n");

	const FORMATINFO *pfi = m_pCodec->GetCompressedFormat();

	if (dwTypeIndex != 0)
		return DMO_E_NO_MORE_ITEMS;

	memset(pmt, 0, sizeof(DMO_MEDIA_TYPE));
	pmt->majortype            = MEDIATYPE_Video;
	pmt->subtype              = pfi->guidMediaSubType;
	pmt->bFixedSizeSamples    = FALSE;
	pmt->bTemporalCompression = m_pCodec->IsTemporalCompressionSupported();

	if (InputTypeSet(0))
	{
		const DMO_MEDIA_TYPE *pmtIn = InputType(0);
		const VIDEOINFOHEADER *pvihIn = (const VIDEOINFOHEADER *)pmtIn->pbFormat;
		VIDEOINFOHEADER *pvih;
		DWORD biSize;

		biSize = m_pCodec->CompressGetFormat(&pvihIn->bmiHeader, NULL);
		MoInitMediaType(pmt, sizeof(DMO_MEDIA_TYPE) - sizeof(BITMAPINFOHEADER) + biSize);
		pvih = (VIDEOINFOHEADER *)pmt->pbFormat;
		memcpy(pvih, pvihIn, sizeof(VIDEOINFOHEADER) - sizeof(BITMAPINFOHEADER));
		m_pCodec->CompressGetFormat(&pvihIn->bmiHeader, &pvih->bmiHeader);
		pmt->formattype = FORMAT_VideoInfo;
	}

	return S_OK;
}

HRESULT CDMOEncoder::InternalGetInputSizeInfo(DWORD dwInputStreamIndex, DWORD *pcbSize, DWORD *pcbMaxLookahead, DWORD *pcbAlignment)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalGetInputSizeInfo()\n");

	*pcbSize = 0;
	*pcbMaxLookahead = 0;
	*pcbAlignment = 4;

	return S_OK;
}

HRESULT CDMOEncoder::InternalGetOutputSizeInfo(DWORD dwOutputStreamIndex, DWORD *pcbSize, DWORD *pcbAlignment)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalGetOutputSizeInfo()\n");

	*pcbSize = 0;
	*pcbAlignment = 4;

	return S_OK;
}

HRESULT CDMOEncoder::InternalGetInputMaxLatency(DWORD dwInputStreamIndex, REFERENCE_TIME *prtMaxLatency)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalGetInputMaxLatency()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalSetInputMaxLatency(DWORD dwInputStreamIndex, REFERENCE_TIME rtMaxLatency)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalSetInputMaxLatency()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalFlush()
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalFlush()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalDiscontinuity(DWORD dwInputStreamIndex)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalDiscontinuity()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalAllocateStreamingResources()
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalAllocateStreamingResources()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalFreeStreamingResources()
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalFreeStreamingResources()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalProcessInput(DWORD dwInputStreamIndex, IMediaBuffer *pBuffer, DWORD dwFlags, REFERENCE_TIME rtTimestamp, REFERENCE_TIME rtTimelength)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalProcessInput()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, DMO_OUTPUT_DATA_BUFFER *pOutputBuffers, DWORD *pdwStatus)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalProcessOutput()\n");

	return E_NOTIMPL;
}

HRESULT CDMOEncoder::InternalAcceptingInput(DWORD dwInputStreamIndex)
{
	_RPT0(_CRT_WARN, "CDMOEncoder::InternalAcceptingInput()\n");

	return E_NOTIMPL;
}
