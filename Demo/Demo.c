//
// @depletionmode 2018
//

#include <Windows.h>
#include <stdio.h>

char vanity[] =
"             _______  _____ \n"
" _    _____ /  _/ _ \\/ ___/\n"
"| |/|/ (_-<_/ // ___/ /__   \n"
"|__,__/___/___/_/   \\___/  \n"
" POC by @depletionmode      \n";

int usage(char *process_name)
{
    fprintf(stderr, "usage(): %s <recv/send>\n", process_name);

    exit(-1);
}

int g_sender;

typedef HRESULT(*IpcSend)(PBYTE, ULONG);
typedef HRESULT(*IpcReceive)(BYTE*, SIZE_T, SIZE_T*);

int main(int ac, char *av[])
{
    printf(vanity);

    if (ac < 2) {
        usage(av[0]);
    }

    if (strcmp(av[1], "recv") == 0) {
        g_sender = 0;
    }
    else if (strcmp(av[1], "send") == 0) {
        g_sender = 1;
    }
    else {
        usage(av[0]);
    }

    HMODULE lib = LoadLibraryA("wsIPC.dll");
    if (!lib) {
        fprintf(stderr, "[!] Failed to load library\n");
        return -1;
    }

    printf("[+] wsIpc library loaded successfully @ 0x%p.\n", lib);

    IpcSend pIpcSend = (IpcSend)GetProcAddress(lib, "Send");
    IpcReceive pIpcReceive = (IpcReceive)GetProcAddress(lib, "Receive");

    if (g_sender) {
        CHAR msg[] = "ArthurMorgan";

        printf("[-] Attempting to send message (%s[%d])...\n", msg, sizeof(msg));

        if (SUCCEEDED(pIpcSend((PBYTE)msg, sizeof(msg)))) {
            printf("[+] ...successfully sent!\n");
        }
        else {
            fprintf(stderr, "[!] failed!\n");
            return -1;
        }

    }
    else {
        CHAR msg[0x80] = { 0 };
        ULONG received;

        printf("[-] Attempting to read message...\n");

        if (SUCCEEDED(pIpcReceive(msg, sizeof(msg), &received))) {
            printf("[+] ...successfully received! -> %s\n", msg);
        }
        else {
            fprintf(stderr, "[!] failed!\n");
            return -1;
        }
    }

    Sleep(2000);

    CloseHandle(lib);

    return 0;
}
