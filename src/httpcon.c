//
// Created by Rene on 16/12/2024.
//

#include "httpcon.h"
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <dirent.h>
#include <unistd.h>

DWORD WINAPI download(void* vargp);

progress_t p;


void drop_http_prefix(char* new_url, const char *url) {

    if (strcmp(url, "https://") > 0) {
        strcpy(new_url, url + 8);
        return;
    }
    else if (strcmp(url, "http://") > 0) {
        strcpy(new_url, url + 7);
        return;
    }
    strcpy(new_url, url);
}

void get_hostname(char* server_name, const char *url) {
    drop_http_prefix(server_name, url);
    for (int i = 0; i < strlen(server_name); ++i) {
        if (server_name[i] == '/')
            server_name[i] = '\0';
    }
}

void get_path(char* path, const char *url) {
    drop_http_prefix(path, url);
    for (int i = 0; i < strlen(path); ++i) {
        if (path[i] == '/') {
            strcpy(path, path + i);
            break;
        }
    }
}

void get_filename(char* filename, const char *url) {
    for (size_t i = strlen(url); i > 0; --i) {
        if (url[i] == '/') {
            strcpy(filename, url + i + 1);
            return;
        }
    }
}

uint8_t check_header_end(const char* end) {
    return (end[0] == '\n' && end[1] == '\r' && end[2] == '\n');
}

size_t parse_data_length(const char* header) {
    char* index = strstr(header, "Content-Length: ");
    if (index == NULL)
        return -1;

    char* end = strstr(index, "\r");
    if (end == NULL)
        return -1;

    return strtoll(index + 16, &end, 10);

}

int get_address_by_hostname(struct addrinfo* result, const char* host, const char* port, struct sockaddr_in* addr, struct addrinfo* hints) {
    int ret = getaddrinfo(host, port, hints, &result);
    if (ret != 0) {
        return 1; // Failed to resolve host
    }

    struct sockaddr_in* resolved = (struct sockaddr_in*)result->ai_addr;
    memcpy(addr, resolved, sizeof(struct sockaddr_in));

    freeaddrinfo(result);
    return 0; // Success
}


int is_part_file(char* filename) {
    size_t len = strlen(filename);
    return len > 5 && strcmp(filename + len - 5, ".part") == 0;
}

int concat_part_files(char* directory, char* filename) {
    FILE *main_file = fopen(filename, "wb");
    DIR *d;

    uint8_t buffer[BUFFER_SIZE];
    struct dirent *dir;
    d = opendir(directory);
    FILE *file_to_concat;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (!is_part_file(dir->d_name))
                continue;

            file_to_concat = fopen(dir->d_name, "rb");
            size_t bytes_read;
            while ((bytes_read = fread(buffer, sizeof(uint8_t), BUFFER_SIZE, file_to_concat)) > 0) {
                fwrite(buffer, sizeof(uint8_t), bytes_read, main_file);
            }
            fclose(file_to_concat);
            remove(dir->d_name);
        }
        closedir(d);
    }

    fclose(main_file);
    return(0);
}



int download_file(const char *url) {
    WSADATA WSAData;
    SOCKET client;

    char filename[260];
    char path[2048];
    char hostname[260];

    get_filename(filename, url);
    get_path(path, url);
    get_hostname(hostname, url);

    WSAStartup(MAKEWORD(2,0), &WSAData);
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        return 1;
    }

    struct addrinfo result;
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;                      // IPv4
    hints.ai_socktype = SOCK_STREAM;                // TCP socket

    struct sockaddr_in addr;
    if (get_address_by_hostname(&result, hostname, "80", &addr, &hints) != 0) {
        printf("Failed to resolve hostname.\n");
        WSACleanup();
        return 1;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN * sizeof(char));
    printf("Host Resolution: %s\n", ip);

    if (connect(client, (struct sockaddr*)&addr, INET_ADDRSTRLEN) < 0) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 1;
    }
    // sending header
    char message_header[BUFFER_SIZE] = "GET /";
    char reply_header[BUFFER_SIZE];

    // getting size of file
    sprintf(message_header, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, hostname);
    printf("Sending Header: %s\n", message_header);

    if (send(client, message_header, (int) strlen(message_header), 0) == SOCKET_ERROR) {
        printf("Error sending header: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 1;
    }

    uint8_t found_end = 0;
    char chars[2] = {'\r', '\n'};
    int all = 0;
    uint8_t toggle = 0;
    while (found_end != 4 && all < BUFFER_SIZE) {
        (void) recv(client, reply_header + all, 1, 0);
        if (reply_header[all++] == chars[toggle]) {
            ++found_end;
            toggle ^= 1;
        }
        else {
            found_end = 0;
            toggle = 0;
        }
    }

    reply_header[all - 4] = '\0';
    //printf("%s\n", reply_header);
    size_t file_length;
    if ((file_length = parse_data_length(reply_header)) == -1) {
        fprintf(stderr, "Failed to get Filesize\n");
        return -1;
    }
    printf("REP: %s\n", reply_header);
}


int download_file_threaded(const char *url, int threads) {
    WSADATA WSAData;
    SOCKET client;
    p.current_all = 0;
    p.size = 0;
    p.current = (size_t*) calloc(threads, sizeof(size_t));

    char filename[260];
    char path[2048];
    char hostname[260];

    get_filename(filename, url);
    get_path(path, url);
    get_hostname(hostname, url);

    WSAStartup(MAKEWORD(2,0), &WSAData);
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        return 1;
    }

    struct addrinfo result;
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;                      // IPv4
    hints.ai_socktype = SOCK_STREAM;                // TCP socket

    struct sockaddr_in addr;
    if (get_address_by_hostname(&result, hostname, "80", &addr, &hints) != 0) {
        printf("Failed to resolve hostname.\n");
        WSACleanup();
        return 1;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN * sizeof(char));
    printf("Host Resolution: %s\n", ip);

    if (connect(client, (struct sockaddr*)&addr, INET_ADDRSTRLEN) < 0) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 1;
    }
    // sending header
    char message_header[BUFFER_SIZE] = "GET /";
    char reply_header[BUFFER_SIZE];

    // getting size of file
    sprintf(message_header, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, hostname);
    printf("Sending Header: %s\n", message_header);

    if (send(client, message_header, (int) strlen(message_header), 0) == SOCKET_ERROR) {
        printf("Error sending header: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 1;
    }

    uint8_t found_end = 0;
    char chars[2] = {'\r', '\n'};
    int all = 0;
    register uint8_t toggle = 0;
    while (found_end != 4 && all < BUFFER_SIZE) {
        (void) recv(client, reply_header + all, 1, 0);
        if (reply_header[all++] == chars[toggle]) {
            ++found_end;
            toggle ^= 1;
        }
        else {
            found_end = 0;
            toggle = 0;
        }
    }

    reply_header[all - 4] = '\0';
    //printf("%s\n", reply_header);
    size_t file_length;
    if ((file_length = parse_data_length(reply_header)) == -1) {
        fprintf(stderr, "Failed to get Filesize\n");
        return -1;
    }
    size_t bytes_per_thread = file_length / threads;
    p.size = file_length;

    d_args_t* args = (d_args_t*) malloc(sizeof(d_args_t) * threads);
    HANDLE* thread = (HANDLE*) malloc(sizeof(HANDLE) * threads);
    int i;
    for (i = 0; i < threads - 1; ++i) {
        args[i].length = bytes_per_thread;
        args[i].addr = &addr;
        args[i].start = bytes_per_thread * i;
        args[i].index = i;
        args[i].path = path;
        args[i].hostname = hostname;
        thread[i] = CreateThread(NULL, BUFFER_SIZE * 3, download, args + i, CREATE_SUSPENDED, NULL);
        ResumeThread(thread[i]);
    }

    args[i].length = file_length - (bytes_per_thread * (threads - 1));
    args[i].addr = &addr;
    args[i].start = bytes_per_thread * i;
    args[i].index = i;
    args[i].path = path;
    args[i].hostname = hostname;
    thread[i] = CreateThread(NULL, BUFFER_SIZE * 3, download, args + i, CREATE_SUSPENDED, NULL);
    ResumeThread(thread[i]);

    while (p.current_all < file_length) {
        p.current_all = 0;
        for (int j = 0; j < threads; ++j) {
            p.current_all += p.current[j];
        }
        printf("\r%zu%%", (p.current_all * 100) / p.size);
        fflush(stdout);
        sleep(1);
    }

    printf("\n");

    WaitForMultipleObjects(threads, thread, TRUE, INFINITE);

    for (i = 0; i < threads; ++i) {
        CloseHandle(thread[i]);
    }

    free(args);
    concat_part_files(".", filename);

    printf("Done Downloading\n");
    return 0;
}

DWORD WINAPI download(void* vargp) {
    WSADATA WSAData;
    SOCKET client;

    WSAStartup(MAKEWORD(2,0), &WSAData);
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        return 1;
    }
    if (connect(client, (struct sockaddr*) (((d_args_t*) vargp)->addr), INET_ADDRSTRLEN) < 0) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 1;
    }

    char message_header[BUFFER_SIZE] = "GET /";
    char reply_header[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    // getting size of file
    sprintf(message_header, "GET %s HTTP/1.0\r\nHost: %s\r\nRange: bytes=%d-\r\n\r\n",
            ((d_args_t*) vargp)->path, ((d_args_t*) vargp)->hostname, ((d_args_t*) vargp)->start);


    if (send(client, message_header, (int) strlen(message_header), 0) == SOCKET_ERROR) {
        printf("Error sending header: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 0;
    }

    uint8_t found_end = 0;
    char chars[2] = {'\r', '\n'};
    size_t all = 0;
    uint8_t toggle = 0;
    while (found_end != 4 && all < BUFFER_SIZE) {
        (void) recv(client, reply_header + all, 1, 0);
        if (reply_header[all++] == chars[toggle]) {
            ++found_end;
            toggle ^= 1;
        }
        else {
            found_end = 0;
            toggle = 0;
        }
    }
    char name[32];
    sprintf(name, "%d.part", ((d_args_t*) vargp)->index);
    FILE* f = fopen(name, "wb");

    size_t bytes_recv_all = 0;

    while (bytes_recv_all < ((d_args_t*) vargp)->length) {
        int bytes_received = recv(client, file_buffer, 1, 0);
        bytes_recv_all += bytes_received;
        p.current[((d_args_t*) vargp)->index] = bytes_recv_all;
        fwrite(file_buffer, bytes_received, 1, f);
    }
    p.current[((d_args_t*) vargp)->index] = bytes_recv_all;
    fclose(f);
    return 0;
}
