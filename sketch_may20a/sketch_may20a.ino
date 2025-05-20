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
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n"; // Boundary corrected
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
  String html = "<!DOCTYPE html><html><head><title>ESP32-CAM 안전 모니터링</title>";
  html += "<meta charset='utf-8'>"; // 한글 표시를 위한 메타 태그 추가
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; display: flex; }"; // 레이아웃 조정을 위한 flexbox 사용
  html += "#video-container { flex: 1; max-width: 640px; position: relative; }"; // 영상 컨테이너 (최대 너비 설정)
  html += "#video-stream { width: 100%; height: auto; display: block; }"; // 영상 스트림 크기 조정
  html += "#info-panel { width: 200px; background-color: #000; color: #fff; padding: 10px; box-sizing: border-box; display: flex; flex-direction: column; }"; // 정보 패널 (오른쪽, 세로 정렬)
  html += "h2 { margin-top: 0; font-size: 22px; }"; // 제목 스타일
  html += ".info-section { margin-bottom: 15px; }"; // 정보 섹션 간격
  html += ".info-section p { margin: 5px 0; font-size: 18px; }"; // 일반 텍스트 스타일
  html += ".warning-message { color: red; font-size: 16px; word-wrap: break-word; margin-bottom: 5px; }"; // 경고 메시지 스타일 (작은 폰트, 줄바꿈)
  html += ".normal-status { color: green; font-size: 18px; }"; // 정상 상태 메시지 스타일
  html += ".button-container { margin-top: auto; }"; // 버튼을 패널 하단으로 이동
  html += "</style>";
  html += "</head><body>";

  // 영상 영역
  html += "<div id='video-container'>";
  html += "<img id='video-stream' src='http://" + WiFi.localIP().toString() + ":81/stream'>"; // 카메라 스트림 이미지 태그
  html += "</div>"; // video-container 끝

  // 정보 패널 (오른쪽)
  html += "<div id='info-panel'>";
  html += "<h2>안전 모니터링</h2>";

  // 시간 표시 영역
  html += "<div class='info-section'><p>시간: <span id='current-time'>로딩 중...</span></p></div>"; // 시간 표시 (JavaScript 업데이트)

  html += "<hr>"; // 구분선

  // 경고/상태 메시지 출력 영역
  html += "<div id='message-area'>"; // 메시지들을 담을 컨테이너
  // JavaScript를 통해 여기에 경고 메시지 또는 정상 상태 메시지가 추가될 예정
  html += "<div class='normal-status' id='status-text'>정상 상태</div>"; // 초기 상태 메시지
  html += "</div>"; // message-area 끝

  // 버튼 컨테이너
  html += "<div class='button-container'>";
  html += "<button id='analyze-button' style='width: 100%; padding: 10px;'>분석 시작</button>"; // 분석 시작 버튼 (JavaScript 함수 호출)
  html += "</div>"; // button-container 끝

  html += "</div>"; // info-panel 끝

  html += "<script>";
  // Google Gemini API Key - 실제 사용시 보안에 매우 취약하니 주의하세요!
  html += "const GEMINI_API_KEY = \"AIzaSyCK5WE5NxHlCHQGd5agdkl5dZs0KLgFIXM\";"; // <-- 실제 API 키로 변경하세요!
  html += "const GEMINI_API_URL = \"https://generativelanguage.googleapis.com/v1beta/models/gemini-pro-vision:generateContent?key=\" + GEMINI_API_KEY;";

  // 30가지 위험/불편 상황 정의 (test.py의 danger_scenarios와 유사)
  html += "const dangerScenarios = [";
  html += "{ id: 'gas_stove', name: '가스불', warning: '경고: 가스불이 켜져 있습니다!' },";
  html += "{ id: 'electric_heater', name: '전기히터', warning: '경고: 전기히터가 켜져 있습니다!' },";
  html += "{ id: 'sink_water', name: '싱크대 물', warning: '경고: 싱크대에 물이 흐르고 있습니다!' },";
  html += "{ id: 'bathtub_water', name: '욕조 물', warning: '경고: 욕조에 물이 가득 차 있습니다!' },";
  html += "{ id: 'fridge_door', name: '냉장고 문', warning: '경고: 냉장고 문이 열려 있습니다!' },";
  html += "{ id: 'door_window', name: '문/창문', warning: '경고: 문이나 창문이 제대로 잠겨있지 않습니다!' },";
  html += "{ id: 'gas_valve', name: '가스 밸브', warning: '경고: 가스 밸브가 잠겨있지 않습니다!' },";
  html += "{ id: 'air_conditioner', name: '냉난방기기', warning: '경고: 냉난방기기가 오래 켜져 있습니다!' },";
  html += "{ id: 'slippery_floor', name: '미끄러운 바닥', warning: '경고: 바닥이 미끄럽습니다!' },";
  html += "{ id: 'medicine', name: '약 복용', warning: '경고: 약을 과다복용했거나 복용을 잊었습니다!' },";
  html += "{ id: 'stranger_door', name: '낯선 사람', warning: '경고: 낯선 사람이 방문했습니다!' },"; // 11번
  html += "{ id: 'metal_microwave', name: '금속 전자레인지', warning: '경고: 금속 그릇을 전자레인지에 넣었습니다!' },"; // 12번
  html += "{ id: 'wet_electric', name: '젖은 전기', warning: '경고: 전자제품이 젖었거나 젖은 손으로 다루고 있습니다!' },"; // 13번
  html += "{ id: 'medicine_time', name: '약 시간', warning: '알림: 약 복용 시간입니다!' },"; // 14번
   html += "{ id: 'appliance_off', name: '가전 끄기', warning: '알림: 가스불이나 전자레인지를 끄세요!' },"; // 15번
   html += "{ id: 'food_storage', name: '식품 보관', warning: '알림: 냉장보관이 필요한 식품이 있습니다!' },"; // 16번
   html += "{ id: 'laundry', name: '세탁물', warning: '알림: 세탁물을 꺼내야 합니다!' },"; // 17번
   html += "{ id: 'doorbell', name: '초인종', warning: '알림: 초인종이 울렸습니다!' },"; // 18번
   html += "{ id: 'pet_food', name: '반려동물 사료', warning: '알림: 반려동물 사료 시간입니다!' },"; // 19번
   html += "{ id: 'bathroom_light', name: '화장실 불', warning: '알림: 화장실 불을 끄세요!' },"; // 20번
   html += "{ id: 'repetitive_behavior', name: '반복 행동', warning: '알림: 같은 행동을 반복하고 있습니다!' },"; // 21번
   html += "{ id: 'lost_item', name: '물건 잃어버림', warning: '알림: 물건을 어디에 뒀는지 잊었습니다!' },"; // 22번
   html += "{ id: 'direction_confusion', name: '방향 혼동', warning: '알림: 집안에서 길을 잃었습니다!' },"; // 23번
   html += "{ id: 'time_confusion', name: '시간 혼동', warning: '알림: 시간 인식이 혼동됩니다!' },"; // 24번
   html += "{ id: 'seasonal_clothes', name: '계절 옷', warning: '알림: 계절에 맞지 않는 옷을 입고 있습니다!' },"; // 25번
   html += "{ id: 'cooking_confusion', name: '요리 혼동', warning: '알림: 요리 재료나 순서가 혼동됩니다!' },"; // 26번
   html += "{ id: 'appliance_on', name: '가전 켜짐', warning: '알림: TV나 조명이 켜져 있습니다!' },"; // 27번
   html += "{ id: 'appliance_usage', name: '가전 사용', warning: '알림: 가전제품 사용법이 혼동됩니다!' }"; // 28번
     // 나머지 상황 추가 (총 30개)
     // 예: { id: 'fall_detection', name: '넘어짐 감지', warning: '경고: 넘어짐이 감지되었습니다!' },
     // { id: 'smoke_detected', name: '연기 감지', warning: '경고: 연기가 감지되었습니다!' },
     // { id: 'unusual_activity', name: '특이 행동', warning: '경고: 특이 행동이 감지되었습니다!' }
      // 여기에 2개의 시나리오를 더 추가해야 합니다.
       html += "];";


  html += "let dangerStates = {};"; // 현재 위험 상태 저장 (JavaScript 객체)
   html += "let isAnalyzing = false;"; // 분석 중인지 여부 플래그
   html += "let analysisInterval = 3000; // 분석 간격 (밀리초), 필요에 따라 조정";
   html += "let analysisTimer = null;"; // 분석 타이머 ID
   html += "let speakingTimer = {};"; // 음성 출력 타이머 저장을 위한 객체

  // dangerStates 초기화
  html += "dangerScenarios.forEach(scenario => {";
  html += "  dangerStates[scenario.id] = 0;";
  html += "});";


  // 시간 업데이트 함수 (기존 코드 유지)
  html += "function updateTime() {";
  html += "  var now = new Date();";
  html += "  var timeString = now.toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });"; // 초까지 표시
  html += "  document.getElementById('current-time').textContent = timeString;";
  html += "}";
  html += "setInterval(updateTime, 1000);";
  html += "updateTime();";

  // 텍스트 음성 변환 함수
  html += "function speak(text, scenarioId) {";
   html += "  if ('speechSynthesis' in window) {";
   html += "    const utterance = new SpeechSynthesisUtterance(text);";
   html += "    utterance.lang = 'ko-KR'; // 한국어 설정";
   // html += "    utterance.rate = 1.0; // 음성 속도 조절 (필요시)";
   // html += "    utterance.volume = 1.0; // 음성 볼륨 조절 (필요시)";

   // 음성 종료 시 다시 재생 예약
   html += "    utterance.onend = function(event) {";
   html += "      if (dangerStates[scenarioId] === 1) {"; // 경고 상태가 유지되면
   html += "        speakingTimer[scenarioId] = setTimeout(() => speak(text, scenarioId), 5000);"; // 5초 후 다시 말하기
   html += "      }";
   html += "    };";

   // 이미 해당 시나리오에 대해 말하고 있지 않다면 시작
   html += "    if (!speakingTimer[scenarioId]) {";
    html += "      window.speechSynthesis.cancel();"; // 현재 재생 중인 음성 중단
   html += "      window.speechSynthesis.speak(utterance);";
   // 첫 음성 재생 후 반복 타이머 설정 (onend에서 처리)
   html += "      speakingTimer[scenarioId] = true;"; // 재생 중임을 표시 (타이머 객체 사용 시에는 타이머 ID 저장)
   html += "    }";

   html += "  } else {";
   html += "    console.log('Web Speech API를 지원하지 않는 브라우저입니다.');";
   html += "  }";
  html += "}";

   // 음성 재생 중지 함수
  html += "function stopSpeaking(scenarioId) {";
   html += "  if (speakingTimer[scenarioId]) {";
   html += "    clearTimeout(speakingTimer[scenarioId]);";
   html += "    delete speakingTimer[scenarioId];";
    // 특정 음성만 중지하는 기능은 Web Speech API에 직접적으로 없으므로, 모든 음성을 중지합니다.
    html += "    window.speechSynthesis.cancel();"; // 모든 음성 중단
   html += "  }";
  html += "}";


  // 이미지 캡처 및 Base64 변환 함수
  html += "function captureFrame() {";
  html += "  const video = document.getElementById('video-stream');";
  html += "  const canvas = document.getElementById('capture-canvas');";
  html += "  const context = canvas.getContext('2d');";
  html += "  canvas.width = video.videoWidth;"; // 영상의 실제 해상도 사용
  html += "  canvas.height = video.videoHeight;";
  html += "  context.drawImage(video, 0, 0, video.videoWidth, video.videoHeight);";
  html += "  return canvas.toDataURL('image/jpeg').split(',')[1]; // Base64 데이터만 반환";
  html += "}";

  // Gemini API 호출 함수
  html += "async function analyzeImage(base64Image) {";
  html += "  console.log('Gemini 분석 요청 중...');";
  html += "  try {";
  html += "    const response = await fetch(GEMINI_API_URL, {";
  html += "      method: 'POST',";
  html += "      headers: {";
  html += "        'Content-Type': 'application/json'";
  html += "      },";
  html += "      body: JSON.stringify({";
  html += "        contents: [";
  html += "          {";
  // Gemini에게 질문할 프롬프트 생성
  html += "            parts: [";
  html += "              { text: \"다음 상황들을 분석하여 각각 0(아니오) 또는 1(예)로 JSON 형식으로 응답해주세요. 반드시 코드블록(```) 없이 순수 JSON만 반환하세요:\\n\" +";
   html += "                      dangerScenarios.map(s => `- ${s.description}`).join('\\n') +";
   html += "                      \"\\n응답 형식: \" + JSON.stringify(Object.fromEntries(dangerScenarios.map(s => [s.id, 0])))}"; // 동적으로 응답 형식 생성
  html += "              ,";
  html += "              { inline_data: { mime_type: 'image/jpeg', data: base64Image } }";
  html += "            ]";
  html += "          }";
  html += "        ]";
  html += "      })";
  html += "    });";

  html += "    if (!response.ok) {";
  html += "      const errorText = await response.text();";
  html += "      throw new Error(`API 요청 실패: ${response.status} ${response.statusText} - ${errorText}`);";
  html += "    }";

  html += "    const data = await response.json();";
  html += "    console.log('Gemini 응답:', data);";

  // 응답에서 텍스트 부분 파싱
  html += "    if (data.candidates && data.candidates[0] && data.candidates[0].content && data.candidates[0].content.parts && data.candidates[0].content.parts[0]) {";
  html += "      let textResponse = data.candidates[0].content.parts[0].text;";
   // 코드블록 제거 로직 추가
   html += "      textResponse = textResponse.trim();";
   html += "      if (textResponse.startsWith('```')) {";
   html += "        textResponse = textResponse.split('```')[1] || textResponse;";
   html += "      }";
   html += "      textResponse = textResponse.replace('json', '').trim();"; // 'json' 마크다운 제거

  html += "      try {";
  html += "        const result = JSON.parse(textResponse);";
  html += "        return result;"; // 파싱된 JSON 객체 반환
  html += "      } catch (e) {";
  html += "        console.error('JSON 파싱 오류:', e, '원본 텍스트:', textResponse);";
  html += "        return null;";
  html += "      }";
  html += "    } else {";
  html += "      console.error('Gemini 응답 형식 오류:', data);";
  html += "      return null;";
  html += "    }";

  html += "  } catch (error) {";
  html += "    console.error('Gemini 분석 중 오류 발생:', error);";
  html += "    return null;";
  html += "  }";
  html += "}";

  // UI 업데이트 함수
  html += "function updateUI(analysisResult) {";
  html += "  const messageArea = document.getElementById('message-area');";
  html += "  const statusTextElement = document.getElementById('status-text');"; // 정상 상태 텍스트 요소를 가져옴
  html += "  messageArea.innerHTML = ''; // 기존 메시지 지우기"; // messageArea만 초기화
  html += "  let hasDanger = false;";

  html += "  if (analysisResult) {";
  html += "    dangerScenarios.forEach(scenario => {";
  html += "      const state = parseInt(analysisResult[scenario.id], 10);"; // 결과를 정수로 변환
  html += "      dangerStates[scenario.id] = state;"; // 상태 업데이트

  html += "      if (state === 1) {";
  html += "        hasDanger = true;";
  // 경고 메시지 표시
  html += "        messageArea.innerHTML += '<div class=\"warning-message\">' + scenario.warning + '</div>';";
  // 경고 메시지 음성 출력 (반복 재생)
  html += "        speak(scenario.warning, scenario.id);";
  html += "      } else {";
  // 해당 시나리오의 음성 재생 중지
   html += "        stopSpeaking(scenario.id);";
   html += "      }";
   html += "    });";
   html += "  }";

  // 정상 상태 메시지 표시/숨김
  html += "  if (!hasDanger) {";
   // 모든 경고 음성 중지 (hasDanger가 false일 때만 호출)
   html += "    Object.keys(speakingTimer).forEach(scenarioId => stopSpeaking(scenarioId));";
   // messageArea는 경고 메시지로만 사용, 정상 상태는 statusTextElement로 관리
   html += "    statusTextElement.style.display = 'block'; // 정상 상태 표시";
   html += "  } else {";
   html += "    statusTextElement.style.display = 'none'; // 정상 상태 숨김";
   html += "  }";
  html += "}";

   // 분석 시작 함수 (분석 중지 함수 제거)
   html += "function startAnalysis() {\n";
   html += "  if (isAnalyzing) {\n";
   html += "    console.log('이미 분석 중입니다.');\n";
   html += "  }\n";
   html += "  isAnalyzing = true;\n";
    html += "  document.getElementById('analyze-button').disabled = true;\n"; // 버튼 비활성화
   html += "  console.log('분석 시작...');\n";

   html += "  async function performAnalysis() {\n";
   html += "    console.log('프레임 캡처 및 분석 실행');\n"; // 로그 추가
   html += "    const base64Image = captureFrame();\n";
   html += "    if (base64Image) {\n";
   html += "      const analysisResult = await analyzeImage(base64Image);\n";
   html += "      if (analysisResult) {\n";
   html += "        updateUI(analysisResult);\n";
   html += "      } else {\n";
        html += "        console.error('분석 결과 없음');\n"; // 분석 결과 없을 때 로그
   html += "      }\n";
   html += "    } else {\n";
    html += "      console.error('프레임 캡처 실패');\n"; // 프레임 캡처 실패 시 로그
   html += "    }\n";
    html += "    isAnalyzing = false;\n"; // 분석 완료 후 플래그 해제
    html += "    document.getElementById('analyze-button').disabled = false;\n"; // 버튼 다시 활성화
   html += "  }\n";

   html += "  performAnalysis(); // 버튼 누르면 1회 실행\n";
   html += "}\n";

   // 이벤트 리스너 연결
   html += "document.getElementById('analyze-button').addEventListener('click', startAnalysis);\n"; // 분석 시작 버튼 이벤트 유지

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

