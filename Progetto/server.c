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

typedef struct {
    int socket;
    char username[100];
} ClientContext;  

void *handle_client(void *arg);
void init_db();
void close_db();
void register_user(int client_socket, char *username, char *password);
void login_user(int client_socket, char *username, char *password);
void view_books(int client_socket);
void send_response(int client_socket, const char *message);

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

    printf("Server listening on port %d...\n", PORT);

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
        pthread_t thread;
        ClientContext *client_context = malloc(sizeof(ClientContext));
        client_context->socket = new_socket;
        strcpy(client_context->username, ""); // Inizialmente nessun utente è autenticato

        if (pthread_create(&thread, NULL, handle_client, (void *)client_context) != 0) {
            perror("pthread_create");
        }
        thread_id[client_count++] = thread;
    }

    for (int i = 0; i < client_count; i++) {
        pthread_join(thread_id[i], NULL);
    }

    close_db();
    return 0;
}

void init_db() {
    pthread_mutex_init(&db_mutex, NULL);
    if (sqlite3_open("database.db", &db)) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    char *err_msg = 0;
    char *sql = "CREATE TABLE IF NOT EXISTS utente ("
                "matricola INTEGER PRIMARY KEY AUTOINCREMENT, "
                "nome TEXT NOT NULL, "
                "username TEXT UNIQUE NOT NULL, "
                "password TEXT NOT NULL"
                ");"
                "CREATE TABLE IF NOT EXISTS libro ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "titolo TEXT NOT NULL, "
                "autore TEXT NOT NULL, "
                "categoria TEXT NOT NULL, "
                "anno_pubblicazione INTEGER, "
                "disponibilita INTEGER NOT NULL"
                ");"
                "CREATE TABLE IF NOT EXISTS prenotazione ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "matricola INTEGER, "
                "id_libro INTEGER, "
                "FOREIGN KEY (matricola) REFERENCES utente(matricola), "
                "FOREIGN KEY (id_libro) REFERENCES libro(id)"
                ");";

    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    printf("Database initialized successfully.\n");
}

void close_db() {
    sqlite3_close(db);
    pthread_mutex_destroy(&db_mutex);
}

void register_user(int client_socket, char *username, char *password) {
    pthread_mutex_lock(&db_mutex);

    char *sql = sqlite3_mprintf("INSERT INTO utente (nome, username, password) VALUES ('', '%q', '%q');", username, password);
    char *err_msg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        send_response(client_socket, "REGISTRATION_FAILURE");
    } else {
        printf("Registrazione utente avvenuta con successo!.\n");
        send_response(client_socket, "REGISTRATION_SUCCESS");
    }

    sqlite3_free(sql);
    pthread_mutex_unlock(&db_mutex);
}

void login_user(int client_socket, char *username, char *password) {
    pthread_mutex_lock(&db_mutex);

    char *sql = sqlite3_mprintf("SELECT * FROM utente WHERE username='%q' AND password='%q';", username, password);
    sqlite3_stmt *res;
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "LOGIN_FAILURE");
    } else {
        if (sqlite3_step(res) == SQLITE_ROW) {
            send_response(client_socket, "LOGIN_SUCCESS");
        } else {
            send_response(client_socket, "LOGIN_FAILURE");
        }
    }

    sqlite3_finalize(res);
    sqlite3_free(sql);
    pthread_mutex_unlock(&db_mutex);
}
void view_books(int client_socket) {
    pthread_mutex_lock(&db_mutex);

    const char *sql = "SELECT id, titolo, autore, categoria, anno_pubblicazione, disponibilita FROM libro WHERE disponibilita > 0;";
    sqlite3_stmt *res;
    char buffer[2048] = ""; // Dimensione del buffer aumentata per gestire più dati

    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "VIEW_BOOKS_FAILURE");
    } else {
        printf("DEBUG: Eseguendo la query per visualizzare i libri disponibili...\n");
        while (sqlite3_step(res) == SQLITE_ROW) {
            char row[256];
            snprintf(row, sizeof(row), "ID: %d, Titolo: %s, Autore: %s, Categoria: %s, Anno: %d, Disponibilità: %d\n",
                     sqlite3_column_int(res, 0),
                     sqlite3_column_text(res, 1),
                     sqlite3_column_text(res, 2),
                     sqlite3_column_text(res, 3),
                     sqlite3_column_int(res, 4),
                     sqlite3_column_int(res, 5));
            printf("DEBUG: Dati del libro: %s", row); // Messaggio di debug per ogni riga
            strncat(buffer, row, sizeof(buffer) - strlen(buffer) - 1);
        }

        if (strlen(buffer) == 0) {
            snprintf(buffer, sizeof(buffer), "Nessun libro disponibile.\n");
        }

        buffer[sizeof(buffer) - 1] = '\0'; // Assicuro che il buffer sia terminato correttamente
        send_response(client_socket, buffer);
        fflush(stdout); // Flush del buffer di output
    }

    sqlite3_finalize(res);
    pthread_mutex_unlock(&db_mutex);
}
void send_response(int client_socket, const char *data) {
    int data_len = strlen(data);
    int bytes_sent = 0;
    int total_bytes_sent = 0;

    while (total_bytes_sent < data_len) {
        bytes_sent = send(client_socket, data + total_bytes_sent, data_len - total_bytes_sent, 0);
        if (bytes_sent == -1) {
            perror("Error sending data to client");
            break;
        }
        total_bytes_sent += bytes_sent;
    }
}
int get_user_id(const char *username) {
    pthread_mutex_lock(&db_mutex);

    int user_id = -1;
    char *sql = sqlite3_mprintf("SELECT matricola FROM utente WHERE username = '%q';", username);
    sqlite3_stmt *res;
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW) {
        user_id = sqlite3_column_int(res, 0);
    }

    sqlite3_finalize(res);
    sqlite3_free(sql);
    pthread_mutex_unlock(&db_mutex);

    return user_id;
}

void handle_reserve_book(int client_socket, char *book_id_str, int user_id) {
    pthread_mutex_lock(&db_mutex);

    char *err_msg = 0;
    sqlite3_stmt *stmt;
    int book_id = atoi(book_id_str); // Converto l'ID del libro da stringa a intero

    // Verifico la disponibilità del libro
    char *sql_check_availability = "SELECT disponibilita FROM libro WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql_check_availability, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch book availability: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_bind_int(stmt, 1, book_id);

    int disponibilita = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        disponibilita = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (disponibilita <= 0) {
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    // Inizia la transazione per garantire coerenza tra le operazioni di inserimento e aggiornamento
    char *sql_begin_transaction = "BEGIN TRANSACTION;";
    rc = sqlite3_exec(db, sql_begin_transaction, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to begin transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    // Inserisco la prenotazione
    char *sql_insert_reservation = "INSERT INTO prenotazione (matricola, id_libro) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(db, sql_insert_reservation, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare insert reservation statement: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, book_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert reservation: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_finalize(stmt);

    // Aggiorno la disponibilità del libro
    char *sql_update_availability = "UPDATE libro SET disponibilita = disponibilita - 1 WHERE id = ?;";
    rc = sqlite3_prepare_v2(db, sql_update_availability, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare update availability statement: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_bind_int(stmt, 1, book_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to update availability: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_finalize(stmt);

    // Termino la transazione
    char *sql_commit_transaction = "COMMIT;";
    rc = sqlite3_exec(db, sql_commit_transaction, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to commit transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        send_response(client_socket, "RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    // Se tutte le operazioni sono avvenute con successo, invio il successo della prenotazione al client
    send_response(client_socket, "RESERVATION_SUCCESS");

    pthread_mutex_unlock(&db_mutex);
}
void view_my_reservations(int client_socket, const char *username) {
    pthread_mutex_lock(&db_mutex);

    // Preparazione della query SQL con un placeholder per l'username
    const char *sql = "SELECT p.id, l.titolo, l.autore, l.categoria, l.anno_pubblicazione "
                      "FROM prenotazione p "
                      "JOIN utente u ON p.matricola = u.matricola "
                      "JOIN libro l ON p.id_libro = l.id "
                      "WHERE u.username = ?;";

    sqlite3_stmt *res;
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "VIEW_MY_RESERVATIONS_FAILURE");
    } else {
        // Bind dell'username
        sqlite3_bind_text(res, 1, username, -1, SQLITE_STATIC);

        char buffer[2048] = ""; // Buffer per memorizzare i risultati
        while (sqlite3_step(res) == SQLITE_ROW) {
            char row[256];
            snprintf(row, sizeof(row),
                     "ID Prenotazione: %d, Titolo: %s, Autore: %s, Categoria: %s, Anno: %d\n",
                     sqlite3_column_int(res, 0),
                     sqlite3_column_text(res, 1),
                     sqlite3_column_text(res, 2),
                     sqlite3_column_text(res, 3),
                     sqlite3_column_int(res, 4));
            strncat(buffer, row, sizeof(buffer) - strlen(buffer) - 1);
        }

        if (strlen(buffer) == 0) {
            snprintf(buffer, sizeof(buffer), "\nNessuna prenotazione trovata.\n");
        }

        send_response(client_socket, buffer);
    }

    sqlite3_finalize(res);
    pthread_mutex_unlock(&db_mutex);
}
void cancel_reservation(int client_socket, int reservation_id) {
    pthread_mutex_lock(&db_mutex);

    char *err_msg = 0;
    sqlite3_stmt *stmt;
    int book_id = -1;

    // Inizia la transazione per garantire coerenza tra le operazioni di cancellazione e aggiornamento
    char *sql_begin_transaction = "BEGIN TRANSACTION;";
    int rc = sqlite3_exec(db, sql_begin_transaction, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to begin transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    // Recupero l'id del libro dalla prenotazione
    char *sql_get_book_id = "SELECT id_libro FROM prenotazione WHERE id = ?;";
    rc = sqlite3_prepare_v2(db, sql_get_book_id, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement to get book ID: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_bind_int(stmt, 1, reservation_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        book_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (book_id == -1) {
        // Se non troviamo il book_id, fallisce l'operazione
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    // Cancella la prenotazione
    char *sql_delete_reservation = "DELETE FROM prenotazione WHERE id = ?;";
    rc = sqlite3_prepare_v2(db, sql_delete_reservation, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare delete reservation statement: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_bind_int(stmt, 1, reservation_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to delete reservation: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_finalize(stmt);

    // Aggiorna la disponibilità del libro correlato
    char *sql_update_availability = "UPDATE libro SET disponibilita = disponibilita + 1 WHERE id = ?;";
    rc = sqlite3_prepare_v2(db, sql_update_availability, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare update availability statement: %s\n", sqlite3_errmsg(db));
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_bind_int(stmt, 1, book_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to update availability: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_finalize(stmt);

    // Termina la transazione
    char *sql_commit_transaction = "COMMIT;";
    rc = sqlite3_exec(db, sql_commit_transaction, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to commit transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        send_response(client_socket, "CANCEL_RESERVATION_FAILURE");
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    // Se tutte le operazioni sono avvenute con successo, invio il successo della cancellazione al client
    send_response(client_socket, "CANCEL_RESERVATION_SUCCESS");

    pthread_mutex_unlock(&db_mutex);
}



void *handle_client(void *arg) {
    ClientContext *client_context = (ClientContext *)arg;
    int client_socket = client_context->socket;
    char buffer[1024] = {0};
    int read_size;

    while ((read_size = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[read_size] = '\0';
        printf("DEBUG: Richiesta ricevuta dal client: %s\n", buffer);

        if (strncmp(buffer, "REGISTER", strlen("REGISTER")) == 0) {
            char username[100];
            char password[100];
            sscanf(buffer + strlen("REGISTER") + 1, "%s\n%s\n", username, password);
            register_user(client_socket, username, password);
        } else if (strncmp(buffer, "LOGIN", strlen("LOGIN")) == 0) {
            char username[100];
            char password[100];
            sscanf(buffer + strlen("LOGIN") + 1, "%s\n%s\n", username, password);
            login_user(client_socket, username, password);
            strcpy(client_context->username, username); // Imposta l'username nella sessione del client
        } else if (strncmp(buffer, "VIEW_BOOKS", strlen("VIEW_BOOKS")) == 0) {
            view_books(client_socket);
        } else if (strncmp(buffer, "RESERVE", strlen("RESERVE")) == 0) {
            char book_id[100];
            sscanf(buffer + strlen("RESERVE") + 1, "%s\n", book_id);

            if (strlen(client_context->username) > 0) {
                int user_id = get_user_id(client_context->username); // Funzione helper per ottenere l'ID utente
                handle_reserve_book(client_socket, book_id, user_id);
            } else {
                send_response(client_socket, "USER_NOT_LOGGED_IN");
            }
        } else if (strncmp(buffer, "VIEW_MY_RESERVATIONS", strlen("VIEW_MY_RESERVATIONS")) == 0) {
            if (strlen(client_context->username) > 0) {
                view_my_reservations(client_socket, client_context->username);
            } else {
                send_response(client_socket, "USER_NOT_LOGGED_IN");
            }
        } else if (strncmp(buffer, "CANCEL_RESERVATION", strlen("CANCEL_RESERVATION")) == 0) {
    if (strlen(client_context->username) > 0) {
        int reservation_id;
        sscanf(buffer + strlen("CANCEL_RESERVATION") + 1, "%d\n", &reservation_id);
        cancel_reservation(client_socket, reservation_id);
    } else {
        send_response(client_socket, "USER_NOT_LOGGED_IN");
    }
}
else {
            send_response(client_socket, "UNKNOWN_COMMAND");
        }

        memset(buffer, 0, sizeof(buffer));
    }

    if (read_size == 0) {
        printf("Client disconnected\n");
    } else if (read_size == -1) {
        perror("recv failed");
    }

    close(client_socket);
    free(client_context);
    return NULL;
}

