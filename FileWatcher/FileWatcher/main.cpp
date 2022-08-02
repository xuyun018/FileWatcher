#include <Windows.h>
#include <stdio.h>
#include "file_watcher.h"

int wmain(int argc, WCHAR* argv[])
{
	struct file_watcher pwatcher[1];

	file_watcher_startup(pwatcher, 4, L"C:\\");

	getchar();

	file_watcher_cleanup(pwatcher);

	return(0);
}