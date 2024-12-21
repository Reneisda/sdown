//
// Created by Rene on 21/12/2024.
//

#include "httpscon.h"
#include <stdint.h>
#include <stdio.h>
#include "httpcon.h"


int download_https(const char* url) {
    if (url == NULL) {
        return 1;
    }
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

}