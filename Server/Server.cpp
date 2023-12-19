#include "Server.h"


// 계정인증 통신 시작 부 (class Server)
// 소멸 시 Thread Pool 삭제 -> 서버 종료
Server::~Server() {
	threadPool->join();
	delete threadPool;
}

void Server::ac_start_wrapper(int i) {
        ac_start();
}

Server::Server(MysqlPool* pool) : mysqlPool(pool)  {
	threadPool = new ThreadPool(5); // 5개의 ThreadPool 생성
	int i, port = 8081;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		std::perror("socket");
		exit(1);
	}

	sockaddr_in server_address{};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	server_address.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
		std::perror("bind");
		exit(1);
	}

	listen(server_socket, MAX_CONNECTIONS);
	FD_ZERO(&master_set);
    FD_SET(server_socket, &master_set);
	max_sd = server_socket;
	for (i = 0; i < 5; i++) {
        threadPool->enqueue(std::bind(&Server::ac_start_wrapper, this, i));
    }
}


void Server::ac_start() {

    while (true) {
        fd_set copy_set = master_set; // 복사본 생성
        char buffer[500];
        std::string total_buffer, req_buf;
        ssize_t n;

        if (select(max_sd + 1, &copy_set, nullptr, nullptr, nullptr) < 0) { // socket 
            std::perror("select");
            exit(1);
        }

        for (int fd = 0; fd <= max_sd; fd++) {
            if (!FD_ISSET(fd, &copy_set)) continue;

            sockaddr_in client_address{};
            socklen_t client_len = sizeof(client_address);
            // Client 연결
            int client_socket = accept(fd, (struct sockaddr*)&client_address, &client_len); 
            if (client_socket < 0) {
				std::perror("accept");
				break;
			}
            // Client 요청 종류 확인
            n = read(client_socket, buffer, 499);
            if (n < 0){
                std::perror("read");
                std::cout << "Client request error\n";
                exit(1);
            }
            
            req_buf = buffer;
            std::cout << "Client request : " << req_buf <<std::endl; 

            if (req_buf == "account"){ // 계정 관련 요청인 경우
                Account_Session* account_session = new Account_Session(client_socket);
                account_session->start();
                delete account_session;
            } else if (req_buf == "pc_info"){ // PC 상태정보 전송인 경우
                PC_Info_Session* pc_info_session = new PC_Info_Session(client_socket, mysqlPool);
                pc_info_session->start();
                delete pc_info_session; // 삭제는 session 내부에서
            }
        }
    }
}

