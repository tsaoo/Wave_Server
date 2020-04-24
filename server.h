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
#define GTSTAT  111
#define STAT 	112
#define SETSTAT	113
#define REQFO	114
#define REQDV	115
#define DV	116
#define DLCMT	117
#define MVART	118
#define ADLKE 	119

//C1状态字	
#define PWFAIL	200
#define USFAIL	201
#define REGSUCS	202
#define LOGSUCS	203
#define BNULL	204
#define BADCODE	205
#define DATSUCS	206
#define DATFAIL 207
#define REJ		208
#define ALIVE	222

//C2控制字
#define KEEP	0xDD
#define PARTEND	0xEE
#define STOP	0xFF

//参数控制
#define MAXCLNT 100
#define MAX_PATH_LEN 256
#define MAX_USERNAME_LEN 100
#define MAX_PASSWORD_LEN 100
#define PAKLEN	1024
#define MAX_TITLE_LEN 100
#define ROOT "/server/"
#define DB_ROOT "/server/db/"
#define CONFIG_ROOT "/server/config/"
#define LOG_PATH "/server/config/log/"
#define USER_PATH "/server/config/usr/"
#define USERLIST_PATH "/server/config/usr/userlist"
#define DV_PATH "/server/db/dv"
#define DV_ANN_PATH "/server/db/dv_ann"

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
	char comment[921];
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
	char ori;
	char isliked;
	char empty2;
	char empty3;
	int like;
};

struct Servconf{
	int maxc;
	char reg;
	char up;
	char comt;
	char refresh;
	char log;
	char fo;
	char ann;
	int checksum;
};

struct DailyVerse{
	char content[821];
	char author[100];
	char source[100];
};

pthread_t clnt_thread[MAXCLNT];				//用户线程组
struct User clnt_users[MAXCLNT];			//连接对应账户组
int clnt_npthinfo = 0;						//新线程附加信息暂存
int clnt_socks[MAXCLNT];					//用户套接字组
int clnt_stats[MAXCLNT];					//用户套接字状态
struct sockaddr_in clnt_addrs[MAXCLNT];		//用户地址组
socklen_t clnt_addr_sizes[MAXCLNT];			//用户地址大小组
int* clnt_writing[MAXCLNT];				//用户正在修改的文章

//全局设置
int conclnt = 0;			//当前连接数int
int maxclnt = MAXCLNT;		//最大连接数
char p_reg = 1;				//允许注册
char p_log = 1;				//允许登陆
char p_up = 1;				//允许上传
char p_refresh = 1;			//允许刷新
char p_fo = 1;				//允许飞花令
char p_comt = 1;			//允许评论
char p_ann = 0;				//每日一句读取公告

char dic_stats[2] = {READY,READY};
char* DB_PATH[2] = {"/server/db/0/","/server/db/1/"};
char* DIC_PATH[2] = {"/server/db/_0dic","/server/db/_1dic"};

extern void clnt_func();
extern void anacmd(char,char*);
extern int senddat(int,uchar,uchar*,uchar,char);
extern int sendcmd(int,uchar,char);
extern int recvdat(int,uchar*);
extern int adduser(struct User *);
extern struct User locuser(const char*);
extern struct User locuser_byid(int);
extern int checkuserid(struct User*,int,int);
extern int randuserid();
extern int createart(struct Artini,char*,int,size_t);
extern int addcomment(ARTCODE,BLOCKCODE,struct Cmtdat);
extern struct Cmtdat* readcmt(ARTCODE,BLOCKCODE,int,int,int*);
extern int readart(struct Artini,char*,int,size_t);
extern int deleteart(ARTCODE,BLOCKCODE);
extern struct Artini locart(ARTCODE,BLOCKCODE);
extern int getartcode(BLOCKCODE);
extern int randartcode(BLOCKCODE);
extern int checkartcode(struct Artini*,int,ARTCODE,BLOCKCODE);
extern int readdic(struct Artini*, char,int,unsigned int);
extern int adddic(struct Artini);
extern int updatedic(struct Artini,struct Artini);
extern char getisliked(ARTCODE,BLOCKCODE,unsigned int);
extern int minlike(ARTCODE,BLOCKCODE,unsigned int);
extern int addlike(ARTCODE,BLOCKCODE,unsigned int);
extern int readdv(int,struct DailyVerse*);
extern int readdv_ann(struct DailyVerse*);
extern void inttostr(char*,int);
extern void writelog(char*,...);
extern void throwex(char*,...);