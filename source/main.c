#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <3ds.h>

#ifndef VERSION
#define VERSION
#endif

#define ENTRY_SIZE 0x10

enum Mode {
	MODE_SINGLE, MODE_ALL, MODE_FILE //Not implemented
};

enum Mode mode;

int debug;

int confirm() {
	while (aptMainLoop()) {
		hidScanInput();
		gspWaitForVBlank();

		u32 kDown = hidKeysDown();
		if (!(kDown & (KEY_A | KEY_B)))
			break;
	}
	while (aptMainLoop()) {
		hidScanInput();
		gspWaitForVBlank();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_A)
			return true;
		if (kDown & KEY_B)
			return false;
	}
	return false;
}

FS_Archive extdata_archive;

FS_Path verListPath;

Result initFS() {
	u32 extdata_archive_lowpathdata[3] = { MEDIATYPE_NAND, 0xf000000e, 0 };
	extdata_archive = (FS_Archive ) { ARCHIVE_SHARED_EXTDATA,
					(FS_Path ) { PATH_BINARY, 0xC,
									(u8*) extdata_archive_lowpathdata } };

	verListPath = fsMakePath(PATH_ASCII, "/versionList.dat");

	return FSUSER_OpenArchive(&extdata_archive);
}

Result getFile(u8 ** buffer, u64 * size) {
	Result ret = 0;
	Handle filehandle = 0;
	u32 tmpval = 0;

	ret = FSUSER_OpenFile(&filehandle, extdata_archive, verListPath,
			FS_OPEN_READ, 0);
	if (ret != 0)
		return ret;

	ret = FSFILE_GetSize(filehandle, size);
	if (ret != 0)
		return ret;

	if (*size <= ENTRY_SIZE) {
		*size = 0;
		return ret;
	}

	if (mode == MODE_ALL)
		*size = ENTRY_SIZE;

	*buffer = malloc(*size);
	ret = FSFILE_Read(filehandle, &tmpval, 0, *buffer, *size);

	if (ret != 0)
		return ret;

	FSFILE_Close(filehandle);
	return ret;
}

Result putFile(u8 * buffer, u64 size) {
	Result ret = 0;
	Handle filehandle = 0;
	u32 tmpval = 0;

	ret = FSUSER_DeleteFile(extdata_archive, verListPath);

	ret = FSUSER_CreateFile(extdata_archive, verListPath, 0, size);
	if (ret != 0)
		return ret;

	ret = FSUSER_OpenFile(&filehandle, extdata_archive, verListPath,
			FS_OPEN_WRITE, 0);
	if (ret != 0)
		return ret;

	ret = FSFILE_Write(filehandle, &tmpval, 0, buffer, size, FS_WRITE_FLUSH);
	FSFILE_Close(filehandle);

	return ret;
}

void setModes(void) {
	mode = (access("UpdateSuppressor.xml", F_OK) != -1) ?
			MODE_SINGLE : MODE_ALL;

	debug = (access("debug", F_OK) != -1);
}

int deleteEntry(u64 PID, u8 * buffer, u64 size) {
	u64 *file = (u64 *) buffer;

	u64 entry = 0;
	while (true) {

		if (file[entry * 0x2] == PID)
			break;

		entry++;
		if (entry * ENTRY_SIZE >= size)
			break;
	}

	if (entry * ENTRY_SIZE >= size) {
		printf("Version record not found.\n");
		debug = true;
		return 2;
	} else {
		printf("Version record found at entry %X.\n",
				(unsigned int) (entry & 0xFFFFFFFF));

		gfxFlushBuffers();
		gfxSwapBuffers();

		if (debug) {
			printf("Do you want to remove this record?(A/B)\n");
			gfxFlushBuffers();
			gfxSwapBuffers();
			if (confirm() == false) {
				return 1;
			}
		}

		file[entry * 0x2] = 0x0;
	}
	return 0;
}

int main() {

	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);

	printf("Update Suppressor %s\n", VERSION);

	setModes();

	if (debug)
		printf("Debug Mode\n");

	switch (mode) {
	case MODE_SINGLE:
		puts("Single Mode");
		break;
	case MODE_ALL:
		puts("All Mode");
		break;
	case MODE_FILE:
		puts("Not Implemented: File Mode");
		break;
	default:
		puts("wtf? Unknown Mode");
		break;
	}

	u64 PID = 0;
	if (mode == MODE_SINGLE) {
		aptOpenSession();
		APT_GetProgramID(&PID);
		aptCloseSession();

		printf("Title Id: %08X%08X\n", (unsigned int) (PID >> 32),
				(unsigned int) (PID & 0xFFFFFFFF));
	}

	gfxFlushBuffers();
	gfxSwapBuffers();

	u8* u8file;
	u64 size;

	Result ret = initFS();
	if (ret != 0) {
		printf("Error Initializing: %li\n", ret);
		debug = true;
	} else {

		ret = getFile(&u8file, &size);
		if (ret != 0) {
			printf("Error Reading: %li\n", ret);
			debug = true;
		} else if (size == 0) {
			puts("File already cleared");
			debug = true;
		} else {

			if (mode != MODE_ALL) {

				ret = (Result) deleteEntry(PID, u8file, size);
			} else if (debug) {
				printf("Do you want to remove the list?(A/B)\n");
				gfxFlushBuffers();
				gfxSwapBuffers();
				if (confirm() == false) {
					ret = 1;
				}
			}

			if (ret == 1) {
				gfxExit();
				return 0;
			}

			if (ret == 0) {
				ret = putFile(u8file, size);

				if (ret != 0) {
					printf("Error Writing: %li\n", ret);
					debug = true;
				} else {
					printf("Version record removed.\n");
				}
			}
		}

		FSUSER_CloseArchive(&extdata_archive);
	}

	gfxFlushBuffers();
	gfxSwapBuffers();

	if (debug)
		confirm();

	gfxExit();
	return 0;
}
