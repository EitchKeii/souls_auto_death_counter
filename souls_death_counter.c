#include <Windows.h>
#include <TlHelp32.h>
#include <io.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t   b8;
typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct games
{
	char* game_name;
	int   offset[7];
	int   offset_count;
} games;

u32 get_pid(const char* process_name)
{
	PROCESSENTRY32 process_info;
	u32 result = 0;
	void* process_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (process_snapshot == INVALID_HANDLE_VALUE) return 0;

	process_info.dwSize = sizeof(process_info);

	if (!Process32First(process_snapshot, &process_info))
	{
		CloseHandle(process_snapshot);
		printf("Failed to gather information on system processes!\n");
		return 0;
	}

	do
	{
		if (strcmp(process_name, process_info.szExeFile) == 0)
		{
			result = process_info.th32ProcessID;
			break;
		}
	} while (Process32Next(process_snapshot, &process_info));

	CloseHandle(process_snapshot);
	return result;
}

u8* get_base_address(const char* module_name, unsigned long process_id)
{
	MODULEENTRY32 mod_entry = { 0 };
	void* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 current = { 0 };
		current.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(snapshot, &current))
		{
			do
			{
				if (memcmp(module_name, current.szModule, sizeof(module_name)) == 0)
				{
					mod_entry = current;
					break;
				}
			} while (Module32Next(snapshot, &current));
		}
	}
	//printf("%lld\n", mod_entry.modBaseAddr);
	return mod_entry.modBaseAddr;
}

void print_to_file(const char* string)
{
	FILE* fp = fopen("deaths.txt", "w");
	if (!fp) exit(1); 
	fprintf(fp, "%s", string);
	fclose(fp);
}

b8 process_ended(const char* process_name, void* process_handle)
{
	DWORD exit_code;
	GetExitCodeProcess(process_handle, &exit_code);
	// printf("exit code: %d\n", exit_code);
	if (exit_code == 0)
	{
		printf("%s has been closed.\n\n", process_name);
		return TRUE;
	}
	return FALSE;
}

int main(int argc, char** argv)
{
	u32   pid;
	u8*   base_address;
	u64   death_addr;
	u32   deaths;
	u32   new_deaths;
	void* process_handle = NULL;
	// char* death_counter_string = calloc(128, sizeof(char));
	char  death_counter_string[128];
	int   is_wow64;
	int   game_index;
	u8 	  addr_size;
	games game[] = {{"DarkSoulsRemastered.exe", {0x1C8A530, 0x98}, 2},
					{"DarkSoulsII.exe", 		{0x1150414, 0x74, 0xB8, 0x34, 0x4, 0x28C, 0x100}, 7},
					{"DarkSoulsIII.exe", 		{0x47572B8, 0x98}, 2},
					{"sekiro.exe", 				{0x3D5AAC0, 0x90}, 2}};
	

	start:
	deaths = 0;
	new_deaths = 0;
	death_addr = 0;
	if (_access("deaths.txt", 0) == -1) print_to_file("Deaths: 0");
	printf("looking for Souls game process...\n");

	do
	{
		for (int i = 0; i < sizeof(game) / sizeof(game[0]); i++)
		{
			pid = get_pid(game[i].game_name);
			if (pid)
			{
				game_index = i;
				printf("%s found.\n", game[i].game_name);
				break;
			}
		}
		if (!pid) Sleep(500);
	}
	while (!pid);
	process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	printf("process id: %d\n", pid);

	do
	{
		base_address = get_base_address(game[game_index].game_name, pid);
		if (process_ended(game[game_index].game_name, process_handle)) goto start;
	}
	while (!base_address);
	printf("base address: 0x%llX\n", (u64)base_address);

	IsWow64Process(process_handle, &is_wow64);
	if (is_wow64)
	{
		addr_size = sizeof(u32);
		printf("process architecture: 32bit\n");
	}
	else
	{
		addr_size = sizeof(u64);
		printf("process architecture: 64bit\n");
	}

	while (1)
	{
		if (deaths == 0 || death_addr == 0)
		{
			u64 new_death_addr = 0;
			while (!new_death_addr)
			{
				// printf("looking for death count address\n");
				ReadProcessMemory(process_handle, (void*)(base_address + game[game_index].offset[0]), &new_death_addr, addr_size, 0);
				for (int i = 1; i < game[game_index].offset_count-1; i++)
				{
					ReadProcessMemory(process_handle, (void*)(new_death_addr + game[game_index].offset[i]), &new_death_addr, addr_size, 0);
				}
				if (process_ended(game[game_index].game_name, process_handle)) goto start;
				if (!new_death_addr) Sleep(1000);
			}
			new_death_addr = new_death_addr + game[game_index].offset[game[game_index].offset_count-1];
			if (new_death_addr != death_addr)
			{
				death_addr = new_death_addr;
				printf("death count address: 0x%llX\n", death_addr);
			}
			else 
			{
				Sleep(200);
			}
		}

		ReadProcessMemory(process_handle, (void*)(death_addr), &new_deaths, sizeof(deaths), 0);

		if (new_deaths != deaths)
		{
			deaths = new_deaths;
			sprintf(death_counter_string, "Deaths: %d", deaths);
			print_to_file(death_counter_string);
			printf("%s\n", death_counter_string);
			Sleep(10000);
		}

		if (process_ended(game[game_index].game_name, process_handle)) break;
		Sleep(200);
	}
	Sleep(10000);
	goto start;

	return 0;
}
