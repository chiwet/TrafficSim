-- db_setup.sql
CREATE TABLE IF NOT EXISTS load_history (
    id SERIAL PRIMARY KEY,
    node_ip VARCHAR(15) NOT NULL,
    node_type INT NOT NULL,
    current_load REAL NOT NULL,
    sim_hour INT NOT NULL,
    sim_day INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_load_history_ip ON load_history(node_ip);