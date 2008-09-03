/* 文字コードはＳＪＩＳ 改行コードはＣＲＬＦ */
/* $Id$ */
/*
 * Ut Video Codec Suite
 * Copyright (C) 2008  UMEZAWA Takeshi
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * 
 * 
 * Ut Video Codec Suite
 * Copyright (C) 2008  梅澤 威志
 * 
 * このプログラムはフリーソフトウェアです。あなたはこれを、フリーソフ
 * トウェア財団によって発行された GNU 一般公衆利用許諾契約書(バージョ
 * ン2か、希望によってはそれ以降のバージョンのうちどれか)の定める条件
 * の下で再頒布または改変することができます。
 * 
 * このプログラムは有用であることを願って頒布されますが、*全くの無保
 * 証* です。商業可能性の保証や特定の目的への適合性は、言外に示された
 * ものも含め全く存在しません。詳しくはGNU 一般公衆利用許諾契約書をご
 * 覧ください。
 * 
 * あなたはこのプログラムと共に、GNU 一般公衆利用許諾契約書の複製物を
 * 一部受け取ったはずです。もし受け取っていなければ、フリーソフトウェ
 * ア財団まで請求してください(宛先は the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA)。
 */

#include "StdAfx.h"
#include "utvideo.h"
#include "UAY2Decoder.h"
#include "Predict.h"
#include "Convert.h"

const CPackedDecoder::OUTPUTFORMAT CUAY2Decoder::m_outfmts[] = {
	{ FCC('YUY2'), 16, TRUE }, { FCC('YUYV'), 16, TRUE }, { FCC('YUNV'), 16, TRUE },
	{ FCC('UYVY'), 16, TRUE }, { FCC('UYNV'), 16, TRUE },
	{ FCC('YVYU'), 16, TRUE },
	{ FCC('VYUY'), 16, TRUE },
	{ BI_RGB, 32, FALSE },
	{ BI_RGB, 24, FALSE },
};

CUAY2Decoder::CUAY2Decoder(void)
{
}

CUAY2Decoder::~CUAY2Decoder(void)
{
}

int CUAY2Decoder::GetNumSupportedOutputFormats(void)
{
	return _countof(m_outfmts);
}

CDecoder *CUAY2Decoder::CreateInstance(void)
{
	return new CUAY2Decoder();
}

void CUAY2Decoder::CalcPlaneSizes(const BITMAPINFOHEADER *pbihOut)
{
	m_dwPlaneStride[0] = ROUNDUP(pbihOut->biWidth, 2);
	m_dwPlaneSize[0]   = m_dwPlaneStride[0] * m_dwNumStrides;

	m_dwPlaneStride[1] = ROUNDUP(pbihOut->biWidth, 2) / 2;
	m_dwPlaneSize[1]   = m_dwPlaneStride[1] * m_dwNumStrides;

	m_dwPlaneStride[2] = m_dwPlaneStride[1];
	m_dwPlaneSize[2]   = m_dwPlaneSize[1];
}

void CUAY2Decoder::ConvertFromPlanar(DWORD nBandIndex)
{
	DWORD dwPlaneStrideBegin = m_dwNumStrides *  nBandIndex      / m_dwDivideCount;
	DWORD dwPlaneStrideEnd   = m_dwNumStrides * (nBandIndex + 1) / m_dwDivideCount;
	DWORD dwFrameStrideBegin, dwFrameStrideEnd;

	const BYTE *y, *u, *v;
	BYTE *pDstBegin, *pDstEnd, *p;

	if (!m_bBottomUpFrame)
	{
		dwFrameStrideBegin = dwPlaneStrideBegin;
		dwFrameStrideEnd   = dwPlaneStrideEnd;
	}
	else
	{
		dwFrameStrideBegin = m_dwNumStrides - dwPlaneStrideEnd;
		dwFrameStrideEnd   = m_dwNumStrides - dwPlaneStrideBegin;
	}

	pDstBegin = ((BYTE *)m_icd->lpOutput) + dwFrameStrideBegin * m_dwFrameStride;
	pDstEnd   = ((BYTE *)m_icd->lpOutput) + dwFrameStrideEnd   * m_dwFrameStride;
	y = m_pCurFrame->GetPlane(0) + dwPlaneStrideBegin * m_dwPlaneStride[0];
	u = m_pCurFrame->GetPlane(1) + dwPlaneStrideBegin * m_dwPlaneStride[1];
	v = m_pCurFrame->GetPlane(2) + dwPlaneStrideBegin * m_dwPlaneStride[2];

	switch (m_icd->lpbiOutput->biCompression)
	{
	case FCC('YUY2'):
	case FCC('YUYV'):
	case FCC('YUNV'):
		for (p = pDstBegin; p < pDstEnd; p += 4)
		{
			*p     = *y++;
			*(p+1) = *u++;
			*(p+2) = *y++;
			*(p+3) = *v++;
		}
		break;
	case FCC('UYVY'):
	case FCC('UYNV'):
		for (p = pDstBegin; p < pDstEnd; p += 4)
		{
			*p     = *u++;
			*(p+1) = *y++;
			*(p+2) = *v++;
			*(p+3) = *y++;
		}
		break;
	case FCC('YVYU'):
		for (p = pDstBegin; p < pDstEnd; p += 4)
		{
			*p     = *y++;
			*(p+1) = *v++;
			*(p+2) = *y++;
			*(p+3) = *u++;
		}
		break;
	case FCC('VYUY'):
		for (p = pDstBegin; p < pDstEnd; p += 4)
		{
			*p     = *v++;
			*(p+1) = *y++;
			*(p+2) = *u++;
			*(p+3) = *y++;
		}
		break;
	case BI_RGB:
		switch (m_icd->lpbiOutput->biBitCount)
		{
		case 24:
			ConvertULY2ToBottomupRGB24(pDstBegin, pDstEnd, y, u, v, m_dwFrameStride, m_icd->lpbiOutput->biWidth * 3);
			break;
		case 32:
			ConvertULY2ToBottomupRGB32(pDstBegin, pDstEnd, y, u, v, m_dwFrameStride, m_dwFrameStride);
			break;
		}
		break;
	}
}
