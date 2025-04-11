#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <thread>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
std::string ConvertToUTF8(const std::string& ansiStr) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, NULL, 0);
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, &wstr[0], wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string utf8str(ulen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8str[0], ulen, NULL, NULL);
    return utf8str;
}
void receiveMessages(SOCKET sock) {
    char buffer[1024];
    while (true) {
        int recvLen = recv(sock, buffer, sizeof(buffer) - 1, 0); // �� ���� ���� -1
        if (recvLen <= 0) break;
        buffer[recvLen] = '\0'; // ���� ���ڿ� ���� ����
        std::string msg(buffer);
        std::cout << "\n[Server] " << msg << std::endl;
        std::cout << ">> "; // ������Ʈ �����
        std::cout.flush();   // ��� ����
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
            std::cout << "== �ʱ�ȭ�� ==\n1. Register\n2. Login\n3. Exit\n>> ";
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
                    std::cout << "[Server] " << buf << std::endl;
                }
            }
            else if (choice == 2) {
                std::string id, pw;
                std::cout << "== �α��� ==\nUsername: "; std::getline(std::cin, id);
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
            std::cout << "== �α��� �� �޴� ==\n1. Chat\n2. Logout\n3. Exit\n>> ";
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            if (choice == 1) {
                std::cout << "[ä�� ���] '/exit' �Է� �� ä�� ����\n";
                std::cout << "[ä�� ���] '/history' �Է� �� ��ȭ ��� ����\n";
                while (true) {
                    std::cout << ">> ";
                    std::string content;
                    std::getline(std::cin, content);
                    if (content == "/exit") break;
                    else if (content == "/history") {
                        std::string msg = "/history";
                        send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                        // �����κ��� ���� �� �ޱ�
                        char buf[1024];
                        int len;
                        // �����丮 ��� - �� �� �������� ���� ��Ȳ�� ���� ����
                        while ((len = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
                            buf[len] = '\0';
                            std::string line(buf);
                            // ������ ������ ���� ���� �� Ư�� Ű���带 �־ ���� ���� (���û���)
                            if (line == "[End of History]") break;
                            std::cout << "[Server] " << line << std::endl;
                            // ���� �ӵ� ����
                            Sleep(50);
                        }
                        std::cout << "=== ��ȭ ��� �� ===\n";
                        continue;
                    }
                    std::string msg = "CHAT:" + ConvertToUTF8(content);
                    send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
                    char buf[1024];
                    int len = recv(sock, buf, sizeof(buf) - 1, 0);
                    if (len > 0) {
                        buf[len] = '\0';
                        std::cout << "[Server] " << buf << std::endl;
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
