/* A simple server in the internet domain using TCP
 The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define FILEPATH "textref.txt"
#define MAXBUFSIZE 1400
#define MAXSEND 20
#define false 0
#define true 1

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


typedef struct dgram {
    uint32_t seqNum;                /* store sequence # */
    uint32_t ts;                    /* time stamp */
    uint32_t dataSize;              /* datasize , normally it will be 1460*/
    char buf[MAXBUFSIZE];            /* Data buffer */
    int retransmit;
}dgram;

char *allNodes;
int iteratingptr;
char *data;
struct stat statbuf;

struct timespec start , stop;
double totaltime;

pthread_t thrd[1];
int datarecv = 0;
int filesize;
int sockfd;
struct sockaddr_in serv_addr;

int totalPacketsArrived =0 , totalSeq=0;

//LinkedList * anchor;


void error(const char *msg)
{
    perror(msg);
    exit(1);
}


void *startSendingBack(void* arg){
    int totalseq , lefbyte , lastseq , size_p,n=0;
    dgram sendDG;
    int packetmiss,errno;
    socklen_t servlen;
    
    servlen = sizeof(serv_addr);
    
    printf("enter startSendingBack client\n");
    //fflush(stdout);
    // sleep(10);
    while (1) {
        
        n = recvfrom(sockfd,&packetmiss,sizeof(int),0,(struct sockaddr *)&serv_addr,&servlen);
        //    printf("packetmiss:: %d\n",packetmiss);
        if(n<0){
            error("rcv from\n");
        }
        if( packetmiss < 0){
            printf("data recv fully");
            pthread_exit(0);
        }
        totalseq =  filesize / MAXBUFSIZE;
        lefbyte = filesize % MAXBUFSIZE;
        //
        size_p = MAXBUFSIZE;
        if(lefbyte != 0 && totalseq ==  packetmiss)
        {
        	size_p = lefbyte;
        }
        pthread_mutex_lock(&lock);
        
        memcpy( sendDG.buf , &data[packetmiss*MAXBUFSIZE] , size_p );
        pthread_mutex_unlock(&lock);
        
        sendDG.seqNum = packetmiss;
        sendDG.dataSize = size_p;
        sendDG.retransmit = 1;
        
        n = sendto(sockfd,&sendDG,sizeof(dgram), 0,(struct sockaddr *) &serv_addr,servlen);
        if (n < 0)
            error("sendto");
        
    }
    
}

char *openFILE(char *str){
    int fp;
    int pagesize;
    char *data;
    
    fp = open(str, O_RDONLY);
    if(fp < 0 ){
        error("Error: while opening the file\n");
    }
    
    /* get the status of the file */
    if(fstat(fp,&statbuf) < 0){
        error("ERROR: while file status\n");
    }
    
    /* mmap it , data is a pointer which is mapping the whole file*/
    data = mmap((caddr_t)0, statbuf.st_size , PROT_READ , MAP_SHARED , fp , 0 ) ;
    
    if(data == MAP_FAILED ){
        error("ERROR : mmap \n");
    }
    
    return data;
    
}


int iterateArrayAgain(){
	int i;
	if( allNodes == NULL ){
		return -1;
	}
	for ( i = iteratingptr; i <= totalSeq ; i++)
	{
		if (allNodes[i] == 0 )
		{
			if (i==totalSeq)
				iteratingptr=0;
			else
				iteratingptr=(i+1);
            return i;
		}
	}
	iteratingptr=0;
	return -1;
    
}

void* checkme(void* arg){
    int n=0;// , i = 0;
    
  //  LinkedList *requestNodePointer = anchor;
	//int ack = -1;
    int requestIndex;
    printf("enter check me server\n");
    while(1)
    {
        if(totalPacketsArrived == totalSeq+1 ) {
            printf("all data recved!!\n");
            pthread_exit(0);
        }
        usleep(100);
        pthread_mutex_lock(&lock);
        requestIndex = iterateArrayAgain();
        pthread_mutex_unlock(&lock);
        if( requestIndex < 0){
            // sleep(1);
        }
        else if(requestIndex >= 0 && requestIndex<=totalSeq){
            n = sendto(sockfd, &requestIndex ,sizeof(int),0,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
            
            if (n < 0)
                error("sendto\n");
        }
        
    }
}

int deleteNodesFromArray(char **arrayNode , int seqNum){
    
	if( seqNum >=0 && seqNum <=totalSeq){
        if( allNodes[seqNum] == 0 ){
			allNodes[seqNum] = 1;
			return 1;
		} else {
			return 0;
		}
	}
    return 0;
}


int main(int argc, char *argv[])
{
    int portno,i=0;
    socklen_t fromlen;
    int n;
    FILE *fp;
    int errno = 0 ;
    int last = -1 , seq=0;
    //int diff=0;
    
    dgram recvDG;
    int ack = -1;
    
    int datasize = 0;
   // anchor=NULL;
    
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    long int sndsize = 50000000;
    
    if(setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndsize, (long int)sizeof(sndsize)) == -1)
    {
        printf("error with setsocket");
    }
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&sndsize, (long int)sizeof(sndsize)) == -1)
    {
        printf("error with setsocket");
        
    }
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
	fromlen = sizeof(struct sockaddr_in);
    /* prerequisite */
    n = recvfrom(sockfd,&filesize,sizeof(filesize),0,(struct sockaddr *)&serv_addr,&fromlen);
    
    totalSeq = ceil(filesize / MAXBUFSIZE);
	printf("Seqtotal:: %d",totalSeq);
    
    fp = fopen(FILEPATH , "w+");
    
    if((errno = pthread_create(&thrd[0], 0, checkme , (void*)0 ))){
        fprintf(stderr, "pthread_create[0] %s\n",strerror(errno));
        pthread_exit(0);
    }
    
    allNodes = calloc(totalSeq , sizeof(char));
    
    /*  last = totalSeq;
     for (i=last;i>=0;i--)
     addNodes(&anchor,i);*/
    
    if(clock_gettime(CLOCK_REALTIME , &start) == -1){
        error("clock get time");
    }
    
    while (1)
    {
		//fflush(stdout);
		//printNodes(&anchor);
        
        n = recvfrom(sockfd,&recvDG,sizeof(dgram),0,(struct sockaddr *)&serv_addr,&fromlen);
        
        if( n == sizeof(int) ){
            continue;
		}
        if (n < 0)
            error("recvfrom");
        
        
        pthread_mutex_lock(&lock);
        if(deleteNodesFromArray( &allNodes , recvDG.seqNum)){
			
            fseek( fp , MAXBUFSIZE*recvDG.seqNum , SEEK_SET  );
            fwrite(&recvDG.buf , recvDG.dataSize , 1 , fp);
            fflush(fp);
            totalPacketsArrived++;
        } else {
        }
        pthread_mutex_unlock(&lock);
    if(totalPacketsArrived == totalSeq+1 ){
        printf("we got the whole file!! wow\n");
        for(i=0 ; i<MAXSEND ; i++){
            printf("sending ack\n");
            n = sendto(sockfd,&ack,sizeof(int), 0,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
            if (n < 0)
                error("sendto");
        }
        
        break;
    }
}
    /*
if(clock_gettime(CLOCK_REALTIME,&stop) == -1){
    error("clock get time stop");
}*/

//totaltime = (stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec - start.tv_nsec)/1e9;

//printf("Time taken to complete : %f sec",totaltime);

pthread_join(thrd[0], 0);

fclose(fp);
/*-----------------------------------starts sending back-------------------*/

/*memset(&start , 0 ,sizeof(timespec));
memset(&stop , 0 , sizeof(timespec));
*/
/* send the filesize */

pthread_mutex_lock(&lock);

data = openFILE(FILEPATH);
datasize = statbuf.st_size;

pthread_mutex_unlock(&lock);

    usleep(100);
    printf("filesize while sending back data: %d",filesize);
/*    for(i=0 ; i<10 ; i++){
        n = sendto(sockfd,&filesize,sizeof(filesize), 0,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
        if (n < 0)
            error("sendto");
    }
*/
if((errno = pthread_create(&thrd[0], 0, startSendingBack , (void*)0 ))){
    fprintf(stderr, "pthread_create[0] %s\n",strerror(errno));
    pthread_exit(0);
}

/*if(clock_gettime(CLOCK_REALTIME , &start) == -1){
    error("clock get time");
}*/

while (datasize > 0) {
    int chunk , share;
    dgram sendDG;
    memset(&sendDG , 0 , sizeof(dgram));
    
    share = datasize;
    chunk = MAXBUFSIZE;
    
    if(share - chunk < 0){
        chunk = share;
    } else {
        share = share - chunk;
    }
    pthread_mutex_lock(&lock);
    
    memcpy(sendDG.buf , &data[seq*MAXBUFSIZE] , chunk);
    
    pthread_mutex_unlock(&lock);
    
    sendDG.seqNum = seq;
    sendDG.dataSize = chunk;
    sendDG.retransmit = 0;
    // printf("SeqNum= %d\n",sendDG.seqNum);
    usleep(100);
    sendto(sockfd , &sendDG , sizeof(sendDG) , 0 , (struct sockaddr *) &serv_addr,sizeof(serv_addr));
    seq++;
    datasize -= chunk;
    
}

pthread_join(thrd[0], 0);

if(clock_gettime(CLOCK_REALTIME,&stop) == -1){
    error("clock get time stop");
}
totaltime = (stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec - start.tv_nsec)/1e9;

printf("Time taken to complete : %f sec",totaltime);

munmap(data, statbuf.st_size);

close(sockfd);
return 0;
}




