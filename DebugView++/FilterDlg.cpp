// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#include "stdafx.h"
#include <boost/algorithm/string.hpp>
#include <atlstr.h>
#include "LogFilter.h"
#include "resource.h"
#include "Utilities.h"
#include "FilterDlg.h"

namespace fusion {
namespace debugviewpp {

static COLORREF HighlightColors[16] = 
{
	RGB(255, 255, 255), // white
	RGB(192, 192, 192), // light-grey
	RGB(128, 128, 128), // mid-grey
	RGB( 64,  64,  64), // dark-grey
	RGB(  0,   0,   0), // black
	RGB( 27, 161, 226), // blue
	RGB(160,  80,   0), // brown
	RGB( 51, 153,  51), // green
	RGB(162, 193,  57), // lime
	RGB(216,   0, 115), // magenta
	RGB(240, 150,   9), // mango (orange)
	RGB(230, 113, 184), // pink
	RGB(162,   0, 255), // purple
	RGB(229,  20,   0), // red
	RGB(  0, 171, 169), // teal (viridian)
	RGB(255, 255, 255), // white
};

void InitializeCustomColors()
{
	auto colors = ColorDialog::GetCustomColors();
	for (int i = 0; i < 16; ++i)
		colors[i] = HighlightColors[i];
}

bool CustomColorsInitialized = (InitializeCustomColors(), true);

BEGIN_MSG_MAP_TRY(CFilterDlg)
	MSG_WM_INITDIALOG(OnInitDialog)
	MSG_WM_DESTROY(OnDestroy)
	MSG_WM_SIZE(OnSize)
	COMMAND_ID_HANDLER_EX(IDC_FILTER_SAVE, OnSave)
	COMMAND_ID_HANDLER_EX(IDC_FILTER_LOAD, OnLoad)
	COMMAND_ID_HANDLER_EX(IDC_FILTER_CLEARALL, OnClearAll)
	COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
	COMMAND_ID_HANDLER_EX(IDOK, OnOk)
	NOTIFY_CODE_HANDLER_EX(TCN_SELCHANGE, OnTabSelChange)
	REFLECT_NOTIFICATIONS()
	CHAIN_MSG_MAP(CDialogResize<CFilterDlg>)
END_MSG_MAP_CATCH(ExceptionHandler)

CFilterDlg::CFilterDlg(const std::wstring& name, const LogFilter& filters) :
	m_messagePage(filters.messageFilters),
	m_processPage(filters.processFilters),
	m_name(name),
	m_filter(filters)
{
}

std::wstring CFilterDlg::GetName() const
{
	return m_name;
}

LogFilter CFilterDlg::GetFilters() const
{
	return m_filter;
}

void CFilterDlg::ExceptionHandler()
{
	MessageBox(WStr(GetExceptionMessage()).c_str(), LoadString(IDR_APPNAME).c_str(), MB_ICONERROR | MB_OK);
}

BOOL CFilterDlg::OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/)
{
	SetDlgItemText(IDC_NAME, m_name.c_str());

	m_tabCtrl.Attach(GetDlgItem(IDC_TAB));
	m_tabCtrl.AddItem(L"Messages");
	m_tabCtrl.AddItem(L"Processes");
	CRect tabRect;
	m_tabCtrl.GetWindowRect(&tabRect);
	m_tabCtrl.AdjustRect(false, &tabRect);
	m_tabCtrl.ScreenToClient(&tabRect);

	CRect dlgRect;
	GetClientRect(&dlgRect);
	m_border.cx = dlgRect.Width() - tabRect.Width();
	m_border.cy = dlgRect.Height() - tabRect.Height();

	m_messagePage.Create(m_tabCtrl, tabRect);
	m_messagePage.MoveWindow(&tabRect);
	m_messagePage.ShowWindow(SW_SHOW);

	m_processPage.Create(m_tabCtrl, tabRect);
	m_processPage.MoveWindow(&tabRect);
	m_processPage.ShowWindow(SW_HIDE);

	CenterWindow(GetParent());
	DlgResize_Init();

	return TRUE;
}

void CFilterDlg::OnDestroy()
{
}

void CFilterDlg::OnSize(UINT /*nType*/, CSize size)
{
	RECT rect;
	m_tabCtrl.GetWindowRect(&rect);
	m_tabCtrl.AdjustRect(false, &rect);
	m_tabCtrl.ScreenToClient(&rect);
	rect.right = rect.left + size.cx - m_border.cx;
	rect.bottom = rect.top + size.cy - m_border.cy;

	m_messagePage.MoveWindow(&rect);
	m_processPage.MoveWindow(&rect);
	SetMsgHandled(FALSE);
}

LRESULT CFilterDlg::OnTabSelChange(NMHDR* /*pnmh*/)
{
	int tab = m_tabCtrl.GetCurSel();
	m_messagePage.ShowWindow(tab == 0 ? SW_SHOW : SW_HIDE);
	m_processPage.ShowWindow(tab == 1 ? SW_SHOW : SW_HIDE);
	return 0;
}

std::wstring GetFileNameExt(const std::wstring& fileName)
{
	auto it = fileName.find_last_of('.');
	if (it == fileName.npos)
		return std::wstring();
	return fileName.substr(it + 1);
}

void CFilterDlg::OnSave(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFileDialog dlg(false, L".xml", m_name.c_str(), OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		L"XML Files (*.xml)\0*.xml\0"
		L"JSON Files (*.json)\0*.json\0"
		L"All Files\0*.*\0"
		L"\0", 0);
	dlg.m_ofn.nFilterIndex = 0;
	dlg.m_ofn.lpstrTitle = L"Save DebugView Filter";
	if (dlg.DoModal() != IDOK)
		return;

	LogFilter filter;
	auto name = fusion::GetDlgItemText(*this, IDC_NAME);
	filter.messageFilters = m_messagePage.GetFilters();
	filter.processFilters = m_processPage.GetFilters();

	auto ext = GetFileNameExt(dlg.m_szFileName);
	auto fileName = Str(dlg.m_szFileName).str();
	if (boost::iequals(ext, L"json"))
		SaveJson(fileName, Str(name), filter);
	else /* if (boost::iequals(ext, L"xml")) */
		SaveXml(fileName, Str(name), filter);
}

void CFilterDlg::OnLoad(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFileDialog dlg(true, L".xml", m_name.c_str(), OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		L"XML Files (*.xml)\0*.xml\0"
		L"JSON Files (*.json)\0*.json\0"
		L"All Files\0*.*\0"
		L"\0", 0);
	dlg.m_ofn.nFilterIndex = 0;
	dlg.m_ofn.lpstrTitle = L"Load DebugView Filter";

	// setting m_ofn.lpstrInitialDir works on Windows7, see http://msdn.microsoft.com/en-us/library/ms646839 at lpstrInitialDir 
	std::wstring path;
	TCHAR szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, 
                             CSIDL_PERSONAL, 
                             NULL, 
                             0, 
                             szPath)))
	{
	    path = szPath;
		path += L"\\DebugView++ Filters";
		dlg.m_ofn.lpstrInitialDir = path.c_str();
	}

	if (dlg.DoModal() != IDOK)
		return;

	auto ext = GetFileNameExt(dlg.m_szFileName);
	auto fileName = Str(dlg.m_szFileName).str();
	FilterData data;
	if (boost::iequals(ext, L"json"))
		data = LoadJson(fileName);
	else /* if (boost::iequals(ext, L"xml")) */
		data = LoadXml(fileName);

	SetDlgItemTextA(*this, IDC_NAME, data.name.c_str());
	auto msgFilters = m_messagePage.GetFilters();
	for (auto f = data.filter.messageFilters.begin(); f != data.filter.messageFilters.end(); f++)
	{
		msgFilters.push_back(*f);
	}
	m_messagePage.SetFilters(msgFilters);

	auto procFilters = m_processPage.GetFilters();
	for (auto f = data.filter.processFilters.begin(); f != data.filter.processFilters.end(); f++)
	{
		procFilters.push_back(*f);
	}
	m_processPage.SetFilters(procFilters);
}

void CFilterDlg::OnClearAll(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	m_messagePage.SetFilters(std::vector<MessageFilter>());
	m_processPage.SetFilters(std::vector<ProcessFilter>());
}

void CFilterDlg::OnCancel(UINT /*uNotifyCode*/, int nID, CWindow /*wndCtl*/)
{
	EndDialog(nID);
}

void CFilterDlg::OnOk(UINT /*uNotifyCode*/, int nID, CWindow /*wndCtl*/)
{
	m_name = fusion::GetDlgItemText(*this, IDC_NAME);
	m_filter.messageFilters = m_messagePage.GetFilters();
	m_filter.processFilters = m_processPage.GetFilters();
	EndDialog(nID);
}

} // namespace debugviewpp 
} // namespace fusion
