#include "FileUtils.hpp"

std::string appDataDirPath;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
void SyncFileSystem()
{
	EM_ASM(
		FS.syncfs(false, function (err) { });
	);
}
#else
void SyncFileSystem() { }
#endif
