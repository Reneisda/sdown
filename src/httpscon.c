//
// Created by Rene on 21/12/2024.
//

#include "httpscon.h"
#include <stdint.h>
#include <stdio.h>
#include "httpcon.h"

#pragma comment(lib, "ws2_32.lib")


int download_https(const char* url) {
    if (url == NULL) {
        return 1;
    }

    int sock;
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
    if (get_address_by_hostname(&result, hostname, "443", &addr, &hints) != 0) {
        printf("Failed to resolve hostname.\n");
        WSACleanup();
        return 2;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN * sizeof(char));
    printf("Host Resolution: %s\n", ip);
    if (connect(client, (struct sockaddr*)&addr, INET_ADDRSTRLEN) < 0) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 3;
    }
    // sending header
    char request_header[BUFFER_SIZE] = "GET /";
    char reply_header[BUFFER_SIZE];

    // getting size of file
    sprintf(request_header, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    printf("Sending Header: %s\n", request_header);

    if (send(client, request_header, (int) strlen(request_header), 0) == SOCKET_ERROR) {
        printf("Error sending header: %d\n", WSAGetLastError());
        closesocket(client);
        WSACleanup();
        return 4;
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
    printf("%s\n", reply_header);
    size_t file_length;
    if ((file_length = parse_data_length(reply_header)) == -1) {
        fprintf(stderr, "Failed to get Filesize\n");
        return 5;
    }
    printf("Length: %zu\n", file_length);
    return 0;
}