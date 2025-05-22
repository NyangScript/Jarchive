#ifndef PAGE1_H
#define PAGE1_H

#include <WebServer.h>

extern WebServer server;

// 이상행동 기록 페이지 핸들러 함수
void handleAnomalousRecord() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>";
  html += "<title>이상행동 기록</title>"; // 페이지 타이틀
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Arial', sans-serif; background-color: #e0f7fa; color: #333; line-height: 1.6; padding-bottom: 60px; }"; // 바텀 네비게이션 높이만큼 패딩 추가
  html += ".header { background-color: #00BCD4; color: white; padding: 15px 0; text-align: center; box-shadow: 0 2px 4px rgba(0,0,0,0.1); position: relative; }";
  html += ".header h1 { font-size: 20px; font-weight: bold; }";
  html += ".back-button { position: absolute; left: 15px; top: 50%; transform: translateY(-50%); font-size: 24px; color: white; text-decoration: none; }"; // 뒤로가기 버튼 스타일
  html += ".container { padding: 10px; }";
  html += ".card { background-color: #ffffff; margin: 10px 0; padding: 15px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";

  /* Record Summary */
  html += ".record-summary { display: flex; align-items: center; margin-bottom: 15px; }";
  html += ".record-summary .icon { width: 40px; height: 40px; background-color: #00BCD4; border-radius: 50%; display: flex; justify-content: center; align-items: center; color: white; font-size: 20px; margin-right: 10px; }";
  html += ".record-summary .text h2 { font-size: 18px; margin-bottom: 5px; }";
  html += ".record-summary .text p { font-size: 14px; color: #555; }";
  html += ".record-summary .count { font-size: 24px; color: #00BCD4; font-weight: bold; margin-left: auto; }";

  /* Detailed Counts */
  html += ".detailed-counts { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 10px; }"; // 그리드 레이아웃
  html += ".count-item { background-color: #e0f2f7; padding: 10px; border-radius: 4px; text-align: center; }";
  html += ".count-item p { font-size: 12px; color: #555; margin-bottom: 3px; }";
  html += ".count-item h3 { font-size: 18px; color: #00796b; font-weight: bold; }";

  /* Graph Placeholder */
  html += ".graph-placeholder { width: 100%; height: 200px; background-color: #f5f5f5; display: flex; justify-content: center; align-items: center; color: #aaa; font-size: 14px; border-radius: 4px; margin-top: 15px; }";

  /* Bottom Navigation */
  html += ".bottom-nav { display: flex; justify-content: space-around; padding: 10px 0; background-color: #ffffff; border-top: 1px solid #eee; position: fixed; width: 100%; bottom: 0; left: 0; box-shadow: 0 -2px 5px rgba(0,0,0,0.1); z-index: 1000; }";
  html += ".nav-item { text-align: center; flex-grow: 1; color: #777; font-size: 10px; text-decoration: none; }";
  html += ".nav-item .icon { width: 24px; height: 24px; font-size: 20px; margin: 0 auto 5px; }";
  html += ".nav-item.active .icon { color: #00BCD4; }";
  html += ".nav-item.active { color: #00BCD4; }"; // 활성 상태 텍스트 색상

  html += "</style>";
  html += "</head><body>";

  /* Header */
  html += "<div class='header'>";
  html += "<a href='/' class='back-button'>&larr;</a>"; // 메인 화면으로 돌아가는 링크 (Flutter webview에서 처리될 수 있음)
  html += "<h1>이상행동 기록</h1>";
  html += "</div>";

  /* Main Content */
  html += "<div class='container'>";

  /* Record Summary Card */
  html += "<div class='card'>";
  html += "<div class='record-summary'>";
  html += "<div class='icon'>?</div>"; // 아이콘
  html += "<div class='text'><h2>감지된 이상행동 기록</h2><p>총 횟수</p></div>";
  html += "<div class='count'>9<span style='font-size:16px;'>회</span></div>"; // 임시 데이터
  html += "</div>"; // end record-summary
  html += "</div>"; // end card

  /* Detailed Counts Card */
  html += "<div class='card'>";
  html += "<div class='detailed-counts'>";
  html += "<div class='count-item'><p>주방에서</p><p>감지됨</p><h3>5<span style='font-size:12px;'>번</span></h3></div>"; // 임시 데이터
  html += "<div class='count-item'><p>거실에서</p><p>감지됨</p><h3>3<span style='font-size:12px;'>번</span></h3></div>"; // 임시 데이터
  html += "<div class='count-item'><p>복도에서</p><p>감지됨</p><h3>1<span style='font-size:12px;'>번</span></h3></div>"; // 임시 데이터
  html += "<div class='count-item'><p>다용도실에서</p><p>감지됨</p><h3>0<span style='font-size:12px;'>번</span></h3></div>"; // 임시 데이터
  html += "</div>"; // end detailed-counts
  html += "</div>"; // end card

  /* Graph Card */
  html += "<div class='card'>";
  html += "<h2>추이 그래프 (예정)</h2>"; // 그래프 타이틀
  html += "<div class='graph-placeholder'>그래프 영역</div>"; // 그래프 플레이스홀더
  html += "</div>"; // end card

  html += "</div>"; // end container

  /* Bottom Navigation */
  // 홈 버튼을 활성 상태로 표시
  html += "<div class='bottom-nav'>";
  html += "<a href='/' class='nav-item'><div class='icon'>🏠</div>홈</a>"; // 홈으로 이동 링크
  html += "<a href='/anomalous_record' class='nav-item active'><div class='icon'>?</div>이상행동 기록</a>"; // 현재 페이지 링크
  html += "<a href='/dangerous_record' class='nav-item'><div class='icon'>▲</div>위험행동 기록</a>"; // 위험행동 기록으로 이동 링크
  html += "<a href='/report' class='nav-item'><div class='icon'>🔔</div>신고</a>"; // 신고 페이지 (예정)
  html += "</div>"; // end bottom-nav


  html += "</body></html>";

  server.send(200, "text/html", html);
}

#endif