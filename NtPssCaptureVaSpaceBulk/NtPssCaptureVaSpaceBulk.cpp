// NtPssCaptureVaSpaceBulk.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"
#include "stdafx.h"

#define ulonglong unsigned long long
#define ulong unsigned long
#define ushort unsigned short
#define uchar unsigned char

#define STATUS_MORE_ENTRIES 0x105

struct _PSS_CAPTURE_BULK
{
	ulong Flags;
	ulong NumEntries;
	ulonglong LastBaseAddress;
};


typedef ulonglong (*fNtPssCaptureVaSpaceBulk)(
HANDLE hProcess, 
ulonglong BaseAddress, 
_PSS_CAPTURE_BULK* pInfo, 
ulonglong InfoLength, 
ulonglong* pResultLength);



extern "C"
{
	int ZwTerminateProcess(HANDLE hProcess,ulong ExitStatus);
	int ZwClose(HANDLE Handle);
}


fNtPssCaptureVaSpaceBulk NtPssCaptureVaSpaceBulk = 0;


HANDLE CreateDummyProcess(HANDLE* phThread)
{
	PROCESS_INFORMATION PI;
	STARTUPINFO SI = {sizeof(SI)};

	wchar_t Path_All[MAX_PATH+1] = {0};
	GetSystemDirectory(Path_All,MAX_PATH);
	wcscat(Path_All,L"\\charmap.exe");

	if(!CreateProcess(Path_All,0,0,0,FALSE,0,0,0,&SI,&PI))
	{
		printf("Error creating process, err: %X\r\n", GetLastError());
		return (HANDLE)-1;
	}

	if(phThread) *phThread = PI.hThread;
	return PI.hProcess;
}


void PrintMBI(MEMORY_BASIC_INFORMATION* pMBI)
{
	printf("AllocationBase: %I64X\r\n",pMBI->AllocationBase);
	printf("RegionSize: %I64X\r\n",pMBI->RegionSize);
	printf("AllocationProtect: %I64X\r\n",pMBI->AllocationProtect);
	printf("BaseAddress: %I64X\r\n",pMBI->BaseAddress);
	printf("Protect: %I64X\r\n",pMBI->Protect);

	printf("Type: %I64X\r\n",pMBI->Type);
	printf("State: %I64X\r\n",pMBI->State);
}


int _tmain(int argc, _TCHAR* argv[])
{
	HMODULE hM = GetModuleHandle(L"ntdll.dll");


	NtPssCaptureVaSpaceBulk = (fNtPssCaptureVaSpaceBulk)GetProcAddress(hM,"NtPssCaptureVaSpaceBulk");
	printf("NtPssCaptureVaSpaceBulk at: %I64X\r\n",NtPssCaptureVaSpaceBulk);

	if(!NtPssCaptureVaSpaceBulk)
	{
		printf("OS not supported\r\n");
		ExitProcess(0);
	}

	HANDLE hThread = 0;
	HANDLE hProcess = CreateDummyProcess(&hThread);
	if(hProcess)
	{
		ulonglong BaseAddress = 0;//begin at null page
		ulonglong ResultLength = 0;
		int ret = 0;

		ulonglong sz = 0x1000;
		_PSS_CAPTURE_BULK* pBulk = (_PSS_CAPTURE_BULK*)VirtualAlloc(0,sz,MEM_COMMIT,PAGE_READWRITE);
		if(pBulk)
		{
			pBulk->Flags = 1;//or 2 or 3

			while(	(ret = NtPssCaptureVaSpaceBulk(hProcess,BaseAddress,pBulk,sz,&ResultLength) ) == STATUS_MORE_ENTRIES)
			{
				printf("NtPssCaptureVaSpaceBulk, ret: %X\r\n",ret);

				VirtualFree(pBulk,0,MEM_RELEASE);

				sz += sz;
				pBulk = (_PSS_CAPTURE_BULK*)VirtualAlloc(0,sz,MEM_COMMIT,PAGE_READWRITE);
				if(!pBulk)
				{
					printf("Insufficient memory\r\n");
					ZwTerminateProcess(hProcess,0);
					ZwClose(hProcess);
					return -1;
				}

				pBulk->Flags = 1;//or 2 or 3
			}

			

			if(ret >= 0)
			{
				_MEMORY_BASIC_INFORMATION* pMBI = (_MEMORY_BASIC_INFORMATION*)
					(((uchar*)pBulk)+sizeof(_PSS_CAPTURE_BULK));
				
				printf("Number Of Entries: %X\r\n",pBulk->NumEntries);
				printf("Last Address: %I64X\r\n",pBulk->LastBaseAddress);

				for(ulong i=0;i<pBulk->NumEntries;i++)
				{
					PrintMBI(&pMBI[i]);
					printf("----------\r\n");
				}
			}

			VirtualFree(pBulk,0,MEM_RELEASE);
		}
		ZwTerminateProcess(hProcess,0);
		ZwClose(hProcess);
	}

	
	return 0;
}

