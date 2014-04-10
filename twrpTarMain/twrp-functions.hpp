/*
	Copyright 2012 bigbiff/Dees_Troy TeamWin
	This file is part of TWRP/TeamWin Recovery Project.

	TWRP is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	TWRP is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with TWRP.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TWRPFUNCTIONS_HPP
#define _TWRPFUNCTIONS_HPP

#include <string>
#include <vector>

using namespace std;

typedef enum
{
	rb_current = 0,
	rb_system,
	rb_recovery,
	rb_poweroff,
	rb_bootloader,     // May also be fastboot
	rb_download,
} RebootCommand;

// Partition class
class TWFunc
{
public:
	static string Get_Root_Path(string Path);                                   // Trims any trailing folders or filenames from the path, also adds a leading / if not present
	static string Get_Path(string Path);                                        // Trims everything after the last / in the string
	static string Get_Filename(string Path);                                    // Trims the path off of a filename

	static int Exec_Cmd(const string& cmd, string &result);                     //execute a command and return the result as a string by reference
	static int Exec_Cmd(const string& cmd);                                     //execute a command
	static int Wait_For_Child(pid_t pid, int *status, string Child_Name);       // Waits for pid to exit and checks exit status
	static bool Path_Exists(string Path);                                       // Returns true if the path exists
	static int Get_File_Type(string fn); // Determines file type, 0 for unknown, 1 for gzip, 2 for OAES encrypted
	static int Try_Decrypting_File(string fn, string password); // -1 for some error, 0 for failed to decrypt, 1 for decrypted, 3 for decrypted and found gzip format
	static unsigned long Get_File_Size(string Path);                            // Returns the size of a file
	static std::string Remove_Trailing_Slashes(const std::string& path, bool leaveLast = false); // Normalizes the path, e.g /data//media/ -> /data/media
		// read from file
		static int read_file(string fn, string& results);
		static int read_file_line_by_line(string fn, vector<string>& lines, bool skip_empty);
		//write from file
		static int write_file(string fn, string& line);
};

#endif // _TWRPFUNCTIONS_HPP
