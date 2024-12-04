// thư viện lay MAC từ smartphone
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
// thư viên gửi MAC lên server
#include <WiFi.h>
#include <HTTPClient.h>
#include <string.h>
//
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
// Khai báo các đối tượng và biến toàn cục
AsyncWebServer server(80); // Web server chạy trên cổng 80
DNSServer dnsServer;       // DNS server để điều hướng
Preferences preferences;   // Lưu trữ cấu hình vào flash
// UUID của dịch vụ và characteristic
#define SERVICE_UUID "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321"
// WIfi
String ssid;
String password;
// Server
String server_address;
String ipv4;
BLEServer *pServer = nullptr;
//
bool deviceConnected = false;
char connectedDeviceMAC[18] = "";
String copy_response;
const char *nguoila = "Không tìm thấy sinh viên với địa chỉ MAC này";
//
const int redPin = 13;
const int greenPin = 12;
const int bluePin = 14;
//
const int buttonPin = 27; // Using GPIO 33 for button
volatile bool buttonPressed = false;
unsigned long pressStartTime = 0;
unsigned long timePress = 0;
unsigned long timeLED = 0;
const unsigned long holdThreshold = 5000; // 5 seconds threshold

void IRAM_ATTR buttonInterrupt()
{
    if (digitalRead(buttonPin) == LOW)
    {                              // If button is pressed (LOW)
        pressStartTime = millis(); // Record the time button is pressed
        buttonPressed = true;      // Set flag to true
    }
}

const char index_html[] PROGMEM = R"rawliteral(
      <!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
  <title>Cấu hình Wi-Fi</title>
  <style>
    body {
      font-family: 'Arial', sans-serif;
      font-size: 18px; /* Tăng cỡ chữ */
    }
    h1 {
      font-size: 24px; /* Tăng cỡ chữ của tiêu đề */
    }
    input {
      font-size: 16px; /* Tăng cỡ chữ trong các ô nhập liệu */
    }
  </style>
</head>
<body>
  <h1>Cấu hình Wi-Fi</h1>
  <form action="/save" method="POST">
    SSID: <input type="text" name="ssid"><br>
    Password: <input type="password" name="password"><br>
    IPv4 Address:<input type="text" name="ipv4"><br>
    <input type="submit" value="Lưu">
  </form>
</body>
</html>
)rawliteral";

void startAdvertising();
bool sendMacToServer(const char *mac);
void handleSaveData(AsyncWebServerRequest *request);
void setupServer();
void config_wifi();
//

// Callback xử lý kết nối và ngắt kết nối
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
    {
        esp_bd_addr_t mac;
        memcpy(mac, param->connect.remote_bda, sizeof(esp_bd_addr_t));

        // Lưu địa chỉ MAC dưới dạng chuỗi
        sprintf(connectedDeviceMAC, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        Serial.printf("Device connected. MAC Address: %s\n", connectedDeviceMAC);
        deviceConnected = true; // Đánh dấu rằng thiết bị đã kết nối
    }

    void onDisconnect(BLEServer *pServer)
    {
        Serial.println("Device disconnected.");
        deviceConnected = false;      // Đánh dấu ngắt kết nối
        connectedDeviceMAC[0] = '\0'; // Xóa địa chỉ MAC
        startAdvertising();           // Quảng cáo lại khi ngắt kết nối
    }
};

void setup()
{
    // Khởi tạo Serial để in thông tin
    Serial.begin(115200);
    // setup led+buzzer
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);
    //
    pinMode(buttonPin, INPUT_PULLUP); // Set GPIO 33 as input with pull-u
    // Set up interrupt on GPIO 33, trigger when button is pressed (falling edge)
    attachInterrupt(digitalPinToInterrupt(buttonPin), buttonInterrupt, FALLING);
    // check wifi
    config_wifi();
    // Khởi tạo BLE và cấu hình
    BLEDevice::init("Do an 2");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Tạo dịch vụ GATT
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Tạo characteristic (không cần xử lý dữ liệu từ smartphone)
    pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

    // Bắt đầu dịch vụ
    pService->start();

    // Bắt đầu quảng cáo
    startAdvertising();
}

void loop()
{
    if (deviceConnected && connectedDeviceMAC[0] != '\0')
    {
        Serial.printf("Sending MAC to server: %s\n", connectedDeviceMAC);

        // Gửi địa chỉ MAC đến khi nào thành công
        if (sendMacToServer(connectedDeviceMAC))
        {
            Serial.println("MAC sent successfully. Disconnecting device...");
            //
            if (strcmp(copy_response.c_str(), nguoila) == 0)
            {
                Serial.println("nguoi la oi");
                digitalWrite(redPin, HIGH);  // Sáng đỏ
                digitalWrite(greenPin, LOW); // Tắt xanh lá
                digitalWrite(bluePin, LOW);  // Tắt xanh dương
                delay(1000);
                digitalWrite(redPin, LOW);   // Tắt đỏ
                digitalWrite(greenPin, LOW); // Sáng xanh lá
                digitalWrite(bluePin, LOW);  // Giữ màu đỏ trong 1 giây
            }
            else
            {
                // Serial.println(copy_response);
                digitalWrite(redPin, LOW);    // Sáng đỏ
                digitalWrite(greenPin, HIGH); // Tắt xanh lá
                digitalWrite(bluePin, LOW);
                delay(1000);
                digitalWrite(redPin, LOW);   // Tắt đỏ
                digitalWrite(greenPin, LOW); // Sáng xanh lá
                digitalWrite(bluePin, LOW);  // Giữ màu đỏ trong 1 giây
            }
            pServer->disconnect(0);  // Ngắt kết nối với thiết bị đầu tiên
            deviceConnected = false; // Reset trạng thái
        }
        else
        {
            Serial.println("Retrying to send MAC...");
        }
    }
    // Gọi dnsServer.processNextRequest() trong loop để xử lý các yêu cầu DNS
    if (WiFi.getMode() == WIFI_AP)
    {
        dnsServer.processNextRequest(); // Xử lý yêu cầu DNS
    }
    if (buttonPressed)
    {
        timePress = millis() - pressStartTime; // Calculate how long button has been pressed
        if (timePress >= holdThreshold)
        {
            Serial.println("Button pressed for 5 seconds! Entering interrupt.");
            timeLED = millis();
            while (millis() - timeLED < 2000)
            {
                digitalWrite(bluePin, HIGH); // Sáng xanh lá
                delay(300);
                digitalWrite(bluePin, LOW);
                delay(300);
            }
            preferences.clear();   // Call interrupt action
            buttonPressed = false; // Reset button pressed flag
            WiFi.mode(WIFI_AP);
            WiFi.softAP("ESP32", "12345678"); // Mở AP với SSID và mật khẩu
            dnsServer.start(53, "*", WiFi.softAPIP());
            setupServer();
        }
    }
    delay(1000); // Tránh lặp nhanh quá
}

bool sendMacToServer(const char *mac)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        String jsonPayload = "{\"mac_address\": \"" + String(mac) + "\"}";
        server_address = "http://" + ipv4 + ":" + "3000";
        String url = server_address + "/check-mac";
        http.begin(url.c_str());
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(jsonPayload);

        if (httpResponseCode > 0)
        {
            Serial.printf("Server response: %d\n", httpResponseCode);
            String response = http.getString();
            copy_response = response;
            Serial.println("Phản hồi từ server:");
            Serial.println(response);
            http.end();
            return true; // Gửi thành công
        }
        else
        {
            Serial.printf("Failed to send MAC, error: %d\n", httpResponseCode);
            http.end();
            return false; // Gửi thất bại
        }
    }
    else
    {
        Serial.println("WiFi not connected.");
        return false;
    }
}
// Hàm bắt đầu quảng cáo
void startAdvertising()
{
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    Serial.println("Advertising started...");
}

void handleSaveData(AsyncWebServerRequest *request)
{
    // Kiểm tra và lấy các tham số từ form
    if (request->hasParam("ssid", true))
    {
        ssid = request->getParam("ssid", true)->value();
        preferences.putString("ssid", ssid);
        Serial.println(ssid);
    }
    if (request->hasParam("password", true))
    {
        password = request->getParam("password", true)->value();
        preferences.putString("password", password);
        Serial.println(password);
    }
    if (request->hasParam("ipv4", true))
    {
        ipv4 = request->getParam("ipv4", true)->value();
        preferences.putString("ipv4", ipv4);
        Serial.println(ipv4);
    }
    // Gửi lại thông báo cho người dùng sau khi nhận dữ liệu
    String message = "Cấu hình Wi-Fi đã lưu. SSID: " + ssid + ", Password: " + password + ", IPv4: " + ipv4;
    request->send(200, "text/plain", message);
    // config_wifi();
    WiFi.begin(ssid, password);
    unsigned long startTime = millis(); // Khởi tạo biến startTime
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 4900)
    {
        delay(1000);
        Serial.print(".");
        yield(); // Gọi yield() để tránh hệ thống bị treo
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Kết nối Wi-Fi thất bại, chuyển sang AP mode.");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32", "12345678"); // Mở AP với SSID và mật khẩu
        dnsServer.start(53, "*", WiFi.softAPIP());
        setupServer();
    }
    else
    {
        Serial.println("Kết nối Wi-Fi thành công");
        digitalWrite(redPin, LOW);   // Sáng đỏ
        digitalWrite(greenPin, LOW); // Tắt xanh lá
        digitalWrite(bluePin, HIGH);
        delay(1000);
        digitalWrite(redPin, LOW);   // Tắt đỏ
        digitalWrite(greenPin, LOW); // Sáng xanh lá
        digitalWrite(bluePin, LOW);
    }
}
void setupServer()
{
    // Cung cấp trang cấu hình Wi-Fi
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html); });

    // Xử lý form dữ liệu người dùng gửi từ /save
    server.on("/save", HTTP_POST, handleSaveData);

    // Windows-specific routes
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", "Microsoft NCSI"); });

    server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", index_html); });

    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", index_html); });

    // Default handler for unknown URLs
    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(200, "text/html", index_html); });

    server.begin();
}
void config_wifi()
{
    preferences.begin("wificonfig", false);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    ipv4 = preferences.getString("ipv4", "");
    Serial.println(ssid);
    Serial.println(password);

    WiFi.begin(ssid, password); // Bắt đầu kết nối Wi-Fi

    unsigned long startTime = millis(); // Khởi tạo biến startTime
    while (WiFi.status() != WL_CONNECTED)
    {

        unsigned long elapsedTime = millis() - startTime;
        if (elapsedTime >= 4900)
        { // Nếu kết nối quá 10 giây
            break;
        }
        delay(1000);       // Chờ nửa giây
        Serial.print("."); // In ra dấu chấm khi đang kết nối
        yield();           // Gọi yield() để tránh hệ thống bị treo
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Kết nối Wi-Fi thất bại, chuyển sang AP mode.");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32", "12345678"); // Mở AP với SSID và mật khẩu
        dnsServer.start(53, "*", WiFi.softAPIP());
        setupServer();
    }
    else
    {
        Serial.println("Kết nối Wi-Fi thành công");
        digitalWrite(redPin, LOW);   // Tắt đèn đỏ
        digitalWrite(greenPin, LOW); // Tắt đèn xanh lá
        digitalWrite(bluePin, HIGH); // Bật đèn xanh dương
        delay(1000);
        digitalWrite(redPin, LOW);   // Tắt đèn đỏ
        digitalWrite(greenPin, LOW); // Tắt đèn xanh lá
        digitalWrite(bluePin, LOW);  // Tắt đèn xanh dương
    }
}
