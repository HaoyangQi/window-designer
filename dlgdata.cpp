#include "dlgdata.h"

// Reference: http://bytepointer.com/resources/win32_res_format_older.htm

// aligned to 2-byte boundary
PBYTE alignToWORD(PBYTE ptr, int* padSize)
{
	// multiple of 2 always hold 0 on first binary digit, 
	// and this bit represents the number of padding bits (0 or 1)
	unsigned long long val = (unsigned long long)ptr & 1;

	if (padSize) {
		*padSize = val;
	}

	return ptr + val;
}

// aligned to 4-byte boundary
PBYTE alignToDWORD(PBYTE ptr, int* padSize)
{
	// standard padding (on n-byte bound): aligned = ((addr + n - 1) / n) * n, 
	// where the division only keep integer part
	// unsigned long long (hence __int64) is the only type that is safe for casting pointer
	unsigned long long val = (((unsigned long long)ptr + 3) >> 2) << 2;

	if (padSize) {
		*padSize = (PBYTE)val - ptr;
	}

	return (PBYTE)val;
}

// Every array is WCHAR and WORD-aligned
bool safeCopy(PBYTE pBlockStart, PBYTE pTempl, void* data, size_t size, size_t reserved)
{
	pTempl = alignToWORD(pTempl, NULL);

	if (pTempl + size - pBlockStart > reserved) {
		return false;
	}

	memcpy(pTempl, data, size);

	return true;
}

size_t TemplateGetSizeAligned(DIALOG_TEMPLATE_DATA* pdtd)
{
	// set initial dummy addr to 0 since it is on DWORD boundary
	PBYTE s = (PBYTE)0; // use a false pointer to test size after padding

	s += 26; // first 10 fields

	s = alignToWORD(s, NULL);
	s += pdtd->size_class;
	s = alignToWORD(s, NULL);
	s += pdtd->size_menu;
	s = alignToWORD(s, NULL);
	s += pdtd->size_title;

	if (pdtd->style & DS_SETFONT || pdtd->style & DS_SHELLFONT) {
		s += 6; // 4 fields for fonts
		s = alignToWORD(s, NULL);
		s += pdtd->size_typeface;
	}

	if (pdtd->cDlgItems != 0) {
		DIALOG_TEMPLATE_ITEM_DATA* pItem = pdtd->pItemBegin;
		while (pItem) {
			s = alignToDWORD(s, NULL);

			s += 24; // 8 fields
			s = alignToWORD(s, NULL);
			s += pItem->size_class;
			s = alignToWORD(s, NULL);
			s += pItem->size_title;
			s += 2;

			if (pItem->extraCount > 0) {
				s = alignToWORD(s, NULL);
				s += pItem->extraCount;
			}

			pItem = pItem->next;
		}
	}

	return (size_t)s;
}

int TemplateBindDialogData(PBYTE pMem, PBYTE pTempl, DIALOG_TEMPLATE_DATA* pdtd, size_t reserved)
{
	size_t delta;

	// if not aligned on DWORD boundary, raise error
	if ((unsigned long long)pTempl & 3) {
		return DLG_MEM_NOT_ALIGNED;
	}

	*((PWORD)pTempl) = pdtd->dlgVer;
	pTempl += sizeof(WORD);
	*((PWORD)pTempl) = pdtd->signature;
	pTempl += sizeof(WORD);
	*((PDWORD)pTempl) = pdtd->helpID;
	pTempl += sizeof(DWORD);
	*((PDWORD)pTempl) = pdtd->exStyle;
	pTempl += sizeof(DWORD);
	*((PDWORD)pTempl) = pdtd->style;
	pTempl += sizeof(DWORD);
	*((PWORD)pTempl) = pdtd->cDlgItems;
	pTempl += sizeof(WORD);
	*((short*)pTempl) = pdtd->x;
	pTempl += sizeof(short);
	*((short*)pTempl) = pdtd->y;
	pTempl += sizeof(short);
	*((short*)pTempl) = pdtd->cx;
	pTempl += sizeof(short);
	*((short*)pTempl) = pdtd->cy;
	pTempl += sizeof(short);

	delta = pdtd->size_menu;
	if (!safeCopy(pMem, pTempl, pdtd->menu, delta, reserved)) {
		return DLG_MEM_EXCESS_USAGE;
	}
	pTempl += delta;

	delta = pdtd->size_class;
	if (!safeCopy(pMem, pTempl, pdtd->windowClass, delta, reserved)) {
		return DLG_MEM_EXCESS_USAGE;
	}
	pTempl += delta;

	delta = pdtd->size_title;
	if (!safeCopy(pMem, pTempl, pdtd->title, delta, reserved)) {
		return DLG_MEM_EXCESS_USAGE;
	}
	pTempl += delta;

	if (pdtd->style & DS_SETFONT || pdtd->style & DS_SHELLFONT)
	{
		*((PWORD)pTempl) = pdtd->pointsize;
		pTempl += sizeof(WORD);
		*((PWORD)pTempl) = pdtd->weight;
		pTempl += sizeof(WORD);
		*pTempl = pdtd->italic;
		pTempl += sizeof(BYTE);
		*pTempl = pdtd->charset;
		pTempl += sizeof(BYTE);

		delta = pdtd->size_typeface;
		if (!safeCopy(pMem, pTempl, pdtd->typeface, delta, reserved)) {
			return DLG_MEM_EXCESS_USAGE;
		}
		pTempl += delta;
	}

	if (pdtd->cDlgItems > 0)
	{
		DIALOG_TEMPLATE_ITEM_DATA* pItem = pdtd->pItemBegin;
		while (pItem) {
			pTempl = alignToDWORD(pTempl, NULL);

			*((PDWORD)pTempl) = pItem->helpID;
			pTempl += sizeof(DWORD);
			*((PDWORD)pTempl) = pItem->exStyle;
			pTempl += sizeof(DWORD);
			*((PDWORD)pTempl) = pItem->style;
			pTempl += sizeof(DWORD);
			*((short*)pTempl) = pItem->x;
			pTempl += sizeof(short);
			*((short*)pTempl) = pItem->y;
			pTempl += sizeof(short);
			*((short*)pTempl) = pItem->cx;
			pTempl += sizeof(short);
			*((short*)pTempl) = pItem->cy;
			pTempl += sizeof(short);
			*((PDWORD)pTempl) = pItem->id;
			pTempl += sizeof(DWORD);

			delta = pItem->size_class;
			if (!safeCopy(pMem, pTempl, pItem->windowClass, delta, reserved)) {
				return DLG_MEM_EXCESS_USAGE;
			}
			pTempl += delta;

			delta = pItem->size_title;
			if (!safeCopy(pMem, pTempl, pItem->title, delta, reserved)) {
				return DLG_MEM_EXCESS_USAGE;
			}
			pTempl += delta;

			*((PWORD)pTempl) = pItem->extraCount;
			pTempl += sizeof(WORD);

			if (pItem->extraCount > 0) {
				if (!safeCopy(pMem, pTempl, pItem->extraData, pItem->extraCount, reserved)) {
					return DLG_MEM_EXCESS_USAGE;
				}
				pTempl += pItem->extraCount;
			}

			pItem = pItem->next;
		}
	}

	return DLG_MEM_SUCCESS;
}

DIALOG_TEMPLATE_DATA* TemplateCreate()
{
	DIALOG_TEMPLATE_DATA* pdtd = (DIALOG_TEMPLATE_DATA*)malloc(sizeof(DIALOG_TEMPLATE_DATA));

	memset(pdtd, 0, sizeof(DIALOG_TEMPLATE_DATA));
	pdtd->dlgVer = 1;
	pdtd->signature = 0xFFFF;

	// since memory is reset to 0, so following are not necessary, but they are for correctness
	//pwd->pDlgTemplate->cDlgItems = 0;
	//pwd->pDlgTemplate->item_id = 0;

	return pdtd;
}

void TemplateRelease(DIALOG_TEMPLATE_DATA* pdtd)
{
	free(pdtd->menu);
	free(pdtd->windowClass);
	free(pdtd->title);
	free(pdtd->typeface);

	DIALOG_TEMPLATE_ITEM_DATA* pItem = pdtd->pItemBegin, * pTemp;
	while (pItem) {
		pTemp = pItem->next;
		TemplateDeleteByReference(pdtd, pItem);
		pItem = pTemp;
	}

	free(pdtd);
}
