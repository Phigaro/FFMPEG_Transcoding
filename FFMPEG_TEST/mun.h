///> Include FFMpeg

#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable:4996)

extern "C" {
#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#endif
}

#include <iostream>
#include <windows.h>

using namespace std;
///> Library Link On Windows System
#pragma comment( lib, "avformat.lib" )	
#pragma comment( lib, "avutil.lib" )
#pragma comment( lib, "avcodec.lib" )
#pragma comment( lib, "swscale")


int main(void)
{
	AVOutputFormat	*ofmt = NULL;
	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext *ofmt_ctx = NULL;

	AVCodecID	codec_id = AV_CODEC_ID_MPEG2VIDEO;
	const char	*szFilePath = "sample2.mp4";
	const char	*outputfile = "output.ts";

	av_register_all();

#ifdef _NETWORK
	avformat_network_init();
#endif

	int ret;

	if ((avformat_open_input(&ifmt_ctx, szFilePath, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", szFilePath);
	}
	if ((avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
	}

	av_dump_format(ifmt_ctx, 0, szFilePath, 0);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outputfile);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
	}

	///> Find Video Stream
	int nVSI = -1;
	int nASI = -1;
	int i;

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		if (nVSI < 0 && ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			nVSI = i;
		}
		else if (nASI < 0 && ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			nASI = i;
		}
	}

	if (nVSI < 0 && nASI < 0) {
		av_log(NULL, AV_LOG_ERROR, "No Video & Audio Streams were Found\n");
		return 0;
	}

	AVCodec *codec;
	AVCodecContext *c = NULL;

	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		return 0;
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		return 0;
	}
	c->bit_rate = 400 * 1000;
	c->bit_rate = c->rc_max_rate >> 1;
	c->rc_buffer_size = c->rc_max_rate;
	c->rc_initial_buffer_occupancy = c->rc_max_rate;
	c->width = ifmt_ctx->streams[nVSI]->codec->width;
	c->height = ifmt_ctx->streams[nVSI]->codec->height;
	c->time_base = { 1,25 };
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		return 0;
	}

	ofmt = ofmt_ctx->oformat;

	for (int i = 0; i < ifmt_ctx->nb_streams; i++) {

		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			fprintf(stderr, "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
		}
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			ret = avcodec_copy_context(out_stream->codec, c);
		}
		else if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		}

		if (ret < 0) {
			fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
		}
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	av_dump_format(ofmt_ctx, 0, outputfile, 1);
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, outputfile, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output file '%s'", outputfile);
		}
	}
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
	}

	AVCodecContext *pVCtx = ifmt_ctx->streams[nVSI]->codec;
	AVCodecContext *pACtx = ifmt_ctx->streams[nASI]->codec;
	///> Find Video Decoder
	AVCodec *pVideoCodec = avcodec_find_decoder(ifmt_ctx->streams[nVSI]->codec->codec_id);
	if (pVideoCodec == NULL) {
		av_log(NULL, AV_LOG_ERROR, "No Video Decoder was Found\n");
		exit(-1);
	}

	///> Initialize Codec Context as Decoder
	if (avcodec_open2(pVCtx, pVideoCodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Fail to Initialize Decoder\n");
		exit(-1);
	}

	///> Find Audio Decoder
	AVCodec *pAudioCodec = avcodec_find_decoder(ifmt_ctx->streams[nASI]->codec->codec_id);
	if (pAudioCodec == NULL) {
		av_log(NULL, AV_LOG_ERROR, "No Audio Decoder was Found\n");
		exit(-1);
	}

	///> Initialize Codec Context as Decoder
	if (avcodec_open2(pACtx, pAudioCodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Fail to Initialize Decoder\n");
		exit(-1);
	}

	AVPacket pkt, outpkt;
	AVFrame* pVFrame, *pAFrame;
	int bGotPicture = 0;	// flag for video decoding
	int bGotSound = 0;		// flag for audio decoding

	int bPrint = 0;	// 비디오 첫 장면만 파일로 남기기 위한 임시 flag 입니다

	pVFrame = av_frame_alloc();
	pAFrame = av_frame_alloc();


	AVFrame *frame;

	frame = av_frame_alloc();

	i = 0;
	//int j = 0;
	int p, d;
	while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
		///> Decoding
		p = pkt.pts;
		d = pkt.dts;
		if (pkt.stream_index == nVSI) {
			if (avcodec_decode_video2(pVCtx, pVFrame, &bGotPicture, &pkt) >= 0) {
				if (bGotPicture) {
					av_init_packet(&outpkt);
					outpkt.data = NULL;    // packet data will be allocated by the encoder
					outpkt.size = 0;

					ret = avcodec_encode_video2(c, &outpkt, pVFrame, &bGotPicture);
					if (bGotPicture) {
						printf("Write frame %3d (size=%5d)\n", pVFrame->pts, outpkt.size);
						av_interleaved_write_frame(ofmt_ctx, &outpkt);
						av_free_packet(&outpkt);
					}
				}
			}
		}
		else if (pkt.stream_index == nASI) {
			if (pkt.data != NULL) {
				av_interleaved_write_frame(ofmt_ctx, &pkt);
			}
		}
		av_free_packet(&pkt);
	}
	for (bGotPicture = 1; bGotPicture; i++) {
		fflush(stdout);
		ret = avcodec_encode_video2(c, &pkt, NULL, &bGotPicture);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}
		if (bGotPicture) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			av_interleaved_write_frame(ofmt_ctx, &pkt);
			av_free_packet(&pkt);
		}
	}


	av_write_trailer(ofmt_ctx);
	avcodec_close(c);
	av_free(c);
	av_free(pVFrame);
	av_free(pAFrame);

	///> Close an opened input AVFormatContext. 
	avformat_close_input(&ifmt_ctx);
	avformat_close_input(&ofmt_ctx);
	///> Undo the initialization done by avformat_network_init.

#ifdef _NETWORK
	avformat_network_deinit();
#endif

	return 0;
}
///> Include FFMpeg

#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable:4996)

extern "C" {
#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#endif
}

#include <iostream>
#include <windows.h>

using namespace std;
///> Library Link On Windows System
#pragma comment( lib, "avformat.lib" )	
#pragma comment( lib, "avutil.lib" )
#pragma comment( lib, "avcodec.lib" )
#pragma comment( lib, "swscale")


int main(void)
{
	AVOutputFormat	*ofmt = NULL;
	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext *ofmt_ctx = NULL;

	AVCodecID	codec_id = AV_CODEC_ID_MPEG2VIDEO;
	const char	*szFilePath = "sample2.mp4";
	const char	*outputfile = "output.ts";

	av_register_all();

#ifdef _NETWORK
	avformat_network_init();
#endif

	int ret;

	if ((avformat_open_input(&ifmt_ctx, szFilePath, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", szFilePath);
	}
	if ((avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
	}

	av_dump_format(ifmt_ctx, 0, szFilePath, 0);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outputfile);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
	}

	///> Find Video Stream
	int nVSI = -1;
	int nASI = -1;
	int i;

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		if (nVSI < 0 && ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			nVSI = i;
		}
		else if (nASI < 0 && ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			nASI = i;
		}
	}

	if (nVSI < 0 && nASI < 0) {
		av_log(NULL, AV_LOG_ERROR, "No Video & Audio Streams were Found\n");
		return 0;
	}

	AVCodec *codec;
	AVCodecContext *c = NULL;

	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		return 0;
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		return 0;
	}
	c->bit_rate = 400 * 1000;
	c->bit_rate = c->rc_max_rate >> 1;
	c->rc_buffer_size = c->rc_max_rate;
	c->rc_initial_buffer_occupancy = c->rc_max_rate;
	c->width = ifmt_ctx->streams[nVSI]->codec->width;
	c->height = ifmt_ctx->streams[nVSI]->codec->height;
	c->time_base = { 1,25 };
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		return 0;
	}

	ofmt = ofmt_ctx->oformat;

	for (int i = 0; i < ifmt_ctx->nb_streams; i++) {

		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			fprintf(stderr, "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
		}
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			ret = avcodec_copy_context(out_stream->codec, c);
		}
		else if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		}

		if (ret < 0) {
			fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
		}
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	av_dump_format(ofmt_ctx, 0, outputfile, 1);
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, outputfile, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output file '%s'", outputfile);
		}
	}
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
	}

	AVCodecContext *pVCtx = ifmt_ctx->streams[nVSI]->codec;
	AVCodecContext *pACtx = ifmt_ctx->streams[nASI]->codec;
	///> Find Video Decoder
	AVCodec *pVideoCodec = avcodec_find_decoder(ifmt_ctx->streams[nVSI]->codec->codec_id);
	if (pVideoCodec == NULL) {
		av_log(NULL, AV_LOG_ERROR, "No Video Decoder was Found\n");
		exit(-1);
	}

	///> Initialize Codec Context as Decoder
	if (avcodec_open2(pVCtx, pVideoCodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Fail to Initialize Decoder\n");
		exit(-1);
	}

	///> Find Audio Decoder
	AVCodec *pAudioCodec = avcodec_find_decoder(ifmt_ctx->streams[nASI]->codec->codec_id);
	if (pAudioCodec == NULL) {
		av_log(NULL, AV_LOG_ERROR, "No Audio Decoder was Found\n");
		exit(-1);
	}

	///> Initialize Codec Context as Decoder
	if (avcodec_open2(pACtx, pAudioCodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Fail to Initialize Decoder\n");
		exit(-1);
	}

	AVPacket pkt, outpkt;
	AVFrame* pVFrame, *pAFrame;
	int bGotPicture = 0;	// flag for video decoding
	int bGotSound = 0;		// flag for audio decoding

	int bPrint = 0;	// 비디오 첫 장면만 파일로 남기기 위한 임시 flag 입니다

	pVFrame = av_frame_alloc();
	pAFrame = av_frame_alloc();


	AVFrame *frame;

	frame = av_frame_alloc();

	i = 0;
	//int j = 0;
	int p, d;
	while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
		///> Decoding
		p = pkt.pts;
		d = pkt.dts;
		if (pkt.stream_index == nVSI) {
			if (avcodec_decode_video2(pVCtx, pVFrame, &bGotPicture, &pkt) >= 0) {
				if (bGotPicture) {
					av_init_packet(&outpkt);
					outpkt.data = NULL;    // packet data will be allocated by the encoder
					outpkt.size = 0;

					ret = avcodec_encode_video2(c, &outpkt, pVFrame, &bGotPicture);
					if (bGotPicture) {
						printf("Write frame %3d (size=%5d)\n", pVFrame->pts, outpkt.size);
						av_interleaved_write_frame(ofmt_ctx, &outpkt);
						av_free_packet(&outpkt);
					}
				}
			}
		}
		else if (pkt.stream_index == nASI) {
			if (pkt.data != NULL) {
				av_interleaved_write_frame(ofmt_ctx, &pkt);
			}
		}
		av_free_packet(&pkt);
	}
	for (bGotPicture = 1; bGotPicture; i++) {
		fflush(stdout);
		ret = avcodec_encode_video2(c, &pkt, NULL, &bGotPicture);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}
		if (bGotPicture) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			av_interleaved_write_frame(ofmt_ctx, &pkt);
			av_free_packet(&pkt);
		}
	}


	av_write_trailer(ofmt_ctx);
	avcodec_close(c);
	av_free(c);
	av_free(pVFrame);
	av_free(pAFrame);

	///> Close an opened input AVFormatContext. 
	avformat_close_input(&ifmt_ctx);
	avformat_close_input(&ofmt_ctx);
	///> Undo the initialization done by avformat_network_init.

#ifdef _NETWORK
	avformat_network_deinit();
#endif

	return 0;
}
