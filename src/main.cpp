#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include <math.h>
#include <Adafruit_Sensor.h>
#include <MQUnifiedsensor.h>
#include <HTTPClient.h>

// danh sách hằng số
#define DHT_TYPE DHT11

// danh sách các chân esp32
#define RED_PIN 5
#define GREEN_PIN 18
#define BLUE_PIN 19
#define BUZZER_PIN 22
#define DHT_PIN 33
#define FIRE_PIN 32
#define GAS_PIN 35

// thông số wifi
const char *ssid = "Nguyên";
const char *password = "12345678";

// thông số broker
String mqtt_server = "broker.hivemq.com";
const uint16_t mqtt_port = 1883;
String topic = "nguyentran/iot";

// Thông số môi trường
float humi, temp;
int fire, gas;

// mốc thời gian
int lastUpdate = 0;

// khởi tạo các cấu trúc dữ liệu
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// chuyển đổi 0-3.3V 12bit -> 0-5V 10bit
int convert(int a)
{
  return (int)(a / 4096.0 * 3.3 / 5.0 * 1024.0);
}

// đổi màu led
void changeLedColor(String color)
{
  if (color == "red")
  {
    digitalWrite(RED_PIN, HIGH);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
  }
  if (color == "green")
  {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
    digitalWrite(BLUE_PIN, LOW);
  }
  if (color == "blue") {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BLUE_PIN, HIGH);
  }
}

// bật còi báo động
void warning()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

// hàm reconnect broker
void reconnectBroker()
{
  Serial.println("Connecting to Broker ... ");

  // chưa kết nối được
  while (!mqttClient.connect("ESP32_ID1", "ESP_OFFLINE", 0, 0, "ESP32_ID1_OFFLINE"))
  {
    Serial.print("Error, rc = ");
    Serial.print(mqttClient.state());
    Serial.println("Try again in 5 seconds");

    changeLedColor("blue");
    delay(5000);
  }

  // Kết nối thành công
  changeLedColor("green");
  Serial.println("Connected to Broker!");
  delay(1000);
}

// hàm reconnect wifi
void reconnectWifi()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Connecting to WiFi ... ");
    WiFi.disconnect();
    WiFi.reconnect();

    changeLedColor("blue");
    delay(3000);
  }

  changeLedColor("green");
  Serial.print("Connected to Wifi: ");
  Serial.println(ssid);
  delay(1000);
}

void setup()
{
  Serial.begin(9600);

  // đặt chế độ cho các chân
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FIRE_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);

  // khởi động cảm biến DHT
  dht.begin();

  // Cấu hình Wifi
  WiFi.begin(ssid, password);

  // Cấu hình MQTT server và port
  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);

  // cho đèn màu xanh khi khởi động hệ thống
  changeLedColor("green");
}

void loop()
{
  // Mất kết nối tới Wifi
  if (WiFi.status() != WL_CONNECTED)
  {
    reconnectWifi();
    return;
  }

  // Sau khi kết nối Wifi, tiến hành kết nối lại MQTT broker nếu cần
  if (!mqttClient.connected())
  {
    reconnectBroker();
    // Hoàn tất kết nối
    // delay(1000);
  }

  // đủ 1 chu kỳ đọc dữ liệu
  if (millis() - lastUpdate > 5000)
  {
    // gán lại mốc thời gian
    lastUpdate = millis();

    // đọc dữ liệu từ cảm biến nhiệt độ, độ ẩm
    float humi = dht.readHumidity();
    float temp = dht.readTemperature();

    Serial.print(F("Độ ẩm: "));
    Serial.print(humi);
    Serial.print(F("%  Nhiệt độ: "));
    Serial.print(temp);
    Serial.print(F("°C "));

    // Đọc dữ liệu từ cảm biến lửa
    fire = analogRead(FIRE_PIN);
    fire = convert(fire);
    Serial.print("Fire: ");
    Serial.print(fire);

    // Đọc dữ liệu từ cảm biến khí gas
    gas = analogRead(GAS_PIN);
    gas = convert(gas);
    Serial.print(" Gas: ");
    Serial.println(gas);

    // nếu phát hiện lửa và rò rỉ khí gas
    if (fire < 600 || gas > 430)
    {
      if (fire < 600)
      {
        Serial.print("HAS FIRE");
      }
      if (gas > 430)
      {
        Serial.print(" HAS GAS");
      }
      Serial.println("");
      changeLedColor("red");
      warning();
    }
    else
    {
      changeLedColor("green");
    }

    // đóng gói dữ liệu
    JSONVar json;
    json["temp"] = temp;
    json["humi"] = humi;
    json["fire"] = fire;
    json["gas"] = gas;

    // gửi dữ liệu lên broker
    mqttClient.publish(topic.c_str(), JSON.stringify(json).c_str());
  }

  // Wait a few seconds between measurements.
  delay(100);
}