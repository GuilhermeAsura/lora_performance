// Implementação de um broker MQTT-like
// Recebe mensagens, gerencia assinaturas e salva logs em Preferences

#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"

// Pinos para ESP32 ~AJUSTE conforme necessário!!
#define RX_PIN 16
#define TX_PIN 17
#define AUX_PIN 15
#define M0_PIN 19
#define M1_PIN 21

// Configuração LoRa (usada apenas para sendFixedMessage)
#define LORA_CHANNEL 23      // Canal: 915 MHz
#define LORA_BAUD 9600       // Baud Rate para Serial2

LoRa_E22 lora(&Serial2, AUX_PIN, M0_PIN, M1_PIN); // M0, M1, AUX

// Estruturas de pacotes MQTT-like
struct ConnectPacket {
  uint8_t type = 0x01;    // CONNECT
  uint16_t client_id;     // ID do cliente
  uint16_t keep_alive;    // Keep-alive em segundos
};

struct ConnAckPacket {
  uint8_t type = 0x02;    // CONNACK
  uint8_t return_code;    // 0=Sucesso, 1=Erro
};

struct SubscribePacket {
  uint8_t type = 0x03;    // SUBSCRIBE
  uint16_t packet_id;     // ID do pacote
  char topic[50];         // Tópico (máx. 50 bytes)
};

struct pubAckPacket {
  uint8_t type = 0x04;    // pubAck
  uint16_t packet_id;     // Mesmo ID do SUBSCRIBE
  uint8_t return_code;    // 0=Sucesso, 1=Erro
};

struct PublishPacket {
  uint8_t type = 0x05;    // PUBLISH
  uint16_t packet_id;     // ID do pacote
  char topic[50];         // Tópico
  char payload[190];      // Payload (máx. 190 bytes, total < 240 bytes)
};

struct PubAckPacket {
  uint8_t type = 0x06;    // PUBACK
  uint16_t packet_id;     // Mesmo ID do PUBLISH
};

Preferences controle;       // Namespace para contador de sessões
Preferences transmissao;    // Namespace para armazenamento
Preferences leitor;         // Namespace para leitura
String nameSpc;            // Namespace da sessão atual
int packet_counter = 0;    // Contador de pacotes recebidos
uint16_t last_packet_id = 0; // Último ID de pacote recebido
bool first_packet = true;   // Flag para primeiro pacote

void setup() {
  // Inicializa Serial para depuração
  Serial.begin(115200);
  delay(1000);

  // Inicializa Serial2 para LoRa
  Serial2.begin(LORA_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

  // Inicializa LoRa sem configuração específica
  lora.begin();

  // Configura namespace dinâmico
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "transmissao" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  transmissao.begin(nameSpc.c_str(), false);

  Serial.print("Broker iniciado. Namespace atual: ");
  Serial.println(nameSpc);
  Serial.println("Use 'listar' ou 'transmissaoX' para ver logs.");
}

void loop() {
  if (lora.available() > 0) {
    // Tenta receber um pacote
    ResponseStructContainer rsc = lora.receiveMessage(sizeof(ConnectPacket));
    if (rsc.status.code == 1) {
      uint8_t type;
      memcpy(&type, rsc.data, sizeof(type));

      if (type == 0x01) { // CONNECT
        ConnectPacket connect;
        memcpy(&connect, rsc.data, sizeof(connect));
        String log_entry = "Recebido CONNECT, client_id=" + String(connect.client_id);
        Serial.println(log_entry);
        salvarDados(log_entry);

        // Envia CONNACK
        ConnAckPacket connAck = {0x02, 0}; // Sucesso
        lora.sendFixedMessage(highByte(0x0002), lowByte(0x0002), LORA_CHANNEL, &connAck, sizeof(connAck));
        log_entry = "Enviado CONNACK, client_id=" + String(connect.client_id);
        Serial.println(log_entry);
        salvarDados(log_entry);
      }
      else if (type == 0x03) { // SUBSCRIBE
        SubscribePacket subscribe;
        memcpy(&subscribe, rsc.data, sizeof(subscribe));
        String log_entry = "Recebido SUBSCRIBE, packet_id=" + String(subscribe.packet_id) + ", topic=" + String(subscribe.topic);
        Serial.println(log_entry);
        salvarDados(log_entry);

        // Envia pubAck
        pubAckPacket pubAck = {0x04, subscribe.packet_id, 0}; // Sucesso
        lora.sendFixedMessage(highByte(0x0002), lowByte(0x0002), LORA_CHANNEL, &pubAck, sizeof(pubAck));
        log_entry = "Enviado pubAck, packet_id=" + String(subscribe.packet_id);
        Serial.println(log_entry);
        salvarDados(log_entry);
      }
      else if (type == 0x05) { // PUBLISH
        rsc.close();
        rsc = lora.receiveMessage(sizeof(PublishPacket));
        if (rsc.status.code == 1) {
          PublishPacket publish;
          memcpy(&publish, rsc.data, sizeof(publish));
          String topic = String(publish.topic);
          String payload = String(publish.payload);
          String chave = "pacote" + String(packet_counter);

          // Salva mensagem
          String msg = "topic=" + topic + ", payload=" + payload;
          transmissao.putString(chave.c_str(), msg);
          String log_entry = "Salvou: " + chave + " = " + msg;
          Serial.println(log_entry);
          salvarDados(log_entry);

          // Verifica perda de pacotes
          if (first_packet) {
            last_packet_id = publish.packet_id;
            first_packet = false;
          } else if (publish.packet_id != last_packet_id + 1) {
            log_entry = "Pacote(s) perdido(s) entre ID " + String(last_packet_id) + " e " + String(publish.packet_id);
            Serial.println(log_entry);
            salvarDados(log_entry);
          }
          last_packet_id = publish.packet_id;

          // Envia PUBACK
          PubAckPacket pubAck = {0x06, publish.packet_id};
          lora.sendFixedMessage(highByte(0x0002), lowByte(0x0002), LORA_CHANNEL, &pubAck, sizeof(pubAck));
          log_entry = "Enviado PUBACK, packet_id=" + String(publish.packet_id);
          Serial.println(log_entry);
          salvarDados(log_entry);

          packet_counter++;
        }
      }
      rsc.close();
    }
  }

  // Comandos via Serial
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando == "listar") {
      list();
    } else if (comando.startsWith("transmissao")) {
      read(comando);
    } else {
      Serial.println("Comando inválido. Use 'listar' ou 'transmissaoX'.");
    }
  }
}

void salvarDados(const String& dados) {
  String chave = "log" + String(packet_counter);
  transmissao.putString(chave.c_str(), dados);
}

void read(const String& nome) {
  if (leitor.begin(nome.c_str(), true)) {
    Serial.print("Lendo mensagens de: ");
    Serial.println(nome);
    for (int i = 0; i < 1000; i++) {
      String chave = "pacote" + String(i);
      String valor = leitor.getString(chave.c_str(), "");
      if (valor == "") break;
      Serial.print("  ");
      Serial.print(chave);
      Serial.print(": ");
      Serial.println(valor);
    }
    leitor.end();
  } else {
    Serial.println("Namespace não encontrado ou inválido.");
  }
}

void list() {
  controle.begin("controle", true);
  int totalTransmissoes = controle.getInt("prox", 0);
  controle.end();
  Serial.println("Namespaces salvos:");
  for (int i = 0; i < totalTransmissoes; i++) {
    Serial.print("  transmissao");
    Serial.println(i);
  }
}