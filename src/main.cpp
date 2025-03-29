#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Metro.h>

const char* ssid = "alien";
const char* password = "5an32sjr";

WebServer server(80);
WebSocketsServer webSocket(81);

const int inputPins[] = {13, 14, 15, 16, 17, 18, 19, 21, 22};
const int numPins = sizeof(inputPins) / sizeof(inputPins[0]);
int pinStates[numPins];

void handleRoot();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void sendPinStates();
String getPinStatesJson();

Metro pinMetro = Metro(10);

// Cola para almacenar los últimos 1000 estados de los pines
const int maxDataPoints = 1000;
int pinData[maxDataPoints];
int dataIndex = 0;

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < numPins; i++) {
    pinMode(inputPins[i], INPUT_PULLUP);
    pinStates[i] = digitalRead(inputPins[i]);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");
}

void loop() {
  server.handleClient();
  webSocket.loop();

  if (pinMetro.check() == 1) {
    sendPinStates();
  }
}

void handleRoot() {

    String htmlContent = R"(
        <!DOCTYPE html>
        <html lang="es">
        <head>
          <meta charset="UTF-8">
          <meta name="viewport" content="width=device-width, initial-scale=1">
          <title>ESP32 Monitor - Señales FPGA</title>
          <link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
          <style>
            /* Estilo Dark Hacker */
            body {
              font-family: 'Share Tech Mono', monospace;
              background: radial-gradient(ellipse at center, #1b2735 0%, #090a0f 100%);
              color: #f0f0f0;
              margin: 0;
              padding: 20px;
              display: flex;
              flex-direction: column;
              align-items: center;
            }
            header {
              text-align: center;
              margin-bottom: 30px;
            }
            h1 {
              font-size: 2.5em;
              color: #00ff99;
              text-shadow: 0 0 10px #00ff99;
              margin: 0;
            }
            /* Indicadores LED */
            #ledContainer {
              display: flex;
              justify-content: center;
              gap: 20px;
              margin: 20px 0;
            }
            .led {
              width: 25px;
              height: 25px;
              border-radius: 50%;
              background: #444;
              box-shadow: 0 0 10px rgba(0, 255, 153, 0.7);
              transition: background-color 0.3s ease, box-shadow 0.3s ease;
            }
            /* Bloque de conversión de los 8 primeros bits */
            #byteDisplay {
              background: rgba(0, 0, 0, 0.5);
              backdrop-filter: blur(8px);
              padding: 15px 20px;
              border-radius: 10px;
              box-shadow: 0 4px 10px rgba(0,0,0,0.7);
              margin-bottom: 30px;
              text-align: center;
              width: 90%;
              max-width: 600px;
              font-size: 1.1em;
              color: #00ff99;
            }
            /* Controles */
            #controls {
              background: rgba(0, 0, 0, 0.5);
              backdrop-filter: blur(8px);
              padding: 20px;
              border-radius: 10px;
              box-shadow: 0 4px 10px rgba(0,0,0,0.7);
              text-align: center;
              margin-bottom: 30px;
              width: 90%;
              max-width: 600px;
            }
            .slider-container {
              margin: 15px 0;
              display: flex;
              justify-content: center;
              align-items: center;
              gap: 10px;
            }
            input[type=range] {
              -webkit-appearance: none;
              width: 200px;
              background: transparent;
            }
            input[type=range]:focus {
              outline: none;
            }
            input[type=range]::-webkit-slider-runnable-track {
              width: 100%;
              height: 8px;
              cursor: pointer;
              background: #333;
              border-radius: 5px;
            }
            input[type=range]::-webkit-slider-thumb {
              border: 2px solid #00ff99;
              height: 20px;
              width: 20px;
              border-radius: 50%;
              background: #000;
              cursor: pointer;
              -webkit-appearance: none;
              margin-top: -6px;
              box-shadow: 0 0 5px #00ff99;
            }
            /* Botones de control de la gráfica */
            #graphControls {
              margin: 15px 0;
              text-align: center;
            }
            #graphControls button {
              background: #000;
              border: 2px solid #00ff99;
              color: #00ff99;
              padding: 10px 15px;
              margin: 5px;
              border-radius: 5px;
              cursor: pointer;
              font-family: 'Share Tech Mono', monospace;
            }
            #graphControls button:hover {
              background: #00ff99;
              color: #000;
            }
            /* Canvas con borde neon */
            canvas {
              border: 2px solid #00ff99;
              border-radius: 10px;
              background-color: #000;
              display: block;
              margin: 0 auto;
              box-shadow: 0 0 20px #00ff99;
            }
          </style>
        </head>
        <body>
          <header>
            <h1>ESP32 Monitor - Señales FPGA</h1>
          </header>
          <div id="ledContainer">
            <!-- Indicadores LED para cada uno de los 9 pines -->
            <div class="led" id="led0"></div>
            <div class="led" id="led1"></div>
            <div class="led" id="led2"></div>
            <div class="led" id="led3"></div>
            <div class="led" id="led4"></div>
            <div class="led" id="led5"></div>
            <div class="led" id="led6"></div>
            <div class="led" id="led7"></div>
            <div class="led" id="led8"></div>
          </div>
          <!-- Bloque de visualización de conversión de datos de los 8 primeros bits -->
          <div id="byteDisplay">
            <p>
              <strong>Byte:</strong>
              <span id="byteBinary"></span> | 
              <span id="byteDecimal"></span> | 
              <span id="byteHex"></span> | 
              <span id="byteASCII"></span>
            </p>
          </div>
          <div id="controls">
            <div class="slider-container">
              <label for="zoomSlider">Zoom:</label>
              <input type="range" id="zoomSlider" min="0.5" max="5" step="0.1" value="1">
            </div>
            <div class="slider-container">
              <label for="scrollSlider">Desplazamiento:</label>
              <input type="range" id="scrollSlider" min="0" max="0" step="1" value="0">
            </div>
            <div id="graphControls">
              <button id="btnPlay">Play</button>
              <button id="btnPause">Pause</button>
              <button id="btnStop">Stop</button>
              <button id="btnRestart">Reiniciar</button>
              <button id="btnExport">Export JSON</button>
            </div>
          </div>
          <canvas id="myCanvas" width="1000" height="300"></canvas>
          <script>
            var canvas = document.getElementById("myCanvas");
            var ctx = canvas.getContext("2d");
            
            // Variables de control de la adquisición y buffer interno
            var isCollecting = false; // Inicia en false hasta que se presione Play
            var isPaused = false;
            var dataBuffer = []; // Buffer interno para almacenar todos los datos
            var socket = new WebSocket("ws://" + window.location.hostname + ":81/");
            
            // Configuración inicial
            var numPins = 9;
            var historyLength = 1000;
            var histories = [];
            for (var i = 0; i < numPins; i++) {
              histories[i] = [];
            }
            var rowHeight = canvas.height / numPins;
            function getY(pinIndex, state) {
              return state == 1 ? rowHeight * (pinIndex + 0.3) : rowHeight * (pinIndex + 0.7);
            }
            
            var zoomFactor = 1;
            var scrollOffset = 0;
            var zoomSlider = document.getElementById("zoomSlider");
            var scrollSlider = document.getElementById("scrollSlider");
            
            zoomSlider.addEventListener("input", function() {
              zoomFactor = parseFloat(this.value);
              redraw();
            });
            
            scrollSlider.addEventListener("input", function() {
              scrollOffset = parseInt(this.value);
              redraw();
            });
            
            // Función que procesa los datos recibidos por el WebSocket
            function onMessageHandler(event) {
              // Si no se está colectando, se ignora el mensaje
              if (!isCollecting) return;
              var pinData = JSON.parse(event.data);
              var timestamp = new Date().toISOString();
              // Se guarda en el buffer interno, incluso si está pausado
              //dataBuffer.push({ timestamp: timestamp, data: pinData });
              
              // Si se encuentra en pausa, no actualiza la gráfica ni los indicadores
              if (isPaused) return;
              
              // Actualiza los indicadores LED según el estado recibido
              pinData.forEach(function(pinObj, index) {
                var led = document.getElementById("led" + index);
                if (led) {
                  led.style.backgroundColor = pinObj.state == 1 ? "#00ff99" : "#ff073a";
                  led.style.boxShadow = pinObj.state == 1 ? "0 0 10px #00ff99" : "0 0 10px #ff073a";
                }
              });
              
              // Actualiza el historial de cada pin
              for (var i = 0; i < numPins; i++) {
                // Se invierte el estado (manteniendo la lógica original)
                pinData[i].state = !pinData[i].state;
                var state = pinData[i].state;
                histories[i].push(state);
                if (histories[i].length > historyLength) {
                  histories[i].shift();
                  if (scrollOffset > 0) {
                    scrollOffset--;
                  }
                }
              }
              
              // Combina los 8 primeros bits en un solo byte
              var byteValue = 0;
              for (var i = 0; i < 8; i++) {
                byteValue = (byteValue << 1) | (pinData[i].state ? 1 : 0);
              }
              var binaryStr = byteValue.toString(2).padStart(8, '0');
              var decimalStr = byteValue.toString(10);
              var hexStr = "0x" + byteValue.toString(16).toUpperCase();
              var asciiStr = (byteValue >= 32 && byteValue < 127) ? String.fromCharCode(byteValue) : ".";
              
              document.getElementById("byteBinary").innerText = binaryStr;
              document.getElementById("byteDecimal").innerText = decimalStr;
              document.getElementById("byteHex").innerText = hexStr;
              document.getElementById("byteASCII").innerText = asciiStr;
              
              redraw();
            }
            
            socket.onmessage = onMessageHandler;
            
            // Función para redibujar la gráfica
            function redraw() {
              ctx.clearRect(0, 0, canvas.width, canvas.height);
              var scaleX = (canvas.width / historyLength) * zoomFactor;
              var visiblePoints = Math.floor(canvas.width / scaleX);
              var maxOffset = Math.max(0, histories[0].length - visiblePoints);
              scrollSlider.max = maxOffset;
              if (scrollOffset > maxOffset) {
                scrollOffset = maxOffset;
                scrollSlider.value = scrollOffset;
              }
              
              // Paleta de colores neon para cada pin
              var colors = ["#ff073a", "#00ff99", "#00bfff", "#ff8c00", "#8a2be2", "#ff4500", "#00ced1", "#ff1493", "#7b68ee"];
              
              for (var i = 0; i < numPins; i++) {
                if (histories[i].length === 0) continue;
                ctx.beginPath();
                var startIndex = scrollOffset;
                var x = 0;
                var state = histories[i][startIndex];
                var y = getY(i, state);
                ctx.moveTo(x, y);
                
                for (var j = startIndex + 1; j < histories[i].length; j++) {
                  var newState = histories[i][j];
                  var newX = (j - startIndex) * scaleX;
                  var newY = getY(i, newState);
                  if (newState !== state) {
                    ctx.lineTo(newX, y);
                    ctx.lineTo(newX, newY);
                    state = newState;
                    y = newY;
                  } else {
                    ctx.lineTo(newX, y);
                  }
                  if (newX > canvas.width) break;
                }
                ctx.strokeStyle = colors[i % colors.length];
                ctx.lineWidth = 2;
                ctx.stroke();
              }
            }
            
            // Controles de la gráfica
            document.getElementById("btnPlay").addEventListener("click", function(){
              // Si la adquisición no está activa, se inicia; si está en pausa se reanuda
              if (!isCollecting) {
                isCollecting = true;
                isPaused = false;
                // Si el socket está cerrado, se reabre la conexión
                if (socket.readyState === WebSocket.CLOSED) {
                  socket = new WebSocket("ws://" + window.location.hostname + ":81/");
                  socket.onmessage = onMessageHandler;
                }
              } else {
                isPaused = false;
              }
            });
            
            document.getElementById("btnPause").addEventListener("click", function(){
              if (isCollecting) {
                isPaused = true;
              }
            });
            
            document.getElementById("btnStop").addEventListener("click", function(){
              // Se detiene la adquisición y se cierra el socket
              isCollecting = false;
              isPaused = false;
              if (socket.readyState === WebSocket.OPEN) {
                socket.close();
              }
            });
            
            document.getElementById("btnRestart").addEventListener("click", function(){
              // Se limpia el buffer interno y se reinicia la gráfica
              dataBuffer = [];
              for (var i = 0; i < numPins; i++) {
                histories[i] = [];
              }
              scrollOffset = 0;
              redraw();
            });
            
            document.getElementById("btnExport").addEventListener("click", function(){
              // Se exporta el buffer interno en formato JSON
              var json = JSON.stringify(dataBuffer);
              var blob = new Blob([json], { type: "application/json" });
              var url = URL.createObjectURL(blob);
              var a = document.createElement("a");
              a.href = url;
              a.download = "data_export.json";
              a.click();
              URL.revokeObjectURL(url);
            });
          </script>
        </body>
        </html>
        )";
        
  server.send(200, "text/html", htmlContent);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\n", num, length);
      break;
  }
}

void sendPinStates() {
  String json = getPinStatesJson();
  webSocket.broadcastTXT(json);
}

String getPinStatesJson() {
  for (int i = 0; i < numPins; i++) {
    pinStates[i] = digitalRead(inputPins[i]);
  }

  String json = "[";
  for (int i = 0; i < numPins; i++) {
    json += "{\"pin\":" + String(inputPins[i]) + ",\"state\":" + String(pinStates[i]) + "}";
    if (i < numPins - 1) {
      json += ",";
    }
  }
  json += "]";
  
  return json;
}
