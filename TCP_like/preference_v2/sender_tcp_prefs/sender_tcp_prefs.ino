#include "LoRa_E220.h"
#include "Arduino.h"
#include <Preferences.h>

// Pinos para ESP32
#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 19, 21, 15); // M0, M1, AUX

#define MAX_RETRANSMISSIONS 3  // Máximo de retransmissões
#define TIMEOUT 3000           // Timeout para SYN-ACK (ms)

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
String nomeNamespace;       // Namespace da sessão atual
uint16_t contador = 0;     // Contador para sequência de mensagens
int log_index = 0;         // Índice para chaves de log

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configura namespace dinâmico
  controle.begin("controle", false);
  int numeroTransmissao = controle.getInt("prox", 0);
  nomeNamespace = "transmissao" + String(numeroTransmissao);
  controle.putInt("prox", numeroTransmissao + 1);
  controle.end();

  transmissao.begin(nomeNamespace.c_str(), false);

  Serial.print("Namespace atual: ");
  Serial.println(nomeNamespace);
  Serial.println("Transmissor iniciado. Envie 'listar' para ver logs.");

  // Inicializa comunicação LoRa
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e220.begin();
}

void loop() {
  // Inicia conexão com handshake
  bool conexao_estabelecida = false;
  int tentativas = 0;
  uint16_t session_id = random(1000);

  while (!conexao_estabelecida && tentativas < MAX_RETRANSMISSIONS) {
    // Envia SynPacket
    SynPacket syn = {0x03, session_id};
    e220.sendMessage(&syn, sizeof(syn));
    String log_entry = "Enviado SYN, session_id=" + String(session_id) + ", Tentativa " + String(tentativas + 1);
    Serial.println(log_entry);
    salvarDados(log_entry);

    // Espera por SynAckPacket
    unsigned long start_time = millis();
    while (millis() - start_time < TIMEOUT) {
      if (e220.available() > 0) {
        ResponseStructContainer rsc = e220.receiveMessage(sizeof(SynAckPacket));
        if (rsc.status.code == 1) {
          SynAckPacket synAck;
          memcpy(&synAck, rsc.data, sizeof(SynAckPacket));
          if (synAck.type == 0x04 && synAck.session_id == session_id) {
            conexao_estabelecida = true;
            log_entry = "Recebido SYN-ACK, session_id=" + String(session_id);
            Serial.println(log_entry);
            salvarDados(log_entry);
            rsc.close();
            break;
          }
        }
        rsc.close();
      }
    }

    if (!conexao_estabelecida) {
      tentativas++;
      if (tentativas < MAX_RETRANSMISSIONS) {
        String log_entry = "Nenhum SYN-ACK, retransmitindo SYN, session_id=" + String(session_id);
        Serial.println(log_entry);
        salvarDados(log_entry);
      } else {
        log_entry = "Falha na conexão após " + String(MAX_RETRANSMISSIONS) + " tentativas, session_id=" + String(session_id);
        Serial.println(log_entry);
        salvarDados(log_entry);
        delay(2000); // Espera antes de tentar nova conexão
        return;
      }
    }
  }

  // Se conexão estabelecida, envia mensagem
  if (conexao_estabelecida) {
    String mensagem = "mensagem" + String(contador);
    ResponseStatus rs = e220.sendMessage(mensagem);
    if (rs.code == 1) {
      String log_entry = "Enviado: Seq " + String(contador) + ", Mensagem: " + mensagem;
      Serial.println(log_entry);
      salvarDados(log_entry);
      contador++;
    } else {
      String log_entry = "Erro ao enviar Seq " + String(contador) + ": " + rs.getResponseDescription();
      Serial.println(log_entry);
      salvarDados(log_entry);
    }
  }

  // Verifica comando 'listar' no Serial
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando == "listar") {
      Serial.println("Logs salvos nesta sessão:");
      for (int i = 0; i < log_index; i++) {
        String chave = "log" + String(i);
        String valor = transmissao.getString(chave.c_str(), "(vazio)");
        Serial.print("  ");
        Serial.print(chave);
        Serial.print(": ");
        Serial.println(valor);
      }
    }
  }

  delay(2000); // Intervalo entre tentativas
}

void salvarDados(const String& dados) {
  String chave = "log" + String(log_index);
  transmissao.putString(chave.c_str(), dados);
  log_index++;
}
