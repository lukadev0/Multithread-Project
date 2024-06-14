#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 10

sqlite3 *db;
pthread_mutex_t db_mutex;

void *handle_client(void *arg);
void init_db();
void close_db();

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id[MAX_CLIENTS];
    int client_count = 0;

    init_db();

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
        if (pthread_create(&thread_id[client_count++], NULL, handle_client, (void *)&new_socket) != 0) {
            perror("pthread_create");
        }
    }

    for (int i = 0; i < client_count; i++) {
        pthread_join(thread_id[i], NULL);
    }

    close_db();
    return 0;
}

void init_db() {
    pthread_mutex_init(&db_mutex, NULL);
    if (sqlite3_open("library.db", &db)) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    char *err_msg = 0;
    char *sql = "CREATE TABLE IF NOT EXISTS books ("
                "id INTEGER PRIMARY KEY, "
                "title TEXT NOT NULL, "
                "author TEXT NOT NULL, "
                "available INTEGER NOT NULL);"
                "CREATE TABLE IF NOT EXISTS reservations ("
                "id INTEGER PRIMARY KEY, "
                "book_id INTEGER, "
                "user TEXT, "
                "FOREIGN KEY (book_id) REFERENCES books (id));";

    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
}

void close_db() {
    sqlite3_close(db);
    pthread_mutex_destroy(&db_mutex);
}

void *handle_client(void *arg) {
    int socket = *(int *)arg;
    char buffer[1024] = {0};
    char *response;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(socket, buffer, 1024);
        if (bytes_read <= 0) {
            close(socket);
            return NULL;
        }

        pthread_mutex_lock(&db_mutex);

        if (strncmp(buffer, "CHECK_AVAILABILITY", 18) == 0) {
            char *sql = "SELECT id, title, author, available FROM books;";
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
                response = strdup("");
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int id = sqlite3_column_int(stmt, 0);
                    const unsigned char *title = sqlite3_column_text(stmt, 1);
                    const unsigned char *author = sqlite3_column_text(stmt, 2);
                    int available = sqlite3_column_int(stmt, 3);
                    char temp[256];
                    snprintf(temp, sizeof(temp), "ID: %d, Title: %s, Author: %s, Available: %d\n", id, title, author, available);
                    response = realloc(response, strlen(response) + strlen(temp) + 1);
                    strcat(response, temp);
                }
                sqlite3_finalize(stmt);
            } else {
                response = strdup("Failed to execute query.\n");
            }
        } else if (strncmp(buffer, "RESERVE_BOOK", 12) == 0) {
            int book_id;
            char user[50];
            sscanf(buffer + 13, "%d %49s", &book_id, user);
            char *sql = sqlite3_mprintf("UPDATE books SET available = available - 1 WHERE id = %d AND available > 0;", book_id);
            char *err_msg = 0;
            if (sqlite3_exec(db, sql, 0, 0, &err_msg) == SQLITE_OK && sqlite3_changes(db) > 0) {
                char *sql_res = sqlite3_mprintf("INSERT INTO reservations (book_id, user) VALUES (%d, %Q);", book_id, user);
                if (sqlite3_exec(db, sql_res, 0, 0, &err_msg) == SQLITE_OK) {
                    response = strdup("Book reserved successfully.\n");
                } else {
                    response = strdup("Failed to reserve book.\n");
                }
                sqlite3_free(sql_res);
            } else {
                response = strdup("Book not available or does not exist.\n");
            }
            sqlite3_free(sql);
        } else if (strncmp(buffer, "CANCEL_RESERVATION", 18) == 0) {
            int book_id;
            char user[50];
            sscanf(buffer + 19, "%d %49s", &book_id, user);
            char *sql = sqlite3_mprintf("DELETE FROM reservations WHERE book_id = %d AND user = %Q;", book_id, user);
            char *err_msg = 0;
            if (sqlite3_exec(db, sql, 0, 0, &err_msg) == SQLITE_OK && sqlite3_changes(db) > 0) {
                char *sql_update = sqlite3_mprintf("UPDATE books SET available = available + 1 WHERE id = %d;", book_id);
                if (sqlite3_exec(db, sql_update, 0, 0, &err_msg) == SQLITE_OK) {
                    response = strdup("Reservation cancelled successfully.\n");
                } else {
                    response = strdup("Failed to update book availability.\n");
                }
                sqlite3_free(sql_update);
            } else {
                response = strdup("Reservation not found.\n");
            }
            sqlite3_free(sql);
        } else {
            response = strdup("Invalid command.\n");
        }

        send(socket, response, strlen(response), 0);
        free(response);

        pthread_mutex_unlock(&db_mutex);
    }

    close(socket);
    return NULL;
}

