#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAX_CLIENTS 10

sqlite3 *db;

void *handle_client(void *arg);
int reserve_book(int book_id, const char *user);

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    pthread_t tid[MAX_CLIENTS];
    int client_count = 0;

    // Initialize SQLite
    if (sqlite3_open("library.db", &db)) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
        if (client_count < MAX_CLIENTS) {
            pthread_create(&tid[client_count], NULL, handle_client, (void *)&new_socket);
            client_count++;
        } else {
            printf("Max clients reached. Rejecting client...\n");
            close(new_socket);
        }
    }

    sqlite3_close(db);
    return 0;
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[1024] = {0};
    int book_id;
    char user[100];

    read(client_socket, buffer, 1024);
    sscanf(buffer, "%d %s", &book_id, user);

    if (reserve_book(book_id, user)) {
        char *message = "Book reserved successfully.\n";
        send(client_socket, message, strlen(message), 0);
    } else {
        char *message = "Failed to reserve book.\n";
        send(client_socket, message, strlen(message), 0);
    }

    close(client_socket);
    return NULL;
}

int reserve_book(int book_id, const char *user) {
    char *err_msg = 0;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT available FROM books WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch book: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, book_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int available = sqlite3_column_int(stmt, 0);

        if (available > 0) {
            sqlite3_finalize(stmt);

            const char *sql_update = "UPDATE books SET available = available - 1 WHERE id = ?";
            const char *sql_insert = "INSERT INTO reservations (book_id, user) VALUES (?, ?)";

            sqlite3_exec(db, "BEGIN TRANSACTION", 0, 0, 0);

            sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0);
            sqlite3_bind_int(stmt, 1, book_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
            sqlite3_bind_int(stmt, 1, book_id);
            sqlite3_bind_text(stmt, 2, user, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            sqlite3_exec(db, "COMMIT", 0, 0, 0);

            return 1;
        }
    }

    sqlite3_finalize(stmt);
    return 0;
}
