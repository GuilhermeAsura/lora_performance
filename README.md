# 📡 **Research Project: Application and Performance of LoRa Technology in Rockets**

## 🛰️ **About the Project**
This repository contains the codes developed as part of my Scientific Initiation, entitled:

**"Application and Performance of LoRa Technology during rocket flight based on communication integrity"**.

The main objective of this work is to evaluate the efficiency of LoRa communication, focusing on **packet loss** and **transmission rate**, both in **static** experiments (with both radios stationary) and in **dynamic** experiments (with a radio in motion, simulating the flight of a rocket).

## ⚡ **Project Objectives**
- 📶 **Analyze the transmission rate and packet loss** in LoRa communication.
- 🔄 **Implement and test the TCP Communication Protocol** for LoRa P2P.
- 💾 **Store communication data in the ESP32's internal memory**, minimizing the impact on throughput.
- 🚀 **Apply the results to rocket telemetry**, targeting competitions such as the **Latin American Space Challenge (LASC)**.

## 🛠️ **Hardware Used**
- **Microcontroller:** ESP32
- **LoRa Modules:** Ebyte E220-400T22D
- **Communication:** UART between ESP32 and LoRa modules

## 📚 **Libraries Used**
For configuration and sending data through LoRa radios, we use the following libraries:
- [EByte LoRa E220 Series Library](https://github.com/xreef/EByte_LoRa_E220_Series_Library)
- [EByte LoRa E22 Series Library](https://github.com/xreef/EByte_LoRa_E22_Series_Library)
- [LoRa E32 Series Library](https://github.com/xreef/LoRa_E32_Series_Library)

## 📂 **Repository Structure**
📁 **/codigo_principal** - Basic implementation of LoRa communication for performance analysis.

📁 **/implementacao_tcp** - Attempt to implement the TCP Protocol for LoRa communication.

📁 **/arrazenamento_interno** - Methods for saving data in the ESP32's internal memory.

## 🤝 **Collaboration**
This project is being developed in collaboration with the **Propulsion and Aerospace Technology Team** of the **Federal University of Uberlândia (UFU)**. Our goal is to better understand the behavior of LoRa communication in rockets for **telemetry** applications and improve the efficiency of data sending during flight.

## 📩 **Contact**
If you have any questions or suggestions, feel free to open an **issue** or get in touch!


---

PT-BR:
# 🛰️ **Projeto de Pesquisa: Aplicação e Desempenho da Tecnologia LoRa em Foguetes**🚀

## 📡 **Sobre o Projeto**
Este repositório contém os códigos desenvolvidos como parte da minha Iniciação Científica, intitulada:

**"Aplicação e Desempenho da Tecnologia LoRa durante o vôo de foguetes com base na integridade da comunicação"**.

O objetivo principal deste trabalho é avaliar a eficiência da comunicação LoRa, focando na **perda de pacotes** e **taxa de transmissão**, tanto em experimentos **estáticos** (com ambos os rádios estacionários) quanto em experimentos **dinâmicos** (com um rádio em movimento, simulando o voo de um foguete).

## ⚡ **Objetivos do Projeto**
- 📶 **Analisar a taxa de transmissão e a perda de pacotes** em comunicação LoRa.
- 🔄 **Implementar e testar o Protocolo de Comunicação TCP** para LoRa P2P.
- 💾 **Armazenar os dados de comunicação na memória interna do ESP32**, minimizando o impacto no throughput.
- 🚀 **Aplicar os resultados para a telemetria de foguetes**, visando competições como a **Latin American Space Challenge (LASC)**.

## 🛠️ **Hardware Utilizado**
- **Microcontrolador:** ESP32
- **Módulos LoRa:** Ebyte E220-400T22D, E220-900T22D, E22-400T22D, E22-900T22D
- **Comunicação:** UART entre ESP32 e módulos LoRa

## 📚 **Bibliotecas Utilizadas**
Para a configuração e envio de dados pelos rádios LoRa, utilizamos as seguintes bibliotecas:
- [EByte LoRa E220 Series Library](https://github.com/xreef/EByte_LoRa_E220_Series_Library)
- [EByte LoRa E22 Series Library](https://github.com/xreef/EByte_LoRa_E22_Series_Library)
- [LoRa E32 Series Library](https://github.com/xreef/LoRa_E32_Series_Library)

## 📂 **Estrutura do Repositório**
📁 **/spiffs/littlefs/preferences_transmission** - Implementação base da comunicação LoRa para análise de desempenho para testar cada alternativa de salvamento (usando as bibliotecas: SPIFFS, LittleFS e Preferences).  
📁 **/TCP_like** - Tentativa de implementação do Protocolo TCP para comunicação LoRa.  
📁 **/save_alternatives** - Métodos de salvamento de dados na memória interna do ESP32.

## 🤝 **Colaboração**
Este projeto está sendo desenvolvido em colaboração com a **Equipe de Propulsão e Tecnologia Aeroespacial** da **Universidade Federal de Uberlândia (UFU)**. Nosso objetivo é entender melhor o comportamento da comunicação LoRa em foguetes para aplicações de **telemetria** e aprimorar a eficiência do envio de dados durante o voo.

## 📩 **Contato**
Caso tenha dúvidas ou sugestões, fique à vontade para abrir uma **issue** ou entrar em contato!


