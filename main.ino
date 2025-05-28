#if defined(ESP32)
  #include <WiFi.h>
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
#endif

#include "secrets.h"

// Pines del motor paso a paso
const int IN1 = 14;
const int IN2 = 27;
const int IN3 = 26;
const int IN4 = 25;

// LEDS (opcional)
const int LED = 12;
const int LED2 = 13;

const int pasosPorVuelta = 512; // Para 28BYJ-48
int paso[4][4] = {
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 1},
  {1, 0, 0, 1}
};

boolean sentido = true; // true = adelante, false = atrás

const uint32_t TiempoEsperaWifi = 5000;
unsigned long timeCurrent = 0;
unsigned long timePrevious = 0;
unsigned long timeCancellation = 3000;

WiFiServer server(80);

// HTML básico
const String page = R"====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Motor Paso a Paso</title>
</head>
<body>
<h2>Control del Motor Paso a Paso</h2>
)====";

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(LED, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED, LOW);
  digitalWrite(LED2, LOW);

  Serial.println("Conectando WiFi...");
  wifiMulti.addAP(ssid_1, password_1);
  WiFi.mode(WIFI_STA);

  while (wifiMulti.run(TiempoEsperaWifi) != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\n¡Conectado!");
  Serial.println("IP: " + WiFi.localIP().toString());

  server.begin();
}

void loop() {
  WiFiClient user = server.available();

  if (user) {
    Serial.println("Cliente conectado");
    timeCurrent = millis();
    timePrevious = timeCurrent;
    String lineCurrent = "";

    while (user.connected() && timeCurrent - timePrevious <= timeCancellation) {
      if (user.available()) {
        timeCurrent = millis();
        char letter = user.read();

        if (letter == '\n') {
          if (lineCurrent.length() == 0) {
            moverMotor(sentido);
            answerUser(user);
            break;
          } else {
            Serial.println(lineCurrent);
            checkMessage(lineCurrent);
            lineCurrent = "";
          }
        } else if (letter != '\r') {
          lineCurrent += letter;
        }
      }
    }

    user.flush();
    user.stop();
    Serial.println("Cliente desconectado");
  }
}

void checkMessage(String message) {
  if (message.indexOf("GET /adelante") >= 0) {
    sentido = true;
  } else if (message.indexOf("GET /atras") >= 0) {
    sentido = false;
  }
}

void moverMotor(bool direccion) {
  Serial.println(direccion ? "Moviendo adelante" : "Moviendo atrás");
  for (int vueltas = 0; vueltas < pasosPorVuelta; vueltas++) {
    for (int i = 0; i < 4; i++) {
      int pasoIndex = direccion ? i : 3 - i;
      digitalWrite(IN1, paso[pasoIndex][0]);
      digitalWrite(IN2, paso[pasoIndex][1]);
      digitalWrite(IN3, paso[pasoIndex][2]);
      digitalWrite(IN4, paso[pasoIndex][3]);
      delay(10);
    }
  }

  // Señal visual con LED
  digitalWrite(LED, HIGH);
  delay(300);
  digitalWrite(LED, LOW);
}

void answerUser(WiFiClient& user) {
  user.print("HTTP/1.1 200 OK\r\n");
  user.print("Content-Type: text/html\r\n");
  user.print("Connection: close\r\n\r\n");

  user.print(page);
  user.print("IP del Cliente: ");
  user.print(user.remoteIP());
  user.print("<br>Dirección del motor: ");
  user.print(sentido ? "Adelante" : "Atrás");
  user.print("<br><a href='/");
  user.print(sentido ? "atras" : "adelante");
  user.print("'>Cambiar dirección y mover</a><br>");
  user.print("</body></html>");
}