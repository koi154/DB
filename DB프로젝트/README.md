# 🧵 Console Chat System (C++ / WinSock / MySQL)

한국어 인코딩 문제를 완벽히 해결한 윈도우 콘솔 기반 채팅 시스템입니다.  
WinSock을 기반으로 TCP 통신을 수행하며, 사용자는 **회원가입 / 로그인 / 채팅 / 히스토리 조회** 기능을 이용할 수 있습니다.

> ✅ 한글 입력/출력 및 MySQL 저장 모두 **UTF-8 ↔ CP949 변환 처리 완료**

---

## 🧩 구성

- **Client_완.cpp**: 클라이언트 코드
- **Server_완.cpp**: 서버 코드
- **MySQL Schema**: 사용자, 세션, 메시지 로그를 저장하는 DB 스키마

---

## ✨ 주요 기능

- 📥 회원가입 / 로그인
- 💬 실시간 채팅
- 🕓 채팅 히스토리 `/history` 명령으로 확인
- 📚 메시지 DB 저장 (`utf8mb4` 적용)
- 🧠 한글 인코딩 대응 (`ConvertToUTF8`, `UTF8ToCP949` 함수 사용)

---

## 🛠 실행 방법

### 1. MySQL DB 세팅

```sql
CREATE DATABASE chat_system CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE chat_system;

CREATE TABLE users (
  user_id INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(50) NOT NULL UNIQUE,
  password VARCHAR(100) NOT NULL,
  status VARCHAR(20) DEFAULT 'active'
);

CREATE TABLE user_sessions (
  session_id INT AUTO_INCREMENT PRIMARY KEY,
  user_id INT NOT NULL,
  login_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  logout_time TIMESTAMP NULL,
  ip_address VARCHAR(50),
  FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
);

CREATE TABLE message_log (
  message_id INT AUTO_INCREMENT PRIMARY KEY,
  sender_id INT NOT NULL,
  content TEXT NOT NULL,
  sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (sender_id) REFERENCES users(user_id) ON DELETE CASCADE
);
```

### 2. 서버 실행

```bash
g++ Server_완.cpp -o server -lws2_32 -lmysqlcppconn
./server
```

▶ 실행 후 DB 이름과 비밀번호 입력

### 3. 클라이언트 실행

```bash
g++ Client_완.cpp -o client -lws2_32
./client
```

---

## 🎨 인코딩 처리 핵심

| 방향 | 처리 |
|------|------|
| 입력 (콘솔 → 서버) | `ConvertToUTF8()` → UTF-8로 변환 |
| 출력 (서버 → 콘솔) | `UTF8ToCP949()` → CP949로 변환 |
| DB 저장 | `utf8mb4`로 저장 |

> 💬 서버는 메시지를 그대로 UTF-8로 클라이언트에 echo함

---

## 💡 기타 참고

- Windows 한글 콘솔(CP949)을 위한 로케일 설정이 필요합니다:
```cpp
setlocale(LC_ALL, "korean");
```

- 클라이언트에서 한글이 깨질 경우 `UTF8ToCP949()`가 빠진 출력 부분이 있는지 점검하세요.

---

## 🤝 기여

이 프로젝트는 학습 및 실습 목적으로 제작되었습니다.  
코드 개선, 기능 확장 제안은 언제든지 환영합니다!
