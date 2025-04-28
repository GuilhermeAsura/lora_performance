#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E220.h"

// Pinos para ESP32
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 lora(&Serial2, 19, 21, 15); // M0, M1, AUX

// Estruturas de pacotes para handshake
struct SynPacket {
  uint8_t type = 0x03;  // SYN
  uint16_t session_id;
};

struct SynAckPacket {
  uint8_t type = 0x04;  // SYN-ACK
  uint16_t session_id;
};

Preferences controle;       // Namespace para contador de sessões
Preferences transmissao;    // Namespace para armazenamento
Preferences leitor;         // Namespace para leitura
int contador = 0;          // Contador de pacotes recebidos
String nameSpc;            // Namespace da sessão atual

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  lora.begin();

  // Configura namespace dinâmico
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "transmissao" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  Serial.print("Namespace atual: ");
  Serial.println(nameSpc);
  Serial.println("Receptor iniciado. Use 'listar' ou 'transmissaoX'.");

  transmissao.begin(nameSpc.c_str(), false);
}

void loop() {
  if (lora.available() > 0) {
    // Tenta receber um SynPacket primeiro
    ResponseStructContainer rsc = lora.receiveMessage(sizeof(SynPacket));
    if (rsc.status.code == 1) {
      uint8_t type;
      memcpy(&type, rsc.data, sizeof(type));

      if (type == 0x03) { // SynPacket
        SynPacket syn;
        memcpy(&syn, rsc.data, sizeof(syn));
        String log_entry = "Recebido SYN, session_id=" + String(syn.session_id);
        Serial.println(log_entry);
        salvarDados(log_entry);

        // Envia SynAckPacket
        SynAckPacket synAck = {0x04, syn.session_id};
        lora.sendMessage(&synAck, sizeof(synAck));
        log_entry = "Enviado SYN-ACK, session_id=" + String(syn.session_id);
        Serial.println(log_entry);
        salvarDados(log_entry);
      }
      rsc.close();
    }

    // Tenta receber mensagem (string)
    ResponseContainer rc = lora.receiveMessage();
    if (rc.status.code == 1) {
      String msg = rc.data;
      msg.trim();
      String chave = "pacote" + String(contador);
      transmissao.putString(chave.c_str(), msg);
      String log_entry = "Salvou: " + chave + " = " + msg;
      Serial.println(log_entry);
      salvarDados(log_entry);
      contador++;
    }
  }

  // Comandos via Serial
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      list();
    } else if (comando.startsWith("transmissao")) {
      read(comando);
    } else {
      Serial.println("Comando inválido. Use 'listar' ou 'transmissaoX'.");
    }
  }
}

void salvarDados(const String& dados) {
  String chave = "log" + String(contador);
  transmissao.putString(chave.c_str(), dados);
}

void read(const String& nome) {
  if (leitor.begin(nome.c_str(), true)) {
    Serial.print("Lendo mensagens de: ");
    Serial.println(nome);
    for (int i = 0; i < 1000; i++) {
      String chave = "pacote" + String(i);
      String valor = leitor.getString(chave.c_str(), "");
      if (valor == "") break;
      Serial.print("  ");
      Serial.print(chave);
      Serial.print(": ");
      Serial.println(valor);
    }
    leitor.end();
  } else {
    Serial.println("Namespace não encontrado ou inválido.");
  }
}

void list() {
  controle.begin("controle", true);
  int totalTransmissoes = controle.getInt("prox", 0);
  controle.end();
  Serial.println("Namespaces salvos:");
  for (int i = 0; i < totalTransmissoes; i++) {
    Serial.print("  transmissao");
    Serial.println(i);
  }
}
