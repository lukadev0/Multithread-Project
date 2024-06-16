#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void show_menu();
void handle_registration(int socket);
void handle_login(int socket);
void handle_dashboard(int socket);
void handle_view_books(int socket);
void handle_reserve_book(int socket);
void handle_cancel_reservation(int socket);
void handle_view_reservations(int socket);

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    // Creazione del socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Errore nella creazione del socket");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Conversione dell'indirizzo IP
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Indirizzo non valido / non supportato");
        return -1;
    }

    // Connessione al server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connessione fallita");
        return -1;
    }

    int choice;
    do {
        show_menu();
        printf("\nInserisci la tua scelta: ");
        scanf("%d", &choice);
        getchar(); // Pulire il buffer del newline

        switch (choice) {
            case 1:
                handle_registration(sock);
                break;
            case 2:
                handle_login(sock);
                break;
            case 3:
                printf("\nUscita...\n");
                break;
            default:
                printf("Scelta non valida. Riprova.\n");
        }
    } while (choice != 3);

    close(sock);
    return 0;
}

void show_menu() {
    printf("\n===== Menu =====\n");
    printf("1. Registrati\n");
    printf("2. Login\n");
    printf("3. Esci\n");
}

void handle_registration(int socket) {
    char username[100];
    char password[100];

    printf("\nInserisci il nome utente: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0'; // Rimuove il newline

    printf("Inserisci la password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = '\0'; // Rimuove il newline

    // Invio richiesta di registrazione al server
    char message[256];
    snprintf(message, sizeof(message), "REGISTER\n%s\n%s\n", username, password);
    send(socket, message, strlen(message), 0);

    // Attendere la risposta del server
    char response[256];
    int valread = read(socket, response, sizeof(response) - 1);
    response[valread] = '\0';

    // Se la registrazione ha successo, procedi alla dashboard
    if (strcmp(response, "REGISTRATION_SUCCESS") == 0) {
        printf("Registrazione avvenuta con successo. Effettuando il login...\n");
        handle_dashboard(socket);
    }
}

void handle_login(int socket) {
    char username[100];
    char password[100];

    printf("\nInserisci il nome utente: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0'; // Rimuove il newline

    printf("Inserisci la password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = '\0'; // Rimuove il newline

    // Invio richiesta di login al server
    char message[256];
    snprintf(message, sizeof(message), "LOGIN\n%s\n%s\n", username, password);
    send(socket, message, strlen(message), 0);

    // Attendere la risposta del server
    char response[256];
    int valread = read(socket, response, sizeof(response) - 1);
    response[valread] = '\0';
    

    // Se il login ha successo, procedi alla dashboard
    if (strcmp(response, "LOGIN_SUCCESS") == 0) {
        printf("\nLogin avvenuto con successo! Benvenuto %s\n", username);
        handle_dashboard(socket);
    }
}

void handle_dashboard(int socket) {
    int choice;
    do {
        printf("\n======= Dashboard =======\n");
        printf("\n1. Visualizza tutti i libri disponibili\n");
        printf("2. Prenota un libro\n");
        printf("3. Annulla prenotazione\n");
        printf("4. Visualizza le mie prenotazioni\n");
        printf("5. Logout\n");
        printf("\nInserisci la tua scelta: ");
        scanf("%d", &choice);
        getchar(); // Pulire il buffer del newline

        switch (choice) {
            case 1:
                handle_view_books(socket);
                break;
            case 2:
                handle_reserve_book(socket);
                break;
            case 3:
                handle_cancel_reservation(socket);
                break;
            case 4:
                handle_view_reservations(socket);
                break;
            case 5:
                printf("\nDisconnessione in corso...\n");
                break;
            default:
                printf("\nScelta non valida. Riprova.\n");
        }
    } while (choice != 5);
}
void handle_view_books(int socket) {
    // Invia la richiesta al server per ottenere tutti i libri disponibili
    send(socket, "VIEW_BOOKS\n", strlen("VIEW_BOOKS\n"), 0);
    
    // Ricezione dei dati dal server
    char buffer[2048];  // Un buffer ragionevolmente grande
    int total_bytes_received = 0;
    int bytes_received = 0;

    printf("\nLIBRI DISPONIBILI:\n\n");
    while ((bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        // Termina la stringa ricevuta con '\0'
        buffer[bytes_received] = '\0';
        
        // Stampa i dati ricevuti
        printf("%s", buffer);

        // Aggiungi i bytes ricevuti al totale
        total_bytes_received += bytes_received;

        // Se abbiamo riempito il buffer e non ci sono più dati da leggere, esci dal loop
        if (bytes_received < sizeof(buffer) - 1) {
            break;
        }
    }

    // Se non abbiamo ricevuto alcun dato, stampa un messaggio di errore
    if (total_bytes_received == 0) {
        printf("\nNon ci sono libri disponibili / errore nella ricezione dati.\n");
    }
}



void handle_reserve_book(int socket) {
    char book_id[100];

    // Chiedi all'utente di inserire l'ID del libro da prenotare
    printf("\nInserisci l'ID del libro da prenotare: ");
    fgets(book_id, sizeof(book_id), stdin);
    book_id[strcspn(book_id, "\n")] = '\0'; // Rimuove il newline

    // Verifica che l'ID del libro non sia vuoto
    if (strlen(book_id) == 0) {
        printf("\nID del libro non può essere vuoto. Riprova.\n");
        return;
    }

    // Prepara il messaggio di prenotazione
    char message[256];
    snprintf(message, sizeof(message), "RESERVE\n%s\n", book_id);

    // Invia la richiesta di prenotazione al server
    send(socket, message, strlen(message), 0);

    // Attendere la risposta del server
    char response[256];
    int valread = recv(socket, response, sizeof(response) - 1, 0);
    
    // Se c'è un errore nella ricezione dei dati
    if (valread < 0) {
        perror("\nErrore nella ricezione della risposta dal server");
        return;
    }

    // Termina la stringa ricevuta con '\0'
    response[valread] = '\0';

    // Gestisci la risposta dal server
    if (strcmp(response, "RESERVATION_SUCCESS") == 0) {
        printf("\nPrenotazione avvenuta con successo.\n");
    } else if (strcmp(response, "RESERVATION_FAILURE") == 0) {
        printf("\nPrenotazione fallita. Il libro potrebbe non essere disponibile.\n");
    } 
}


void handle_cancel_reservation(int socket) {
    char reservation_id[100];

    printf("\nInserisci l'ID della prenotazione da annullare: ");
    fgets(reservation_id, sizeof(reservation_id), stdin);
    reservation_id[strcspn(reservation_id, "\n")] = '\0'; // Rimuove il newline

    // Invia richiesta di annullamento prenotazione al server
    char message[256];
    snprintf(message, sizeof(message), "CANCEL_RESERVATION\n%s\n", reservation_id);
    send(socket, message, strlen(message), 0);

    // Attendere la risposta del server
    char response[256];
    int valread = read(socket, response, sizeof(response) - 1);
    response[valread] = '\0';

    // Gestisci la risposta dal server
    if (strcmp(response, "CANCEL_RESERVATION_SUCCESS") == 0) {
        printf("\nPrenotazione annullata con successo!.\n");
    } else if (strcmp(response, "CANCEL_RESERVATION_FAILURE") == 0) {
        printf("\nAnnullamento della prenotazione fallito. Verifica l'ID della prenotazione.\n");
    } 
}


void handle_view_reservations(int socket) {
    // Invia richiesta al server per visualizzare le prenotazioni dell'utente
    send(socket, "VIEW_MY_RESERVATIONS\n", strlen("VIEW_MY_RESERVATIONS\n"), 0);

    // Riceve la risposta del server e la visualizza
    char buffer[2048]; // Aumentiamo il buffer per gestire risposte più grandi
    int total_bytes_received = 0;
    int bytes_received;

    printf("\n==== Le mie prenotazioni ====\n");

    // Ricevi i dati dal server in un ciclo finché c'è ancora qualcosa da ricevere
    while ((bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Assicura che la stringa ricevuta sia terminata correttamente
        printf("%s", buffer); // Stampa i dati ricevuti
        total_bytes_received += bytes_received;

        // Se abbiamo riempito il buffer e non ci sono più dati da leggere, esci dal loop
        if (bytes_received < sizeof(buffer) - 1) {
            break;
        }
    }

    // Se non abbiamo ricevuto alcun dato, stampa un messaggio di errore
    if (total_bytes_received == 0) {
        printf("\n\nNessuna prenotazione trovata o errore nella ricezione dei dati.\n");
    }
}

