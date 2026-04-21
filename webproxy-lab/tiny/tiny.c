/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

/* 클라이언트 요청 하나를 처리하는 핵심 함수 */
void doit(int fd);
/* 요청 헤더의 나머지 줄들을 읽어서 버리는 함수 */
void read_requesthdrs(rio_t *rp);
/* URI가 정적 콘텐츠인지 동적 콘텐츠인지 판별하고 경로를 파싱 */
int parse_uri(char *uri, char *filename, char *cgiargs);
/* 정적 파일을 읽어 HTTP 응답으로 전송 */
void serve_static(int fd, char *filename, int filesize);
/* 파일 확장자에 따라 Content-Type 문자열 결정 */
void get_filetype(char *filename, char *filetype);
/* CGI 프로그램을 실행해 동적 콘텐츠 응답 생성 */
void serve_dynamic(int fd, char *filename, char *cgiargs);
/* 에러 응답 페이지와 HTTP 헤더를 생성해 클라이언트에 전송 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,	char *longmsg);

int main(int argc, char **argv) {
	/* listenfd는 접속 대기용, connfd는 실제 클라이언트와 연결된 소켓 */
	int listenfd, connfd;
	/* 접속한 클라이언트의 호스트 이름과 포트 문자열 저장 */
	char hostname[MAXLINE], port[MAXLINE];
	/* accept가 주소 구조체 크기를 읽고 갱신할 때 사용 */
	socklen_t clientlen;
	/* IPv4/IPv6를 모두 담을 수 있는 범용 주소 구조체 */
	struct sockaddr_storage clientaddr;

	/* Check command line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	/* argv[1] 포트에서 접속을 기다리는 listen 소켓 생성 */
	listenfd = Open_listenfd(argv[1]);
	/* Iterative 서버이므로 한 번에 한 클라이언트씩 계속 처리 */
	while (1) {
		/* accept 전에 주소 구조체 크기를 초기화 */
		clientlen = sizeof(clientaddr);
		/* 클라이언트 접속을 수락하고 연결 전용 소켓 반환 */
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
		/* 접속한 클라이언트 주소를 사람이 읽을 수 있는 문자열로 변환 */
		Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		/* 요청 하나를 처리 */
		doit(connfd);  // line:netp:tiny:doit
		/* 응답이 끝난 연결 소켓 닫기 */
		Close(connfd); // line:netp:tiny:close
	}
}

void doit(int fd) {
	/* is_static은 정적/동적 콘텐츠 구분 플래그 */
	int is_static;
	/* stat 결과로 파일의 존재 여부와 권한을 확인 */
	struct stat sbuf;
	/* 요청 라인과 파싱 결과를 저장할 버퍼들 */
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];

	/* Robust I/O 상태를 관리할 구조체 */
	rio_t rio;

	/* 연결된 소켓 fd를 rio 버퍼와 연결 */
	Rio_readinitb(&rio, fd);
	/* 요청의 첫 줄(Request line)을 읽음 */
	Rio_readlineb(&rio, buf, MAXLINE);
	printf("Request headers:\n");
	printf("%s", buf);
	/* "GET /index.html HTTP/1.0" 같은 요청 라인을 메서드, URI, 버전으로 분리 */
	sscanf(buf, "%s %s %s", method, uri, version);
	/* Tiny는 GET 메서드만 처리 */
	if(strcasecmp(method, "GET")) {
		clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
		return;
	}
	/* 요청 헤더의 나머지 줄을 읽어 소비 */
	read_requesthdrs(&rio);

	/* URI를 파싱해 파일 경로와 CGI 인자를 만들고 정적/동적 여부 판별 */
	is_static = parse_uri(uri, filename, cgiargs);
	/* 요청한 파일이나 프로그램이 실제로 존재하는지 확인 */
	if(stat(filename, &sbuf) < 0) {
		clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
		return;
	}

	if(is_static) {
		/* 정적 콘텐츠는 일반 파일이고 읽기 권한이 있어야 함 */
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
			return;
		}
		/* 파일 내용을 직접 응답으로 전송 */
		serve_static(fd, filename, sbuf.st_size);
	}
	else {
		/* 동적 콘텐츠는 실행 가능한 일반 파일이어야 함 */
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
			return;
		}
		/* CGI 프로그램을 실행해 동적 응답 생성 */
		serve_dynamic(fd, filename, cgiargs);
	}
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
	/* buf는 응답 헤더, body는 에러 HTML 본문 */
	char buf[MAXLINE], body[MAXBUF];
	/* 간단한 HTML 에러 페이지 구성 */
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff""\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	/* 상태줄과 응답 헤더 전송 */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	/* 마지막으로 HTML 본문 전송 */
	Rio_writen(fd, buf, strlen(body));
}

void read_requesthdrs(rio_t *rp) {
	/* 빈 줄(\r\n)이 나올 때까지 요청 헤더를 한 줄씩 읽음 */
	char buf[MAXLINE];
	Rio_readlineb(rp, buf, MAXLINE);
	while(strcmp(buf, "\r\n")) {
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
	char* ptr;
	/* "cgi-bin"이 없으면 정적 콘텐츠 요청으로 처리 */
	if(!strstr(uri, "cgi-bin")) {
		/* 정적 콘텐츠는 CGI 인자가 없고 현재 디렉토리 기준 경로를 만듦 */
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		/* URI가 / 로 끝나면 기본 파일 home.html을 붙임 */
		if(uri[strlen(uri)-1] == '/') {
			strcat(filename, "home.html");
		}
		return 1;
	}
	else {
		/* 동적 콘텐츠는 ? 뒤를 CGI 인자로 분리 */
		ptr = index(uri, '?');
		if(ptr) {
			strcpy(cgiargs, ptr+1);
			*ptr = '\0';
		}
		else {
			strcpy(cgiargs, "");
		}
		strcpy(filename, ".");
		strcat(filename, uri);
		return 0;
	}
}

void serve_static(int fd, char *filename, int filesize) {
	/* srcfd는 파일 디스크립터, srcp는 mmap으로 매핑된 파일 내용 */
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];

	/* 파일 확장자에 맞는 Content-Type 결정 */
	get_filetype(filename, filetype);
	/* 상태줄과 헤더 작성 후 클라이언트에 전송 */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));
	printf("Response headers:\n");
	printf("%s", buf);

	/* 파일을 메모리에 매핑해 한 번에 응답 본문으로 전송 */
	srcfd = Open(filename,O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype) {
	/* 파일 확장자를 보고 MIME 타입 문자열을 결정 */
	if(strstr(filename, ".html")) {
		strcpy(filetype, "text/html");
	}
	else if(strstr(filename, ".gif")) {
		strcpy(filetype, "image/gif");
	}
	else if(strstr(filename, ".png")) {
		strcpy(filetype, "image/png");
	}
	else if(strstr(filename, ".jpg")) {
		strcpy(filetype, "image/jpg");
	}
	else {
		strcpy(filetype, "text/plain");
	}
	
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
	/* CGI 프로그램 실행에는 argv가 필요하므로 빈 인자 목록 사용 */
	char buf[MAXLINE], *emptylist[] = {NULL};

	/* CGI 프로그램이 자체적으로 본문을 출력하므로 최소한의 응답 헤더만 먼저 전송 */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server\r\n");
	Rio_writen(fd, buf, strlen(buf));

	/* 자식 프로세스가 CGI 프로그램을 실행하고 부모는 끝날 때까지 기다림 */
	if(Fork() == 0) {
		/* CGI 프로그램이 getenv("QUERY_STRING")으로 인자를 읽을 수 있게 설정 */
		setenv("QUERY_STRING", cgiargs, 1);
		/* CGI 프로그램의 표준 출력을 클라이언트 소켓으로 연결 */
		Dup2(fd, STDOUT_FILENO);
		/* 자식 프로세스를 CGI 프로그램으로 교체 */
		Execve(filename, emptylist, environ);
	}
	Wait(NULL);
}

