/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;

    // 인자 추출
    if ((buf = getenv("QUERY_STIRNG")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p+1);

        strcpy(arg1, index(arg1, '=')+1);
        strcpy(arg2, index(arg2, '=')+1);

        n1 = atoi(arg1);
        n2 = atoi(arg2);
    }

    // 응답 body 버퍼에 저장
    sprintf(content, "QUERY_STRING=%s", buf);
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sThe Internet addition portal. \r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
    // sprintf(content, "%s");

    // HTTP 응답 생성
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    if (strcasecmp(getenv("REQUEST_METHOD"), "GET") == 0) printf("%s", content);
    fflush(stdout);
    
    exit(0);
}
/* $end adder */
