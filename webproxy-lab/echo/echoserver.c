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
    

    /* 코드 구현 부분: listen 소켓 생성, accept, echo 호출, close 흐름을 작성 */
    Open_listenfd(argv[1]);
    



    return 0;
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
