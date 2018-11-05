// tividconvert.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <string.h>
#include <Windows.h>

char szFilename[MAX_PATH];
char szTmp[MAX_PATH];
char szBuf[1024*1024];		// 1MB should be plenty big
FILE *fp;
const int INVALID_RETURN_CODE = 0xffff;
const int EXECUTE_FAILED = 0xfffe;
const int INVALID_EXIT_CODE = 0xfffd;
char *szTempPath = "temp";

// Runs pCmdLine as child process & redirect its stdIn and stdOut.
// Inputs:  Fullpath of command line
// Outputs: Returns the exit code of the called process
//          INVALID_RETURN_CODE if the code could not be retrieved,
//          EXECUTE_FAILED if anything else went wrong
int doExecuteCommand(char *pCmdLine) {
	SECURITY_ATTRIBUTES sa;
	HANDLE hChildStdoutRdTmp, hChildStdoutWr, hChildStdoutRd;
	HANDLE hChildStderrWr;
	HANDLE hChildStdinRd, hChildStdinWrTmp, hChildStdinWr;
	BOOL bSuccess;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD dwRead;

	printf("EXECUTE: %s\n", pCmdLine);

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	// Create a pipe for Stdout
	if (!CreatePipe(&hChildStdoutRdTmp, &hChildStdoutWr, &sa, 0)) {
		printf("Could not create pipe for Stdout redirection\n");
		return EXECUTE_FAILED;
	}

	// Create a separate handle for Stderr
	bSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutWr, GetCurrentProcess(),
		&hChildStderrWr, 0, TRUE, DUPLICATE_SAME_ACCESS);
	if (!bSuccess) {
		printf("Unable to duplicate handle for Stderr\n");
		CloseHandle(hChildStdoutRdTmp);
		CloseHandle(hChildStdoutWr);
		return EXECUTE_FAILED;
	}

	// Create non-inheritable copies of the stdout read and stdin write (below)
	// This is needed to ensure that the handles are closable
	bSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRdTmp, GetCurrentProcess(),
		&hChildStdoutRd, 0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!bSuccess) {
		printf("Unable to duplicate handle for Stdout redirection\n");
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdoutWr);
		return EXECUTE_FAILED;
	}
	CloseHandle(hChildStdoutRdTmp);

	// Do the same for Stdin
	if (!CreatePipe(&hChildStdinRd, &hChildStdinWrTmp, &sa, 0)) {
		printf("Could not create pipe for Stdin redirection\n");
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdoutWr);
		return EXECUTE_FAILED;
	}

	bSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdinWrTmp, GetCurrentProcess(),
		&hChildStdinWr, 0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!bSuccess) {
		printf("Unable to duplicate handle for Stdin redirection\n");
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdoutWr);
		CloseHandle(hChildStdinRd);
		CloseHandle(hChildStdinWrTmp);
		return EXECUTE_FAILED;
	}
	CloseHandle(hChildStdinWrTmp);

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	si.hStdInput = hChildStdinRd;
	si.hStdError = hChildStderrWr;
	si.hStdOutput = hChildStdoutWr;
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWMINNOACTIVE;
	ZeroMemory( &pi, sizeof(pi) );

	char *pszSystemRoot = ".\\";

	// Start the child process. 
	if( !CreateProcess( NULL, // No module name (use command line). 
		pCmdLine,         // Command line. 
		NULL,             // Process handle not inheritable. 
		NULL,             // Thread handle not inheritable. 
		TRUE,             // Set handle inheritance to TRUE. 
		0,                // No creation flags. 
		NULL,             // Use parent's environment block. 
		pszSystemRoot,	  // Use SystemRoot as current directory
		&si,              // Pointer to STARTUPINFO structure.
		&pi )             // Pointer to PROCESS_INFORMATION structure.
		) 
	{
		printf("Failed to create process using path %s, return code %d\n", pCmdLine, GetLastError());
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdinRd);
		CloseHandle(hChildStdinWr);
		return EXECUTE_FAILED;
	}

	// Close the handles used by the child, they are no longer needed
	CloseHandle(hChildStderrWr);
	CloseHandle(hChildStdoutWr);
	CloseHandle(hChildStdinRd);

	int nLength = 0;
	int nPosition = 0;
	int nPrinted = 0;
	DWORD dwAvail;

	// Loop until the process exits, collecting (and printing!) it's output
	while (WAIT_TIMEOUT == WaitForSingleObject(pi.hProcess, 0)) {
		if (PeekNamedPipe(hChildStdoutRd, NULL, 0, NULL, &dwAvail, NULL)) {
			if (dwAvail < 1) {
				Sleep(200);
				continue;
			}
		} else {
			Sleep(200);
			continue;
		}

		bSuccess = ReadFile(hChildStdoutRd, &szBuf[nPosition], 1024*1024 - nPosition, &dwRead, NULL);
		if (!bSuccess) {
			if (ERROR_MORE_DATA != GetLastError()) {
				break;
			}
		}
		if (0 == dwRead) {
			continue;
		}
		nLength = nPosition + dwRead;
		szBuf[nLength] = '\0';

		printf("%s\n", &szBuf[nPrinted]);
		nPosition = nLength;
		nPrinted = nLength;

		if (nPosition > 1024*1024-10) {
			printf("** Out of text buffer - preserving first 5k and wrapping the rest.\n");
			nPosition = 5*1024;
		}
	}

	// Get the exit code of the spawned process
	DWORD dwExitCode;
	int nExitCode;
	if (GetExitCodeProcess( pi.hProcess, &dwExitCode )) {
		nExitCode = int(dwExitCode);
	} else {
		nExitCode = INVALID_EXIT_CODE;
	}

	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	
	// get the rest of the output
	for(;;) {
		DWORD dwAvail;
		if (PeekNamedPipe(hChildStdoutRd, NULL, 0, NULL, &dwAvail, NULL)) {
			if (dwAvail < 1) {
				break;
			}
		} else {
			break;
		}

		bSuccess = ReadFile(hChildStdoutRd, &szBuf[nPosition], (1024*1024) - nPosition, &dwRead, NULL);
		if ((!bSuccess)||(0 == dwRead)) {
			if (ERROR_MORE_DATA != GetLastError()) {
				break;
			}
		}
		nLength = nPosition + dwRead;
		szBuf[nLength] = '\0';

		printf("%s\n", &szBuf[nPrinted]);
		nPosition = nLength;
		nPrinted = nLength;

		if (nPosition > 1024*1024-10) {
			printf("** Out of text buffer - preserving first 5k and wrapping the rest.\n");
			nPosition = 5*1024;
		}
	}

	// close read and write handles
	CloseHandle(hChildStdoutRd);
	CloseHandle(hChildStdinWr);

	printf("%s\n", &szBuf[nPrinted]);

	// we're done
	return nExitCode;
}

int getNumberOfCores() {
	int numCPU;
	SYSTEM_INFO sysinfo;

	GetSystemInfo( &sysinfo );
	numCPU = sysinfo.dwNumberOfProcessors;

	printf("Detected %d cores.\n", numCPU);

	return numCPU;
}

void delfiles(char *szPath) {
	SHFILEOPSTRUCT sh;
	char buf[MAX_PATH+2];

	memset(buf, 0, sizeof(buf));
	GetCurrentDirectory(sizeof(buf), buf);
	strcat(buf, "\\");
	strcat(buf, szPath);
	buf[MAX_PATH-1]='\0';

	sh.hwnd = NULL;
	sh.wFunc = FO_DELETE;
	sh.pFrom = buf;
	sh.pTo = NULL;
	sh.fFlags = FOF_FILESONLY | FOF_NOCONFIRMATION | FOF_SILENT;
	sh.fAnyOperationsAborted = FALSE;
	sh.hNameMappings = NULL;
	sh.lpszProgressTitle = NULL;
	int ret = SHFileOperation(&sh);
	if (ret) {
		printf("Warning: file deletion failed (%d).\n", ret);
	} 
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("tividconvert <inputvideo>\n");
		printf("Inputvideo is any supported by FFMPEG\n");
		printf("Both a raw file and a cart file will be generated.\n");
		printf("For a bank-switched cart. The maximum size is 4096 pages\n");
		printf("(Although the current maximum size is 2MB (256 pages)\n");
		return -1;
	}

	// now verify that the video file exists
	strncpy(szFilename, argv[1], MAX_PATH);
	szFilename[MAX_PATH-1] = '\0';
	fp = fopen(szFilename, "rb");
	if (NULL == fp) {
		printf("Can't open the file '%s'\n", szFilename);
		return -1;
	}
	fclose(fp);

	// create a temporary work folder
	if (!CreateDirectory(szTempPath, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			printf("Can't create the temp folder\n");
			return -1;
		} else {
			// let's ASK if we should kill the temp folder
			for (;;) {
				printf("Temp folder already exists - is it okay to erase it?\n");
				printf("Y/N >");
				gets_s(szTmp);
				_strlwr(szTmp);
				if (szTmp[0] == 'y') {
					// and let's kill those temp files
					printf("Removing temp folder...\n");
					delfiles(szTempPath);
					if (!CreateDirectory(szTempPath, NULL)) {
						printf("Can't create the temp folder\n");
						return -1;
					}
					break;
				} else if (szTmp[0] == 'n') {
					printf("Exitting...\n");
					return -1;
				}
			}
		}
	}

	// spawn ffmpeg and extract the still images we need
	// ffmpeg.exe -i spaceballs.mp4 -f image2 -vf fps=8.9132 scene%05d.png
//	sprintf(szTmp, "TOOLS\\ffmpeg -y -i %s -f image2 -vf fps=8.9132 %s\\scene%%05d.png", szFilename, szTempPath);
   	sprintf(szTmp, "TOOLS\\ffmpeg -y -i %s -f image2 -vf fps=8.6458 %s\\scene%%05d.png", szFilename, szTempPath);

	if (0 != doExecuteCommand(szTmp)) {
		printf("ffmpeg failed! Give up.\n");
		return -1;
	}

    // figure out how many cores to pass to dircmd
   	int x = getNumberOfCores();
	if (x<1) x=1;
	if (x > 2) --x;		// save one core for the user
    
    // excellente! Now, process those still images into the 192x128 on 256x192 images we need.
    // although mogrify can do a folder with a wildcard, we can do it faster with dircmd for cores
    // This means we do lose error checking
	// dircmd.exe -8 *.png mogrify.exe -resize 192x128^ -gravity Center -crop 192x128+0+0 +repage -gravity NorthWest -extent 256x192 "$f"
	sprintf(szTmp, "TOOLS\\dircmd -%d %s\\*.png mogrify -verbose -resize 192x128^ -gravity Center -crop 192x128+0+0 +repage -gravity NorthWest -extent 256x192 -sharpen 5x5 \"%s\\$f\"", x, szTempPath, szTempPath);
	if (0 != doExecuteCommand(szTmp)) {
		printf("mogrify (ImageMagick) failed! Give up.\n");
		return -1;
	}

	// now execute the image conversion on each frame. We will run a thread on each CPU core.
	// dircmd.exe -8 scene*.png Convert9918 "$f" "out\$f"
	sprintf(szTmp, "TOOLS\\dircmd -%d %s\\scene*.png Convert9918 \"%s\\$f\" \"%s\\$f\"", x, szTempPath, szTempPath, szTempPath);
	// no error return on this, so we just hope for the best. It should work if everything
	// else did, as long as we don't run out of disk space.
	doExecuteCommand(szTmp);

	// clean up some temporary files to free up disk space - we don't need the PNG or BMP files
	printf("Clean up temp files...\n");

	sprintf(szTmp, "%s\\*.png", szTempPath);
	delfiles(szTmp);
	sprintf(szTmp, "%s\\*.bmp", szTempPath);
	delfiles(szTmp);

	// video frames are ready! Now, we need to get the audio going

	// Next extract the audio track as WAV:
	// ffmpeg.exe -i spaceballs.mp4 -vn -ac 1 test.wav
	sprintf(szTmp, "TOOLS\\ffmpeg -y -i %s -vn -ac 1 %s\\test.wav", szFilename, szTempPath);
	if (0 != doExecuteCommand(szTmp)) {
		printf("ffmpeg failed on audio! Give up.\n");
		return -1;
	}
	
	// now, from the output we can extract the file length, which will let us make some
	// calculations about the audio frequency
	// We need to find this string: Duration: 00:00:30.02, start: 0.000000, bitrate: 542 kb/s
	char *p = strstr(szBuf, "Duration:");
	int hh,mm;
	double duration;
	if (NULL == p) {
		printf("Can't get Duration string from ffmpeg. Giving up.\n");
		return -1;
	}
	p+=10;
	if (3 != sscanf(p, "%d:%d:%lf", &hh, &mm, &duration)) {
		printf("Failed to parse duration string from ffmpeg. Giving up.\n");
		return -1;
	}
	duration+=(mm*60.0)+(hh*3600.0);

	// now we also need to figure out how many frames there are
	WIN32_FIND_DATA dat;
	sprintf(szTmp, "%s\\*.TIAP", szTempPath);
	HANDLE hFile = FindFirstFile(szTmp, &dat);
	if (INVALID_HANDLE_VALUE == hFile) {
		printf("Couldn't count frames in temp folder - giving up.\n");
		return -1;
	}
	int frameCnt = 1;
	while (FindNextFile(hFile, &dat)) frameCnt++;
	FindClose(hFile);

	// so, finally, we need to calculate the resulting frequency
	// Duration: 30.02 (from ffmpeg) seconds
	// Actual output - 268 frames * 1544 bytes per frame = 413792 bytes.
	// 413792 / 30.02 = 13783.8774 (round up) = 13784 Hz. (Usually want to round up.)
	int finalFreq = (int)(frameCnt * 1544 / duration + 0.5);

	// report what we learned
	printf("Final is %d frames with a duration of %g, freq %dHz\n", frameCnt, duration, finalFreq);

	// now SOX will handle the conversion from whatever to unsigned 8 bits
	// No dither cause we amplify the noise
	// sox --temp ./ --norm audio16.wav -D -c 1 --rate 13784 -b 8 -e unsigned-integer out.wav
	sprintf(szTmp, "TOOLS\\sox --temp %s\\ --norm %s\\test.wav -D -c 1 --rate %d -b 8 -e unsigned-integer %s\\out.wav",
		szTempPath, szTempPath, finalFreq, szTempPath);
	if (0 != doExecuteCommand(szTmp)) {
		printf("sox failed! Give up.\n");
		return -1;
	}

	// finally, we're ready to start converting to TI format
	// Now, convert the 8-bit WAV into TI format
	// audioconvert out.wav audio.bin
	sprintf(szTmp, "TOOLS\\audioconvert %s\\out.wav %s\\audio.bin", szTempPath, szTempPath);
	if (0 != doExecuteCommand(szTmp)) {
		printf("audioconvert failed! Give up.\n");
		return -1;
	}

	// Now all conversions are done, we just need to pack:
	// videopack out\scene audio.bin finalPACK.bin
	sprintf(szTmp, "TOOLS\\videopack %s\\scene %s\\audio.bin finalPACK.bin", szTempPath, szTempPath);
	if (0 != doExecuteCommand(szTmp)) {
		printf("videopack failed! Give up.\n");
		return -1;
	}
	printf("\n\nALL DONE! Raw output is 'finalPACK.bin'\n");

	// now repack into a cartridge
	sprintf(szTmp, "TOOLS\\cartrepack finalPACK.bin finalPACK8.bin");
	if (0 != doExecuteCommand(szTmp)) {
		printf("cartrepack failed! Give up.\n");
		return -1;
	}
	printf("\n\nALL DONE! Cart output is 'finalPACK8.bin'\n");

	// delete local temp file
	delfiles("testout.raw");

	// let's ASK if we should kill the temp folder
	for (;;) {
		printf("Do you want to save the temp files?\n");
		printf("Answer YES if you might want to reprocess/repack for audio!\n");
		printf("Y/N >");
		gets_s(szTmp);
		_strlwr(szTmp);
		if (szTmp[0] == 'n') {
			// and let's kill those temp files
			printf("Removing temp folder...\n");
			delfiles(szTempPath);
			break;
		} else if (szTmp[0] == 'y') {
			break;
		}
	}

	return 0;
}

