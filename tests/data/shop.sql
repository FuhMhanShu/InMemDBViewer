CREATE TABLE kunden (id INTEGER PRIMARY KEY, name TEXT NOT NULL, ort TEXT);
CREATE TABLE bestellungen (id INTEGER PRIMARY KEY, kunde_id INTEGER REFERENCES kunden(id), betrag REAL);
INSERT INTO kunden (id, name, ort) VALUES (1, 'Ada', 'Berlin'), (2, 'Bob', 'Bonn');
INSERT INTO bestellungen VALUES (10, 1, 99.5), (11, 2, 12.0);
