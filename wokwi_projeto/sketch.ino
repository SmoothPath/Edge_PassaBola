// Projeto: Munhequeira Monitora - Passa a Bola
// Dispositivo: ESP32
// Sensores: Potenciometro (Batimento Cardíaco), DHT22 (Temperatura Corporal)
// Atuador: Buzzer (Alerta)
// Funcionalidade: Monitora parâmetros fisiológicos e publica via MQTT para FIWARE
// Baseado no código original de: Fábio Henrique Cabrini
//Adaptado por: Equipe Smoothpath
/*Autores: Gabriel dos Santos Cardoso - 562103
//Geovana Maria da Silva Cardoso - 566254
//Gustavo Torres Caldeira - 561613
//Lucas Oliveira Santos - 563617
Mariana Silva do Egito Moreira - 562544
Turma: 1ESPF*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h> // Inclui a biblioteca para o sensor DHT

// ==================== CONFIGURAÇÕES EDITÁVEIS ====================
// Configurações de Rede
const char* default_SSID = "Wokwi-GUEST";    // Nome da rede Wi-Fi
const char* default_PASSWORD = "";            // Senha da rede Wi-Fi

// Configurações MQTT
const char* default_BROKER_MQTT = "20.220.27.106"; // IP do Broker MQTT
const int default_BROKER_PORT = 1883;              // Porta do Broker MQTT
const char* default_ID_MQTT = "fiware_munhequeira_001"; // ID MQTT

// Tópicos MQTT - Padrão FIWARE (IoT Agent JSON)
const char* default_TOPICO_SUBSCRIBE = "/TEF/munhequeira001/cmd"; // Tópico para receber comandos
const char* default_TOPICO_PUBLISH = "/TEF/munhequeira001/attrs"; // Tópico para publicar ALL attributes

// Definições dos pinos e sensores
const int default_PINO_BUZZER = 25;    // Pino do Buzzer (Alerta)
const int default_PINO_BATIMENTO = 34; // Pino do Potenciômetro (simula batimento)
const int default_PINO_DHT = 15;       // Pino do DHT22 (Temperatura)
#define DHTTYPE DHT22                  // Define o tipo de sensor DHT

// Intervalos de leitura e publicação
const int default_INTERVALO_LEITURA = 2000; // Lê sensores a cada 2 segundos

// ==================== DECLARAÇÃO DE VARIÁVEIS ====================
// Variáveis para configurações (permitem alteração em runtime se necessário)
char* SSID = const_cast<char*>(default_SSID);
char* PASSWORD = const_cast<char*>(default_PASSWORD);
char* BROKER_MQTT = const_cast<char*>(default_BROKER_MQTT);
int BROKER_PORT = default_BROKER_PORT;
char* TOPICO_SUBSCRIBE = const_cast<char*>(default_TOPICO_SUBSCRIBE);
char* TOPICO_PUBLISH = const_cast<char*>(default_TOPICO_PUBLISH);
char* ID_MQTT = const_cast<char*>(default_ID_MQTT);

int PINO_BUZZER = default_PINO_BUZZER;
int PINO_BATIMENTO = default_PINO_BATIMENTO;
int PINO_DHT = default_PINO_DHT;
int INTERVALO_LEITURA = default_INTERVALO_LEITURA;

// Instância dos objetos
WiFiClient espClient;
PubSubClient MQTT(espClient);
DHT dht(PINO_DHT, DHTTYPE); // Objeto DHT

// Variáveis de estado e sensores
int batimentoCardiaco = 0;
float temperaturaCorporal = 0.0;
float caloriasGastas = 0.0;
bool alertaAtivo = false;
unsigned long ultimaLeitura = 0;

// ==================== PROTÓTIPOS DE FUNÇÃO ====================
void initSerial();
void initWiFi();
void initMQTT();
void initSensores();
void reconectWiFi();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void VerificaConexoesWiFIEMQTT();
void reconnectMQTT();
void publicaDadosMQTT();
void verificaParametrosFisiologicos();
void ativarAlerta();
void desativarAlerta();

// ==================== SETUP ====================
void setup() {
  initSerial();
  initSensores();
  initWiFi();
  initMQTT();
  delay(5000); // Aguarda estabilização
  // Publica uma mensagem inicial de "on" (dispositivo ligado)
  MQTT.publish(TOPICO_PUBLISH, "s|on");
}

// ==================== LOOP PRINCIPAL ====================
void loop() {
  VerificaConexoesWiFIEMQTT(); // Mantém as conexões ativas

  // Lê e publica os dados no intervalo definido
  if (millis() - ultimaLeitura > INTERVALO_LEITURA) {
    lerDadosSensores();
    calcularCalorias(); // Calcula calorias com base nos dados
    verificaParametrosFisiologicos(); // Verifica se está tudo ok
    publicaDadosMQTT(); // Envia os dados para o Broker
    ultimaLeitura = millis();
  }

  MQTT.loop(); // Processa mensagens MQTT recebidas
}

// ==================== IMPLEMENTAÇÃO DAS FUNÇÕES ====================

void initSerial() {
  Serial.begin(115200);
}

void initWiFi() {
  delay(10);
  Serial.println("------ Conexao WI-FI ------");
  Serial.print("Conectando-se na rede: ");
  Serial.println(SSID);
  Serial.println("Aguarde");
  reconectWiFi();
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback); // Define a função de callback
}

void initSensores() {
  pinMode(PINO_BUZZER, OUTPUT);
  digitalWrite(PINO_BUZZER, LOW); // Garante que o buzzer comece desligado
  pinMode(PINO_BATIMENTO, INPUT); // Pino do potenciômetro é entrada analógica

  dht.begin(); // Inicializa o sensor DHT22
  Serial.println("Sensores inicializados.");

  // Sinalização de inicialização com o buzzer
  for (int i = 0; i < 3; i++) {
    digitalWrite(PINO_BUZZER, HIGH);
    delay(100);
    digitalWrite(PINO_BUZZER, LOW);
    delay(100);
  }
}

void reconectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Conectado com sucesso na rede ");
  Serial.print(SSID);
  Serial.println("IP obtido: ");
  Serial.println(WiFi.localIP());
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  // Converte o payload para uma string
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("- Mensagem recebida: ");
  Serial.println(msg);
  Serial.print("- Topico: ");
  Serial.println(topic);

  // Processa o comando recebido. Exemplo: "munhequeira001@alertaOff|"
  // Você pode definir comandos para silenciar o buzzer remotamente, etc.
  if (msg.indexOf("alertaOff") >= 0) {
    Serial.println("Comando recebido: Desativar Alerta");
    desativarAlerta(); // Função para desativar o buzzer
    // Pode publicar uma confirmação se necessário
    // MQTT.publish(TOPICO_PUBLISH, "alerta|off");
  }
  // Adicione outros comandos conforme a necessidade
  // else if (msg.indexOf("outroComando") >= 0) { ... }
}

void VerificaConexoesWiFIEMQTT() {
  if (!MQTT.connected())
    reconnectMQTT(); // Reconecta ao Broker, se necessário
  reconectWiFi();    // Reconecta ao WiFi, se necessário
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("* Tentando se conectar ao Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado com sucesso ao broker MQTT!");
      MQTT.subscribe(TOPICO_SUBSCRIBE); // Se inscreve no tópico de comandos
    } else {
      Serial.println("Falha ao reconectar no broker.");
      Serial.println("Haverá nova tentativa de conexão em 2s");
      delay(2000);
    }
  }
}

void lerDadosSensores() {
  // Lê o valor analógico do potenciômetro (0-4095) e converte para BPM (ex: 50-120)
  int valorAnalogico = analogRead(PINO_BATIMENTO);
  batimentoCardiaco = map(valorAnalogico, 0, 4095, 50, 120);

  // Lê a temperatura do DHT22
  float temp = dht.readTemperature(); // Lê a temperatura em Celsius
  if (!isnan(temp)) { // Verifica se a leitura é válida
    temperaturaCorporal = temp;
  } else {
    Serial.println("Falha ao ler o sensor DHT22!");
    // Mantém o último valor válido, ou define um valor de erro.
  }

  Serial.print("Batimento Cardiaco: ");
  Serial.print(batimentoCardiaco);
  Serial.print(" bpm - Temperatura: ");
  Serial.print(temperaturaCorporal);
  Serial.println(" °C");
}

void calcularCalorias() {
  // Constantes para cálculo baseado nos ranges dos sensores
  const float TAXA_BASAL_MINUTO = 0.02f; // kcal por minuto (base para repouso)
  const float FATOR_BATIMENTO = 0.02f;   // ajuste maior para tornar perceptível
  const float FATOR_TEMPERATURA = 0.1f;  // ajuste maior para tornar perceptível
  
  // Valores médios de referência
  const float BATIMENTO_MEDIO = 85.0f;
  const float TEMPERATURA_MEDIA = 36.5f;
  
  // Garante valores dentro de ranges plausíveis
  float batimentoAjustado = constrain(batimentoCardiaco, 50.0f, 120.0f);
  float temperaturaAjustada = constrain(temperaturaCorporal, 35.0f, 39.0f);
  
  // Cálculo dos ajustes
  float ajusteBatimento = (batimentoAjustado - BATIMENTO_MEDIO) * FATOR_BATIMENTO;
  float ajusteTemperatura = (temperaturaAjustada - TEMPERATURA_MEDIA) * FATOR_TEMPERATURA;
  
  // Taxa metabólica total
  float taxaMetabolica = TAXA_BASAL_MINUTO * (1.0f + ajusteBatimento + ajusteTemperatura);
  
  // Converte intervalo de leitura de ms para minutos
  float minutos = (INTERVALO_LEITURA / 1000.0f) / 60.0f;
  
  // Para testes: multiplicar por 60 para simular 1 minuto a cada leitura
  minutos *= 60;  // remove efeito de intervalo curto temporariamente
  
  // Incrementa calorias
  caloriasGastas += taxaMetabolica * minutos;
  caloriasGastas = max(caloriasGastas, 0.0f);
  
  Serial.print("Calorias gastas estimadas: ");
  Serial.print(caloriasGastas, 2);
  Serial.println(" kcal");
}

  
  // Debug opcional dos cálculos
  // Serial.print("Taxa metabólica: ");
  // Serial.print(taxaMetabolica, 4);
  // Serial.print(" kcal/min - Ajuste BPM: ");
  // Serial.print(ajusteBatimento, 4);
  // Serial.print(" - Ajuste Temp: ");
  // Serial.println(ajusteTemperatura, 4);

void verificaParametrosFisiologicos() {
  // Define faixas seguras (consulte um profissional de saúde para valores reais)
  const int BATIMENTO_MAXIMO = 100;
  const int BATIMENTO_MINIMO = 60;
  const float TEMPERATURA_MAXIMA = 38.0; // 38°C pode indicar febre/overheating

  bool batimentoAlerta = (batimentoCardiaco > BATIMENTO_MAXIMO) || (batimentoCardiaco < BATIMENTO_MINIMO);
  bool temperaturaAlerta = (temperaturaCorporal > TEMPERATURA_MAXIMA);

  if (batimentoAlerta || temperaturaAlerta) {
    Serial.println("ALERTA: Parâmetro fisiológico fora da faixa segura!");
    ativarAlerta();
    alertaAtivo = true;
  } else {
    if (alertaAtivo) {
      Serial.println("Parâmetros normalizados. Alerta desativado.");
      desativarAlerta();
      alertaAtivo = false;
    }
  }
}

void ativarAlerta() {
  tone(PINO_BUZZER, 1000, 500); // 1000 Hz (tom de alerta) por meio segundo
}

void desativarAlerta() {
  noTone(PINO_BUZZER);
}

void publicaDadosMQTT() {
  // Formata a mensagem no padrão esperado pelo IoT Agent JSON
  String mensagem = "b|" + String(batimentoCardiaco) +
                    "#t|" + String(temperaturaCorporal, 1) + // 1 casa decimal
                    "#c|" + String(caloriasGastas, 2); // 2 casas decimais

  // Adiciona o status de alerta à mensagem, se necessário
  if(alertaAtivo) { mensagem += "#a|on"; } else { mensagem += "#a|off"; }
                  

  // Publica a mensagem no tópico
  MQTT.publish(TOPICO_PUBLISH, mensagem.c_str());
  
   // Apresentação clara no Serial Terminal
  Serial.println("┌──────────────────────────────────────");
  Serial.println("│           DADOS MQTT ENVIADOS        ");
  Serial.println("├──────────────────────────────────────");
  Serial.printf ("│ BPM: %3d │  Temp: %4.1f°C\n", batimentoCardiaco, temperaturaCorporal);
  Serial.printf ("│ Calorias: %6.2f kcal\n", caloriasGastas);
  Serial.printf ("│ Mensagem: %s\n", mensagem.c_str());
  Serial.printf ("│  Tópico: %s\n", TOPICO_PUBLISH);
  Serial.println("└──────────────────────────────────────");
}
