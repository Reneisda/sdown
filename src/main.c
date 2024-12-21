
#include<stdio.h>
#include "httpcon.h"
#include "httpscon.h"
#pragma comment(lib, "ws2_32")


int main() {
    char* url = "https://ash-speed.hetzner.com/100MB.bin";
    //download_https(url);

    download_file_threaded(url, 20);
    return 0;
}
