#include <sys/stat.h>
#if !defined(_WIN32) && !defined(_WIN64)
	#include <dirent.h>
	#include <utime.h>
#else
	#include <sys/utime.h>
	#include <windows.h>
	#define _utime utime
	#define _utimbuf utimbuf;
#endif
#include <map>
#include "CString.h"
#include "CFileSystem.h"

#if defined(_WIN32) || defined(_WIN64)
	const char fSep = '\\';
	const char fSep_O = '/';
#else
	const char fSep = '/';
	const char fSep_O = '\\';
#endif

extern CString homepath;

void CFileSystem::addDir(const CString& dir, const CString& wildcard)
{
	// Format the directory.
	CString newDir(dir);
	if (newDir[newDir.length() - 1] == '/' || newDir[newDir.length() - 1] == '\\')
		CFileSystem::fixPathSeparators(&newDir);
	else
	{
		newDir << fSep;
		CFileSystem::fixPathSeparators(&newDir);
	}

	// See if the directory already exists.
	// If it does, first remove it before re-adding it.
	if (finddiri(newDir).length() != 0)
		removeDir(newDir);

	// Add the directory to the directory list.
	dirList[newDir] = CString() << homepath << newDir << wildcard;

	// Load up the files in the directory.
	loadAllDirectories(dirList[newDir], false);
}

void CFileSystem::removeDir(const CString& dir)
{
	// Format the search directory.
	CString searchDir(CString() << homepath << dir);
	if (searchDir[searchDir.length() - 1] == '/' || searchDir[searchDir.length() - 1] == '\\')
		CFileSystem::fixPathSeparators(&searchDir);
	else
		searchDir << fSep;

	// Remove every file that belongs to the specified root directory.
	for (std::map<CString, CString>::iterator i = fileList.begin(); i != fileList.end();)
	{
		if (i->second.findi(searchDir))
			fileList.erase(i++);
		else ++i;
	}

	// Remove the directory from the directory list.
	for (std::map<CString, CString>::iterator i = dirList.begin(); i != dirList.end();)
	{
		if (i->second.findi(searchDir))
			dirList.erase(i++);
		else ++i;
	}
}

void CFileSystem::addFile(const CString& file)
{
	// Grab the file name and directory.
	CString filename(file.subString(file.findl(fSep) + 1));
	CString directory(file.subString(0, file.find(filename)));

	// Fix directory path separators.
	CFileSystem::fixPathSeparators(&directory);
	if (directory.find(homepath) != -1)
		directory.removeI(0, homepath.length());

	// Add to the map.
	fileList[filename] = CString() << homepath << directory << filename;
}

void CFileSystem::removeFile(const CString& file)
{
	// Grab the file name and directory.
	CString filename(file.subString(file.findl(fSep) + 1));
	CString directory(file.subString(0, file.find(filename)));

	// Fix directory path separators.
	CFileSystem::fixPathSeparators(&directory);

	// Remove it from the map.
	fileList.erase(filename);
}

void CFileSystem::resync()
{
	// Clear the file list.
	fileList.clear();

	// Iterate through all the directories, reloading their file list.
	for (std::map<CString, CString>::const_iterator i = dirList.begin(); i != dirList.end(); ++i)
		loadAllDirectories(i->second, false);
}

CString CFileSystem::find(const CString& file) const
{
	std::map<CString, CString>::const_iterator i = fileList.find(file);
	if (i == fileList.end()) return CString();
	return CString(i->second);
}

CString CFileSystem::findi(const CString& file) const
{
	for (std::map<CString, CString>::const_iterator i = fileList.begin(); i != fileList.end(); ++i)
		if (i->first.comparei(file)) return CString(i->second);
	return CString();
}

CString CFileSystem::finddir(const CString& dir) const
{
	CString searchDir(dir);
	if (searchDir[searchDir.length() - 1] == '/' || searchDir[searchDir.length() - 1] == '\\')
		CFileSystem::fixPathSeparators(&searchDir);
	else
		searchDir << fSep;

	std::map<CString, CString>::const_iterator i = dirList.find(searchDir);
	if (i == fileList.end()) return CString();
	return CString(i->second);
}

CString CFileSystem::finddiri(const CString& dir) const
{
	CString searchDir(dir);
	if (searchDir[searchDir.length() - 1] == '/' || searchDir[searchDir.length() - 1] == '\\')
		CFileSystem::fixPathSeparators(&searchDir);
	else
		searchDir << fSep;

	for (std::map<CString, CString>::const_iterator i = dirList.begin(); i != dirList.end(); ++i)
		if (i->first.comparei(searchDir)) return CString(i->second);
	return CString();
}

#if defined(_WIN32) || defined(_WIN64)
void CFileSystem::loadAllDirectories(const CString& directory, bool recursive)
{
	CString dir = CString() << directory.remove(directory.findl(fSep)) << fSep;
	WIN32_FIND_DATAA filedata;
	HANDLE hFind = FindFirstFileA(directory.text(), &filedata);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (filedata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (filedata.cFileName[0] != '.' && recursive)
				{
					// We need to add the directory to the directory list.
					CString newDir(filedata.cFileName);
					for (std::map<CString, CString>::const_iterator i = dirList.begin(); i != dirList.end();)
					{
						// We found a match for the previous directory.
						CString sDir = CString() << i->second.remove(i->second.findl(fSep)) << fSep;
						if (sDir == dir)
						{
							CString firstDir = i->first;
							if (firstDir[firstDir.length() - 1] != CString(fSep))
								firstDir << fSep;
							firstDir << newDir;
							addDir(firstDir);	// Add the directory to the directory list.
							i = dirList.end();
						} else ++i;
					}
				}
			}
			else
			{
				// Grab the file name.
				CString file((char *)filedata.cFileName);
				fileList[file] = CString(dir) << filedata.cFileName;
			}
		} while (FindNextFileA(hFind, &filedata));
	}
	FindClose(hFind);
}
#else
void CFileSystem::loadAllDirectories(const CString& directory, bool recursive)
{
	CString path = CString() << directory.remove(directory.findl(fSep)) << fSep;
	CString wildcard = directory.subString(directory.findl(fSep) + 1);
	DIR *dir;
	struct stat statx;
	struct dirent *ent;

	// Try to open the directory.
	if ((dir = opendir(path.text())) == 0)
		return;

	// Read everything in it now.
	while ((ent = readdir(dir)) != 0)
	{
		if (ent->d_name[0] != '.')
		{
			CString dircheck = CString() << path << ent->d_name;
			stat(dircheck.text(), &statx);
			if ((statx.st_mode & S_IFDIR) && recursive)
			{
				// We need to add the directory to the directory list.
				CString newDir(ent->d_name);
				for (std::map<CString, CString>::const_iterator i = dirList.begin(); i != dirList.end();)
				{
					// We found a match for the previous directory.
					CString sDir = CString() << i->second.remove(i->second.findl(fSep)) << fSep;
					if (sDir == path)
					{
						CString firstDir = i->first;
						if (firstDir[firstDir.length() - 1] != CString(fSep))
							firstDir << fSep;
						firstDir << newDir;
						addDir(firstDir);	// Add the directory to the directory list.
						i = dirList.end();
					} else ++i;
				}
				continue;
			}
		}
		else continue;

		// Grab the file name.
		CString file(ent->d_name);
		if (file.match(wildcard))
			fileList[file] = CString(path) << file;
	}
	closedir(dir);
}
#endif

CString CFileSystem::load(const CString& file) const
{
	// Get the full path to the file.
	CString fileName = find(file);
	if (fileName.length() == 0) return CString();

	// Load the file.
	CString fileData;
	fileData.load(fileName);

	return fileData;
}

time_t CFileSystem::getModTime(const CString& file) const
{
	// Get the full path to the file.
	CString fileName = find(file);
	if (fileName.length() == 0) return 0;

	struct stat fileStat;
	if (stat(fileName.text(), &fileStat) != -1)
		return (time_t)fileStat.st_mtime;
	return 0;
}

bool CFileSystem::setModTime(const CString& file, time_t modTime) const
{
	// Get the full path to the file.
	CString fileName = find(file);
	if (fileName.length() == 0) return false;

	// Set the times.
	struct utimbuf ut;
	ut.actime = modTime;
	ut.modtime = modTime;

	// Change the file.
	if (utime(fileName.text(), &ut) == 0) return true;
	return false;
}

int CFileSystem::getFileSize(const CString& file) const
{
	// Get the full path to the file.
	CString fileName = find(file);
	if (fileName.length() == 0) return 0;

	struct stat fileStat;
	if (stat(fileName.text(), &fileStat) != -1)
		return fileStat.st_size;
	return 0;
}

void CFileSystem::fixPathSeparators(CString* pPath)
{
	pPath->replaceAllI(fSep_O, fSep);
}

char CFileSystem::getPathSeparator()
{
	return fSep;
}
