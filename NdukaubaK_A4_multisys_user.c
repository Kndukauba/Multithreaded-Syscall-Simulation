/*
* Operating Systems
* Date: April 29, 2026
* Description: Simulated User Process (User Side)
*/

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFSIZE 256
#define MANAGER_PIPE "\\\\.\\pipe\\KernelManager"
#define MAX_MESSAGE_SIZE 256

typedef struct {
	int functionCode;
	char parameter[200];
} Message;

void send_message(HANDLE pipe, Message* msg);
int receive_message(HANDLE pipe, Message* msg);
void operational_phase(char* pipename);
void init_phase(void);

void send_message(HANDLE pipe, Message* msg) {
	DWORD bytesWritten = 0;
	if (!WriteFile(pipe, msg, sizeof(Message), &bytesWritten, NULL)) {
		printf("Error writing to pipe: %lu\n", GetLastError());
		return;
	}
	if (bytesWritten != sizeof(Message)) {
		printf("Partial write to pipe: %lu bytes\n", (unsigned long)bytesWritten);
	}
}

int receive_message(HANDLE pipe, Message* msg) {
	DWORD bytesRead = 0;
	if (!ReadFile(pipe, msg, sizeof(Message), &bytesRead, NULL)) {
		DWORD err = GetLastError();

		if (err == ERROR_BROKEN_PIPE) {
			printf("Server closed the connection (broken pipe)\n");
		}
		else {
			printf("Error reading from pipe: %lu\n", err);
		}
		return 0;
	}
	if (bytesRead != sizeof(Message)) {
		printf("Partial/invalid message read: %lu bytes\n", (unsigned long)bytesRead);
		return 0;
	}
	return 1;
}

void operational_phase(char* pipename) {
	HANDLE hPipe = INVALID_HANDLE_VALUE;
	Message msg;
	int choice = 1;
	int retry_count = 0;
	const int MAX_RETRIES = 5;

	printf("\nConnecting to kernel service pipe: %s\n", pipename);

	while (retry_count < MAX_RETRIES) {
		hPipe = CreateFileA(
			pipename,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (hPipe != INVALID_HANDLE_VALUE) {
			break;
		}

		DWORD err = GetLastError();
		if (err == ERROR_PIPE_BUSY) {
			printf("Pipe busy, waiting...\n");
			if (!WaitNamedPipeA(pipename, 2000)) {
				printf("Timeout waiting for pipe\n");
			}
			retry_count++;
		}
		else {
			printf("Error connecting to pipe: %lu\n", err);
			return;
		}
	}

	if (hPipe == INVALID_HANDLE_VALUE) {
		printf("Failed to connect after %d retries\n", MAX_RETRIES);
		return;
	}

	printf("Connected to kernel successfully!\n\n");

	while (choice != 0) {
		printf("\n--- Client Operations Menu ---\n");
		printf("0: Exit Thread\n");
		printf("1: Get System Time\n");
		printf("2: Get Random Number\n");
		printf("3: Square a Number\n");
		printf("4: Convert String to Uppercase\n");
		printf("5: Get My Thread Info\n");
		printf("6: Get My Thread Data\n");
		printf("7: Set My Thread Data\n");
		printf("\nEnter choice: ");
		if (scanf("%d", &choice) != 1) {

			int ch;
			while ((ch = getchar()) != '\n' && ch != EOF) {}
			printf("Invalid input. Please enter a number.\n");
			choice = -1;
			continue;
		}
		getchar();

		if (choice < 0 || choice > 7) {
			printf("Invalid choice. Please try again.\n");
			continue;
		}

		msg.functionCode = choice;
		strcpy(msg.parameter, "");

		if (choice == 3) {
			printf("Enter a number to square: ");
			scanf("%s", msg.parameter);
			getchar();
		}
		else if (choice == 4) {
			printf("Enter string to convert to uppercase: ");
			fgets(msg.parameter, sizeof(msg.parameter) - 1, stdin);
			msg.parameter[strcspn(msg.parameter, "\n")] = '\0';
		}
		else if (choice == 7) {
			printf("Enter integer value to set: ");
			scanf("%s", msg.parameter);
			getchar();
		}

		send_message(hPipe, &msg);

		if (receive_message(hPipe, &msg)) {
			printf("\nServer Response: %s\n", msg.parameter);
		}
		else {
			printf("Connection lost. Exiting thread.\n");
			break;
		}

		if (choice == 0) {
			printf("Exiting thread and returning to main menu.\n");
			break;
		}
	}

	CloseHandle(hPipe);
}

void init_phase(void) {
	HANDLE hPipe = INVALID_HANDLE_VALUE;
	Message msg;
	int choice = 1;

	while (choice != 0) {
		printf("\n--- Kernel Manager Menu ---\n");
		printf("0: Exit Program\n");
		printf("10: Request User Access (Get Client Pipe)\n");
		printf("99: Request Server to Exit\n");
		printf("\nEnter choice: ");
		if (scanf("%d", &choice) != 1) {
			int ch;
			while ((ch = getchar()) != '\n' && ch != EOF) {}
			printf("Invalid input. Please enter a number.\n");
			choice = -1;
			continue;
		}
		getchar();

		if (choice != 0 && choice != 10 && choice != 99) {
			printf("Invalid choice. Please try again.\n");
			continue;
		}

		if (choice == 0) {
			printf("Exiting program.\n");
			break;
		}

		int pipe_retry = 0;
		while (pipe_retry < 3) {
			hPipe = CreateFileA(
				MANAGER_PIPE,
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (hPipe != INVALID_HANDLE_VALUE) {
				break;
			}

			DWORD err = GetLastError();
			if (err == ERROR_FILE_NOT_FOUND) {
				printf("Error connecting to kernel manager: Server not running\n");
				break;
			}
			else if (err == ERROR_PIPE_BUSY) {
				printf("Manager pipe busy, waiting...\n");
				WaitNamedPipeA(MANAGER_PIPE, 2000);
				pipe_retry++;
			}
			else {
				printf("Error connecting: %lu\n", err);
				break;
			}
		}

		if (hPipe == INVALID_HANDLE_VALUE) {
			printf("Is the kernel server running?\n");
			continue;
		}

		msg.functionCode = choice;
		strcpy(msg.parameter, "");
		send_message(hPipe, &msg);

		if (receive_message(hPipe, &msg)) {
			printf("\nServer Response: %s\n", msg.parameter);

			if (choice == 10) {
				if (strlen(msg.parameter) > 0) {
					CloseHandle(hPipe);
					operational_phase(msg.parameter);
					continue;
				}
			}
			else if (choice == 99) {
				printf("Server is exiting. Program will terminate.\n");
				CloseHandle(hPipe);
				break;
			}
		}

		CloseHandle(hPipe);
	}
}

int main(void) {
	printf("========================================\n");
	printf("Assignment 4 - User Client\n");
	printf("Multithreaded Syscall Simulation\n");
	printf("========================================\n");

	init_phase();

	printf("\nClient terminated.\n");
	return 0;
}