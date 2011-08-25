/* 文字コードはＳＪＩＳ 改行コードはＣＲＬＦ */
/* $Id$ */

#include "stdafx.h"
//#include <windows.h>
//#include <algorithm>
//using namespace std;
#include "HuffmanCode.h"

struct hufftree {
	struct hufftree *left;
	struct hufftree *right;
	uint32_t count;
	uint32_t symbol;	// uint8_t ではないシンボルが入ることがある
};

inline bool hufftree_gt(const struct hufftree *a, const struct hufftree *b)
{
	return (a->count > b->count);
}

bool generate_code_length(uint8_t *codelen, const struct hufftree *node, uint8_t curlen)
{
	if (node->left == NULL) {
		codelen[node->symbol] = curlen;
		return (curlen > 24);
	} else {
		return
			generate_code_length(codelen, node->left, curlen+1) ||
			generate_code_length(codelen, node->right, curlen+1);
	}
}

static void GenerateLengthLimitedHuffmanCodeLengthTable(uint8_t *pCodeLengthTable)
{
	// とりあえずこれで逃げる。
	memset(pCodeLengthTable, 8, 256);
}

void GenerateHuffmanCodeLengthTable(uint8_t *pCodeLengthTable, const uint32_t *pCountTable)
{
	struct hufftree *huffsort[256];
	struct hufftree huffleaf[256];
	struct hufftree huffnode[256];
	int nsym;

	nsym = 0;
	for (int i = 0; i < 256; i++) {
		if (pCountTable[i] != 0) {
			huffleaf[nsym].left = NULL;
			huffleaf[nsym].right = NULL;
			huffleaf[nsym].count = pCountTable[i];
			huffleaf[nsym].symbol = i;
			huffsort[nsym] = &huffleaf[nsym];
			nsym++;
		}
		else
			pCodeLengthTable[i] = 255;
	}

	sort(huffsort, huffsort+nsym, hufftree_gt);
	for (int i = nsym - 2; i >= 0; i--) {
		huffnode[i].left = huffsort[i];
		huffnode[i].right = huffsort[i+1];
		huffnode[i].count = huffsort[i]->count + huffsort[i+1]->count;

		struct hufftree **insptr = upper_bound(huffsort, huffsort+i, &huffnode[i], hufftree_gt);
		struct hufftree **movptr;
		for (movptr = huffsort+i-1; movptr >= insptr; movptr--)
			*(movptr+1) = *movptr;
		*insptr = &huffnode[i];
	}

	if (generate_code_length(pCodeLengthTable, huffsort[0], 0))
		GenerateLengthLimitedHuffmanCodeLengthTable(pCodeLengthTable);
}

struct CODE_LENGTH_SORT
{
	uint8_t symbol;
	uint8_t codelen;
};

inline void sort_codelength(struct CODE_LENGTH_SORT *p)
{
	_ASSERT(sizeof(CODE_LENGTH_SORT) == sizeof(uint16_t));
	sort((uint16_t *)p, (uint16_t *)p+256);
}

void GenerateHuffmanEncodeTable(HUFFMAN_ENCODE_TABLE *pEncodeTable, const uint8_t *pCodeLengthTable)
{
	struct CODE_LENGTH_SORT cls[256];
	uint32_t curcode;

	for (int i = 0; i < 256; i++)
	{
		cls[i].symbol = i;
		cls[i].codelen = pCodeLengthTable[i];
	}

	sort_codelength(cls);

	if (cls[0].codelen == 0)
	{
		memset(pEncodeTable, 0, sizeof(HUFFMAN_ENCODE_TABLE));
		return;
	}

	memset(pEncodeTable, 0xff, sizeof(HUFFMAN_ENCODE_TABLE));

	curcode = 0;
	for (int i = 255; i >= 0; i--)
	{
		if (cls[i].codelen == 255)
			continue;
		pEncodeTable->dwTableMux[cls[i].symbol] = curcode | cls[i].codelen;
		curcode += 0x80000000 >> (cls[i].codelen - 1);
	}
}

// IA-32 の BSR 命令
// 本物の BSR 命令では入力が 0 の場合に出力が不定になる。
inline int bsr(uint32_t curcode)
{
	_ASSERT(curcode != 0);

	for (int i = 31; i >= 0; i--)
		if (curcode & (1 << i))
			return i;
	return rand() % 32;
}

void GenerateHuffmanDecodeTable(HUFFMAN_DECODE_TABLE *pDecodeTable, const uint8_t *pCodeLengthTable)
{
	struct CODE_LENGTH_SORT cls[256];
	int nLastIndex;
	int j;
	int base;
	uint32_t curcode;
	int nextfillidx;
	int lastfillidx;
	int prevbsrval;

	for (int i = 0; i < 256; i++)
	{
		cls[i].symbol = i;
		cls[i].codelen = pCodeLengthTable[i];
	}

	sort_codelength(cls);

	if (cls[0].codelen == 0)
	{
		memset(pDecodeTable, 0, sizeof(HUFFMAN_DECODE_TABLE));
		for (int i = 0; i < _countof(pDecodeTable->nCodeShift); i++)
			pDecodeTable->nCodeShift[i] = 31;
		for (int i = 0; i < _countof(pDecodeTable->SymbolAndCodeLength); i++)
		{
			pDecodeTable->SymbolAndCodeLength[i].nCodeLength = 0;
			pDecodeTable->SymbolAndCodeLength[i].bySymbol    = cls[0].symbol;
		}
		for (int i = 0; i < _countof(pDecodeTable->LookupSymbolAndCodeLength); i++)
		{
			pDecodeTable->LookupSymbolAndCodeLength[i].nCodeLength = 0;
			pDecodeTable->LookupSymbolAndCodeLength[i].bySymbol    = cls[0].symbol;
		}
		return;
	}

	// 最も長い符号長を持つシンボルの cls 上での位置を求める
	for (nLastIndex = 255; nLastIndex >= 0; nLastIndex--)
	{
		if (cls[nLastIndex].codelen != 255)
			break;
	}

	curcode = 1;
	j = 0;
	base = 0;
	nextfillidx = 0;
	prevbsrval = 0;
	for (int i = nLastIndex; i >= 0; i--)
	{
		int bsrval = bsr(curcode);
		if (bsrval != prevbsrval)
		{
			base = nextfillidx - (curcode >> (32 - cls[i].codelen));
		}
		for (; j <= bsrval; j++)
		{
			pDecodeTable->nCodeShift[j] = 32 - cls[i].codelen;
			pDecodeTable->dwSymbolBase[j] = base;
		}
		lastfillidx = nextfillidx + (1 << (32 - pDecodeTable->nCodeShift[bsrval] - cls[i].codelen));
		for (; nextfillidx < lastfillidx; nextfillidx++)
		{
			pDecodeTable->SymbolAndCodeLength[nextfillidx].bySymbol    = cls[i].symbol;
			pDecodeTable->SymbolAndCodeLength[nextfillidx].nCodeLength = cls[i].codelen;
		}
		curcode += 0x80000000 >> (cls[i].codelen - 1);
		prevbsrval = bsrval;
	}


	// テーブル一発参照用

	for (int i = 0; i < _countof(pDecodeTable->LookupSymbolAndCodeLength); i++)
		pDecodeTable->LookupSymbolAndCodeLength[i].nCodeLength = 255;

	curcode = 0;
	for (int i = 255; i >= 0; i--)
	{
		if (cls[i].codelen == 255)
			continue;
		if (cls[i].codelen <= HUFFMAN_DECODE_TABLELOOKUP_BITS)
		{
			int idx = curcode >> (32 - HUFFMAN_DECODE_TABLELOOKUP_BITS);
			for (int j = 0; j < (1 << (HUFFMAN_DECODE_TABLELOOKUP_BITS - cls[i].codelen)); j++)
			{
				pDecodeTable->LookupSymbolAndCodeLength[idx+j].nCodeLength = cls[i].codelen;
				pDecodeTable->LookupSymbolAndCodeLength[idx+j].bySymbol = cls[i].symbol;
			}
		}
		curcode += 0x80000000 >> (cls[i].codelen - 1);
	}
}

inline void EncodeSymbol(uint8_t bySymbol, const HUFFMAN_ENCODE_TABLE *pEncodeTable, uint32_t *&pDst, uint32_t &dwTmpEncoded, int &nBits)
{
	int nCurBits = pEncodeTable->dwTableMux[bySymbol] & 0xff;
	uint32_t dwCurEncoded = pEncodeTable->dwTableMux[bySymbol] & 0xffffff00;

	dwTmpEncoded |= dwCurEncoded >> nBits;
	nBits += nCurBits;
	if (nBits >= 32)
	{
		*pDst++ = dwTmpEncoded;
		nBits -= 32;
		dwTmpEncoded = dwCurEncoded << (nCurBits - nBits);
	}
}

size_t cpp_HuffmanEncode(uint8_t *pDstBegin, const uint8_t *pSrcBegin, const uint8_t *pSrcEnd, const HUFFMAN_ENCODE_TABLE *pEncodeTable)
{
	int nBits;
	uint32_t dwTmpEncoded;
	uint32_t *pDst;
	const uint8_t *p;

	if (pEncodeTable->dwTableMux[0] == 0)
		return 0;

	nBits = 0;
	dwTmpEncoded = 0;
	pDst = (uint32_t *)pDstBegin;

	for (p = pSrcBegin; p < pSrcEnd; p++)
		EncodeSymbol(*p, pEncodeTable, pDst, dwTmpEncoded, nBits);

	if (nBits != 0)
		*pDst++ = dwTmpEncoded;

	return ((uint8_t *)pDst) - pDstBegin;
}

inline void DecodeSymbol(uint32_t *&pSrc, int &nBits, const HUFFMAN_DECODE_TABLE *pDecodeTable, bool bAccum, uint8_t &byPrevSymbol, uint8_t *pDst, int nCorrPos, bool bDummyAlpha)
{
	uint32_t code;
	uint8_t symbol;
	int codelen;

	if (nBits == 0)
		code = (*pSrc) | 0x00000001;
	else
		code = ((*pSrc) << nBits) | ((*(pSrc+1)) >> (32 - nBits)) | 0x00000001;

	int tableidx = code >> (32 - HUFFMAN_DECODE_TABLELOOKUP_BITS);
	if (pDecodeTable->LookupSymbolAndCodeLength[tableidx].nCodeLength != 255)
	{
		symbol = pDecodeTable->LookupSymbolAndCodeLength[tableidx].bySymbol;
		codelen = pDecodeTable->LookupSymbolAndCodeLength[tableidx].nCodeLength;
	}
	else
	{
		int bsrval = bsr(code);
		int codeshift = pDecodeTable->nCodeShift[bsrval];
		code >>= codeshift;
		tableidx = pDecodeTable->dwSymbolBase[bsrval] + code;
		symbol = pDecodeTable->SymbolAndCodeLength[tableidx].bySymbol;
		codelen = pDecodeTable->SymbolAndCodeLength[tableidx].nCodeLength;
	}

	if (bAccum)
	{
		symbol += byPrevSymbol;
		byPrevSymbol = symbol;
	}

	if (nCorrPos != 0)
		symbol += *(pDst + nCorrPos) + 0x80;

	*pDst = symbol;
	if (bDummyAlpha)
		*(pDst+1) = 0xff;
	nBits += codelen;

	if (nBits >= 32)
	{
		nBits -= 32;
		pSrc++;
	}
}

static void cpp_HuffmanDecode_common(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, bool bAccum, int nStep, bool bBottomup, size_t dwNetWidth, size_t dwGrossWidth, int nCorrPos, bool bDummyAlpha)
{
	int nBits;
	uint32_t *pSrc;
	uint8_t *p;
	uint8_t prevsym;
	uint8_t *pStripeBegin;

	nBits = 0;
	pSrc = (uint32_t *)pSrcBegin;
	prevsym = 0x80;

	if (!bBottomup)
	{
		for (pStripeBegin = pDstBegin; pStripeBegin < pDstEnd; pStripeBegin += dwGrossWidth)
		{
			uint8_t *pStripeEnd = pStripeBegin + dwNetWidth;
			for (p = pStripeBegin; p < pStripeEnd; p += nStep)
				DecodeSymbol(pSrc, nBits, pDecodeTable, bAccum, prevsym, p, nCorrPos, bDummyAlpha);
		}
	}
	else
	{
		for (pStripeBegin = pDstEnd - dwGrossWidth; pStripeBegin >= pDstBegin; pStripeBegin -= dwGrossWidth)
		{
			uint8_t *pStripeEnd = pStripeBegin + dwNetWidth;
			for (p = pStripeBegin; p < pStripeEnd; p += nStep)
				DecodeSymbol(pSrc, nBits, pDecodeTable, bAccum, prevsym, p, nCorrPos, bDummyAlpha);
		}
	}
}

void cpp_HuffmanDecode(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, false, 1, false, pDstEnd - pDstBegin, pDstEnd - pDstBegin, 0, false);
}

void cpp_HuffmanDecodeAndAccum(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 1, false, pDstEnd - pDstBegin, pDstEnd - pDstBegin, 0, false);
}

void cpp_HuffmanDecodeAndAccumStep2(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 2, false, pDstEnd - pDstBegin, pDstEnd - pDstBegin, 0, false);
}

void cpp_HuffmanDecodeAndAccumStep4(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 4, false, pDstEnd - pDstBegin, pDstEnd - pDstBegin, 0, false);
}

void cpp_HuffmanDecodeAndAccumStep4ForBottomupRGB32Green(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, size_t dwNetWidth, size_t dwGrossWidth)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 4, true, dwNetWidth, dwGrossWidth, 0, false);
}

void cpp_HuffmanDecodeAndAccumStep4ForBottomupRGB32Blue(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, size_t dwNetWidth, size_t dwGrossWidth)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 4, true, dwNetWidth, dwGrossWidth, +1, false);
}

void cpp_HuffmanDecodeAndAccumStep4ForBottomupRGB32Red(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, size_t dwNetWidth, size_t dwGrossWidth)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 4, true, dwNetWidth, dwGrossWidth, -1, false);
}

void cpp_HuffmanDecodeAndAccumStep4ForBottomupRGB32RedAndDummyAlpha(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, size_t dwNetWidth, size_t dwGrossWidth)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 4, true, dwNetWidth, dwGrossWidth, -1, true);
}

void cpp_HuffmanDecodeAndAccumStep3ForBottomupRGB24Green(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, size_t dwNetWidth, size_t dwGrossWidth)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 3, true, dwNetWidth, dwGrossWidth, 0, false);
}

void cpp_HuffmanDecodeAndAccumStep3ForBottomupRGB24Blue(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, size_t dwNetWidth, size_t dwGrossWidth)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 3, true, dwNetWidth, dwGrossWidth, +1, false);
}

void cpp_HuffmanDecodeAndAccumStep3ForBottomupRGB24Red(uint8_t *pDstBegin, uint8_t *pDstEnd, const uint8_t *pSrcBegin, const HUFFMAN_DECODE_TABLE *pDecodeTable, size_t dwNetWidth, size_t dwGrossWidth)
{
	cpp_HuffmanDecode_common(pDstBegin, pDstEnd, pSrcBegin, pDecodeTable, true, 3, true, dwNetWidth, dwGrossWidth, -1, false);
}
