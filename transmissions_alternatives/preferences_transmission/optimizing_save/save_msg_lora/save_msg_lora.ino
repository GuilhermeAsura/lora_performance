#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"

// Configuração dos pinos do módulo LoRa
LoRa_E22 lora(&Serial2, 18, 21, 19);

Preferences controle;
Preferences transmissao;
Preferences leitor;

int contador = 0;
String nomeNamespace;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicializa LoRa
  lora.begin();

  // Cria um novo namespace exclusivo para essa execução
  controle.begin("controle", false);
  int numeroTransmissao = controle.getInt("prox", 0);
  nomeNamespace = "transmissao" + String(numeroTransmissao);
  controle.putInt("prox", numeroTransmissao + 1);
  controle.end();

  Serial.print("Namespace atual: ");
  Serial.println(nomeNamespace);
  Serial.println("Aguardando mensagens via LoRa...");
  Serial.println("Digite 'listar' para ver esta execução ou 'ler transmissaoX' para ver anteriores.");

  transmissao.begin(nomeNamespace.c_str(), false);
}

void loop() {
  // Recebe mensagens via LoRa
  ResponseContainer resposta = lora.receiveMessage();

  if (resposta.status.code == 1) {
    String mensagemRecebida = resposta.data;
    String chave = "msg" + String(contador);

    transmissao.putString(chave.c_str(), mensagemRecebida);

    Serial.print("Salvou: ");
    Serial.print(chave);
    Serial.print(" = ");
    Serial.println(mensagemRecebida);

    contador++;
  }

  // Comandos via Serial
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      listarMensagensAtuais();
    } else if (comando.startsWith("ler ")) {
      String nome = comando.substring(4); // extrai o nome após "ler "
      lerNamespace(nome);
    }
  }
}

void listarMensagensAtuais() {
  Serial.println("Mensagens salvas nesta execução:");
  for (int i = 0; i < contador; i++) {
    String chave = "msg" + String(i);
    String valor = transmissao.getString(chave.c_str(), "(vazia)");
    Serial.print("  ");
    Serial.print(chave);
    Serial.print(": ");
    Serial.println(valor);
  }
}

void lerNamespace(const String& nome) {
  if (leitor.begin(nome.c_str(), true)) { // modo somente leitura
    Serial.print("Lendo mensagens de: ");
    Serial.println(nome);

    for (int i = 0; i < 1000; i++) {
      String chave = "msg" + String(i);
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
