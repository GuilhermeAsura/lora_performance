#include "LoRa_E22.h"
#include "Arduino.h"
#include <Preferences.h>
#include <nvs_flash.h>

// Pinout & Setup
#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

// Addressing
#define RECEIVER_ADDRESS 0x0011
#define LORA_CHANNEL     0x12

// Packet Types
enum PacketType : uint8_t {
  DATA    = 0x03,
  CONTROL = 0x05
};

// Control Types
enum ControlType : uint8_t {
  END_SESSION = 0x01
};

// Packet Structures (32 bytes)
const int PACKET_SIZE = 32;

struct DataPacket {
  uint8_t type;
  uint16_t id;
  char payload[PACKET_SIZE - 3];
} __attribute__((packed));

struct ControlPacket {
  uint8_t type;
  uint8_t control_type;
  uint8_t padding[PACKET_SIZE - 2];
} __attribute__((packed));

// Throughput Measurement Variables
unsigned long session_start_time = 0;
unsigned long total_bytes_received = 0;
uint16_t last_id = 0xFFFF;
uint16_t packets_received_count = 0;

// NVS Data Saving Variables
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
    config.ADDH = highByte(RECEIVER_ADDRESS);
    config.ADDL = lowByte(RECEIVER_ADDRESS);
    config.CHAN = LORA_CHANNEL;
    config.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    config.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;
    ResponseStatus rs = e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("LoRa Receiver (Unreliable) configured. Address: " + String(RECEIVER_ADDRESS, HEX));
    } else {
      Serial.println("Error setting configuration: " + rs.getResponseDescription());
    }
    c.close();
  } else {
    Serial.println("Error reading initial configuration: " + c.status.getResponseDescription());
  }

  // Initialize Preferences for data logging
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "transmissao" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  transmissao.begin(nameSpc.c_str(), false);
  Serial.println("Receiver started. Namespace: " + nameSpc);
}

void loop() {
  if (e22.available() > 0) {
    byte received_packet[PACKET_SIZE];
    ResponseStructContainer rsc = e22.receiveMessageRSSI(PACKET_SIZE);

    if (rsc.status.code == E22_SUCCESS && rsc.data != nullptr) {
      uint8_t packet_type = ((uint8_t*)rsc.data)[0];
      int rssiVal = (int)rsc.rssi;
      if (rssiVal > 127) rssiVal -= 256;

      switch (packet_type) {
        case DATA: {
          DataPacket data;
          memcpy(&data, rsc.data, sizeof(data));

          if (session_start_time == 0) {
              session_start_time = millis();
              Serial.println("Session started.");
          }

          char nome[10] = {0};
          strncpy(nome, data.payload, 9);
          Serial.println("Received Packet ID: " + String(data.id) + " | " + String(nome) + " | " + String(rssiVal) + " dBm");
          
          String chave = "pacote" + String(packets_received_count);
          String valor = "ID: " + String(data.id) + "; " + String(nome) + "; RSSI: " + String(rssiVal);
          transmissao.putString(chave.c_str(), valor);

          total_bytes_received += sizeof(DataPacket);
          packets_received_count++;
          last_id = data.id; // Keep track of the last ID for packet loss calculation
          break;
        }
        case CONTROL: {
          ControlPacket control;
          memcpy(&control, rsc.data, sizeof(control));

          if (control.control_type == END_SESSION) {
            Serial.println("Received END_SESSION signal.");
            calculate_metrics();
            
            // Reset for next session
            session_start_time = 0;
            total_bytes_received = 0;
            packets_received_count = 0;
            last_id = 0xFFFF;

            controle.begin("controle", false);
            int numTransm = controle.getInt("prox", 0);
            nameSpc = "transmissao" + String(numTransm);
            controle.putInt("prox", numTransm + 1);
            controle.end();
            transmissao.end();
            transmissao.begin(nameSpc.c_str(), false);
            Serial.println("New namespace created: " + nameSpc);
          }
          break;
        }
      }
      rsc.close();
    }
  }

  // Handle serial commands
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando == "listar") {
      list();
    } else if (comando.startsWith("transmissao")) {
      read(comando);
    } else if (comando == "limpar") {
      limparMemoria();
    } else {
      Serial.println("Comando inválido. Use 'listar', 'transmissaoX' ou 'limpar'.");
    }
  }
}

void calculate_metrics() {
  if (session_start_time == 0 || total_bytes_received == 0) {
    Serial.println("No data received to calculate metrics.");
    return;
  }
  unsigned long session_end_time = millis();
  float duration_s = (session_end_time - session_start_time) / 1000.0;
  float throughput_bps = (total_bytes_received * 8) / duration_s;

  // Packet Loss Calculation
  // Assumes packet IDs are sequential starting from 0.
  uint16_t total_packets_sent = last_id + 1;
  float packet_loss_percentage = 0.0;
  if (total_packets_sent > 0) {
    packet_loss_percentage = ((float)(total_packets_sent - packets_received_count) / total_packets_sent) * 100.0;
  }

  Serial.println("--- SESSION METRICS ---");
  Serial.println("Total Bytes Received: " + String(total_bytes_received));
  Serial.println("Total Session Time: " + String(duration_s, 2) + " s");
  Serial.println("Throughput: " + String(throughput_bps, 2) + " bps");
  Serial.println("Packets Sent (estimated): " + String(total_packets_sent));
  Serial.println("Packets Received: " + String(packets_received_count));
  Serial.println("Packet Loss: " + String(packet_loss_percentage, 2) + " %");
  Serial.println("-------------------------");
}


void read(const String& nome) {
  Preferences leitor;
  if (leitor.begin(nome.c_str(), true)) {
    Serial.println("Reading data from: " + nome);
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

void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Available Namespaces:");
  for (int i = 0; i < total; i++) {
    Serial.println("  transmissao" + String(i));
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