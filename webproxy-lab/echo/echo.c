#include "csapp.h"



int main(int argc, char const *argv[]) {
    /* 서버와 연결된 클라이언트 소켓 파일 디스크립터 */
    int clientfd;
    /* 접속할 서버 주소, 포트, 입출력에 사용할 한 줄 버퍼 */
    char *host, *port, buf[MAXLINE];
    /* Robust I/O가 내부 버퍼 상태를 관리하기 위한 구조체 */
    rio_t rio;

    /* 클라이언트는 서버 호스트와 포트 번호를 인자로 받아야 함 */
    if(argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    /* 명령행 인자에서 접속할 서버 호스트와 포트를 꺼냄 */
    host = argv[1];
    port = argv[2];

    /* 서버에 TCP 연결을 시도하고, 성공하면 연결된 소켓 fd를 받음 */
    clientfd = Open_clientfd(host, port);
    
    /* 연결된 소켓을 Robust I/O 버퍼와 연결 */
    Rio_readinitb(&rio, clientfd);

    /* 표준 입력에서 한 줄씩 읽어 서버로 전송 */
    while(Fgets(buf, MAXLINE, stdin) != NULL) {
        /* 사용자가 입력한 한 줄을 서버에 보냄 */
        Rio_writen(clientfd, buf, strlen(buf));
        /* 현재 코드는 서버 응답이 아니라 방금 입력한 내용을 출력함 */
        Fputs(buf, stderr);
    }
    /* 입력이 끝나면 서버와의 연결을 닫음 */
    Close(clientfd);
    /* 프로그램 정상 종료 */
    exit(0);
    return 0;
}


































#if 0

/* IPv4 주소를 저장하는 구조체 */
struct in_addr {
	uint32_t s_addr;
};

/* IPv4 소켓 주소 정보를 저장하는 구조체 */
// sin -> socket internet
// family -> 주소 종류 표시 -> IPv4, IPv6, Unix domain socket
// Unix domain socket은 로컬에서 프로세스끼리 통신하는 소켓
// sin_zero는 패딩 역할 -> 왜 패딩이 필요하지? -> 실제로 connect, bind함수는 sockaddr* 를 매개변수로 받음.
// 이때 sockaddr_in을 매개변수로 넣고 싶을 때 크기를 맞춰야 하기 때문에 패딩 사용 -> 16byte로 맞춤
// sockaddr_in은 IPv4 전용
struct sockaddr_in {
	uint16_t sin_family;
	uint16_t sin_port;
	struct in_addr sin_addr;
	unsigned char sin_zero[8];
};

/* 다양한 종류의 소켓 주소를 일반적으로 다루기 위한 구조체 */
// 왜 sa_port가 없지? -> 소켓통신에서 포트는 인터넷 통신에서만 사용
// -> Unix domain socket의 경로가 클경우는? -> Unix domain socket은 sockaddr_un* 를 매개변수로 받음 -> 주소 크기 108byte
struct sockaddr {
	uint16_t sa_family;
	char sa_data[14];
};

/* getaddrinfo가 주소 후보 목록을 만들 때 사용하는 구조체 */
struct addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	char *ai_canonname;
	size_t ai_addrlen;
	struct sockaddr *ai_addr;
	struct addrinfo *ai_next;
};


int open_clientfd(char *hostname, char *port) {
    /* 서버에 연결할 클라이언트 소켓 파일 디스크립터 */
    int clientfd;
    /* 주소 조건, 주소 목록, 현재 순회 중인 주소 포인터 */
    struct addrinfo hints, *listp, *p;

    /* hints 구조체를 초기화한 뒤 필요한 조건만 설정 */
    memset(&hints, 0, sizeof(struct addrinfo));
    /* TCP 소켓 주소만 찾도록 설정 */
    hints.ai_socktype = SOCK_STREAM;
    /* port 인자를 숫자 문자열로 해석하도록 설정 */
    hints.ai_flags = AI_NUMERICSERV;
    /* 현재 시스템에서 사용 가능한 주소 체계만 반환하도록 설정 */
    hints.ai_flags |= AI_ADDRCONFIG;

    Getaddrinfo(hostname, port, &hints, &listp);

    for(p = listp; p; p = p->ai_next) {
        if((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;

        if(connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break;
        Close(clientfd);
    }

    Freeaddrinfo(listp);
    if(!p) return -1;
    else return clientfd;
}

int main(int argc, char const *argv[]) {
    /* 주소 정보 목록을 순회할 포인터와 getaddrinfo에 넘길 조건 구조체 */
    struct addrinfo *p, *listp, hints;
    /* 변환된 IP 주소 문자열을 저장할 버퍼 */
    char buf[MAXLINE];
    /* rc는 함수 반환값, flags는 getnameinfo 동작 옵션 */
    int rc, flags;

    /* 프로그램 인자가 도메인 이름 하나인지 확인 */
    if(argc != 2) {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }


    /* hints 구조체를 0으로 초기화해서 필요한 조건만 직접 설정 */
    memset(&hints, 0, sizeof(struct addrinfo));
    /* IPv4 주소만 찾도록 설정 */
    hints.ai_family = AF_INET;
    /* TCP 연결에 사용할 주소만 찾도록 설정 */
    hints.ai_socktype = SOCK_STREAM;
    /* argv[1] 도메인 이름에 해당하는 주소 정보 목록을 가져옴 */
    if((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }

    /* 주소를 도메인 이름이 아니라 숫자 IP 문자열로 받도록 설정 */
    flags = NI_NUMERICHOST;
    /* getaddrinfo가 돌려준 주소 후보들을 하나씩 순회 */
    for(p = listp; p; p = p->ai_next) {
        /* 소켓 주소 구조체를 사람이 읽을 수 있는 IP 문자열로 변환 */
        Getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, flags);
        /* 변환된 IP 주소 출력 */
        printf("%s\n", buf);
    }

    /* getaddrinfo가 할당한 주소 정보 목록 메모리 해제 */
    Freeaddrinfo(listp);
    
    return 0;
}


# endif

