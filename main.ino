#if defined(ESP32)
  #include <WiFi.h>
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
#endif

#include "secrets.h"  // Define ssid_1 y password_1

// Pines del motor paso a paso
const int IN1 = 14;
const int IN2 = 27;
const int IN3 = 26;
const int IN4 = 25;

// LEDs (opcional)
const int LED = 12;
const int LED2 = 13;

const int pasosPorVuelta = 512; // Para 28BYJ-48

int paso[4][4] = {
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 1},
  {1, 0, 0, 1}
};

boolean sentido = true;               // Última dirección ejecutada
volatile bool mover = false;          // Solicitud pendiente
bool sentidoPendiente = true;         // Dirección a ejecutar

const uint32_t TiempoEsperaWifi = 5000;
unsigned long timeCurrent = 0;
unsigned long timePrevious = 0;
unsigned long timeCancellation = 3000;

WiFiServer server(80);

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

  // Ejecuta movimiento del motor si hay orden pendiente
  if (mover) {
    moverMotor(sentidoPendiente);
    sentido = sentidoPendiente;
    mover = false;
  }
}

void checkMessage(String message) {
  if (message.indexOf("GET /adelante") >= 0) {
    sentidoPendiente = true;
    mover = true;
  } else if (message.indexOf("GET /atras") >= 0) {
    sentidoPendiente = false;
    mover = true;
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
  user.print("<br>Última dirección ejecutada: ");
  user.print(sentido ? "Adelante" : "Atrás");

  user.print(R"rawliteral(
    <br><br>
    <button onclick="moverAdelante(event)">Mover Adelante</button>
    <button onclick="moverAtras(event)">Mover Atrás</button>

    <script>
      function moverAdelante(e) {
        e.preventDefault();
        fetch('/adelante', { method: 'GET' })
          .then(() => console.log('Adelante solicitado'));
      }

      function moverAtras(e) {
        e.preventDefault();
        fetch('/atras', { method: 'GET' })
          .then(() => console.log('Atrás solicitado'));
      }
    </script>
  )rawliteral");

  user.print("</body></html>");
}