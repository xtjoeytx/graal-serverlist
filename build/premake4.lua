newoption {
	trigger		= "no-64bit",
	description	= "Don't add the 64-bit project configuration."
}

newoption {
	trigger		= "no-mysql",
	description = "Don't use MySQL.  Disables accounts/profiles."
}

solution "serverlist2"
	configurations { "Debug", "Release" }
	platforms { "native", "x32" }
	if not _OPTIONS["no-64bit"] then platforms { "x64" } end
	flags { "Symbols", "Unicode" }
	
	project "serverlist2"
		kind "ConsoleApp"
		-- kind "WindowedApp"
		language "C++"
		location "projects"
		targetdir "../bin"
		targetname "serverlist2"
		files { "../server/include/**", "../server/src/**" }
		includedirs { "../server/include" }
		
		-- Dependencies.
		files { "../dependencies/zlib/**" }
		files { "../dependencies/bzip2/**" }
		includedirs { "../dependencies/include" }
		includedirs { "../dependencies/zlib" }
		includedirs { "../dependencies/bzip2" }
		
		-- Global defines.
		if _OPTIONS["no-mysql"] then
			defines { "NO_MYSQL" }
		end

		-- Libraries.
		libdirs { "../dependencies/" }
		if not _OPTIONS["no-mysql"] then
			links { "mysqlclient" }
		end
		configuration "windows"
			links { "ws2_32" }
		
		-- Windows defines.
		configuration "windows"
			defines { "WIN32", "_WIN32" }
		if not _OPTIONS["no-64bit"] then 
			configuration { "windows", "x64" }
				defines { "WIN64", "_WIN64" }
		end
		
		-- Debug options.
		configuration "Debug"
			defines { "DEBUG" }
			targetsuffix "_d"
			flags { "NoEditAndContinue" }
		
		-- Release options.
		configuration "Release"
			defines { "NDEBUG" }
			flags { "OptimizeSpeed" }
