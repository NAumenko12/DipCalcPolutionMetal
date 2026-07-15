INSERT INTO metals (id, name, symbol, unit) VALUES
    (1, 'Lead', 'Pb', 'mg/m3'),
    (2, 'Cadmium', 'Cd', 'mg/m3'),
    (3, 'Mercury', 'Hg', 'mg/m3'),
    (4, 'Arsenic', 'As', 'mg/m3'),
    (5, 'Nickel', 'Ni', 'mg/m3')
ON CONFLICT (id) DO NOTHING;

SELECT setval('metals_id_seq', GREATEST((SELECT MAX(id) FROM metals), 1));

