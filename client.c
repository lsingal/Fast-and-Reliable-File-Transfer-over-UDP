#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FILENAME "textfile.txt"
#define FILEPATH_C "textfile_back.txt"
#define MAXSEND 20
#define PAYLOAD 1400


pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


int filesize = 0;
int errno= -1;
char *data;
char *allNodes;

struct sockaddr_in serv_addr;
struct stat statbuf;
int iteratingptr;

//off_t filesize;
int totalPacketsArrived =0 , totalSeq=0;

struct timespec start , stop;
double totaltime;

typedef struct dgram {
    uint32_t seqNum;                /* store sequence # */
    uint32_t ts;                    /* time stamp */
    uint32_t dataSize;              /* datasize , normally it will be 1460*/
    char buf[PAYLOAD];            /* Data buffer */
    int retransmit;
}dgram;

pthread_t thrd[1];
int sockfd;

void error(const char *msg)
{
    perror(msg);
    exit(0);
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

void* receiveMe(void* arg){
    int n=0;// , i = 0;
 //   LinkedList *requestNodePointer = anchor;
	//int ack = -1;
    int requestIndex;
    printf("enter receiveMe server\n");
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
void *checkme(void* arg){
    int totalseq , lefbyte , lastseq , size_p,n=0;
    dgram sendDG ,  recvDG;
    int packetmiss,errno;
    socklen_t servlen;
    servlen = sizeof(serv_addr);
    
    printf("enter check me client\n");
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
        totalseq =  filesize / PAYLOAD;
        lefbyte = filesize % PAYLOAD;
        //
        size_p = PAYLOAD;
        if(lefbyte != 0 && totalseq ==  packetmiss)
        {
        	size_p = lefbyte;
        }
        pthread_mutex_lock(&lock);
        
        memcpy( sendDG.buf , &data[packetmiss*PAYLOAD] , size_p );
        pthread_mutex_unlock(&lock);
        
        sendDG.seqNum = packetmiss;
        sendDG.dataSize = size_p;
        sendDG.retransmit = 1;
        
        n = sendto(sockfd,&sendDG,sizeof(dgram), 0,(struct sockaddr *) &serv_addr,servlen);
        if (n < 0)
            error("sendto");
        
    }
    
}


int main(int argc, char *argv[])
{
	int count = 0;
	int cnt = 0;
	int rdbytes = 0;
	unsigned char buffer[PAYLOAD];
	unsigned char* chptr = NULL;
	struct stat st;
    int seq = 0 , datasize =0 ;//, filesize =0;
    socklen_t fromlen;
    int ack = -1;
    dgram recvDG;

    
    //char *data;
    
    FILE* fp = NULL;
    
    int portno, n;
    struct hostent *server;
    int i , j;
    
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,server->h_length);
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
    
    
    
	if ((chptr = memset(buffer, '\0', PAYLOAD)) == NULL)
	{
        error("memset:");
	}
    
	if ((fp = fopen(FILENAME, "r")) == NULL)
	{
        error("fopen:");
	}
	
	if(stat(FILENAME,&st)==0)
	{
        filesize=st.st_size;
        printf("The size of this file is %d.\n", filesize);
	}

    
    
    for(i=0 ; i<10 ; i++){
        n = sendto(sockfd,&filesize,sizeof(filesize), 0,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
        if (n < 0)
            error("sendto");
    }
    pthread_mutex_lock(&lock);
    
    data = openFILE(FILENAME);
    datasize = statbuf.st_size;
    
    pthread_mutex_unlock(&lock);
    
    if((errno = pthread_create(&thrd[0], 0, checkme , (void*)0 ))){
        fprintf(stderr, "pthread_create[0] %s\n",strerror(errno));
        pthread_exit(0);
    }
    
    if(clock_gettime(CLOCK_REALTIME , &start) == -1){
        error("clock get time");
    }
    
    //   while (fgets(buffer ,PAYLOAD , fp) != NULL ) {
    while (datasize > 0) {
        int chunk , share;
        dgram sendDG;
        memset(&sendDG , 0 , sizeof(dgram));
        
        share = datasize;
        chunk = PAYLOAD;
        
        if(share - chunk < 0){
            chunk = share;
        } else {
            share = share - chunk;
        }
        pthread_mutex_lock(&lock);
        
        memcpy(sendDG.buf , &data[seq*PAYLOAD] , chunk);
        
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
    //printf("join");
    
    pthread_join(thrd[0], 0);
    
/*    if(clock_gettime(CLOCK_REALTIME,&stop) == -1){
        error("clock get time stop");
    }
    totaltime = (stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec - start.tv_nsec)/1e9;
    
    printf("Time taken to complete : %f sec",totaltime);
    */
    //fclose(fp);
    /* unmap the data */
    munmap(data, statbuf.st_size);
    
    /*--------------------------------------------- start recv ---------------------------------*/
/* recv filesize */
    fromlen = sizeof(struct sockaddr_in);
  /*  n = recvfrom(sockfd,&filesize,sizeof(filesize),0,(struct sockaddr *)&serv_addr,&fromlen);*/
    printf("filesize %d",filesize);
    totalSeq = ceil(filesize / PAYLOAD);
	printf("Seqtotal:: %d",totalSeq);
    
    fp = fopen(FILEPATH_C , "w+");
    
    if((errno = pthread_create(&thrd[0], 0, receiveMe , (void*)0 ))){
        fprintf(stderr, "pthread_create[0] %s\n",strerror(errno));
        pthread_exit(0);
    }
    
    allNodes = calloc(totalSeq , sizeof(char));

 /*   if(clock_gettime(CLOCK_REALTIME , &start) == -1){
        error("clock get time");
    }
*/
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
			
            fseek( fp , PAYLOAD*recvDG.seqNum , SEEK_SET  );
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

    pthread_join(thrd[0], 0);
    
    if(clock_gettime(CLOCK_REALTIME,&stop) == -1){
        error("clock get time stop");
    }
    totaltime = (stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec - start.tv_nsec)/1e9;
    
    printf("Time taken to complete : %f sec",totaltime);
    
    munmap(data, statbuf.st_size);
    
    fclose(fp);
    close(sockfd);
    return 0;
}
