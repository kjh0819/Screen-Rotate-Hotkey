#include <windows.h>
#include <shellapi.h>

// 관리자 권한 확인 및 상승 관련 함수들
BOOL IsRunningAsAdmin() {
    BOOL fIsAdmin = FALSE;
    PSID pAdminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminGroup)) {
        if (!CheckTokenMembership(NULL, pAdminGroup, &fIsAdmin)) {
            fIsAdmin = FALSE;
        }
        FreeSid(pAdminGroup);
    }
    return fIsAdmin;
}

BOOL ElevateProcess() {
    WCHAR szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        if (!ShellExecuteExW(&sei)) {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_CANCELLED) {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

HHOOK hHook = NULL;

// 화면 회전 함수 (팝업 제거됨)
void RotateScreen(DWORD orientation) {
    DEVMODE devmode;
    memset(&devmode, 0, sizeof(devmode));
    devmode.dmSize = sizeof(devmode);

    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode) != 0) {
        // 현재 방향과 목표 방향이 다를 때만 너비/높이 교환
        if ((devmode.dmDisplayOrientation % 2) != (orientation % 2)) {
            DWORD temp = devmode.dmPelsWidth;
            devmode.dmPelsWidth = devmode.dmPelsHeight;
            devmode.dmPelsHeight = temp;
        }

        devmode.dmDisplayOrientation = orientation;
        devmode.dmFields = DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT;

        // 테스트 후 성공 시에만 실제 적용
        if (ChangeDisplaySettings(&devmode, CDS_TEST) == DISP_CHANGE_SUCCESSFUL) {
            ChangeDisplaySettings(&devmode, 0);
        }
    }
}

// 키보드 후크 프로시저
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;
        BOOL isCtrlPressed = GetAsyncKeyState(VK_CONTROL) & 0x8000;
        BOOL isAltPressed = GetAsyncKeyState(VK_MENU) & 0x8000;

        if (isCtrlPressed && isAltPressed) {
            switch (pkbhs->vkCode) {
                case VK_UP:    RotateScreen(DMDO_DEFAULT); return 1;
                case VK_DOWN:  RotateScreen(DMDO_180);    return 1;
                case VK_LEFT:  RotateScreen(DMDO_270);   return 1;
                case VK_RIGHT: RotateScreen(DMDO_90);     return 1;
            }
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

// 메인 함수
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!IsRunningAsAdmin()) {
        ElevateProcess();
        return 0;
    }

    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (hHook == NULL) {
        // 치명적 오류 발생 시에만 메시지 표시
        MessageBox(NULL, "키보드 후크 설치에 실패했습니다!\n다른 키보드 후킹 프로그램과 충돌할 수 있습니다.", "Fatal Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hHook);
    return 0;
}