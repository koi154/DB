#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <winsock2.h>
#include <mysql/jdbc.h>
#pragma comment(lib, "ws2_32.lib")
const std::string DB_HOST = "tcp://127.0.0.1:3306";
std::string DB_USER = "root";
std::string DB_PASS;
std::string DB_NAME;
const int PORT = 9000;
std::map<SOCKET, int> socketToUserId;
std::string ConvertToUTF8(const std::string& ansiStr) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, NULL, 0);
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, &wstr[0], wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string utf8str(ulen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8str[0], ulen, NULL, NULL);
    return utf8str;
}
void handleClient(SOCKET clientSocket) {
    std::vector<char> buffer(1024);
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        std::unique_ptr<sql::Connection> conn(driver->connect(DB_HOST, DB_USER, DB_PASS));
        conn->setClientOption("characterEncoding", "utf8mb4");
        std::unique_ptr<sql::Statement> charsetStmt(conn->createStatement());
        charsetStmt->execute("SET NAMES utf8mb4");
        conn->setSchema(DB_NAME);
        while (true) {
            int recvLen = recv(clientSocket, buffer.data(), buffer.size(), 0);
            if (recvLen <= 0) {
                break;
            }
            std::string msg(buffer.data(), recvLen);
            std::cout << "[RECV] " << msg << std::endl;
            if (msg.rfind("REGISTER:", 0) == 0) {
                size_t pos1 = msg.find(":", 9);
                size_t pos2 = msg.find(":", pos1 + 1);
                std::string username = msg.substr(9, pos1 - 9);
                std::string password = msg.substr(pos1 + 1, pos2 - pos1 - 1);
                std::unique_ptr<sql::PreparedStatement> checkStmt(
                    conn->prepareStatement("SELECT * FROM users WHERE username = ?")
                );
                checkStmt->setString(1, username);
                std::unique_ptr<sql::ResultSet> checkRes(checkStmt->executeQuery());
                if (checkRes->next()) {
                    std::string response = "ID already exists";
                    send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                }
                else {
                    std::unique_ptr<sql::PreparedStatement> insertStmt(
                        conn->prepareStatement("INSERT INTO users (username, password, status) VALUES (?, ?, 'active')")
                    );
                    insertStmt->setString(1, username);
                    insertStmt->setString(2, password);
                    insertStmt->execute();
                    std::string response = "Register Success";
                    send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                }
            }
            else if (msg.rfind("LOGIN:", 0) == 0) {
                size_t pos1 = msg.find(":", 6);
                size_t pos2 = msg.find(":", pos1 + 1);
                std::string username = msg.substr(6, pos1 - 6);
                std::string password = msg.substr(pos1 + 1, pos2 - pos1 - 1);
                std::unique_ptr<sql::PreparedStatement> pstmt(
                    conn->prepareStatement("SELECT user_id FROM users WHERE username = ? AND password = ?")
                );
                pstmt->setString(1, username);
                pstmt->setString(2, password);
                std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
                if (res->next()) {
                    int userId = res->getInt("user_id");
                    socketToUserId[clientSocket] = userId;
                    // 세션 기록 INSERT
                    std::unique_ptr<sql::PreparedStatement> sessionStmt(
                        conn->prepareStatement("INSERT INTO user_sessions (user_id, ip_address) VALUES (?, ?)"));
                    sessionStmt->setInt(1, userId);
                    sessionStmt->setString(2, "127.0.0.1"); // 실제 IP 파싱은 생략
                    sessionStmt->execute();
                    std::string result = "Login Success";
                    send(clientSocket, result.c_str(), static_cast<int>(result.length()), 0);
                }
                else {
                    std::string result = "Login Failed";
                    send(clientSocket, result.c_str(), static_cast<int>(result.length()), 0);
                }
            }
            else if (msg.rfind("CHAT:", 0) == 0) {
                std::string content = msg.substr(5);
                std::string echoMsg = "Echo: " + content;
                send(clientSocket, echoMsg.c_str(), static_cast<int>(echoMsg.length()), 0);
                if (socketToUserId.count(clientSocket)) {
                    int senderId = socketToUserId[clientSocket];
                    std::unique_ptr<sql::PreparedStatement> insertMsg(
                        conn->prepareStatement("INSERT INTO message_log (sender_id, content) VALUES (?, ?)")
                    );
                    insertMsg->setInt(1, senderId);
                    insertMsg->setString(2, content);
                    insertMsg->execute();
                }
            }
            else if (msg == "exit") {
                if (socketToUserId.count(clientSocket)) {
                    int userId = socketToUserId[clientSocket];
                    std::unique_ptr<sql::PreparedStatement> logoutStmt(
                        conn->prepareStatement("UPDATE user_sessions SET logout_time = NOW() WHERE user_id = ? ORDER BY session_id DESC LIMIT 1")
                    );
                    logoutStmt->setInt(1, userId);
                    logoutStmt->execute();
                    socketToUserId.erase(clientSocket);
                }
                std::string bye = "Goodbye!";
                send(clientSocket, bye.c_str(), static_cast<int>(bye.length()), 0);
                break;
            }
            else if (msg == "/history") {
                if (socketToUserId.count(clientSocket)) {
                    int userId = socketToUserId[clientSocket];
                    std::unique_ptr<sql::PreparedStatement> historyStmt(
                        conn->prepareStatement("SELECT content, sent_at FROM message_log WHERE sender_id = ? ORDER BY sent_at ASC")
                    );
                    historyStmt->setInt(1, userId);
                    std::unique_ptr<sql::ResultSet> historyRes(historyStmt->executeQuery());
                    while (historyRes->next()) {
                        std::string logLine = "[" + historyRes->getString("sent_at") + "] " + historyRes->getString("content");
                        send(clientSocket, logLine.c_str(), static_cast<int>(logLine.length()), 0);
                        Sleep(50); // 너무 빠르게 여러 줄 보내면 클라이언트에서 깨질 수 있으니 살짝 지연
                    }
                    std::string end = "[End of History]";
                    send(clientSocket, end.c_str(), static_cast<int>(end.length()), 0);
                }
                else {
                    std::string err = "Not logged in";
                    send(clientSocket, err.c_str(), static_cast<int>(err.length()), 0);
                }
            }
            else {
                std::string error = "Unknown Command";
                send(clientSocket, error.c_str(), static_cast<int>(error.length()), 0);
            }
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "MySQL Error: " << e.what() << std::endl;
        std::string err = "DB Error";
        send(clientSocket, err.c_str(), static_cast<int>(err.length()), 0);
    }
    closesocket(clientSocket);
}
int main() {
    std::cout << "DB Name: " << DB_NAME;
    std::getline(std::cin, DB_NAME);
    std::cout << "Password: " << DB_PASS;
    std::getline(std::cin, DB_PASS);
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket failed" << std::endl;
        WSACleanup();
        return 1;
    }
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);
    std::cout << "[Server start] Listen in " << PORT << "..." << std::endl;
    std::cout << "[DB Connect]" << std::endl;
    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            std::cout << "[Client connect]" << std::endl;
            std::thread t(handleClient, clientSocket);
            t.detach();
        }
    }
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}