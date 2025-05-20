import cv2  # noqa
import os
import time
import google.generativeai as genai  # noqa
import numpy as np  # noqa
from PIL import Image, ImageDraw, ImageFont  # noqa
import io
import json

class SafetyMonitor:
    def __init__(self):
        self.danger_scenarios = [
            {
                'id': 'gas_stove',
                'name': '가스불',
                'warning': '경고: 가스불이 켜져 있습니다!',
                'description': '가스불이 켜져 있는지'
            },
            {
                'id': 'electric_heater',
                'name': '전기히터',
                'warning': '경고: 전기히터가 켜져 있습니다!',
                'description': '전기히터나 전기레인지가 켜져 있는지'
            },
            {
                'id': 'sink_water',
                'name': '싱크대 물',
                'warning': '경고: 싱크대에 물이 흐르고 있습니다!',
                'description': '싱크대의 수도꼭지에서 물줄기가 내려오고 있는지'
            },
            {
                'id': 'bathtub_water',
                'name': '욕조 물',
                'warning': '경고: 욕조에 물이 가득 차 있습니다!',
                'description': '욕조에 물이 가득 차 있는지'
            },
            {
                'id': 'fridge_door',
                'name': '냉장고 문',
                'warning': '경고: 냉장고 문이 열려 있습니다!',
                'description': '냉장고 문이 열려 있는지'
            },
            {
                'id': 'door_window',
                'name': '문/창문',
                'warning': '경고: 문이나 창문이 제대로 잠겨있지 않습니다!',
                'description': '현관문이나 창문이 제대로 잠겨있는지'
            },
            {
                'id': 'gas_valve',
                'name': '가스 밸브',
                'warning': '경고: 가스 밸브가 잠겨있지 않습니다!',
                'description': '가스 밸브가 잠겨있지 않은지'
            },
            {
                'id': 'air_conditioner',
                'name': '냉난방기기',
                'warning': '경고: 냉난방기기가 오래 켜져 있습니다!',
                'description': '냉난방기기가 오래 켜져 있는지'
            },
            {
                'id': 'slippery_floor',
                'name': '미끄러운 바닥',
                'warning': '경고: 바닥이 미끄럽습니다!',
                'description': '욕실이나 부엌 바닥이 미끄러운지'
            },
            {
                'id': 'medicine',
                'name': '약 복용',
                'warning': '경고: 약을 과다복용했거나 복용을 잊었습니다!',
                'description': '약을 과다복용했거나 복용을 잊었는지'
            },
            {
                'id': 'stranger_door',
                'name': '낯선 사람',
                'warning': '경고: 낯선 사람이 방문했습니다!',
                'description': '낯선 사람이 방문했는지'
            },
            {
                'id': 'metal_microwave',
                'name': '금속 전자레인지',
                'warning': '경고: 금속 그릇을 전자레인지에 넣었습니다!',
                'description': '금속 그릇을 전자레인지에 넣었는지'
            },
            {
                'id': 'wet_electric',
                'name': '젖은 전기',
                'warning': '경고: 전자제품이 젖었거나 젖은 손으로 다루고 있습니다!',
                'description': '전자제품이 젖었거나 젖은 손으로 다루고 있는지'
            },
            {
                'id': 'medicine_time',
                'name': '약 시간',
                'warning': '알림: 약 복용 시간입니다!',
                'description': '약 복용 시간이 되었는지'
            },
            {
                'id': 'appliance_off',
                'name': '가전 끄기',
                'warning': '알림: 가스불이나 전자레인지를 끄세요!',
                'description': '가스불이나 전자레인지를 끄는 것을 잊었는지'
            },
            {
                'id': 'food_storage',
                'name': '식품 보관',
                'warning': '알림: 냉장보관이 필요한 식품이 있습니다!',
                'description': '냉장보관이 필요한 식품이 상온에 있는지'
            },
            {
                'id': 'laundry',
                'name': '세탁물',
                'warning': '알림: 세탁물을 꺼내야 합니다!',
                'description': '세탁기가 작동을 마쳤는지'
            },
            {
                'id': 'doorbell',
                'name': '초인종',
                'warning': '알림: 초인종이 울렸습니다!',
                'description': '초인종이 울렸는지'
            },
            {
                'id': 'pet_food',
                'name': '반려동물 사료',
                'warning': '알림: 반려동물 사료 시간입니다!',
                'description': '반려동물 사료를 주는 시간인지'
            },
            {
                'id': 'bathroom_light',
                'name': '화장실 불',
                'warning': '알림: 화장실 불을 끄세요!',
                'description': '화장실 불이 켜져 있는지'
            },
            {
                'id': 'repetitive_behavior',
                'name': '반복 행동',
                'warning': '알림: 같은 행동을 반복하고 있습니다!',
                'description': '같은 행동을 반복하고 있는지'
            },
            {
                'id': 'lost_item',
                'name': '물건 잃어버림',
                'warning': '알림: 물건을 어디에 뒀는지 잊었습니다!',
                'description': '물건을 어디에 뒀는지 잊었는지'
            },
            {
                'id': 'direction_confusion',
                'name': '방향 혼동',
                'warning': '알림: 집안에서 길을 잃었습니다!',
                'description': '집안에서 길을 잃었는지'
            },
            {
                'id': 'time_confusion',
                'name': '시간 혼동',
                'warning': '알림: 시간 인식이 혼동됩니다!',
                'description': '시간 인식이 혼동되는지'
            },
            {
                'id': 'seasonal_clothes',
                'name': '계절 옷',
                'warning': '알림: 계절에 맞지 않는 옷을 입고 있습니다!',
                'description': '계절에 맞지 않는 옷을 입고 있는지'
            },
            {
                'id': 'cooking_confusion',
                'name': '요리 혼동',
                'warning': '알림: 요리 재료나 순서가 혼동됩니다!',
                'description': '요리 재료나 순서가 혼동되는지'
            },
            {
                'id': 'appliance_on',
                'name': '가전 켜짐',
                'warning': '알림: TV나 조명이 켜져 있습니다!',
                'description': 'TV나 조명이 켜져 있는지'
            },
            {
                'id': 'appliance_usage',
                'name': '가전 사용',
                'warning': '알림: 가전제품 사용법이 혼동됩니다!',
                'description': '가전제품 사용법이 혼동되는지'
            }
        ]
        
        self.API_KEY = "AIzaSyCK5WE5NxHlCHQGd5agdkl5dZs0KLgFIXM"
        genai.configure(api_key=self.API_KEY)
        self.model = genai.GenerativeModel('gemini-2.0-flash')

        self.active_warnings = []
        self.font_path = '/Users/brody/Downloads/Mediapipe/Project/NanumGothic.ttf'
        self.font_size = 30
        self.is_analyzing = False
        self.last_analysis_time = 0
        self.analysis_interval = 1.0

        self.danger_states = {
            'gas_stove': 0,
            'electric_heater': 0,
            'sink_water': 0,
            'bathtub_water': 0,
            'fridge_door': 0,
            'door_window': 0,
            'gas_valve': 0,
            'air_conditioner': 0,
            'slippery_floor': 0,
            'medicine': 0,
            'stranger_door': 0,
            'metal_microwave': 0,
            'wet_electric': 0,
            'medicine_time': 0,
            'appliance_off': 0,
            'food_storage': 0,
            'laundry': 0,
            'doorbell': 0,
            'pet_food': 0,
            'bathroom_light': 0,
            'repetitive_behavior': 0,
            'lost_item': 0,
            'direction_confusion': 0,
            'time_confusion': 0,
            'seasonal_clothes': 0,
            'cooking_confusion': 0,
            'appliance_on': 0,
            'appliance_usage': 0
        }

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
            prompt += "\n응답 형식: {\"gas_stove\": 0, \"electric_heater\": 0, \"sink_water\": 0, \"bathtub_water\": 0, \"fridge_door\": 0, \"door_window\": 0, \"gas_valve\": 0, \"air_conditioner\": 0, \"slippery_floor\": 0, \"medicine\": 0, \"stranger_door\": 0, \"metal_microwave\": 0, \"wet_electric\": 0, \"medicine_time\": 0, \"appliance_off\": 0, \"food_storage\": 0, \"laundry\": 0, \"doorbell\": 0, \"pet_food\": 0, \"bathroom_light\": 0, \"repetitive_behavior\": 0, \"lost_item\": 0, \"direction_confusion\": 0, \"time_confusion\": 0, \"seasonal_clothes\": 0, \"cooking_confusion\": 0, \"appliance_on\": 0, \"appliance_usage\": 0}"

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

    def put_korean_text(self, img, text, position, color=(0, 0, 255)):
        img_pil = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
        draw = ImageDraw.Draw(img_pil)
        font = ImageFont.truetype(self.font_path, self.font_size)
        draw.text(position, text, font=font, fill=color)
        return cv2.cvtColor(np.array(img_pil), cv2.COLOR_RGB2BGR)

def main():
    # ESP32-S3-CAM 스트리밍 URL 사용
    cap = cv2.VideoCapture("http://192.168.2.101:81/stream")
    if not cap.isOpened():
        print("카메라 스트림을 초기화할 수 없습니다.")
        return

    # 스트리밍 해상도 설정
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)  # ESP32-CAM 기본 해상도
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    monitor = SafetyMonitor()

    print("실시간 안전 모니터링을 시작합니다.")
    print("'a' 키를 눌러 분석을 시작합니다.")
    print("'q' 키를 눌러 종료합니다.")

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

            # 배경 사각형
            overlay = frame.copy()
            cv2.rectangle(overlay, (0, 0), (500, 400), (0, 0, 0), -1)
            cv2.addWeighted(overlay, 0.7, frame, 0.3, 0, frame)

            # 제목
            y_position = 30
            frame = monitor.put_korean_text(frame, "안전 모니터링 시스템", (10, y_position), (255, 255, 255))
            y_position += 40

            # 경고 메시지 출력
            has_danger = False
            for scenario in monitor.danger_scenarios:
                if monitor.danger_states.get(scenario['id'], 0) == 1:
                    frame = monitor.put_korean_text(frame, scenario['warning'], (10, y_position), (0, 0, 255))
                    y_position += 40
                    has_danger = True

            if not has_danger:
                frame = monitor.put_korean_text(frame, "정상 상태", (10, y_position), (0, 255, 0))

            cv2.imshow('안전 모니터링 시스템', frame)

            if key == ord('q'):
                break

    finally:
        cap.release()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
