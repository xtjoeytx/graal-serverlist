--
-- server premake script
--

project_dynamic("serverlist2", "c++", "exe")

package.files =
{
    matchfiles(rootdir.."server/src/*.cpp"),
    matchfiles(rootdir.."server/include/*.h"),
}

-- No MySQL command line option.
addoption("no-mysql", "Don't use MySQL.  Disables accounts/profiles.")

-- Windows library includes.
if (windows) then
	include(rootdir.."dependencies/include")
	librarypath(rootdir.."dependencies")
end

-- Libraries to link to.
if (linux or target == "cb-gcc" or target == "gnu") then
	library("z")
	library("bz2")
else
	library("libz")
	library("libbz2")
end
if (windows) then library("ws2_32") end
if (not options["no-mysql"]) then
	library("mysqlclient")
else
	table.insert(package.defines,"NO_MYSQL")
end
