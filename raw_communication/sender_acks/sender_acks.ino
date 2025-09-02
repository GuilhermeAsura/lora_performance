#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"
#include <nvs_flash.h>

// Pinout & Setup
#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

// Addressing
#define SENDER_ADDRESS   0x0010
#define RECEIVER_ADDRESS 0x0011
#define LORA_CHANNEL     0x12

// Packet Types
enum PacketType : uint8_t {
  DATA    = 0x03,
  ACK     = 0x04,
  CONTROL = 0x05
};

// Control Types
enum ControlType : uint8_t {
  END_SESSION = 0x01
};

// Packet Structures (32 bytes)
const int PACKET_SIZE = 32;

struct DataPacket {
  uint8_t type = DATA;
  uint16_t id;
  char payload[PACKET_SIZE - 3];
} __attribute__((packed));

struct AckPacket {
  uint8_t type = ACK;
  uint16_t id;
} __attribute__((packed));

struct ControlPacket {
  uint8_t type = CONTROL;
  uint8_t control_type;
  uint8_t padding[PACKET_SIZE - 2];
} __attribute__((packed));

// Application State
uint16_t packet_id_counter = 0;
Preferences controle;
Preferences transmissao;
String nameSpc;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  // Configure LoRa Radio
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration config = *(Configuration*) c.data;
    config.ADDH = highByte(SENDER_ADDRESS);
    config.ADDL = lowByte(SENDER_ADDRESS);
    config.CHAN = LORA_CHANNEL;
    config.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    Serial.println("LoRa Sender configured. Address: 0x" + String(SENDER_ADDRESS, HEX));
    c.close();
  }

  // Initialize Preferences for data logging
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "LoRaTX" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  transmissao.begin(nameSpc.c_str(), false);
  Serial.println("Sender started. Namespace: " + nameSpc);
}

void loop() {
  // Main loop to send data packets
  send_data_packet(packet_id_counter);

  // Check for serial commands
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando == "end") {
      send_end_session();
    } else if (comando == "listar") {
      list();
    } else if (comando.startsWith("LoRaTX")) {
      read(comando);
    } else if (comando == "limpar") {
      limparMemoria();
    } else {
      Serial.println("Comando inválido. Use 'end', 'listar', 'LoRaTX<numero>' ou 'limpar'.");
    }
  }
}

void send_data_packet(uint16_t id) {
  DataPacket packet;
  packet.id = id;
  // Using a fixed payload for consistent testing
  strncpy(packet.payload, "TEST_DATA", sizeof(packet.payload));

  bool ack_received = false;
  int retries = 0;
  
  while (!ack_received && retries < 3) {
    Serial.println("Sending Packet ID: " + String(id));
    e22.sendFixedMessage(highByte(RECEIVER_ADDRESS), lowByte(RECEIVER_ADDRESS), LORA_CHANNEL, &packet, sizeof(packet));

    unsigned long ack_wait_start = millis();
    while (millis() - ack_wait_start < 2000) { // 2-second timeout for ACK
      if (e22.available() > 0) {
        ResponseStructContainer rsc = e22.receiveMessage(sizeof(AckPacket));
        if (rsc.status.code == E22_SUCCESS) {
          AckPacket ack;
          memcpy(&ack, rsc.data, sizeof(ack));
          if (ack.type == ACK && ack.id == id) {
            Serial.println("ACK received for Packet ID: " + String(id));
            salvarDados(packet);
            ack_received = true;
            rsc.close();
            break;
          }
        }
        rsc.close();
      }
    }

    if (!ack_received) {
      Serial.println("Timeout: No ACK for Packet ID: " + String(id) + ". Retry " + String(retries + 1));
      retries++;
    }
  }

  if (ack_received) {
    packet_id_counter++;
    delay(1000); // Wait 1 second before sending the next packet
  } else {
    Serial.println("Failed to send Packet ID: " + String(id) + " after 3 retries. Moving to next packet.");
    // In a pure LoRa test, we might want to just move on.
    packet_id_counter++; 
  }
}

void send_end_session() {
  Serial.println("Sending END_SESSION signal...");
  ControlPacket end_packet;
  end_packet.control_type = END_SESSION;
  e22.sendFixedMessage(highByte(RECEIVER_ADDRESS), lowByte(RECEIVER_ADDRESS), LORA_CHANNEL, &end_packet, sizeof(end_packet));

  // Create a new namespace for the next session
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "LoRaTX" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();
  transmissao.end();
  transmissao.begin(nameSpc.c_str(), false);
  Serial.println("New namespace created: " + nameSpc);
  packet_id_counter = 0;
}

void salvarDados(const DataPacket& packet) {
  char nome[10] = {0}; // Ensure buffer is large enough
  strncpy(nome, packet.payload, 9);
  String chave = "pacote" + String(packet.id);
  String valor = "|id:" + String(packet.id) + "|dados:" + String(nome);
  transmissao.putString(chave.c_str(), valor);
}

void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Available Namespaces:");
  for (int i = 0; i < total; i++) {
    Serial.println("  LoRaTX" + String(i));
  }
}

void read(const String& nome) {
  Preferences leitor;
  if (leitor.begin(nome.c_str(), true)) {
    Serial.println("Reading packets from: " + nome);
    for (int i = 0; i < 10000; i++) {
      String chave = "pacote" + String(i);
      String valor = leitor.getString(chave.c_str(), "");
      if (valor == "") break;
      Serial.println("  " + chave + ": " + valor);
    }
    leitor.end();
  } else {
    Serial.println("Namespace not found.");
  }
}

void limparMemoria() {
  Serial.println("Erasing NVS memory...");
  delay(500);
  esp_err_t status = nvs_flash_erase();
  if (status == ESP_OK) {
    Serial.println("NVS memory erased successfully!");
  } else {
    Serial.print("Error erasing NVS: ");
    Serial.println(status);
  }
  Serial.println("Restarting ESP...");
  delay(2000);
  ESP.restart();
}