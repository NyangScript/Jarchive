#include "esp_camera.h"
#include "WiFi.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include "driver/rtc_io.h"
#include <WebServer.h> // ESP32 WebServer 라이브러리 사용


// WARNING!!! Make sure that public ssh key is changed in examples/ota_verify_ant_rollback/main/ota_verify_example_main.c
// Otherwise OTA verification will fail
#define PART_BOUNDARY "123456789000000000000987654321"


// AI-Thinker ESP32-CAM PIN Map
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1 // RST IO
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26 // SDA
#define SIOC_GPIO_NUM     27 // SCL


#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// Wi-Fi Credentials
const char* ssid = "2F-CR3_CR4";         // 실제 Wi-Fi SSID로 변경
const char* password = "WMS1348B2F"; // 실제 Wi-Fi 비밀번호로 변경


WebServer server(80); // HTTP 서버 포트 80번
WebServer streamServer(81); // MJPEG 스트리밍 서버 포트 81번


static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "s\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";


// Forward declarations
void handleRoot();
void handleStream();
void handleNotFound();


void setup() {
 WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector


 Serial.begin(115200);
 Serial.setDebugOutput(true);
 Serial.println();


 camera_config_t config;
 config.ledc_channel = LEDC_CHANNEL_0;
 config.ledc_timer = LEDC_TIMER_0;
 config.pin_d0 = Y2_GPIO_NUM;
 config.pin_d1 = Y3_GPIO_NUM;
 config.pin_d2 = Y4_GPIO_NUM;
 config.pin_d3 = Y5_GPIO_NUM;
 config.pin_d4 = Y6_GPIO_NUM;
 config.pin_d5 = Y7_GPIO_NUM;
 config.pin_d6 = Y8_GPIO_NUM;
 config.pin_d7 = Y9_GPIO_NUM;
 config.pin_xclk = XCLK_GPIO_NUM;
 config.pin_pclk = PCLK_GPIO_NUM;
 config.pin_vsync = VSYNC_GPIO_NUM;
 config.pin_href = HREF_GPIO_NUM;
 config.pin_sccb_sda = SIOD_GPIO_NUM;
 config.pin_sccb_scl = SIOC_GPIO_NUM;
 config.pin_pwdn = PWDN_GPIO_NUM;
 config.pin_reset = RESET_GPIO_NUM;
 config.xclk_freq_hz = 20000000; // 20MHz
 config.pixel_format = PIXFORMAT_JPEG; // JPEG 포맷으로 설정 (MJPEG에 적합)


 // 프레임 크기 설정 (해상도가 너무 높으면 스트리밍이 느릴 수 있음)
 // PIXFORMAT_JPEG일 때 다음 크기 중 하나를 사용:
 // FRAMESIZE_UXGA (1600x1200)
 // FRAMESIZE_SXGA (1280x1024)
 // FRAMESIZE_XGA (1024x768)
 // FRAMESIZE_SVGA (800x600)
 // FRAMESIZE_VGA (640x480)
 // FRAMESIZE_CIF (352x288)
 // FRAMESIZE_QVGA (320x240)
 // FRAMESIZE_HQVGA (240x176)
 // FRAMESIZE_QQVGA (160x120)
 config.frame_size = FRAMESIZE_VGA; // VGA (640x480)로 설정. 필요에 따라 변경
 config.jpeg_quality = 12; // 0-63, 낮을수록 품질 높음 (압축률 낮음), 높을수록 품질 낮음 (압축률 높음)
                          // 스트리밍에는 10-12 정도가 적당할 수 있음
 config.fb_count = 2;     // 프레임 버퍼 수. 2 이상 권장 (더블 버퍼링)
 config.fb_location = CAMERA_FB_IN_PSRAM; // PSRAM 사용 가능하면 PSRAM에 프레임 버퍼 할당
 config.grab_mode = CAMERA_GRAB_LATEST;   // 최신 프레임만 가져오도록 설정


 // 카메라 초기화
 esp_err_t err = esp_camera_init(&config);
 if (err != ESP_OK) {
   Serial.printf("Camera init failed with error 0x%x", err);
   return;
 }
 Serial.println("Camera initialized successfully.");


 // Wi-Fi 연결
 WiFi.begin(ssid, password);
 Serial.print("Connecting to WiFi...");
 while (WiFi.status() != WL_CONNECTED) {
   delay(500);
   Serial.print(".");
 }
 Serial.println("\nWiFi connected");
 Serial.print("IP Address: ");
 Serial.println(WiFi.localIP());


 // 웹 서버 핸들러 설정
 server.on("/", HTTP_GET, handleRoot);
 server.onNotFound(handleNotFound);
 server.begin(); // 포트 80 서버 시작
 Serial.println("HTTP server started on port 80");


 // MJPEG 스트리밍 서버 핸들러 설정
 streamServer.on("/stream", HTTP_GET, handleStream);
 streamServer.onNotFound(handleNotFound); // 스트림 서버에도 404 핸들러 추가
 streamServer.begin(); // 포트 81 서버 시작
 Serial.println("MJPEG Stream server started on port 81 (/stream)");
}


void loop() {
 server.handleClient();
 streamServer.handleClient();
 delay(1); // 다른 작업 처리 시간 확보 (선택적)
}


void handleRoot() {
 String html = "<html><head><title>ESP32-CAM Stream</title></head><body>";
 html += "<h1>ESP32-CAM MJPEG Stream</h1>";
 html += "<p>Stream available at: <a href='/stream'>/stream</a> (on port 81)</p>";
 html += "<img src='http://" + WiFi.localIP().toString() + ":81/stream' width='640' height='480' />";
 html += "</body></html>";
 server.send(200, "text/html", html);
}


void handleStream() {
 WiFiClient client = streamServer.client(); // 현재 연결된 클라이언트 가져오기
 if (!client.connected()) {
   return; // 클라이언트 연결 없으면 종료
 }


 // 스트림 응답 헤더 전송
 String response = "HTTP/1.1 200 OK\r\n";
 response += "Content-Type: " + String(_STREAM_CONTENT_TYPE) + "\r\n";
 response += "Connection: close\r\n"; // 각 프레임 전송 후 연결을 닫도록 유도 (클라이언트가 재연결)
                                    // 또는 "Connection: keep-alive" 와 함께 적절한 타이밍 관리 필요
 response += "Access-Control-Allow-Origin: *\r\n"; // CORS 허용 (필요한 경우)
 response += "\r\n";
 client.print(response); // 헤더 전송


 while (client.connected()) { // 클라이언트가 연결되어 있는 동안 반복
   camera_fb_t * fb = NULL;
   fb = esp_camera_fb_get(); // 프레임 버퍼 가져오기
   if (!fb) {
     Serial.println("Camera capture failed");
     // client.stop(); // 오류 발생 시 클라이언트 연결 종료
     delay(100); // 잠시 대기 후 재시도
     continue;
   }


   if(fb->format != PIXFORMAT_JPEG){
     Serial.println("Non-JPEG frame format. This should not happen with current config.");
     esp_camera_fb_return(fb); // 프레임 버퍼 반환
     delay(100);
     continue;
   }


   client.print(_STREAM_BOUNDARY); // 파트 경계 전송
  
   char buf[128];
   sprintf(buf, _STREAM_PART, fb->len); // Content-Type 및 Content-Length 정보 생성
   client.print(buf); // 파트 헤더 전송


   client.write(fb->buf, fb->len); // JPEG 데이터 전송
   client.print("\r\n"); // 파트 끝 CRLF


   esp_camera_fb_return(fb); // 사용한 프레임 버퍼 반환


   if(!client.connected()){ // 전송 후 클라이언트 연결 상태 확인
     break;
   }
   // delay(66); // 약 15 FPS (1000ms / 15fps = ~66ms). 프레임 속도 조절. 너무 짧으면 CPU 부하 증가
 }
 Serial.println("Client disconnected from stream.");
}


void handleNotFound() {
 String message = "File Not Found\n\n";
 message += "URI: ";
 message += server.uri(); // 또는 streamServer.uri() - 어떤 서버의 요청인지 구분 필요
 message += "\nMethod: ";
 message += (server.method() == HTTP_GET) ? "GET" : "POST";
 message += "\nArguments: ";
 message += server.args();
 message += "\n";
 for (uint8_t i = 0; i < server.args(); i++) {
   message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
 }
 // 현재 요청이 어떤 서버로 왔는지에 따라 server 또는 streamServer 사용
 if (server.client().localPort() == 80) {
     server.send(404, "text/plain", message);
 } else if (streamServer.client().localPort() == 81) {
     streamServer.send(404, "text/plain", message);
 }
}

