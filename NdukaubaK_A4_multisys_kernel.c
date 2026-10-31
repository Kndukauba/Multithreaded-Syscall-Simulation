/*
* Operating Systems
* Date: April 29, 2026
* Description: Simulated Kernel Process (Server Side) 
*/

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define BUFSIZE 256
#define MANAGER_PIPE "\\\\.\\pipe\\KernelManager"
#define MAX_CLIENTS 10
#define MAX_MESSAGE_SIZE 256

typedef struct {
	int functionCode;
	char parameter[200];
} Message;

typedef struct {
	int cdbid;
	char pipename[100];
	int data;
	HANDLE thread_handle;
} ClientDataBlock;

ClientDataBlock CDB_Table[MAX_CLIENTS] = { 0 };
HANDLE g_mutex = NULL;
int g_server_exit = 0;
int g_next_client_id = 0;

void initialize_cdb_table(void);
int allocate_cdb(void);
void free_cdb(int cdbid);
void handle_manager_request(Message* request, Message* response);
int process_client_operation(int cdbid, Message* request, Message* response);
DWORD WINAPI client_thread_wrapper(LPVOID param);

void initialize_cdb_table(void) {
	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		CDB_Table[i].cdbid = -1;
		strcpy(CDB_Table[i].pipename, "");
		CDB_Table[i].data = 0;
		CDB_Table[i].thread_handle = NULL;
	}
}

int allocate_cdb(void) {
	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (CDB_Table[i].cdbid == -1) {
			CDB_Table[i].cdbid = g_next_client_id++;
			sprintf(CDB_Table[i].pipename, "\\\\.\\pipe\\Client_%d", CDB_Table[i].cdbid);
			CDB_Table[i].data = rand() % 1001;
			CDB_Table[i].thread_handle = NULL;
			return i;
		}
	}
	return -1;
}

void free_cdb(int cdbid) {
	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (CDB_Table[i].cdbid == cdbid) {
			CDB_Table[i].cdbid = -1;
			strcpy(CDB_Table[i].pipename, "");
			CDB_Table[i].data = 0;
			if (CDB_Table[i].thread_handle != NULL) {
				CloseHandle(CDB_Table[i].thread_handle);
			}
			CDB_Table[i].thread_handle = NULL;
			break;
		}
	}
}

int process_client_operation(int cdbid, Message* request, Message* response) {
	int cdb_index = -1;
	int i;
	time_t now;
	struct tm* timeinfo;
	char time_buffer[100];
	int num, squared;
	int value;

	WaitForSingleObject(g_mutex, INFINITE);
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (CDB_Table[i].cdbid == cdbid) {
			cdb_index = i;
			break;
		}
	}
	ReleaseMutex(g_mutex);

	if (cdb_index == -1) {
		sprintf(response->parameter, "Error: Invalid client ID");
		return 0;
	}

	response->functionCode = request->functionCode;

	switch (request->functionCode) {
	case 0:
		sprintf(response->parameter, "Exiting client thread %d", cdbid);
		WaitForSingleObject(g_mutex, INFINITE);
		free_cdb(cdbid);
		ReleaseMutex(g_mutex);
		return 0;

	case 1:
		time(&now);
		timeinfo = localtime(&now);
		strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
		sprintf(response->parameter, "Current time: %s", time_buffer);
		break;

	case 2:
		sprintf(response->parameter, "Random number: %d", rand() % 100);
		break;

	case 3:
		num = atoi(request->parameter);
		squared = num * num;
		sprintf(response->parameter, "%d squared = %d", num, squared);
		break;

	case 4:
		strcpy(response->parameter, request->parameter);
		for (i = 0; response->parameter[i]; i++) {
			response->parameter[i] = toupper((unsigned char)response->parameter[i]);
		}
		break;

	case 5:
		sprintf(response->parameter,
			"Client ID: %d, Thread Handle: %p, Data: %d",
			cdbid, CDB_Table[cdb_index].thread_handle, CDB_Table[cdb_index].data);
		break;

	case 6:
		WaitForSingleObject(g_mutex, INFINITE);
		sprintf(response->parameter, "Your thread data: %d", CDB_Table[cdb_index].data);
		ReleaseMutex(g_mutex);
		break;

	case 7:
		value = atoi(request->parameter);
		WaitForSingleObject(g_mutex, INFINITE);
		CDB_Table[cdb_index].data = value;
		sprintf(response->parameter, "Thread data set to: %d", value);
		ReleaseMutex(g_mutex);
		break;

	default:
		sprintf(response->parameter, "Unknown operation code: %d", request->functionCode);
		break;
	}

	return 1;
}

DWORD WINAPI client_thread_wrapper(LPVOID param) {
	int cdb_index = (int)(intptr_t)param;
	int cdbid;
	char pipename[100];
	HANDLE hPipe = INVALID_HANDLE_VALUE;
	Message request, response;
	DWORD bytesRead = 0, bytesWritten = 0;
	int continue_thread = 1;

	WaitForSingleObject(g_mutex, INFINITE);
	cdbid = CDB_Table[cdb_index].cdbid;
	strcpy(pipename, CDB_Table[cdb_index].pipename);
	ReleaseMutex(g_mutex);

	printf("Thread %d started, creating pipe: %s\n", cdbid, pipename);

	hPipe = CreateNamedPipeA(
		pipename,
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		1,
		BUFSIZE,
		BUFSIZE,
		0,
		NULL);

	if (hPipe == INVALID_HANDLE_VALUE) {
		printf("Error creating pipe for client %d: %lu\n", cdbid, GetLastError());
		WaitForSingleObject(g_mutex, INFINITE);
		free_cdb(cdbid);
		ReleaseMutex(g_mutex);
		return 1;
	}

	printf("Client %d pipe created, waiting for connection...\n", cdbid);

	if (!ConnectNamedPipe(hPipe, NULL)) {
		DWORD err = GetLastError();

		if (err != ERROR_PIPE_CONNECTED) {
			printf("Error connecting to pipe for client %d: %lu\n", cdbid, err);
			CloseHandle(hPipe);
			WaitForSingleObject(g_mutex, INFINITE);
			free_cdb(cdbid);
			ReleaseMutex(g_mutex);
			return 1;
		}
	}

	printf("Client %d connected!\n", cdbid);

	while (continue_thread && !g_server_exit) {
		if (!ReadFile(hPipe, &request, sizeof(Message), &bytesRead, NULL)) {
			DWORD error = GetLastError();
			if (error != ERROR_BROKEN_PIPE) {
				printf("Error reading from pipe for client %d: %lu\n", cdbid, error);
			}
			break;
		}

		if (bytesRead != sizeof(Message)) {
			printf("Partial/invalid read from client %d: %lu bytes\n", cdbid, (unsigned long)bytesRead);
			break;
		}

		continue_thread = process_client_operation(cdbid, &request, &response);

		if (!WriteFile(hPipe, &response, sizeof(Message), &bytesWritten, NULL)) {
			printf("Error writing to pipe for client %d: %lu\n", cdbid, GetLastError());
			break;
		}

		if (bytesWritten != sizeof(Message)) {
			printf("Partial write to client %d: %lu bytes\n", cdbid, (unsigned long)bytesWritten);
			break;
		}
	}

	printf("Client %d disconnecting...\n", cdbid);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);

	WaitForSingleObject(g_mutex, INFINITE);
	free_cdb(cdbid);
	ReleaseMutex(g_mutex);

	printf("Client %d thread exiting\n", cdbid);
	return 0;
}

void handle_manager_request(Message* request, Message* response) {
	int cdb_index;
	HANDLE hThread;
	DWORD thread_id;

	response->functionCode = request->functionCode;

	switch (request->functionCode) {
	case 10:
		WaitForSingleObject(g_mutex, INFINITE);
		cdb_index = allocate_cdb();
		if (cdb_index == -1) {
			strcpy(response->parameter, "Error: No available client slots");
			ReleaseMutex(g_mutex);
		}
		else {
			int cdbid = CDB_Table[cdb_index].cdbid;
			strcpy(response->parameter, CDB_Table[cdb_index].pipename);
			printf("Allocated client slot %d with pipe: %s\n", cdbid, response->parameter);

			hThread = CreateThread(
				NULL,
				0,
				client_thread_wrapper,
				(LPVOID)(intptr_t)cdb_index,
				0,
				&thread_id);

			if (hThread == NULL) {
				printf("Error creating thread for client %d: %lu\n", cdbid, GetLastError());
				free_cdb(cdbid);
				strcpy(response->parameter, "Error: Could not create thread");
			}
			else {
				CDB_Table[cdb_index].thread_handle = hThread;
				printf("Thread %lu created for client %d\n", thread_id, cdbid);
			}
			ReleaseMutex(g_mutex);
		}
		break;

	case 99:
		sprintf(response->parameter, "Server shutting down");
		g_server_exit = 1;
		break;

	default:
		sprintf(response->parameter, "Unknown function code: %d", request->functionCode);
		break;
	}
}

int main(void) {
	HANDLE hManagerPipe;
	Message request, response;
	DWORD bytesRead, bytesWritten;
	int i;

	printf("========================================\n");
	printf("Assignment 4 - Kernel Server\n");
	printf("Multithreaded Syscall Simulation\n");
	printf("========================================\n\n");

	srand((unsigned int)time(0));

	initialize_cdb_table();

	g_mutex = CreateMutexA(NULL, FALSE, NULL);
	if (g_mutex == NULL) {
		printf("Error creating mutex: %lu\n", GetLastError());
		return 1;
	}

	hManagerPipe = CreateNamedPipeA(
		MANAGER_PIPE,
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES,
		BUFSIZE,
		BUFSIZE,
		0,
		NULL);

	if (hManagerPipe == INVALID_HANDLE_VALUE) {
		printf("Error creating manager pipe: %lu\n", GetLastError());
		CloseHandle(g_mutex);
		return 1;
	}

	printf("Kernel server started, listening on manager pipe...\n");
	printf("Max clients supported: %d\n\n", MAX_CLIENTS);

	while (!g_server_exit) {
		printf("Waiting for manager requests...\n");

		if (!ConnectNamedPipe(hManagerPipe, NULL)) {
			DWORD err = GetLastError();
			if (err != ERROR_PIPE_CONNECTED) {
				printf("Error connecting to manager pipe: %lu\n", err);
				Sleep(100);
				continue;
			}
		}

		if (!ReadFile(hManagerPipe, &request, sizeof(Message), &bytesRead, NULL)) {
			printf("Error reading from manager pipe: %lu\n", GetLastError());
			DisconnectNamedPipe(hManagerPipe);
			continue;
		}

		if (bytesRead != sizeof(Message)) {
			printf("Partial/invalid manager request read: %lu bytes\n", (unsigned long)bytesRead);
			DisconnectNamedPipe(hManagerPipe);
			continue;
		}

		printf("Manager request received: function code %d\n", request.functionCode);

		handle_manager_request(&request, &response);

		if (!WriteFile(hManagerPipe, &response, sizeof(Message), &bytesWritten, NULL)) {
			printf("Error writing to manager pipe: %lu\n", GetLastError());
		}
		else if (bytesWritten != sizeof(Message)) {
			printf("Partial write to manager pipe: %lu bytes\n", (unsigned long)bytesWritten);
		}

		DisconnectNamedPipe(hManagerPipe);

		if (g_server_exit) {
			printf("Server exit requested, cleaning up...\n");
			break;
		}
	}

	printf("\nWaiting for all client threads to terminate...\n");
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (CDB_Table[i].cdbid != -1 && CDB_Table[i].thread_handle != NULL) {
			WaitForSingleObject(CDB_Table[i].thread_handle, INFINITE);
			CloseHandle(CDB_Table[i].thread_handle);
			CDB_Table[i].thread_handle = NULL;
		}
	}

	CloseHandle(hManagerPipe);
	CloseHandle(g_mutex);

	printf("Kernel server terminated.\n");
	return 0;
}