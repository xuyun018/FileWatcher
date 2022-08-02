#include "file_watcher.h"
//---------------------------------------------------------------------------
unsigned int file_watcher_parse(unsigned char* buffer, unsigned int bufferlength,
	const WCHAR *directoryname)
{
	FILE_NOTIFY_INFORMATION* pfni;
	unsigned int l;
	unsigned int result = 0;

	l = directoryname[0];

	pfni = (FILE_NOTIFY_INFORMATION*)buffer;
	while ((unsigned char *)pfni + sizeof(FILE_NOTIFY_INFORMATION) <= buffer + bufferlength)
	{
		WCHAR *filename;

		filename = (WCHAR*)MALLOC((l << 1) + pfni->FileNameLength + 2);
		wcscpy(filename, directoryname + 1);
		wcsncpy(filename + l, pfni->FileName, pfni->FileNameLength >> 1);
		filename[l + (pfni->FileNameLength >> 1)] = '\0';

		wprintf(L"len %d, added.\r\n", pfni->FileNameLength);

		if (0)
		{
			switch (pfni->Action)
			{
			case FILE_ACTION_MODIFIED:
				wprintf(L"%d, %s modified.\r\n", GetCurrentThreadId(), filename);
				break;
			case FILE_ACTION_ADDED:
				wprintf(L"%d, %s added.\r\n", GetCurrentThreadId(), filename);
				break;
			case FILE_ACTION_REMOVED:
				wprintf(L"%d, %s removed.\r\n", GetCurrentThreadId(), filename);
				break;
			case FILE_ACTION_RENAMED_OLD_NAME:
				wprintf(L"%d, %s old named.\r\n", GetCurrentThreadId(), filename);
				break;
			case FILE_ACTION_RENAMED_NEW_NAME:
				wprintf(L"%d, %s new named.\r\n", GetCurrentThreadId(), filename);
				break;
			default:
				break;
			}
		}

		FREE(filename);

		result++;

		if (pfni->NextEntryOffset == 0)
		{
			break;
		}

		pfni = (FILE_NOTIFY_INFORMATION *)((unsigned char *)pfni + pfni->NextEntryOffset);
	}

	return(result);
}

unsigned char *file_watcher_read(PWATCH_OVERLAPPED pwo, unsigned char *buffer, unsigned int *bufferlength, unsigned int buffersize, HANDLE hdirectory)
{
	OVERLAPPED* po;
	DWORD bytes_returned;
	int errorcode;

	*bufferlength = 0;

	memset(&pwo->o, 0, sizeof(OVERLAPPED));
	po = &pwo->o;

	bytes_returned = 0;
	if (ReadDirectoryChangesW(
		hdirectory,                                  // handle to directory
		buffer,                                    // read results buffer
		buffersize,                                // length of buffer
		TRUE,                                 // monitoring option
		FILE_NOTIFY_CHANGE_SECURITY |
		FILE_NOTIFY_CHANGE_CREATION |
		FILE_NOTIFY_CHANGE_LAST_ACCESS |
		FILE_NOTIFY_CHANGE_LAST_WRITE |
		FILE_NOTIFY_CHANGE_SIZE |
		FILE_NOTIFY_CHANGE_ATTRIBUTES |
		FILE_NOTIFY_CHANGE_DIR_NAME |
		FILE_NOTIFY_CHANGE_FILE_NAME,            // filter conditions
		&bytes_returned,              // bytes returned
		po,                          // overlapped buffer
		NULL// completion routine
	))
	{
		// 接收成功

		if (bytes_returned)
		{
			*bufferlength = bytes_returned;
		}
		else
		{
			buffer = NULL;
		}
	}
	else
	{
		errorcode = GetLastError();
		switch (errorcode)
		{
		case ERROR_IO_PENDING:
			buffer = NULL;

			wprintf(L"post ok\r\n");
			break;
		default:
			//_tprintf(_T("WSAGetLastError %d\r\n"), errorcode);
			//MessageBox(NULL, L"", L"", MB_OK);
			wprintf(L"errorcode %d\r\n", errorcode);
			break;
		}
	}

	return(buffer);
}

struct file_watcher_thread_parameter
{
	struct file_watcher* pwatcher;

	HANDLE hevent;
};

DWORD CALLBACK file_watcher_threadproc(LPVOID parameter)
{
	struct file_watcher_thread_parameter* pfwtp = (struct file_watcher_thread_parameter*)parameter;
	struct file_watcher* pwatcher = pfwtp->pwatcher;
	HANDLE hcompletion = pwatcher->hcompletion;
	HANDLE hdirectory = pwatcher->hdirectory;
	OVERLAPPED *po;
	PWATCH_OVERLAPPED pwo;
	ULONG_PTR completionkey;
	DWORD numberofbytes;
	int errorcode;
	BOOL flag;
	unsigned char *buffer;
	unsigned int bufferlength;

	SetEvent(pfwtp->hevent);

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);

	while (pwatcher->working)
	{
		errorcode = 0;

		flag = GetQueuedCompletionStatus(hcompletion, &numberofbytes, &completionkey, &po, INFINITE);
		if (po)
		{
			pwo = (PWATCH_OVERLAPPED)CONTAINING_RECORD(po, WATCH_OVERLAPPED, o);

			if (numberofbytes)
			{
				buffer = (unsigned char *)pwo->buffer;
				bufferlength = numberofbytes;

				do
				{
					file_watcher_parse(buffer, bufferlength, pwatcher->directoryname);

					buffer = file_watcher_read(pwo, buffer, &bufferlength, pwo->buffersize, pwatcher->hdirectory);
				} while (pwatcher->working && buffer && bufferlength);
			}
		}
	}

	return(0);
}

int file_watcher_startup(struct file_watcher *pwatcher, unsigned int count, const WCHAR *directoryname)
{
	struct file_watcher_thread_parameter pfwtp[1];
	unsigned int i;

	pfwtp->hevent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (pfwtp->hevent)
	{
		pwatcher->hthreads = (HANDLE*)MALLOC(sizeof(HANDLE) * count);
		pwatcher->wos = (WATCH_OVERLAPPED*)MALLOC(sizeof(WATCH_OVERLAPPED) * count);
		pwatcher->count = count;

		pwatcher->working = TRUE;

		pwatcher->hcompletion = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

		wcscpy(pwatcher->directoryname + 1, directoryname);
		pwatcher->directoryname[0] = wcslen(directoryname);
		pwatcher->hdirectory= CreateFile(directoryname, // pointer to the file name
			FILE_LIST_DIRECTORY,                // access (read/write) mode
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,  // share mode
			NULL,                               // security descriptor
			OPEN_EXISTING,                      // how to create
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,         // file attributes
			NULL                                // file with attributes to copy
		);

		CreateIoCompletionPort(pwatcher->hdirectory, pwatcher->hcompletion, (ULONG_PTR)pwatcher->hdirectory, count);

		pfwtp->pwatcher = pwatcher;

		unsigned char* buffer;
		unsigned int bufferlength;
		unsigned int buffersize;
		unsigned int j = 0;

		buffersize = 65536;

		for (i = 0; i < count; i++)
		{
			pwatcher->wos[i].buffersize = buffersize;
			pwatcher->wos[i].buffer = (unsigned char*)MALLOC(pwatcher->wos[i].buffersize);
		}

		for (i = 0; i < count; i++)
		{
			pwatcher->hthreads[i] = NULL;

			j++;

			buffer = pwatcher->wos[i].buffer;

			do
			{
				buffer = file_watcher_read(&pwatcher->wos[i], buffer, &bufferlength, buffersize, pwatcher->hdirectory);
				if (buffer && bufferlength)
				{
					file_watcher_parse(buffer, bufferlength, pwatcher->directoryname);
				}
			} while (buffer && bufferlength);

			if (buffer == NULL || bufferlength == 0)
			{
				ResetEvent(pfwtp->hevent);
				pwatcher->hthreads[i] = CreateThread(NULL, 0, file_watcher_threadproc, (LPVOID)pfwtp, 0, NULL);
				if (pwatcher->hthreads[i])
				{
					WaitForSingleObject(pfwtp->hevent, INFINITE);
				}
			}
		}

		CloseHandle(pfwtp->hevent);
	}

	return(0);
}

int file_watcher_cleanup(struct file_watcher *pwatcher)
{
	pwatcher->working = FALSE;

	unsigned int i;

	if (pwatcher->hdirectory != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pwatcher->hdirectory);
	}

	for (i = 0; i < pwatcher->count; i++)
	{
		if (pwatcher->hthreads[i])
		{
			PostQueuedCompletionStatus(pwatcher->hcompletion, 0, (ULONG_PTR)NULL, NULL);
		}
	}

	for (i = 0; i < pwatcher->count; i++)
	{
		if (pwatcher->hthreads[i])
		{
			WaitForSingleObject(pwatcher->hthreads[i], INFINITE);
			CloseHandle(pwatcher->hthreads[i]);
		}
	}

	for (i = 0; i < pwatcher->count; i++)
	{
		pwatcher->wos[i].buffersize = 0;
		FREE(pwatcher->wos[i].buffer);
		pwatcher->wos[i].buffer = NULL;
	}

	CloseHandle(pwatcher->hcompletion);

	FREE(pwatcher->hthreads);
	pwatcher->hthreads = NULL;

	FREE(pwatcher->wos);
	pwatcher->wos = NULL;

	return(0);
}
//---------------------------------------------------------------------------