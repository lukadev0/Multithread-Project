#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

int main() {
    struct sockaddr_in address;
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    int book_id;
    char user[100];

    printf("Enter book ID to reserve: ");
    scanf("%d", &book_id);
    printf("Enter your name: ");
    scanf("%s", user);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    snprintf(buffer, sizeof(buffer), "%d %s", book_id, user);
    send(sock, buffer, strlen(buffer), 0);
    read(sock, buffer, 1024);
    printf("%s", buffer);

    close(sock);
    return 0;
}
