CREATE TABLE books (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    author TEXT NOT NULL,
    available INTEGER NOT NULL
);

CREATE TABLE reservations (
    id INTEGER PRIMARY KEY,
    book_id INTEGER,
    user TEXT,
    FOREIGN KEY (book_id) REFERENCES books (id)
);
