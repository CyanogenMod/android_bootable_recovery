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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "twrp-functions.hpp"
#include "twcommon.h"

#ifndef TW_EXCLUDE_ENCRYPTED_BACKUPS
	#include "../openaes/inc/oaes_lib.h"
#endif

extern "C" {
	#include "../libcrecovery/common.h"
}

/* Execute a command */
int TWFunc::Exec_Cmd(const string& cmd, string &result) {
	FILE* exec;
	char buffer[130];
	int ret = 0;
	exec = __popen(cmd.c_str(), "r");
	if (!exec) return -1;
	while(!feof(exec)) {
		memset(&buffer, 0, sizeof(buffer));
		if (fgets(buffer, 128, exec) != NULL) {
			buffer[128] = '\n';
			buffer[129] = '\0';
			result += buffer;
		}
	}
	ret = __pclose(exec);
	return ret;
}

int TWFunc::Exec_Cmd(const string& cmd) {
	pid_t pid;
	int status;
	switch(pid = fork())
	{
		case -1:
			LOGERR("Exec_Cmd(): vfork failed: %d!\n", errno);
			return -1;
		case 0: // child
			execl("/sbin/sh", "sh", "-c", cmd.c_str(), NULL);
			_exit(127);
			break;
		default:
		{
			if (TWFunc::Wait_For_Child(pid, &status, cmd) != 0)
				return -1;
			else
				return 0;
		}
	}
}

// Returns "file.name" from a full /path/to/file.name
string TWFunc::Get_Filename(string Path) {
	size_t pos = Path.find_last_of("/");
	if (pos != string::npos) {
		string Filename;
		Filename = Path.substr(pos + 1, Path.size() - pos - 1);
		return Filename;
	} else
		return Path;
}

// Returns "/path/to/" from a full /path/to/file.name
string TWFunc::Get_Path(string Path) {
	size_t pos = Path.find_last_of("/");
	if (pos != string::npos) {
		string Pathonly;
		Pathonly = Path.substr(0, pos + 1);
		return Pathonly;
	} else
		return Path;
}

int TWFunc::Wait_For_Child(pid_t pid, int *status, string Child_Name) {
	pid_t rc_pid;

	rc_pid = waitpid(pid, status, 0);
	if (rc_pid > 0) {
		if (WEXITSTATUS(*status) == 0)
			LOGINFO("%s process ended with RC=%d\n", Child_Name.c_str(), WEXITSTATUS(*status)); // Success
		else if (WIFSIGNALED(*status)) {
			LOGINFO("%s process ended with signal: %d\n", Child_Name.c_str(), WTERMSIG(*status)); // Seg fault or some other non-graceful termination
			return -1;
		} else if (WEXITSTATUS(*status) != 0) {
			LOGINFO("%s process ended with ERROR=%d\n", Child_Name.c_str(), WEXITSTATUS(*status)); // Graceful exit, but there was an error
			return -1;
		}
	} else { // no PID returned
		if (errno == ECHILD)
			LOGINFO("%s no child process exist\n", Child_Name.c_str());
		else {
			LOGINFO("%s Unexpected error\n", Child_Name.c_str());
			return -1;
		}
	}
	return 0;
}

bool TWFunc::Path_Exists(string Path) {
	// Check to see if the Path exists
	struct stat st;
	if (stat(Path.c_str(), &st) != 0)
		return false;
	else
		return true;
}

int TWFunc::Get_File_Type(string fn) {
	string::size_type i = 0;
	int firstbyte = 0, secondbyte = 0;
	char header[3];

	ifstream f;
	f.open(fn.c_str(), ios::in | ios::binary);
	f.get(header, 3);
	f.close();
	firstbyte = header[i] & 0xff;
	secondbyte = header[++i] & 0xff;

	if (firstbyte == 0x1f && secondbyte == 0x8b)
		return 1; // Compressed
	else if (firstbyte == 0x4f && secondbyte == 0x41)
		return 2; // Encrypted
	else
		return 0; // Unknown

	return 0;
}

int TWFunc::Try_Decrypting_File(string fn, string password) {
#ifndef TW_EXCLUDE_ENCRYPTED_BACKUPS
	OAES_CTX * ctx = NULL;
	uint8_t _key_data[32] = "";
	FILE *f;
	uint8_t buffer[4096];
	uint8_t *buffer_out = NULL;
	uint8_t *ptr = NULL;
	size_t read_len = 0, out_len = 0;
	int firstbyte = 0, secondbyte = 0, key_len;
	size_t _j = 0;
	size_t _key_data_len = 0;

	// mostly kanged from OpenAES oaes.c
	for( _j = 0; _j < 32; _j++ )
		_key_data[_j] = _j + 1;
	_key_data_len = password.size();
	if( 16 >= _key_data_len )
		_key_data_len = 16;
	else if( 24 >= _key_data_len )
		_key_data_len = 24;
	else
	_key_data_len = 32;
	memcpy(_key_data, password.c_str(), password.size());

	ctx = oaes_alloc();
	if (ctx == NULL) {
		LOGERR("Failed to allocate OAES\n");
		return -1;
	}

	oaes_key_import_data(ctx, _key_data, _key_data_len);

	f = fopen(fn.c_str(), "rb");
	if (f == NULL) {
		LOGERR("Failed to open '%s' to try decrypt\n", fn.c_str());
		return -1;
	}
	read_len = fread(buffer, sizeof(uint8_t), 4096, f);
	if (read_len <= 0) {
		LOGERR("Read size during try decrypt failed\n");
		fclose(f);
		return -1;
	}
	if (oaes_decrypt(ctx, buffer, read_len, NULL, &out_len) != OAES_RET_SUCCESS) {
		LOGERR("Error: Failed to retrieve required buffer size for trying decryption.\n");
		fclose(f);
		return -1;
	}
	buffer_out = (uint8_t *) calloc(out_len, sizeof(char));
	if (buffer_out == NULL) {
		LOGERR("Failed to allocate output buffer for try decrypt.\n");
		fclose(f);
		return -1;
	}
	if (oaes_decrypt(ctx, buffer, read_len, buffer_out, &out_len) != OAES_RET_SUCCESS) {
		LOGERR("Failed to decrypt file '%s'\n", fn.c_str());
		fclose(f);
		free(buffer_out);
		return 0;
	}
	fclose(f);
	if (out_len < 2) {
		LOGINFO("Successfully decrypted '%s' but read length %i too small.\n", fn.c_str(), out_len);
		free(buffer_out);
		return 1; // Decrypted successfully
	}
	ptr = buffer_out;
	firstbyte = *ptr & 0xff;
	ptr++;
	secondbyte = *ptr & 0xff;
	if (firstbyte == 0x1f && secondbyte == 0x8b) {
		LOGINFO("Successfully decrypted '%s' and file is compressed.\n", fn.c_str());
		free(buffer_out);
		return 3; // Compressed
	}
	if (out_len >= 262) {
		ptr = buffer_out + 257;
		if (strncmp((char*)ptr, "ustar", 5) == 0) {
			LOGINFO("Successfully decrypted '%s' and file is tar format.\n", fn.c_str());
			free(buffer_out);
			return 2; // Tar
		}
	}
	free(buffer_out);
	LOGINFO("No errors decrypting '%s' but no known file format.\n", fn.c_str());
	return 1; // Decrypted successfully
#else
	LOGERR("Encrypted backup support not included.\n");
	return -1;
#endif
}

unsigned long TWFunc::Get_File_Size(string Path) {
	struct stat st;

	if (stat(Path.c_str(), &st) != 0)
		return 0;
	return st.st_size;
}

std::string TWFunc::Remove_Trailing_Slashes(const std::string& path, bool leaveLast)
{
	std::string res;
	size_t last_idx = 0, idx = 0;

	while(last_idx != std::string::npos)
	{
		if(last_idx != 0)
			res += '/';

		idx = path.find_first_of('/', last_idx);
		if(idx == std::string::npos) {
			res += path.substr(last_idx, idx);
			break;
		}

		res += path.substr(last_idx, idx-last_idx);
		last_idx = path.find_first_not_of('/', idx);
	}

	if(leaveLast)
		res += '/';
	return res;
}

int TWFunc::read_file(string fn, string& results) {
	ifstream file;
	file.open(fn.c_str(), ios::in);
	if (file.is_open()) {
		file >> results;
		file.close();
		return 0;
	}
	LOGINFO("Cannot find file %s\n", fn.c_str());
	return -1;
}

int TWFunc::read_file_line_by_line(string fn, vector<string>& lines, bool skip_empty) {
	if (fn.empty())
		return 0;

	ifstream file;
	file.open(fn.c_str(), ios::in);
	if (file.is_open()) {
		string line;
		while (getline(file, line)) {
			if (line.empty() && skip_empty)
				continue;
			lines.push_back(line);
		}
		file.close();
		return 1;
	}
	LOGINFO("Cannot find file %s\n", fn.c_str());
	return 0;
}

int TWFunc::write_file(string fn, string& line) {
	FILE *file;
	file = fopen(fn.c_str(), "w");
	if (file != NULL) {
		fwrite(line.c_str(), line.size(), 1, file);
		fclose(file);
		return 0;
	}
	LOGINFO("Cannot find file %s\n", fn.c_str());
	return -1;
}

