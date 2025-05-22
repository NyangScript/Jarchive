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


// ESP32-S3 AI Camera PIN Map
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     5
#define Y9_GPIO_NUM       4
#define Y8_GPIO_NUM       6
#define Y7_GPIO_NUM       7
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       17
#define Y4_GPIO_NUM       21
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       16
#define VSYNC_GPIO_NUM    1
#define HREF_GPIO_NUM     2
#define PCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM  8
#define SIOC_GPIO_NUM  9


// Wi-Fi Credentials
const char* ssid = "TOFFICE-RPL";         // 실제 Wi-Fi SSID로 변경
const char* password = "WMS1348B2F"; // 실제 Wi-Fi 비밀번호로 변경


WebServer server(80); // HTTP 서버 포트 80번
WebServer streamServer(81); // MJPEG 스트리밍 서버 포트 81번


static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n"; // Boundary corrected
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";


// Forward declarations
void handleRoot();
void handleStream();
void handleNotFound();


void setup() {
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
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // 카메라 초기화
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("카메라 초기화 성공");

  // Wi-Fi 연결
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("WiFi 연결 중...");
  
  int wifi_retry = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retry < 20) {
    delay(500);
    Serial.print(".");
    wifi_retry++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi 연결 실패");
    return;
  }
  
  Serial.println("\nWiFi 연결 성공");
  Serial.print("IP 주소: ");
  Serial.println(WiFi.localIP());

  // 웹 서버 핸들러 설정
  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP 서버가 포트 80에서 시작됨");

  // MJPEG 스트리밍 서버 핸들러 설정
  streamServer.on("/stream", HTTP_GET, handleStream);
  streamServer.onNotFound(handleNotFound);
  streamServer.begin();
  Serial.println("MJPEG 스트림 서버가 포트 81에서 시작됨 (/stream)");
}


void loop() {
 server.handleClient();
 streamServer.handleClient();
 delay(1); // 다른 작업 처리 시간 확보 (선택적)
}


void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>";
  html += "<title>ESP32-CAM 안전 모니터링</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; }";
  html += "#video-container { width: 100%; position: relative; background-color: #000; }";
  html += "#video-stream { width: 100%; height: auto; display: block; }";
  html += "#info-panel { width: 100%; background-color: #fff; padding: 15px; box-shadow: 0 -2px 10px rgba(0,0,0,0.1); }";
  html += "h2 { color: #333; font-size: 20px; margin-bottom: 15px; }";
  html += ".info-section { margin-bottom: 15px; }";
  html += ".info-section p { margin: 5px 0; font-size: 16px; color: #666; }";
  html += ".warning-message { color: #ff4444; font-size: 14px; padding: 8px; background-color: #ffeeee; border-radius: 4px; margin-bottom: 8px; }";
  html += ".normal-status { color: #4CAF50; font-size: 16px; padding: 8px; background-color: #e8f5e9; border-radius: 4px; }";
  html += ".button-container { margin-top: 15px; }";
  html += "#analyze-button { width: 100%; padding: 12px; background-color: #2196F3; color: white; border: none; border-radius: 4px; font-size: 16px; cursor: pointer; }";
  html += "#analyze-button:disabled { background-color: #ccc; }";
  html += "#analysis-status { text-align: center; margin-top: 10px; font-size: 14px; }";
  html += "@media (min-width: 768px) {";
  html += "  body { display: flex; }";
  html += "  #video-container { flex: 2; }";
  html += "  #info-panel { flex: 1; max-width: 300px; }";
  html += "}";
  html += "</style>";
  html += "</head><body>";

  // 영상 영역
  html += "<div id='video-container'>";
  html += "<img id='video-stream' src='http://" + WiFi.localIP().toString() + ":81/stream' crossorigin='anonymous'>";
  html += "</div>";

  // 정보 패널
  html += "<div id='info-panel'>";
  html += "<h2>안전 모니터링</h2>";
  html += "<div class='info-section'><p>시간: <span id='current-time'>로딩 중...</span></p></div>";
  html += "<div id='message-area'>";
  html += "<div class='normal-status' id='status-text'>정상 상태</div>";
  html += "</div>";
  html += "<div class='button-container'>";
  html += "<button id='analyze-button'>분석 시작</button>";
  html += "</div>";
  html += "<div id='analysis-status'></div>";
  html += "</div>";

  html += "<script>";
  html += "const GEMINI_API_KEY = 'AIzaSyCK5WE5NxHlCHQGd5agdkl5dZs0KLgFIXM';";
  html += "const GEMINI_API_URL = 'https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-preview-04-17:generateContent?key=' + GEMINI_API_KEY;";

  // 위험 시나리오 정의
  html += "const dangerScenarios = [";
  html += "{ id: 'left_hand', name: '왼손 들기', warning: '테스트: 왼손을 들었습니다!', description: '사람이 왼손을 들고 있는지' },";
  html += "{ id: 'right_hand', name: '오른손 들기', warning: '테스트: 오른손을 들었습니다!', description: '사람이 오른손을 들고 있는지' },";
  html += "{ id: 'standing', name: '서있음', warning: '테스트: 서있는 자세입니다!', description: '사람이 서있는 자세인지' },";
  html += "{ id: 'sitting', name: '앉아있음', warning: '테스트: 앉아있는 자세입니다!', description: '사람이 앉아있는 자세인지' },";
  html += "{ id: 'walking', name: '걷기', warning: '테스트: 걷는 동작입니다!', description: '사람이 걷는 동작을 하고 있는지' }";
  html += "];";

  html += "let dangerStates = {};";
  html += "let isAnalyzing = false;";
  html += "let analysisInterval = 3000;";
  html += "let analysisTimer = null;";
  html += "let speakingTimer = {};";

  // dangerStates 초기화
  html += "dangerScenarios.forEach(scenario => {";
  html += "  dangerStates[scenario.id] = 0;";
  html += "});";

  // 시간 업데이트 함수
  html += "function updateTime() {";
  html += "  const now = new Date();";
  html += "  const timeString = now.toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });";
  html += "  document.getElementById('current-time').textContent = timeString;";
  html += "}";
  html += "setInterval(updateTime, 1000);";
  html += "updateTime();";

  // 이미지 캡처 함수
  html += "function captureFrame() {";
  html += "  const video = document.getElementById('video-stream');";
  html += "  const canvas = document.createElement('canvas');";
  html += "  canvas.width = video.naturalWidth;";
  html += "  canvas.height = video.naturalHeight;";
  html += "  const context = canvas.getContext('2d');";
  html += "  context.drawImage(video, 0, 0, canvas.width, canvas.height);";
  html += "  return canvas.toDataURL('image/jpeg').split(',')[1];";
  html += "}";

  // Gemini API 호출 함수
  html += "async function analyzeImage(base64Image) {";
  html += "  try {";
  html += "    const response = await fetch(GEMINI_API_URL, {";
  html += "      method: 'POST',";
  html += "      headers: { 'Content-Type': 'application/json' },";
  html += "      body: JSON.stringify({";
  html += "        contents: [{";
  html += "          parts: [{";
  html += "            text: '다음 상황들을 분석하여 각각 0(아니오) 또는 1(예)로 JSON 형식으로 응답해주세요. 반드시 코드블록(```) 없이 순수 JSON만 반환하세요:\\n' +";
  html += "              dangerScenarios.map(s => '- ' + s.description).join('\\n') +";
  html += "              '\\n응답 형식: {\\\"left_hand\\\": 0, \\\"right_hand\\\": 0, \\\"standing\\\": 0, \\\"sitting\\\": 0, \\\"walking\\\": 0}'";
  html += "          }, {";
  html += "            inline_data: { mime_type: 'image/jpeg', data: base64Image }";
  html += "          }]";
  html += "        }]";
  html += "      })";
  html += "    });";

  html += "    if (!response.ok) {";
  html += "      throw new Error(`API 요청 실패: ${response.status}`);";
  html += "    }";

  html += "    const data = await response.json();";
  html += "    if (data.candidates && data.candidates[0] && data.candidates[0].content && data.candidates[0].content.parts && data.candidates[0].content.parts[0]) {";
  html += "      let textResponse = data.candidates[0].content.parts[0].text;";
  html += "      textResponse = textResponse.trim();";
  html += "      if (textResponse.startsWith('```')) {";
  html += "        textResponse = textResponse.split('```')[1] || textResponse;";
  html += "      }";
  html += "      textResponse = textResponse.replace('json', '').trim();";
  html += "      return JSON.parse(textResponse);";
  html += "    }";
  html += "    return null;";
  html += "  } catch (error) {";
  html += "    console.error('분석 중 오류 발생:', error);";
  html += "    return null;";
  html += "  }";
  html += "}";

  // UI 업데이트 함수
  html += "function updateUI(analysisResult) {";
  html += "  const messageArea = document.getElementById('message-area');";
  html += "  const statusTextElement = document.getElementById('status-text');";
  html += "  messageArea.innerHTML = '';";
  html += "  let hasDanger = false;";

  html += "  if (analysisResult) {";
  html += "    dangerScenarios.forEach(scenario => {";
  html += "      const state = parseInt(analysisResult[scenario.id], 10);";
  html += "      dangerStates[scenario.id] = state;";
  html += "      if (state === 1) {";
  html += "        hasDanger = true;";
  html += "        messageArea.innerHTML += '<div class=\"warning-message\">' + scenario.warning + '</div>';";
  html += "      }";
  html += "    });";
  html += "  }";

  html += "  statusTextElement.style.display = hasDanger ? 'none' : 'block';";
  html += "}";

  // 분석 시작 함수
  html += "function startAnalysis() {";
  html += "  const analyzeButton = document.getElementById('analyze-button');";
  html += "  if (isAnalyzing) return;";

  html += "  isAnalyzing = true;";
  html += "  analyzeButton.disabled = true;";
  html += "  analyzeButton.textContent = '분석 중...';";

  html += "  const base64Image = captureFrame();";
  html += "  if (base64Image) {";
  html += "    analyzeImage(base64Image).then(result => {";
  html += "      if (result) {";
  html += "        updateUI(result);";
  html += "      }";
  html += "      isAnalyzing = false;";
  html += "      analyzeButton.disabled = false;";
  html += "      analyzeButton.textContent = '분석 시작';";
  html += "    }).catch(error => {";
  html += "      console.error('분석 중 오류:', error);";
  html += "      isAnalyzing = false;";
  html += "      analyzeButton.disabled = false;";
  html += "      analyzeButton.textContent = '분석 시작';";
  html += "    });";
  html += "  } else {";
  html += "    isAnalyzing = false;";
  html += "    analyzeButton.disabled = false;";
  html += "    analyzeButton.textContent = '분석 시작';";
  html += "  }";
  html += "}";

  // 이벤트 리스너 연결
  html += "document.getElementById('analyze-button').addEventListener('click', startAnalysis);";

  html += "</script>";
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
 response += "Connection: keep-alive\r\n"; // 연결 유지
 response += "Access-Control-Allow-Origin: *\r\n"; // CORS 허용 (필요한 경우)
 response += "\r\n";
 client.print(response); // 헤더 전송


 while (client.connected()) { // 클라이언트가 연결되어 있는 동안 반복
   camera_fb_t * fb = NULL;
   fb = esp_camera_fb_get(); // 프레임 버퍼 가져오기
   if (!fb) {
     Serial.println("Camera capture failed");
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


   // 짧은 딜레이로 CPU 사용량 조절 가능 (필요시)
   // delay(10);
 }
 Serial.println("Client disconnected from stream.");
}


void handleNotFound() {
 String message = "File Not Found\n\n";
 message += "URI: ";
 // 어떤 서버의 요청인지 구분하여 uri 가져오기
 if (server.client().localPort() == 80) {
     message += server.uri();
 } else if (streamServer.client().localPort() == 81) {
     message += streamServer.uri();
 }
 message += "\nMethod: ";
 message += (server.method() == HTTP_GET) ? "GET" : "POST";
 message += "\nArguments: ";
 if (server.client().localPort() == 80) {
 message += server.args();
 message += "\n";
 for (uint8_t i = 0; i < server.args(); i++) {
   message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
 }
 } else if (streamServer.client().localPort() == 81) {
      // 스트림 서버는 일반적으로 인자가 없음
      message += "None\n";
 }


 // 현재 요청이 어떤 서버로 왔는지에 따라 server 또는 streamServer 사용
 if (server.client().localPort() == 80) {
     server.send(404, "text/plain", message);
 } else if (streamServer.client().localPort() == 81) {
     streamServer.send(404, "text/plain", message);
 }
}
