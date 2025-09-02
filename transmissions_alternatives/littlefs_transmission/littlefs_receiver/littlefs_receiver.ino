#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#include "Arduino.h"
#include "LittleFS.h"

// Definição dos pinos para ESP32 (ajuste conforme sua conexão)
//SoftwareSerial mySerial(2, 3); // RX, TX
//LoRa_E220 e220(&mySerial, 4, 5, 6); // M0, M1, AUX
//Usando Serial2 para o ESP32 (pinos padrão: RX=16, TX=17, ajuste se necessário)
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX (Corrigido de Serial12 para Serial2)

uint16_t ultimo_contador = 0;
bool primeiro_pacote = true;

void setup() {
  Serial.begin(115200); // Para depuração básica
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados
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
    ResponseContainer rc = e220.receiveMessage();
    if (rc.status.code == 1) { // Sucesso na recepção
      String mensagem = rc.data;

      // Salva o pacote recebido no LittleFS
      salvarDados(mensagem);

      // Extrai o número do pacote da mensagem para detecção de perda
      int pos = mensagem.lastIndexOf(' ');
      uint16_t contador_recebido = mensagem.substring(pos + 1).toInt();

      // Verifica perda de pacotes
      if (primeiro_pacote) {
        ultimo_contador = contador_recebido;
        primeiro_pacote = false;
      } else {
        if (contador_recebido != ultimo_contador + 1) {
          String perda = "Pacote(s) perdido(s) entre " + String(ultimo_contador) + " e " + String(contador_recebido);
          Serial.println(perda); // Imprime apenas perdas para depuração mínima
          salvarDados(perda); // Salva a perda no LittleFS
        }
        ultimo_contador = contador_recebido;
      }
    } else {
      Serial.println("Erro ao receber: " + rc.status.getResponseDescription());
    }
  }
}

void salvarDados(const String& dados) {
  File arquivo = LittleFS.open("/received_packets.txt", FILE_APPEND);
  if (!arquivo) {
    Serial.println("Falha ao abrir o arquivo");
    return;
  }
  arquivo.println(dados);
  arquivo.close();
}

// Função opcional para ler os pacotes salvos (chamar manualmente após o experimento)
void lerPacotesSalvos() {
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
