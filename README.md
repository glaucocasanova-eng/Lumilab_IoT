📄 README — Sistema IoT de Monitoramento de Luminosidade
🔹 1. Visão Geral
O projeto consiste em um sistema IoT para monitoramento de luminosidade em tempo real, utilizando um microcontrolador ESP32 integrado a um backend baseado em Node-RED, com persistência em banco de dados MySQL e visualização via dashboard web.
O sistema é capaz de:
Monitorar luminosidade ambiente
Classificar o status (NORMAL, BAIXO, ALTO)
Registrar histórico e eventos
Disponibilizar APIs REST
Exibir dados em tempo real no dashboard
Gerar alertas e eventos automaticamente
🔹 2. Arquitetura do Sistema

ESP32 → MQTT → Node-RED → MySQL → Dashboard → API REST → Telegram
Componentes:
ESP32 → coleta dados do sensor
MQTT (broker local/AWS) → transporte dos dados
Node-RED → processamento e regras de negócio
MySQL → armazenamento
Dashboard → visualização
API REST → integração externa
🔹 3. Tecnologias Utilizadas
ESP32 (C++ / Arduino)
Node-RED
MQTT (porta 1883)
MySQL 8
Dashboard Node-RED
JavaScript (Function nodes)
REST API
🔹 4. Funcionamento do Sistema
📡 Entrada de dados
O ESP32 publica no tópico:

iot/monitoramento_luz
Payload esperado:
JSON
{
  "valor_sensor": 30,
  "limite_min": 120,
  "limite_max": 800,
  "status": "BAIXO"
}
⚙️ Processamento no Node-RED
🔸 1. Preparação dos dados
Função: Preparar Dashboard Tempo Real
Valida payload
Formata status com emoji
Distribui para:
Gauge
Texto
Gráfico
Limites
Hora
🔸 2. Persistência no banco
Função: Gerar SQL INSERT
SQL
INSERT INTO monitoramento_luz 
(valor_sensor, limite_min, limite_max, status)
VALUES (...)
Executado via nó MySQL:
Salvar no MySQL
🔸 3. Detecção de eventos
Função: Detectar Eventos
Compara status atual com anterior
Evita duplicação (anti-spam)
Gera evento somente quando há mudança
SQL
INSERT INTO eventos_luz (evento, valor_sensor, status)
🔹 5. Banco de Dados
📊 Tabela: monitoramento_luz
Campo
Tipo
id
int (PK)
valor_sensor
int
limite_min
int
limite_max
int
status
varchar
data_registro
timestamp
📊 Tabela: eventos_luz
Campo
Tipo
data_evento
timestamp
evento
varchar
valor_sensor
int
status
varchar
🔹 6. Dashboard
📍 Página: Monitoramento em Tempo Real
Componentes:
Gauge (luminosidade)
Status textual
Limites mínimo/máximo
Última leitura
Gráfico em tempo real
📍 Página: Histórico
Atualização automática a cada 35 segundos
Query:
SQL
SELECT 
    data_registro,
    valor_sensor,
    limite_min,
    limite_max,
    status
FROM monitoramento_luz
ORDER BY data_registro DESC
LIMIT 5
📍 Página: Eventos
Atualização a cada 20 segundos
Query:
SQL
SELECT
    data_evento,
    evento,
    valor_sensor,
    status
FROM eventos_luz
ORDER BY data_evento DESC
LIMIT 20
🔹 7. APIs REST
📌 Endpoint: Luminosidade

GET /api/luminosidade
Retorno:
JSON
{
  "valor_sensor": 30,
  "limite_min": 120,
  "limite_max": 800,
  "status": "BAIXO"
}
📌 Endpoint: Histórico

GET /api/historico
Retorna últimos registros do banco.
📌 Endpoint: Eventos

GET /api/eventos
Retorna eventos registrados.
🔹 8. MQTT
Broker configurado:

localhost:1883
ou
3.12.197.220:1883
🔹 9. Controle de Navegação
Botões implementados no dashboard:
Monitoramento em tempo real
Histórico
Função:
JavaScript
msg.payload = { page: 'Monitoramento em Tempo Real' }
🔹 10. Melhorias Implementadas
✔ Validação de payload antes de inserir
✔ Controle de eventos (evita duplicação)
✔ Limitação de histórico (LIMIT 5)
✔ Limitação de eventos (LIMIT 20)
✔ Formatação de dados para dashboard
✔ Uso de timestamp para gráficos
🔹 11. Problemas Resolvidos
Durante o desenvolvimento:
❌ Erro MySQL (Access Denied)
✔ Corrigido criando usuário:
SQL
CREATE USER 'nodered'@'localhost' IDENTIFIED BY '123456';
GRANT ALL PRIVILEGES ON iot_project.* TO 'nodered'@'localhost';
FLUSH PRIVILEGES;
❌ Erro CR_BAD_FIELD_ERROR
✔ Ajustado nome das colunas conforme tabela
❌ Acúmulo de dados no dashboard
✔ Resolvido com:
LIMIT no SQL
Controle no template
🔹 12. Estrutura do Fluxo Node-RED
Fluxo principal identificado em:
👉  
flows .json None
Principais blocos:
MQTT IN → Entrada de dados
Functions → Processamento
MySQL → Persistência
UI → Dashboard
Inject → Atualizações periódicas
🔹 13. Execução do Projeto
1. Instalar dependências
Node-RED
MySQL
MQTT broker (Mosquitto)
2. Configurar banco
SQL
CREATE DATABASE iot_project;
Criar tabelas conforme seção 5.
3. Configurar Node-RED
Importar flows.json
Ajustar:
MySQL (user/senha)
MQTT broker
4. Executar ESP32
Enviar código .ino
Conectar ao Wi-Fi
Publicar dados via MQTT
🔹 14. Melhorias Futuras
Integração com app mobile
Autenticação nas APIs
Armazenamento em nuvem (RDS)
Multi sensores
Machine Learning para previsão
✅ Conclusão
O sistema demonstra integração completa entre hardware e software, com:
Processamento em tempo real
Persistência estruturada
Interface visual
APIs para integração externa
