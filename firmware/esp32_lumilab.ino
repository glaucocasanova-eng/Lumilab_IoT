#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// =====================================================
// OLED
// =====================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SDA_PIN 8
#define SCL_PIN 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// PINOS
// =====================================================
#define LDR_PIN 4
#define LED_VERDE 6
#define LED_VERMELHO 7
#define BUZZER 5
#define BTN_MAIS 15
#define BTN_MENOS 16

// =====================================================
// LIMITES
// =====================================================
int limiteMin = 120;
int limiteMax = 800;
int limiteLuzNoturna = 80;  // calibrado com base nos seus testes

// Histerese para evitar oscilação no estado BAIXO/NORMAL
int limiteBaixoEntrada = 110; // entra em BAIXO
int limiteBaixoSaida   = 130; // volta para NORMAL

String ultimoStatusMQTT = "NORMAL";

// =====================================================
// CONTROLE DOS BOTÕES
// =====================================================
unsigned long ultimoBotaoMais = 0;
unsigned long ultimoBotaoMenos = 0;
const unsigned long debounceDelay = 200;

// =====================================================
// CONTROLE DO BUZZER
// evita beep contínuo a cada loop
// =====================================================
unsigned long ultimoBeep = 0;
const unsigned long intervaloBeep = 3000;

// =====================================================
// WIFI
// =====================================================
const char* ssid = "SUA REDE WIFI";
const char* password = "SUA SENHA";

// =====================================================
// MQTT
// =====================================================
const char* mqtt_server = "SEU SERVIDOR";
const int mqtt_port = 1883;
const char* mqtt_topic = "iot/monitoramento_luz";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long ultimoEnvio = 0;
const unsigned long intervaloEnvio = 5000; // 5 segundos

// =====================================================
// FUNÇÕES AUXILIARES
// =====================================================
void beepCurto() {
  digitalWrite(BUZZER, HIGH);
  delay(40);
  digitalWrite(BUZZER, LOW);
}

void beepControlado() {
  unsigned long agora = millis();
  if (agora - ultimoBeep >= intervaloBeep) {
    ultimoBeep = agora;
    beepCurto();
  }
}

void telaInicial() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(10, 8);
  display.println("LumiLab");

  display.setTextSize(1);
  display.setCursor(5, 35);
  display.println("Monitor de Luz");

  display.display();
  delay(1500);
}

void conectarWiFi() {
  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado.");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
}

void configurarHorario() {
  // Brasil UTC-3
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Sincronizando horario via NTP...");

  struct tm timeinfo;
  int tentativas = 0;

  while (!getLocalTime(&timeinfo) && tentativas < 20) {
    Serial.print(".");
    delay(500);
    tentativas++;
  }

  if (getLocalTime(&timeinfo)) {
    Serial.println("\nHorario sincronizado.");
  } else {
    Serial.println("\nFalha ao sincronizar horario.");
  }
}

void conectarMQTT() {
  while (!client.connected()) {
    Serial.println("Conectando ao broker MQTT...");

    String clientId = "ESP32_LDR_";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT conectado.");
    } else {
      Serial.print("Falha MQTT. rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s...");
      delay(5000);
    }
  }
}

// =====================================================
// STATUS COM HISTERESE
// =====================================================
String calcularStatusMQTT(int valor) {
  // prioridade para condição alta
  if (valor > limiteMax) {
    ultimoStatusMQTT = "ALTO";
    return "ALTO";
  }

  // se estava BAIXO, só volta para NORMAL acima do limite de saída
  if (ultimoStatusMQTT == "BAIXO") {
    if (valor > limiteBaixoSaida) {
      ultimoStatusMQTT = "NORMAL";
    } else {
      ultimoStatusMQTT = "BAIXO";
    }
    return ultimoStatusMQTT;
  }

  // se estava ALTO e saiu da condição alta
  if (ultimoStatusMQTT == "ALTO") {
    if (valor <= limiteMax) {
      if (valor < limiteBaixoEntrada) {
        ultimoStatusMQTT = "BAIXO";
      } else {
        ultimoStatusMQTT = "NORMAL";
      }
    }
    return ultimoStatusMQTT;
  }

  // se estava NORMAL, só entra em BAIXO abaixo do limite de entrada
  if (valor < limiteBaixoEntrada) {
    ultimoStatusMQTT = "BAIXO";
  } else {
    ultimoStatusMQTT = "NORMAL";
  }

  return ultimoStatusMQTT;
}

String calcularStatusTextoPorMQTT(String statusMQTT) {
  if (statusMQTT == "BAIXO") {
    return "SUB-ILUMIN.";
  } else if (statusMQTT == "ALTO") {
    return "SOBRE-ILUM.";
  } else {
    return "NORMAL";
  }
}

int lerMediaSensor(int pino, int amostras = 10) {
  long soma = 0;
  for (int i = 0; i < amostras; i++) {
    soma += analogRead(pino);
    delay(10);
  }
  return soma / amostras;
}

// =====================================================
// MODO ESCURIDÃO: 22h às 6h
// =====================================================
bool periodoEscuridaoAtivo(int &horaAtual) {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    horaAtual = -1;
    return false;
  }

  horaAtual = timeinfo.tm_hour;
  return (horaAtual >= 22 || horaAtual < 6);
}

/*
=====================================================
TESTE RÁPIDO DO MODO NOTURNO
Se quiser forçar o modo noturno temporariamente:

bool periodoEscuridaoAtivo(int &horaAtual) {
  horaAtual = 23;
  return true;
}
=====================================================
*/

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);

  pinMode(LDR_PIN, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(BTN_MAIS, INPUT_PULLUP);
  pinMode(BTN_MENOS, INPUT_PULLUP);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(BUZZER, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Falha ao iniciar OLED");
    while (true);
  }

  telaInicial();

  conectarWiFi();
  configurarHorario();

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // mantém Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    configurarHorario();
  }

  // mantém MQTT
  if (!client.connected()) {
    conectarMQTT();
  }

  client.loop();

  int valor = lerMediaSensor(LDR_PIN, 10);
  unsigned long agora = millis();

  // =====================================================
  // BOTÕES
  // =====================================================
  if (digitalRead(BTN_MAIS) == LOW && (agora - ultimoBotaoMais > debounceDelay)) {
    limiteMax += 50;
    ultimoBotaoMais = agora;
    beepCurto();
  }

  if (digitalRead(BTN_MENOS) == LOW && (agora - ultimoBotaoMenos > debounceDelay)) {
    limiteMax -= 50;
    if (limiteMax < limiteMin + 50) limiteMax = limiteMin + 50;
    ultimoBotaoMenos = agora;
    beepCurto();
  }

  // =====================================================
  // STATUS E PERÍODO
  // =====================================================
  String statusMQTT = calcularStatusMQTT(valor);
  String statusTela = calcularStatusTextoPorMQTT(statusMQTT);

  int horaAtual = -1;
  bool periodoEscuridao = periodoEscuridaoAtivo(horaAtual);

  String periodoTexto = periodoEscuridao ? "ESCURIDAO" : "MONITORAMENTO";
  String statusNoturno = "N/A";
  bool alertaNoturno = false;

  // =====================================================
  // LÓGICA DO SISTEMA
  // =====================================================
  if (periodoEscuridao) {
    // No período de escuridão:
    // só alerta se houver luz indevida
    if (valor > limiteLuzNoturna) {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      beepControlado();
      statusNoturno = "LUZ INDEVIDA";
      alertaNoturno = true;
    } else {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, LOW);
      digitalWrite(BUZZER, LOW);
      statusNoturno = "ESCURIDAO OK";
      alertaNoturno = false;
    }
  } else {
    // Durante o dia: lógica normal usando status estabilizado
    if (statusMQTT == "BAIXO") {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      beepControlado();
    }
    else if (statusMQTT == "NORMAL") {
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_VERMELHO, LOW);
      digitalWrite(BUZZER, LOW);
    }
    else if (statusMQTT == "ALTO") {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      beepControlado();
    }
  }

  // =====================================================
  // SERIAL
  // =====================================================
  Serial.print("Hora: ");
  Serial.print(horaAtual);
  Serial.print(" | Periodo: ");
  Serial.print(periodoTexto);
  Serial.print(" | Valor: ");
  Serial.print(valor);
  Serial.print(" | Min: ");
  Serial.print(limiteMin);
  Serial.print(" | Max: ");
  Serial.print(limiteMax);
  Serial.print(" | Status: ");
  Serial.print(statusMQTT);
  Serial.print(" | Status noturno: ");
  Serial.print(statusNoturno);
  Serial.print(" | Alerta: ");
  Serial.println(alertaNoturno ? "SIM" : "NAO");

  // =====================================================
  // OLED
  // sincronizado com dashboard e alerta
  // =====================================================
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("LumiLab IoT");

  display.setCursor(0, 12);
  display.print("Luz: ");
  display.println(valor);

  display.setCursor(0, 24);
  display.print("Modo: ");
  display.println(periodoTexto);

  display.setCursor(0, 36);
  if (periodoEscuridao) {
    if (alertaNoturno) {
      display.println("ALERTA: LUZ");
    } else {
      display.println("ESCURIDAO OK");
    }
  } else {
    display.println(statusTela);
  }

  display.setCursor(0, 48);
  display.print("Hr: ");
  if (horaAtual >= 0) {
    display.println(horaAtual);
  } else {
    display.println("--");
  }

  display.display();

  // =====================================================
  // MQTT
  // sempre envia alerta e status_noturno
  // =====================================================
  if (agora - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = agora;

    StaticJsonDocument<256> doc;
    doc["valor_sensor"] = valor;
    doc["limite_min"] = limiteMin;
    doc["limite_max"] = limiteMax;
    doc["status"] = statusMQTT;
    doc["periodo"] = periodoTexto;
    doc["hora_atual"] = horaAtual;
    doc["status_noturno"] = statusNoturno;
    doc["alerta"] = alertaNoturno;

    char buffer[256];
    serializeJson(doc, buffer);

    Serial.println("Enviando MQTT:");
    Serial.println(buffer);

    bool enviado = client.publish(mqtt_topic, buffer);

    if (enviado) {
      Serial.println("Mensagem publicada com sucesso.");
    } else {
      Serial.println("Falha ao publicar mensagem.");
    }
  }

  delay(250);
}
