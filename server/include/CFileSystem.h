#ifndef CFILESYSTEM_H
#define CFILESYSTEM_H

#include <map>
#include "CString.h"

class CFileSystem
{
	public:
		CFileSystem() {};

		void addDir(const CString& dir, const CString& wildcard = "*");
		void removeDir(const CString& dir);
		void addFile(const CString& file);
		void removeFile(const CString& file);
		void resync();

		CString find(const CString& file) const;
		CString findi(const CString& file) const;
		CString finddir(const CString& file) const;
		CString finddiri(const CString& file) const;
		CString load(const CString& file) const;
		time_t getModTime(const CString& file) const;
		bool setModTime(const CString& file, time_t modTime) const;
		int getFileSize(const CString& file) const;
		std::map<CString, CString>* getFileList()	{ return &fileList; }
		std::map<CString, CString>* getDirList()	{ return &dirList; }

		static void fixPathSeparators(CString* pPath);
		static char getPathSeparator();

	private:
		void loadAllDirectories(const CString& directory, bool recursive = false);

		CString basedir;
		std::map<CString, CString> fileList;
		std::map<CString, CString> dirList;
};

#endif
