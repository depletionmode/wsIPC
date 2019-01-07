#pragma once
#include <Windows.h>

__declspec(dllexport)
HRESULT Send(
	_In_reads_bytes_(BufferSize) PBYTE Buffer,
	_In_ ULONG BufferSize
);

__declspec(dllexport)
HRESULT Receive(
	_Out_writes_bytes_(*BytesReceived) BYTE* Buffer,
	_In_ SIZE_T BufferSize,
	_Out_ SIZE_T *BytesReceived
);
