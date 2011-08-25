/* �����R�[�h�͂r�i�h�r ���s�R�[�h�͂b�q�k�e */
/* $Id$ */

#include "StdAfx.h"
#include "utvideo.h"
#include "ULRACodec.h"
#include "Predict.h"
#include "MediaSubType.h"

const utvf_t CULRACodec::m_utvfEncoderInput[] = {
	UTVF_ARGB32_WIN,
	UTVF_RGB32_WIN,
	UTVF_INVALID,
};

const utvf_t CULRACodec::m_utvfDecoderOutput[] = {
	UTVF_ARGB32_WIN,
	UTVF_RGB32_WIN,
	UTVF_INVALID,
};

const utvf_t CULRACodec::m_utvfCompressed[] = {
	UTVF_ULRA,
	UTVF_INVALID,
};

CULRACodec::CULRACodec(const char *pszInterfaceName) : CUL00Codec(pszInterfaceName)
{
}

CULRACodec::~CULRACodec(void)
{
}

CCodec *CULRACodec::CreateInstance(const char *pszInterfaceName)
{
	return new CULRACodec(pszInterfaceName);
}

void CULRACodec::CalcPlaneSizes(unsigned int width, unsigned int height)
{
	m_dwPlaneSize[0]          = width * height;
	m_dwPlaneSize[1]          = width * height;
	m_dwPlaneSize[2]          = width * height;
	m_dwPlaneSize[3]          = width * height;

	m_dwPlaneWidth[0]         = width;
	m_dwPlaneWidth[1]         = width;
	m_dwPlaneWidth[2]         = width;
	m_dwPlaneWidth[3]         = width;

	m_dwPlaneStripeSize[0]    = width;
	m_dwPlaneStripeSize[1]    = width;
	m_dwPlaneStripeSize[2]    = width;
	m_dwPlaneStripeSize[3]    = width;

	m_dwPlanePredictStride[0] = width;
	m_dwPlanePredictStride[1] = width;
	m_dwPlanePredictStride[2] = width;
	m_dwPlanePredictStride[3] = width;
}

void CULRACodec::ConvertToPlanar(DWORD nBandIndex)
{
	BYTE *g, *b, *r, *a;
	const BYTE *pSrcBegin, *pSrcEnd, *pStrideBegin, *p;

	pSrcBegin = ((BYTE *)m_pInput) + m_dwRawStripeBegin[nBandIndex] * m_dwRawStripeSize;
	pSrcEnd   = ((BYTE *)m_pInput) + m_dwRawStripeEnd[nBandIndex]   * m_dwRawStripeSize;
	g = m_pCurFrame->GetPlane(0) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[0];
	b = m_pCurFrame->GetPlane(1) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[1];
	r = m_pCurFrame->GetPlane(2) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[2];
	a = m_pCurFrame->GetPlane(3) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[3];

	switch (m_utvfRaw)
	{
	case UTVF_ARGB32_WIN:
	case UTVF_RGB32_WIN:
		for (pStrideBegin = pSrcEnd - m_dwRawGrossWidth; pStrideBegin >= pSrcBegin; pStrideBegin -= m_dwRawGrossWidth)
		{
			const BYTE *pStrideEnd = pStrideBegin + m_nWidth * 4;
			for (p = pStrideBegin; p < pStrideEnd; p += 4)
			{
				*g++ = *(p+1);
				*b++ = *(p+0) - *(p+1) + 0x80;
				*r++ = *(p+2) - *(p+1) + 0x80;
				*a++ = *(p+3);
			}
		}
		break;
	}
}

void CULRACodec::ConvertFromPlanar(DWORD nBandIndex)
{
	const BYTE *g, *b, *r, *a;
	BYTE *pDstBegin, *pDstEnd, *pStrideBegin, *p;

	pDstBegin = ((BYTE *)m_pOutput) + m_dwRawStripeBegin[nBandIndex] * m_dwRawStripeSize;
	pDstEnd   = ((BYTE *)m_pOutput) + m_dwRawStripeEnd[nBandIndex]   * m_dwRawStripeSize;
	g = m_pCurFrame->GetPlane(0) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[0];
	b = m_pCurFrame->GetPlane(1) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[1];
	r = m_pCurFrame->GetPlane(2) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[2];
	a = m_pCurFrame->GetPlane(3) + m_dwPlaneStripeBegin[nBandIndex] * m_dwPlaneStripeSize[3];

	switch (m_utvfRaw)
	{
	case UTVF_ARGB32_WIN:
	case UTVF_RGB32_WIN:
		for (pStrideBegin = pDstEnd - m_dwRawGrossWidth; pStrideBegin >= pDstBegin; pStrideBegin -= m_dwRawGrossWidth)
		{
			BYTE *pStrideEnd = pStrideBegin + m_nWidth * 4;
			for (p = pStrideBegin; p < pStrideEnd; p += 4)
			{
				*(p+1) = *g;
				*(p+0) = *b + *g - 0x80;
				*(p+2) = *r + *g - 0x80;
				*(p+3) = *a;
				g++; b++; r++; a++;
			}
		}
		break;
	}
}

bool CULRACodec::DecodeDirect(DWORD nBandIndex)
{
	if ((m_fi.dwFlags0 & FI_FLAGS0_INTRAFRAME_PREDICT_MASK) != FI_FLAGS0_INTRAFRAME_PREDICT_LEFT)
		return false;

	BYTE *pDstBegin = ((BYTE *)m_pOutput) + m_dwRawStripeBegin[nBandIndex] * m_dwRawStripeSize;
	BYTE *pDstEnd   = ((BYTE *)m_pOutput) + m_dwRawStripeEnd[nBandIndex]   * m_dwRawStripeSize;

	switch (m_utvfRaw)
	{
	case UTVF_ARGB32_WIN:
	case UTVF_RGB32_WIN:
		HuffmanDecodeAndAccumStep4ForBottomupRGB32Green(pDstBegin+1, pDstEnd+1, m_pDecodeCode[0][nBandIndex], &m_hdt[0], m_dwRawNetWidth, m_dwRawGrossWidth);
		HuffmanDecodeAndAccumStep4ForBottomupRGB32Blue (pDstBegin+0, pDstEnd+0, m_pDecodeCode[1][nBandIndex], &m_hdt[1], m_dwRawNetWidth, m_dwRawGrossWidth);
		HuffmanDecodeAndAccumStep4ForBottomupRGB32Red  (pDstBegin+2, pDstEnd+2, m_pDecodeCode[2][nBandIndex], &m_hdt[2], m_dwRawNetWidth, m_dwRawGrossWidth);
		HuffmanDecodeAndAccumStep4ForBottomupRGB32Green(pDstBegin+3, pDstEnd+3, m_pDecodeCode[3][nBandIndex], &m_hdt[3], m_dwRawNetWidth, m_dwRawGrossWidth);
		return true;
	}

	return false;
}