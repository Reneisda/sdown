//
// Created by Rene on 16/12/2024.
//

#ifndef SDOWN_HTTPCON_H
#define SDOWN_HTTPCON_H

#include <ws2tcpip.h>
#include <stdint.h>

#define BUFFER_SIZE 8096

typedef struct d_args {
    uint32_t length;
    uint32_t start;
    uint32_t index;
    char* path;
    char* hostname;
    struct sockaddr_in* addr;
} d_args_t;

typedef struct progress {
    size_t size;
    size_t current_all;
    size_t* current;
} progress_t;


int get_address_by_hostname(struct addrinfo* result, const char* host, const char* port, struct sockaddr_in* addr, struct addrinfo* hints);
int download_file(const char *url);
int download_file_threaded(const char *url, int threads);
void drop_http_prefix(char* new_url, const char *url);
void get_hostname(char* server_name, const char *url);
void get_path(char* path, const char *url);
void get_filename(char* filename, const char *url);
size_t parse_data_length(const char* header);

#endif //SDOWN_HTTPCON_H
