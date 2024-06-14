#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void print_menu() {
    printf("Menu:\n");
    printf("1. Check availability\n");
    printf("2. Reserve a book\n");
    printf("3. Cancel a reservation\n");
    printf("4. Exit\n");
    printf("Enter your choice: ");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

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

    int choice;
    while (1) {
        print_menu();
        scanf("%d", &choice);

        if (choice == 1) {
            char *message = "CHECK_AVAILABILITY";
            send(sock, message, strlen(message), 0);
        } else if (choice == 2) {
            int book_id;
            char user[50];
            printf("Enter book ID to reserve: ");
            scanf("%d", &book_id);
            printf("Enter your name: ");
            scanf("%s", user);
            char message[100];
            snprintf(message, sizeof(message), "RESERVE_BOOK %d %s", book_id, user);
            send(sock, message, strlen(message), 0);
        } else if (choice == 3) {
            int book_id;
            char user[50];
            printf("Enter book ID to cancel reservation: ");
            scanf("%d", &book_id);
            printf("Enter your name: ");
            scanf("%s", user);
            char message[100];
            snprintf(message, sizeof(message), "CANCEL_RESERVATION %d %s", book_id, user);
            send(sock, message, strlen(message), 0);
        } else if (choice == 4) {
            close(sock);
            printf("Goodbye!\n");
            break;
        } else {
            printf("Invalid choice, please try again.\n");
            continue;
        }

        read(sock, buffer, 1024);
        printf("%s\n", buffer);
        memset(buffer, 0, sizeof(buffer));
    }

    return 0;
}
