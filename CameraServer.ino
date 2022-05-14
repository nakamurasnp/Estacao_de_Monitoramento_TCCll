#include "esp_camera.h"
#include "soc/soc.h"              // Disable brownour problems
#include "soc/rtc_cntl_reg.h"    // Disable brownour problems
#include "driver/rtc_io.h"      //Biblioteca ESP32 usada para sono profundo e pinos de ativação RTC
#include <WiFi.h>              //Comunicação com o Wifi
#include <WiFiClient.h>       //Cria um cliente que pode se conectar a um endereço IP e porta da Internet especificados
#include "ESP32_FTPClient.h" // Biblioteca que permite fazer upload de imagens par um site
#include <NTPClient.h>      //Biblioteca para solicitar data e hora
#include <WiFiUdp.h>       //Cria um objeto da classe UDP
#include "time.h"         // biblioteca para manipulacao de  datas e horarios
#include <ESPAsyncWebServer.h> // Biblioteca para construir servidor Web
#include <esp32-hal-timer.h>   // Biblioteca para tipos de Timers (temporizador)
#include "DHTesp.h"            // Biblioteca sensor de temperatura e humidade
DHTesp dht;                    // Declaração do metodo de forma global

char* ftp_server = "XXX.XXX.XXX.XXX"; // Conexao com o servidor FTP
char* ftp_user = "XXXXXXXXXXXX";      // Usuario do servidor FTP
char* ftp_pass = "XXXXXXXXXXXX";     // Senha do servidor FTP
char* ftp_path = "/XXXXXX/";          // Pasta destino do aquivo (foto)
const char* WIFI_SSID = "XXXXXXXXXX"; // Usuario e senha do Wifi para conectar a rede
const char* WIFI_PASS = "XXXXXXXXXX";
const char* PARAM_MESSAGE = "message";  // html

unsigned long millisPause = 0; // Inicia variáveis de tempo tirar foto *teste

WiFiUDP ntpUDP;   // Por padrão, 'pool.ntp.org' é usado com intervalo de atualização de 60 segundos e sem deslocamento
NTPClient timeClient(ntpUDP, "pool.ntp.org", (-3600*4), 60000); // Pega o horario do pool UTC-04: 00 para o MT
AsyncWebServer server(80); // html Cria o objeto AsyncWebServer na porta 80
ESP32_FTPClient ftp (ftp_server,ftp_user,ftp_pass, 5000, 2); //passa um tempo limite de FTP e modo de depuração nos últimos 2 argumentos
hw_timer_t *timer = NULL; //faz o controle do temporizador (interrupção por tempo)  * re-start

// Definição de pin para CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

camera_config_t config;

// Função para ler temperatura
String readDHTTemperature() {
  float t = dht.getTemperature();   // Faz a leitura da temperatura e armazena em um float
  float bt;                         // Cria um float para armazenar o backup
  if (isnan(t)) {                    // condição que retorna erro e dois traços (-) caso o sensor não consiga obter as leituras
    Serial.println("Falha ao ler o sensor DHT! A ultima temperatura registrada foi:");
    return String(bt);               // Retorna o backup caso nao consiga ler                    
  }
  else {                             // Caso contrario retorna a temperatura
    Serial.print("Temperatura: ");   
    bt = t;                          // Armazena a ultima temperatura registrada
    Serial.println(t);               // Imprime o valor da temperatura no serial
    return String(t);                // Converte o valor de float para String e o retorna
  } 
}
// Função para ler a Humidade
String readDHTHumidity() {
  float h = dht.getHumidity();       // Faz a leitura da humidade e armazena em um float
  float bh;                          // Cria um float para armazenar o backup
  if (isnan(h)) {                    // condição que retorna erro e dois traços (-) caso o sensor não consiga obter as leituras
    Serial.println("Falha ao ler o sensor DHT!");
    return String(bh);
  }
  else {
    Serial.print("Humidade: ");
    bh = h;                          // Armazena a ultima humidade registrada
    Serial.println(h);               // Imprime o valor da humidade no serial
    return String(h);                // Converte o valor de float para String e o retorna
  }  
}
// Função para ler o nivel de sinal do Wifi
String readSignal() {  
  float rssi = WiFi.RSSI();         // Faz a leitura do nivel de sinal e armazena em um float
  if (isnan(rssi)) {                // condição que retorna erro e dois traços (-) caso o sensor não consiga obter as leituras
    Serial.println("Failed to read from rssi!");
    return "--";
  }
  else {
    //Serial.println(rssi);         // Imprime o valor do sinal no serial
    return String(rssi);            // Converte o valor de float para String e o retorna
  }
}
// Construção da Pagina Web
const char index_html[] PROGMEM = R"rawliteral(
 <!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">  
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous"> 
  <style>
    html {                 // CSS para estilizar a pagina
     font-family: Arial;   // fonte ariel
     display: inline-block; 
     margin: 0px auto;     // Bloco sem margem
     text-align: center;   // alinhado ao centro
    }
    h1 { font-size: 3.0rem; }  // Defini o tamanho das fontes
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{                // Rotulos para leituras estilizados
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <img src="http://portal.unemat.br/imagens/logo-unemat-maldonado.png">
  <h1><i class="fas fa-info-circle" style="color:#05149e;"></i> ESP32-CAM Monitoramento</h1>
  <p>
    <i class="fas fa-thermometer-half" style="color:#33eb0e;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#1ee4d3;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <i class="fas fa-signal" style="color:#e5f50b;"></i> 
    <span class="dht-labels">Signal Strength</span>
    <span id="Signal">%SIGNAL%</span>
    <span class="units">dBm</span>    
  </p>
  <br />
  <p>
    <h2><i class="fas fa-camera" style="color:#f7081c;"></i> Monitoramento via Web</h2> 
  </p>
  <p>
    <i class="fab fa-internet-explorer" style="color:#1506e9;"></i> 
    <span class="labels">Host Address: </span>
    <a href="https://198.136.59.206:2083/cpsess9304810291/frontend/paper_lantern/filemanager/index.html?dirselect=homedir&dir=/esp32" 
    target="_blank">Clique aqui!</a>       
  </p>
  <p>
    <i class="fas fa-user-tie" style="color:#08f508;"></i> 
    <span class="labels">User: </span>
    <span class="labels">nakamura</span>          
  </p>
  <p>
    <i class="fas fa-key" style="color:#f5087f;"></i> 
    <span class="labels">Password: </span>
    <span class="labels">OM99XAxMlvuS</span>           
  </p>    
</body>
<script>
setInterval(function ( ) {                                                    // Função que é executada a cada 10 segundos para obter a leitura de temperatura mais recente           
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {             
    if (this.readyState == 4 && this.status == 200) {                         // Responsável por atualizar a temperatura de forma assíncrona
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);                                    // Atualiza o elemento HTML cujo id é temperature
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {                                                    // Função que é executada a cada 10 segundos para obter a leitura da humidade mais recente
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);                                       // Atualiza o elemento HTML cujo id é humidity
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {                                                    // Função que é executada a cada 10 segundos para obter a leitura do nivel de sinal mais recente
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {                         // Responsável por atualizar a temperatura de forma assíncrona
      document.getElementById("Signal").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/Signal", true);                                         // Atualiza o elemento HTML cujo id é Signal
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";

// Essa função ira substituir os espaços reservados pelos valores reais
String processor(const String& var){
  if(var == "TEMPERATURE"){         // Retorna a temperatura chamando a função ja criada readDHTTemperature()
    return readDHTTemperature();
  }
  else if(var == "HUMIDITY"){       // Retorna a humidade chamando a função ja criada readDHTHumidity()
    return readDHTHumidity();
  }
  else if(var == "SIGNAL"){         // Retorna o nivel de sinal chamando a função ja criadareadSignal()
    return readSignal();
  }
  return String();
}
// Esta função manipula a rota não encontrada, retornando ao clientes a resposta HTTP 404
void notFound(AsyncWebServerRequest *request) {  // html
   request->send(404, "text/plain", "Not found");
} 

//função que o temporizador irá chamar, para reiniciar o ESP32 * re-start
void IRAM_ATTR interruptReboot(){         
  ets_printf("(watchdog) reiniciar\n"); //imprime no log * re-start
  ESP.restart();                        // reinicializar * re-start
}

// Inicializa a rede de WiFi
void setupWiFi(){        
  WiFi.mode(WIFI_STA);                      // web server - Chamado de station mode, é o modo de operação para que o dispositivo funcione como um cliente wireless. 
  WiFi.begin(WIFI_SSID, WIFI_PASS);         // Conecta com o Wifi  
  Serial.println("Conectando ao Wifi..."); 
  int counter =0;                           // Declara o contador = 0.
  while (WiFi.status() != WL_CONNECTED && counter < 10) {   //Caso nao esteja conectado ao host, ira tentar se conectar ate 10 tentativas.
      delay(1000);                         //Espera ate que a conexao seja feita.
      Serial.println("Conectando ao Wifi.."); 
      counter++;                           // Incrementa em 1 o contador.
  }
  if (WiFi.status() != WL_CONNECTED){      // Caso nao consiga se conectar no Wifi o sistema re-inicializa.
       ESP.restart();                      // Função que chama a re-inicialização
  }
  Serial.println("Endereco de IP : ");      
  Serial.println(WiFi.localIP());          // Informa o IP que foi pego
}

// Inicialização da camera
void initCamera() { 
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

  // Configuração da Qualidade e tamanho da imagem
  if(psramFound()){  
    config.frame_size = FRAMESIZE_UXGA;//FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }  

  // Inicializa a camera
  esp_err_t err = esp_camera_init(&config);  
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }  
}

// Tira foto e envia 
void takePhoto() {  
  camera_fb_t * fb = NULL;      // ponteiro FB
  fb = esp_camera_fb_get();     // é usado para obter uma única imagem e armazena em um buffer (fb)
  if(!fb) {
    Serial.println("A captura da câmera falhou");
    return;
  }
  ftp.OpenConnection(); //Para abrir uma conexao na nuvem acima de 15 min    
  ftp.ChangeWorkDir(ftp_path); // Muda o diretório para o diretório de trabalho "ftp_pat"
  ftp.InitFile("Type I"); //  Informa ao servidor ftp que ira ser enviado o arquivo tipo 1

  String nomeArquivo = timeClient.getFullFormattedTimeForFile()+"T"+readDHTTemperature()+"H"+readDHTHumidity()+".jpg"; // Nessa linha pega a data e hora e adiciona o jpg para o nome do arquivo
  Serial.println("Enviando "+nomeArquivo);
  int str_len = nomeArquivo.length() + 1;                                //  Comprimento (com um caractere extra para o terminador nulo (adionar)
 
  char char_array[str_len];                      // Prepara a matriz de caracteres (o buffer)
  nomeArquivo.toCharArray(char_array, str_len); // converte o arquivo de string para uma matriz
  
  ftp.NewFile(char_array);              //Converte em uma matriz de char
  ftp.WriteData( fb->buf, fb->len );   // Escreve o buffer
  ftp.CloseFile();                    // fecha o arquivo  
  esp_camera_fb_return(fb);          //  deve ser usado para liberar a memória alocada por fb.   
}

// Refente a comunicação HTML da porta 80
void serverSetup(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){              //A função processor irá substituir todos os marcadores com os valores corretos
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){   // usado o metodo c_str ()para enviar como caractere
    request->send_P(200, "text/plain", readDHTTemperature().c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){      // usado o metodo c_str ()para enviar como caractere
    request->send_P(200, "text/plain", readDHTHumidity().c_str());
  }); 
  server.on("/Signal", HTTP_GET, [](AsyncWebServerRequest *request){        // usado o metodo c_str ()para enviar como caractere
    request->send_P(200, "text/plain", readSignal().c_str());
  });
   server.onNotFound(notFound); // Informa a acao a ser executada (notFound)  quando e feita uma tentativa de acessar um recurto inválido
   server.begin();              // Inicia o serviço
}

// Setup Principal
void setup() {           
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //desabilitar detector de queda de energia
 
  Serial.begin(115200);  //Habilita a comunicaçao serial para a string recebida ser lida no Serial monitor.
  setupWiFi();           // inicializar o WiFi
  initCamera();          // inicializa a camera
  timeClient.begin();
  timeClient.update();
  ftp.OpenConnection();  // inicia a conexao FTP com servidor
  serverSetup();         // inicializa o servidor porta 80
  dht.setup(14, DHTesp :: DHT22); // Inicia conexao dht
  Serial.println(timeClient.getFullFormattedTimeForFile());  // visualiza o horario atual

 // Codigo referente ao Watchdog
  timer = timerBegin(0, 80, true); //timerID 0, div 80 (clock do esp), contador progressivo * re-start
  timerAttachInterrupt(timer, &interruptReboot, true); //instancia de timer, função callback, interrupção de borda * re-start
  timerAlarmWrite(timer, 3900000000, true); // instancia de timer, tempo (us),3900000000 us = 65 minutos , repetição * re-start
  timerAlarmEnable(timer); //habilita a interrupção * re-start
  
}

// Executa varias vezes uma função
void loop() {
  timerWrite(timer, 0); //Zera o temporizador (alimenta o watchdog) * re-start
  long tme = millis();  //Tempo inicial do loop    * re-start  
  
  timeClient.update();  //Esta função pega o tempo atualizado do NTP na forma de string e o armazena na variável formattedTime
  takePhoto();          // tirar foto  
  millisPause = millis();
  while(millis() < (millisPause + 3600000)) {  //Esperar por 1 hora  e faz nada. 
    
  }
  tme = millis() - tme; //Calcula o tempo (atual - inicial) * re-start
}
