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


char* DB_PATH[3] = {"/server/poem/db/0/","/server/poem/db/1/","/server/poem/db/2/"};
char* DIC_PATH[3] = {"/server/poem/db/_0dic","/server/poem/db/_1dic","/server/poem/db/_2dic"};
extern void inttostr(char*,int);
extern int checkuserid(struct User*,int,int);
extern int randuserid();
extern int getartcode(BLOCKCODE);
extern int checkartcode(struct Artini*,int,ARTCODE,BLOCKCODE);
extern int randartcode(BLOCKCODE);


int main(int argc,char* argv[]){
	setlocale(LC_ALL,"");
	if(argc==1) return 0;

	if(strcmp(argv[1],"mkuser") == 0){
		if(argc < 6){
			printf("too few peremeters\n");
			return 0;
		}
		struct User newuser;
		memset(&newuser,0,sizeof(struct User));
		memcpy(newuser.name,argv[2],atoi(argv[3]));
		memcpy(newuser.pw,argv[4],atoi(argv[5]));

		FILE* userlist = fopen(USERLIST_PATH,"rb+");
		fseek(userlist,0,SEEK_END);
		int usercount = ftell(userlist)/sizeof(struct User);
		rewind(userlist);
		struct User users[usercount];
		fread(users,sizeof(users),usercount,userlist);
		fclose(userlist);
		for(int i=0;i<usercount;i++){
			if(strcmp(users[i].name,newuser.name) == 0){
				printf("error in adduser\n");
				return -1;
			}
		}

		newuser.id = checkuserid(users,usercount,randuserid());

		//添加用户
		userlist = fopen(USERLIST_PATH,"rb+");
		fseek(userlist,0,SEEK_END);
		fwrite(&newuser,sizeof(struct User),1,userlist);
		fclose(userlist);
		return 1;
	}

	else if(strcmp(argv[1],"locuser") == 0){
		if(argc < 3){
			printf("too few parameters\n");
			return -1;
		}
		FILE* userlist = fopen(USERLIST_PATH,"rb+");
		fseek(userlist,0,SEEK_END);
		int usercount = ftell(userlist)/sizeof(struct User);
		rewind(userlist);
		struct User users[usercount];
		fread(users,sizeof(users),usercount,userlist);
		fclose(userlist);
		for(int i=0;i<usercount;i++){
			if(strcmp(users[i].name,argv[2]) == 0){
				printf("found, name:%s pw:%s id:%d\n",users[i].name,users[i].pw,users[i].id);
				return 0;
			}
		}
		printf("not found\n");
	}

	else if(strcmp(argv[1],"getuserid") == 0){
		if(argc < 3){
			printf("too few parameters\n");
			return -1;
		}
		FILE* userlist = fopen(USERLIST_PATH,"rb+");
		fseek(userlist,0,SEEK_END);
		int usercount = ftell(userlist)/sizeof(struct User);
		rewind(userlist);
		struct User users[usercount];
		fread(users,sizeof(users),usercount,userlist);
		fclose(userlist);
		for(int i=0;i<usercount;i++){
			if(strcmp(users[i].name,argv[2]) == 0){
				printf("id:\t%d\n",users[i].id);
				return 0;
			}
		}
	}

	else if(strcmp(argv[1],"deluser") == 0){
		if(argc < 3){
			printf("too few parameters\n");
			return -1;
		}
		FILE* userlist = fopen(USERLIST_PATH,"rb+");
		fseek(userlist,0,SEEK_END);
		int usercount = ftell(userlist)/sizeof(struct User);
		rewind(userlist);
		struct User users[usercount];
		fread(users,sizeof(users),usercount,userlist);
		rewind(userlist);
		for(int i=0;i<usercount;i++){
			if(strcmp(users[i].name,argv[2]) == 0){
				for(int j=0;j<i;j++)
					fwrite(&users[j],sizeof(struct User),1,userlist);
				for(int j=i+1;j<usercount;j++)
					fwrite(&users[j],sizeof(struct User),1,userlist);
				printf("deleted\n");
				fclose(userlist);
				return 0;
			}
		}
		printf("unfound\n");
		fclose(userlist);
		return -1;
	}

	else if(strcmp(argv[1],"deleteart") == 0){
		if(argc < 4){
			printf("too few parameters\n");
			return -1;
		}
		BLOCKCODE bcode = *argv[2]-48;
		ARTCODE acode = atoi(argv[3]);

		FILE* dic = fopen(DIC_PATH[bcode],"rb+");
		fseek(dic,0,SEEK_END);
		int count = ftell(dic)/sizeof(struct Artini);
		rewind(dic);

		struct Artini list[count];
		memset(list,0,sizeof(list));
		fread(list,sizeof(struct Artini),count,dic);
		fclose(dic);

		dic = fopen(DIC_PATH[bcode],"wb+");
		char is_found = 0;
		for(int i=0;i<count;++i){
			if(list[i].code == acode){
				is_found = 1;
				for(int j=0;j<i;++j)
					fwrite(&list[j],sizeof(struct Artini),1,dic);
				for(int j=i+1;j<count;++j)
					fwrite(&list[j],sizeof(struct Artini),1,dic);
				fclose(dic);
				printf("found & deleted info\n");
			}
		}
		if(!is_found){
			for(int j=0;j<count;++j)
				fwrite(&list[j],sizeof(struct Artini),1,dic);
			fclose(dic);
			return -1;
		}

		char path[MAX_PATH_LEN],mainpath[MAX_PATH_LEN],cmtpath[MAX_PATH_LEN];
		memset(path,0,MAX_PATH_LEN);
		char acodestr[11];
		inttostr(acodestr,acode);
		strcpy(path,DB_PATH[bcode]);
		strcpy(path+strlen(path),acodestr);
		strcpy(mainpath,path);
		strcpy(cmtpath,path);
		strcpy(mainpath+strlen(mainpath),"/main");
		strcpy(cmtpath+strlen(cmtpath),"/comment");
		remove(mainpath);
		remove(cmtpath);
		rmdir(path);
		printf("found & deleted data\n");
		return 1;
	}
}

void inttostr(char* s,int l){
	for(int i=0;i<10;i++)
		s[i] = (l%(int)pow(10,10-i))/(int)pow(10,9-i) + 48;
	s[11]=0;
}

//用以递归调用
int checkuserid(struct User* list,int count,int id){
	for(int i=0;i<count;++i){
		if(list[i].id == id){
			//若此ID与表中某ID相等，则再次递归检查新生成的随机ID
			return checkuserid(list,count,randuserid());
		}
	}
	//若此ID与表中的任何ID都不相等，则返回这个ID
	return id;
}

//不加检查地创建一个随机ID
int randuserid(){
	int id = 0;
	for(int i=0;i<5;++i)
		id += ((int)rand()%100)*((int)pow(10,i*2));
	return id;
}

//获取一个可用的code
int getartcode(BLOCKCODE bcode){
	//非递归部分，用于获取list和count
	FILE* dic = fopen(DIC_PATH[bcode],"rb+");
	fseek(dic,0,SEEK_END);
	int count = ftell(dic)/sizeof(struct Artini);
	rewind(dic);
	struct Artini list[count];
	fread(list,sizeof(struct Artini),count,dic);
	fclose(dic);

	ARTCODE code = checkartcode(list,count,randartcode(bcode),bcode);
	return code;
}

//用以递归调用
int checkartcode(struct Artini* list,int count,ARTCODE n_code,BLOCKCODE bcode){
	for(int i=0;i<count;++i){
		if(list[i].code == n_code){
			//若此code与表中某code相等，则再次递归检查新生成的随机code
			printf("%d denied\n",n_code);
			return checkartcode(list,count,randartcode(bcode),bcode);
		}
	}
	//若此code与表中的任何code都不相等，则返回这个code
	printf("%d passed\n",n_code);
	return n_code;
}

//不加检查地创建一个随机code
int randartcode(BLOCKCODE bcode){
	//保证code是以bcode开头的10位十进制整数
	ARTCODE code = bcode*1e10;
	for(int i=0;i<4;++i)
		code += ((int)rand()%100)*((int)pow(10,i*2));
	printf("generated %d\n",code);
	return code;
}