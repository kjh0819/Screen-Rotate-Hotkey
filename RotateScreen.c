#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

// 이전과 동일한 관리자 권한 관련 함수들...
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

// 화면 회전 함수 (수정됨)
void RotateScreen(DWORD orientation) {
    char msg[256];
    DEVMODE devmode;
    memset(&devmode, 0, sizeof(devmode));
    devmode.dmSize = sizeof(devmode);

    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode) != 0) {
        // 너비와 높이를 교환
        if ((devmode.dmDisplayOrientation + orientation) % 2 != 0) { // 가로 <-> 세로 전환 시
            DWORD temp = devmode.dmPelsWidth;
            devmode.dmPelsWidth = devmode.dmPelsHeight;
            devmode.dmPelsHeight = temp;
        }

        devmode.dmDisplayOrientation = orientation;
        devmode.dmFields = DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT;

        // CDS_UPDATEREGISTRY 대신 CDS_TEST로 먼저 테스트
        LONG result = ChangeDisplaySettings(&devmode, CDS_TEST);
        if (result == DISP_CHANGE_SUCCESSFUL) {
            // 테스트 성공 시 실제 적용
            result = ChangeDisplaySettings(&devmode, 0);
            sprintf(msg, "Rotation successful! Result code: %ld", result);
            MessageBox(NULL, msg, "Rotation Result", MB_OK);
        } else {
            sprintf(msg, "Rotation failed test. Result code: %ld\n(-2 = Bad Mode)", result);
            MessageBox(NULL, msg, "Rotation Result", MB_OK);
        }

    } else {
        MessageBox(NULL, "Failed to get current display settings.", "Error", MB_ICONERROR);
    }
}

// 키보드 후크 프로시저 (디버그 메시지 유지)
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

// 메인 함수 (디버그 메시지 제거)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!IsRunningAsAdmin()) {
        ElevateProcess();
        return 0;
    }

    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (hHook == NULL) {
        MessageBox(NULL, "Failed to set keyboard hook!", "Fatal Error", MB_ICONERROR | MB_OK);
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
