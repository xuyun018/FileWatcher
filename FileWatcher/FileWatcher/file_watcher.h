#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H
//---------------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>

#include <stdio.h>
//---------------------------------------------------------------------------
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
//---------------------------------------------------------------------------
typedef struct _WATCH_OVERLAPPED
{
	OVERLAPPED o;

	unsigned char* buffer;
	unsigned int buffersize;

}WATCH_OVERLAPPED, *PWATCH_OVERLAPPED;

struct file_watcher
{
	HANDLE hcompletion;

	HANDLE hdirectory;

	HANDLE *hthreads;
	WATCH_OVERLAPPED *wos;

	unsigned int count;

	int working;

	WCHAR directoryname[256];
};
//---------------------------------------------------------------------------
int file_watcher_startup(struct file_watcher* pwatcher, unsigned int count, const WCHAR* directoryname);
int file_watcher_cleanup(struct file_watcher* pwatcher);
//---------------------------------------------------------------------------
#endif