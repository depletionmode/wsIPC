//
// @depletionmode 2018
//

#include "wsIPC.h"

#include <Psapi.h>

#define PAGE_SIZE 0x1000

#pragma const_seg(".ipcseg")
static const BYTE g_recvReady[PAGE_SIZE] = { 0 };
static const BYTE g_recvDone[PAGE_SIZE] = { 0 };
static const BYTE g_dataReady[PAGE_SIZE] = { 0 };
static const BYTE g_dataDone[PAGE_SIZE] = { 0 };

#define DATA_SIZE sizeof(BYTE) * 8
static const BYTE g_Data[PAGE_SIZE * DATA_SIZE];
#pragma const_seg()
#pragma comment(linker, "/SECTION:.ipcseg,R")

#define GET_DATAPAGE_ADDRESS(x) (g_Data + (x * PAGE_SIZE))

#define READY_POLL_DELAY_MS 50

VOID _markReceiverReady();
VOID _markReceiverDone();
VOID _markDataReady();
VOID _markDataDone();

VOID _clearReceiverReady();
VOID _clearReceiverDone();
VOID _clearDataReady();
VOID _clearDataDone();

VOID _waitOnReceiverReady();
VOID _waitOnReceiverDone();
VOID _waitOnDataReady();
VOID _waitOnPage(_In_ PVOID Address);

BOOL _testDataDone();
BOOL _testPageInWorkingSet(_In_ PVOID Address);

VOID _clearByte();
VOID _encodeByte(_In_ BYTE Value);
VOID _decodeByte(_Out_ BYTE* Value);

HRESULT Send(
    _In_reads_bytes_(BufferSize) PBYTE Buffer,
    _In_ ULONG BufferSize
)
{
    PBYTE currentByte = Buffer;

    _clearDataDone();

    while (currentByte < Buffer + BufferSize) {
        _clearDataReady();
        _waitOnReceiverReady();
        _encodeByte(*currentByte++);
        _markDataReady();
        _waitOnReceiverDone();
    }

    _markDataDone();

    return S_OK;
}

HRESULT Receive(
    _Out_writes_bytes_(*BytesReceived) BYTE* Buffer,
    _In_ SIZE_T BufferSize,
    _Out_ SIZE_T *BytesReceived
)
{
    PBYTE currentByte = Buffer;

    *BytesReceived = 0;
    
    _clearReceiverDone();

    do {
        _markReceiverReady();
        _clearReceiverDone();
        _waitOnDataReady();
        _clearReceiverReady();
        _decodeByte(currentByte++);

        if (currentByte > Buffer + BufferSize) {
            //
            // Overflow passed end of buffer
            //

            return E_ABORT;
        }

        *BytesReceived++;

        _markReceiverDone();

        Sleep(100);    // Hack
    } while (!_testDataDone());

    return S_OK;
}

VOID _markReceiverReady() { volatile BYTE nooneCares = *(BYTE*)g_recvReady; }
VOID _markReceiverDone() { volatile BYTE nooneCares = *(BYTE*)g_recvDone; }
VOID _markDataReady() { volatile BYTE nooneCares = *(BYTE*)g_dataReady; }
VOID _markDataDone() { volatile BYTE nooneCares = *(BYTE*)g_dataDone; }

VOID _clearReceiverReady() { VirtualUnlock((PVOID)g_recvReady, PAGE_SIZE); }
VOID _clearReceiverDone() { VirtualUnlock((PVOID)g_recvDone, PAGE_SIZE); }
VOID _clearDataReady() { VirtualUnlock((PVOID)g_dataReady, PAGE_SIZE); }
VOID _clearDataDone() { VirtualUnlock((PVOID)g_dataDone, PAGE_SIZE); }

VOID _waitOnReceiverReady() { _waitOnPage((PVOID)g_recvReady); }
VOID _waitOnReceiverDone() { _waitOnPage((PVOID)g_recvDone); }
VOID _waitOnDataReady() { _waitOnPage((PVOID)g_dataReady); }

BOOL _testDataDone() { return _testPageInWorkingSet((PVOID)g_dataDone); }

VOID _waitOnPage(_In_ PVOID Address) {
    //
    // Test Address page for waiting data.
    //

    while (!_testPageInWorkingSet(Address)) {
        Sleep(READY_POLL_DELAY_MS);
    }
}

VOID _clearByte() {
    ULONG bit;

    bit = DATA_SIZE;
    while (bit-- > 0) {
        //
        // Clear all data bits by evicting g_Data pages.
        //

        VirtualUnlock((PVOID)GET_DATAPAGE_ADDRESS(bit), PAGE_SIZE);
    }
}

VOID _encodeByte(_In_ BYTE Value) {
    ULONG bit;
    volatile BYTE nooneCares;

    _clearByte();

    bit = DATA_SIZE;
    while (bit-- > 0) {
        if (0 != (Value & (1 << bit))) {
            //
            // Encode bit by making page resident.
            //

            nooneCares = *GET_DATAPAGE_ADDRESS(bit);
        }
    }
}

VOID _decodeByte(_Out_ BYTE* Value) {
    ULONG bit;
    volatile BYTE nooneCares;
    PSAPI_WORKING_SET_EX_INFORMATION pv[DATA_SIZE];

    //
    // Populate working set information structure array and make all data pages 
    // resident.
    //

    bit = DATA_SIZE;
    while (bit-- > 0) {
        pv[bit].VirtualAddress = (PVOID)GET_DATAPAGE_ADDRESS(bit);
        nooneCares = *GET_DATAPAGE_ADDRESS(bit);
    }

    //
    // Query working set metadata.
    //

    QueryWorkingSetEx(GetCurrentProcess(), pv, sizeof(pv));

    //
    // Decode bits from ShareCount
    //

    *Value = 0;

    bit = DATA_SIZE;
    while (bit-- > 0) {
        if (pv[bit].VirtualAttributes.ShareCount > 1) {
            *Value |= 1 << bit;
        }
    }
}

BOOL _testPageInWorkingSet(_In_ PVOID Address) {
    PSAPI_WORKING_SET_EX_INFORMATION pv;

    pv.VirtualAddress = Address;

    //
    // Bring Address resident.
    //

    volatile BYTE nooneCares = *(BYTE*)Address;

    //
    // Query working set metadata for ShareCount.
    //

    BOOL res = QueryWorkingSetEx(GetCurrentProcess(), &pv, sizeof(pv));

    return pv.VirtualAttributes.ShareCount > 1;
}
