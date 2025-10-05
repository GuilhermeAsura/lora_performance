#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"
#include <nvs_flash.h>

// Pinout & Setup
#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

// Addressing
#define MY_ADDRESS 0x0010
#define BROKER_ADDRESS 0x0010
#define DEST_ADDH 0x00
#define DEST_ADDL 0x10
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
  uint8_t type = CONNECT;
  uint16_t client_id = MY_ADDRESS;
  uint16_t next_packet_id;
  uint8_t padding[PACKET_SIZE - 5];
} __attribute__((packed));

struct ConnAckPacket {
  uint8_t type = CONNACK;
  uint8_t return_code;
} __attribute__((packed));

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

// Application state and global variables
bool connected = false;
uint16_t packet_id_counter = 0;
Preferences controle;
Preferences transmissao;
Preferences unsent_packets; // Namespace for failed packets
String nameSpc;

// FreeRTOS task handles and queues
TaskHandle_t LoRaTaskHandle;
TaskHandle_t AppTaskHandle;
QueueHandle_t lora_queue;

// Function to save unsent packets
void save_unsent_packet(const DataPacket& packet) {
  unsent_packets.begin("unsent", false);
  String key = "unsent_" + String(packet.id);
  unsent_packets.putBytes(key.c_str(), &packet, sizeof(DataPacket));
  unsent_packets.end();
  Serial.println("Pacote ID: " + String(packet.id) + " salvo para envio posterior.");
}

// Function prototypes
void connect_to_broker();
void send_data_packet(uint16_t id);
void send_end_session();
void list();
void read(const String& nome);
void limparMemoria();

// Task 1: LoRa Operations(Core 1)
void LoRaTask(void *pvParameters) {
  Serial.println("Task LoRa iniciada no Core 1.");
  DataPacket packet_to_send;

  for (;;) {
    // Waits for a packet in the queue to be sent
    if (xQueueReceive(lora_queue, &packet_to_send, portMAX_DELAY) == pdPASS) {
      if (!connected) {
        connect_to_broker();
      }
      if (connected) {
        send_data_packet(packet_to_send.id);
      }
    }
  }
}

// Task 2: application logic (Core 0)
void AppTask(void *pvParameters) {
  Serial.println("Task de Aplicação iniciada no Core 0.");
  unsigned long last_send_time = 0;

  for (;;) {
    // Logic for sending data packets
    if (connected && millis() - last_send_time > 1000) {
      DataPacket new_packet;
      new_packet.id = packet_id_counter;
      strncpy(new_packet.payload, "TESTE", sizeof(new_packet.payload));
      
  // Send packet to LoRa task queue
      if (xQueueSend(lora_queue, &new_packet, portTICK_PERIOD_MS * 100) != pdPASS) {
        Serial.println("Falha ao enviar pacote para a fila LoRa.");
      }
      last_send_time = millis();
    }

  // Process serial commands
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
  // delay to avoid overloading the core
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

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
  // *** IMPROVEMENT: ENABLE LISTEN BEFORE TALK (LBT) ***
    config.TRANSMISSION_MODE.enableLBT = LBT_ENABLED;
    e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    Serial.println("Módulo Transmissor LoRa configurado. Endereço: 0x" + String(MY_ADDRESS, HEX));
    c.close();
  }

  // Initialize Preferences
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "LoRaTX" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();
  transmissao.begin(nameSpc.c_str(), false);
  Serial.println("Transmissor iniciado no namespace: " + nameSpc);

  // Create the communication queue between tasks
  lora_queue = xQueueCreate(10, sizeof(DataPacket));

  // Create the tasks and pin them to cores
  xTaskCreatePinnedToCore(LoRaTask, "LoRaTask", 4096, NULL, 1, &LoRaTaskHandle, 1);
  xTaskCreatePinnedToCore(AppTask, "AppTask", 4096, NULL, 1, &AppTaskHandle, 0);

  // Try to connect to the broker at startup
  connect_to_broker();
}

// Main loop is empty because FreeRTOS manages the tasks
void loop() {
  vTaskDelete(NULL); // Delete a loop's task 
}


// LoRa communication functions (called by LoRaTask)
void connect_to_broker() {
  Serial.println("Enviando CONNECT para o broker...");
  ConnectPacket connect_packet;
  connect_packet.next_packet_id = packet_id_counter;
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
  strncpy(packet.payload, "TESTE", sizeof(packet.payload));

  bool ack_received = false;
  int retries = 0;
  
  while (!ack_received && retries < 3) {
    char nome[7] = {0};
    strncpy(nome, packet.payload, 6);
    Serial.println("PACOTE " + String(id) + " | ID: " + String(id) + " | Nome: " + String(nome));
    e22.sendFixedMessage(DEST_ADDH, DEST_ADDL, LORA_CHANNEL, &packet, sizeof(packet));

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
      Serial.println("Timeout: ACK não recebido para o pacote ID: " + String(id) + ". Tentativa " + String(retries + 1));
      retries++;
    }
  }

  if (ack_received) {
    transmissao.putString(("pacote" + String(id)).c_str(), ("|id:" + String(id) + "|dados:" + String(packet.payload)).c_str());
    packet_id_counter++;
  } else {
    Serial.println("Falha ao enviar o pacote ID: " + String(id) + " após 3 tentativas. Abortando e salvando pacote.");
  // *** IMPROVEMENT: SAVE FAILED PACKET ***
    save_unsent_packet(packet);
    connected = false;
  }
}

// Control and NVS functions (called by AppTask)
void send_end_session() {
  Serial.println("Enviando sinal de fim de sessão...");
  ControlPacket end_packet;
  end_packet.control_type = END_SESSION;
  e22.sendFixedMessage(DEST_ADDH, DEST_ADDL, LORA_CHANNEL, &end_packet, sizeof(end_packet));

  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "LoRaTX" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();
  transmissao.end();
  transmissao.begin(nameSpc.c_str(), false);
  Serial.println("Novo namespace criado: " + nameSpc);
  packet_id_counter = 0;
}

void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();
  Serial.println("Namespaces disponíveis:");
  for (int i = 0; i < total; i++) {
    Serial.println("  LoRaTX" + String(i));
  }
}

void read(const String& nome) {
  Preferences leitor;
  if (leitor.begin(nome.c_str(), true)) {
    Serial.println("Lendo pacotes de: " + nome);
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