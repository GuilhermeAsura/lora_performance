#include "LoRa_E220.h"
//#include <SoftwareSerial.h> //  dá problema demais
#include "FS.h"
#include "SPIFFS.h"

// Definição dos pinos para ESP32 (ajuste conforme o circuito)
//SoftwareSerial mySerial(2, 3); // RX, TX
//LoRa_E220 e22(&mySerial, 4, 5, 6); // M0, M1, AUX

// Usando Serial2 para o ESP32 (pinos padrão: RX=16, TX=17, ajuste se necessário)
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX (Corrigido de Serial12 para Serial2)

// Estrutura do pacote de dados
struct DataPacket {
  uint8_t type;         // Tipo: 0x01 para dados
  uint16_t seq_num;     // Número de sequência
  char data[240];       // Dados
};

// Estrutura do pacote de ACK
struct AckPacket {
  uint8_t type = 0x02;  // Tipo: 0x02 para ACK
  uint16_t seq_num;     // Número de sequência do pacote confirmado
};

uint16_t ultimo_contador = 0;
bool primeiro_pacote = true;

void setup() {
  Serial.begin(9600); // Para depuração
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados
  e220.begin();
  // Inicializa o SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar o SPIFFS");
    while (1);
  }
  Serial.println("Receptor iniciado e SPIFFS montado");
}

void loop() {
  if (e220.available() > 0) {
    // Recebe o pacote com RSSI
    ResponseStructContainer rsc = e220.receiveMessageRSSI(sizeof(DataPacket)); // Alterado para incluir RSSI
    if (rsc.status.code == 1) { // Sucesso na recepção
      DataPacket packet;
      memcpy(&packet, rsc.data, sizeof(DataPacket));

      if (packet.type == 0x01) { // Verifica se é um pacote de dados
        String mensagem = String(packet.data);
        int rssi = rsc.rssi; // Obtém RSSI do ResponseStructContainer

        // Salva o pacote recebido no SPIFFS
        File file = SPIFFS.open("/received_packets.txt", "a");
        if (file) {
          file.println("Recebido: Seq " + String(packet.seq_num) + ", Mensagem: " + mensagem + ", RSSI: " + String(rssi));
          file.close();
        } else {
          Serial.println("Falha ao abrir o arquivo para escrita");
        }

        // Verifica perda de pacotes
        if (primeiro_pacote) {
          ultimo_contador = packet.seq_num;
          primeiro_pacote = false;
        } else {
          if (packet.seq_num != ultimo_contador + 1) {
            Serial.println("Pacote(s) perdido(s) entre " + String(ultimo_contador) + " e " + String(packet.seq_num));
            file = SPIFFS.open("/received_packets.txt", "a");
            if (file) {
              file.println("Pacote(s) perdido(s) entre " + String(ultimo_contador) + " e " + String(packet.seq_num));
              file.close();
            }
          }
          ultimo_contador = packet.seq_num;
        }

        // Envia ACK
        AckPacket ack;
        ack.seq_num = packet.seq_num;
        e220.sendMessage((uint8_t*)&ack, sizeof(AckPacket));
        Serial.println("ACK enviado para Seq " + String(packet.seq_num));
      }
    } else {
      Serial.println("Erro ao receber: " + rsc.status.getResponseDescription());
    }
    rsc.close();
  }
}

// Função opcional para ler pacotes salvos (chamar manualmente após o experimento)
void lerPacotesSalvos() {
  File file = SPIFFS.open("/received_packets.txt", "r");
  if (!file) {
    Serial.println("Falha ao abrir o arquivo para leitura");
    return;
  }
  Serial.println("Conteúdo do arquivo /received_packets.txt:");
  while (file.available()) {
    String linha = file.readStringUntil('\n');
    Serial.println(linha);
  }
  file.close();
}
