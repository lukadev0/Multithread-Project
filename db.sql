CREATE TABLE IF NOT EXISTS utente (
    matricola INTEGER PRIMARY KEY,
    nome TEXT NOT NULL,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS libro (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    titolo TEXT NOT NULL,
    autore TEXT NOT NULL,
    categoria TEXT NOT NULL,
    anno_pubblicazione INTEGER,
    disponibilita INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS prenotazione (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    matricola INTEGER,
    id_libro INTEGER,
    FOREIGN KEY (matricola) REFERENCES utente(matricola),
    FOREIGN KEY (id_libro) REFERENCES libro(id)
);

