#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <3ds.h>

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

Result getFile(u8 ** buffer, u64 * size) {
	Result ret = 0;
	Handle filehandle = 0;
	u32 tmpval = 0;

	u32 extdata_archive_lowpathdata[3] = { mediatype_NAND, 0xf000000e, 0 };
	FS_archive extdata_archive = (FS_archive ) { ARCH_SHARED_EXTDATA,
					(FS_path ) { PATH_BINARY, 0xC,
									(u8*) extdata_archive_lowpathdata } };
	FSUSER_OpenArchive(NULL, &extdata_archive);

	ret = FSUSER_OpenFile(NULL, &filehandle, extdata_archive,
			FS_makePath(PATH_CHAR, "/versionList.dat"), FS_OPEN_READ, 0);

	ret = FSFILE_GetSize(filehandle, size);

	*buffer = malloc(*size);
	ret = FSFILE_Read(filehandle, &tmpval, 0, *buffer, *size);

	FSFILE_Close(filehandle);
	return ret;
}

Result putFile(u8 * buffer, u64 size) {
	Result ret = 0;
	Handle filehandle = 0;
	u32 tmpval = 0;

	FS_path fspath = FS_makePath(PATH_CHAR, "/versionList.dat");

	u32 extdata_archive_lowpathdata[3] = { mediatype_NAND, 0xf000000e, 0 };
	FS_archive extdata_archive = (FS_archive ) { ARCH_SHARED_EXTDATA,
					(FS_path ) { PATH_BINARY, 0xC,
									(u8*) extdata_archive_lowpathdata } };
	FSUSER_OpenArchive(NULL, &extdata_archive);

	ret = FSUSER_DeleteFile(NULL, extdata_archive, fspath);

	ret = FSUSER_CreateFile(NULL, extdata_archive, fspath, size);
	if (ret != 0)
		return ret;

	ret = FSUSER_OpenFile(NULL, &filehandle, extdata_archive, fspath,
	FS_OPEN_WRITE, 0);
	if (ret != 0)
		return ret;

	ret = FSFILE_Write(filehandle, &tmpval, 0, buffer, size, FS_WRITE_FLUSH);
	FSFILE_Close(filehandle);

	return ret;
}

int main() {

	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);

	printf("Update Suppressor %s\n", VERSION);

	int halt = (access("halt", F_OK) != -1);

	if (halt)
		printf("Halting: Always halt\n");

	u64 tid = 0;

	aptOpenSession();
	APT_GetProgramID(NULL, &tid);
	aptCloseSession();

	printf("Title Id: %08X%08X\n", (unsigned int) (tid >> 32),
			(unsigned int) (tid & 0xFFFFFFFF));

	gfxFlushBuffers();
	gfxSwapBuffers();

	u8* u8file;
	u64 size;

	Result ret = getFile(&u8file, &size);
	if (ret != 0) {
		printf("Error Reading: %X\n", (unsigned int) ret);
		halt = true;
	} else {

		u64 *file = (u64 *) u8file;

		u64 entry = 0;
		while (true) {

			if (file[entry * 0x2] == tid)
				break;

			entry++;
			if (entry * 0x10 >= size)
				break;
		}

		if (entry * 0x10 >= size) {
			printf("Version record not found.\n");
			halt = true;
		} else {
			printf("Version record found at entry %X.\n",
					(unsigned int) (entry & 0xFFFFFFFF));

			gfxFlushBuffers();
			gfxSwapBuffers();

			if (halt) {
				printf("Do you want to remove this record?(A/B)\n");
				gfxFlushBuffers();
				gfxSwapBuffers();
				if (confirm() == false) {
					gfxExit();
					return 0;
				}
			}

			file[entry * 0x2] = 0x0;

			ret = putFile(u8file, size);

			if (ret != 0) {
				printf("Error Writing: %X\n", (unsigned int) ret);
				halt = true;
			} else {
				printf("Version record removed.\n");
			}
		}
	}

	gfxFlushBuffers();
	gfxSwapBuffers();

	if (halt)
		confirm();

	gfxExit();
	return 0;
}
