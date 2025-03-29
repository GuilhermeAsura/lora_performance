#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#include "Arduino.h"
#include <Preferences.h>

// Definição dos pinos para ESP32 (ajuste conforme o circuito)
//SoftwareSerial mySerial(2, 3); // RX, TX
//LoRa_E220 e220(&mySerial, 4, 5, 6); // M0, M1, AUX

// Usando Serial2 para o ESP32 (pinos padrão: RX=16, TX=17, ajuste se necessário)
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX (Corrigido de Serial12 para Serial2)

Preferences preferencias;
uint16_t ultimo_contador = 0;
bool primeiro_pacote = true;
int pacote_index = 0; // Índice para chaves no Preferences

void setup() {
  Serial.begin(115200); // Para depuração básica
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados  
  e220.begin();

  // Inicializa a memória Preferences
  preferencias.begin("loraDados", false); // Namespace "loraDados", modo read/write
  Serial.println("Receptor iniciado e Preferences montado");
}

void loop() {
  if (e220.available() > 0) {
    ResponseContainer rc = e220.receiveMessage();
    if (rc.status.code == 1) { // Sucesso na recepção
      String mensagem = rc.data;

      // Salva o pacote recebido no Preferences
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
          salvarDados(perda); // Salva a perda no Preferences
        }
        ultimo_contador = contador_recebido;
      }
    } else {
      Serial.println("Erro ao receber: " + rc.status.getResponseDescription());
    }
  }
}

void salvarDados(const String& dados) {
  String chave = "recv" + String(pacote_index);
  preferencias.putString(chave.c_str(), dados); // Salva como chave-valor
  pacote_index++; // Incrementa o índice para a próxima chave
}

// Função opcional para ler os pacotes salvos (chamar manualmente após o experimento)
void lerPacotesSalvos() {
  Serial.println("Conteúdo salvo no Preferences:");
  for (int i = 0; i < pacote_index; i++) {
    String chave = "recv" + String(i);
    String valor = preferencias.getString(chave.c_str(), "Nenhum dado");
    Serial.println(chave + ": " + valor);
  }
}
