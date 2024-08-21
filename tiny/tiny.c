/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr,
                        &clientlen);  // line:netp:tiny:accept
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                    0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);   // line:netp:tiny:doit
        Close(connfd);  // line:netp:tiny:close
    }
}

void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // 요청라인과 헤더를 읽는 부분
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); // 예: GET, /, HTTP/1.1
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) { // GET 메소드가 아닌 요청이 들어왔을 경우
        clienterror(fd, method, "501", "Not implemented",
                    "Tiny does not implement this method"); // 에러 메시지를 출력하고
        return; // 함수 종료(이후 연결 종료하게 됨)
    }
    read_requesthdrs(&rio); // GET 메소드가 맞는 경우 헤더를 읽어들인다.

    // URI 파싱
    is_static = parse_uri(uri, filename, cgiargs); // uri에서 파일이름과 인자들을 추출하여 filename, cgiargs 배열에 담는다.
    if (stat(filename, &sbuf) < 0) { // 파일을 열 수 없으면
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file"); // 에러 메시지를 출력하고
        return; // 함수 종료
    }

    //
    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, method);
    }
    else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs, method);
    }
}

/**
 * @brief  클라이언트에게 오류 응답을 보냄
 * 
 * 입력된 인자들로 HTTP 응답을 생성하고 클라이언트에 전송하는 함수
 * 
 * @param  fd 클라이언트 파일 식별자
 * @param  cause 에러 발생 부분
 * @param  errnum HTTP 에러 코드
 * @param  shortmsg shortmessage
 * @param  longmsg longmessage
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    // HTTP 응답 생성, body에 저장
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); // \r\n : HTTP프로토콜, 윈도우에서 사용하는 행 변경 문자
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);
    
    // HTTP 응답 출력
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 버퍼에 응답 메시지 저장
    Rio_writen(fd, buf, strlen(buf)); // 클라이언트에 전달
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // HTTP 응답은 본체에서 컨텐츠의 크기를 표시해야 한다.
    Rio_writen(fd, buf, strlen(buf));
    // HTTP body 출력
    Rio_writen(fd, body, strlen(body));
}

/**
 * @brief  요청 헤더를 읽는 함수
 * 
 * 요청 헤더를 종료 빈 줄이 나올 때까지 출력한다.
 * 
 * @note   요청 헤더를 읽고 표시만 할 뿐 추가 작업은 없음
 */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/**
 * @brief  URI를 파싱
 * 
 * URI를 입력받아 파일명과 인자로 처리해주는 함수
 * 
 * @param  uri uri 문자열
 * @param  filename filename 반환
 * @param  cgiargs cgi 인자 반환
 * 
 * @return dynamic: 0, static: 1
 * 
 * @note   정적 컨텐츠의 기본 파일 이름은 home.html
 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    // 정적 컨텐츠
    if (!strstr(uri, "cgi-bin")) { // uri에 cgi-bin이라는 문자열이 없으면
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri); // strcat: 문자열 결합
        // 예: "/res/img.jpg" -> "./res/img.jpg"
        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "home.html");
            // 예: "/" -> "./home.html"
        return 1;
    }
    // 동적 컨텐츠
    else {
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1); // 인자(? 뒷부분)가 있으면 cgiargs에 추가
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

/**
 * @brief  정적 컨텐츠 제공
 * 
 * HTML, 텍스트, GIF, PNG, JPEG를 지원한다.
 * 
 * @param  fd
 * @param  filename filename
 * @param  filesize
 * 
 * @note   정적 컨텐츠의 기본 파일 이름은 home.html
 */
void serve_static(int fd, char *filename, int filesize, char *method) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    // 응답 헤더
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 응답 헤더의 첫 줄은 version status-code status-message
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers2:\n");
    printf("%s", buf);

    // 응답 body
    if (strcasecmp(method, "HEAD")) {
        srcfd = Open(filename, O_RDONLY, 0); // filename의 파일 식별자를 가져옴
        // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일 식별자에 해당하는 파일을 메모리에 복사
        srcp = (char *)malloc(filesize);
        Rio_readn(srcfd, srcp, filesize);
        Close(srcfd);
        Rio_writen(fd, srcp, filesize); // 파일내용을 fd로 쓰기
        // Munmap(srcp, filesize);
        free(srcp);
    }
}

/**
 * @brief  url 헤더의 파일 타입을 반환
 * 
 * filename의 확장자를 통해 filetype에 파일 타입 저장
 * 
 */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
    char buf[MAXLINE], *emptylist[] = { NULL };

    // 응답 헤더
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { // 분기 후 자식 노드인 경우에만 아래를 실행
        setenv("QUERY_STIRNG", cgiargs, 1);
        setenv("REQUEST_METHOD", method, 1);
        Dup2(fd, STDOUT_FILENO); // 표준 출력을 연결 파일 식별자로 재지정
        Execve(filename, emptylist, environ); // CGI 프로그램(filename) 실행
        exit(0);
    }
    Wait(NULL);
}