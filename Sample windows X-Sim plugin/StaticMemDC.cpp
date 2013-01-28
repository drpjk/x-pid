// StaticMemDC.cpp: Implementierungsdatei
//

#include "stdafx.h"
//#include "Settings.h"
#include "StaticMemDC.h"


// CStaticMemDC

IMPLEMENT_DYNAMIC(CStaticMemDC, CStatic)

CStaticMemDC::CStaticMemDC()
{

}

CStaticMemDC::~CStaticMemDC()
{
}


BEGIN_MESSAGE_MAP(CStaticMemDC, CStatic)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
END_MESSAGE_MAP()



// CXPStatic-Meldungshandler


BOOL CStaticMemDC::OnEraseBkgnd(CDC* pDC)
{
	return FALSE;
}

void CStaticMemDC::OnPaint()
{
	CPaintDC dc(this);
	CRect clip;
    GetWindowRect(&clip);		// get rect of the control
    ScreenToClient(&clip);
    CMemDC2 memDC(&dc,&clip);
	DefWindowProcA(WM_PAINT, (WPARAM)memDC->m_hDC, (LPARAM)0);
}