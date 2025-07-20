// Implementação de um client MQTT-like com medição de throughput
// Publica mensagens em tópicos e mede a taxa de transferência
#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"

// Pinout & Setup
#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

// Endereçamento 
#define MY_ADDRESS 0x0001
#define BROKER_ADDRESS 0x0002
#define LORA_CHANNEL 0x12

// Tipos de Pacote 
enum PacketType : uint8_t {
  CONNECT = 0x01,
  CONNACK = 0x02,
  DATA    = 0x03,
  ACK     = 0x04,
  CONTROL = 0x05 // Novo tipo para pacotes de controle
};

// Tipos de Controle 
enum ControlType : uint8_t {
  END_SESSION = 0x01
};

// Estruturas de Pacote (com tamanho consistente)
// Usamos __attribute__((packed)) para evitar padding do compilador.
const int PACKET_SIZE = 32; // Tamanho fixo para todos os pacotes de dados e controle

struct ConnectPacket {
  uint8_t type = CONNECT;
  uint16_t client_id = MY_ADDRESS;
  uint8_t padding[PACKET_SIZE - 3]; // Preenchimento para manter o tamanho fixo
} __attribute__((packed));

struct ConnAckPacket {
  uint8_t type = CONNACK;
  uint8_t return_code;
} __attribute__((packed));

struct DataPacket {
  uint8_t type = DATA;
  uint16_t id;
  char payload[PACKET_SIZE - 3]; // Payload
} __attribute__((packed));

struct AckPacket {
  uint8_t type = ACK;
  uint16_t id; // ID do pacote que está sendo confirmado
} __attribute__((packed));

struct ControlPacket {
  uint8_t type = CONTROL;
  uint8_t control_type;
  uint8_t padding[PACKET_SIZE - 2];
} __attribute__((packed));

// Estado da Aplicação
bool connected = false;
uint16_t packet_id_counter = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  // Configuração do rádio LoRa 
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration config = *(Configuration*) c.data;
    config.ADDH = highByte(MY_ADDRESS);
    config.ADDL = lowByte(MY_ADDRESS);
    config.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    Serial.println("Módulo Transmissor LoRa configurado.");
    c.close();
  }
}

void loop() {
  if (!connected) {
    connect_to_broker();
  } else {
    // Lógica de Envio com ACK e Retransmissão 
    send_data_packet(packet_id_counter);
  }
  
  // Comandos via Serial para controle
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "end") {
      send_end_session();
    }
  }
}

void connect_to_broker() {
  Serial.println("Enviando CONNECT para o broker...");
  ConnectPacket connect_packet;
  e22.sendFixedMessage(highByte(BROKER_ADDRESS), lowByte(BROKER_ADDRESS), LORA_CHANNEL, &connect_packet, sizeof(connect_packet));

  unsigned long start_time = millis();
  while (millis() - start_time < 2000) {
    if (e22.available() > 0) {
      ResponseStructContainer rsc = e22.receiveMessage(sizeof(ConnAckPacket));
      if (rsc.status.code == E22_SUCCESS) {
        ConnAckPacket connAck;
        memcpy(&connAck, rsc.data, sizeof(connAck));
        if (connAck.type == CONNACK && connAck.return_code == 0) {
          Serial.println("Conexão estabelecida com sucesso!");
          connected = true;
          rsc.close();
          return;
        }
      }
      rsc.close();
    }
  }
  Serial.println("Timeout: CONNACK não recebido. Tentando novamente...");
  delay(2000);
}

void send_data_packet(uint16_t id) {
  DataPacket packet;
  packet.id = id;
  snprintf(packet.payload, sizeof(packet.payload), "Data #%d", id);

  bool ack_received = false;
  int retries = 0;
  
  while (!ack_received && retries < 3) {
    Serial.print("Enviando pacote ID: " + String(id));
    if (retries > 0) {
      Serial.print(" (Tentativa " + String(retries + 1) + ")");
    }
    Serial.println();
    
    e22.sendFixedMessage(DEST_ADDH, DEST_ADDL, LORA_CHANNEL, &packet, sizeof(packet));

    // --- Aguarda pelo ACK ---
    unsigned long ack_wait_start = millis();
    while (millis() - ack_wait_start < 2000) { 
      if (e22.available() > 0) {
        ResponseStructContainer rsc = e22.receiveMessage(sizeof(AckPacket));
        if (rsc.status.code == E22_SUCCESS) {
          AckPacket ack;
          memcpy(&ack, rsc.data, sizeof(ack));
          if (ack.type == ACK && ack.id == id) {
            Serial.println("ACK recebido para o pacote ID: " + String(id));
            ack_received = true;
            rsc.close();
            break; 
          }
        }
        rsc.close();
      }
    }

    if (!ack_received) {
      Serial.println("Timeout: ACK não recebido para o pacote ID: " + String(id));
      retries++;
    }
  }

  if (ack_received) {
    packet_id_counter++;
    // Intervalo entre pacotes
    delay(1000); 
  } else {
    Serial.println("Falha ao enviar o pacote ID: " + String(id) + " após 3 tentativas. Abortando.");
    // Aqui você poderia adicionar uma lógica para resetar a conexão
    connected = false; 
  }
}

void send_end_session() {
  Serial.println("Enviando sinal de fim de sessão...");
  ControlPacket end_packet;
  end_packet.control_type = END_SESSION;
  e22.sendFixedMessage(BROKER_ADDRESS, LORA_CHANNEL, &end_packet, sizeof(end_packet));
}