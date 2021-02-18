/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-websocket-server-arduino/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_AMG88xx.h>

/*
// wifi.h should contain the following, with your wifi credentials:
const char* ssid = "WIFI SSID";
const char* password = "WIFI PASSWORD";
 */
#include "wifi.h"



bool ledState = 0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Adafruit_AMG88xx amg;
float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
<style>
canvas {
  image-rendering: optimizeSpeed;             /* Older versions of FF          */
  image-rendering: -moz-crisp-edges;          /* FF 6.0+                       */
  image-rendering: -webkit-optimize-contrast; /* Safari                        */
  image-rendering: -o-crisp-edges;            /* OS X & Windows Opera (12.02+) */
  image-rendering: pixelated;                 /* Awesome future-browsers       */
  -ms-interpolation-mode: nearest-neighbor;   /* IE                            */
}
</style>
</head>
<body>
<canvas style="width: 400px; height: 400px;" width="8" height="8" id="canvas"></canvas><br/>
<input type="checkbox" id="cbNormalize" checked="true">Normalize</input>
<p id="state">state</p>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  var canvas = document.getElementById("canvas");
  var context = canvas.getContext('2d');
  var cbNormalize = document.getElementById("cbNormalize");

  window.addEventListener('load', onLoad);

  function clip(lo, val, hi) {
    return Math.max(lo, Math.min(hi, val));
  }


  function wrap(lo, val, hi) {
    let width = hi-lo;
    return ((((val+lo) %% width)+width) %% width)-lo;
  }

  function send(a, b, c, d) {
    let ws = websocket;
    ws.binaryType = 'arraybuffer';
    const data = new Int16Array(4);
    data[0] = a*0xFFFF;
    data[1] = b*0xFFFF;
    data[2] = c*0xFFFF;
    data[3] = d*0xFFFF;
    ws.send(data);
  }

  let txt = document.getElementById("txt");
   
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.binaryType = "arraybuffer";
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    document.getElementById('state').textContent = event.data;
    //window.msg = event.data;
    window.msg = new Float32Array(event.data);
    let pixels = context.getImageData(0,0,canvas.width,canvas.height);
    let s = 1;
    let c = 0;
    if (cbNormalize.checked) {
      let mn = 100000;
      let mx = -100000;
      for (i in msg) {
        let v = msg[i];
        if (v > mx) {
          mx = v;
        }
        if (v < mn) {
          mn = v;
        }
        c = -mn;
        s = 255 / (mx-mn);
      }
    }
    let idx = 0;
    for (i in msg) {
      pixels.data[idx++] = s*(msg[i]+c);
      pixels.data[idx++] = s*(msg[i]+c);
      pixels.data[idx++] = s*(msg[i]+c);
      pixels.data[idx++] = 255;
    }
    context.putImageData(pixels, 0, 0);
  }
  function onLoad(event) {
    initWebSocket();
  }
</script>
</body>
</html>
)rawliteral";

void notifyClients() {
  ws.textAll(String(ledState));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_BINARY) {
    data[len] = 0;


    int a = (data[1] * 0x100) + data[0];
    int b = (data[3] * 0x100) + data[2];
    int c = (data[5] * 0x100) + data[4];
    int d = (data[7] * 0x100) + data[6];

    Serial.print(a);
    Serial.print(",");
    Serial.print(b);
    Serial.print(",");
    Serial.print(c);
    Serial.print(",");
    Serial.print(d);
    Serial.println();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if (ledState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Start server
  server.begin();

  bool status;
  
  // default settings
  status = amg.begin();
  if (!status) {
      Serial.println("Could not find a valid AMG88xx sensor, check wiring!");
      while (1);
  }

  delay(500);
}

//#define LOG_DATA
#define LOG_STATS
int count = 0;
unsigned long timestart = 0;

void loop() {
  //read all the pixels
  amg.readPixels(pixels);
  ws.binaryAll((uint8_t *) &pixels, AMG88xx_PIXEL_ARRAY_SIZE * sizeof(float));

  #ifdef LOG_DATA
    Serial.print("[");
    for(int i=1; i<=AMG88xx_PIXEL_ARRAY_SIZE; i++){
      Serial.print(pixels[i-1]);
      Serial.print(", ");
      if( i%8 == 0 ) Serial.println();
    }
    Serial.println("]");
    Serial.println();
  #endif
  #ifdef LOG_STATS
    count++;
    if (count >= 10) {
      unsigned long timestop = millis();
      Serial.print("ms: ");
      Serial.println((timestop-timestart)/((double) count));
      count = 0;
      timestart = timestop;
    }
  #endif
  
  ws.cleanupClients();
  delay(50);
}
