#include <Preferences.h>

Preferences controle;
Preferences transmissao;

int indice = 0;
unsigned long ultimoSalvamento = 0;
const unsigned long intervalo = 1000;
const int LIMITE_PACOTES = 16;

String nomeNamespace;
bool salvamentoAtivo = true;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Define o namespace da transmissão atual
  controle.begin("controle", false);
  int numeroTransmissao = controle.getInt("prox", 0);
  nomeNamespace = "transmissao" + String(numeroTransmissao);
  controle.putInt("prox", numeroTransmissao + 1);
  controle.end();

  Serial.print("Namespace atual: ");
  Serial.println(nomeNamespace);
  Serial.println("Aguardando salvamentos automáticos. Envie 'listar' para ver os dados.");

  transmissao.begin(nomeNamespace.c_str(), false);
}

void loop() {
  // Salvamento automático até o limite de pacotes
  if (salvamentoAtivo && millis() - ultimoSalvamento >= intervalo) {
    if (indice >= LIMITE_PACOTES) {
      salvamentoAtivo = false;
      Serial.println("Limite de pacotes atingido. Envie 'listar' para ver os dados.");
      return;
    }

    String chave = "msg" + String(indice);
    String mensagem = "Mensagem " + String(indice);
    transmissao.putString(chave.c_str(), mensagem);

    Serial.print("Salvou: ");
    Serial.print(chave);
    Serial.print(" = ");
    Serial.println(mensagem);

    indice++;
    ultimoSalvamento = millis();
  }

  // Leitura via Serial: digite 'listar'
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim(); // remove espaços e quebras de linha

    if (comando == "listar") {
      Serial.println("Mensagens salvas nesta execução:");
      for (int i = 0; i < indice; i++) {
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
