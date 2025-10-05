#include "LoRa_E22.h"
#include "Arduino.h"
#include <Preferences.h>
#include <nvs_flash.h>

// Pinout & Setup
#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

// Addressing
#define MY_ADDRESS 0x0010
#define LORA_CHANNEL 0x12

// Packet Types
enum PacketType : uint8_t {
  CONNECT = 0x01,
  CONNACK = 0x02,
  DATA    = 0x03,
  ACK     = 0x04,
  CONTROL = 0x05
};

// Control Types
enum ControlType : uint8_t {
  END_SESSION = 0x01
};

// Packet Structures
const int PACKET_SIZE = 32;

struct ConnectPacket {
  uint8_t type;
  uint16_t client_id;
  uint16_t next_packet_id;
  uint8_t padding[PACKET_SIZE - 5];
} __attribute__((packed));

struct ConnAckPacket {
  uint8_t type = CONNACK;
  uint8_t return_code = 0;
} __attribute__((packed));

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

// Global variables
unsigned long global_session_start_time = 0;
unsigned long global_total_bytes_received = 0;
uint16_t last_id = 0xFFFF;
Preferences controle;
Preferences transmissao;
int contador = 0;
String nameSpc;

// FreeRTOS task handles
TaskHandle_t LoRaReceiverTaskHandle;
TaskHandle_t SerialTaskHandle;

// Prototypes
void calculate_throughput();
void list();
void read(const String& nome);
void limparMemoria();

// Task 1: LoRa Receiver (CORE 1)
void LoRaReceiverTask(void *pvParameters) {
  Serial.println("Task Receptora LoRa iniciada no Core 1.");
  byte received_packet[PACKET_SIZE];

  for (;;) {
    if (e22.available() > 0) {
      ResponseStructContainer rsc = e22.receiveMessageRSSI(PACKET_SIZE);

      if (rsc.status.code == E22_SUCCESS && rsc.data != nullptr) {
        memcpy(received_packet, rsc.data, sizeof(received_packet));
        uint8_t packet_type = received_packet[0];
        int rssiVal = (int)rsc.rssi;
        if (rssiVal > 127) rssiVal -= 256;

        switch (packet_type) {
          case CONNECT: {
            ConnectPacket connect;
            memcpy(&connect, received_packet, sizeof(connect));
            Serial.println("Recebido CONNECT do client ID: " + String(connect.client_id));

            last_id = connect.next_packet_id == 0 ? 0xFFFF : connect.next_packet_id - 1;
            if (global_session_start_time == 0) {
              global_session_start_time = millis();
            }

            ConnAckPacket connack_packet;
            e22.sendFixedMessage(highByte(connect.client_id), lowByte(connect.client_id), LORA_CHANNEL, &connack_packet, sizeof(connack_packet));
            Serial.println("CONNACK enviado.");
            break;
          }
          case DATA: {
            DataPacket data;
            memcpy(&data, received_packet, sizeof(data));

            if (data.id != last_id) {
              char nome[7] = {0};
              strncpy(nome, data.payload, 6);
              Serial.println("PACOTE " + String(contador) + " | ID: " + String(data.id) + " | " + String(nome) + " | " + String(rssiVal) + " dBm");
              
              String chave = "pacote" + String(contador);
              String valor = "ID: " + String(data.id) + "; " + String(nome) + "; RSSI: " + String(rssiVal);
              transmissao.putString(chave.c_str(), valor);
              
              global_total_bytes_received += sizeof(DataPacket);
              last_id = data.id;
              contador++;
            } else {
              Serial.println("Pacote duplicado recebido. ID: " + String(data.id) + ". Descartado.");
            }

            AckPacket ack;
            ack.id = data.id;
            e22.sendFixedMessage(highByte(0x0010), lowByte(0x0010), LORA_CHANNEL, &ack, sizeof(ack));
            break;
          }
          case CONTROL: {
            ControlPacket control;
            memcpy(&control, received_packet, sizeof(control));

            if (control.control_type == END_SESSION) {
              Serial.println("Recebido sinal de FIM DE SESSÃO.");
              calculate_throughput();
              global_session_start_time = 0;
              global_total_bytes_received = 0;
              last_id = 0xFFFF;
              
              controle.begin("controle", false);
              int numTransm = controle.getInt("prox", 0);
              nameSpc = "transmissao" + String(numTransm);
              controle.putInt("prox", numTransm + 1);
              controle.end();
              transmissao.end();
              transmissao.begin(nameSpc.c_str(), false);
              Serial.println("Novo namespace criado: " + nameSpc);
              contador = 0;
            }
            break;
          }
        }
        rsc.close();
      } else {
        Serial.println("Erro ao receber pacote LoRa: " + rsc.status.getResponseDescription());
      }
    }
  // Release the core briefly
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

// TASK 2: Serial command handler (Core 0)
void SerialTask(void *pvParameters) {
  Serial.println("Task Serial iniciada no Core 0.");
  for (;;) {
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
    vTaskDelay(50 / portTICK_PERIOD_MS); // Checks for commands every 50ms
  }
}


// Setup function
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  // LoRa radio configuration
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration config = *(Configuration*) c.data;
    config.ADDH = highByte(MY_ADDRESS);
    config.ADDL = lowByte(MY_ADDRESS);
    config.CHAN = LORA_CHANNEL;
    config.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    config.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;
  // *** IMPROVEMENT: ENABLE LISTEN BEFORE TALK (LBT) ***
    config.TRANSMISSION_MODE.enableLBT = LBT_ENABLED;
    
    ResponseStatus rs = e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("Módulo Receptor LoRa configurado. Endereço: " + String(MY_ADDRESS, HEX));
    } else {
      Serial.println("Erro ao configurar módulo: " + rs.getResponseDescription());
    }
    c.close();
  } else {
    Serial.println("Erro ao ler configuração inicial: " + c.status.getResponseDescription());
  }

  // Initialize Preferences
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "transmissao" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  transmissao.begin(nameSpc.c_str(), false);
  Serial.println("Receptor iniciado. Namespace: " + nameSpc);

  // Create tasks and pin them to cores
  xTaskCreatePinnedToCore(LoRaReceiverTask, "LoRaReceiverTask", 4096, NULL, 1, &LoRaReceiverTaskHandle, 1);
  xTaskCreatePinnedToCore(SerialTask, "SerialTask", 4096, NULL, 1, &SerialTaskHandle, 0);
}

// Main loop is empty because FreeRTOS manages the tasks
void loop() {
  vTaskDelete(NULL); // Delete the loop task as it's no longer needed
}

// Utility functions
void calculate_throughput() {
  if (global_session_start_time == 0 || global_total_bytes_received == 0) {
    Serial.println("Nenhum dado recebido para calcular throughput.");
    return;
  }
  unsigned long session_end_time = millis();
  float duration_s = (session_end_time - global_session_start_time) / 1000.0;
  float throughput_bps = (global_total_bytes_received * 8) / duration_s;

  Serial.println("--- RESULTADO DO THROUGHPUT ---");
  Serial.println("Total de Bytes Recebidos: " + String(global_total_bytes_received));
  Serial.println("Tempo Total da Sessão: " + String(duration_s, 2) + " s");
  Serial.println("Throughput: " + String(throughput_bps, 2) + " bps");
  Serial.println("------------------------------");
}

void read(const String& nome) {
  Preferences leitor;
  if (leitor.begin(nome.c_str(), true)) {
    Serial.println("Lendo dados de: " + nome);
    uint32_t i = 0;
    String key;
    do {
      key = "pacote" + String(i);
      if (leitor.isKey(key.c_str())) {
        String valor = leitor.getString(key.c_str(), "");
        Serial.println("  " + key + ": " + valor);
      }
      i++;
    } while(leitor.isKey(key.c_str()));
    leitor.end();
  } else {
    Serial.println("Namespace não encontrado.");
  }
}

void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Namespaces disponíveis:");
  for (int i = 0; i < total; i++) {
    Serial.println("  transmissao" + String(i));
  }
}

void limparMemoria() {
  Serial.println("Apagando a memória NVS...");
  delay(500);
  esp_err_t status = nvs_flash_erase();
  if (status == ESP_OK) {
    Serial.println("Memória NVS apagada com sucesso!");
  } else {
    Serial.print("Erro ao apagar NVS: ");
    Serial.println(status);
  }
  Serial.println("Reiniciando ESP...");
  delay(2000);
  ESP.restart();
}