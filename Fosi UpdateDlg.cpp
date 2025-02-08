// Fosi UpdateDlg.cpp: 实现文件

#include "pch.h"
#include "framework.h"
#include "Fosi Update.h"
#include "Fosi UpdateDlg.h"
#include "afxdialogex.h"

#include "hidapi.h"
#include "fstream"
#include <vector>

#include "afx.h"
#include <atomic>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框
class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// CFosiUpdateDlg 对话框
CFosiUpdateDlg::CFosiUpdateDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_FOSI_UPDATE_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CFosiUpdateDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CFosiUpdateDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON1, &CFosiUpdateDlg::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON2, &CFosiUpdateDlg::OnBnClickedButton2)
END_MESSAGE_MAP()

BOOL CFosiUpdateDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr) {
        CString strAboutMenu;
        strAboutMenu.LoadString(IDS_ABOUTBOX);
        if (!strAboutMenu.IsEmpty()) {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);
    //CString fileInfo;
    //fileInfo.Format(_T("E:\\fosi_stuff\\ZH3_v_0_1\\proj_by_RTOS\\ZH3_V0_1.bin"));
    //SetDlgItemText(IDC_EDIT1, fileInfo);
    return TRUE;
}

void CFosiUpdateDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

void CFosiUpdateDlg::OnPaint()
{
    if (IsIconic()) {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;
        dc.DrawIcon(x, y, m_hIcon);
    }
    else {
        CDialogEx::OnPaint();
    }
}

HCURSOR CFosiUpdateDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

unsigned short CalculateCRC16(const unsigned char* data, size_t length)
{
    uint16_t crc = 0x0000;
    uint16_t poly = 0x1021;

    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= (data[i] << 8);
        for (uint16_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}
unsigned short CalculateFileCRC(CString filePath, LPVOID pParam)
{
    CFosiUpdateDlg* dlg = reinterpret_cast<CFosiUpdateDlg*>(pParam);
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        dlg->ShowErrorMessage(_T("Failed to open file!"));
        return 0;
    }

    // 获取文件大小
    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // 读取文件内容
    unsigned char* buffer = new unsigned char[fileSize];
    file.read(reinterpret_cast<char*>(buffer), fileSize);

    // 计算CRC
    unsigned short crc = CalculateCRC16(buffer, fileSize);

    delete[] buffer;
    file.close();

    return crc;
}

std::atomic<CFosiUpdateDlg::CFosiUpdateDlg::updatepack_st_t> packupdate_st(CFosiUpdateDlg::updatepack_st_t::UPDATE_ST_WAIT);
UINT CFosiUpdateDlg::SendHIDDataThread(LPVOID pParam)
{
    CFosiUpdateDlg* dlg = reinterpret_cast<CFosiUpdateDlg*>(pParam);

    // Get the file path from the dialog
    CString filePath;
    dlg->GetDlgItemText(IDC_EDIT1, filePath);

    // Open the file in binary mode
    std::ifstream file(filePath.GetString(), std::ios::binary);

    if (!file.is_open()) {
        dlg->ShowErrorMessage(_T("Failed to open file!"));
        return 1;
    }

    // Get the file size and calculate the number of packets needed
    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    size_t totalPackets = (fileSize + 29) / 30;
    dlg->SetDlgItemText(IDC_STATIC, _T("Updating...\n"));



    unsigned char data[51] = { 0 };  // Buffer for sending data
    size_t packetIndex = 0;
    const int maxRetries = 150;        // Maximum retries for each packet
    int retryAllCount = 0;            // Total retry count




    // Read the file and send data in 30-byte packets
    while (file.read(reinterpret_cast<char*>(data + 6), 30) || file.gcount() > 0) {
        size_t bytesRead = static_cast<size_t>(file.gcount());
        UCHAR update_datalength = 30;

        // Pad the data if less than 30 bytes
        if (bytesRead < 30) {
            memset(data + 6 + bytesRead, 0, 30 - bytesRead);
            update_datalength = bytesRead;
        }

        // Set up the packet with header, data length, and CRC
        data[0] = 0x01;
        data[1] = 0xAA;
        data[2] = 0x55;
        data[3] = 0x01;
        data[4] = update_datalength;
        data[5] = 0x23;
        unsigned short crc = CalculateCRC16(data + 1, 38);
        data[39] = static_cast<unsigned char>((crc >> 8) & 0xFF);
        data[40] = static_cast<unsigned char>(crc & 0xFF);

        // Retry mechanism for sending the packet
        int res = -1;
        int retryCount = 0;
        while (res < 0 && retryCount < maxRetries) {
            // Open the HID device
            hid_device* handle = hid_open(0x152a, 0x889b, nullptr);
            if (!handle) {
                dlg->ShowErrorMessage(_T("Failed to open HID device!"));
                return 1;
            }
            res = hid_write(handle, data, sizeof(data));
            Sleep(10);
            retryCount++;
            if (res < 0) {
                retryAllCount++;
            }
            int dly_max_tmp = 10,idxtemp= packetIndex;
            for (int i = 0;i < dly_max_tmp;i++)
            {
                switch (packupdate_st)
                {
                case UPDATE_ST_WAIT:
                    Sleep(10);
                    break;
                case UPDATE_ST_RETRY:
                    res = -1;
                    i = dly_max_tmp;
                    break;
                case UPDATE_ST_RESTART:
                    packetIndex = 0;
                    i = dly_max_tmp;
                    break;
                case UPDATE_ST_SUCCESS:
                {
                    packetIndex++;
                    i = dly_max_tmp;
                    packupdate_st = UPDATE_ST_WAIT;
                }
                break;
                default:
                    break;
                }
                if (idxtemp == packetIndex)
                {
                    res = -1;
                }
            }
            hid_close(handle);
            hid_exit();
        }

        // If the packet was successfully sent, update the UI
        if (res >= 0) {
            CString updateMsg;
            updateMsg.Format(_T("Updating %d / %zu\n"), packetIndex, totalPackets);
            dlg->SetDlgItemText(IDC_STATIC, updateMsg);

            // Wait for a response from the device (simulated here with Sleep)

           // if()
        }
        else {
            CString errorMsg;
            errorMsg.Format(_T("Failed to send packet %d after %d retries"), packetIndex, retryCount);
            dlg->ShowErrorMessage(errorMsg);
            break;
        }

        // Stop if all packets have been sent
        if (packetIndex >= totalPackets) {
            // Show completion message
            CString completionMsg;
            completionMsg.Format(_T("Update completed. Total retries: %d"), retryAllCount);
            dlg->SetDlgItemText(IDC_STATIC, completionMsg);
            break;
        }
    }

    // Close the file and HID device
    file.close();




    return 0;
}
UINT CFosiUpdateDlg::ReadHIDDataThread(LPVOID pParam)
{
    CFosiUpdateDlg* dlg = reinterpret_cast<CFosiUpdateDlg*>(pParam);

    if (hid_init()) {
        dlg->ShowErrorMessage(_T("HID initialization failed!"));
        return 1;
    }



    unsigned char buffer[64] = { 0 };
    buffer[0] = 0x01;
    while (true) {
        hid_device* handle = hid_open(0x152a, 0x889b, nullptr);
        if (!handle) {
            dlg->ShowErrorMessage(_T("Failed to open HID device!"));
            return 1;
        }
        int bytesRead = hid_read(handle, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            CString output;
            uint8_t recv_buf[8] = { 0 };
            for (int i = 0;i < 8;i++)
            {
                recv_buf[i] = buffer[i + 1];
            }
            uint16_t recv_crc = (recv_buf[6] << 8) | recv_buf[7];
            uint16_t cal_crc = CalculateCRC16(recv_buf, 6);
            if (cal_crc == recv_crc && recv_buf[0] == 0xAA)
            {
                switch (recv_buf[3])
                {
                case 0:
                    packupdate_st = UPDATE_ST_RETRY;
                    //AfxMessageBox(_T("RETRY"));
                    break;
                case 1:
                    packupdate_st = UPDATE_ST_SUCCESS;
                    //AfxMessageBox(_T("SUCCESS"));
                    break;
                case 2:
                    packupdate_st = UPDATE_ST_RESTART;
                    // AfxMessageBox(_T("RESTART"));
                    break;
                default:
                    packupdate_st = UPDATE_ST_WAIT;
                    break;
                }
            }
            else if (cal_crc == recv_crc && recv_buf[0] == 0x00)
            {
                //packupdate_st = UPDATE_ST_SUCCESS;
                //AfxMessageBox(_T("CRC ERROR,RETRY"));
                //packupdate_st = UPDATE_ST_WAIT;
            }
            else
            {
                // AfxMessageBox(_T("CRC ERROR,RETRY"));
                packupdate_st = UPDATE_ST_RETRY;
            }
            hid_close(handle);
            hid_exit();
        }
        else if (bytesRead < 0) {
            dlg->ShowErrorMessage(_T("Failed to read data from USB device!"));
            break;
        }
        //Sleep(10);
    }



    return 0;
}
UINT CFosiUpdateDlg::HIDCommunicationThread(LPVOID pParam)
{
    CFosiUpdateDlg* dlg = reinterpret_cast<CFosiUpdateDlg*>(pParam);

    if (hid_init()) {
        dlg->ShowErrorMessage(_T("HID initialization failed!"));
        return 1;
    }

    CString filePath;
    dlg->GetDlgItemText(IDC_EDIT1, filePath);
    uint16_t filecrc = CalculateFileCRC(filePath, pParam);
    std::ifstream file(filePath.GetString(), std::ios::binary);
    if (!file.is_open()) {
        dlg->ShowErrorMessage(_T("Failed to open file!"));
        return 1;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    size_t totalPackets = (fileSize + 29) / 30;

    CString fileInfo;
    fileInfo.Format(_T("Start:\nFile Path: %s\nFile Size: %zu bytes\nTotal Packets: %zu\n"), filePath, fileSize, totalPackets);
    dlg->SetDlgItemText(IDC_STATIC, fileInfo);
    CString idc_content;
    dlg->GetDlgItemText(IDC_STATIC, idc_content);

    size_t packetIndex = 0;
    size_t packetLastIndex = -1;
    unsigned char data[41] = { 0 };

    hid_device* handle = hid_open(0x152a, 0x889b, nullptr);
    if (!handle) {
        dlg->ShowErrorMessage(_T("Failed to open HID device!"));
        return 1;
    }
    size_t packetStartIdx = 9;
    size_t file_send_cnt = 0;
    file.rdbuf()->pubsetbuf(nullptr, 0);
   
    //when packet idx is 0,send file info
    while (packetIndex <= totalPackets ) {
        
        
        if (packetLastIndex != packetIndex)
        {
            if (packetIndex)
            {
                memset(data, 0xff, sizeof(data));
                file.read(reinterpret_cast<char*>(data + packetStartIdx), 30);
                
            }
                
            else
            {
                data[8] = fileSize >> 16;
                data[9] = fileSize >>8;
                data[10] = fileSize;
                data[11] = filecrc >> 8;
                data[12] = filecrc;
                file_send_cnt = 0;
            }
            packetLastIndex = packetIndex;
        }
           
        size_t bytesRead  = static_cast<size_t>(file.gcount());

      /*  if (bytesRead < 30 && packetIndex && packetLastIndex != packetIndex) {
            memset(data + packetStartIdx + bytesRead, 0, 30 - bytesRead);
        }*/
        if (packetIndex == 0)
        {
            int i = 0;
        }
        /*size_t bytesRead = 30;
        if (packetIndex == totalPackets-1) {
            bytesRead = fileSize - packetIndex * 30;
        }*/
        data[0] = 0x01;
        data[1] = 0xAA;
        data[2] = 0x55;
        data[3] = 0x01;
        data[4] = static_cast<unsigned char>(bytesRead);
        data[5] = 0x23;
        data[6] = packetIndex >> 8;
        data[7] = packetIndex;
        unsigned short crc = CalculateCRC16(data + 1, 38);
        data[39] = static_cast<unsigned char>((crc >> 8) & 0xFF);
        data[40] = static_cast<unsigned char>(crc & 0xFF);
       
        int res = hid_write(handle, data, sizeof(data));
        if (res < 0) {
            dlg->ShowErrorMessage(_T("Failed to send data to USB device!"));
            break;
        }

        // 等待设备响应
        unsigned char buffer[64] = { 0 };
        res = hid_read_timeout(handle, buffer, sizeof(buffer), 500); // 500ms 超时
        if (res > 0) {
            uint8_t recvBuf[8] = { 0 };
            for (int i = 0; i < 8; i++) {
                recvBuf[i] = buffer[i + 1];
            }

            uint16_t recvCrc = (recvBuf[6] << 8) | recvBuf[7];
            uint16_t calCrc = CalculateCRC16(recvBuf, 6);
            if (calCrc == recvCrc && recvBuf[0] == 0xAA) {
                switch (recvBuf[3]) {
                case 0:
                    dlg->SetDlgItemText(IDC_STATIC, _T("Retrying current packet..."));
                    break;
                case 1:
                {
                    if (packetIndex)
                        file_send_cnt += bytesRead;
                    packetIndex++;
                    CString updateMsg;
                    updateMsg.Format(idc_content + _T("Packet sent successfully.Updating %d of %d\n"), packetIndex, totalPackets);
                    dlg->SetDlgItemText(IDC_STATIC, updateMsg);
                    
                }
                    break;
                case 2:
                    dlg->SetDlgItemText(IDC_STATIC, _T("Restarting update..."));
                    packetIndex = 0;
                    file.clear();
                    file.seekg(0, std::ios::beg);
                    break;
                default:
                    dlg->ShowErrorMessage(_T("Unexpected response from device."));
                    break;
                }
            }
            else if (calCrc == recvCrc && recvBuf[0] == 0x00)
            {
                //packupdate_st = UPDATE_ST_SUCCESS;
                //AfxMessageBox(_T("CRC ERROR,RETRY"));
                //packupdate_st = UPDATE_ST_WAIT;
            }
            else
            {
                // AfxMessageBox(_T("CRC ERROR,RETRY"));
                packupdate_st = UPDATE_ST_RETRY;
            }
        }
        else if (res < 0) {
            dlg->ShowErrorMessage(_T("Failed to read response from USB device!"));
            break;
        }
    }

    file.close();
    hid_close(handle);
    hid_exit();

    dlg->SetDlgItemText(IDC_STATIC, idc_content + _T("Update process finished."));
    return 0;
}

void CFosiUpdateDlg::OnBnClickedButton1()
{
    AfxBeginThread(HIDCommunicationThread, this);
   /* AfxBeginThread(SendHIDDataThread, this);
    AfxBeginThread(ReadHIDDataThread, this);*/
}

void CFosiUpdateDlg::OnBnClickedButton2()
{
    CFileDialog fileDlg(TRUE, _T("bin"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("BIN FILE (*.bin)|*.bin|ALL FILE (*.*)|*.*||"), this);

    if (fileDlg.DoModal() == IDOK) {
        CString filePath = fileDlg.GetPathName();
        SetDlgItemText(IDC_EDIT1, filePath);
    }

    CString filePath;
    GetDlgItemText(IDC_EDIT1, filePath);

    std::ifstream file(filePath.GetString(), std::ios::binary);
    if (!file.is_open()) {
        AfxMessageBox(_T("Failed to open file!"));
        return;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    CString fileInfo;
    fileInfo.Format(_T("File Information:\nFile Path: %s\nFile Size: %zu bytes\nPack amount: %d\n"), filePath, fileSize, (fileSize + 29) / 30);
    SetDlgItemText(IDC_STATIC, fileInfo);
    file.close();



}

void CFosiUpdateDlg::ShowErrorMessage(const CString& message)
{
    AfxMessageBox(message);
}

void CFosiUpdateDlg::UpdateOutput(const CString& message)
{
    SetDlgItemText(IDC_STATIC, message);
}



