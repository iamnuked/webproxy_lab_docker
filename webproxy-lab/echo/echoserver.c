#include "csapp.h"

/* 클라이언트와 연결된 소켓에서 데이터를 읽고 그대로 다시 보내는 함수 */
void echo(int connfd);

int main(int argc, char const *argv[]) {
    /* listenfd는 접속 요청을 기다리는 소켓, connfd는 실제 클라이언트와 연결된 소켓 */
    int listenfd, connfd;
    /* accept가 클라이언트 주소 구조체의 크기를 읽고 갱신할 때 사용 */
    socklen_t clientlen;
    /* IPv4/IPv6 등 여러 주소 체계를 담을 수 있는 충분히 큰 주소 구조체 */
    struct sockaddr_storage clientaddr;
    /* 접속한 클라이언트의 호스트 이름과 포트 문자열을 저장할 버퍼 */
    char client_hostname[MAXLINE], client_port[MAXLINE];

    /* 서버는 포트 번호 하나를 인자로 받아야 함 */
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    

    /* argv[1] 포트에서 클라이언트 접속을 기다리는 listen 소켓을 생성 */
    listenfd = Open_listenfd(argv[1]);

    /* 서버는 종료하지 않고 계속 다음 클라이언트 접속을 기다림 */
    while(1) {
        /* accept에 넘기기 전에 주소 구조체 크기를 알려줌 */
        clientlen = sizeof(struct sockaddr_storage);

        /* 클라이언트가 접속할 때까지 기다렸다가, 연결 전용 소켓 connfd를 반환 */
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* accept가 채운 clientaddr을 사람이 읽을 수 있는 host/port 문자열로 변환 */
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        /* 접속한 클라이언트 정보를 서버 터미널에 출력 */
        printf("connected to (%s, %s)\n", client_hostname, client_port);
    
        /* 연결된 클라이언트와 echo 통신을 수행 */
        echo(connfd);
        /* 클라이언트와의 연결이 끝났으므로 연결 소켓을 닫음 */
        Close(connfd);
    }

    /* while(1) 때문에 보통 여기까지 오지 않지만, 정상 종료 코드를 명시 */
    exit(0);
}

void echo(int connfd) {
    /* n은 읽은 바이트 수 */
    size_t n;
    /* 클라이언트가 보낸 한 줄을 저장할 버퍼 */
    char buf[MAXLINE];
    /* Robust I/O가 내부 버퍼 상태를 관리하기 위한 구조체 */
    rio_t rio;

    /* connfd를 Robust I/O 버퍼와 연결 */
    Rio_readinitb(&rio, connfd);
    /* 클라이언트가 연결을 끊어 0을 반환할 때까지 한 줄씩 읽음 */
    while((n = rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        /* 서버 터미널에 받은 바이트 수를 출력 */
        printf("server received %d bytes\n", (int)n);
        /* 읽은 내용을 그대로 클라이언트에게 다시 보냄 */
        Rio_writen(connfd, buf, n);
    }
}
