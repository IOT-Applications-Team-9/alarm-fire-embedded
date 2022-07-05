# Fire Alarm Embedded
Phần code của vi xử lý ESP32 trong hệ thống báo cháy

ESP32 sử dụng giao thức MQTT để giao tiếp với server.

Các linh kiện:
- Cảm biến nhiệt độ, độ ẩm: DHT11
- Cảm biến lửa: MH-Sensor-Series
- Cảm biến khí gas: MQ-6
- Led 3 màu
- Màn LCD 1602 giao tiếp qua I2C
- Còi (buzzer)
