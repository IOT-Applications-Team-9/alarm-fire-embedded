/**************** Import thư viện ******************/
#include <Wire.h>
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
#include <LiquidCrystal_I2C.h>

/**************** Danh sách hằng số ******************/
#define DHT_TYPE DHT11
#define LCD_ADDR 0x27 // địa chỉ I2C
#define LCD_HEIGHT 2  // chiều cao LCD
#define LCD_WIDTH 16  // chiều rộng LCD
#define STEP 100
#define DATA_CYCLE 5000 // gửi dữ liệu mỗi 5s

/**************** Danh sách GPIO ******************/
#define RED_PIN 15
#define GREEN_PIN 2
#define BLUE_PIN 4
#define BUZZER_PIN 5
#define DHT_PIN 33
#define FIRE_PIN 32
#define GAS_PIN 35

/**************** Tham số sử dụng ******************/
// thông số wifi
const char *ssid = "Nguyen";
const char *password = "12345678";

// thông số broker
String mqtt_server = "broker.hivemq.com";
const uint16_t mqtt_port = 1883;
String dataTopic = "mqtt_fas_data";
String commandTopic = "mqtt_fas_command";
String stateTopic = "mqtt_fas_state";

// trạng thái bật/tắt của hệ thống
int isOn = 1;

// id của thiết bị
const int deviceId = 305419896;

// Thông số môi trường
float humi, temp;
int fire, gas;
const int fireThreshold = 3500, gasThreshold = 2375;
boolean hasFire, hasGas;

// mốc thời gian
int lastUpdate = 0;

/**************** Khởi tạo cấu trúc dữ liệu ******************/
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_WIDTH, LCD_HEIGHT);

/**************** Định nghĩa hàm ******************/
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
  if (color == "blue")
  {
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

// Hiển thị khung dữ liệu
void displayInfo(float temp, float humi)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.setCursor(0, 1);
  lcd.print("Humi:");
  lcd.setCursor(9, 0);
  lcd.print("Fire:No");
  lcd.setCursor(9, 1);
  lcd.print("Gas :No");
  lcd.setCursor(7, 0);
  lcd.print("C");
  lcd.setCursor(7, 1);
  lcd.print("%");

  // Thông số môi trường
  lcd.setCursor(5, 0);
  lcd.print((int)round(temp));
  lcd.setCursor(5, 1);
  lcd.print((int)round(humi));
}

// hiển thị dữ liệu server gửi về
void displayStatus(int hasFire, int hasGas)
{
  if (hasFire && hasGas)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FIRE!");
    lcd.setCursor(7, 0);
    lcd.print("GAS LEAK!");
  }
  else if (hasFire)
  {
    lcd.clear();
    lcd.setCursor(5, 0);
    lcd.print("FIRE!!!");
  }
  else if (hasGas)
  {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("GAS LEAK!!!");
  }

  lcd.setCursor(4, 1);
  lcd.print("        ");
  lcd.setCursor(4, 1);
  lcd.print("!DANGER!");
}

// Cập nhật trạng thái lên server
void stateReport(int state)
{
  Serial.print("Update trạng thái: ");
  Serial.println(state);
  JSONVar json;
  json["deviceid"] = deviceId;
  json["state"] = state;
  mqttClient.publish(stateTopic.c_str(), JSON.stringify(json).c_str());
}

// Hàm call back khi có dữ liệu gửi tới từ Broker
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("New message from topic: ");
  Serial.println(topic);
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Xử lý ở đây
  if ((String)topic == commandTopic)
  {
    JSONVar command = JSON.parse((String)((const char *)payload));

    // Kiểm tra xem tín hiệu điều khiển có phải cho mình không
    if ((int)command["deviceid"] == deviceId)
    {
      Serial.println("Right ID");

      // Tín hiệu nhận được là tín hiệu điều khiển
      if ((String)((const char *)command["type"]) == "control")
      {
        Serial.println("Control Signal");
        int signal = (int)command["state"];

        // Tín hiệu điều khiển khác với trạng thái hiện tại
        if (signal != isOn)
        {
          isOn = signal;

          // gửi cập nhật tới server
          stateReport(isOn);

          // Xử lý
          if (isOn)
          {
            Serial.println("Signal: Turn on");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("System is ON!");
            changeLedColor("green");
          }
          else
          {
            Serial.println("Signal: Turn off");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("System is OFF!");
            changeLedColor("blue");
          }

          delay(1000);
        }
      }

      // Tín hiệu nhận được là tín hiệu cảnh báo
      else
      {
        Serial.println("Warning Signal");

        // Lấy tham số gửi về
        int hasFire = command["hasFire"];
        int hasGas = command["hasGas"];
        int danger = command["danger"];

        // Chuyển màu led
        if (!danger)
        {
          changeLedColor("green");
        }
        else
        {
          // Hiển thị lên
          if (isOn)
          {
            displayStatus(hasFire, hasGas);
          }
          changeLedColor("red");
          warning();
        }
      }
    }
  }
}

// hàm reconnect broker
void reconnectBroker()
{
  Serial.println("Connecting to Broker ... ");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("Broker ...");

  // chưa kết nối được
  while (!mqttClient.connect("ESP32_ID1", "ESP_OFFLINE", 0, 0, "ESP32_ID1_OFFLINE"))
  {
    Serial.print("Error, rc = ");
    Serial.print(mqttClient.state());
    Serial.println("Try again in 5 seconds");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error!!!");
    lcd.setCursor(0, 1);
    lcd.print("Try again in 5s");

    changeLedColor("blue");
    delay(5000);
  }

  // Kết nối thành công
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected to");
  lcd.setCursor(0, 1);
  lcd.print("Broker");

  // Subscribe vào topic command để nhận tín hiệu điều khiển
  mqttClient.subscribe(commandTopic.c_str());

  changeLedColor("green");
  Serial.println("Connected to Broker!");
  delay(1000);
}

// hàm reconnect wifi
void reconnectWifi()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("Wifi ...");

  // chưa kết nối được
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Connecting to WiFi ... ");
    WiFi.disconnect();
    WiFi.reconnect();

    changeLedColor("blue");
    delay(3000);
  }

  // kết nối thành công
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected to");
  lcd.setCursor(0, 1);
  lcd.print("Wifi!");
  lcd.setCursor(7, 1);
  lcd.print(ssid);

  changeLedColor("green");
  Serial.print("Connected to Wifi: ");
  Serial.println(ssid);
  delay(1000);
}

/**************** Cấu hình hệ thống ******************/
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

  // Cấu hình màn LCD
  Serial.println("Starting LCD ... ");
  lcd.init();
  lcd.backlight();
  lcd.print("Hello!");
  delay(1000);

  // Cấu hình Wifi
  WiFi.begin(ssid, password);

  // Cấu hình MQTT server và port
  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
  mqttClient.setCallback(callback);

  // cho đèn màu xanh khi khởi động hệ thống
  changeLedColor("green");
  //stateReport(isOn);
}

/**************** Vòng lặp hệ thống ******************/
void loop()
{
  /**************** 1. Kiểm tra kết nối ******************/
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
    lcd.clear();
    lcd.setCursor(0, 0);
    if (isOn)
    {
      lcd.print("System is ON!");
    }
    else
    {
      lcd.print("System is OFF!");
    }
    delay(500);
  }

  // Đọc dữ liệu trên mqtt queue
  mqttClient.loop();

  /**************** 2. Gửi dữ liệu lên server ******************/
  // Hệ thống đang bật
  if (isOn)
  {
    // đủ 1 chu kỳ đọc dữ liệu
    if (millis() - lastUpdate > DATA_CYCLE)
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
      Serial.print("Fire: ");
      Serial.print(fire);

      // Đọc dữ liệu từ cảm biến khí gas
      gas = analogRead(GAS_PIN);
      Serial.print(" Gas: ");
      Serial.println(gas);

      // đóng gói dữ liệu
      JSONVar json;
      json["deviceid"] = deviceId;
      json["temperature"] = temp;
      json["humidity"] = humi;
      json["fire"] = fire;
      json["gas"] = gas;

      // gửi dữ liệu lên broker
      mqttClient.publish(dataTopic.c_str(), JSON.stringify(json).c_str());

      // Hiển thị dữ liệu
      displayInfo(temp, humi);
    }
  }

  // Wait a few seconds between measurements.
  delay(STEP);
}