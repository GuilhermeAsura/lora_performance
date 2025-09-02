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
#define SENDER_ADDRESS   0x0010
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
  uint8_t type;
  uint16_t id;
  char payload[PACKET_SIZE - 3];
} __attribute__((packed));

struct AckPacket {
  uint8_t type = ACK;
  uint16_t id;
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

// NVS Data Saving Variables
Preferences controle;
Preferences transmissao;
int contador = 0;
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
    config.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED; // Enable RSSI
    ResponseStatus rs = e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("LoRa Receiver configured. Address: " + String(RECEIVER_ADDRESS, HEX));
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
      memcpy(received_packet, rsc.data, sizeof(received_packet));
      uint8_t packet_type = received_packet[0];
      int rssiVal = (int)rsc.rssi;
      if (rssiVal > 127) rssiVal -= 256; // Correct for signed value

      switch (packet_type) {
        case DATA: {
          DataPacket data;
          memcpy(&data, received_packet, sizeof(data));

          if (session_start_time == 0) { // Start session on first data packet
              session_start_time = millis();
              Serial.println("Session started.");
          }

          if (data.id != last_id) {
            char nome[10] = {0};
            strncpy(nome, data.payload, 9);
            Serial.println("Packet " + String(contador) + " | ID: " + String(data.id) + " | " + String(nome) + " | " + String(rssiVal) + " dBm");
            
            // Save to NVS
            String chave = "pacote" + String(contador);
            String valor = "ID: " + String(data.id) + "; " + String(nome) + "; RSSI: " + String(rssiVal);
            transmissao.putString(chave.c_str(), valor);

            total_bytes_received += sizeof(DataPacket);
            last_id = data.id;
            contador++;
          } else {
            Serial.println("Duplicate packet detected. ID: " + String(data.id) + ". Sending ACK again.");
          }

          // Send ACK
          AckPacket ack;
          ack.id = data.id;
          e22.sendFixedMessage(highByte(SENDER_ADDRESS), lowByte(SENDER_ADDRESS), LORA_CHANNEL, &ack, sizeof(ack));
          break;
        }
        case CONTROL: {
          ControlPacket control;
          memcpy(&control, received_packet, sizeof(control));

          if (control.control_type == END_SESSION) {
            Serial.println("Received END_SESSION signal.");
            calculate_throughput();
            
            // Reset for next session
            session_start_time = 0;
            total_bytes_received = 0;
            last_id = 0xFFFF;
            
            // Create a new namespace
            controle.begin("controle", false);
            int numTransm = controle.getInt("prox", 0);
            nameSpc = "transmissao" + String(numTransm);
            controle.putInt("prox", numTransm + 1);
            controle.end();
            transmissao.end();
            transmissao.begin(nameSpc.c_str(), false);
            Serial.println("New namespace created: " + nameSpc);
            contador = 0;
          }
          break;
        }
      }
      rsc.close();
    } else {
      Serial.println("Error receiving LoRa packet: " + rsc.status.getResponseDescription());
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

void calculate_throughput() {
  if (session_start_time == 0 || total_bytes_received == 0) {
    Serial.println("No data received to calculate throughput.");
    return;
  }
  unsigned long session_end_time = millis();
  float duration_s = (session_end_time - session_start_time) / 1000.0;
  float throughput_bps = (total_bytes_received * 8) / duration_s;

  Serial.println("--- THROUGHPUT RESULTS ---");
  Serial.println("Total Bytes Received: " + String(total_bytes_received));
  Serial.println("Total Session Time: " + String(duration_s, 2) + " s");
  Serial.println("Throughput: " + String(throughput_bps, 2) + " bps");
  Serial.println("---------------------------");
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