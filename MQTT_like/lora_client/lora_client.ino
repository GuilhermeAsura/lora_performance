// Implementação de um client MQTT-like
// Publica mensagens em tópicos e salva logs em Preferences

#include "LoRa_E22.h"
#include "Arduino.h"
#include <Preferences.h>

// Pinos para ESP32 ~AJUSTE conforme necessário
#define RX_PIN 16
#define TX_PIN 17
#define AUX_PIN 15
#define M0_PIN 19
#define M1_PIN 21

// Configuração LoRa (usada apenas para sendFixedMessage)
#define BROKER_ADDRESS 0x0001 // Endereço do broker
#define LORA_CHANNEL 23      // Canal (915 MHz, ajuste se necessário)
#define LORA_BAUD 9600       // Baud rate para Serial2

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
  char topic[50];         // Tópico
};

struct SubAckPacket {
  uint8_t type = 0x04;    // SUBACK
  uint16_t packet_id;     // Mesmo ID do SUBSCRIBE
  uint8_t return_code;    // 0=Sucesso, 1=Erro
};

struct PublishPacket {
  uint8_t type = 0x05;    // PUBLISH
  uint16_t packet_id;     // ID do pacote
  char topic[50];         // Tópico
  char payload[190];      // Payload
};

struct PubAckPacket {
  uint8_t type = 0x06;    // PUBACK
  uint16_t packet_id;     // Mesmo ID do PUBLISH
};

Preferences controle;       // Namespace para contador de sessões
Preferences transmissao;    // Namespace para armazenamento
String nomeNamespace;       // Namespace da sessão atual
uint16_t packet_id = 0;    // ID para pacotes
int packet_counter = 0;    // Contador de pacotes enviados
bool connected = false;     // Estado da conexão

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
  int numeroTransmissao = controle.getInt("prox", 0);
  nomeNamespace = "transmissao" + String(numeroTransmissao);
  controle.putInt("prox", numeroTransmissao + 1);
  controle.end();

  transmissao.begin(nomeNamespace.c_str(), false);

  Serial.print("Client iniciado. Namespace atual: ");
  Serial.println(nomeNamespace);
  Serial.println("Conectando ao broker...");
}

void loop() {
  if (!connected) {
    connectToBroker();
    delay(2000);
    return;
  }

  // Envia mensagem PUBLISH
  PublishPacket publish;
  publish.type = 0x05;
  publish.packet_id = packet_id;
  strncpy(publish.topic, "sensors/temperature", sizeof(publish.topic) - 1);
  publish.topic[sizeof(publish.topic) - 1] = '\0';
  String payload = "Temp: " + String(packet_counter);
  strncpy(publish.payload, payload.c_str(), sizeof(publish.payload) - 1);
  publish.payload[sizeof(publish.payload) - 1] = '\0';

  ResponseStatus rs = lora.sendFixedMessage(highByte(BROKER_ADDRESS), lowByte(BROKER_ADDRESS), LORA_CHANNEL, &publish, sizeof(publish));
  String log_entry = "Enviado PUBLISH, packet_id=" + String(publish.packet_id) + ", topic=" + String(publish.topic) + ", payload=" + String(publish.payload);
  Serial.println(log_entry);
  salvarDados(log_entry);

  // Espera PUBACK
  bool ack_received = false;
  unsigned long start_time = millis();
  while (millis() - start_time < 3000) {
    if (lora.available() > 0) {
      ResponseStructContainer rsc = lora.receiveMessage(sizeof(PubAckPacket));
      if (rsc.status.code == 1) {
        PubAckPacket pubAck;
        memcpy(&pubAck, rsc.data, sizeof(pubAck));
        if (pubAck.type == 0x06 && pubAck.packet_id == publish.packet_id) {
          ack_received = true;
          log_entry = "Recebido PUBACK, packet_id=" + String(pubAck.packet_id);
          Serial.println(log_entry);
          salvarDados(log_entry);
          rsc.close();
          break;
        }
      }
      rsc.close();
    }
  }

  if (ack_received) {
    packet_id++;
    packet_counter++;
  } else {
    log_entry = "Nenhum PUBACK para packet_id=" + String(publish.packet_id);
    Serial.println(log_entry);
    salvarDados(log_entry);
  }

  // Verifica comando 'listar'
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando == "listar") {
      Serial.println("Logs salvos nesta sessão:");
      for (int i = 0; i < packet_counter; i++) {
        String chave = "log" + String(i);
        String valor = transmissao.getString(chave.c_str(), "(vazio)");
        Serial.print("  ");
        Serial.print(chave);
        Serial.print(": ");
        Serial.println(valor);
      }
    }
  }

  delay(2000); // Intervalo entre envios
}

void connectToBroker() {
  // Envia CONNECT
  ConnectPacket connect = {0x01, 0x0002, 60}; // client_id fixo como 0x0002
  ResponseStatus rs = lora.sendFixedMessage(highByte(BROKER_ADDRESS), lowByte(BROKER_ADDRESS), LORA_CHANNEL, &connect, sizeof(connect));
  String log_entry = "Enviado CONNECT, client_id=" + String(connect.client_id);
  Serial.println(log_entry);
  salvarDados(log_entry);

  // Espera CONNACK
  unsigned long start_time = millis();
  while (millis() - start_time < 3000) {
    if (lora.available() > 0) {
      ResponseStructContainer rsc = lora.receiveMessage(sizeof(ConnAckPacket));
      if (rsc.status.code == 1) {
        ConnAckPacket connAck;
        memcpy(&connAck, rsc.data, sizeof(connAck));
        if (connAck.type == 0x02 && connAck.return_code == 0) {
          connected = true;
          log_entry = "Recebido CONNACK, conexão estabelecida";
          Serial.println(log_entry);
          salvarDados(log_entry);

          // Envia SUBSCRIBE
          SubscribePacket subscribe = {0x03, packet_id, ""};
          strncpy(subscribe.topic, "sensors/temperature", sizeof(subscribe.topic) - 1);
          subscribe.topic[sizeof(subscribe.topic) - 1] = '\0';
          lora.sendFixedMessage(highByte(BROKER_ADDRESS), lowByte(BROKER_ADDRESS), LORA_CHANNEL, &subscribe, sizeof(subscribe));
          log_entry = "Enviado SUBSCRIBE, packet_id=" + String(subscribe.packet_id) + ", topic=" + String(subscribe.topic);
          Serial.println(log_entry);
          salvarDados(log_entry);

          // Espera SUBACK
          start_time = millis();
          while (millis() - start_time < 3000) {
            if (lora.available() > 0) {
              ResponseStructContainer sub_rsc = lora.receiveMessage(sizeof(SubAckPacket));
              if (sub_rsc.status.code == 1) {
                SubAckPacket subAck;
                memcpy(&subAck, sub_rsc.data, sizeof(subAck));
                if (subAck.type == 0x04 && subAck.packet_id == subscribe.packet_id) {
                  log_entry = "Recebido SUBACK, packet_id=" + String(subAck.packet_id);
                  Serial.println(log_entry);
                  salvarDados(log_entry);
                  sub_rsc.close();
                  break;
                }
              }
              sub_rsc.close();
            }
          }
          rsc.close();
          break;
        }
      }
      rsc.close();
    }
  }
}

void salvarDados(const String& dados) {
  String chave = "log" + String(packet_counter);
  transmissao.putString(chave.c_str(), dados);
}