///> Include FFMpeg
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable:4996)

#include <iostream>

///> Include FFMpeg
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>
}

///> Library Link On Windows System
#pragma comment( lib, "avformat.lib" )   
#pragma comment( lib, "avutil.lib" )
#pragma comment( lib, "avcodec.lib" )

class FileWriter
 { 
 public:
    FileWriter();
    virtual ~FileWriter();

     int openFile(char *filename, int codec, int width, int height);
     int writeFile(enum AVMediaType type, char *buf, int len);
     void closeFile();

 protected:
     AVStream* addVideoStream(enum CodecID codec_id, int width, int height);
     int writeVideo(char *buf, int len);
     int writeAudio(char *buf, int len);

 protected:
     AVFormatContext *m_pFormatContext;
     AVStream  *m_pVideoStream;
     AVPacket  m_avPacket;

     enum CodecID m_nCodecID;

     __int64 m_nFrameCount; 
 };