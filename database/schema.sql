CREATE DATABASE IF NOT EXISTS iot_project;

USE iot_project;

-- Tabela de leituras
CREATE TABLE IF NOT EXISTS monitoramento_luz (
    id INT AUTO_INCREMENT PRIMARY KEY,
    valor_sensor INT,
    limite_min INT,
    limite_max INT,
    status VARCHAR(30),
    data_registro TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Tabela de eventos
CREATE TABLE IF NOT EXISTS eventos_luz (
    id INT AUTO_INCREMENT PRIMARY KEY,
    data_evento TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    evento VARCHAR(255),
    valor_sensor INT,
    status VARCHAR(30)
);

-- Tabela de alertas enviados
CREATE TABLE IF NOT EXISTS alertas_enviados (
    id INT AUTO_INCREMENT PRIMARY KEY,
    data_alerta TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    canal VARCHAR(50),
    status VARCHAR(30),
    valor_sensor INT,
    motivo VARCHAR(255),
    repeticoes_5min INT,
    tempo_critico_seg INT
);