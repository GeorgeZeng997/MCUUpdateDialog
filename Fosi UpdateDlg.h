
// Fosi UpdateDlg.h: 头文件
//

#pragma once


// CFosiUpdateDlg 对话框
class CFosiUpdateDlg : public CDialogEx
{
// 构造
public:
	CFosiUpdateDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FOSI_UPDATE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg
	static	UINT SendHIDDataThread(LPVOID pParam);
	void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	static UINT ReadHIDDataThread(LPVOID pParam);
	static UINT HIDCommunicationThread(LPVOID pParam);
	void ShowErrorMessage(const CString& message);
	void UpdateOutput(const CString& message);
	enum updatepack_st_t
	{
		UPDATE_ST_WAIT,
		UPDATE_ST_RETRY,
		UPDATE_ST_RESTART,
		UPDATE_ST_SUCCESS,
	};
};
