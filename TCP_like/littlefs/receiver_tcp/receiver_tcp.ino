#include "LoRa_E220.h"
#include "Arduino.h"
#include "LittleFS.h"

// Usando Serial2 para o ESP32 (pinos padrão: RX=16, TX=17, ajuste se necessário)
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX

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
  Serial.begin(115200); // Para depuração
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e220.begin();

  // Inicializa o LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Falha ao montar o LittleFS");
    while (1); // Para o programa em caso de erro
  }
  Serial.println("Receptor iniciado e LittleFS montado");
}

void loop() {
  if (e220.available() > 0) {
    // Recebe o pacote com RSSI
    ResponseStructContainer rsc = e220.receiveMessageRSSI(sizeof(DataPacket));
    if (rsc.status.code == 1) { // Sucesso na recepção
      DataPacket packet;
      memcpy(&packet, rsc.data, sizeof(DataPacket));

      if (packet.type == 0x01) { // Verifica se é um pacote de dados
        String mensagem = String(packet.data);
        int rssi = rsc.rssi; // Obtém RSSI do ResponseStructContainer

        // Salva o pacote recebido no LittleFS
        String log_entry = "Recebido: Seq " + String(packet.seq_num) + ", Mensagem: " + mensagem + ", RSSI: " + String(rssi);
        salvarDados(log_entry);

        // Verifica perda de pacotes
        if (primeiro_pacote) {
          ultimo_contador = packet.seq_num;
          primeiro_pacote = false;
        } else {
          if (packet.seq_num != ultimo_contador + 1) {
            log_entry = "Pacote(s) perdido(s) entre " + String(ultimo_contador) + " e " + String(packet.seq_num);
            Serial.println(log_entry);
            salvarDados(log_entry);
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
      String log_entry = "Erro ao receber: " + rsc.status.getResponseDescription();
      Serial.println(log_entry);
      salvarDados(log_entry);
    }
    rsc.close();
  }
}

void salvarDados(const String& dados) {
  File arquivo = LittleFS.open("/received_packets.txt", FILE_APPEND);
  if (!arquivo) {
    Serial.println("Falha ao abrir o arquivo para escrita");
    return;
  }
  arquivo.println(dados);
  arquivo.close();
}

// Função opcional para ler os dados salvos (chamar manualmente após o experimento)
void lerDadosSalvos() {
  File arquivo = LittleFS.open("/received_packets.txt", FILE_READ);
  if (!arquivo) {
    Serial.println("Falha ao abrir o arquivo para leitura");
    return;
  }
  Serial.println("Conteúdo do arquivo /received_packets.txt:");
  while (arquivo.available()) {
    String linha = arquivo.readStringUntil('\n');
    Serial.println(linha);
  }
  arquivo.close();
}
