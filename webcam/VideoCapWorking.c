/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include<math.h>
#include <getopt.h>             /* getopt_long() */
#include<pthread.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include"avimfmt.h"
#include <asm/types.h>          /* for videodev2.h */
#include <alsa/asoundlib.h>
#include <linux/videodev2.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include<time.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

struct WavHeader {
	char riff[4];
	int file_size;
	char wave[4];
	char fmt[4];
	int fmt_len;
	short format;
	short channels;
	int sample_rate;
	int bytes_per_sec;
	short bytes_per_sam;
	short bits_per_sam;
	char data[4];
	int data_size;
};

typedef enum {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
} io_method;

struct buffer {
	void *                  start;
	size_t                  length;
};

struct threadargs
{
	int fd;
  int socket_fd;
};

struct jpg_image {
  unsigned int size;
  unsigned char* data;
};

struct socket_data {
  int size;
  unsigned char* data;
};

struct image_transport_header {
  unsigned int size;
  unsigned int sequence_no;
};

struct FrameTime {
	struct timespec time_stamp;
	int audio_file_num;
};

struct command_info {
  char id[4];
  unsigned int command;
  unsigned char payload[32];
};

struct audio_frame_info {
	struct timespec capture_start;
	struct timespec capture_end;
	int buffer_index;
};
#define CAPTURE_SECS 300
#define AUDIO_BUFFER_SIZE 1
#define MAX_SAMPLE_RATE 48000
#define MIN_FRAME_RATE 7
#define CAP_MIN_FRAME CAPTURE_SECS*MIN_FRAME_RATE
#define MAX_FRAME_TIME CAPTURE_SECS / CAP_MIN_FRAME
#define MAX_BYTES_PER_SAMPLE 2
//#define CAPTURE_AUDIO_BYTES CAPTURE_SECS*MAX_SAMPLE_RATE*MAX_BYTES_PER_SAMPLE
#define CAPTURE_AUDIO_BYTES 60*MAX_SAMPLE_RATE*MAX_BYTES_PER_SAMPLE
#define NUM_AUDIO_BUFFERS CAPTURE_AUDIO_BYTES / AUDIO_BUFFER_SIZE
#define NUM_AUDIO_BUFFERS_EXTRA (120*NUM_AUDIO_BUFFERS) / 100
#define MAX_FRAME_EST CAPTURE_SECS*60
#define FRAMES_IN_AVI 35*CAPTURE_SECS
#define BILLION  1000000000L
#define COMMBINE_SEG_COUNT 10
int bytes_per_sample = 0;
int av_file_number = 0;
FILE* av_main_file_name = NULL;
const int num_audio_buffers = (int)NUM_AUDIO_BUFFERS_EXTRA;
struct audio_frame_info audio_frame[NUM_AUDIO_BUFFERS_EXTRA];
unsigned int audio_frame_inx = 0;
struct timespec previousEntryTime, entryTime;
struct timespec vidCapStart, vidCapNow, mainCapTime, mainCapTimeStart;
struct timespec audioCapStart, audioCapNow;
int done_capture = 0;
static char *           dev_name = NULL;
static io_method        io = IO_METHOD_MMAP;
static int              fd = -1;
static int frameNumber = 0;
static int fileNumber = 0;
static int framesInFile = 0;
unsigned int chuckSizeCum = 4;
int fileSizeOffset = 0;
int moviListSizeOffset = 0;
int totalFrameOffset = 0;
int streamLengthOffset = 0;
int frameRateOffset = 0;
int frameTimeOffset = 0;
double meanFrameTime = 0.0;
double t_last = 0.0;
FILE* timingFile = NULL;
int hres = 920;
int vres = 540;
const int captureTimeInSecs = (int)CAPTURE_SECS;
const int capture_frame_count = (int)MAX_FRAME_EST;

struct FrameTime frame_timestamps[MAX_FRAME_EST];

int filesCaptured = 0;
int audio_file_index = 0;

static char streamId[] = { '0', '0', 'd', 'c' };
static char idx[] = { 'i', 'd', 'x', '1' };
FILE* currentAVIFile = NULL;
FILE* currentWAVFile = NULL;
struct IndxBlock indexBlock[FRAMES_IN_AVI];
struct buffer *         buffers = NULL;
static unsigned int     n_buffers = 0;
struct timespec first_frame_time;
int first_pts_timestamp = 0;
//struct timepsec vidCapStart;
int find_audio_device();
void* combine_video_and_audio(void *ptr);
void* video_capture_thread(void *ptr);
void* audio_capture_thread(void *ptr);
int first_entry = 1;
char device_bus_info[32];
char working_directory[512];
char audio_working_directory[512];
char video_working_directory[512];
char av_working_directory[512];
int new_video_file = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int new_audio_file = 0;
pthread_mutex_t aud_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t aud_cond = PTHREAD_COND_INITIALIZER;
int stream_write_pipe = 0;

// streaming socket info
char live_ip_address[256];
char live_port_no[256];
char disk_ip_address[256];
char disk_port_no[256];
char camera_id[256];
unsigned short stream_flag[2];
const unsigned char webcam_stream_marker[] = {0xff,0x00,0xfe,0x01,0xfd,0x02,0xfc,0x03};
const unsigned int webcam_stream_marker_len = sizeof(webcam_stream_marker);

int socket_send(const char* ip_address,
                     const char* portNo,
                     const char* message_str,
                     unsigned int message_len)
{
  int sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buffer[512];

  portno = atoi(portNo);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("ERROR opening socket");
    return 0;
  }
  server = gethostbyname(ip_address);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    close(sockfd);
    return 0;
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
    (char *)&serv_addr.sin_addr.s_addr,
    server->h_length);
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
    perror("ERROR connecting");
    close(sockfd);
    return 0;
  }

  n = write(sockfd,message_str,message_len);


  if (n < 0) {
    perror("ERROR writing to socket");
    close(sockfd);
    return 0;
  }

  if(shutdown(sockfd,SHUT_WR) != 0)
  {
    perror("ERROR shutting down socket");
    close(sockfd);
    return 0;
  }
  
 /* memset(buffer,0,sizeof(buffer));

  n = read(sockfd,buffer,511);

  if (n < 0) {
    perror("ERROR reading from socket");
    close(sockfd);
    return 0;
  }*/
  //printf("READ: %s\n",buffer);
  close(sockfd);
  return 1;
}

void *socket_receive(void *p)
{
   struct threadargs * thread_data = (struct threadargs *)p;
	int portno = thread_data->fd;

  int sockfd, newsockfd;
  socklen_t clilen;
  const unsigned int buffsize = 1024;
  char buffer[buffsize];

  struct sockaddr_in serv_addr, cli_addr;
  int n;

  //portno = 17213;
  //portno = (int)atoi(camera_port_no.c_str());  
  //portno += 500;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) 
    perror("ERROR opening socket");

  bzero((char *) &serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    perror("ERROR on binding");

  listen(sockfd,5);
  clilen = sizeof(cli_addr);
    
  const char* ping_msg = "Hello from Webcam";

  while(1)
  {
    newsockfd = accept(sockfd, 
      (struct sockaddr *) &cli_addr, 
      &clilen);

    if (newsockfd < 0) 
      perror("ERROR on accept");

    

    int data_count = 0;

    while(1) {      
      n = read(newsockfd,buffer+data_count,buffsize-data_count);

      if(n <= 0)
        break;

      data_count += n;

      if(data_count >= buffsize)
        break;
    }

    printf("DATA SIZE: %d\n",data_count);

    if (n < 0) 
      perror("ERROR reading from socket");

    if(data_count == sizeof(struct command_info))
    {
      struct command_info command;
      memcpy(&command,buffer,data_count);
      char cmd[5];
      memset(cmd,0,sizeof(cmd));
      memcpy(cmd,command.id,4);

      printf("Command: '%s'\n",cmd);

      switch(command.command)
      {
      case 0:
        stream_flag[0] = 0;
        stream_flag[1] = 0;
        printf("live and disk stream disabled.\n");
        break;
      case 1:
        stream_flag[0] = 1;
        stream_flag[1] = 0;
        printf("live: enabled, disk: disabled.\n");
        break;
      case 2:
        stream_flag[0] = 0;
        stream_flag[1] = 1;
        printf("live: disabled, disk: enabled.\n");
        break;
      case 3:
        stream_flag[0] = 1;
        stream_flag[1] = 1;
        printf("live: enabled, disk: enabled.\n");
        break;
      case 4:
        printf("live: %u, disk: %u.\n",(unsigned int)stream_flag[0],(unsigned int)stream_flag[1]);
        n = write(newsockfd,&stream_flag,sizeof(stream_flag));
        break;
      case 11:
        stream_flag[0] = 1;        
        printf("live: enabled.\n");
        n = write(newsockfd,camera_id,strlen(camera_id));
        break;
      case 12:        
        stream_flag[1] = 1;
        printf("disk: enabled.\n");
        n = write(newsockfd,camera_id,strlen(camera_id));
        break;
      default:
        printf("id: %s\n",camera_id);
        n = write(newsockfd,camera_id,strlen(camera_id));
        break;
      }
    }

    close(newsockfd);

  }

  close(sockfd);
}

void *socket_transmit(void *p)
{
  struct threadargs * thread_data = (struct threadargs *)p;
	int fd = thread_data->fd;
  const char* ip_address = "192.168.1.14";
  const char* port_no = "47016";
  unsigned int block_count = 0;
  //static const char* frame_signature = "*$FRAME$*";
  while(1)
  {
    struct socket_data *dat;
		if (read(fd, &dat, sizeof(dat)) != sizeof(dat)) {
			printf("read stream error\n");
			exit(1);
		}
    unsigned char* data_ptr = dat->data;
    int buffer_size = dat->size + webcam_stream_marker_len;
    unsigned char* temp_buffer = (unsigned char*)malloc(buffer_size);

    memcpy(temp_buffer,webcam_stream_marker,webcam_stream_marker_len);
    memcpy(temp_buffer+webcam_stream_marker_len,data_ptr,dat->size);

    struct image_transport_header* header = (struct image_transport_header*)data_ptr;    
    //printf("%d...%u First, size %u\n",buffer_size,header->sequence_no,header->size);
    //if(block_count == 0)
    //{
    //  FILE* dump = NULL;
    //  dump = fopen("/home/gary/dump.bin","wb");
    //  //fwrite(&yuv_image[0],1,yuv_image.size(),dump);
    //  fwrite(dat->data,1,dat->size,dump);
    //  fclose(dump);
    //}
    
    block_count++;
    if(stream_flag[0] == 1) {
      socket_send(live_ip_address,live_port_no,(const char*)temp_buffer,(unsigned int)buffer_size);
    }

    if(stream_flag[1] == 1) {
      socket_send(disk_ip_address,disk_port_no,(const char*)temp_buffer,(unsigned int)buffer_size);
    }

    free(dat->data);
    free(temp_buffer);
    free(dat);
  }
}

void *streamer(void *p)
{  
	int i;
  struct threadargs * thread_data = (struct threadargs *)p;
	int fd = thread_data->fd;
  int soc_wr_fd = thread_data->socket_fd;
  printf("ready for streaming\n");
  unsigned int image_count = 0;
  time_t start_time = time(0);
  time_t current_time = start_time;
  time_t buffer_start_time = time(0);
  //time_t buffer_current_time = start_time;
  double time_delta = 0.0;
  double time_delta_buffer = 0.0;
  unsigned int total_data = 0;
  const int max_buffer_size = 1024*1024*2;
  const int image_transport_header_size = sizeof(struct image_transport_header);
  unsigned char* transport_buffer_a = (unsigned char*)malloc(max_buffer_size);
  unsigned char* transport_buffer_b = (unsigned char*)malloc(max_buffer_size);

  int buffer_pos[2];
  //memset(&buffer_pos,0,sizeof(buffer_pos));
  buffer_pos[0] = 0;
  buffer_pos[1] = 0;

  int current_buffer = 0;
  int bytes_remaining_in_buf = max_buffer_size;
  int payload_size = 0;
  unsigned int seq_no = 0;
  //image_transport_header
  while(1)
  {
    struct jpg_image *img;
		if (read(fd, &img, sizeof(img)) != sizeof(img)) {
			printf("read stream error\n");
			exit(1);
		}
    seq_no++;
    total_data += img->size;
    image_count++;

    current_time = time(0);
    //buffer_current_time = current_time;
    time_delta = difftime(current_time,start_time);
    time_delta_buffer = difftime(current_time,buffer_start_time);
    if(time_delta >= 10.0)
    {
      float data_rate = (float)total_data / 10.0f;
      float frame_rate = (float)image_count / 10.0f;

      printf("Data rate %fkB/s, frame rate %f fps\n",data_rate/1000.0f,frame_rate);
      start_time = current_time;
      total_data = 0;
      image_count = 0;
      //printf("%u Image size %u\n",image_count,img->size);
    }

    payload_size = img->size + image_transport_header_size;

    bytes_remaining_in_buf = max_buffer_size - buffer_pos[current_buffer];
  
    if((payload_size > bytes_remaining_in_buf) ||
      (time_delta_buffer >= 1.0))
    {
      // send current buffer
      if(buffer_pos[current_buffer] > 0)
      {
        //printf("BUFFER %u\n",(unsigned int)current_buffer);
        struct socket_data *dat = (struct socket_data *)malloc(sizeof(struct socket_data));
        dat->size = buffer_pos[current_buffer];

        unsigned char* send_buf = (unsigned char*)malloc(dat->size);
        //dat->size = 9;
        
        
        if(current_buffer == 0) {
          //dat->data = transport_buffer_a;
          memcpy(send_buf,transport_buffer_a,dat->size);
        }
        else {
          //dat->data = transport_buffer_b;
          memcpy(send_buf,transport_buffer_b,dat->size);
        }

        dat->data = send_buf;
        int rc = write(soc_wr_fd, &dat, sizeof(dat));
      }
      buffer_pos[current_buffer] = 0;
      buffer_start_time = current_time;
      if(current_buffer == 0)
        current_buffer = 1;
      else
        current_buffer = 0;
    }
    unsigned char* buf_ptr = NULL;

    if(current_buffer == 0)
      buf_ptr = transport_buffer_a + buffer_pos[current_buffer];
    else
      buf_ptr = transport_buffer_b + buffer_pos[current_buffer];

    struct image_transport_header* header = (struct image_transport_header*)buf_ptr;
    header->size = img->size;
    header->sequence_no = seq_no;
    buf_ptr += image_transport_header_size;
    memcpy(buf_ptr,img->data,img->size);
    buffer_pos[current_buffer] += payload_size;
    free(img->data);
    free(img);    
  }

  free(transport_buffer_a);
  free(transport_buffer_b);
}

struct timespec TimeDiff(struct timespec start, struct timespec end)
{
	struct timespec temp;
	if ((end.tv_nsec - start.tv_nsec) < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	}
	else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp;
}
static inline void time_difference(struct timespec* start, struct timespec* end, struct timespec* tempDiff) {
	double t_s = (double)start->tv_sec + (double)start->tv_nsec*1.0e-9;
	double t_e = (double)end->tv_sec + (double)end->tv_nsec*1.0e-9;

	double diff = t_s - t_e;
	tempDiff->tv_sec = (long)diff;
	diff -= (double)tempDiff->tv_sec;
	diff *= 1.0e9;
	tempDiff->tv_nsec = (long)diff;
}

static inline void TimeDiffP(struct timespec* start, struct timespec* end, struct timespec* tempDiff)
{
	if ((end->tv_nsec - start->tv_nsec) < 0) {
		tempDiff->tv_sec = end->tv_sec - start->tv_sec - 1;
		tempDiff->tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
	}
	else {
		tempDiff->tv_sec = end->tv_sec - start->tv_sec;
		tempDiff->tv_nsec = end->tv_nsec - start->tv_nsec;
	}
}

static void
errno_exit(const char *           s)
{
	fprintf(stderr, "%s error %d, %s\n",
		s, errno, strerror(errno));

	exit(EXIT_FAILURE);
}

static int
xioctl(int                    fd,
int                    request,
void *                 arg)
{
	int r;

	do r = ioctl(fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}


static void process_image2(const void* p, int len) {
	clock_gettime(CLOCK_REALTIME, &entryTime);
	memcpy((void*)&vidCapNow, (const void*)&entryTime, sizeof(struct timespec));
	if (fileNumber > 0) {
		struct timespec timeDiff;
		TimeDiffP(&previousEntryTime, &entryTime, &timeDiff);
		double frameTime = (double)timeDiff.tv_nsec;
		frameTime /= 1e9;
		meanFrameTime += frameTime;
	}
	memcpy((void*)&previousEntryTime, (const void*)&entryTime, sizeof(struct timespec));

	if (currentAVIFile == NULL) {
		char file_name[512];
		memcpy(file_name, video_working_directory, sizeof(file_name));
		int file_name_base_len = strlen(file_name);
		sprintf(file_name + file_name_base_len, "video%d.avi", fileNumber);
		//char fileName[] = "//home//gary//testvid//video0//videoj0000.avi\0";
		/*char fileName[] = "/home/gary/testvid/video0/videoj0000.avi\0";
		 *
		 *    int devNameLen = strlen(dev_name);
		 *
		 *    int fileNameLen = strlen(fileName);
		 *
		 *    memcpy((void*)(fileName+(fileNameLen-21)),(const void*)(dev_name+(devNameLen-6)),6);
		 *
		 *    char str[256];
		 *    memset((void*)str,'0',256);
		 *    sprintf(str,"%d",fileNumber);
		 *    if(fileNumber < 10)
		 *      memcpy((void*)(fileName+(fileNameLen-5)),(const void*)str,1);
		 *    else if(fileNumber < 100)
		 *      memcpy((void*)(fileName+(fileNameLen-6)),(const void*)str,2);
		 *    else if(fileNumber < 1000)
		 *      memcpy((void*)(fileName+(fileNameLen-7)),(const void*)str,3);
		 *    else
		 *      memcpy((void*)(fileName+(fileNameLen-8)),(const void*)str,4);*/

		fileNumber++;
		currentAVIFile = fopen(file_name, "wb");
		struct LargeAVIHeader largeAVIHeader;
		char tempFileName[] = "/home/gary/testvid/template/aviHeaderTemplate.hd0";
		FILE* tempFile = fopen(tempFileName, "rb");
		fread((void*)&largeAVIHeader, sizeof(struct LargeAVIHeader), 1, tempFile);
		fclose(tempFile);
		largeAVIHeader.riffHeader.bitmapInfoHeader.biWidth = hres;
		largeAVIHeader.riffHeader.bitmapInfoHeader.biHeight = vres;

		largeAVIHeader.riffHeader.aviHeader.dwWidth = hres;
		largeAVIHeader.riffHeader.aviHeader.dwHeight = vres;


		largeAVIHeader.riffHeader.aviStreamHeader.rcFrame.right = (short int)hres;
		largeAVIHeader.riffHeader.aviStreamHeader.rcFrame.bottom = (short int)vres;

		fwrite((void*)&largeAVIHeader, sizeof(struct LargeAVIHeader), 1, currentAVIFile);
		unsigned char* s1 = (unsigned char*)(&largeAVIHeader);
		unsigned char* p1 = (unsigned char*)(&largeAVIHeader.moviList.moviListSize);
		unsigned char* p2 = (unsigned char*)(&largeAVIHeader.riffHeader.fileSize);
		unsigned char* p3 = (unsigned char*)(&largeAVIHeader.riffHeader.aviHeader.dwTotalFrames);
		unsigned char* p4 = (unsigned char*)(&largeAVIHeader.riffHeader.aviStreamHeader.dwLength);

		unsigned char* p5 = (unsigned char*)(&largeAVIHeader.riffHeader.aviHeader.dwMicroSecPerFrame);
		unsigned char* p6 = (unsigned char*)(&largeAVIHeader.riffHeader.aviStreamHeader.dwRate);


		fileSizeOffset = p2 - s1;
		moviListSizeOffset = p1 - s1;
		totalFrameOffset = p3 - s1;
		streamLengthOffset = p4 - s1;

		frameRateOffset = p6 - s1;
		frameTimeOffset = p5 - s1;
		memcpy((void*)&vidCapStart, (const void*)&entryTime, sizeof(struct timespec));
		chuckSizeCum = 4;
	}

	frameNumber++;
	int chuckSize = 0;

	unsigned short* data = (unsigned short*)p;
	int i = 0;

	/*for(;;) {
	 *    chuckSize++;
	 *    if(*data == 0xd9ff)
	 *      break;
	 *
	 *    data++;
	 }

	 chuckSize *= sizeof(unsigned short);*/
	/*if(len%2 != 0)
	 *    printf("fruity length!\n");*/

	chuckSize = len;
	//printf("dataCount calc: %d\n",chuckSize);

	if (currentAVIFile) {
		fwrite((const void*)streamId, sizeof(char), (size_t)4, currentAVIFile);
		fwrite((const void*)&chuckSize, sizeof(int), 1, currentAVIFile);
		fwrite((const void*)p, 1, chuckSize, currentAVIFile);
		if (len % 2 != 0) {
			char pad = 0;
			fwrite((const void*)&pad, 1, 1, currentAVIFile);
			chuckSize++;
		}
	}


	char* idTxt = indexBlock[framesInFile].ckId;
	memcpy((void*)idTxt, (const void*)streamId, 4);
	indexBlock[framesInFile].cOffset = chuckSizeCum;
	indexBlock[framesInFile].cFlags = 16;
	indexBlock[framesInFile].cSize = chuckSize;

	chuckSizeCum += (chuckSize + 8);
	memcpy(&(frame_timestamps[framesInFile].time_stamp), &vidCapNow, sizeof(vidCapNow));
	//memset(&(frame_timestamps[framesInFile].time_stamp),0,sizeof(struct timespec));
	frame_timestamps[framesInFile].audio_file_num = audio_file_index;
	framesInFile++;

	struct timespec timeDiff2;
	TimeDiffP(&vidCapStart, &vidCapNow, &timeDiff2);

	if (timeDiff2.tv_sec >= captureTimeInSecs) {
		memcpy((void*)&vidCapStart, (const void*)&entryTime, sizeof(struct timespec));

		meanFrameTime /= (double)framesInFile;
		double framesPerSec = 1.0 / meanFrameTime;
		printf("fps: %lf\n", framesPerSec);
		framesPerSec = round(framesPerSec);
		int iFramesPerSec = (int)framesPerSec;

		double frameTimeD = 1000000.0 / (double)iFramesPerSec;
		int microFrameTime = (int)frameTimeD;
		int chkSize = framesInFile*sizeof(struct IndxBlock);

		fwrite((const void*)idx, sizeof(char), 4, currentAVIFile);
		fwrite((const void*)&chkSize, sizeof(int), 1, currentAVIFile);
		fwrite((const void*)&indexBlock[0], sizeof(struct IndxBlock), framesInFile, currentAVIFile);
		fwrite((const void*)&frame_timestamps[0], sizeof(struct FrameTime), framesInFile, currentAVIFile);
		fwrite((const void*)&framesInFile, sizeof(framesInFile), 1, currentAVIFile);
		int totalFileSize = chuckSizeCum + sizeof(struct LargeAVIHeader) + sizeof(int)+chkSize - 8;
		fseek(currentAVIFile, fileSizeOffset, SEEK_SET);
		fwrite((const void*)&totalFileSize, sizeof(DWORD), 1, currentAVIFile);
		fseek(currentAVIFile, moviListSizeOffset, SEEK_SET);
		fwrite((const void*)&chuckSizeCum, sizeof(DWORD), 1, currentAVIFile);

		fseek(currentAVIFile, totalFrameOffset, SEEK_SET);
		fwrite((const void*)&framesInFile, sizeof(DWORD), 1, currentAVIFile);

		fseek(currentAVIFile, streamLengthOffset, SEEK_SET);
		fwrite((const void*)&framesInFile, sizeof(DWORD), 1, currentAVIFile);

		fseek(currentAVIFile, frameRateOffset, SEEK_SET);
		fwrite((const void*)&iFramesPerSec, sizeof(DWORD), 1, currentAVIFile);

		fseek(currentAVIFile, frameTimeOffset, SEEK_SET);
		fwrite((const void*)&microFrameTime, sizeof(DWORD), 1, currentAVIFile);

		printf("fps out: %d\n", iFramesPerSec);
		framesInFile = 0;
		chuckSizeCum = 4;
		meanFrameTime = 0.0;

		/*if(timingFile)
		 *      fclose(timingFile);
		 *
		 *    timingFile = fopen("timing.txt","a");*/

		if (currentAVIFile)
			fclose(currentAVIFile);

		currentAVIFile = NULL;
		fflush(stdout);
		filesCaptured++;

		pthread_mutex_lock(&lock);
		new_video_file = 1;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&lock);
	}
}

static int
read_frame(void)
{
	struct v4l2_buffer buf;
	unsigned int i;

	switch (io) {
	case IO_METHOD_READ:
		if (-1 == read(fd, buffers[0].start, buffers[0].length)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("read");
			}
		}

		//process_image2(buffers[0].start, 0);

		break;

	case IO_METHOD_MMAP:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("VIDIOC_DQBUF");
			}
		}

		assert(buf.index < n_buffers);
		int dataCount = buf.bytesused;
		//printf("dataCount buff: %d\n",dataCount);
		//__u32* ptsPtr = (__u32*)buf.timecode.type;
		/*if(timingFile == NULL) {
		 *		timingFile = fopen("timing.txt","w");
		 *		clock_gettime(CLOCK_REALTIME, &first_frame_time);
		 *		first_pts_timestamp = buf.timecode.type;
		 }*/

		struct timespec currentCPUTime;

		clock_gettime(CLOCK_REALTIME, &currentCPUTime);
		//printf("PTS: %u\n",*ptsPtr);
		double t2 = 0.0;
		t2 = (double)buf.timecode.type;
		const double t_period = 1.0 / 15.0e6;

		if (timingFile != NULL) {
			if (first_entry == 1) {
				first_pts_timestamp = (int)t2;
				memcpy(&currentCPUTime, &first_frame_time, sizeof(first_frame_time));
				first_entry = 0;
			}
			double pts_time = (t2 - (double)first_pts_timestamp)*t_period;

			struct timespec timeDiff;
			TimeDiffP(&first_frame_time, &currentCPUTime, &timeDiff);
			double frameTime = (double)timeDiff.tv_sec + (((double)timeDiff.tv_nsec) / 1e9);
			//frameTime /= 1e9;
			fprintf(timingFile, "pts: %f, cpu time: %f\n", pts_time, frameTime);
		}

		if (t_last > 0) {
			double frame_rate = 1.0 / ((t2 - t_last)*t_period);
			//printf("frame rate: %f\n",frame_rate);
			//printf("PTS: %u\n",buf.timecode.type);
		}

		t_last = t2;
		//printf("dataCount %d buf.index %d\n",dataCount,buf.index);
    struct jpg_image *img = (struct jpg_image *)malloc(sizeof(struct jpg_image));
		img->size = dataCount;
    img->data = (unsigned char*)malloc(dataCount);
    memcpy(img->data,buffers[buf.index].start,dataCount);

		int rc = write(stream_write_pipe, &img, sizeof(img));
    //
		//process_image2(buffers[buf.index].start, dataCount);

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");

		break;

	case IO_METHOD_USERPTR:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("VIDIOC_DQBUF");
			}
		}

		for (i = 0; i < n_buffers; ++i)
		if (buf.m.userptr == (unsigned long)buffers[i].start
			&& buf.length == buffers[i].length)
			break;

		assert(i < n_buffers);

		//process_image2((void *)buf.m.userptr, 0);

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");

		break;
	}

	return 1;
}

static void
mainloop(void)
{
	//   unsigned long count;
	//   int secCount = (int)CAPTURE_SECS;
	//   //mainCapTime,mainCapTimeStart
	//   count = 10*secCount;
	//   clock_gettime(CLOCK_REALTIME, &mainCapTimeStart);
	unsigned int count = 0;

	while (1) {
		count++;
		//     clock_gettime(CLOCK_REALTIME, &mainCapTime);
		//     
		//     struct timespec timeElapsed;
		//     
		//     TimeDiffP(&mainCapTimeStart, &mainCapTime,&timeElapsed);   
		//     
		//     if(timeElapsed.tv_sec >= count) {
		//       printf("Captured %ld secs of video.\n",count);
		//       break;
		//     }
		for (;;) {
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r) {
				if (EINTR == errno)
					continue;

				errno_exit("select");
			}

			if (0 == r) {
				fprintf(stderr, "select timeout\n");
				exit(EXIT_FAILURE);
			}

			if (read_frame())
				break;

			/* EAGAIN - continue select loop. */
		}
	}

	printf("%d frames captured.\n", count);
	done_capture = 1;
}

static void
stop_capturing(void)
{
	enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
			errno_exit("VIDIOC_STREAMOFF");

		break;
	}
}

static void
start_capturing(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
		}
		//       type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		//       
		//       if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
		// 	printf("Did not need to stop stream.\n");


		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");

		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.index = i;
			buf.m.userptr = (unsigned long)buffers[i].start;
			buf.length = buffers[i].length;

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");

		break;
	}
}

static void
uninit_device(void)
{
	unsigned int i;

	switch (io) {
	case IO_METHOD_READ:
		free(buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		printf("n_buffers %d\n", n_buffers);
		for (i = 0; i < n_buffers; ++i)

		if (-1 == munmap(buffers[i].start, buffers[i].length))
			errno_exit("munmap");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i)
			free(buffers[i].start);
		break;
	}

	free(buffers);
}

static void
init_read(unsigned int           buffer_size)
{
	buffers = calloc(1, sizeof (*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	buffers[0].length = buffer_size;
	buffers[0].start = malloc(buffer_size);

	if (!buffers[0].start) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
}

static void
init_mmap(void)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				"memory mapping\n", dev_name);
			exit(EXIT_FAILURE);
		}
		else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n",
			dev_name);
		exit(EXIT_FAILURE);
	}

	buffers = calloc(req.count, sizeof (*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
			mmap(NULL /* start anywhere */,
			buf.length,
			PROT_READ | PROT_WRITE /* required */,
			MAP_SHARED /* recommended */,
			fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit("mmap");
	}
}

static void
init_userp(unsigned int           buffer_size)
{
	struct v4l2_requestbuffers req;
	unsigned int page_size;

	page_size = getpagesize();
	buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				"user pointer i/o\n", dev_name);
			exit(EXIT_FAILURE);
		}
		else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	buffers = calloc(4, sizeof (*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
		buffers[n_buffers].length = buffer_size;
		buffers[n_buffers].start = memalign(/* boundary */ page_size,
			buffer_size);

		if (!buffers[n_buffers].start) {
			fprintf(stderr, "Out of memory\n");
			exit(EXIT_FAILURE);
		}
	}
}

static void SetControl(int id, int val_sel) {
	struct v4l2_queryctrl query_ctrl;
	struct v4l2_control control;
	struct v4l2_control check_control;
	// white balance
	memset(&query_ctrl, 0, sizeof(query_ctrl));
	query_ctrl.id = id;

	if (-1 == xioctl(fd, VIDIOC_QUERYCTRL, &query_ctrl))
		printf("Can't change control %d.\n", id);
	else {
		memset(&control, 0, sizeof(control));
		control.id = id;
		if (val_sel == 0)
			control.value = query_ctrl.minimum;
		else if (val_sel == 1)
			control.value = query_ctrl.default_value;
		else
			control.value = query_ctrl.maximum;

		memset(&check_control, 0, sizeof(check_control));
		check_control.id = control.id;


		if (-1 == xioctl(fd, VIDIOC_G_CTRL, &check_control)) {
			printf("Could not set control setting %d.\n", id);
		}

		if (check_control.value != control.value) {
			if (-1 == xioctl(fd, VIDIOC_S_CTRL, &control)) {
				printf("Could not set control setting %d.\n", id);
			}
		}
	}
}

static void
init_device(void)
{
	//SetControl(V4L2_CID_GAIN,0);
	//SetControl(V4L2_CID_GAMMA,0);
	   struct v4l2_queryctrl query_ctrl;
	   struct v4l2_control control;
	   struct v4l2_control check_control;
	//   // white balance
	//   memset(&query_ctrl,0,sizeof(query_ctrl));
	//   query_ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
	//   
	//   if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &query_ctrl))
	//     printf("Can't change exposure time.\n");
	//   else {
	//     memset(&control,0,sizeof(control));
	//     control.id = V4L2_CID_AUTO_WHITE_BALANCE;
	//     control.value = 0;
	//     memset(&check_control,0,sizeof(check_control));
	//     check_control.id = control.id;
	//     
	//     
	//     if(-1 == xioctl (fd, VIDIOC_G_CTRL, &check_control)) {
	//       printf("Could not set exposure setting to manual.\n");
	//     }
	//     
	//     if(check_control.value !=  control.value) {
	//       if(-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
	// 	printf("Could not set exposure setting to manual.\n");
	//       }
	//     }
	//   }
	   // focus
	   memset(&query_ctrl,0,sizeof(query_ctrl));
	   query_ctrl.id = V4L2_CID_FOCUS_AUTO;
	   
	   if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &query_ctrl))
	     printf("Can't change auto focus.\n");
	   else {
	     memset(&control,0,sizeof(control));
	     control.id = V4L2_CID_FOCUS_AUTO;
       control.value = 0;
       memset(&check_control,0,sizeof(check_control));
       check_control.id = control.id;


       if(-1 == xioctl (fd, VIDIOC_G_CTRL, &check_control)) {
         printf("Could not set auto focus setting to manual.\n");
       }

       if(check_control.value !=  control.value) {
         if(-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
           printf("Could not set auto focus setting to manual.\n");
         }
       }
	   }
     // focus value
     memset(&query_ctrl,0,sizeof(query_ctrl));
     query_ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;

     if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &query_ctrl))
       printf("Can't change auto focus.\n");
     else {
       memset(&control,0,sizeof(control));
       control.id = V4L2_CID_FOCUS_ABSOLUTE;
       control.value = query_ctrl.minimum;
       memset(&check_control,0,sizeof(check_control));
       check_control.id = control.id;


       if(-1 == xioctl (fd, VIDIOC_G_CTRL, &check_control)) {
         printf("Could not set focus setting to manual.\n");
       }

       if(check_control.value !=  control.value) {
         if(-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
           printf("Could not set focus setting to manual.\n");
         }
       }
     }
	//   // white balance val
	//   memset(&query_ctrl,0,sizeof(query_ctrl));
	//   query_ctrl.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
	//   
	//   if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &query_ctrl))
	//     printf("Can't change exposure time.\n");
	//   else {
	//     memset(&control,0,sizeof(control));
	//     control.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
	//     control.value =query_ctrl.default_value;
	//     memset(&check_control,0,sizeof(check_control));
	//     check_control.id = control.id;
	//     
	//     
	//     if(-1 == xioctl (fd, VIDIOC_G_CTRL, &check_control)) {
	//       printf("Could not set exposure setting to manual.\n");
	//     }
	//     
	//     if(check_control.value !=  control.value) {
	//       if(-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
	// 	printf("Could not set exposure setting to manual.\n");
	//       }
	//     }
	//   }
	//   // manual exposure
	//   memset(&query_ctrl,0,sizeof(query_ctrl));
	//   query_ctrl.id = V4L2_CID_EXPOSURE_AUTO;
	//   
	//   if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &query_ctrl))
	//     printf("Can't change exposure time.\n");
	//   else {
	//     memset(&control,0,sizeof(control));
	//     control.id = V4L2_CID_EXPOSURE_AUTO;
	//     //control.value = V4L2_EXPOSURE_MANUAL;
	//     control.value = V4L2_EXPOSURE_AUTO;
	//     memset(&check_control,0,sizeof(check_control));
	//     check_control.id = control.id;
	//     
	//     
	//     if(-1 == xioctl (fd, VIDIOC_G_CTRL, &check_control)) {
	//       printf("Could not set exposure setting to manual.\n");
	//     }
	//     
	//     if(check_control.value !=  control.value) {       
	//       if(-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
	// 	printf("Could not set exposure setting to manual.\n");
	//       }
	//     }
	//   }
	//   
	//   // set exposure
	//   memset(&query_ctrl,0,sizeof(query_ctrl));
	//   query_ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
	//   
	//   if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &query_ctrl))
	//     printf("Can't change exposure time.\n");
	//   else {
	//     memset(&control,0,sizeof(control));
	//     control.id = V4L2_CID_EXPOSURE_ABSOLUTE;
	//     control.value = query_ctrl.default_value;
	//     //control.value = query_ctrl.minimum;
	//     memset(&check_control,0,sizeof(check_control));
	//     check_control.id = control.id;
	//     
	//     
	//     if(-1 == xioctl (fd, VIDIOC_G_CTRL, &check_control)) {
	//       printf("Could not set exposure setting to manual.\n");
	//     }
	//     
	//     if(check_control.value !=  control.value) {       
	//       if(-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
	// 	printf("Could not set exposure setting to manual.\n");
	//       }
	//       else
	// 	printf("exposure set to %d!\n",control.value);
	//     }    
	//   }
	//   int qw = 0;
	//   qw = 1;
	// adjust
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n",
				dev_name);
			exit(EXIT_FAILURE);
		}
		else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	int bus_info_size = sizeof(device_bus_info) > sizeof(cap.bus_info) ? sizeof(cap.bus_info) : sizeof(device_bus_info);

	memcpy(device_bus_info, cap.bus_info, bus_info_size);
	//printf("bus info: %s\n",cap.bus_info);
	printf("card info: %s\n", cap.card);

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n",
			dev_name);
		exit(EXIT_FAILURE);
	}

	switch (io) {
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
			fprintf(stderr, "%s does not support read i/o\n",
				dev_name);
			exit(EXIT_FAILURE);
		}

		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "%s does not support streaming i/o\n",
				dev_name);
			exit(EXIT_FAILURE);
		}

		break;
	}


	/* Select video input, video standard and tune here. */


	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	}
	else {
		/* Errors ignored. */
	}


	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = hres;
	fmt.fmt.pix.height = vres;
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.pixelformat = 0x47504a4d;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	int formatTest = 0x56595559;
	formatTest = V4L2_PIX_FMT_YUYV;
	//fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
		errno_exit("VIDIOC_S_FMT");

	/* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	switch (io) {
	case IO_METHOD_READ:
		init_read(fmt.fmt.pix.sizeimage);
		break;

	case IO_METHOD_MMAP:
		init_mmap();
		break;

	case IO_METHOD_USERPTR:
		init_userp(fmt.fmt.pix.sizeimage);
		break;
	}

	struct v4l2_frmivalenum fval;
	fval.pixel_format = fmt.fmt.pix.pixelformat;
	fval.width = fmt.fmt.pix.width;
	fval.height = fmt.fmt.pix.height;
	fval.index = 0;
	if (-1 == xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fval))
		errno_exit("VIDIOC_ENUM_FRAMEINTERVALS");

	struct v4l2_streamparm sparm;
	sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(fd, VIDIOC_G_PARM, &sparm))
		errno_exit("VIDIOC_G_PARM");

	/*if(sparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
	 *    if (-1 == xioctl (fd, VIDIOC_S_PARM, &sparm))
	 *    errno_exit ("VIDIOC_G_PARM");
	 }*/


}

static void
close_device(void)
{
	if (-1 == close(fd))
		errno_exit("close");

	fd = -1;
}

static void
open_device(void)
{
	struct stat st;

	if (-1 == stat(dev_name, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n",
			dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void
usage(FILE *                 fp,
int                    argc,
char **                argv)
{
	fprintf(fp,
		"Usage: %s [options]\n\n"
		"Options:\n"
		"-d | --device name   Video device name [/dev/video]\n"
		"-h | --help          Print this message\n"
		"-m | --mmap          Use memory mapped buffers\n"
		"-r | --read          Use read() calls\n"
		"-u | --userp         Use application allocated buffers\n"
		"",
		argv[0]);
}

static const char short_options[] = "d:hmru";

static const struct option
long_options[] = {
	{ "device", required_argument, NULL, 'd' },
	{ "help", no_argument, NULL, 'h' },
	{ "mmap", no_argument, NULL, 'm' },
	{ "read", no_argument, NULL, 'r' },
	{ "userp", no_argument, NULL, 'u' },
	{ 0, 0, 0, 0 }
};


int
main(int argc, char** argv)
{
  FILE* config_file = NULL;

  config_file = fopen("config.cam","r");
  char buffer[1024];
  memset(buffer,0,sizeof(buffer));
  unsigned int state = 0;
  
  memset(stream_flag,0,sizeof(stream_flag));
  char word[1024];
  unsigned int c = 0;
  unsigned int word_len = 0;
  unsigned short error = 0;
  sprintf(camera_id,"553wyxnw1566,webcam");
  int default_port = 8900;

  if(config_file)
  {
    while (fgets(buffer,1024, config_file)!=NULL)
    {
      unsigned int buffer_len = (unsigned int)strlen(buffer);
      buffer[buffer_len] = ' ';
      buffer_len++;
      memset(word,0,sizeof(word));
      word_len = 0;
      for(c = 0; c < buffer_len; c++)
      {
        if(buffer[c] == ' ' || buffer[c] == '\n')
        {
          if(word_len > 0)
          {
            switch(state)
            {
            case 0:
              if(strcmp(word,"live") != 0) {
                printf("Error: expected 'live' got %s",word);
                error = 1;
              }
              break;
            case 1:
              if(strcmp(word,"A") == 0)
                stream_flag[0] = 1;
              break;
            case 2:
              sprintf(live_ip_address,"%s",word);
              break;
            case 3:
              sprintf(live_port_no,"%s",word);
              break;
            case 4:
              if(strcmp(word,"disk") != 0) {
                printf("Error: expected 'disk' got %s",word);
                error = 1;
              }
              break;
            case 5:
              if(strcmp(word,"A") == 0)
                stream_flag[1] = 1;
              break;
            case 6:
              sprintf(disk_ip_address,"%s",word);
              break;
            case 7:
              sprintf(disk_port_no,"%s",word);
              break;
            case 8:
              sprintf(camera_id,"553wyxnw1566,%s",word);
              break;
            case 9:
              //sprintf(camera_id,"553wyxnw1566,%s",word);
              default_port = atoi(word);

              if(default_port <= 0)
                default_port = 8900;
              break;
            }
            state++;
          }
          memset(word,0,sizeof(word));
          word_len = 0;
        }
        else
        {
          word[word_len] = buffer[c];
          word_len++;
        }
      }        
    }

    fclose(config_file);


  }
  else
  {
    error = 1;
  }


  if(error != 0) {
    printf("Can not read config.cam file!\n");
    exit(1);
  }

  printf("live: %s %s\n",live_ip_address,live_port_no);
  printf("disk: %s %s\n",disk_ip_address,disk_port_no);
  printf("camera id: %s using rx port %d\n",camera_id,default_port);

	dev_name = "/dev/video0";
	memset(working_directory, 0, sizeof(working_directory));
	sprintf(working_directory, "/home/gary/testvid/");
	memcpy(audio_working_directory, working_directory, sizeof(audio_working_directory));
	memcpy(video_working_directory, working_directory, sizeof(video_working_directory));
	memcpy(av_working_directory, working_directory, sizeof(av_working_directory));

	char audio_card_name[32];
	memset(device_bus_info, 0, sizeof(device_bus_info));
	//printf("size of bus info: %d\n",sizeof(device_bus_info));

	//printf("timestamp size: %d\n",sizeof(struct timespec));

	for (;;) {
		int index;
		int c;

		c = getopt_long(argc, argv,
			short_options, long_options,
			&index);

		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			dev_name = optarg;
			break;

		case 'h':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);

		case 'm':
			io = IO_METHOD_MMAP;
			break;

		case 'r':
			io = IO_METHOD_READ;
			break;

		case 'u':
			io = IO_METHOD_USERPTR;
			break;

		default:
			usage(stderr, argc, argv);
			exit(EXIT_FAILURE);
		}
	}

	pthread_t video_thread, audio_thread, av_thread;
	int ret1, ret2, ret3;

	int dev_name_len = strlen(dev_name);
	sprintf(audio_working_directory + strlen(audio_working_directory), "audio%c/", dev_name[dev_name_len - 1]);
	sprintf(video_working_directory + strlen(video_working_directory), "video%c/", dev_name[dev_name_len - 1]);
	sprintf(av_working_directory + strlen(av_working_directory), "av%c/", dev_name[dev_name_len - 1]);
	//printf("wd: %s\n",video_working_directory);
	//printf("dev len: %d\n",dev_name_len);
	open_device();

	init_device();
	printf("bus info: %s\n", device_bus_info);

	int audio_card_num = find_audio_device();


  int pipefd[2];
  int socket_pipefd[2];

	if (pipe(pipefd) != 0) {
		perror("pipe");
		exit(1);
	}
  
  if (pipe(socket_pipefd) != 0) {
		perror("pipe");
		exit(1);
	}

  pthread_t streamingthread;
  struct threadargs args;
  args.fd = pipefd[0]; // read end of the pipe
  args.socket_fd = socket_pipefd[1];
  stream_write_pipe = pipefd[1];
	pthread_create(&streamingthread, NULL, &streamer, (void *)&args);

  // socket thread
  pthread_t socketthread;
  struct threadargs args_soc;
  args_soc.fd = socket_pipefd[0]; // read end of the pipe  
	pthread_create(&socketthread, NULL, &socket_transmit, (void *)&args_soc);

   // socket receiver thread
  pthread_t socketthreadrx;
  struct threadargs args_soc_rx;
  args_soc_rx.fd = default_port; // listening port  
	pthread_create(&socketthreadrx, NULL, &socket_receive, (void *)&args_soc_rx);

	start_capturing();
	//audio_card_num = -1;

	if (audio_card_num >= 0 && 0) {
		printf("Composite USB audio device found.\n");
		memset(audio_card_name, 0, sizeof(audio_card_name));
		sprintf(audio_card_name, "hw:%d,0", audio_card_num);
		ret2 = pthread_create(&audio_thread, NULL, audio_capture_thread, (void*)audio_card_name);
		ret3 = pthread_create(&av_thread, NULL, combine_video_and_audio, (void*)dev_name);
	}

	//ret1 = pthread_create(&video_thread, NULL, video_capture_thread, (void*)dev_name);
	mainloop();
	//pthread_join(video_thread, NULL);

	if (audio_card_num >= 0 && 0) {
		pthread_join(audio_thread, NULL);
		pthread_join(av_thread, NULL);
	}

	stop_capturing();

	uninit_device();

	close_device();

	exit(EXIT_SUCCESS);

	return 0;
}

void* combine_video_and_audio(void *ptr) {
	printf("AV thread started...\n");
	const double max_frame_time = 1.0 / (double)CAP_MIN_FRAME;
	const double max_frame_rate = (double)MAX_SAMPLE_RATE;
	const double min_sample_time = 1.0 / max_frame_rate;

	const double max_audio_sam_per_frame = max_frame_time / min_sample_time;

	const int imax_audio_sam_per_frame = (int)max_audio_sam_per_frame + 1;


	char file_name[512];
	int file_name_base_len = strlen(video_working_directory);
	int at_main_start = 1;
	char aud_file_name[512];
	char av_file_name[512];
	char indx_file_name[512];
	int av_file_name_base_len = strlen(av_working_directory);
	int aud_file_name_base_len = strlen(audio_working_directory);
	FILE* indx_file = NULL;
	struct FrameTime first_video_frame;
	struct audio_frame_info audio_frame;
	int audio_chunk_size = 4096;
	char* aud_stream_id = "01wb";
	//const int prev_buffer_size = 1 + (audio_chunk_size / (int)AUDIO_BUFFER_SIZE);
	char* video_data_buffer = malloc(vres*hres * 3);
	char audio_output_buffer[4 * imax_audio_sam_per_frame];
	//struct audio_frame_info prev_audio_buffers[prev_buffer_size];
	struct LargeAVIHeader large_video_header;
	struct WavHeader audio_header;
	struct LargeAVIHeaderAV largeAVIHeaderAV;
	struct IndxBlock indx_block_video;
	struct IndxBlock indx_block_audio;

	memcpy((void*)indx_block_video.ckId, (const void*)streamId, 4);
	memcpy((void*)indx_block_audio.ckId, (const void*)aud_stream_id, 4);

	//indexBlock[framesInFile].cOffset = chuckSizeCum;
	indx_block_video.cFlags = 16;
	indx_block_audio.cFlags = 16;
	//indexBlock[framesInFile].cSize = chuckSize;

	for (;;) {
		if (done_capture == 1)
			break;

		if (av_main_file_name == NULL) {
			memcpy(av_file_name, av_working_directory, sizeof(av_file_name));
			sprintf(av_file_name + av_file_name_base_len, "av%d.avi", av_file_number);

			memcpy(indx_file_name, av_working_directory, sizeof(av_file_name));
			sprintf(indx_file_name + av_file_name_base_len, "av.tmp");

			av_main_file_name = fopen(av_file_name, "wb");
			indx_file = fopen(indx_file_name, "wb+");

			if (av_main_file_name == NULL || indx_file == NULL) {
				continue;
			}

			av_file_number++;

			char tempFileName[] = "//home//gary//testvid//template//aviHeaderTemplate_av.hd0";
			FILE* tempFile = fopen(tempFileName, "rb");
			fread((void*)&largeAVIHeaderAV, sizeof(struct LargeAVIHeaderAV), 1, tempFile);
			fclose(tempFile);
			largeAVIHeaderAV.riffHeader.bitmapInfoHeader.biWidth = hres;
			largeAVIHeaderAV.riffHeader.bitmapInfoHeader.biHeight = vres;

			largeAVIHeaderAV.riffHeader.aviHeader.dwWidth = hres;
			largeAVIHeaderAV.riffHeader.aviHeader.dwHeight = vres;


			largeAVIHeaderAV.riffHeader.aviStreamHeader.rcFrame.right = (short int)hres;
			largeAVIHeaderAV.riffHeader.aviStreamHeader.rcFrame.bottom = (short int)vres;

			fwrite(&largeAVIHeaderAV, sizeof(largeAVIHeaderAV), 1, av_main_file_name);
		}

		pthread_mutex_lock(&lock);
		while (!new_video_file) {
			pthread_cond_wait(&cond, &lock);
		}

		new_video_file = 0;
		pthread_mutex_unlock(&lock);

		int vid_file_ind = fileNumber - 1;
		memcpy(file_name, video_working_directory, sizeof(file_name));
		sprintf(file_name + file_name_base_len, "video%d.avi", vid_file_ind);


		FILE* vid_file = NULL;
		vid_file = fopen(file_name, "rb");

		if (vid_file == NULL) {
			continue;
		}

		fread(&large_video_header, sizeof(large_video_header), 1, vid_file);

		int frame_count = 0;
		fseek(vid_file, -sizeof(frame_count), SEEK_END);
		fread(&frame_count, sizeof(frame_count), 1, vid_file);

		int first_frame_offset = sizeof(struct FrameTime)*frame_count + sizeof(frame_count);
		fseek(vid_file, -first_frame_offset, SEEK_END);
		fread(&first_video_frame, sizeof(struct FrameTime), 1, vid_file);

		memcpy(aud_file_name, audio_working_directory, sizeof(aud_file_name));
		sprintf(aud_file_name + aud_file_name_base_len, "audio%d.wav", first_video_frame.audio_file_num);

		/*while(first_video_frame.audio_file_num >= audio_file_index) {
		 *      sleep(1);
		 }*/
		pthread_mutex_lock(&aud_lock);
		while (!new_audio_file) {
			pthread_cond_wait(&aud_cond, &aud_lock);
		}

		new_audio_file = 0;
		pthread_mutex_unlock(&aud_lock);


		FILE* audio_file = NULL;
		audio_file = fopen(aud_file_name, "rb");

		/*while(audio_file == NULL) {
		 *    audio_file = fopen(aud_file_name,"rb");
		 *    sleep(1);
		 }*/

		if (audio_file == NULL) {
			fclose(vid_file);
			continue;
		}

		fread(&audio_header, sizeof(audio_header), 1, audio_file);

		int buffer_count = 0;
		fseek(audio_file, -sizeof(buffer_count), SEEK_END);

		fread(&buffer_count, sizeof(audio_file), 1, audio_file);
		int first_buffer_offset = sizeof(struct audio_frame_info)*buffer_count + sizeof(buffer_count);
		fseek(audio_file, -first_buffer_offset, SEEK_END);
		int as = 0;
		int start_frame_num = 0;
		int start_buffer_num = -1;
		long prev_diff = -1;
		int first_run = 1;
		int aud_time_sam = 1;
		int left_time = 0;

		//double vid_frm_time = (double)CAPTURE_SECS / (double)frame_count;
		double vid_frm_time = 1.0 / (double)large_video_header.riffHeader.aviStreamHeader.dwRate;
		int micro_sec = (int)(vid_frm_time * 1000000);
		largeAVIHeaderAV.riffHeader.aviHeader.dwMicroSecPerFrame = micro_sec;
		largeAVIHeaderAV.riffHeader.aviHeader.dwTotalFrames = frame_count;
		double aud_sam_time = 1.0 / (double)audio_header.sample_rate;

		double aud_sam_per_frm = vid_frm_time / aud_sam_time;
		int iaud_sam_per_frm = (int)aud_sam_per_frm;

		if (iaud_sam_per_frm % 2 != 0)
			iaud_sam_per_frm++;

		iaud_sam_per_frm *= audio_header.bytes_per_sam;

		audio_chunk_size = iaud_sam_per_frm;
		//for(as = 0; as < 1; as++) {
		for (;;) {
			if (aud_time_sam == 1) {
				fread(&audio_frame, sizeof(audio_frame), 1, audio_file);
				printf("audio, %ld sec, %ld nsec.\n", audio_frame.capture_end.tv_sec, audio_frame.capture_end.tv_nsec);
				start_buffer_num++;
				aud_time_sam = 0;
			}
			struct timespec time_diff;

			printf("video, %ld sec, %ld nsec\n", first_video_frame.time_stamp.tv_sec, first_video_frame.time_stamp.tv_nsec);
			//TimeDiffP(&first_video_frame.time_stamp,&audio_frame.capture_start,&time_diff);
			time_difference(&first_video_frame.time_stamp, &audio_frame.capture_end, &time_diff);
			printf("buffer: %d, tds: %ld,%ld\n", start_frame_num, time_diff.tv_sec, time_diff.tv_nsec);
			/*TimeDiffP(&first_video_frame.time_stamp,&audio_frame.capture_end,&time_diff);
			 *  printf("buffer: %d, tde: %ld\n",as,time_diff.tv_nsec);*/
			if (time_diff.tv_nsec < 0) {
				time_diff.tv_nsec *= -1;
				left_time = 1;
			}
			else
				left_time = 0;

			printf("p,c = %ld,%ld\n", prev_diff, time_diff.tv_nsec);
			if (first_run != 1) {
				if (time_diff.tv_nsec < prev_diff) {
					start_frame_num++;
				}
				else
					break;
			}
			else {
				if (left_time == 1) {
					first_run = 1;
					aud_time_sam = 1;
				}
				else
					first_run = 0;
			}


			if (as > 200)
				break;
			printf("flag ast: %d, left : %d\n", aud_time_sam, left_time);

			if (aud_time_sam != 1) {
				fread(&first_video_frame, sizeof(struct FrameTime), 1, vid_file);
				prev_diff = (long)time_diff.tv_nsec;
			}
			as++;
		}



		printf("Combining... audio buffer count: %d, start frame: %d, buffer: %d\n", buffer_count, start_frame_num, start_buffer_num);



		int frames_in_file = large_video_header.riffHeader.aviStreamHeader.dwLength;
		int audio_data_size = audio_header.data_size;

		fseek(audio_file, sizeof(audio_header), SEEK_SET);
		int buffer_length = bytes_per_sample*(int)AUDIO_BUFFER_SIZE;
		start_buffer_num = 0;
		while (start_buffer_num > 0) {
			fread(audio_output_buffer, buffer_length, 1, audio_file);
			start_buffer_num--;
			audio_data_size -= buffer_length;
		}

		int available_audio_chunks = audio_data_size / audio_chunk_size;
		fseek(vid_file, sizeof(large_video_header), SEEK_SET);




		start_frame_num = 0;
		start_buffer_num = 0;

		int aud_data_out_size = 0;
		int video_data_size = 4;
		char stream_id[4];
		int chunk_size = 0;
		int chunk_offset = 4;
		int audio_chunk_offset = 0;
		char dummmy_byte = 0;
		int fr = 0;
		char insert_dummy_byte = 0;
		int indx_data_size = 0;
		int movi_list_size = 0;

		for (fr = 0; fr < frames_in_file; fr++) {
			fread(stream_id, sizeof(stream_id), 1, vid_file);
			fread(&chunk_size, sizeof(chunk_size), 1, vid_file);
			fread(video_data_buffer, 1, chunk_size, vid_file);

			insert_dummy_byte = 0;
			if (chunk_size % 2 != 0) {
				fread(&dummmy_byte, sizeof(dummmy_byte), 1, vid_file);
				insert_dummy_byte = 1;
			}
			fwrite(stream_id, sizeof(stream_id), 1, av_main_file_name);
			fwrite(&chunk_size, sizeof(chunk_size), 1, av_main_file_name);
			fwrite(video_data_buffer, 1, chunk_size, av_main_file_name);

			if (insert_dummy_byte == 1)
				fwrite(&dummmy_byte, sizeof(dummmy_byte), 1, av_main_file_name);

			indx_block_video.cSize = chunk_size;
			indx_block_video.cOffset = video_data_size;
			//int old_chunk_offset = chunk_offset;
			video_data_size += chunk_size + sizeof(stream_id)+sizeof(chunk_size);
			//video_data_size += (sizeof(stream_id) + sizeof(chunk_size) + chunk_size);
			if (insert_dummy_byte == 1)
				video_data_size += sizeof(dummmy_byte);

			//audio_chunk_offset += (chunk_offset - old_chunk_offset);

			if (fr < available_audio_chunks)
				fread(audio_output_buffer, iaud_sam_per_frm, 1, audio_file);
			else
				memset(audio_output_buffer, 0, iaud_sam_per_frm);


			chunk_size = iaud_sam_per_frm;
			fwrite(aud_stream_id, sizeof(stream_id), 1, av_main_file_name);
			fwrite(&chunk_size, sizeof(chunk_size), 1, av_main_file_name);
			aud_data_out_size += chunk_size;
			fwrite(audio_output_buffer, iaud_sam_per_frm, 1, av_main_file_name);
			indx_block_audio.cSize = chunk_size;
			indx_block_audio.cOffset = video_data_size;

			video_data_size += chunk_size + sizeof(stream_id)+sizeof(chunk_size);

			fwrite(&indx_block_video, sizeof(indx_block_video), 1, indx_file);
			fwrite(&indx_block_audio, sizeof(indx_block_audio), 1, indx_file);
			indx_data_size += sizeof(indx_block_video);
			indx_data_size += sizeof(indx_block_audio);
			//indx_file
		}
		//first_video_frame
		//printf("Processing video file: %s. %d frames.\n",file_name,frame_count);
		movi_list_size = video_data_size;
		int total_file_size = movi_list_size + sizeof(struct LargeAVIHeaderAV) + sizeof(int)+indx_data_size - 8;

		fclose(audio_file);
		fclose(vid_file);
		memcpy(&largeAVIHeaderAV.riffHeader.aviStreamHeader, &large_video_header.riffHeader.aviStreamHeader,
			sizeof(large_video_header.riffHeader.aviStreamHeader));

		memcpy(&largeAVIHeaderAV.riffHeader.bitmapInfoHeader, &large_video_header.riffHeader.bitmapInfoHeader,
			sizeof(large_video_header.riffHeader.bitmapInfoHeader));

		largeAVIHeaderAV.riffHeader.aviStreamHeader_aud.dwRate = audio_header.sample_rate;
		largeAVIHeaderAV.riffHeader.aviStreamHeader_aud.dwLength = aud_data_out_size / 2;
		largeAVIHeaderAV.riffHeader.aviStreamHeader_aud.dwSuggestedBufferSize = iaud_sam_per_frm;
		//memcpy(&largeAVIHeaderAV.riffHeader.pcm_audio_header,&audio_header.fmt_len,sizeof(largeAVIHeaderAV.riffHeader.pcm_audio_header));
		largeAVIHeaderAV.riffHeader.pcm_audio_header.sample_rate = audio_header.sample_rate;
		largeAVIHeaderAV.riffHeader.pcm_audio_header.bytes_per_sec = audio_header.bytes_per_sec;
		largeAVIHeaderAV.riffHeader.fileSize = total_file_size;
		largeAVIHeaderAV.moviList.moviListSize = movi_list_size;

		fseek(av_main_file_name, 0, SEEK_SET);
		fwrite(&largeAVIHeaderAV, sizeof(largeAVIHeaderAV), 1, av_main_file_name);
		fseek(av_main_file_name, 0, SEEK_END);

		char* indx_data = malloc(indx_data_size);
		fseek(indx_file, 0, SEEK_SET);
		fread(indx_data, indx_data_size, 1, indx_file);
		fseek(av_main_file_name, 0, SEEK_END);
		fwrite((const void*)idx, sizeof(char), 4, av_main_file_name);
		fwrite((const void*)&indx_data_size, sizeof(int), 1, av_main_file_name);

		fwrite(indx_data, indx_data_size, 1, av_main_file_name);
		free(indx_data);
		fclose(av_main_file_name);
		fclose(indx_file);
		av_main_file_name = NULL;
	}

	free(video_data_buffer);
}

void* video_capture_thread(void *ptr) {
	mainloop();
}

void* audio_capture_thread(void *ptr) {
	char* audio_device_name = (char *)ptr;
	int i;
	int err;
	const int buffer_len = (int)AUDIO_BUFFER_SIZE;
	short buf[buffer_len];
	bytes_per_sample = sizeof(buf[0]);
	int exact_rate = 44100;
	short channel_count = 1;
	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hw_params;
	snd_timestamp_t timestamp;
	snd_pcm_status_t* status;
	snd_pcm_status_alloca(&status);

	if ((err = snd_pcm_open(&capture_handle, audio_device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "cannot open audio device %s (%s)\n",
			audio_device_name,
			snd_strerror(err));
		pthread_exit(NULL);
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}

	if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
		fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}

	if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "cannot set access type (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}

	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf(stderr, "cannot set sample format (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}
	//printf("Before set rate!\n");

	if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &exact_rate, 0)) < 0) {
		fprintf(stderr, "cannot set sample rate (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}
	//printf("Before set channel!\n");

	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channel_count)) < 0) {
		fprintf(stderr, "cannot set channel count (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}

	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
		fprintf(stderr, "cannot set parameters (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(capture_handle)) < 0) {
		fprintf(stderr, "cannot prepare audio interface for use (%s)\n",
			snd_strerror(err));
		pthread_exit(NULL);
	}
	int rec_time_sec = (int)CAPTURE_SECS;
	int data_size = rec_time_sec*exact_rate*bytes_per_sample;

	struct WavHeader wavHeader = {
		.riff = "RIFF",
		.wave = "WAVE",
		.fmt = "fmt ",
		.data = "data",
		.format = 1,
		.sample_rate = exact_rate,
		.channels = channel_count,
		.bytes_per_sec = exact_rate*bytes_per_sample*channel_count,
		.bits_per_sam = bytes_per_sample * 8,
		.data_size = data_size,
		.file_size = 0,
		.fmt_len = 16,
		.bytes_per_sam = bytes_per_sample };


	char file_name[512];
	memcpy(file_name, audio_working_directory, sizeof(file_name));
	int file_name_base_len = strlen(file_name);
	//audio_file_index   

	printf("Audio capture from card %s started...\n", audio_device_name);
	//snd_pcm_status_get_state(statu
	//for (i = 0; i < exact_rate*rec_time_sec/buffer_len; ++i) {   
	struct timespec capture_start_time;
	snd_htimestamp_t htimestamp;

	while (1) {

		snd_pcm_status(capture_handle, status);
		snd_pcm_status_get_htstamp(status, &htimestamp);
		clock_gettime(CLOCK_REALTIME, &capture_start_time);
		if ((err = snd_pcm_readi(capture_handle, buf, buffer_len)) != buffer_len) {
			fprintf(stderr, "read from audio interface failed (%s)\n",
				snd_strerror(err));
			pthread_exit(NULL);
		}
		clock_gettime(CLOCK_REALTIME, &audioCapNow);
		if (currentWAVFile == NULL) {
			sprintf(file_name + file_name_base_len, "audio%d.wav", audio_file_index);
			currentWAVFile = fopen(file_name, "wb");

			if (currentWAVFile == NULL) {
				pthread_exit(NULL);
			}

			fwrite(&wavHeader, sizeof(struct WavHeader), 1, currentWAVFile);
			clock_gettime(CLOCK_REALTIME, &audioCapStart);
			memcpy(&audioCapNow, &audioCapStart, sizeof(struct timespec));
			printf("Writing to audio file: %s\n", file_name);
		}



		if (currentWAVFile) {
			fwrite(buf, sizeof(short), buffer_len, currentWAVFile);
			struct audio_frame_info* aud_frm = &audio_frame[audio_frame_inx];
			memcpy(&aud_frm->capture_start, &capture_start_time, sizeof(struct timespec));
			memcpy(&aud_frm->capture_end, &audioCapNow, sizeof(struct timespec));
			aud_frm->buffer_index = audio_frame_inx;
			audio_frame_inx++;
		}



		struct timespec timeDiff2;
		TimeDiffP(&audioCapStart, &audioCapNow, &timeDiff2);

		if (timeDiff2.tv_sec >= captureTimeInSecs) {
			printf("Capturing new file...\n");
			fwrite(&audio_frame[0], sizeof(struct audio_frame_info), audio_frame_inx, currentWAVFile);
			fwrite(&audio_frame_inx, sizeof(audio_frame_inx), 1, currentWAVFile);
			int aud_data_size = audio_frame_inx*buffer_len*bytes_per_sample;
			wavHeader.data_size = aud_data_size;
			wavHeader.file_size = aud_data_size + sizeof(wavHeader)-8;
			fseek(currentWAVFile, 0, SEEK_SET);
			fwrite(&wavHeader, sizeof(wavHeader), 1, currentWAVFile);
			fclose(currentWAVFile);
			currentWAVFile = NULL;
			audio_file_index++;
			audio_frame_inx = 0;
			pthread_mutex_lock(&aud_lock);
			new_audio_file = 1;
			pthread_cond_signal(&aud_cond);
			pthread_mutex_unlock(&aud_lock);
			if (done_capture == 1)
				break;
		}

		if (err == snd_pcm_wait(capture_handle, 1000) < 0) {
			printf("Audio polling failed!\n");
			break;
		}
		//usleep(100);
		//printf("ts: %ldS, %lduS\n",timestamp.tv_sec,timestamp.tv_usec);
	}

	snd_pcm_close(capture_handle);
	printf("Audio capture done.\n");
}

int find_audio_device() {
	FILE* card_list_fd = NULL;
	card_list_fd = fopen("/proc/asound/cards", "r");
	char read_line_buffer[256];
	int char_count = -1;
	int device_bus_info_len = strlen(device_bus_info);

	if (device_bus_info_len == 0)
		return -1;

	//printf("my len: %d\n",device_bus_info_len);
	int card_num = -1;
	int capture_card_num = -1;
	__u8 found_audio_capture = 0;

	if (card_list_fd) {
		do {
			memset(read_line_buffer, 0, sizeof(read_line_buffer));
			char_count = fscanf(card_list_fd, "%s", read_line_buffer);
			int str_len = strlen(read_line_buffer) - 1;

			if (str_len == 1 && read_line_buffer[0] == ']' && read_line_buffer[1] == ':') {
				card_num++;
			}

			if (str_len == device_bus_info_len) {
				int c = 0;
				found_audio_capture = 0;
				for (c = 0; c < str_len; c++) {
					if (read_line_buffer[c] != device_bus_info[c]) {
						found_audio_capture = -1;
						break;
					}
					//printf(" %c ", read_line_buffer[c]);
				}

				if (found_audio_capture == 0) {
					found_audio_capture = 1;
					capture_card_num = card_num;
				}
				//printf("\nflag: %d, card #%d\n",(int)found_audio_capture,card_num);
				//printf("' %s, len=%d'",read_line_buffer,str_len);
			}
		} while (char_count > 0);
		fclose(card_list_fd);

		return capture_card_num;
	}

	return -1;
}
