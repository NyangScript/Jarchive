import cv2  
import os
import time
import google.generativeai as genai  
import numpy as np  
from PIL import Image, ImageDraw, ImageFont  
import io
import json

class SafetyMonitor:
    def __init__(self):
        self.danger_scenarios = [
            {
                'id': 'left_hand',
                'name': '왼손 들기',
                'warning': '테스트: 왼손을 들었습니다!',
                'description': '사람이 왼손을 들고 있는지'
            },
            {
                'id': 'right_hand',
                'name': '오른손 들기',
                'warning': '테스트: 오른손을 들었습니다!',
                'description': '사람이 오른손을 들고 있는지'
            },
            {
                'id': 'standing',
                'name': '서있음',
                'warning': '테스트: 서있는 자세입니다!',
                'description': '사람이 서있는 자세인지'
            },
            {
                'id': 'sitting',
                'name': '앉아있음',
                'warning': '테스트: 앉아있는 자세입니다!',
                'description': '사람이 앉아있는 자세인지'
            },
            {
                'id': 'walking',
                'name': '걷기',
                'warning': '테스트: 걷는 동작입니다!',
                'description': '사람이 걷는 동작을 하고 있는지'
            }
        ]
        
        self.API_KEY = "AIzaSyCK5WE5NxHlCHQGd5agdkl5dZs0KLgFIXM"
        genai.configure(api_key=self.API_KEY)
        self.model = genai.GenerativeModel('gemini-2.0-flash')

        self.active_warnings = []
        self.font_path = '/Users/brody/Downloads/Mediapipe/Project/NanumGothic.ttf'
        self.font_size = 36  # 기본 폰트 크기 증가
        self.title_font_size = 44  # 제목 폰트 크기 증가
        self.warning_font_size = 28  # 경고 메시지 폰트 크기 증가
        self.is_analyzing = False
        self.last_analysis_time = 0
        self.analysis_interval = 1.0

        self.danger_states = {scenario['id']: 0 for scenario in self.danger_scenarios}

    def analyze_frame(self, frame):
        current_time = time.time()
        if self.is_analyzing or (current_time - self.last_analysis_time) < self.analysis_interval:
            return

        try:
            self.is_analyzing = True
            self.last_analysis_time = current_time

            image_pil = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
            img_byte_arr = io.BytesIO()
            image_pil.save(img_byte_arr, format='JPEG')
            img_byte_arr = img_byte_arr.getvalue()

            prompt = "다음 상황들을 분석하여 각각 0(아니오) 또는 1(예)로 JSON 형식으로 응답해주세요. 반드시 코드블록(```) 없이 순수 JSON만 반환하세요:\n"
            for scenario in self.danger_scenarios:
                prompt += f"- {scenario['description']}\n"
            prompt += "\n응답 형식: {\"left_hand\": 0, \"right_hand\": 0, \"standing\": 0, \"sitting\": 0, \"walking\": 0}"

            response = self.model.generate_content([
                prompt,
                {"mime_type": "image/jpeg", "data": img_byte_arr}
            ])

            if response and response.text:
                try:
                    text = response.text.strip()
                    # 코드블록 제거
                    if text.startswith("```"):
                        text = text.split("```")[1] if "```" in text else text
                    text = text.replace("json", "", 1).strip() if text.startswith("json") else text
                    result = json.loads(text)
                    for scenario in self.danger_scenarios:
                        self.danger_states[scenario['id']] = int(result.get(scenario['id'], 0))
                except json.JSONDecodeError:
                    print("JSON 파싱 오류:", response.text)
                    
        except Exception as e:
            print(f"분석 중 오류 발생: {e}")
        finally:
            self.is_analyzing = False
        print(result)

    def put_korean_text(self, img, text, position, color=(0, 0, 255), is_title=False, is_warning=False, max_width=None):
        img_pil = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
        draw = ImageDraw.Draw(img_pil)
        font = ImageFont.truetype(self.font_path, self.title_font_size if is_title else (self.warning_font_size if is_warning else self.font_size))

        if max_width is not None:
            words = text.split(' ')
            lines = []
            current_line = ''
            for word in words:
                # Check width with the next word
                test_line = current_line + (' ' if current_line else '') + word
                bbox = draw.textbbox((0, 0), test_line, font=font)
                text_width = bbox[2] - bbox[0] # Use textbbox for accurate width

                if text_width <= max_width:
                    current_line = test_line
                else:
                    # If adding the word exceeds max_width, add the current line
                    # (before adding the word) and start a new line with the word.
                    if current_line: # Only add if not empty
                         lines.append(current_line)
                    current_line = word
            if current_line: # Add the last line
                lines.append(current_line)

            y_offset = position[1]
            line_height = font.getbbox('가')[3] - font.getbbox('가')[1] + 5 # Approximate line height + padding
            for line in lines:
                 draw.text((position[0], y_offset), line, font=font, fill=color)
                 y_offset += line_height
            return cv2.cvtColor(np.array(img_pil), cv2.COLOR_RGB2BGR)
        else:
            # Draw single line if max_width is not specified
            draw.text(position, text, font=font, fill=color)
        return cv2.cvtColor(np.array(img_pil), cv2.COLOR_RGB2BGR)

def main():
    # ESP32-S3-CAM 스트리밍 URL 사용
    stream_url = "http://192.168.2.93:81/stream"
    max_retries = 3
    retry_count = 0
    
    while retry_count < max_retries:
        print(f"카메라 스트림 연결 시도 중... (시도 {retry_count + 1}/{max_retries})")
        cap = cv2.VideoCapture(stream_url)
        if cap.isOpened():
            print("카메라 스트림 연결 성공!")
            break
        else:
            print("카메라 스트림 연결 실패. 재시도 중...")
            retry_count += 1
            time.sleep(2)  # 2초 대기 후 재시도
    
    if not cap.isOpened():
        print("카메라 스트림을 초기화할 수 없습니다.")
        print("다음 사항을 확인해주세요:")
        print("1. ESP32-CAM이 켜져 있는지")
        print("2. ESP32-CAM과 컴퓨터가 같은 네트워크에 연결되어 있는지")
        print("3. ESP32-CAM의 IP 주소가 올바른지")
        return
    #cap = cv2.VideoCapture(0)
    # 스트리밍 해상도 설정
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1600)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1200)

    monitor = SafetyMonitor()

    print("실시간 안전 모니터링을 시작합니다.")
    print("'a' 키를 눌러 분석을 시작합니다.")
    print("'q' 키를 눌러 종료합니다.")

    # 오른쪽 패널의 x 좌표와 너비 설정
    panel_x_start = 1200
    panel_width = 1600 - panel_x_start  # 400 pixels
    text_margin = 20  # 텍스트 좌우 여백
    text_area_width = panel_width - text_margin * 2  # 약 360 픽셀

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("프레임을 가져올 수 없습니다.")
                break

            key = cv2.waitKey(1) & 0xFF
            if key == ord('a'):
                print("\n분석을 시작합니다...")
                monitor.analyze_frame(frame)

            # 배경 사각형 (오른쪽에 정보 표시)
            overlay = frame.copy()
            cv2.rectangle(overlay, (panel_x_start, 0), (1600, 1200), (0, 0, 0), -1)
            cv2.addWeighted(overlay, 0.7, frame, 0.3, 0, frame)

            # 제목
            y_position = 40
            # 제목은 줄바꿈 필요 없으므로 max_width None
            frame = monitor.put_korean_text(frame, "테스트 모니터링", (panel_x_start + text_margin, y_position), (255, 255, 255), is_title=True)
            y_position += 60  # 세로 간격 조정

            # 현재 시간 표시
            current_time = time.strftime("%H:%M:%S")
            # 시간도 줄바꿈 필요 없으므로 max_width None
            frame = monitor.put_korean_text(frame, f"시간: {current_time}", (panel_x_start + text_margin, y_position), (200, 200, 200))
            y_position += 50  # 세로 간격 조정

            # 구분선
            cv2.line(frame, (panel_x_start + text_margin, y_position), (panel_x_start + panel_width - text_margin, y_position), (100, 100, 100), 2)
            y_position += 30  # 세로 간격 조정

            # 감지된 상태 표시
            has_danger = False
            # 각 시나리오별 상태 표시
            for scenario in monitor.danger_scenarios:
                if monitor.danger_states.get(scenario['id'], 0) == 1:
                    # 경고 메시지는 줄바꿈 적용
                    frame = monitor.put_korean_text(frame, scenario['warning'], (panel_x_start + text_margin, y_position), (0, 0, 255), is_warning=True, max_width=text_area_width)

                    # 경고 메시지가 여러 줄일 경우 다음 텍스트 위치 계산
                    dummy_img_pil = Image.new('RGB', (1, 1)) # Dummy image for text size calculation
                    dummy_draw = ImageDraw.Draw(dummy_img_pil)
                    font = ImageFont.truetype(monitor.font_path, monitor.warning_font_size)
                    words = scenario['warning'].split(' ')
                    current_line = ''
                    line_count = 0
                    for word in words:
                        test_line = current_line + (' ' if current_line else '') + word
                        bbox = dummy_draw.textbbox((0,0), test_line, font=font)
                        text_width = bbox[2] - bbox[0]
                        if text_width <= text_area_width:
                             current_line = test_line
                        else:
                            if current_line:
                                line_count += 1
                            current_line = word
                    if current_line:
                         line_count += 1

                    line_height = font.getbbox('가')[3] - font.getbbox('가')[1] + 5
                    y_position += line_height * line_count + 5 # 각 경고 메시지 블록 사이 간격 추가

                    has_danger = True

            # 정상 상태 표시
            if not has_danger:
                frame = monitor.put_korean_text(frame, "정상 상태", (panel_x_start + text_margin, y_position), (0, 255, 0))

            # 하단 안내 메시지
            y_position = 1100
            frame = monitor.put_korean_text(frame, "\'a\': 분석 시작", (panel_x_start + text_margin, y_position), (200, 200, 200))
            y_position += 40
            frame = monitor.put_korean_text(frame, "\'q\': 종료", (panel_x_start + text_margin, y_position), (200, 200, 200))

            cv2.imshow('테스트 모니터링 시스템', frame)

            if key == ord('q'):
                break

    finally:
        cap.release()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
