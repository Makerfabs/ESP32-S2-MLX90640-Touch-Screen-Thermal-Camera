/*
 MLX90640 thermal camera connected to a SparkFun Thing Plus - ESP32 WROOM

 Created by: Christopher Black
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h> // Used for I2C communication

#include <WebSocketsServer_Generic.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include "webpage.h"

#define SCL_PIN 39
#define SDA_PIN 38

#define SSID "Makerfabs"
#define PWD "20160704"

#define DEFAULT_SSID_HEAD "Makerfabs_IRCamera_"

#define SERIAL_PORT USBSerial

WiFiServer server(80);

// declare socket related variables
WebSocketsServer webSocket = WebSocketsServer(81);

// MLX90640 variables
#define TA_SHIFT -64; // Default shift for MLX90640 in open air is 8
static float mlx90640To[768];

// Used to compress data to the client
char positive[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char negative[27] = "abcdefghijklmnopqrstuvwxyz";

TaskHandle_t TaskA;
/* this variable hold queue handle */
xQueueHandle xQueue;

int total = 0;

void setup()
{
  SERIAL_PORT.begin(115200);
  SERIAL_PORT.flush();
  
  delay(1000);

  SERIAL_PORT.println();
  wifi_init();
  // ap_init();

  if (!MDNS.begin("thermal"))
  {
    SERIAL_PORT.println("Error setting up MDNS responder!");
  }
  else
  {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
    SERIAL_PORT.println("mDNS responder started");
  }

  server.begin();

  xQueue = xQueueCreate(1, sizeof(mlx90640To));
  xTaskCreatePinnedToCore(
      Task1,       /* pvTaskCode */
      "Workload1", /* pcName */
      100000,      /* usStackDepth */
      NULL,        /* pvParameters */
      1,           /* uxPriority */
      &TaskA,      /* pxCreatedTask */
      0);          /* xCoreID */

  xTaskCreate(
      receiveTask,   /* Task function. */
      "receiveTask", /* name of task. */
      10000,         /* Stack size of task */
      NULL,          /* parameter of the task */
      1,             /* priority of the task */
      NULL);         /* Task handle to keep track of created task */

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop()
{
  webSocket.loop();

  WiFiClient client = server.available(); // listen for incoming clients

  if (client)
  {
    SERIAL_PORT.println("New Client.");
    String currentLine = "";
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        SERIAL_PORT.write(c);
        if (c == '\n')
        {

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print("<script>const ipAddress = '");
            client.print(WiFi.localIP());
            client.print("'</script>");
            client.println();
            // the content of the HTTP response follows the header:
            client.print(canvas_htm);

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }
          else
          { // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
    SERIAL_PORT.println("Client Disconnected.");
  }
}

// Capture thermal image on a different thread
void Task1(void *parameter)
{
  const byte MLX90640_address = 0x33; // Default 7-bit unshifted address of the MLX90640

  Wire.setClock(400000L);
  Wire.begin(SDA_PIN, SCL_PIN);

  paramsMLX90640 mlx90640;
  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0)
  {
    SERIAL_PORT.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
    while (1)
      ;
  }
  SERIAL_PORT.println("MLX90640 online!");

  // Get device parameters - We only have to do this once
  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);

  if (status != 0)
  {
    SERIAL_PORT.println("Failed to load system parameters");
  }
  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0)
  {
    SERIAL_PORT.println("Parameter extraction failed");
  }
  MLX90640_SetRefreshRate(MLX90640_address, 0x05);
  Wire.setClock(1000000L);
  float mlx90640Background[768];
  while (1)
  {
    // digitalWrite(LED_PIN, HIGH);
    for (byte x = 0; x < 2; x++) // Read both subpages
    {
      uint16_t mlx90640Frame[834];
      int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
      if (status < 0)
      {
        SERIAL_PORT.print("GetFrame Error: ");
        SERIAL_PORT.println(status);
      }

      float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
      float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

      float tr = Ta - TA_SHIFT; // Reflected temperature based on the sensor ambient temperature
      float emissivity = 0.95;

      MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640Background);
    }

    /* time to block the task until the queue has free space */
    const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
    xQueueSendToFront(xQueue, &mlx90640Background, xTicksToWait);

    const TickType_t xDelay = 20 / portTICK_PERIOD_MS; // 8 Hz is 1/8 second
    // digitalWrite(LED_PIN, LOW);
    vTaskDelay(xDelay);
  }
}

void receiveTask(void *parameter)
{
  /* keep the status of receiving data */
  BaseType_t xStatus;
  /* time to block the task until data is available */
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
  while (1)
  {
    /* receive data from the queue */
    xStatus = xQueueReceive(xQueue, &mlx90640To, xTicksToWait);
    /* check whether receiving is ok or not */
    if (xStatus == pdPASS)
    {
      compressAndSend();
      total += 1;
    }
  }
  vTaskDelete(NULL);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
    SERIAL_PORT.println("Socket Disconnected.");
    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    SERIAL_PORT.println("Socket Connected.");
    // send message to client
    webSocket.sendTXT(num, "Connected");
  }
  break;
  case WStype_TEXT:
    // send message to client
    break;
  case WStype_BIN:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

// Some precision is lost during compression but data transfer speeds are
// much faster. We're able to get a higher frame rate by compressing data.
void compressAndSend()
{
  String resultText = "";
  int numDecimals = 1;
  int accuracy = 8;
  int previousValue = round(mlx90640To[0] * pow(10, numDecimals));
  previousValue = previousValue - (previousValue % accuracy);
  resultText.concat(numDecimals);
  resultText.concat(accuracy);
  resultText.concat(previousValue);
  resultText.concat(".");
  char currentLetter = 'A';
  char previousLetter = 'A';
  int letterCount = 1;
  int columnCount = 32;

  for (int x = 1; x < 768; x += 1)
  {
    int currentValue = round(mlx90640To[x] * pow(10, numDecimals));
    currentValue = currentValue - (currentValue % accuracy);
    if (x % columnCount == 0)
    {
      previousValue = round(mlx90640To[x - columnCount] * pow(10, numDecimals));
      previousValue = previousValue - (previousValue % accuracy);
    }
    int correction = 0;
    int diffIndex = (int)(currentValue - previousValue);
    if (abs(diffIndex) > 0)
    {
      diffIndex = diffIndex / accuracy;
    }
    if (diffIndex > 25)
    {
      // correction = (diffIndex - 25) * accuracy;
      diffIndex = 25;
    }
    else if (diffIndex < -25)
    {
      // correction = (diffIndex + 25) * accuracy;
      diffIndex = -25;
    }

    if (diffIndex >= 0)
    {
      currentLetter = positive[diffIndex];
    }
    else
    {
      currentLetter = negative[abs(diffIndex)];
    }

    if (x == 1)
    {
      previousLetter = currentLetter;
    }
    else if (currentLetter != previousLetter)
    {

      if (letterCount == 1)
      {
        resultText.concat(previousLetter);
      }
      else
      {
        resultText.concat(letterCount);
        resultText.concat(previousLetter);
      }
      previousLetter = currentLetter;
      letterCount = 1;
    }
    else
    {
      letterCount += 1;
    }

    previousValue = currentValue - correction;
  }
  if (letterCount == 1)
  {
    resultText.concat(previousLetter);
  }
  else
  {
    resultText.concat(letterCount);
    resultText.concat(previousLetter);
  }
  webSocket.broadcastTXT(resultText);
}

void wifi_init()
{
  SERIAL_PORT.print("Connecting to ");
  SERIAL_PORT.println(SSID);

  // Connect to the WiFi network
  WiFi.begin(SSID, PWD);

  String Host_name = "";
  Host_name = Host_name + DEFAULT_SSID_HEAD + get_uid();
  // WiFi.setHostname("esp32thing1");
  WiFi.setHostname(Host_name.c_str());

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    SERIAL_PORT.print(".");
  }

  SERIAL_PORT.println("");
  SERIAL_PORT.println("WiFi connected.");
  SERIAL_PORT.println("IP address: ");
  SERIAL_PORT.println(WiFi.localIP());
}

void ap_init()
{

  // Connect to the WiFi network
  String AP_name = "";
  AP_name = AP_name + DEFAULT_SSID_HEAD + get_uid();

  SERIAL_PORT.print("Start AP ");
  SERIAL_PORT.println(AP_name);

  WiFi.softAP(AP_name.c_str(), "12345678");
  WiFi.setHostname("esp32thing1");

  IPAddress myIP = WiFi.softAPIP();

  SERIAL_PORT.println("");
  SERIAL_PORT.println("WiFi connected.");
  SERIAL_PORT.println("IP address: ");
  SERIAL_PORT.println(myIP);
}

String get_uid()
{
  // uint64_t chipid;
  uint32_t chipid = 0;
  char c[20];

  // chipid = ESP.getEfuseMac();
  for (int i = 0; i < 17; i = i + 8)
  {
    chipid |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  sprintf(c, "%08X", (uint32_t)chipid);

  return (String)c;
}