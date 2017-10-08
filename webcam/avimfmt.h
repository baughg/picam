#define FOURCC char
#define DWORD unsigned int
#define WORD unsigned short
#define LONG DWORD

typedef struct tagBITMAPINFOHEADER {
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct _avistreamheader {
	FOURCC fcc[4];
	DWORD  cb;
	FOURCC fccType[4];
	FOURCC fccHandler[4];
	DWORD  dwFlags;
	WORD   wPriority;
	WORD   wLanguage;
	DWORD  dwInitialFrames;
	DWORD  dwScale;
	DWORD  dwRate;
	DWORD  dwStart;
	DWORD  dwLength;
	DWORD  dwSuggestedBufferSize;
	DWORD  dwQuality;
	DWORD  dwSampleSize;
	struct {
		short int left;
		short int top;
		short int right;
		short int bottom;
	} rcFrame;
} AVISTREAMHEADER;

typedef struct _avimainheader {
	FOURCC fcc[4];
	DWORD  cb;
	DWORD  dwMicroSecPerFrame;
	DWORD  dwMaxBytesPerSec;
	DWORD  dwPaddingGranularity;
	DWORD  dwFlags;
	DWORD  dwTotalFrames;
	DWORD  dwInitialFrames;
	DWORD  dwStreams;
	DWORD  dwSuggestedBufferSize;
	DWORD  dwWidth;
	DWORD  dwHeight;
	DWORD  dwReserved[4];
} AVIMAINHEADER;

struct RiffHeader {
	char fCode[4]; 
	DWORD fileSize;
	char aType[4];
	char listH1[4];
	DWORD hdrlListSize;
	char hdrlName[4];  
	struct _avimainheader aviHeader;
	char listH2[4];
	DWORD strlListSize;
	char strlName[4];
	struct _avistreamheader aviStreamHeader;
	char strfName[4];
	DWORD strfLength;
	struct tagBITMAPINFOHEADER bitmapInfoHeader;
	unsigned char junk[4392];
};

struct InfoSetHeader {
	char listH3[4];
	DWORD infoSetListSize;
	unsigned char junk[24];
	char junkMarker[4];
	DWORD junkSize;
	unsigned char moreJunk[1016];
};

struct MoviList {
	char listH4[4];
	DWORD moviListSize;
	char moviName[4];
	//char streamId[4];
};



struct  LargeAVIHeader{
	struct RiffHeader riffHeader;
	struct InfoSetHeader infoSetHeader;
	struct MoviList moviList;
};

struct IndxBlock {
	char ckId[4];
	DWORD cFlags;
	DWORD cOffset;
	DWORD cSize;
};

struct _pcm_audio_header {  
  int fmt_len;
  short format;
  short channels;
  int sample_rate;
  int bytes_per_sec;
  short bytes_per_sam;
  short bits_per_sam;
};

struct RiffHeaderAV {
	char fCode[4]; 
	DWORD fileSize;
	char aType[4];
	char listH1[4];
	DWORD hdrlListSize;
	char hdrlName[4];  
	struct _avimainheader aviHeader;
	char listH2[4];
	DWORD strlListSize;
	char strlName[4];
	struct _avistreamheader aviStreamHeader;
	char strfName[4];
	DWORD strfLength;
	struct tagBITMAPINFOHEADER bitmapInfoHeader;
	unsigned char junk[4124]; // av
	//unsigned char junk[4200]; //peep
	char listH_aud[4];
	DWORD strlListSize_aud;
	char strlName_aud[4];
	struct _avistreamheader aviStreamHeader_aud;
	char strf_tag_aud[4];
	struct _pcm_audio_header pcm_audio_header;
	unsigned char junk_aud[4392];
	//unsigned char junk[4392];
};


// #define FOURCC char
// #define DWORD unsigned int
// #define WORD unsigned short
// #define LONG DWORD
// 
// typedef struct tagBITMAPINFOHEADER {
//   DWORD biSize;
//   LONG  biWidth;
//   LONG  biHeight;
//   WORD  biPlanes;
//   WORD  biBitCount;
//   DWORD biCompression;
//   DWORD biSizeImage;
//   LONG  biXPelsPerMeter;
//   LONG  biYPelsPerMeter;
//   DWORD biClrUsed;
//   DWORD biClrImportant;
// } BITMAPINFOHEADER;
// 
// typedef struct _avistreamheader {
//   FOURCC fcc[4];
//   DWORD  cb;
//   FOURCC fccType[4];
//   FOURCC fccHandler[4];
//   DWORD  dwFlags;
//   WORD   wPriority;
//   WORD   wLanguage;
//   DWORD  dwInitialFrames;
//   DWORD  dwScale;
//   DWORD  dwRate;
//   DWORD  dwStart;
//   DWORD  dwLength;
//   DWORD  dwSuggestedBufferSize;
//   DWORD  dwQuality;
//   DWORD  dwSampleSize;
//   struct {
//     short int left;
//     short int top;
//     short int right;
//     short int bottom;
//   } rcFrame;
// } AVISTREAMHEADER;
// 
// typedef struct _avimainheader {
//   FOURCC fcc[4];
//   DWORD  cb;
//   DWORD  dwMicroSecPerFrame;
//   DWORD  dwMaxBytesPerSec;
//   DWORD  dwPaddingGranularity;
//   DWORD  dwFlags;
//   DWORD  dwTotalFrames;
//   DWORD  dwInitialFrames;
//   DWORD  dwStreams;
//   DWORD  dwSuggestedBufferSize;
//   DWORD  dwWidth;
//   DWORD  dwHeight;
//   DWORD  dwReserved[4];
// } AVIMAINHEADER;
// 
// struct RiffHeader {
//   char fCode[4]; 
//   DWORD fileSize;
//   char aType[4];
//   char listH1[4];
//   DWORD hdrlListSize;
//   char hdrlName[4];  
//   struct _avimainheader aviHeader;
//   char listH2[4];
//   DWORD strlListSize;
//   char strlName[4];
//   struct _avistreamheader aviStreamHeader;
//   char strfName[4];
//   DWORD strfLength;
//   struct tagBITMAPINFOHEADER bitmapInfoHeader;
//   unsigned char junk[4392];
// };
// 
// struct InfoSetHeader {
//   char listH3[4];
//   DWORD infoSetListSize;
//   unsigned char junk[24];
//   char junkMarker[4];
//   DWORD junkSize;
//   unsigned char moreJunk[1016];
// };
// 
// struct MoviList {
//   char listH4[4];
//   DWORD moviListSize;
//   char moviName[4];
//   //char streamId[4];
// };
// 
// struct  LargeAVIHeader{
//   struct RiffHeader riffHeader;
//   struct InfoSetHeader infoSetHeader;
//   struct MoviList moviList;
// };

struct  LargeAVIHeaderAV{
	struct RiffHeaderAV riffHeader;
	struct InfoSetHeader infoSetHeader;
	struct MoviList moviList;
};