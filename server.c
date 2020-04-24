#include "server.h"

int main(int argc,char* argv[]){
	if(argc == 1){
		printf("NO IP INPUT\n");
		return 0;
	}

	struct sockaddr_in serv_addr;
	int serv_sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	memset(&serv_addr,0,sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(26534);
	bind(serv_sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr));

	listen(serv_sock,maxclnt);		//启动监听，队列最长为MAXCLNT

	for(int i=0;i<maxclnt;++i){		//初始化
		memset(&clnt_addrs[i],0,sizeof(clnt_addrs[i]));
		clnt_stats[i] = BUSY;
		clnt_addr_sizes[i] = sizeof(clnt_addrs[i]);
		clnt_socks[i] = -1;			//未使用套接字时留空
		printf("client[%d] initialized\n",i);
	}

	while(1){ 
		int avanum = -1;		//可用的用户编号
		for(int i=0;i<maxclnt;++i)
			if(clnt_socks[i]==-1){
				avanum = i;
				break;
			}
		if(avanum!=-1){		//如果有可用位置，中断，直到获取用户连接

			clnt_socks[avanum] = accept(serv_sock,
				(struct sockaddr*)&(clnt_addrs[avanum]),
				&clnt_addr_sizes[avanum]);
			
			printf("clinet connected, avanum:%d\n",avanum);
			writelog("CONNECT clntnum:%d",avanum);
			conclnt ++;

			clnt_npthinfo = avanum;

			pthread_create(&clnt_thread[avanum],
				NULL,
				(void*)clnt_func,
				NULL);
			pthread_detach(clnt_thread[avanum]);

		}
	}
	//write(clnt_socks,message,sizeof(message));
	close(serv_sock);
	//close(clnt_socks);
	return 0;
}

void clnt_func(){
	//连接初始化
	char clnt_num = clnt_npthinfo;
	uchar buffer[PAKLEN-1];
	while(clnt_socks[clnt_num] != -1){
		if(recvdat(clnt_num,buffer) == -1) break;
		anacmd(clnt_num,buffer);
		memset(buffer,0,PAKLEN-1);
	}
	printf("client[%d] disconnected\n",clnt_num);
	writelog("DISCON client[%d](%s) disconnected",clnt_num,clnt_users[clnt_num].name);
	conclnt --;
	return;
}

void anacmd(char clnt_num,char* buffer){
	if(buffer[0] == WFCM){
		clnt_stats[clnt_num] = READY;
		return;
	}

	else if(buffer[0] == REG){
		if(p_reg == 0){
			sendcmd(clnt_num,REJ,WAIT);
			return;
		}

		struct User newuser;
		memset(&newuser,0,sizeof(struct User));
		strcpy(newuser.name,buffer+1);
		if(strcmp(locuser(newuser.name).name,"erruser") == 0){
			sendcmd(clnt_num,USFAIL,WAIT);
			return;
		}
		strcpy(newuser.pw,buffer+101);
		adduser(&newuser);
		sendcmd(clnt_num,REGSUCS,WAIT);
		printf("client[%d] registed name:%s pw:%s\n",clnt_num,newuser.name,newuser.pw);
		writelog("REG client[%d] registed name:%s pw:%s\n",clnt_num,newuser.name,newuser.pw);
		return;
	}

	else if(buffer[0] == LOGIN){
		if(p_log == 0){
			sendcmd(clnt_num,REJ,WAIT);
			return;
		}

		struct User user;
		memcpy(user.name,buffer+1,100);
		memcpy(user.pw,buffer+101,100);
		printf("client[%d] username:%s pw:%s\n",clnt_num,user.name,user.pw);
		writelog("LOGIN client[%d] trys to log with username:%s pw:%s",clnt_num,user.name,user.pw);
		
		if(strcmp(locuser(user.name).name,"unknown") == 0){
			sendcmd(clnt_num,USFAIL,WAIT);
			writelog("USFAIL client[%d] submitted unknown username:%s",clnt_num,user.name);
			return;
		}
		struct User cmpuser = locuser(user.name);
		if(strcmp(cmpuser.pw,user.pw) == 0){
			user.id = cmpuser.id;
			clnt_users[clnt_num] = user;
			char userid[4];
			memcpy(userid,&user.id,4);
			senddat(clnt_num,LOGSUCS,userid,STOP,WAIT);
			writelog("LOGSUCS client[%d] username:%s pw:%s",clnt_num,user.name,user.pw);

		}else{
			sendcmd(clnt_num,PWFAIL,WAIT);
			writelog("PWFAIL client[%d] submitted wrong pw:%s",user.pw);
		}
		return;
	}

	else if(buffer[0] == GTLST){
		if(p_refresh == 0){
			sendcmd(clnt_num,REJ,WAIT);
			return;
		}

		int offset = 0, size = 10;
		memcpy(&offset,buffer+2,4);
		memcpy(&size,buffer+6,4);

		printf("client[%d](%s) is requiring list of block %d\n",clnt_num,clnt_users[clnt_num].name,buffer[1]);
		writelog("GTLST client[%d](%s) requires list of block %d",clnt_num,clnt_users[clnt_num].name,buffer[1]);
		
		char over_flag = 0;
		//包循环，共需发包size/3+1次
		struct Artini* ini = (struct Artini*)(malloc(sizeof(struct Artini)));
		memset(ini,0,sizeof(struct Artini));
		for(int i=0;i<(size/3)+1;++i){

			struct Artini* inipack = (struct Artini*)malloc(3*sizeof(struct Artini));
			memset(inipack,0,3*sizeof(struct Artini));
			char dat[PAKLEN-3];

			//ini循环，每包3个
			for(int j=0;j<3&&(3*i+j)<=size;j++){
				//尚可继续读取，则继续
				int res = 0;
				if(res=(readdic(ini,buffer[1],offset+i*3+j,clnt_users[clnt_num].id))>0){
					memcpy(inipack+j,ini,sizeof(struct Artini));
					memset(ini,0,sizeof(struct Artini));
				}

				//若返回0，则刚好读取完毕，停止读取，直接发送并终止传输
				else if(res == 0){
					for(int k=0;k<j+1;k++){
						memcpy(inipack+j,ini,sizeof(struct Artini));
						memcpy(dat+340*k+0,&inipack[k].code,4);
						memcpy(dat+340*k+4,&inipack[k].bcode,1);
						memcpy(dat+340*k+5,&inipack[k].time,4);
						memcpy(dat+340*k+9,&inipack[k].title,100);
						memcpy(dat+340*k+109,&inipack[k].author,100);
						memcpy(dat+340*k+209,&inipack[k].uploader,100);
						memcpy(dat+340*k+309,&inipack[k].length,4);
						memcpy(dat+340*k+313,&inipack[k].type,1);
						memcpy(dat+340*k+314,&inipack[k].repcount,2);
						memcpy(dat+340*k+316,&inipack[k].uploaderID,4);
						memcpy(dat+340*k+320,&inipack[k].ori,1);
						memcpy(dat+340*k+321,&inipack[k].like,4);
						memcpy(dat+340*k+325,&inipack[k].isliked,1);
					}
					senddat(clnt_num,ARTINI,dat,STOP,WAIT);
					free(inipack);
					free(ini);
					return;
				}

				//若返回-1，则目录为空，发送BNULL
				else{
					sendcmd(clnt_num,BNULL,WAIT);
					free(inipack);
					free(ini);
					return;
				}
			}

			//在包内3个ini读取完毕后，格式化数据，并发送这个包
			for(int j=0;j<3;j++){
				memcpy(dat+340*j+0,&inipack[j].code,4);
				memcpy(dat+340*j+4,&inipack[j].bcode,1);
				memcpy(dat+340*j+5,&inipack[j].time,4);
				memcpy(dat+340*j+9,&inipack[j].title,100);
				memcpy(dat+340*j+109,&inipack[j].author,100);
				memcpy(dat+340*j+209,&inipack[j].uploader,100);
				memcpy(dat+340*j+309,&inipack[j].length,4);
				memcpy(dat+340*j+313,&inipack[j].type,1);
				memcpy(dat+340*j+314,&inipack[j].repcount,2);
				memcpy(dat+340*j+316,&inipack[j].uploaderID,4);
				memcpy(dat+340*j+320,&inipack[j].ori,1);
				memcpy(dat+340*j+321,&inipack[j].like,4);
				memcpy(dat+340*j+325,&inipack[j].isliked,1);
			}
			if(i<(size/3+1)-1)
				senddat(clnt_num,ARTINI,dat,KEEP,WAIT);
			else
				senddat(clnt_num,ARTINI,dat,STOP,WAIT);

			free(inipack);
		}
		free(ini);
		return;
	}

	else if(buffer[0] == GTART){	
		ARTCODE code = 0;
		BLOCKCODE bcode = 0;
		memcpy(&code,buffer+1,4);
		memcpy(&bcode,buffer+5,1);
		printf("client[%d](%s) is requiring artdat %d\n",clnt_num,clnt_users[clnt_num].name,code);
		writelog("GTART client[%d](%s) requires artdat %d",clnt_num,clnt_users[clnt_num].name,code);
		struct Artini ini = locart(code,bcode);
		//如果返回的Artini.code为零，则判断请求的code不存在
		if(ini.code == 0){
			sendcmd(clnt_num,BADCODE,WAIT);
			return;
		}

		char* dat = (char*)malloc(PAKLEN-3);
		memset(dat,0,PAKLEN-3);
		int offset = 0;
		for(int offset=0;readart(ini,dat,offset,PAKLEN-3)==PAKLEN-3;offset+=(PAKLEN-3)){
			senddat(clnt_num,ARTDAT,dat,KEEP,WAIT);
			memset(dat,0,PAKLEN-3);
		}
		senddat(clnt_num,ARTDAT,dat,STOP,WAIT);
		free(dat);
		return;
	}

	else if(buffer[0] == ARTINI){
		if(p_up == 0){
			sendcmd(clnt_num,REJ,WAIT);
			return;
		}
		//解析上传的Artini
		struct Artini ini;
		memset(&ini,0,sizeof(struct Artini));
		memcpy(&ini.bcode,buffer+5,1);
		memcpy(&ini.time,buffer+6,4);
		memcpy(&ini.title,buffer+10,100);
		memcpy(&ini.author,buffer+110,100);
		//memcpy(&ini.uploader,buffer+220,110);
		memcpy(&ini.length,buffer+310,4);
		memcpy(&ini.type,buffer+314,1);
		memcpy(&ini.repcount,buffer+315,2);
		memcpy(&ini.uploaderID,buffer+317,4);
		memcpy(&ini.ori,buffer+321,1);
		memcpy(&ini.like,buffer+322,4);
		ini.code = getartcode(ini.bcode);
		printf("client[%d](%s) is uploading %s(%d) size:%d to block %d\n",clnt_num,clnt_users[clnt_num].name,ini.title,ini.code,ini.length,ini.bcode);
		writelog("ARTINI client[%d](%s) start uploading %s(%d) size:%d to block %d",clnt_num,clnt_users[clnt_num].name,ini.title,ini.code,ini.length,ini.bcode);		
		//获取文章正文
		uchar *dat = (char*)malloc(sizeof(char)*ini.length);
		uchar *buf = (char*)malloc(sizeof(char)*PAKLEN-1);
		int offset = 0;
		memset(dat,0,sizeof(sizeof(char)*ini.length));
		memset(buf,0,PAKLEN-1);

		recvdat(clnt_num,buf);
		while(*(buf+PAKLEN-2) != STOP){
			memcpy(dat+offset,buf+1,PAKLEN-3);
			offset += PAKLEN-3;
			recvdat(clnt_num,buf);
		}
		memcpy(dat+offset,buf+1,ini.length-offset);

		//更新目录，写入正文
		adddic(ini);
		offset = 0;
		while(1){
			int res = createart(ini,dat,offset,1000);
				if(res==-1){
				sendcmd(clnt_num,DATFAIL,WAIT);
				writelog("DATFAIL client[%d](%s) failed to upload",clnt_num,clnt_users[clnt_num].name);
				free(dat);
				free(buf);
				return;
			}
			offset += res;
			if(ini.length - offset < 1000)
				break;
		}
		int res = createart(ini,dat,offset,ini.length-offset);
		sendcmd(clnt_num,DATSUCS,WAIT);
		free(dat);
		free(buf);
		return;
	}

	else if(buffer[0] == COMTUP){
		if(p_comt == 0){
			sendcmd(clnt_num,REJ,WAIT);
			return;
		}

		ARTCODE acode = 0;
		BLOCKCODE bcode = 0;
		struct Cmtdat cmt;
		memset(&cmt,0,sizeof(struct Cmtdat));
		memcpy(&acode,buffer+1,4);
		memcpy(&bcode,buffer+5,1);
		memcpy(&cmt.comment,buffer+6,921);
		cmt.time = time(NULL);
		cmt.makerid = clnt_users[clnt_num].id;

		printf("client[%d](%s) replied %d makerid:%d\n",clnt_num,clnt_users[clnt_num].name,acode,cmt.makerid);
		writelog("COMTUP client[%d](%s) replied %d makerid:%d",clnt_num,clnt_users[clnt_num].name,acode,cmt.makerid);

		if(addcomment(acode,bcode,cmt) == 1)
			sendcmd(clnt_num,DATSUCS,WAIT);
		else
			sendcmd(clnt_num,DATFAIL,WAIT);
		return;
	}

	else if(buffer[0] == GTCMT){
		ARTCODE acode = 0;
		BLOCKCODE bcode = 0;
		int offset = 0;
		int size = 0;
		memcpy(&acode,buffer+1,4);
		memcpy(&bcode,buffer+5,1);
		memcpy(&offset,buffer+6,4);
		memcpy(&size,buffer+10,4);
		int count = 0;
		struct Cmtdat* cmtlist = readcmt(acode,bcode,offset,size,&count);
		if(count <= 0){
			sendcmd(clnt_num,BNULL,WAIT);
			return;
		}

		for(int i=0;i<count;++i){
			char dat[1021];
			memset(dat,0,1021);
			memcpy(dat,cmtlist[i].maker,100);
			memcpy(dat+100,cmtlist[i].comment,921);
			if(i<count-1){
				senddat(clnt_num,COMTDL,dat,KEEP,WAIT);
				//printf("send COMTDL KEEP\n");
			}
			else{
				senddat(clnt_num,COMTDL,dat,STOP,WAIT);
				//printf("send COMTDL STOP\n");
			}
		}
		free(cmtlist);
		return;
	}

	else if(buffer[0] == DLART){
		ARTCODE acode = 0;
		BLOCKCODE bcode = 0;
		memcpy(&acode,buffer+1,4);
		memcpy(&bcode,buffer+5,1);
		printf("client[%d](%s) deleting %d\n",clnt_num,clnt_users[clnt_num].name,acode);
		writelog("DLART client[%d](%s) deleting %d",clnt_num,clnt_users[clnt_num].name,acode);

		if(deleteart(acode,bcode) == 1)
			sendcmd(clnt_num,DATSUCS,WAIT);
		else{
			sendcmd(clnt_num,DATFAIL,WAIT);
			writelog("DATFAIL client[%d](%s) failed to delete %d",clnt_num,clnt_users[clnt_num].name,acode);
		}
		return;
	}

	else if(buffer[0] == GTSTAT){
		char conf[1021];
		memset(conf,0,1021);
		memcpy(conf,&maxclnt,4);
		memcpy(conf+4,&conclnt,4);
		memcpy(conf+8,&p_reg,1);
		memcpy(conf+9,&p_up,1);
		memcpy(conf+10,&p_comt,1);
		memcpy(conf+11,&p_refresh,1);
		memcpy(conf+12,&p_log,1);
		memcpy(conf+13,&p_fo,1);
		memcpy(conf+14,&p_ann,1);
		int checksum = maxclnt+p_reg+p_up+p_comt+p_refresh+p_log+p_fo+p_ann;
		memcpy(conf+15,&checksum,4);
		senddat(clnt_num,STAT,conf,STOP,WAIT);
		writelog("GTSTAT client[%d](%s)",clnt_num,clnt_users[clnt_num].name); 
		return;
	}

	else if(buffer[0] == SETSTAT){
		struct Servconf* conf = (struct Servconf*)malloc(sizeof(struct Servconf));
		//memcpy(conf,buffer+1,14);
		memcpy(&conf->maxc,buffer+1,4);
		memcpy(&conf->reg,buffer+9,1);
		memcpy(&conf->up,buffer+10,1);
		memcpy(&conf->comt,buffer+11,1);
		memcpy(&conf->refresh,buffer+12,1);
		memcpy(&conf->log,buffer+13,1);
		memcpy(&conf->fo,buffer+14,1);
		memcpy(&conf->checksum,buffer+16,4);
		memcpy(&conf->ann,buffer+15,1);
		int checksum = conf->maxc+conf->reg+conf->up+conf->comt+conf->refresh+conf->log+conf->fo+conf->ann;
		if(checksum != conf->checksum){
			sendcmd(clnt_num,DATFAIL,WAIT);
			return;
		}
		printf("client[%d](%s) set server config\n\tmaxclient=%d p_reg=%d p_up=%d p_comt=%d p_refresh=%d p_log=%d p_fo=%d p_ann=%d\n",
			clnt_num,clnt_users[clnt_num].name,conf->maxc,conf->reg,conf->up,conf->comt,conf->refresh,conf->log,conf->fo,conf->ann);
		writelog("SETSTAT client[%d](%s) set server config\n\tmaxclient=%d p_reg=%d p_up=%d p_comt=%d p_refresh=%d p_log=%d p_fo=%d p_ann=%d",
			clnt_num,clnt_users[clnt_num].name,conf->maxc,conf->reg,conf->up,conf->comt,conf->refresh,conf->log,conf->fo,conf->ann);

		maxclnt = conf->maxc;
		p_reg = conf->reg;
		p_up = conf->up;
		p_comt = conf->comt;
		p_refresh = conf->refresh;
		p_log = conf->log;
		p_fo = conf->fo;
		p_ann = conf->ann;

		sendcmd(clnt_num,DATSUCS,WAIT);
		return;
	}

	else if(buffer[0] == REQFO){
		if(p_fo == 1)
			sendcmd(clnt_num,DATSUCS,WAIT);
		else
			sendcmd(clnt_num,REJ,WAIT);
		writelog("REQFO client[%d](%s)",clnt_num,clnt_users[clnt_num].name);
		return;
	}

	else if(buffer[0] == ALIVE){
		return;
	}

	//undone
	else if(buffer[0] == REQDV){
		int index = 0;
		memcpy(&index,buffer+1,4);
		struct DailyVerse* dv = (struct DailyVerse*)(malloc(sizeof(struct DailyVerse)));
		memset(dv,0,sizeof(struct DailyVerse));

		if(p_ann == 1){
			if(readdv_ann(dv) <= 0){
				sendcmd(clnt_num,REJ,WAIT);
				return;
			}
			char dat[1021];
			memcpy(dat,dv,sizeof(struct DailyVerse));
			senddat(clnt_num,DV,dat,STOP,WAIT);
			return;
		}

		if(readdv(index,dv) <= 0){
			sendcmd(clnt_num,REJ,WAIT);
			return;
		}

		char dat[1021];
		memcpy(dat,dv,sizeof(struct DailyVerse));
		senddat(clnt_num,DV,dat,STOP,WAIT);
		free(dv);
		return;
	}
	
	else if(buffer[0] == MVART){
		ARTCODE acode = 0;
		BLOCKCODE bcode = 0;
		memcpy(&acode,buffer+1,sizeof(ARTCODE));
		memcpy(&bcode,buffer+1+sizeof(ARTCODE),sizeof(BLOCKCODE));

		printf("client[%d](%s) reset position of %d \n",clnt_num,clnt_users[clnt_num].name,acode);
		writelog("MVART client[%d](%s) %d ori:%d",clnt_num,clnt_users[clnt_num].name,acode,bcode);
		
		struct Artini oini = locart(acode,bcode);
		if(oini.code == 0){
			sendcmd(clnt_num,DATFAIL,WAIT);
			return;
		}
		
		struct Artini nini = oini;
		if(nini.bcode == 0)
			nini.bcode = 1;
		else
			nini.bcode = 0;
		if(updatedic(oini,nini) != 1){
			sendcmd(clnt_num,DATFAIL,WAIT);
			return;
		}
		sendcmd(clnt_num,DATSUCS,WAIT);
		return;
	}

	else if(buffer[0] == ADLKE){
		ARTCODE acode = 0;
		BLOCKCODE bcode = 0;
		char op = buffer[6];
		memcpy(&acode,buffer+1,4);
		memcpy(&bcode,buffer+5,1);

		if(op == 0){
			if(addlike(acode,bcode,clnt_users[clnt_num].id) <= 0)
				sendcmd(clnt_num,DATFAIL,WAIT);
			else
				sendcmd(clnt_num,DATSUCS,WAIT);
		}else{
			if(minlike(acode,bcode,clnt_users[clnt_num].id) <= 0)
				sendcmd(clnt_num,DATFAIL,WAIT);
			else
				sendcmd(clnt_num,DATSUCS,WAIT);			
		}
		return;
	}
	//sendcmd(clnt_num,REJ,WAIT);
}

//===========================连接控制=======================
int senddat(int clnt_num,uchar c1,uchar* dat,uchar c2,char wait){
	uchar buffer[PAKLEN];
	memset(buffer,0,PAKLEN);
	if(wait){
		while(clnt_stats[clnt_num] != READY){
			if(recvdat(clnt_num,buffer) == -1)	return -1;
			if(buffer[0] == WFCM){
				clnt_stats[clnt_num] = READY;
				printf("client[%d](%s) --> Flag:%d\n",clnt_num,clnt_users[clnt_num].name,WFCM);
			}
		}
		memset(buffer,0,PAKLEN);
	}

	buffer[0] = START;
	buffer[1] = c1;
	memcpy(buffer+2,dat,PAKLEN-3);
	buffer[PAKLEN-1] = c2;
	write(clnt_socks[clnt_num],buffer,PAKLEN);
	clnt_stats[clnt_num] = BUSY;
	printf("client[%d](%s) <-- Dat %d\n",clnt_num,clnt_users[clnt_num].name,c1);
}

int sendcmd(int clnt_num,uchar c1,char wait){
	uchar buffer[PAKLEN];
	memset(buffer,0,PAKLEN);
	if(wait){
		while(clnt_stats[clnt_num] != READY){
			if(recvdat(clnt_num,buffer) == -1)	return -1;
			if(buffer[0] == WFCM){
				clnt_stats[clnt_num] = READY;
				printf("client[%d](%s) --> Flag:%d\n",clnt_num,clnt_users[clnt_num].name,WFCM);
			}
		}
		memset(buffer,0,PAKLEN);
	}

	buffer[0] = START;
	buffer[1] = c1;
	buffer[PAKLEN-1] = STOP;
	write(clnt_socks[clnt_num],buffer,PAKLEN);
	clnt_stats[clnt_num] = BUSY;
	printf("client[%d] <-- Flag %d\n",clnt_num,c1);
}

int recvdat(int clnt_num,uchar* out){
	uchar buffer[PAKLEN];
	memset(buffer,0,PAKLEN);
	while(buffer[0] != 0xAA){
		if(recv(clnt_socks[clnt_num],buffer,PAKLEN,MSG_WAITALL) <= 0){
			close(clnt_socks[clnt_num]);
			clnt_socks[clnt_num] = -1;
			return -1;
		}
	}
	memcpy(out,buffer+1,PAKLEN-1);
	return 1;
}

//========================用户管理===========================
int adduser(struct User *newuser){
	//确认现有用户表中没有该用户
	FILE* userlist = fopen(USERLIST_PATH,"rb+");
	fseek(userlist,0,SEEK_END);
	int usercount = ftell(userlist)/sizeof(struct User);
	rewind(userlist);
	struct User users[usercount];
	fread(users,sizeof(users),usercount,userlist);
	fclose(userlist);
	for(int i=0;i<usercount;i++){
		if(strcmp(users[i].name,newuser->name) == 0){
			printf("error in adduser\n");
			return -1;
		}
	}

	newuser->id = checkuserid(users,usercount,randuserid());

	//添加用户
	userlist = fopen(USERLIST_PATH,"rb+");
	fseek(userlist,0,SEEK_END);
	fwrite(newuser,sizeof(struct User),1,userlist);
	fclose(userlist);
	return 1;
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

struct User locuser(const char* username){
//寻找该用户（代码复用）
	FILE* userlist = fopen(USERLIST_PATH,"rb+");
	fseek(userlist,0,SEEK_END);
	int usercount = ftell(userlist)/sizeof(struct User);
	rewind(userlist);
	struct User* users = (struct User*)malloc(usercount*sizeof(struct User));
	struct User res;
	memset(users,0,sizeof(struct User)*usercount);
	fread(users,sizeof(struct User),usercount,userlist);
	fclose(userlist);
	for(int i=0;i<usercount;i++){
		if(strcmp(users[i].name,username) == 0){
			res = users[i];
			free(users);
			return res;
		}
	}
	free(users);
	struct User erruser={"unknown","unkonwn"};
	return erruser;
}

struct User locuser_byid(int id){
//寻找该用户（代码复用）
	FILE* userlist = fopen(USERLIST_PATH,"rb+");
	fseek(userlist,0,SEEK_END);
	int usercount = ftell(userlist)/sizeof(struct User);
	rewind(userlist);
	struct User* users = (struct User*)malloc(usercount*sizeof(struct User));
	struct User res;
	memset(users,0,sizeof(struct User)*usercount);
	fread(users,sizeof(struct User),usercount,userlist);
	fclose(userlist);
	for(int i=0;i<usercount;i++){
		if(users[i].id == id){
			res = users[i];
			free(users);
			return res;
		}
	}
	free(users);
	struct User erruser={"unknown","unkonwn",0};
	return erruser;
}

//======================文章管理========================
//创建新文章，返回实际写入的字节数
int createart(struct Artini ini,char* dat,int offset,size_t size){
	char codestr[11];
	memset(codestr,0,11);
	inttostr(codestr,ini.code);

	char fpath[MAX_PATH_LEN],cmtpath[MAX_PATH_LEN],likepath[MAX_PATH_LEN];
	strcpy(fpath,DB_PATH[ini.bcode]);
	strcpy(fpath+strlen(fpath),codestr);
	mkdir(fpath,0700);
	strcpy(cmtpath,fpath);
	strcpy(likepath,fpath);
	strcpy(fpath+strlen(fpath),"/main");
	strcpy(cmtpath+strlen(cmtpath),"/comment");
	strcpy(likepath+strlen(likepath),"/like");

	FILE* artfile;
	if((artfile=fopen(fpath,"ab+"))==NULL)
		return -1;
	int writelen = fwrite(dat+offset,1,size,artfile);
	fclose(artfile);

	if((artfile=fopen(cmtpath,"ab+"))==NULL)
		return -1;
	fclose(artfile);

	if((artfile=fopen(likepath,"ab+"))==NULL)
		return -1;
	fclose(artfile);

	return writelen;
}

//添加回复
int addcomment(ARTCODE acode,BLOCKCODE bcode,struct Cmtdat cmt){
	char acodestr[11];
	memset(acodestr,0,11);
	inttostr(acodestr,acode);

	char fpath[MAX_PATH_LEN];
	strcpy(fpath,DB_PATH[bcode]);
	strcpy(fpath+strlen(fpath),acodestr);
	strcpy(fpath+strlen(fpath),"/comment");
	FILE* file;
	if((file=fopen(fpath,"ab+")) == NULL)
		return -2;
	fseek(file,0,SEEK_END);
	int count = ftell(file)/sizeof(struct Cmtdat);
	rewind(file);

	struct Cmtdat list[count+1];
	memset(list,0,sizeof(list));
	fread(list+1,sizeof(struct Cmtdat),count,file);
	fclose(file);
	list[0] = cmt;

	file = fopen(fpath,"wb+");
	if(fwrite(list,sizeof(struct Cmtdat),count+1,file)<=0){
		fclose(file);
		return -1;
	}
	fclose(file);

	struct Artini oini = locart(acode,bcode);
	struct Artini nini = oini;
	nini.repcount ++;
	updatedic(oini,nini);
	return 1;
}

//读取正文
int readart(struct Artini ini,char* buf,int offset,size_t size){
	char codestr[11];
	memset(codestr,0,11);
	inttostr(codestr,ini.code);

	char fpath[MAX_PATH_LEN];
	strcpy(fpath,DB_PATH[ini.bcode]);
	strcpy(fpath+strlen(fpath),codestr);
	strcpy(fpath+strlen(fpath),"/main");

	FILE* artfile;
	if((artfile = fopen(fpath,"rb+")) == NULL)
		return -1;
	fseek(artfile,offset,SEEK_SET);
	memset(buf,0,sizeof(buf));
	int readlen = fread(buf,1,size,artfile);
	fclose(artfile);
	return readlen;
}

int deleteart(ARTCODE acode,BLOCKCODE bcode){
	if(locart(acode,bcode).code == 0){
		return -1;
	}

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
		}
	}
	if(!is_found){
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

	return 1;
}

//读取评论，返回评论数目
struct Cmtdat* readcmt(ARTCODE acode,BLOCKCODE bcode,int offset,int size,int* countout){
	char path[MAX_PATH_LEN];
	memset(path,0,MAX_PATH_LEN);
	char acodestr[11];
	memset(acodestr,0,11);
	inttostr(acodestr,acode);
	strcpy(path,DB_PATH[bcode]);
	strcpy(path+strlen(path),acodestr);
	strcpy(path+strlen(path),"/comment");
	FILE* file;
	if((file = fopen(path,"rb+")) == NULL)
		return NULL;
	fseek(file,0,SEEK_END);
	int count = ftell(file)/sizeof(struct Cmtdat);
	rewind(file);

	struct Cmtdat* list = (struct Cmtdat*)malloc(sizeof(struct Cmtdat)*count);
	struct Cmtdat* res = (struct Cmtdat*)malloc(sizeof(struct Cmtdat)*size);
	fread(list,sizeof(struct Cmtdat),count,file);
	//根据每条评论的评论者ID获取评论者名
	for(int i=0;i+offset<count&&i<size;++i){
		strcpy(list[i+offset].maker, locuser_byid(list[i+offset].makerid).name);
		memcpy(&res[i],&list[i+offset],sizeof(struct Cmtdat));
		*countout += 1;
	}
	fclose(file);
	free(list);
	return res;
}

//读取artini目录，其返回值为本次读取后的剩余条目数
int readdic(struct Artini* ini, BLOCKCODE bcode,int offset,unsigned int userid){
	memset(ini,0,sizeof(struct Artini));
	FILE* dic = fopen(DIC_PATH[bcode],"rb+");
	fseek(dic,0,SEEK_END);
	int count = ftell(dic)/sizeof(struct Artini);
	if(offset >= count){
		fclose(dic);
		return -1;
	}
	fseek(dic,offset*sizeof(struct Artini),SEEK_SET);

	if(fread(ini,sizeof(struct Artini),1,dic) <= 0){
		fclose(dic);
		return -1;
	}
	else{
		fclose(dic);
		memcpy(&ini->uploader,locuser_byid(ini->uploaderID).name,100);
		ini->isliked = getisliked(ini->code,ini->bcode,userid);
		//返回剩余的artini条目数
		return count-offset-1;
	}
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

//以code搜索Artini
struct Artini locart(ARTCODE code,BLOCKCODE bcode){
	FILE* dic = fopen(DIC_PATH[bcode],"rb+");
	fseek(dic,0,SEEK_END);
	int count = ftell(dic)/sizeof(struct Artini);
	rewind(dic);

	struct Artini list[count];
	memset(list,0,sizeof(list));
	fread(list,sizeof(struct Artini),count,dic);
	fclose(dic);
	for(int i=0;i<count;++i){
		if(list[i].code == code){
			//根据uploaderID获取uploader
			strcpy(list[i].uploader, locuser_byid(list[i].uploaderID).name);
			return list[i];
		}
	}
	struct Artini null = {0,0,0,0,0,0};
	return null;
}

//向目录增添文章条目
int adddic(struct Artini ini){
	while(dic_stats[ini.bcode] == BUSY);
	dic_stats[ini.bcode] = BUSY;

	FILE* dic = fopen(DIC_PATH[ini.bcode],"rb+");
	fseek(dic,0,SEEK_END);
	int count = ftell(dic)/sizeof(struct Artini);
	rewind(dic);
	//新加入的文章信息处于目录顶部
	struct Artini list[count + 1];
	fread(list+1,sizeof(struct Artini),count,dic);
	list[0] = ini;
	fclose(dic);
	dic = fopen(DIC_PATH[ini.bcode],"wb+");
	fwrite(list,sizeof(struct Artini),count+1,dic);
	fclose(dic);
	dic_stats[ini.bcode] = READY;
	return 1;
}

//在目录中更新条目
int updatedic(struct Artini oini,struct Artini nini){
	//新旧条目序列号不同，应使用createart和delart
	if(oini.code != nini.code)
		return -2;
	//不转区
	if(oini.bcode == nini.bcode){
		while(dic_stats[oini.bcode] == BUSY);
		dic_stats[oini.bcode] = BUSY;

		FILE* dic = fopen(DIC_PATH[oini.bcode],"rb+");
		fseek(dic,0,SEEK_END);
		int count = ftell(dic)/sizeof(struct Artini);
		rewind(dic);

		struct Artini list[count];
		fread(list,sizeof(struct Artini),count,dic);
		rewind(dic);
		for(int i=0;i<count;++i){
			if(list[i].code == oini.code){
			//更新的文章信息保存在目录顶端
				fwrite(&nini,sizeof(struct Artini),1,dic);
				for(int j=0;j<i;++j)
					fwrite(&list[j],sizeof(struct Artini),1,dic);
				for(int j=i+1;j<count;++j)
					fwrite(&list[j],sizeof(struct Artini),1,dic);
				break;
			}
		}
		fclose(dic);
		dic_stats[oini.bcode] = READY;
		return 1;
	}else{
		//在新目录增添nini条目
		while(dic_stats[nini.bcode] == BUSY);
		dic_stats[nini.bcode] = BUSY;

		//分配新的ArtCOde
		nini.code = getartcode(nini.bcode);

		FILE* dic = fopen(DIC_PATH[nini.bcode],"rb+");
		fseek(dic,0,SEEK_END);
		int count = ftell(dic)/sizeof(struct Artini);
		rewind(dic);

		struct Artini nlist[count+1];
		fread(nlist+1,sizeof(struct Artini),count,dic);
		rewind(dic);
		nlist[0] = nini;
		fwrite(nlist,sizeof(struct Artini),count+1,dic);
		fclose(dic);

		char buf[1000];
		int offset = 0;
		int readlen = 0;
		memset(buf,0,1000);
		while((readlen=readart(oini,buf,offset,1000)) > 0){
			createart(nini,buf,offset,readlen);
			offset+=readlen;
		}

		int cmtcount = 0;
		offset = 0;
		struct Cmtdat* cmt = (struct Cmtdat*)(malloc(sizeof(struct Cmtdat)));
		for(offset;offset<cmtcount;cmt = readcmt(oini.code,oini.bcode,offset,1,&cmtcount)){
			addcomment(nini.code,nini.bcode,*cmt);
		}
		free(cmt);

		dic_stats[nini.bcode] = READY;

		//删除原目录文件下的oini条目	
		while(dic_stats[oini.bcode] == BUSY);
		dic_stats[oini.bcode] = BUSY;

		deleteart(oini.code,oini.bcode);

		dic_stats[oini.bcode] = READY;

		return 1;
	}
}

char getisliked(ARTCODE acode,BLOCKCODE bcode,unsigned int userid){
	char likepath[MAX_PATH_LEN];
	char acodestr[11];
	memset(acodestr,0,11);
	inttostr(acodestr,acode);
	strcpy(likepath,DB_PATH[bcode]);
	strcpy(likepath+strlen(likepath),acodestr);
	strcpy(likepath+strlen(likepath),"/like");
	FILE* f_l;
	if((f_l = fopen(likepath,"rb+")) == NULL){
		f_l = fopen(likepath,"ab+");
	}
	fseek(f_l,0,SEEK_END);
	int c = ftell(f_l)/sizeof(unsigned int);
	rewind(f_l);
	unsigned int ids[c];
	fread(ids,sizeof(unsigned int),c,f_l);

	for(int i=0;i<c;++i){
		//检查该用户是否点过赞
		if(userid == ids[i]){
			fclose(f_l);
			return 1;
		}
	}
	return 0;
}

int minlike(ARTCODE acode,BLOCKCODE bcode,unsigned int userid){
	printf("addlike:%d\n",acode);
	char likepath[MAX_PATH_LEN];
	char acodestr[11];
	memset(acodestr,0,11);
	inttostr(acodestr,acode);
	strcpy(likepath,DB_PATH[bcode]);
	strcpy(likepath+strlen(likepath),acodestr);
	strcpy(likepath+strlen(likepath),"/like");
	FILE* f_l;
	if((f_l = fopen(likepath,"ab+")) == NULL){
		return -2;
	}
	fseek(f_l,0,SEEK_END);
	int c = ftell(f_l)/sizeof(unsigned int);
	rewind(f_l);
	unsigned int ids[c],nids[c-1];
	fread(ids,sizeof(unsigned int),c,f_l);

	char flag = 0;
	for(int i=0;i<c;++i){
		//检查该用户是否点过赞
		if(userid == ids[i]){
			flag = 1;
			for(int j=0;j<i;j++)
				nids[j] = ids[j];
			for(int j=i+1;j<c;j++)
				nids[j] = ids[j];
		}
	}
	if(flag == 0){
		fclose(f_l);
		return -1;
	}

	remove(likepath);
	f_l = fopen(likepath,"ab+");
	fwrite(nids,4,c-1,f_l);
	fclose(f_l);

	while(dic_stats[bcode] == BUSY);
	dic_stats[bcode] = BUSY;

	//更新目录中的点赞数
	FILE* dic = fopen(DIC_PATH[bcode],"rb+");
	fseek(dic,0,SEEK_END);
	int count = ftell(dic)/sizeof(struct Artini);
	rewind(dic);

	struct Artini list[count];
	fread(list,sizeof(struct Artini),count,dic);
	fclose(dic);
	for(int i=0;i<count;++i){
		if(list[i].code == acode){
			list[i].like -= 1;
			break;
		}
	}
	remove(DIC_PATH[bcode]);

	dic = fopen(DIC_PATH[bcode],"ab+");
	fwrite(list,sizeof(struct Artini),count,dic);
	fclose(dic);
	dic_stats[bcode] = READY;

	return 1;
}
int addlike(ARTCODE acode,BLOCKCODE bcode,unsigned int userid){
	printf("addlike:%d\n",acode);
	char likepath[MAX_PATH_LEN];
	char acodestr[11];
	memset(acodestr,0,11);
	inttostr(acodestr,acode);
	strcpy(likepath,DB_PATH[bcode]);
	strcpy(likepath+strlen(likepath),acodestr);
	strcpy(likepath+strlen(likepath),"/like");
	FILE* f_l;
	if((f_l = fopen(likepath,"ab+")) == NULL){
		return -2;
	}
	fseek(f_l,0,SEEK_END);
	int c = ftell(f_l)/sizeof(unsigned int);
	rewind(f_l);
	unsigned int ids[c];
	fread(ids,sizeof(unsigned int),c,f_l);

	for(int i=0;i<c;++i){
		//检查该用户是否点过赞
		if(userid == ids[i]){
			fclose(f_l);
			return -1;
		}
	}

	fseek(f_l,0,SEEK_END);
	fwrite(&userid,4,1,f_l);
	fclose(f_l);

	while(dic_stats[bcode] == BUSY);
	dic_stats[bcode] = BUSY;

	//更新目录中的点赞数
	FILE* dic = fopen(DIC_PATH[bcode],"rb+");
	fseek(dic,0,SEEK_END);
	int count = ftell(dic)/sizeof(struct Artini);
	rewind(dic);

	struct Artini list[count];
	fread(list,sizeof(struct Artini),count,dic);
	fclose(dic);
	for(int i=0;i<count;++i){
		if(list[i].code == acode){
			list[i].like += 1;
			break;
		}
	}
	remove(DIC_PATH[bcode]);

	dic = fopen(DIC_PATH[bcode],"ab+");
	fwrite(list,sizeof(struct Artini),count,dic);
	fclose(dic);
	dic_stats[bcode] = READY;

	return 1;
}

void inttostr(char* s,int l){
	for(int i=0;i<10;i++)
		s[i] = (l%(int)pow(10,10-i))/(int)pow(10,9-i) + 48;
	s[11]=0;
}

int readdv(int index,struct DailyVerse* dv){
	FILE* d_file = fopen(DV_PATH,"rb+");
	fseek(d_file,(index-1)*sizeof(struct DailyVerse),SEEK_SET);
	int res = fread(dv,sizeof(struct DailyVerse),1,d_file);
	fclose(d_file);
	return res;
}

int readdv_ann(struct DailyVerse* dv){
	FILE* d_file = fopen(DV_ANN_PATH,"rb+");
	int res = fread(dv,sizeof(struct DailyVerse),1,d_file);
	fclose(d_file);
	return res;
}

//=======================日志系统=======================
void writelog(char* format,...){
	char ifmt[strlen(format)+1];
	memset(ifmt,0,sizeof(ifmt));
	strcpy(ifmt,format);
	strcpy(ifmt+strlen(format),"\n");
	time_t t = time(0);
	char ts[32];
	strftime(ts,sizeof(ts),"%Y-%m-%d",localtime(&t));

	char logpath[MAX_PATH_LEN];
	memset(logpath,0,MAX_PATH_LEN);
	strcpy(logpath,LOG_PATH);
	strcpy(logpath+strlen(logpath),ts);

	FILE* logfile = fopen(logpath,"ab+");
	fseek(logfile,0,SEEK_END);
	memset(ts,0,sizeof(ts));
	strftime(ts,sizeof(ts),"%H:%M:%S\t",localtime(&t));
	fputs(ts,logfile);

	char info[1024];
	memset(info,0,1024);
	va_list list;
	va_start(list,ifmt);
	vsprintf(info,ifmt,list);
	va_end(list);
	fputs(info,logfile);
	fclose(logfile);
}

void throwex(char *format,...){
	time_t t = time(0);
	char ts[32];
	strftime(ts,sizeof(ts),"%Y-%m-%d-%H:%M:%S",localtime(&t));

	char logpath[MAX_PATH_LEN];
	memset(logpath,0,MAX_PATH_LEN);
	strcpy(logpath,LOG_PATH);
	strcpy(logpath+strlen(logpath),ts);

	FILE* logfile = fopen(logpath,"ab+");
	fseek(logfile,0,SEEK_END);

	char info[1024];
	memset(info,0,1024);
	va_list list;
	va_start(list,format);
	vsprintf(info,format,list);
	va_end(list);
	fputs(info,logfile);
	fclose(logfile);
}