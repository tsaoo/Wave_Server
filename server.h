#include <stdio.h>
#include <pthread.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <wchar.h>
#include <locale.h>
#include <dirent.h>
#include <sys/types.h>
#include <math.h>
#include <stdarg.h>

#define uchar unsigned char
#define ARTCODE unsigned int
#define CMTCODE unsigned char
#define BLOCKCODE char

//状态控制字
#define BUSY 1
#define READY 0
#define WAIT 1
#define NOTWAIT	0

//C0控制字
#define START	0xAA

//C1命令字
#define WFCM	100
#define LOGIN	101
#define REG		102
#define GTART	103
#define GTLST	104
#define ARTINI	105
#define DLART	106
#define ARTDAT	107
#define COMTUP	108
#define COMTDL	109
#define GTCMT	110

//C1状态字	
#define PWFAIL	200
#define USFAIL	201
#define REGSUCS	202
#define LOGSUCS	203
#define BNULL	204
#define BADCODE	205
#define DATSUCS	206
#define DATFAIL 207

//C2控制字
#define KEEP	0xDD
#define PARTEND	0xEE
#define STOP	0xFF

//参数控制
#define MAXCLNT 10
#define MAX_PATH_LEN 256
#define MAX_USERNAME_LEN 256
#define MAX_PASSWORD_LEN 256
#define PAKLEN	1024
#define MAX_TITLE_LEN 256
#define ROOT "/server/poem/"
#define DB_ROOT "/server/poem/db/"
#define CONFIG_ROOT "/server/poem/config/"
#define LOG_PATH "/server/poem/log/"
#define USERLIST_PATH "/server/poem/config/userlist"

struct User
{
	char name[MAX_USERNAME_LEN];
	char pw[MAX_PASSWORD_LEN];
	unsigned int id;
};

struct Cmtdat
{
	time_t time;
	char maker[MAX_USERNAME_LEN];
	int makerid;
	char comment[765];
};

struct Artini
{
	ARTCODE code;
	BLOCKCODE bcode;
	time_t time;
	char title[MAX_TITLE_LEN];
	char author[MAX_USERNAME_LEN];
	char uploader[MAX_USERNAME_LEN];
	int length;
	char type;
	int repcount;
	int uploaderID;
	int empty1;
	int empty2;
};


pthread_t clnt_thread[MAXCLNT];				//用户线程组
struct User clnt_users[MAXCLNT];			//连接对应账户组
int clnt_npthinfo = 0;						//新线程附加信息暂存
int clnt_socks[MAXCLNT];					//用户套接字组
int clnt_stats[MAXCLNT];					//用户套接字状态
struct sockaddr_in clnt_addrs[MAXCLNT];		//用户地址组
socklen_t clnt_addr_sizes[MAXCLNT];			//用户地址大小组
int* clnt_writing[MAXCLNT];				//用户正在修改的文章

char dic_stats[2] = {READY,READY};
char* DB_PATH[2] = {"/server/poem/db/0/","/server/poem/db/1/"};
char* DIC_PATH[2] = {"/server/poem/db/_0dic","/server/poem/db/_1dic"};

extern void clnt_func();
extern void anacmd(char,char*);
extern int senddat(int,uchar,uchar*,uchar,char);
extern int sendcmd(int,uchar,char);
extern int recvdat(int,uchar*);
extern int adduser(struct User *);
extern struct User locuser(const char*);
extern int checkuserid(struct User*,int,int);
extern int randuserid();
extern int createart(struct Artini,char*,int,size_t);
extern int addcomment(ARTCODE,BLOCKCODE,struct Cmtdat);
extern struct Cmtdat* readcmt(ARTCODE,BLOCKCODE,int*);
extern int readart(struct Artini,char*,int,size_t);
extern int deleteart(ARTCODE,BLOCKCODE);
extern struct Artini locart(ARTCODE,BLOCKCODE);
extern int getartcode(BLOCKCODE);
extern int randartcode(BLOCKCODE);
extern int checkartcode(struct Artini*,int,ARTCODE,BLOCKCODE);
extern int readdic(struct Artini*, char,int);
extern int adddic(struct Artini);
extern int updatedic(struct Artini,struct Artini);
extern void inttostr(char*,int);
extern void writelog(char*,...);
extern void throwex(char*,...);