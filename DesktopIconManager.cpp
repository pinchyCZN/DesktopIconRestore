#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include "DesktopIconManager.h"

CDesktopIconManager::CDesktopIconManager()
: m_hProcess(NULL)
, m_hwnd(NULL)
, m_lpMem(NULL)
{
    // Find the icon container window
    m_hwnd = GetTopWindow(GetTopWindow(FindWindow(_T("Progman"), NULL)));
    if (m_hwnd == NULL) return;

    // Get shell process id
    DWORD dwPid;
    GetWindowThreadProcessId(m_hwnd, &dwPid);

    // Open shell process
    m_hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, dwPid);
    if (m_hProcess == NULL) {
        m_hwnd = NULL;
        return;
    }

    // Allocate one page in shell's address space
    m_lpMem = VirtualAllocEx(m_hProcess, NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
    if (m_lpMem == NULL) {
        CloseHandle(m_hProcess);
        m_hProcess = NULL;
        m_hwnd = NULL;
    }
}
int CDesktopIconManager::GetDesktopSize(LPRECT rect)
{
	int result=FALSE;
	if(m_hwnd){
		result=GetWindowRect(m_hwnd,rect);
	}
	return result;
}

CDesktopIconManager::~CDesktopIconManager()
{
    // Deallocate memory
    if (m_lpMem) {
        VirtualFreeEx(m_hProcess, m_lpMem, 0, MEM_RELEASE);
    }

    // Close process handle
    if (m_hProcess) {
        CloseHandle(m_hProcess);
    }
}

int CDesktopIconManager::GetNumIcons(void) const
{
    if (m_hwnd) {
        return(static_cast<int>(SendMessage(m_hwnd, LVM_GETITEMCOUNT, 0, 0)));
    }
    return(0);
}

typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
typedef struct LVITEMA64_s
{
    uint32_t mask;
    int32_t iItem;
    int32_t iSubItem;
    uint32_t state;
    uint32_t stateMask;
    uint64_t pszText;
    int32_t cchTextMax;
    int32_t iImage;
    uint64_t lParam;
    int32_t iIndent;
#if (NTDDI_VERSION >= NTDDI_WINXP)
    int32_t iGroupId;
    uint32_t cColumns; // tile view columns
    uint64_t puColumns;
#endif
#if (NTDDI_VERSION >= NTDDI_VISTA)
    uint64_t piColFmt;
    uint32_t iGroup; // readonly. only valid for owner data.
#endif
} LVITEMA64;
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process=NULL;

BOOL Is64BitWindows()
{
#if defined(_WIN64)
    return TRUE;  // 64-bit programs run only on Win64
#else
    BOOL f64 = FALSE;
	if(NULL==fnIsWow64Process)
		fnIsWow64Process=(LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
	if(NULL!=fnIsWow64Process){
        fnIsWow64Process(GetCurrentProcess(),&f64);
    }
    return f64;
#endif
}

bool CDesktopIconManager::GetIconText(int nIndex, LPTSTR pszText, int cchText) const
{
    if (m_hwnd && m_hProcess && m_lpMem) {
        const int max_size=4096-sizeof(LVITEMA64);
        if (cchText < 0 || cchText > (int)((max_size / sizeof(TCHAR))))
            cchText = max_size / sizeof(TCHAR);

		int size_item;
		void *pitem=0;
		LPTSTR item_text=0;
		if(Is64BitWindows()){
			LVITEMA64 item;
			size_item=sizeof(LVITEMA64);
			pitem=&item;
			ZeroMemory(&item,size_item);
			item.iSubItem = 0;
			item.cchTextMax = cchText;
			item.mask = LVIF_TEXT;
			item_text=(LPTSTR)m_lpMem + size_item;
			item.pszText = uint64_t(item_text);

		}else{
			LVITEM item;
			size_item=sizeof(LVITEM);
			pitem=&item;
			ZeroMemory(&item,size_item);
			item.iSubItem = 0;
			item.cchTextMax = cchText;
			item_text=(LPTSTR)m_lpMem + size_item;
			item.pszText = (LPWSTR)item_text;
		}

        // Copy it to shell process's memory
        DWORD dwNumWritten;
        WriteProcessMemory(m_hProcess, m_lpMem, pitem, sizeof(LVITEM), &dwNumWritten);
        if (dwNumWritten != sizeof(LVITEM)) return(false);

        // Get item item_text
        SendMessage(m_hwnd, LVM_GETITEMTEXT, nIndex, reinterpret_cast<LPARAM>(m_lpMem));

        // Read the process's memory into our buffer
        DWORD dwNumRead;
        ReadProcessMemory(m_hProcess, item_text, pszText, cchText * sizeof(TCHAR), &dwNumRead);
        if (dwNumRead != cchText * sizeof(TCHAR)) return(false);

        return(true);
    }
    return(false);
}

bool CDesktopIconManager::GetIconPosition(int nIndex, LPPOINT ppt) const
{
    if (m_hwnd && m_hProcess && m_lpMem) {
        // Get item position
        SendMessage(m_hwnd, LVM_GETITEMPOSITION, nIndex, reinterpret_cast<LPARAM>(m_lpMem));

        // Read the process's memory into our buffer
        DWORD dwNumRead;
        ReadProcessMemory(m_hProcess, m_lpMem, ppt, sizeof(POINT), &dwNumRead);
        if (dwNumRead != sizeof(POINT)) return(false);

        return(true);
    }
    return(false);
}

bool CDesktopIconManager::GetIconPosition(LPTSTR name, LPPOINT ppt) const
{
	bool result=FALSE;
    if (m_hwnd && m_hProcess && m_lpMem) {
        int i,count;
		int found=FALSE;
		count=GetNumIcons();
		for(i=0;i<count;i++){
			WCHAR tmp[128]={0};
			if(GetIconText(i,tmp,_countof(tmp))){
				if(0==wcsncmp(tmp,name,_countof(tmp))){
					found=TRUE;
					break;
				}
			}
		}
		if(found){
			if(SendMessage(m_hwnd, LVM_GETITEMPOSITION, i, reinterpret_cast<LPARAM>(m_lpMem))){
				DWORD dwNumRead;
				ReadProcessMemory(m_hProcess, m_lpMem, ppt, sizeof(POINT), &dwNumRead);
				if (dwNumRead == sizeof(POINT))
					result=TRUE;
			}
		}
    }
    return result;
}

bool CDesktopIconManager::SetIconPosition(int nIndex, LPPOINT ppt)
{
    if (m_hwnd && m_hProcess && m_lpMem) {
        // Write position to process's memory
        DWORD dwNumWritten;
        WriteProcessMemory(m_hProcess, m_lpMem, ppt, sizeof(POINT), &dwNumWritten);
        if (dwNumWritten != sizeof(POINT)) return(false);

        // Set item position
        SendMessage(m_hwnd, LVM_SETITEMPOSITION32, nIndex, reinterpret_cast<LPARAM>(m_lpMem));

        return(true);
    }
    return(false);
}

int CDesktopIconManager::RefreshDesktop()
{
	return PostMessage(m_hwnd,WM_KEYDOWN,VK_F5,0);
}