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

#include <linux/videodev2.h>
#include<time.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef enum {
  IO_METHOD_READ,
  IO_METHOD_MMAP,
  IO_METHOD_USERPTR,
} io_method;

struct buffer {
  void *                  start;
  size_t                  length;
};
#define FRAMES_IN_AVI 500
#define BILLION  1000000000L

struct timespec previousEntryTime, entryTime;
struct timespec vidCapStart,vidCapNow;

static char *           dev_name        = NULL;
static io_method        io              = IO_METHOD_MMAP;
static int              fd              = -1;
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
int hres = 640;
int vres = 480;
FILE* currentAVIFile = NULL;
struct IndxBlock indexBlock[FRAMES_IN_AVI];
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;
//struct timepsec vidCapStart;

struct timespec TimeDiff(struct timespec start, struct timespec end)
{
  struct timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
}

static void
errno_exit                      (const char *           s)
{
  fprintf (stderr, "%s error %d, %s\n",
	   s, errno, strerror (errno));
  
  exit (EXIT_FAILURE);
}

static int
xioctl                          (int                    fd,
				 int                    request,
				 void *                 arg)
{
  int r;
  
  do r = ioctl (fd, request, arg);
  while (-1 == r && EINTR == errno);
  
  return r;
}

static void
process_image                   (const void *           p)
{
  clock_gettime(CLOCK_REALTIME, &entryTime);
  memcpy((void*)&vidCapNow,(const void*)&entryTime,sizeof(struct timespec));
  
  
  if(fileNumber > 0) {
    struct timespec timeDiff = TimeDiff(previousEntryTime, entryTime);    
    double frameTime = (double)timeDiff.tv_nsec / (double)BILLION;    
    meanFrameTime += frameTime;
  }
  
  memcpy((void*)&previousEntryTime,(const void*)&entryTime,sizeof(struct timespec));
  
  if(currentAVIFile == NULL) {
    char fileName[] = "/home/steph/webcam/vid/videoj0000.avi";
    char str[256];
    memset((void*)str,'0',256);
    sprintf(str,"%d",fileNumber);
    if(fileNumber < 10)
      memcpy((void*)(fileName+37),(const void*)str,1);
    else if(fileNumber < 100)
      memcpy((void*)(fileName+36),(const void*)str,2);
    else if(fileNumber < 1000)
      memcpy((void*)(fileName+35),(const void*)str,3);
    else
      memcpy((void*)(fileName+34),(const void*)str,4);
    
    fileNumber++;
    currentAVIFile = fopen(fileName,"wb");
    struct LargeAVIHeader largeAVIHeader;
    char tempFileName[] = "/home/steph/webcam/template/aviHeaderTemplate.hd0";
    FILE* tempFile = fopen(tempFileName,"rb");
    fread((void*)&largeAVIHeader,sizeof(struct LargeAVIHeader),1,tempFile);
    fclose(tempFile);
    largeAVIHeader.riffHeader.bitmapInfoHeader.biWidth = hres;
    largeAVIHeader.riffHeader.bitmapInfoHeader.biHeight = vres;
    
    largeAVIHeader.riffHeader.aviHeader.dwWidth = hres;
    largeAVIHeader.riffHeader.aviHeader.dwHeight = vres;
    
    
    largeAVIHeader.riffHeader.aviStreamHeader.rcFrame.right = (short int)hres;
    largeAVIHeader.riffHeader.aviStreamHeader.rcFrame.bottom = (short int)vres;
    
    fwrite((void*)&largeAVIHeader,sizeof(struct LargeAVIHeader),1,currentAVIFile);
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
    memcpy((void*)&vidCapStart,(const void*)&entryTime,sizeof(struct timespec));
  }
  //   FILE* dataImageFile;
  //   char fileName[] = "//home//gary//testvid//cam//framej0000.jpg";
  //   char str[256];
  //   memset((void*)str,'0',256);
  //char* s = itoa(frameNumber, str, 10);
  //   sprintf(str,"%d",frameNumber);
  //   if(frameNumber < 10)
  //     memcpy((void*)(fileName+37),(const void*)str,1);
  //   else if(frameNumber < 100)
  //     memcpy((void*)(fileName+36),(const void*)str,2);
  //   else if(frameNumber < 1000)
  //     memcpy((void*)(fileName+35),(const void*)str,3);
  //   else
  //     memcpy((void*)(fileName+34),(const void*)str,4);
  
  frameNumber++;
  int chuckSize = 0;
  //dataImageFile = fopen(fileName,"wb");
  char* data = (char*)p;
  int i = 0;
  //fputc ('.', stdout);
  for(;;) {
    unsigned short* temp = (unsigned short*)data;
    //memcpy((void*)&temp,(const void*)data,sizeof(unsigned short));
    data += sizeof(unsigned short);
    //int val = (int)temp;
    //if(i >= 0)
    //printf("%d,",val);
    //fwrite((const void*)&temp,sizeof(unsigned short),1,dataImageFile);
    
    chuckSize+=sizeof(unsigned short);
    if(*temp == 0xd9ff)
      break;
    
    //fputc (data[i], stdout);
  }
  //fputc ('@', stdout);
  //printf("\n");
  
  //fclose(dataImageFile);
  
  char streamId[] = {'0','0','d','c'};
  
  if(currentAVIFile) {
    fwrite((const void*)streamId,sizeof(char),(size_t)4,currentAVIFile);
    fwrite((const void*)&chuckSize,sizeof(int),1,currentAVIFile);
    fwrite((const void*)p,1,chuckSize,currentAVIFile);
  }
  
  
  char* idTxt = indexBlock[framesInFile].ckId;
  memcpy((void*)idTxt,(const void*)streamId,4);
  indexBlock[framesInFile].cOffset = chuckSizeCum;
  indexBlock[framesInFile].cFlags = 16;
  indexBlock[framesInFile].cSize = chuckSize;
  
  chuckSizeCum += (chuckSize+8);
  framesInFile++;
  
  struct timespec timeDiff2 = TimeDiff(vidCapStart, entryTime);
  
  //printf("capture time: %ld\n",timeDiff2.tv_sec);
  
  //if(framesInFile > FRAMES_IN_AVI) {
    if(timeDiff2.tv_sec > 60L) {
      meanFrameTime /= (double)framesInFile;
      double framesPerSec = 1.0 / meanFrameTime;
      printf("fps: %lf\n",framesPerSec);
      
      framesPerSec = round(framesPerSec);
      
      int iFramesPerSec = (int)framesPerSec;
      
      /* if(iFramesPerSec <= 10)
       *		iFramesPerSec = 10;
       *		else if(iFramesPerSec <= 15)
       *		iFramesPerSec = 15;
       *		else if(iFramesPerSec <= 25)
       *		iFramesPerSec = 25;
       *		else
       *		iFramesPerSec = 30;*/
      
      double frameTimeD = 1000000.0/(double)iFramesPerSec;
      
      int microFrameTime = (int)frameTimeD;
      
      int chkSize = framesInFile*sizeof(struct IndxBlock);
      
      char idx[] = {'i','d','x','1'};
      
      fwrite((const void*)idx,sizeof(char),4,currentAVIFile);
      fwrite((const void*)&chkSize,sizeof(int),1,currentAVIFile);
      fwrite((const void*)&indexBlock,sizeof(struct IndxBlock),framesInFile,currentAVIFile);
      
      int totalFileSize = chuckSizeCum + sizeof(struct LargeAVIHeader) + sizeof(int) + chkSize - 8;
      fseek(currentAVIFile,fileSizeOffset,SEEK_SET);
      fwrite((const void*)&totalFileSize,sizeof(DWORD),1,currentAVIFile);
      fseek(currentAVIFile,moviListSizeOffset,SEEK_SET);
      fwrite((const void*)&chuckSizeCum,sizeof(DWORD),1,currentAVIFile);
      
      fseek(currentAVIFile,totalFrameOffset,SEEK_SET);
      fwrite((const void*)&framesInFile,sizeof(DWORD),1,currentAVIFile);
      
      fseek(currentAVIFile,streamLengthOffset,SEEK_SET);
      fwrite((const void*)&framesInFile,sizeof(DWORD),1,currentAVIFile);
      
      fseek(currentAVIFile,frameRateOffset,SEEK_SET);
      fwrite((const void*)&iFramesPerSec,sizeof(DWORD),1,currentAVIFile);
      
      fseek(currentAVIFile,frameTimeOffset,SEEK_SET);
      fwrite((const void*)&microFrameTime,sizeof(DWORD),1,currentAVIFile);
      
      //frameRateOffset = p6 - s1;
      //frameTimeOffset = p5 - s1;
      
      printf("frame time: %d\n",microFrameTime);
      printf("fps out: %d\n",iFramesPerSec);
      framesInFile = 0;
      chuckSizeCum = 4;
      meanFrameTime = 0.0;
      if(currentAVIFile)
	fclose(currentAVIFile);
      
      currentAVIFile = NULL;
      fflush (stdout);
}


}

static int
read_frame                      (void)
{
  struct v4l2_buffer buf;
  unsigned int i;
  
  switch (io) {
    case IO_METHOD_READ:
      if (-1 == read (fd, buffers[0].start, buffers[0].length)) {
	switch (errno) {
	  case EAGAIN:
	    return 0;
	    
	  case EIO:
	    /* Could ignore EIO, see spec. */
	    
	    /* fall through */
	    
	    default:
	      errno_exit ("read");
	}
      }
      
      process_image (buffers[0].start);
      
      break;
      
	    case IO_METHOD_MMAP:
	      CLEAR (buf);
	      
	      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	      buf.memory = V4L2_MEMORY_MMAP;
	      
	      if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		  case EAGAIN:
		    return 0;
		    
		  case EIO:
		    /* Could ignore EIO, see spec. */
		    
		    /* fall through */
		    
		    default:
		      errno_exit ("VIDIOC_DQBUF");
		}
	      }
	      
	      assert (buf.index < n_buffers);
	      
	      process_image (buffers[buf.index].start);
	      
	      if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
		errno_exit ("VIDIOC_QBUF");
	      
	      break;
	      
		    case IO_METHOD_USERPTR:
		      CLEAR (buf);
		      
		      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		      buf.memory = V4L2_MEMORY_USERPTR;
		      
		      if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			  case EAGAIN:
			    return 0;
			    
			  case EIO:
			    /* Could ignore EIO, see spec. */
			    
			    /* fall through */
			    
			    default:
			      errno_exit ("VIDIOC_DQBUF");
			}
		      }
		      
		      for (i = 0; i < n_buffers; ++i)
			if (buf.m.userptr == (unsigned long) buffers[i].start
			  && buf.length == buffers[i].length)
			  break;
			
			assert (i < n_buffers);
		      
		      process_image ((void *) buf.m.userptr);
		      
		      if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");
		      
		      break;
  }
  
  return 1;
}

static void
mainloop                        (void)
{
  unsigned int count;
  
  count = 1000;
  
  while (count-- > 0) {
    for (;;) {
      fd_set fds;
      struct timeval tv;
      int r;
      
      FD_ZERO (&fds);
      FD_SET (fd, &fds);
      
      /* Timeout. */
      tv.tv_sec = 2;
      tv.tv_usec = 0;
      
      r = select (fd + 1, &fds, NULL, NULL, &tv);
      
      if (-1 == r) {
	if (EINTR == errno)
	  continue;
	
	errno_exit ("select");
      }
      
      if (0 == r) {
	fprintf (stderr, "select timeout\n");
	exit (EXIT_FAILURE);
      }
      
      if (read_frame ())
	break;
      
      /* EAGAIN - continue select loop. */
    }
  }
}

static void
stop_capturing                  (void)
{
  enum v4l2_buf_type type;
  
  switch (io) {
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;
      
    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      
      if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
	errno_exit ("VIDIOC_STREAMOFF");
      
      break;
  }
}

static void
start_capturing                 (void)
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
	
	CLEAR (buf);
	
	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory      = V4L2_MEMORY_MMAP;
	buf.index       = i;
	
	if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
	  errno_exit ("VIDIOC_QBUF");
      }
      
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      
      if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
	errno_exit ("VIDIOC_STREAMON");
      
      break;
      
    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i) {
	struct v4l2_buffer buf;
	
	CLEAR (buf);
	
	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory      = V4L2_MEMORY_USERPTR;
	buf.index       = i;
	buf.m.userptr   = (unsigned long) buffers[i].start;
	buf.length      = buffers[i].length;
	
	if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
	  errno_exit ("VIDIOC_QBUF");
      }
      
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      
      if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
	errno_exit ("VIDIOC_STREAMON");
      
      break;
  }
}

static void
uninit_device                   (void)
{
  unsigned int i;
  
  switch (io) {
    case IO_METHOD_READ:
      free (buffers[0].start);
      break;
      
    case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i)
	if (-1 == munmap (buffers[i].start, buffers[i].length))
	  errno_exit ("munmap");
	break;
      
    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i)
	free (buffers[i].start);
      break;
  }
  
  free (buffers);
}

static void
init_read                       (unsigned int           buffer_size)
{
  buffers = calloc (1, sizeof (*buffers));
  
  if (!buffers) {
    fprintf (stderr, "Out of memory\n");
    exit (EXIT_FAILURE);
  }
  
  buffers[0].length = buffer_size;
  buffers[0].start = malloc (buffer_size);
  
  if (!buffers[0].start) {
    fprintf (stderr, "Out of memory\n");
    exit (EXIT_FAILURE);
  }
}

static void
init_mmap                       (void)
{
  struct v4l2_requestbuffers req;
  
  CLEAR (req);
  
  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_MMAP;
  
  if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      fprintf (stderr, "%s does not support "
      "memory mapping\n", dev_name);
      exit (EXIT_FAILURE);
    } else {
      errno_exit ("VIDIOC_REQBUFS");
    }
  }
  
  if (req.count < 2) {
    fprintf (stderr, "Insufficient buffer memory on %s\n",
	     dev_name);
    exit (EXIT_FAILURE);
  }
  
  buffers = calloc (req.count, sizeof (*buffers));
  
  if (!buffers) {
    fprintf (stderr, "Out of memory\n");
    exit (EXIT_FAILURE);
  }
  
  for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
    struct v4l2_buffer buf;
    
    CLEAR (buf);
    
    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = n_buffers;
    
    if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
      errno_exit ("VIDIOC_QUERYBUF");
    
    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start =
    mmap (NULL /* start anywhere */,
	  buf.length,
	  PROT_READ | PROT_WRITE /* required */,
	  MAP_SHARED /* recommended */,
	  fd, buf.m.offset);
    
    if (MAP_FAILED == buffers[n_buffers].start)
      errno_exit ("mmap");
  }
}

static void
init_userp                      (unsigned int           buffer_size)
{
  struct v4l2_requestbuffers req;
  unsigned int page_size;
  
  page_size = getpagesize ();
  buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);
  
  CLEAR (req);
  
  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_USERPTR;
  
  if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      fprintf (stderr, "%s does not support "
      "user pointer i/o\n", dev_name);
      exit (EXIT_FAILURE);
    } else {
      errno_exit ("VIDIOC_REQBUFS");
    }
  }
  
  buffers = calloc (4, sizeof (*buffers));
  
  if (!buffers) {
    fprintf (stderr, "Out of memory\n");
    exit (EXIT_FAILURE);
  }
  
  for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
    buffers[n_buffers].length = buffer_size;
    buffers[n_buffers].start = memalign (/* boundary */ page_size,
					 buffer_size);
    
    if (!buffers[n_buffers].start) {
      fprintf (stderr, "Out of memory\n");
      exit (EXIT_FAILURE);
    }
  }
}

static void
init_device                     (void)
{
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;
  
  if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      fprintf (stderr, "%s is no V4L2 device\n",
	       dev_name);
      exit (EXIT_FAILURE);
    } else {
      errno_exit ("VIDIOC_QUERYCAP");
    }
  }
  
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf (stderr, "%s is no video capture device\n",
	     dev_name);
    exit (EXIT_FAILURE);
  }
  
  switch (io) {
    case IO_METHOD_READ:
      if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
	fprintf (stderr, "%s does not support read i/o\n",
		 dev_name);
	exit (EXIT_FAILURE);
      }
      
      break;
      
    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
	fprintf (stderr, "%s does not support streaming i/o\n",
		 dev_name);
	exit (EXIT_FAILURE);
      }
      
      break;
  }
  
  
  /* Select video input, video standard and tune here. */
  
  
  CLEAR (cropcap);
  
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */
    
    if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
      switch (errno) {
	case EINVAL:
	  /* Cropping not supported. */
	  break;
	default:
	  /* Errors ignored. */
	  break;
      }
    }
  } else {
    /* Errors ignored. */
  }
  
  
  CLEAR (fmt);
  
  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = hres;
  fmt.fmt.pix.height      = vres;
  //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.pixelformat = 0x47504a4d;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  int formatTest = 0x56595559;
  formatTest = V4L2_PIX_FMT_YUYV;
  //fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  
  if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
    errno_exit ("VIDIOC_S_FMT");
  
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
      init_read (fmt.fmt.pix.sizeimage);
      break;
      
    case IO_METHOD_MMAP:
      init_mmap ();
      break;
      
    case IO_METHOD_USERPTR:
      init_userp (fmt.fmt.pix.sizeimage);
      break;
  }
}

static void
close_device                    (void)
{
  if (-1 == close (fd))
    errno_exit ("close");
  
  fd = -1;
}

static void
open_device                     (void)
{
  struct stat st;
  
  if (-1 == stat (dev_name, &st)) {
    fprintf (stderr, "Cannot identify '%s': %d, %s\n",
	     dev_name, errno, strerror (errno));
    exit (EXIT_FAILURE);
  }
  
  if (!S_ISCHR (st.st_mode)) {
    fprintf (stderr, "%s is no device\n", dev_name);
    exit (EXIT_FAILURE);
  }
  
  fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
  
  if (-1 == fd) {
    fprintf (stderr, "Cannot open '%s': %d, %s\n",
	     dev_name, errno, strerror (errno));
    exit (EXIT_FAILURE);
  }
}

static void
usage                           (FILE *                 fp,
				 int                    argc,
				 char **                argv)
{
  fprintf (fp,
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

static const char short_options [] = "d:hmru";

static const struct option
long_options [] = {
  { "device",     required_argument,      NULL,           'd' },
  { "help",       no_argument,            NULL,           'h' },
  { "mmap",       no_argument,            NULL,           'm' },
  { "read",       no_argument,            NULL,           'r' },
  { "userp",      no_argument,            NULL,           'u' },
  { 0, 0, 0, 0 }
};

int
main(int argc, char** argv)
{
  dev_name = "/dev/video0";
  
  for (;;) {
    int index;
    int c;
    
    c = getopt_long (argc, argv,
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
	usage (stdout, argc, argv);
	exit (EXIT_SUCCESS);
	
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
	usage (stderr, argc, argv);
	exit (EXIT_FAILURE);
    }
  }
  
  open_device ();
  
  init_device ();
  
  start_capturing ();
  
  mainloop ();
  
  stop_capturing ();
  
  uninit_device ();
  
  close_device ();
  
  exit (EXIT_SUCCESS);
  
  return 0;
}
