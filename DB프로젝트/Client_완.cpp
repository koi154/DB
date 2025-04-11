#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <thread>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
std::string UTF8ToCP949(const std::string& utf8Str) {
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    std::wstring wideStr(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wideStr[0], wideLen);

    int ansiLen = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string ansiStr(ansiLen, '\0');
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &ansiStr[0], ansiLen, NULL, NULL);

    return ansiStr;
}
std::string ConvertToUTF8(const std::string& ansiStr) {
    int wideLen = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, NULL, 0);
    std::wstring wideStr(wideLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, &wideStr[0], wideLen);

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string utf8Str(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &utf8Str[0], utf8Len, NULL, NULL);

    return utf8Str;
}
void receiveMessages(SOCKET sock) {
    char buffer[1024];
    while (true) {
        int recvLen = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (recvLen <= 0) break;
        buffer[recvLen] = '\0';
        std::string msg(buffer);

        // ✅ 여기에서 UTF-8 → CP949 변환
        std::cout << "\n[Server] " << UTF8ToCP949(msg) << std::endl;
        std::cout << ">> ";
        std::cout.flush();
    }
}
int main() {
    const char* serverIP = "127.0.0.1";
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    bool loggedIn = false;
    std::string username;
    while (true) {
        if (!loggedIn) {
            std::cout << "== 초기화면 ==\n1. Register\n2. Login\n3. Exit\n>> ";
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            if (choice == 1) {
                std::string id, pw;
                std::cout << "Username: "; std::getline(std::cin, id);
                std::cout << "Password: "; std::getline(std::cin, pw);
                std::string msg = "REGISTER:" + id + ":" + pw + ":";
                send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                char buf[1024];
                int len = recv(sock, buf, sizeof(buf) - 1, 0);
                if (len > 0) {
                    buf[len] = '\0';
                    std::string msg(buf);
                    std::cout << "[Server] " << UTF8ToCP949(msg) << std::endl;
                }
            }
            else if (choice == 2) {
                std::string id, pw;
                std::cout << "== 로그인 ==\nUsername: "; std::getline(std::cin, id);
                std::cout << "Password: "; std::getline(std::cin, pw);
                std::string msg = "LOGIN:" + id + ":" + pw + ":";
                send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                char buf[1024];
                int len = recv(sock, buf, sizeof(buf) - 1, 0);
                if (len > 0) {
                    buf[len] = '\0';
                    std::string reply(buf);
                    reply.erase(reply.find_last_not_of(" \n\r\t") + 1);
                    std::cout << "[Server] " << reply << std::endl;
                    if (reply == "Login Success") {
                        username = id;
                        loggedIn = true;
                    }
                }
            }
            else if (choice == 3) {
                std::string msg = "exit";
                send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                break;
            }
        }
        else {
            std::cout << "== 로그인 후 메뉴 ==\n1. Chat\n2. Logout\n3. Exit\n>> ";
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            if (choice == 1) {
                std::cout << "[채팅 모드] '/exit' 입력 시 채팅 종료\n";
                std::cout << "[채팅 모드] '/history' 입력 시 대화 기록 보기\n";
                while (true) {
                    std::cout << ">> ";
                    std::string content;
                    std::getline(std::cin, content);
                    if (content == "/exit") break;
                    else if (content == "/history") {
                        std::string msg = "/history";
                        send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                        // 서버로부터 여러 줄 받기
                        char buf[1024];
                        int len;
                        // 히스토리 출력 - 몇 번 받을지는 서버 상황에 따라 조정
                        while ((len = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
                            buf[len] = '\0';
                            std::string line(buf);
                            // 서버가 마지막 응답 보낼 때 특정 키워드를 넣어서 종료 가능 (선택사항)
                            if (line == "[End of History]") break;
                            std::cout << "[Server] " << line << std::endl;
                            // 수신 속도 조절
                            Sleep(50);
                        }
                        std::cout << "=== 대화 기록 끝 ===\n";
                        continue;
                    }
                    std::string msg = "CHAT:" + ConvertToUTF8(content);
                    send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                    char buf[1024];
                    int len = recv(sock, buf, sizeof(buf) - 1, 0);
                    if (len > 0) {
                        buf[len] = '\0';
                        std::string msg(buf);
                        std::cout << "[Server] " << UTF8ToCP949(msg) << std::endl;
                    }
                }
            }
            else if (choice == 2) {
                loggedIn = false;
                std::cout << "Logged out.\n";
            }
            else if (choice == 3) {
                std::string msg = "exit";
                send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                break;
            }
        }
    }
    closesocket(sock);
    WSACleanup();
    return 0;
}
