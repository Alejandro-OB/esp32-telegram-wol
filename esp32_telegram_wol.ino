
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ESPping.h>
#include <WebServer.h>
#include <Update.h>
#include "secrets.h"

// Objetos globales
WiFiUDP udp;
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
WebServer otaServer(8266);

// Configuración de red
byte mac[6] = {0x34, 0x5A, 0x60, 0x4F, 0x9A, 0x02};
unsigned long lastCheck = 0, lastUpdateId = 0, ultimaActividad = 0, ultimaConexionWiFi = 0;
int reconexionesWiFi = 0, fallosWOL = 0;
time_t horaDeArranque;
#define TIEMPO_INACTIVIDAD 300000
#define MAX_TIEMPO_SIN_WIFI 60000
#define CHECK_INTERVAL 5000

// ============================ SETUP ============================
void setup() {
  Serial.begin(115200);
  lastUpdateId = 536953618;

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Error configurando IP estática");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setInsecure();

  Serial.println("Conectando WiFi...");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    ultimaConexionWiFi = millis();
    Serial.println("\nConectado!");
    bot.sendMessage(CHAT_ID_AUTORIZADO, "✅ ESP32 conectado a WiFi. IP: `" + WiFi.localIP().toString() + "`", "Markdown");
  }

  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    now = time(nullptr);
  }

  horaDeArranque = now;
  ultimaActividad = millis();

  Serial.print("Hora de arranque (timestamp): "); Serial.println(horaDeArranque);
  bot.sendMessage(CHAT_ID_AUTORIZADO, "♻️ ESP32 ha sido reiniciado correctamente", "Markdown");
  iniciarServidorOTA();
}

// ============================ LOOP ============================
void loop() {
  otaServer.handleClient();

  if (millis() - lastCheck > CHECK_INTERVAL) {
    manejarTelegram();
    lastCheck = millis();
  }

  if (WiFi.status() != WL_CONNECTED) {
    reconectarWiFi();
    if (millis() - ultimaConexionWiFi > MAX_TIEMPO_SIN_WIFI) ESP.restart();
    delay(5000);
  } else {
    ultimaConexionWiFi = millis();
  }
}

// ============================ FUNCIONES PRINCIPALES ============================
void manejarTelegram() {
  Serial.println("Obteniendo actualizaciones de Telegram...");
  int numNewMessages = bot.getUpdates(lastUpdateId + 1);
  Serial.print("Nuevos mensajes: "); Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    unsigned long msg_date = bot.messages[i].date.toInt();

    if (text.length() > 300) {
      bot.sendMessage(chat_id, "⚠️ El mensaje es demasiado largo y ha sido ignorado.", "Markdown");
      continue;
    }

    if (msg_date < horaDeArranque || chat_id != CHAT_ID_AUTORIZADO) continue;
    ultimaActividad = millis();

    if (text == "/start" || text == "/ayuda") mostrarAyuda(chat_id);
    else if (text == "/encender_pc") encenderPC(chat_id);
    else if (text == "/apagar_pc") apagarPC(chat_id);
    else if (text == "/reiniciar_pc") reiniciarPC(chat_id);
    else if (text == "/verificar_pc") verificarPC(chat_id);
    else if (text == "/estado_esp32") mostrarEstadoESP32(chat_id);
    else if (text == "/info_pc") obtenerInfoPC(chat_id);
    else if (text == "/logs_pc") obtenerLogs(chat_id);
    else if (text == "/reiniciar_esp32") reiniciarESP32(chat_id);
    else if (text == "/actualizar_ota") mostrarOTA(chat_id);
    else bot.sendMessage(chat_id, "❓ Comando no reconocido", "Markdown");
  }

  if (numNewMessages > 0) {
    lastUpdateId = bot.messages[numNewMessages - 1].update_id;
  }
}

void reconectarWiFi() {
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    reconexionesWiFi++;
    bot.sendMessage(CHAT_ID_AUTORIZADO, "🔄 Reconexión WiFi exitosa", "Markdown");
  }
}

// ============================ FUNCIONES DE CONTROL ============================
bool enviarPaqueteWOL() {
  byte wolPacket[102];
  memset(wolPacket, 0xFF, 6);
  for (int i = 1; i <= 16; i++) memcpy(&wolPacket[i * 6], mac, 6);
  udp.beginPacket(broadcastIP, 9);
  udp.write(wolPacket, sizeof(wolPacket));
  return udp.endPacket() == 1;
}

bool estaPCEncendido() {
  return Ping.ping(ipPC);
}

bool verificarPCEncendido(int intentos, int intervaloMs) {
  for (int i = 0; i < intentos; i++) {
    if (estaPCEncendido()) return true;
    delay(intervaloMs);
  }
  return false;
}

// ============================ FUNCIONES DE COMANDOS ============================

void iniciarServidorOTA() {
  otaServer.on("/", HTTP_GET, []() {
    otaServer.send(200, "text/html", R"rawliteral(
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <input type='file' name='update'>
        <input type='submit' value='Actualizar'>
      </form>
    )rawliteral");
  });

  otaServer.on("/update", HTTP_POST, []() {
    otaServer.sendHeader("Connection", "close");
    otaServer.send(200, "text/plain", (Update.hasError()) ? "Fallo en actualización" : "Actualización correcta. Reiniciando...");
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = otaServer.upload();
    if (upload.status == UPLOAD_FILE_START && !Update.begin()) Update.printError(Serial);
    if (upload.status == UPLOAD_FILE_WRITE && Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    if (upload.status == UPLOAD_FILE_END && !Update.end(true)) Update.printError(Serial);
  });

  otaServer.begin();
}

void mostrarOTA(String chat_id) {
  String url = "http://" + WiFi.localIP().toString() + ":8266";
  bot.sendMessage(chat_id, "🔄 *Actualización OTA disponible:*[Haz clic aquí para actualizar](" + url + ")", "Markdown");
}

void apagarPC(String chat_id) {
  WiFiClient clientHTTP;
  if (clientHTTP.connect(ipPC, PUERTO_FLASK)) {
    clientHTTP.print("GET /apagar?token=" + tokenPC + " HTTP/1.1\r\nHost: " + ipPC.toString() + "\r\nConnection: close\r\n\r\n");
    bot.sendMessage(chat_id, "🛑 Comando de apagado enviado al PC.", "Markdown");
    delay(5000); 
    if (!estaPCEncendido()) {
      bot.sendMessage(chat_id, "🔌 Confirmado: El PC está apagado.", "Markdown");
    } else {
      bot.sendMessage(chat_id, "⚠️ El PC aún responde al ping. No se apagó.", "Markdown");
    }
  } else {
    bot.sendMessage(chat_id, "❌ No se pudo contactar con el PC.", "Markdown");

  }
}

void reiniciarPC(String chat_id) {
  WiFiClient clientHTTP;
  if (clientHTTP.connect(ipPC, PUERTO_FLASK)) {
    clientHTTP.print("GET /reiniciar?token=" + tokenPC + " HTTP/1.1\r\nHost: " + ipPC.toString() + "\r\nConnection: close\r\n\r\n");
    bot.sendMessage(chat_id, "♻️ Comando de reinicio enviado al PC.", "Markdown");
    bot.sendMessage(chat_id, "♻️ Comando enviado. Verificando reinicio en curso...", "Markdown");
    delay(10000);
    if (verificarPCEncendido(10, 3000)) {
      bot.sendMessage(chat_id, "✅ El PC volvió a encender tras el reinicio.", "Markdown");
    } else {
      bot.sendMessage(chat_id, "⚠️ El PC no respondió después del reinicio.", "Markdown");
    }
  } else {
    bot.sendMessage(chat_id, "❌ No se pudo contactar con el PC.", "Markdown");
  }
}


void obtenerInfoPC(String chat_id) {
  WiFiClient clientHTTP;
  if (clientHTTP.connect(ipPC, PUERTO_FLASK)) {
    clientHTTP.print("GET /info HTTP/1.1\r\nHost: " + ipPC.toString() + "\r\nConnection: close\r\n\r\n");

    int start = response.indexOf('{'), end = response.lastIndexOf('}');
    if (start != -1 && end != -1) {
      String jsonBody = response.substring(start, end + 1);
      Serial.println("JSON recibido:");
      Serial.println(jsonBody);

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, jsonBody);

      if (!error) {

        String msg = "*🧠 Info del PC:*\n\n";
        msg += "*Hostname:* `" + String((const char*)doc["hostname"]) + "`\n";
        msg += "*Sistema:* `" + String((const char*)doc["sistema"]) + "`\n";
        msg += "*Versión:* `" + String((const char*)doc["version"]) + "`\n";
        msg += "*Uptime:* `" + String((const char*)doc["uptime"]) + "`\n";
        msg += "*CPU:* `" + String(doc["cpu_uso_percent"].as<float>(), 1) + "%`\n";
        msg += "*RAM:* `" + String(doc["ram_uso_percent"].as<float>(), 1) + "%`\n";

        if (!doc["temperatura_cpu"].isNull()) {
          msg += "*Temp CPU:* `" + String(doc["temperatura_cpu"].as<float>(), 1) + "°C`\n";
        } else {
          msg += "*Temp CPU:* `No disponible ❌`\n";
        }
        Serial.println("Mensaje enviado a Telegram:");
        Serial.println(msg);

        bot.sendMessage(chat_id, msg, "Markdown");
      } else {
        bot.sendMessage(chat_id, "❌ Error al analizar la respuesta del servidor.", "");
      }
    } else {
      bot.sendMessage(chat_id, "❌ Respuesta del servidor no válida (sin JSON)", "");
    }
  } else {
    bot.sendMessage(chat_id, "❌ No se pudo conectar con el servidor Flask", "");
  }
}

void obtenerLogs(String chat_id) {
  WiFiClient clientHTTP;
  if (clientHTTP.connect(ipPC, PUERTO_FLASK)) {
    clientHTTP.print("GET /logs HTTP/1.1\r\nHost: " + ipPC.toString() + "\r\nConnection: close\r\n\r\n");

    String response = "";
    while (clientHTTP.connected() || clientHTTP.available()) {
      if (clientHTTP.available()) {
        response += clientHTTP.readStringUntil('\n');
      }
    }

    int bodyIndex = response.indexOf("\r\n\r\n");
    String body = (bodyIndex != -1) ? response.substring(bodyIndex + 4) : response;

    bot.sendMessage(chat_id, "*📜 Últimos eventos del servidor:*\n\n```\n" + body + "\n```", "Markdown");
  } else {
    bot.sendMessage(chat_id, "❌ No se pudo conectar al servidor Flask.", "Markdown");
  }
}

void mostrarEstadoESP32(String chat_id) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char bufferHora[30];
  strftime(bufferHora, sizeof(bufferHora), "%d/%m/%Y %H:%M:%S", &timeinfo);

  unsigned long uptime = millis() / 1000;
  String mensaje = "*📋 Estado del ESP32:*\n\n";
  mensaje += "*IP:* `" + WiFi.localIP().toString() + "`\n";
  mensaje += "*WiFi:* `" + WiFi.SSID() + "`\n";
  mensaje += "*Señal:* `" + String(WiFi.RSSI()) + " dBm`\n";
  mensaje += "*Reconexiones:* `" + String(reconexionesWiFi) + "`\n";
  mensaje += "*Fallos WOL:* `" + String(fallosWOL) + "`\n";
  mensaje += "*Última sincronización:* `" + String(bufferHora) + "`\n";
  mensaje += "*Estado del PC:* ";
  mensaje += (estaPCEncendido() ? "✅ Encendido" : "❌ Apagado");

  bot.sendMessage(chat_id, mensaje, "Markdown");
}

void mostrarAyuda(String chat_id) {
  String mensaje = "*Comandos disponibles:*\n\n";
  mensaje += "\\/encender\\_pc \\- _Enciende el PC_\n";
  mensaje += "\\/verificar\\_pc \\- _Verifica si el PC está encendido_\n";
  mensaje += "\\/apagar\\_pc \\- _Apaga el PC_\n";
  mensaje += "\\/reiniciar\\_pc \\- _Reinicia el PC_\n";
  mensaje += "\\/estado\\_esp32 \\- _Muestra estado del ESP32_\n";
  mensaje += "\\/info\\_pc \\- _Muestra información del PC_\n";
  mensaje += "\\/logs\\_pc \\- _Muestra los logs del PC_\n";
  mensaje += "\\/reiniciar\\_esp32 \\- _Reinicia el ESP32_\n";
  mensaje += "\\/actualizar\\_ota \\- _Abre la interfaz para actualizar el ESP32_\n";
  mensaje += "\\/ayuda \\- _Muestra este mensaje_\n";

  bot.sendMessage(chat_id, mensaje, "MarkdownV2");
}

void reiniciarESP32(String chat_id) {
  bot.sendMessage(chat_id, "♻️ Reiniciando ESP32...", "Markdown");
  delay(1000);
  ESP.restart();
}

void verificarPC(String chat_id) {
  bool encendido = estaPCEncendido();
  bot.sendMessage(chat_id, encendido ? "✅ El PC está encendido." : "❌ El PC está apagado.", "Markdown");
}

void encenderPC(String chat_id) {
  if (estaPCEncendido()) {
    bot.sendMessage(chat_id, "✅ El PC ya está encendido", "Markdown");
  } else if (enviarPaqueteWOL()) {
    bot.sendMessage(chat_id, "⚡ Enviando paquete WOL...", "Markdown");
    if (verificarPCEncendido(10, 3000))
      bot.sendMessage(chat_id, "✅ PC encendido correctamente", "Markdown");
    else
      bot.sendMessage(chat_id, "⚠️ El PC no respondió al WOL", "Markdown");
  } else {
    bot.sendMessage(chat_id, "❌ Error al enviar WOL", "Markdown");
  }
}