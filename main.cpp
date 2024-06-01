#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClientSecure.h>

#define ECHO 5
#define TRIG 18
#define RELAY1 13
#define RELAY2 12
#define sensor_pin 32 /* Soil moisture sensor O/P pin */
#define MSG_BUFFER_SIZE (50)

WiFiClient WifiClient;
PubSubClient client(WifiClient);
WiFiClientSecure httpClient;
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);

String project = "Irigasi Tetes";
String id = "SK1";
String clientID = "IrigasiTetes/" + id;

// WiFi credentials
const char *ssid = "MAKERINDO2";        //"Laujulu5G";
const char *password = "makerindo2019"; //"366milala";

// MQTT broker parameters
// const char *mqtt_server = "bitanic.id";
const char *mqtt_server = "broker.hivemq.com"; //"broker.emqx.io";
const char *mqtt_user = "jes";
const char *mqtt_pass = "password";
const char *topic = "IrigasiTetes";

// Gscript ID and required credentials
const int httpsPort = 443;
const char *host = "script.google.com";
String GAS_ID = "AKfycbzR9i7pz-Rse2clZP23Rrstu6gvsNWgzCuXaUm2j--ClX7tEoBEJKlXKRSGQpdhsGjU";

String relay1status = "";
String relay2status = "";
String soilstatus = "";
unsigned long awal = millis();
unsigned long lastMsg = millis();

long duration;
int distance;
int i = 0;
int sum = 0;
int value = 0;
double jarak = 0;
int LCD_Milis = 0;
int LCD_Count = 0;
double _moisture, sensor_analog;
char msg[MSG_BUFFER_SIZE];

void sendData(double jarak, double _moisture, String soilstatus, String relay1status, String relay2status);

void lcdPrint(String text1, String text2)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(text1);
    lcd.setCursor(0, 1);
    lcd.print(text2);
}

void sensorJSN()
{
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(20);
    digitalWrite(TRIG, LOW);

    duration = pulseIn(ECHO, HIGH);
    distance = (duration * 0.034 / 2);

    i++;
    sum += distance;
    if (i == 3)
    {
        jarak = ((sum / 3) + 5);
        i = 0;
        sum = 0;
    }
    // Serial.println(jarak);
    Serial.print("Jarak Air : ");
    Serial.println(jarak);

    if (jarak >= 30)
    {
        digitalWrite(RELAY2, HIGH);
        delay(1000);
    }
    else if (jarak <= 27)
    {
        digitalWrite(RELAY2, LOW);
    }
}
void sensorsoil()
{
    sensor_analog = analogRead(sensor_pin);
    _moisture = (100 - ((sensor_analog / 4095.00) * 100));
    Serial.print("Soil Moisture = ");
    Serial.print(_moisture); /* Print Temperature on the serial window */
    Serial.println("%");

    if (_moisture >= 45)
    {                              // jika kelembaban tanah lebih 45%
        digitalWrite(RELAY1, LOW); // pompa dorong mati
    }
    else if (_moisture <= 45)
    {                               // selain atau dibawah 45%
        digitalWrite(RELAY1, HIGH); // pompa menyala
    }

    delay(1000);
}

class Task
{
private:
    unsigned long previousMillis; // Waktu terakhir tugas dieksekusi
    unsigned long interval;       // Interval waktu antara eksekusi tugas
    void (*lcdUpdate)();          // Pointer ke fungsi tugas

public:
    Task(unsigned long interval, void (*lcdUpdateParam)())
    {
        this->interval = interval;
        this->lcdUpdate = lcdUpdateParam;
        previousMillis = 0;
    }

    // Fungsi untuk menjalankan tugas jika sudah waktunya
    void run()
    {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval)
        {
            previousMillis = currentMillis;
            lcdUpdate();
        }
    }
};

Task dataUpdate(1000, []()
                {
  sensorJSN();
  sensorsoil();

  bool soil;
  if (_moisture >= 45)
  {
    soil = false;
  }
  else if (_moisture <= 45)
  {
    soil = true;
  }

  soilstatus = (soil == false) ? "Basah" : "Kering";

  relay1status = (digitalRead(RELAY1) == LOW) ? "Mati" : "Hidup";
  relay2status = (digitalRead(RELAY2) == LOW) ? "Mati" : "Hidup"; });

Task lcdUpdate(1500, []()
               {
  // = = = = = = = = = = = = = LCD Print = = = = = = = = = = = = =
  // Start Milis for print LCD every 2000ms
  if (millis() - LCD_Milis >= 2000) {
    LCD_Milis = millis();
    if (LCD_Count == 0) {
      lcdPrint(" SISTEM IRIGASI", "     TETES");
      LCD_Count = 1;
    } else if (LCD_Count == 1) {
      lcdPrint("K.Tanah : " + String (_moisture) + "%", "S.Tanah : " + soilstatus);
      LCD_Count = 2;
    } else if (LCD_Count == 2) {
      lcdPrint("   Jarak Air  ", "      " + String(jarak) + "cm");
      LCD_Count = 3;
    } else if (LCD_Count == 3) {
      lcdPrint("P.DORONG : " + relay1status, "P.HISAP  : " + relay2status);
      LCD_Count = 4;
    }
    else if (LCD_Count == 4) {
      LCD_Count = 1;
    }
  }
  lcdUpdate.run(); });

void setup(void)
{
    Serial.begin(115200);
    client.setServer(mqtt_server, 1883);

    // lcd.begin();
    lcd.init();
    lcd.backlight();
    lcd.clear();

    lcdPrint(" Irigasi Tetes", "Sistem Komputer 1");
    delay(3000);
    lcdPrint("Connecting to..", "      WiFi");

    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");
    }
    // Print local IP address and start web server
    lcdPrint("WiFi", "Connected..");
    delay(3000);
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.print(WiFi.localIP());
    Serial.println();

    httpClient.setInsecure();

    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);

    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientID = "IrigasiTetes" + id;
        clientID += String(random(0xffff), HEX);

        // Attempt to connect
        if (client.connect(clientID.c_str()))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish("outTopic", "Systems Irigasi Tetes");
            // ... and resubscribe
            client.subscribe("inTopic");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
    Serial.println(" ");
}

void loop(void)
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    unsigned long sekarang = millis();
    if (sekarang - awal > 2000)
    {
        awal = sekarang;
        sensorJSN();
        sensorsoil();
    }
    unsigned long now2 = millis();
    if (now2 - lastMsg > 3000)
    {
        ++value;
        Serial.println(msg);

        String payloadStr = "01," + (String)_moisture + "," + (String)soilstatus + "," +
                            (String)jarak + "," + (String)relay1status + "," +
                            (String)relay2status + ",#";

        // String payloadStr = "01," + (String)_moisture + "," + (String)jarak + ",#";

        // check if the conversion was successful
        client.publish(topic, payloadStr.c_str());
        sendData(jarak, _moisture, soilstatus, relay1status, relay2status);
        // delay(5000);
    }
    dataUpdate.run();
    lcdUpdate.run();
    /* Wait for 1000mS */
}

void sendData(double jarak, double _moisture, String soilstatus, String relay1status, String relay2status)
{
    // HTTP Connect
    // Serial.println("==========");
    Serial.print("Connecting to ");
    Serial.println(host);

    if (!httpClient.connect(host, httpsPort))
    {
        Serial.println("connection failed");
        return;
    }

    String url = "https://script.google.com/macros/s/" + GAS_ID + "/exec?jarak=" + jarak + "&_moisture=" + _moisture + "&soilstatus=" + soilstatus + "&relay1status=" + relay1status + "&relay2status=" + relay2status;
    Serial.print("requesting URL: ");
    Serial.println(url);

    httpClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "User-Agent: IrigasiTetes\r\n" +
                     "Connection: close\r\n\r\n");

    Serial.println("request sent");
    Serial.println("");

    while (httpClient.connected())
    {
        String line = httpClient.readStringUntil('\n');
        if (line == "\r")
        {
            Serial.println("headers received");
            break;
        }
    }
    String line = httpClient.readStringUntil('\n');
    if (line.startsWith("{\"state\":\"success\""))
    {
        Serial.println("esp32/Arduino CI successfull!");
    }
    else
    {
        Serial.println("esp32/Arduino CI has failed");
    }
    Serial.print("reply was : ");
    Serial.println(line);
    Serial.println("closing connection");
    httpClient.stop();
    Serial.println("==========");
    Serial.println();
}