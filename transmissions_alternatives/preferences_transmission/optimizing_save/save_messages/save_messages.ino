#include "LoRa_E220.h"
#include "Arduino.h"
#include <Preferences.h>

// Pin definitions for ESP32 (adjust as needed)
#define RX_PIN 16  // RX pin for Serial2
#define TX_PIN 17  // TX pin for Serial2
LoRa_E220 e220(&Serial2, 4, 5, 6); // M0, M1, AUX pins

Preferences controle;       // Namespace for session counter
Preferences transmissao;    // Namespace for message storage

String nomeNamespace;       // Current session namespace
int pacote_index = 0;       // Index for message keys
uint16_t ultimo_contador = 0; // Last received packet counter
bool primeiro_pacote = true;  // Flag for first packet

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Set up the dynamic namespace for this session
  controle.begin("controle", false);
  int numeroTransmissao = controle.getInt("prox", 0);
  nomeNamespace = "transmissao" + String(numeroTransmissao);
  controle.putInt("prox", numeroTransmissao + 1);
  controle.end();

  Serial.print("Namespace atual: ");
  Serial.println(nomeNamespace);
  Serial.println("Aguardando mensagens via LoRa. Envie 'listar' para ver os dados.");

  // Open the Preferences namespace for this session
  transmissao.begin(nomeNamespace.c_str(), false);

  // Initialize LoRa communication
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e220.begin();
}

void loop() {
  // Check for incoming LoRa messages
  if (e220.available() > 0) {
    ResponseContainer rc = e220.receiveMessage();
    if (rc.status.code == 1) { // Successful reception
      String mensagem = rc.data;
      mensagem.trim(); // Remove extra whitespace

      // Save the received message
      String chave = "msg" + String(pacote_index);
      transmissao.putString(chave.c_str(), mensagem);
      Serial.print("Recebido e salvo: ");
      Serial.println(mensagem);
      pacote_index++;

      // Extract packet counter and check for loss
      int pos = mensagem.lastIndexOf(' ');
      uint16_t contador_recebido = mensagem.substring(pos + 1).toInt();

      if (primeiro_pacote) {
        ultimo_contador = contador_recebido;
        primeiro_pacote = false;
      } else if (contador_recebido != ultimo_contador + 1) {
        String perda = "Pacote(s) perdido(s) entre " + String(ultimo_contador) + " e " + String(contador_recebido);
        Serial.println(perda);
        chave = "msg" + String(pacote_index);
        transmissao.putString(chave.c_str(), perda);
        pacote_index++;
        ultimo_contador = contador_recebido;
      } else {
        ultimo_contador = contador_recebido;
      }
    } else {
      Serial.println("Erro ao receber: " + rc.status.getResponseDescription());
    }
  }

  // Check for Serial command to list saved messages
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      Serial.println("Mensagens salvas nesta execução:");
      for (int i = 0; i < pacote_index; i++) {
        String chave = "msg" + String(i);
        String valor = transmissao.getString(chave.c_str(), "(vazia)");
        Serial.print("  ");
        Serial.print(chave);
        Serial.print(": ");
        Serial.println(valor);
      }
    }
  }
}

// Optional function to clear saved data (call manually if needed)
void limparDados() {
  transmissao.clear();
  pacote_index = 0;
  Serial.println("Dados limpos.");
}