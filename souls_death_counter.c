#include <Windows.h>
#include <TlHelp32.h>

#include <stdio.h>
#include <stdint.h>

typedef uint8_t   b8;
typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct games
{
	char* game_name;
	int   offset[4];
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
		//printf("Checking process %s\n", process_info.szExeFile);
		if (memcmp(process_name, process_info.szExeFile, sizeof(process_name)) == 0)
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
		printf("%s has been closed.\n", process_name);
		return TRUE;
	}
	return FALSE;
}

int main(int argc, char** argv)
{
	u32   pid;
	u8*   base_address;
	u64   death_addr;
	u32   deaths = 0;
	u32   new_deaths = 0;
	void* process_handle = NULL;
	char* death_counter_string = calloc(128, sizeof(char));
	int   game_index;
	DWORD exit_code;
	games game[] = {{"DarkSoulsRemastered.exe", {0x1C8A530, 0x98}, 2}};
	

	while (1)
	{
		printf("looking for Souls game process...\n");

		do
		{
			for (int i = 0; i < sizeof(game) / sizeof(game[0]); i++)
			{
				pid = get_pid(game[i].game_name);
				if (pid)
				{
					game_index = i;
					printf("found %s.\n", game[i].game_name);
					break;
				}
			}
			if (!pid) Sleep(500);
		}
		while (!pid);

		printf("process id: %d\n", pid);

		do
		{
			base_address = get_base_address(game[game_index].game_name, pid);
			if (process_ended(game[game_index].game_name, process_handle)) break;
		}
		while (!base_address);
		process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		printf("base address: 0x%llX\n", (u64)base_address);

		do
		{
			ReadProcessMemory(process_handle, (void*)(base_address + game[game_index].offset[0]), &death_addr, sizeof(death_addr), 0);
			if (process_ended(game[game_index].game_name, process_handle)) break;
		}
		while (!death_addr);

		for (int i = 1; i < game[game_index].offset_count-1; i++)
		{
			ReadProcessMemory(process_handle, (void*)(death_addr + game[game_index].offset[i]), &death_addr, sizeof(death_addr), 0);
		}
		
		printf("death count address: 0x%llX\n", death_addr);
		sprintf(death_counter_string, "Deaths: %d", deaths);

		while (1)
		{
			ReadProcessMemory(process_handle, (void*)(death_addr + game[game_index].offset[game[game_index].offset_count-1]), &new_deaths, sizeof(deaths), 0);

			if (new_deaths > deaths)
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
		Sleep(3000);
	}

	return 0;
}
