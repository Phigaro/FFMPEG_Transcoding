///> Include FFMpeg
#pragma once
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable:4996)

///> Include FFMpeg
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <iostream>
using namespace std;
#define NUM 1000


extern "C" {
#include <libavformat/avformat.h>		// Basic Format manage header
#include <libavcodec/avcodec.h>			// For using codec header
#include <libavutil/avutil.h>			// AV Convenience header
#include <libavutil\imgutils.h>			// Img Convenience header
#include <libswscale/swscale.h>			// External API header
#include <libavformat/avio.h>			// Input & output header
#include <libavutil/opt.h>				// option header
#include <libavutil/mathematics.h>		// math header
#include <libswresample/swresample.h>	// Resample header
}

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480 //20480
#define AUDIO_REFILL_THRESH 4096
#define STREAM_DURATION   200.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_NB_FRAMES  ((int)(STREAM_DURATION * STREAM_FRAME_RATE))
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

static int sws_flags = SWS_BICUBIC;

///> Library Link On Windows System
#pragma comment( lib, "avformat.lib" )   
#pragma comment( lib, "avutil.lib" )
#pragma comment( lib, "avcodec.lib" )

const char *szFilePath = "./Test_Video.mkv";

const char *szSaveDecodedVideoFilePath = "./decodedVideo.yuv";
const char *AVMediaType2Str(AVMediaType type);
const char *AVCodecID2Str(AVCodecID id);
void Stream_Info(AVFormatContext* pFmtCtx);
void encoding(AVFrame* pVFrame, AVPacket packet);
void audio_encoding(AVFrame* pAFrame, AVPacket packet);
void audio_encoding_ex();


// Video_var
AVFrame *picture, *tmp_picture;
uint8_t *video_outbuf;
int frame_count, video_outbuf_size;

// Video encode_var
AVCodec *codec;						// codec for encode
AVCodecContext* c = NULL;			// Codec Context
int i, ret, x, y, got_output;
FILE *f;
AVFrame* frame;
AVPacket pkt;
uint8_t endcode[] = { 0,0,1,0xb7 };
AVCodecID Codec_id = AV_CODEC_ID_MPEG2VIDEO;

// Audio encode_var
AVCodec *Audio_codec;
AVCodecContext *Audio_codec_c = NULL;
AVFrame *Audio_frame;
AVPacket Audio_pkt;
int Audio_i, Audio_j, Audio_k, Audio_ret, Audio_got_output;
int Audio_buffer_size;
FILE *Audio_f;
uint16_t *Audio_samples;
AVCodecID Audio_Codec_id = AV_CODEC_ID_MP2;

/* Add an output stream. */
static AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id) {
	AVCodecContext *c;
	AVStream *st;
	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id)
		);
		exit(1);
	}
	st = avformat_new_stream(oc, *codec);
	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	st->id = oc->nb_streams - 1;
	c = st->codec;
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt = AV_SAMPLE_FMT_FLTP;
		c->bit_rate = 64000;
		c->sample_rate = 44100;
		c->channels = 2;
		break;
	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		c->bit_rate = 400000;
		/* Resolution must be a multiple of two. */
		c->width = 352;
		c->height = 288;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		* of which frame timestamps are represented. For fixed-fps content,
		* timebase should be 1/framerate and timestamp increments should be
		* identical to 1. */
		c->time_base.den = STREAM_FRAME_RATE;
		c->time_base.num = 1;
		c->gop_size = 12;
		/* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			* This does not happen with normal video, it just happens here as
			* the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		break;
	default:
		break;
	}    /* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	return st;
}

/**************************************************************//* video output */
static AVPicture src_picture, dst_picture;
static void open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st) {
	int ret;
	AVCodecContext *c = st->codec;
	/* open the codec */
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		fprintf(stderr, "Could not open video codec: %s\n", av_make_error_string((char*)AV_ERROR_MAX_STRING_SIZE, AV_ERROR_MAX_STRING_SIZE, ret));
		exit(1);
	}
	/* allocate and init a re-usable frame */
	frame = avcodec_alloc_frame();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}    /* Allocate the encoded raw picture. */
	ret = avpicture_alloc(&dst_picture, c->pix_fmt, c->width, c->height);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate picture: %s\n", av_make_error_string((char*)AV_ERROR_MAX_STRING_SIZE, AV_ERROR_MAX_STRING_SIZE, ret));
		exit(1);
	}
	/* If the output format is not YUV420P, then a temporary YUV420P
	* picture is needed too. It is then converted to the required
	* output format. */
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		ret = avpicture_alloc(&src_picture, AV_PIX_FMT_YUV420P, c->width, c->height);
		if (ret < 0) {
			fprintf(stderr, "Could not allocate temporary picture: %s\n", av_make_error_string((char*)AV_ERROR_MAX_STRING_SIZE, AV_ERROR_MAX_STRING_SIZE, ret));
			exit(1);
		}
	}    /* copy data and linesize picture pointers to frame */
	*((AVPicture *)frame) = dst_picture;
}
/* Prepare a dummy image. */

static void fill_yuv_image(AVPicture *pict, int frame_index, int width, int height) {
	int x, y, i;
	i = frame_index;
	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}

static void write_video_frame(AVFormatContext *oc, AVStream *st) {
	int ret;    static struct SwsContext *sws_ctx;    AVCodecContext *c = st->codec;
	if (frame_count >= STREAM_NB_FRAMES) {
		/* No more frames to compress. The codec has a latency of a few
		* frames if using B-frames, so we get the last frames by
		* passing the same picture again. */
	}
	else {
		if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
			/* as we only generate a YUV420P picture, we must convert it
			* to the codec pixel format if needed */
			if (!sws_ctx) {
				sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_YUV420P, c->width, c->height, c->pix_fmt, sws_flags, NULL, NULL, NULL);
				if (!sws_ctx) {
					fprintf(stderr, "Could not initialize the conversion context\n");
					exit(1);
				}
			}
			fill_yuv_image(&src_picture, frame_count, c->width, c->height);
			sws_scale(sws_ctx, (const uint8_t * const *)src_picture.data, src_picture.linesize, 0, c->height, dst_picture.data, dst_picture.linesize);
		}
		else {
			fill_yuv_image(&dst_picture, frame_count, c->width, c->height);
		}
	}
	if (oc->oformat->flags & AVFMT_RAWPICTURE) {
		/* Raw video case - directly store the picture in the packet */
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = st->index;
		pkt.data = dst_picture.data[0];
		pkt.size = sizeof(AVPicture);
		ret = av_interleaved_write_frame(oc, &pkt);
	}
	else {
		AVPacket pkt = { 0 };
		int got_packet;
		av_init_packet(&pkt);
		/* encode the image */
		ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
		if (ret < 0) {
			fprintf(stderr, "Error encoding video frame: %s\n", av_make_error_string((char*)AV_ERROR_MAX_STRING_SIZE, AV_ERROR_MAX_STRING_SIZE, ret));
			exit(1);
		}
		/* If size is zero, it means the image was buffered. */
		if (!ret && got_packet && pkt.size) {
			pkt.stream_index = st->index;
			/* Write the compressed frame to the media file. */
			ret = av_interleaved_write_frame(oc, &pkt);
		}
		else {
			ret = 0;
		}
	}    if (ret != 0) {
		fprintf(stderr, "Error while writing video frame: %s\n", av_make_error_string((char*)AV_ERROR_MAX_STRING_SIZE, AV_ERROR_MAX_STRING_SIZE, ret)
		);
		exit(1);
	}
	frame_count++;

}

static void close_video(AVFormatContext *oc, AVStream *st) {
	avcodec_close(st->codec);
	av_free(src_picture.data[0]);
	av_free(dst_picture.data[0]);
	av_free(frame);
}

static int check_sample_fmt(AVCodec *codec, enum AVSampleFormat sample_fmt)
{
	const enum AVSampleFormat *p = codec->sample_fmts;

	while (*p != AV_SAMPLE_FMT_NONE) {
		if (*p == sample_fmt)
			return 1;
		p++;
	}
	return 0;
}

/* just pick the highest supported samplerate */
static int select_sample_rate(AVCodec *codec)
{
	const int *p;
	int best_samplerate = 0;

	if (!codec->supported_samplerates)
		return 44100;

	p = codec->supported_samplerates;
	while (*p) {
		best_samplerate = FFMAX(*p, best_samplerate);
		p++;
	}
	return best_samplerate;
}

/* select layout with the highest channel count */
static int select_channel_layout(AVCodec *codec)
{
	const uint64_t *p;
	uint64_t best_ch_layout = 0;
	int best_nb_channells = 0;

	if (!codec->channel_layouts)
		return AV_CH_LAYOUT_STEREO;

	p = codec->channel_layouts;
	while (*p) {
		int nb_channels = av_get_channel_layout_nb_channels(*p);

		if (nb_channels > best_nb_channells) {
			best_ch_layout = *p;
			best_nb_channells = nb_channels;
		}
		p++;
	}
	return best_ch_layout;
}

void Audio_encode_alloc(const char* audio_filename) {



	/* find the MP2 encoder */
	Audio_codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
	if (!Audio_codec) {
		fprintf(stderr, "Audio_codec not found\n");
		system("pause"); exit(1);
	}

	Audio_codec_c = avcodec_alloc_context3(Audio_codec);
	if (!Audio_codec_c) {
		fprintf(stderr, "Could not allocate audio Audio_codec context\n");
		system("pause"); exit(1);
	}

	/* put sample parameters */
	Audio_codec_c->bit_rate = 64000;

	/* check that the encoder supports s16 pcm input */
	Audio_codec_c->sample_fmt = AV_SAMPLE_FMT_S16;
	if (!check_sample_fmt(Audio_codec, Audio_codec_c->sample_fmt)) {
		fprintf(stderr, "Encoder does not support sample format %s",
			av_get_sample_fmt_name(Audio_codec_c->sample_fmt));
		system("pause"); exit(1);
	}

	/* select other audio parameters supported by the encoder */
	Audio_codec_c->sample_rate = select_sample_rate(Audio_codec);
	Audio_codec_c->channel_layout = select_channel_layout(Audio_codec);
	Audio_codec_c->channels = av_get_channel_layout_nb_channels(Audio_codec_c->channel_layout);

	/* open it */
	if (avcodec_open2(Audio_codec_c, Audio_codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		system("pause"); exit(1);
	}

	Audio_f = fopen(audio_filename, "wb");
	if (!Audio_f) {
		fprintf(stderr, "Could not open %s\n", audio_filename);
		system("pause"); exit(1);
	}
}

void encode_alloc(const char* filename) {
	// 강제로 H264로 인코딩
	codec = avcodec_find_encoder(Codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		system("pause"); exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		system("pause"); exit(1);
	}

	c->bit_rate = 500000;

	//1280 528

	c->width = 1280;
	c->height = 528;

	c->time_base = { 1,25 };

	c->gop_size = 10; /* emit one intra frame every ten frames */
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	if (Codec_id == AV_CODEC_ID_H264) {
		av_opt_set(c->priv_data, "preset", "slow", 0);
	}

	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		system("pause"); exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open &s\n", filename);
		system("pause"); exit(1);
	}
}

int main() {
	int argc;
	argc = 2;
	char **argv = new char*;
	argv[0] = "./Test_Video.mkv";
	argv[1] = "./111.sw";

	const char *filename;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVStream *audio_st, *video_st;
	AVCodec *audio_codec, *video_codec;
	video_codec = new AVCodec;
	double audio_time, video_time;
	int ret;

	/* Initialize libavcodec, and register all codecs and formats. */
	av_register_all();
	if (argc != 2) {
		printf("usage: %s output_file\n"
			"API example program to output a media file with libavformat.\n"
			"This program generates a synthetic audio and video stream, encodes and\n"
			"muxes them into a file named output_file.\n"
			"The output format is automatically guessed according to the file extension.\n"
			"Raw images can also be output by using '%%d' in the filename.\n"
			"\n", argv[0]);
		return 1;
	}

	filename = argv[1];

	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, NULL, NULL, filename);
	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}
	if (!oc) {
		return 1;
	}

	fmt = oc->oformat;

	/* Add the audio and video streams using the default format codecs*/
	/* and initialize the codecs. */
	video_st = NULL;
	audio_st = NULL;

	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		video_st = add_stream(oc, &video_codec, fmt->video_codec);
	}

	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		audio_st = add_stream(oc, &audio_codec, fmt->audio_codec);
	}

	/* Now that all the parameters are set, we can open the audio and
	* video codecs and allocate the necessary encode buffers. */

	if (video_st)
		open_video(oc, video_codec, video_st);
	if (audio_st)
		//open_audio(oc, audio_codec, audio_st); 

		av_dump_format(oc, 0, filename, 1);

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open '%s': %s\n", filename);
			return 1;
		}
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file: %s\n");
		return 1;
	}

	int Cnt = 0;

	if (frame)
		frame->pts = 0;
	for (;;) {
		/* Compute current audio and video time. */
		audio_time = audio_st ? audio_st->pts.val * av_q2d(audio_st->time_base) : 0.0;
		video_time = video_st ? video_st->pts.val * av_q2d(video_st->time_base) : 0.0;
		Cnt++;
		if (Cnt < 10)
			break;
		if ((!video_st || video_time >= STREAM_DURATION))
			break;
		/* write interleaved audio and video frames */
		if (!video_st || (video_st && audio_st && audio_time < video_time)) {
			//write_audio_frame(oc, audio_st);
		}
		else {
			write_video_frame(oc, video_st);
			frame->pts += av_rescale_q(1, video_st->codec->time_base, video_st->time_base);
		}
	}
	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */
	av_write_trailer(oc);

	/* Close each codec. */
	if (video_st)
		close_video(oc, video_st);
	if (audio_st)
		//close_audio(oc, audio_st);  
		if (!(fmt->flags & AVFMT_NOFILE))
			/* Close the output file. */
			avio_close(oc->pb);

	/* free the stream */
	avformat_free_context(oc);

	return 0;
}

void encoding(AVFrame* pVFrame, AVPacket packet) {
	av_init_packet(&packet);
	packet.data = NULL;
	// packet data will be allocated by the encoder
	packet.size = 0;
	fflush(stdout);

	/* encode the image */
	// 중요 !!!!!!!!!!!
	// 프레임 단위 별로 나온다.
	// 이 프레임을 packet으로 바꿔줘야함.

	//uint8_t* ptr = pVFrame->data[0];
	//for (y = 0; y < c->height / 2; y++) {
	//	for (x = 0; x < c->width / 2; x++) {
	//		*ptr = 0;
	//	ptr++;
	//		*ptr = 0;
	//		ptr++;
	//	}
	//}

	ret = avcodec_encode_video2(c, &packet, pVFrame, &got_output);
	if (ret < 0) {
		fprintf(stderr, "Error encoding frame\n");
		system("pause"); exit(1);
	}
	if (got_output) {
		printf("Write frame %3d (size=%5d)\n", i, packet.size);
		fwrite(packet.data, 1, packet.size, f);
		if (!ret && got_output && packet.size) {
			packet.stream_index = 0;
			/* Write the compressed frame to the media file. */
			//ret = av_interleaved_write_frame(oc, &packet);
		}
		av_free_packet(&packet);
	}

	for (got_output = 1; got_output; i++) {
		fflush(stdout);

		ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			system("pause"); exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d) \n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_free_packet(&pkt);
		}
	}
	av_free_packet(&packet);
}

void audio_encoding(AVFrame* pAFrame, AVPacket packet) {

	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;
	fflush(stdout);

	pAFrame->nb_samples = Audio_codec_c->frame_size;
	pAFrame->format = Audio_codec_c->sample_fmt;
	pAFrame->channel_layout = Audio_codec_c->channel_layout;

	ret = avcodec_encode_audio2(Audio_codec_c, &packet, pAFrame, &got_output);
	if (ret < 0) {
		fprintf(stderr, "Error encoding frame\n");
		system("pause"); exit(1);
	}
	if (got_output) {
		fwrite(packet.data, 1, packet.size, Audio_f);
		av_free_packet(&packet);
	}

	for (got_output = 1; got_output; i++) {
		ret = avcodec_encode_audio2(Audio_codec_c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding Audio_frame\n");
			system("pause"); exit(1);
		}
		if (got_output) {
			fwrite(pkt.data, 1, pkt.size, Audio_f);
			av_free_packet(&pkt);
		}
	}

	av_freep(&Audio_samples);
	avcodec_free_frame(&Audio_frame);
}

///> Print Stream Information
void Stream_Info(AVFormatContext* pFmtCtx) {
	int i;
	for (i = 0; i < pFmtCtx->nb_streams; i++) {
		AVStream *pStream = pFmtCtx->streams[i];
		const char *szType = AVMediaType2Str(pStream->codec->codec_type);
		const char *szCodecName = AVCodecID2Str(pStream->codec->codec_id);

		av_log(NULL, AV_LOG_INFO, "    > Stream[%d]: %s: %s ", i, szType, szCodecName);
		if (pStream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			av_log(NULL, AV_LOG_INFO, "%dx%d (%.2f fps)", pStream->codec->width, pStream->codec->height, av_q2d(pStream->r_frame_rate));
		}
		else if (pStream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			av_log(NULL, AV_LOG_INFO, "%d Hz", pStream->codec->sample_rate);
		}
		av_log(NULL, AV_LOG_INFO, "\n");
	}
}

const char *AVMediaType2Str(AVMediaType type)
{
	switch (type)
	{
	case AVMEDIA_TYPE_VIDEO:
		return "Video";
	case AVMEDIA_TYPE_AUDIO:
		return "Audio";
	case AVMEDIA_TYPE_SUBTITLE:
		return "Subtitle";
	case AVMEDIA_TYPE_ATTACHMENT:
		return "Attachment";
	}
	return "Unknown";
}
const char *AVCodecID2Str(AVCodecID id)
{
	switch (id)
	{
		/* video codecs */
	case AV_CODEC_ID_MPEG1VIDEO:						return "MPEG1VIDEO";
	case AV_CODEC_ID_MPEG2VIDEO:                        return "MPEG2VIDEO";
	case AV_CODEC_ID_MPEG2VIDEO_XVMC:                   return "MPEG2VIDEO_XVMC";
	case AV_CODEC_ID_H261:                              return "H261";
	case AV_CODEC_ID_H263:                              return "H263";
	case AV_CODEC_ID_RV10:                              return "RV10";
	case AV_CODEC_ID_RV20:                              return "RV20";
	case AV_CODEC_ID_MJPEG:                             return "MJPEG";
	case AV_CODEC_ID_MJPEGB:                            return "MJPEGB";
	case AV_CODEC_ID_LJPEG:                             return "LJPEG";
	case AV_CODEC_ID_SP5X:                              return "SP5X";
	case AV_CODEC_ID_JPEGLS:                            return "JPEGLS";
	case AV_CODEC_ID_MPEG4:                             return "MPEG4";
	case AV_CODEC_ID_RAWVIDEO:                          return "RAWVIDEO";
	case AV_CODEC_ID_MSMPEG4V1:                         return "MSMPEG4V1";
	case AV_CODEC_ID_MSMPEG4V2:                         return "MSMPEG4V2";
	case AV_CODEC_ID_MSMPEG4V3:                         return "MSMPEG4V3";
	case AV_CODEC_ID_WMV1:                              return "WMV1";
	case AV_CODEC_ID_WMV2:                              return "WMV2";
	case AV_CODEC_ID_H263P:                             return "H263P";
	case AV_CODEC_ID_H263I:                             return "H263I";
	case AV_CODEC_ID_FLV1:                              return "FLV1";
	case AV_CODEC_ID_SVQ1:                              return "SVQ1";
	case AV_CODEC_ID_SVQ3:                              return "SVQ3";
	case AV_CODEC_ID_DVVIDEO:                           return "DVVIDEO";
	case AV_CODEC_ID_HUFFYUV:                           return "HUFFYUV";
	case AV_CODEC_ID_CYUV:                              return "CYUV";
	case AV_CODEC_ID_H264:                              return "H264";
	case AV_CODEC_ID_INDEO3:                            return "INDEO3";
	case AV_CODEC_ID_VP3:                               return "VP3";
	case AV_CODEC_ID_THEORA:                            return "THEORA";
	case AV_CODEC_ID_ASV1:                              return "ASV1";
	case AV_CODEC_ID_ASV2:                              return "ASV2";
	case AV_CODEC_ID_FFV1:                              return "FFV1";
	case AV_CODEC_ID_4XM:                               return "4XM";
	case AV_CODEC_ID_VCR1:                              return "VCR1";
	case AV_CODEC_ID_CLJR:                              return "CLJR";
	case AV_CODEC_ID_MDEC:                              return "MDEC";
	case AV_CODEC_ID_ROQ:                               return "ROQ";
	case AV_CODEC_ID_INTERPLAY_VIDEO:                   return "INTERPLAY_VIDEO";
	case AV_CODEC_ID_XAN_WC3:                           return "XAN_WC3";
	case AV_CODEC_ID_XAN_WC4:                           return "XAN_WC4";
	case AV_CODEC_ID_RPZA:                              return "RPZA";
	case AV_CODEC_ID_CINEPAK:                           return "CINEPAK";
	case AV_CODEC_ID_WS_VQA:                            return "WS_VQA";
	case AV_CODEC_ID_MSRLE:                             return "MSRLE";
	case AV_CODEC_ID_MSVIDEO1:                          return "MSVIDEO1";
	case AV_CODEC_ID_IDCIN:                             return "IDCIN";
	case AV_CODEC_ID_8BPS:                              return "8BPS";
	case AV_CODEC_ID_SMC:                               return "SMC";
	case AV_CODEC_ID_FLIC:                              return "FLIC";
	case AV_CODEC_ID_TRUEMOTION1:                       return "TRUEMOTION1";
	case AV_CODEC_ID_VMDVIDEO:                          return "VMDVIDEO";
	case AV_CODEC_ID_MSZH:                              return "MSZH";
	case AV_CODEC_ID_ZLIB:                              return "ZLIB";
	case AV_CODEC_ID_QTRLE:                             return "QTRLE";
	case AV_CODEC_ID_TSCC:                              return "TSCC";
	case AV_CODEC_ID_ULTI:                              return "ULTI";
	case AV_CODEC_ID_QDRAW:                             return "QDRAW";
	case AV_CODEC_ID_VIXL:                              return "VIXL";
	case AV_CODEC_ID_QPEG:                              return "QPEG";
	case AV_CODEC_ID_PNG:                               return "PNG";
	case AV_CODEC_ID_PPM:                               return "PPM";
	case AV_CODEC_ID_PBM:                               return "PBM";
	case AV_CODEC_ID_PGM:                               return "PGM";
	case AV_CODEC_ID_PGMYUV:                            return "PGMYUV";
	case AV_CODEC_ID_PAM:                               return "PAM";
	case AV_CODEC_ID_FFVHUFF:                           return "FFVHUFF";
	case AV_CODEC_ID_RV30:                              return "RV30";
	case AV_CODEC_ID_RV40:                              return "RV40";
	case AV_CODEC_ID_VC1:                               return "VC1";
	case AV_CODEC_ID_WMV3:                              return "WMV3";
	case AV_CODEC_ID_LOCO:                              return "LOCO";
	case AV_CODEC_ID_WNV1:                              return "WNV1";
	case AV_CODEC_ID_AASC:                              return "AASC";
	case AV_CODEC_ID_INDEO2:                            return "INDEO2";
	case AV_CODEC_ID_FRAPS:                             return "FRAPS";
	case AV_CODEC_ID_TRUEMOTION2:                       return "TRUEMOTION2";
	case AV_CODEC_ID_BMP:                               return "BMP";
	case AV_CODEC_ID_CSCD:                              return "CSCD";
	case AV_CODEC_ID_MMVIDEO:                           return "MMVIDEO";
	case AV_CODEC_ID_ZMBV:                              return "ZMBV";
	case AV_CODEC_ID_AVS:                               return "AVS";
	case AV_CODEC_ID_SMACKVIDEO:                        return "SMACKVIDEO";
	case AV_CODEC_ID_NUV:                               return "NUV";
	case AV_CODEC_ID_KMVC:                              return "KMVC";
	case AV_CODEC_ID_FLASHSV:                           return "FLASHSV";
	case AV_CODEC_ID_CAVS:                              return "CAVS";
	case AV_CODEC_ID_JPEG2000:                          return "JPEG2000";
	case AV_CODEC_ID_VMNC:                              return "VMNC";
	case AV_CODEC_ID_VP5:                               return "VP5";
	case AV_CODEC_ID_VP6:                               return "VP6";
	case AV_CODEC_ID_VP6F:                              return "VP6F";
	case AV_CODEC_ID_TARGA:                             return "TARGA";
	case AV_CODEC_ID_DSICINVIDEO:                       return "DSICINVIDEO";
	case AV_CODEC_ID_TIERTEXSEQVIDEO:                   return "TIERTEXSEQVIDEO";
	case AV_CODEC_ID_TIFF:                              return "TIFF";
	case AV_CODEC_ID_GIF:                               return "GIF";
	case AV_CODEC_ID_DXA:                               return "DXA";
	case AV_CODEC_ID_DNXHD:                             return "DNXHD";
	case AV_CODEC_ID_THP:                               return "THP";
	case AV_CODEC_ID_SGI:                               return "SGI";
	case AV_CODEC_ID_C93:                               return "C93";
	case AV_CODEC_ID_BETHSOFTVID:                       return "BETHSOFTVID";
	case AV_CODEC_ID_PTX:                               return "PTX";
	case AV_CODEC_ID_TXD:                               return "TXD";
	case AV_CODEC_ID_VP6A:                              return "VP6A";
	case AV_CODEC_ID_AMV:                               return "AMV";
	case AV_CODEC_ID_VB:                                return "VB";
	case AV_CODEC_ID_PCX:                               return "PCX";
	case AV_CODEC_ID_SUNRAST:                           return "SUNRAST";
	case AV_CODEC_ID_INDEO4:                            return "INDEO4";
	case AV_CODEC_ID_INDEO5:                            return "INDEO5";
	case AV_CODEC_ID_MIMIC:                             return "MIMIC";
	case AV_CODEC_ID_RL2:                               return "RL2";
	case AV_CODEC_ID_ESCAPE124:                         return "ESCAPE124";
	case AV_CODEC_ID_DIRAC:                             return "DIRAC";
	case AV_CODEC_ID_BFI:                               return "BFI";
	case AV_CODEC_ID_CMV:                               return "CMV";
	case AV_CODEC_ID_MOTIONPIXELS:                      return "MOTIONPIXELS";
	case AV_CODEC_ID_TGV:                               return "TGV";
	case AV_CODEC_ID_TGQ:                               return "TGQ";
	case AV_CODEC_ID_TQI:                               return "TQI";
	case AV_CODEC_ID_AURA:                              return "AURA";
	case AV_CODEC_ID_AURA2:                             return "AURA2";
	case AV_CODEC_ID_V210X:                             return "V210X";
	case AV_CODEC_ID_TMV:                               return "TMV";
	case AV_CODEC_ID_V210:                              return "V210";
	case AV_CODEC_ID_DPX:                               return "DPX";
	case AV_CODEC_ID_MAD:                               return "MAD";
	case AV_CODEC_ID_FRWU:                              return "FRWU";
	case AV_CODEC_ID_FLASHSV2:                          return "FLASHSV2";
	case AV_CODEC_ID_CDGRAPHICS:                        return "CDGRAPHICS";
	case AV_CODEC_ID_R210:                              return "R210";
	case AV_CODEC_ID_ANM:                               return "ANM";
	case AV_CODEC_ID_BINKVIDEO:                         return "BINKVIDEO";
	case AV_CODEC_ID_IFF_ILBM:                          return "IFF_ILBM";
	case AV_CODEC_ID_IFF_BYTERUN1:                      return "IFF_BYTERUN1";
	case AV_CODEC_ID_KGV1:                              return "KGV1";
	case AV_CODEC_ID_YOP:                               return "YOP";
	case AV_CODEC_ID_VP8:                               return "VP8";
	case AV_CODEC_ID_PICTOR:                            return "PICTOR";
	case AV_CODEC_ID_ANSI:                              return "ANSI";
	case AV_CODEC_ID_A64_MULTI:                         return "A64_MULTI";
	case AV_CODEC_ID_A64_MULTI5:                        return "A64_MULTI5";
	case AV_CODEC_ID_R10K:                              return "R10K";
	case AV_CODEC_ID_MXPEG:                             return "MXPEG";
	case AV_CODEC_ID_LAGARITH:                          return "LAGARITH";
	case AV_CODEC_ID_PRORES:                            return "PRORES";
	case AV_CODEC_ID_JV:                                return "JV";
	case AV_CODEC_ID_DFA:                               return "DFA";
	case AV_CODEC_ID_WMV3IMAGE:                         return "WMV3IMAGE";
	case AV_CODEC_ID_VC1IMAGE:                          return "VC1IMAGE";
	case AV_CODEC_ID_UTVIDEO:                           return "UTVIDEO";
	case AV_CODEC_ID_BMV_VIDEO:                         return "BMV_VIDEO";
	case AV_CODEC_ID_VBLE:                              return "VBLE";
	case AV_CODEC_ID_DXTORY:                            return "DXTORY";
	case AV_CODEC_ID_V410:                              return "V410";
	case AV_CODEC_ID_XWD:                               return "XWD";
	case AV_CODEC_ID_CDXL:                              return "CDXL";
	case AV_CODEC_ID_XBM:                               return "XBM";
	case AV_CODEC_ID_ZEROCODEC:                         return "ZEROCODEC";
	case AV_CODEC_ID_MSS1:                              return "MSS1";
	case AV_CODEC_ID_MSA1:                              return "MSA1";
	case AV_CODEC_ID_TSCC2:                             return "TSCC2";
	case AV_CODEC_ID_MTS2:                              return "MTS2";
	case AV_CODEC_ID_CLLC:                              return "CLLC";
	case AV_CODEC_ID_MSS2:                              return "MSS2";
	case AV_CODEC_ID_VP9:                               return "VP9";
	case AV_CODEC_ID_AIC:                               return "AIC";
	case AV_CODEC_ID_ESCAPE130_DEPRECATED:              return "ESCAPE130_DEPRECATED";
	case AV_CODEC_ID_G2M_DEPRECATED:                    return "G2M_DEPRECATED";
	case AV_CODEC_ID_WEBP_DEPRECATED:                   return "WEBP_DEPRECATED";
	case AV_CODEC_ID_HNM4_VIDEO:                        return "HNM4_VIDEO";
	case AV_CODEC_ID_HEVC_DEPRECATED:                   return "HEVC_DEPRECATED";
	case AV_CODEC_ID_FIC:                               return "FIC";
	case AV_CODEC_ID_ALIAS_PIX:                         return "ALIAS_PIX";

	case AV_CODEC_ID_BRENDER_PIX:                       return "BRENDER_PIX";
	case AV_CODEC_ID_Y41P:                              return "Y41P";
	case AV_CODEC_ID_ESCAPE130:                         return "ESCAPE130";
	case AV_CODEC_ID_EXR:                               return "EXR";
	case AV_CODEC_ID_AVRP:                              return "AVRP";

	case AV_CODEC_ID_012V:                              return "012V";
	case AV_CODEC_ID_G2M:                               return "G2M";
	case AV_CODEC_ID_AVUI:                              return "AVUI";
	case AV_CODEC_ID_AYUV:                              return "AYUV";
	case AV_CODEC_ID_TARGA_Y216:                        return "TARGA_Y216";
	case AV_CODEC_ID_V308:                              return "V308";
	case AV_CODEC_ID_V408:                              return "V408";
	case AV_CODEC_ID_YUV4:                              return "YUV4";
	case AV_CODEC_ID_SANM:                              return "SANM";
	case AV_CODEC_ID_PAF_VIDEO:                         return "PAF_VIDEO";
	case AV_CODEC_ID_AVRN:                              return "AVRN";
	case AV_CODEC_ID_CPIA:                              return "CPIA";
	case AV_CODEC_ID_XFACE:                             return "XFACE";
	case AV_CODEC_ID_SGIRLE:                            return "SGIRLE";
	case AV_CODEC_ID_MVC1:                              return "MVC1";
	case AV_CODEC_ID_MVC2:                              return "MVC2";
	case AV_CODEC_ID_SNOW:                              return "SNOW";
	case AV_CODEC_ID_WEBP:                              return "WEBP";
	case AV_CODEC_ID_SMVJPEG:                           return "SMVJPEG";
	case AV_CODEC_ID_HEVC:                              return "HEVC";
	case AV_CODEC_ID_VP7:                               return "VP7";

		/* various PCM "codecs" */
	case AV_CODEC_ID_PCM_S16LE:                         return "PCM_S16LE";
	case AV_CODEC_ID_PCM_S16BE:                         return "PCM_S16BE";
	case AV_CODEC_ID_PCM_U16LE:                         return "PCM_U16LE";
	case AV_CODEC_ID_PCM_U16BE:                         return "PCM_U16BE";
	case AV_CODEC_ID_PCM_S8:                            return "PCM_S8";
	case AV_CODEC_ID_PCM_U8:                            return "PCM_U8";
	case AV_CODEC_ID_PCM_MULAW:                         return "PCM_MULAW";
	case AV_CODEC_ID_PCM_ALAW:                          return "PCM_ALAW";
	case AV_CODEC_ID_PCM_S32LE:                         return "PCM_S32LE";
	case AV_CODEC_ID_PCM_S32BE:                         return "PCM_S32BE";
	case AV_CODEC_ID_PCM_U32LE:                         return "PCM_U32LE";
	case AV_CODEC_ID_PCM_U32BE:                         return "PCM_U32BE";
	case AV_CODEC_ID_PCM_S24LE:                         return "PCM_S24LE";
	case AV_CODEC_ID_PCM_S24BE:                         return "PCM_S24BE";
	case AV_CODEC_ID_PCM_U24LE:                         return "PCM_U24LE";
	case AV_CODEC_ID_PCM_U24BE:                         return "PCM_U24BE";
	case AV_CODEC_ID_PCM_S24DAUD:                       return "PCM_S24DAUD";
	case AV_CODEC_ID_PCM_ZORK:                          return "PCM_ZORK";
	case AV_CODEC_ID_PCM_S16LE_PLANAR:                  return "PCM_S16LE_PLANAR";
	case AV_CODEC_ID_PCM_DVD:                           return "PCM_DVD";
	case AV_CODEC_ID_PCM_F32BE:                         return "PCM_F32BE";
	case AV_CODEC_ID_PCM_F32LE:                         return "PCM_F32LE";
	case AV_CODEC_ID_PCM_F64BE:                         return "PCM_F64BE";
	case AV_CODEC_ID_PCM_F64LE:                         return "PCM_F64LE";
	case AV_CODEC_ID_PCM_BLURAY:                        return "PCM_BLURAY";
	case AV_CODEC_ID_PCM_LXF:                           return "PCM_LXF";
	case AV_CODEC_ID_S302M:                             return "S302M";
	case AV_CODEC_ID_PCM_S8_PLANAR:                     return "PCM_S8_PLANAR";
	case AV_CODEC_ID_PCM_S24LE_PLANAR_DEPRECATED:       return "PCM_S24LE_PLANAR_DEPRECATED";
	case AV_CODEC_ID_PCM_S32LE_PLANAR_DEPRECATED:       return "PCM_S32LE_PLANAR_DEPRECATED";
	case AV_CODEC_ID_PCM_S24LE_PLANAR:                  return "PCM_S24LE_PLANAR";
	case AV_CODEC_ID_PCM_S32LE_PLANAR:                  return "PCM_S32LE_PLANAR";
	case AV_CODEC_ID_PCM_S16BE_PLANAR:                  return "PCM_S16BE_PLANAR";

		/* various ADPCM codecs */
	case AV_CODEC_ID_ADPCM_IMA_QT:            			return "ADPCM_IMA_QT";
	case AV_CODEC_ID_ADPCM_IMA_WAV:                     return "ADPCM_IMA_WAV";
	case AV_CODEC_ID_ADPCM_IMA_DK3:                     return "ADPCM_IMA_DK3";
	case AV_CODEC_ID_ADPCM_IMA_DK4:                     return "ADPCM_IMA_DK4";
	case AV_CODEC_ID_ADPCM_IMA_WS:                      return "ADPCM_IMA_WS";
	case AV_CODEC_ID_ADPCM_IMA_SMJPEG:                  return "ADPCM_IMA_SMJPEG";
	case AV_CODEC_ID_ADPCM_MS:                          return "ADPCM_MS";
	case AV_CODEC_ID_ADPCM_4XM:                         return "ADPCM_4XM";
	case AV_CODEC_ID_ADPCM_XA:                          return "ADPCM_XA";
	case AV_CODEC_ID_ADPCM_ADX:                         return "ADPCM_ADX";
	case AV_CODEC_ID_ADPCM_EA:                          return "ADPCM_EA";
	case AV_CODEC_ID_ADPCM_G726:                        return "ADPCM_G726";
	case AV_CODEC_ID_ADPCM_CT:                          return "ADPCM_CT";
	case AV_CODEC_ID_ADPCM_SWF:                         return "ADPCM_SWF";
	case AV_CODEC_ID_ADPCM_YAMAHA:                      return "ADPCM_YAMAHA";
	case AV_CODEC_ID_ADPCM_SBPRO_4:                     return "ADPCM_SBPRO_4";
	case AV_CODEC_ID_ADPCM_SBPRO_3:                     return "ADPCM_SBPRO_3";
	case AV_CODEC_ID_ADPCM_SBPRO_2:                     return "ADPCM_SBPRO_2";
	case AV_CODEC_ID_ADPCM_THP:                         return "ADPCM_THP";
	case AV_CODEC_ID_ADPCM_IMA_AMV:                     return "ADPCM_IMA_AMV";
	case AV_CODEC_ID_ADPCM_EA_R1:                       return "ADPCM_EA_R1";
	case AV_CODEC_ID_ADPCM_EA_R3:                       return "ADPCM_EA_R3";
	case AV_CODEC_ID_ADPCM_EA_R2:                       return "ADPCM_EA_R2";
	case AV_CODEC_ID_ADPCM_IMA_EA_SEAD:                 return "ADPCM_IMA_EA_SEAD";
	case AV_CODEC_ID_ADPCM_IMA_EA_EACS:                 return "ADPCM_IMA_EA_EACS";
	case AV_CODEC_ID_ADPCM_EA_XAS:                      return "ADPCM_EA_XAS";
	case AV_CODEC_ID_ADPCM_EA_MAXIS_XA:                 return "ADPCM_EA_MAXIS_XA";
	case AV_CODEC_ID_ADPCM_IMA_ISS:                     return "ADPCM_IMA_ISS";
	case AV_CODEC_ID_ADPCM_G722:                        return "ADPCM_G722";
	case AV_CODEC_ID_ADPCM_IMA_APC:                     return "ADPCM_IMA_APC";
	case AV_CODEC_ID_VIMA:                              return "VIMA";
	case AV_CODEC_ID_ADPCM_AFC:                         return "ADPCM_AFC";
	case AV_CODEC_ID_ADPCM_IMA_OKI:                     return "ADPCM_IMA_OKI";
	case AV_CODEC_ID_ADPCM_DTK:                         return "ADPCM_DTK";
	case AV_CODEC_ID_ADPCM_IMA_RAD:                     return "ADPCM_IMA_RAD";
	case AV_CODEC_ID_ADPCM_G726LE:                      return "ADPCM_G726LE";

		/* AMR */
	case AV_CODEC_ID_AMR_NB:                            return "AMR_NB";
	case AV_CODEC_ID_AMR_WB:                            return "AMR_WB";

		/* RealAudio codecs*/
	case AV_CODEC_ID_RA_144:                            return "RA_144";
	case AV_CODEC_ID_RA_288:                            return "RA_288";

		/* various DPCM codecs */
	case AV_CODEC_ID_ROQ_DPCM:                          return "ROQ_DPCM";
	case AV_CODEC_ID_INTERPLAY_DPCM:                    return "INTERPLAY_DPCM";
	case AV_CODEC_ID_XAN_DPCM:                          return "XAN_DPCM";
	case AV_CODEC_ID_SOL_DPCM:                          return "SOL_DPCM";

		/* audio codecs */
	case AV_CODEC_ID_MP2:                               return "MP2";
	case AV_CODEC_ID_MP3:                               return "MP3";
	case AV_CODEC_ID_AAC:                               return "AAC";
	case AV_CODEC_ID_AC3:                               return "AC3";
	case AV_CODEC_ID_DTS:                               return "DTS";
	case AV_CODEC_ID_VORBIS:                            return "VORBIS";
	case AV_CODEC_ID_DVAUDIO:                           return "DVAUDIO";
	case AV_CODEC_ID_WMAV1:                             return "WMAV1";
	case AV_CODEC_ID_WMAV2:                             return "WMAV2";
	case AV_CODEC_ID_MACE3:                             return "MACE3";
	case AV_CODEC_ID_MACE6:                             return "MACE6";
	case AV_CODEC_ID_VMDAUDIO:                          return "VMDAUDIO";
	case AV_CODEC_ID_FLAC:                              return "FLAC";
	case AV_CODEC_ID_MP3ADU:                            return "MP3ADU";
	case AV_CODEC_ID_MP3ON4:                            return "MP3ON4";
	case AV_CODEC_ID_SHORTEN:                           return "SHORTEN";
	case AV_CODEC_ID_ALAC:                              return "ALAC";
	case AV_CODEC_ID_WESTWOOD_SND1:                     return "WESTWOOD_SND1";
	case AV_CODEC_ID_GSM:                               return "GSM";
	case AV_CODEC_ID_QDM2:                              return "QDM2";
	case AV_CODEC_ID_COOK:                              return "COOK";
	case AV_CODEC_ID_TRUESPEECH:                        return "TRUESPEECH";
	case AV_CODEC_ID_TTA:                               return "TTA";
	case AV_CODEC_ID_SMACKAUDIO:                        return "SMACKAUDIO";
	case AV_CODEC_ID_QCELP:                             return "QCELP";
	case AV_CODEC_ID_WAVPACK:                           return "WAVPACK";
	case AV_CODEC_ID_DSICINAUDIO:                       return "DSICINAUDIO";
	case AV_CODEC_ID_IMC:                               return "IMC";
	case AV_CODEC_ID_MUSEPACK7:                         return "MUSEPACK7";
	case AV_CODEC_ID_MLP:                               return "MLP";
	case AV_CODEC_ID_GSM_MS:                            return "GSM_MS";
	case AV_CODEC_ID_ATRAC3:                            return "ATRAC3";
	case AV_CODEC_ID_VOXWARE:                           return "VOXWARE";
	case AV_CODEC_ID_APE:                               return "APE";
	case AV_CODEC_ID_NELLYMOSER:                        return "NELLYMOSER";
	case AV_CODEC_ID_MUSEPACK8:                         return "MUSEPACK8";
	case AV_CODEC_ID_SPEEX:                             return "SPEEX";
	case AV_CODEC_ID_WMAVOICE:                          return "WMAVOICE";
	case AV_CODEC_ID_WMAPRO:                            return "WMAPRO";
	case AV_CODEC_ID_WMALOSSLESS:                       return "WMALOSSLESS";
	case AV_CODEC_ID_ATRAC3P:                           return "ATRAC3P";
	case AV_CODEC_ID_EAC3:                              return "EAC3";
	case AV_CODEC_ID_SIPR:                              return "SIPR";
	case AV_CODEC_ID_MP1:                               return "MP1";
	case AV_CODEC_ID_TWINVQ:                            return "TWINVQ";
	case AV_CODEC_ID_TRUEHD:                            return "TRUEHD";
	case AV_CODEC_ID_MP4ALS:                            return "MP4ALS";
	case AV_CODEC_ID_ATRAC1:                            return "ATRAC1";
	case AV_CODEC_ID_BINKAUDIO_RDFT:                    return "BINKAUDIO_RDFT";
	case AV_CODEC_ID_BINKAUDIO_DCT:                     return "BINKAUDIO_DCT";
	case AV_CODEC_ID_AAC_LATM:                          return "AAC_LATM";
	case AV_CODEC_ID_QDMC:                              return "QDMC";
	case AV_CODEC_ID_CELT:                              return "CELT";
	case AV_CODEC_ID_G723_1:                            return "G723_1";
	case AV_CODEC_ID_G729:                              return "G729";
	case AV_CODEC_ID_8SVX_EXP:                          return "8SVX_EXP";
	case AV_CODEC_ID_8SVX_FIB:                          return "8SVX_FIB";
	case AV_CODEC_ID_BMV_AUDIO:                         return "BMV_AUDIO";
	case AV_CODEC_ID_RALF:                              return "RALF";
	case AV_CODEC_ID_IAC:                               return "IAC";
	case AV_CODEC_ID_ILBC:                              return "ILBC";
	case AV_CODEC_ID_OPUS_DEPRECATED:                   return "OPUS_DEPRECATED";
	case AV_CODEC_ID_COMFORT_NOISE:                     return "COMFORT_NOISE";
	case AV_CODEC_ID_TAK_DEPRECATED:                    return "TAK_DEPRECATED";
	case AV_CODEC_ID_METASOUND:                         return "METASOUND";
	case AV_CODEC_ID_FFWAVESYNTH:                       return "FFWAVESYNTH";
	case AV_CODEC_ID_SONIC:                             return "SONIC";
	case AV_CODEC_ID_SONIC_LS:                          return "SONIC_LS";
	case AV_CODEC_ID_PAF_AUDIO:                         return "PAF_AUDIO";
	case AV_CODEC_ID_OPUS:                              return "OPUS";
	case AV_CODEC_ID_TAK:                               return "TAK";
	case AV_CODEC_ID_EVRC:                              return "EVRC";
	case AV_CODEC_ID_SMV:                               return "SMV";

		/* subtitle codecs */
	case AV_CODEC_ID_DVD_SUBTITLE:            			return "DVD_SUBTITLE";
	case AV_CODEC_ID_DVB_SUBTITLE:                      return "DVB_SUBTITLE";
	case AV_CODEC_ID_TEXT:                              return "TEXT";
	case AV_CODEC_ID_XSUB:                              return "XSUB";
	case AV_CODEC_ID_SSA:                               return "SSA";
	case AV_CODEC_ID_MOV_TEXT:                          return "MOV_TEXT";
	case AV_CODEC_ID_HDMV_PGS_SUBTITLE:                 return "HDMV_PGS_SUBTITLE";
	case AV_CODEC_ID_DVB_TELETEXT:                      return "DVB_TELETEXT";
	case AV_CODEC_ID_SRT:                               return "SRT";
	case AV_CODEC_ID_MICRODVD:                          return "MICRODVD";
	case AV_CODEC_ID_EIA_608:                           return "EIA_608";
	case AV_CODEC_ID_JACOSUB:                           return "JACOSUB";
	case AV_CODEC_ID_SAMI:                              return "SAMI";
	case AV_CODEC_ID_REALTEXT:                          return "REALTEXT";
	case AV_CODEC_ID_SUBVIEWER1:                        return "SUBVIEWER1";
	case AV_CODEC_ID_SUBVIEWER:                         return "SUBVIEWER";
	case AV_CODEC_ID_SUBRIP:                            return "SUBRIP";
	case AV_CODEC_ID_WEBVTT:                            return "WEBVTT";
	case AV_CODEC_ID_MPL2:                              return "MPL2";
	case AV_CODEC_ID_VPLAYER:                           return "VPLAYER";
	case AV_CODEC_ID_PJS:                               return "PJS";
	case AV_CODEC_ID_ASS:                               return "ASS";

	case AV_CODEC_ID_TTF:                               return "TTF";
	case AV_CODEC_ID_BINTEXT:                           return "BINTEXT";
	case AV_CODEC_ID_XBIN:                              return "XBIN";
	case AV_CODEC_ID_IDF:                               return "IDF";
	case AV_CODEC_ID_OTF:                               return "OTF";
	case AV_CODEC_ID_SMPTE_KLV:                         return "SMPTE_KLV";
	case AV_CODEC_ID_DVD_NAV:                           return "DVD_NAV";
	case AV_CODEC_ID_TIMED_ID3:                         return "TIMED_ID3";

	case AV_CODEC_ID_PROBE:                             return "PROBE";
	}

	return "";
}

void audio_encoding_ex() {
	float t, tincr;
	int i, j, k;
	/* Audio_frame containing input raw audio */
	Audio_frame = avcodec_alloc_frame();
	if (!Audio_frame) {
		fprintf(stderr, "Could not allocate audio Audio_frame\n");
		system("pause"); exit(1);
	}

	Audio_frame->nb_samples = Audio_codec_c->frame_size;
	Audio_frame->format = Audio_codec_c->sample_fmt;
	Audio_frame->channel_layout = Audio_codec_c->channel_layout;

	/* the Audio_codec gives us the Audio_frame size, in Audio_samples,
	* we calculate the size of the Audio_samples buffer in bytes */
	Audio_buffer_size = av_samples_get_buffer_size(NULL, Audio_codec_c->channels, Audio_codec_c->frame_size,
		Audio_codec_c->sample_fmt, 0);

	Audio_samples = (uint16_t*)av_malloc(Audio_buffer_size);
	if (!Audio_samples) {
		fprintf(stderr, "Could not allocate %d bytes for Audio_samples buffer\n",
			Audio_buffer_size);
		system("pause"); exit(1);
	}


	/* setup the data pointers in the AVFrame */
	ret = avcodec_fill_audio_frame(Audio_frame, Audio_codec_c->channels, Audio_codec_c->sample_fmt,
		(const uint8_t*)Audio_samples, Audio_buffer_size, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not setup audio Audio_frame\n");
		system("pause"); exit(1);
	}

	/* encode a single tone sound */
	t = 0;
	tincr = 2 * M_PI * 440.0 / Audio_codec_c->sample_rate;
	for (i = 0; i < 200; i++) {
		av_init_packet(&pkt);
		pkt.data = NULL; // packet data will be allocated by the encoder
		pkt.size = 0;

		for (j = 0; j < Audio_codec_c->frame_size; j++) {
			Audio_samples[2 * j] = (int)(sin(t) * 10000);

			for (k = 1; k < Audio_codec_c->channels; k++)
				Audio_samples[2 * j + k] = Audio_samples[2 * j];
			t += tincr;
		}
		/* encode the Audio_samples */
		ret = avcodec_encode_audio2(Audio_codec_c, &pkt, Audio_frame, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding audio Audio_frame\n");
			system("pause"); exit(1);
		}
		if (got_output) {
			fwrite(pkt.data, 1, pkt.size, Audio_f);
			av_free_packet(&pkt);
		}
	}

	/* get the delayed frames */
	for (got_output = 1; got_output; i++) {
		ret = avcodec_encode_audio2(Audio_codec_c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding Audio_frame\n");
			system("pause"); exit(1);
		}

		if (got_output) {
			fwrite(pkt.data, 1, pkt.size, Audio_f);
			av_free_packet(&pkt);
		}
	}
	fclose(Audio_f);

	av_freep(&Audio_samples);
	avcodec_free_frame(&Audio_frame);
	avcodec_close(Audio_codec_c);
	av_free(Audio_codec_c);
}