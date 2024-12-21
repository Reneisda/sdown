
#include<stdio.h>
#include "httpcon.h"

#pragma comment(lib, "ws2_32")


int main() {
    char* url = "https://media1.tenor.com/m/uk9xO0xpWoIAAAAd/burger-eating.gif";
    download_file(url);
    download_file_threaded(url, 10);
    return 0;
}
