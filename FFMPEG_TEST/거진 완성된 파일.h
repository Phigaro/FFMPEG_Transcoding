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

int main()
{
	AVOutputFormat   *ofmt = NULL;
	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext *ofmt_ctx = NULL;

	//AVCodecID   v_codec_id = AV_CODEC_ID_H264;
	AVCodecID   v_codec_id = AV_CODEC_ID_H264;
	AVCodecID   a_codec_id = AV_CODEC_ID_MP2;

	const char   *szFilePath = "sample2.mp4";//x264
	const char   *outputfile = "output.mp4";

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

	//avformat_alloc_output_context2(&ofmt_ctx, NULL, ifmt_ctx->iformat->name, outputfile);
	// m4a, mj2 미지원 ifmt_ctx->iformat->name 예외 처리 필요

	avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", outputfile);
	//avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outputfile);

	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
	}
	///> Find Video Stream
	int nVSI = -1;
	int nASI = -1;
	int i;

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		if (nVSI < 0 && ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			nVSI = i;
		}
		else if (nASI < 0 && ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			nASI = i;
		}
	}

	if (nVSI < 0 && nASI < 0) {
		av_log(NULL, AV_LOG_ERROR, "No Video & Audio Streams were Found\n");
		return 0;
	}

	AVCodec *codec;
	AVCodecContext *c = NULL;

	codec = avcodec_find_encoder(v_codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		return 0;
	}

	//codec = avcodec_find_encoder_by_name("libopenh264");

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		return 0;
	}

	c->profile = FF_PROFILE_H264_BASELINE;
	c->width = ifmt_ctx->streams[nVSI]->codecpar->width;
	c->height = ifmt_ctx->streams[nVSI]->codecpar->height;
	//	c->time_base.num = ifmt_ctx->streams[nVSI]->codec->time_base.num;
	//c->time_base.den = ifmt_ctx->streams[nVSI]->codec->time_base.den;
	c->time_base = { 1,22 };
	c->pix_fmt = ifmt_ctx->streams[nVSI]->codec->pix_fmt;
	c->bit_rate = 966 * 1000;

	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		return 0;
	}
	ofmt = ofmt_ctx->oformat;

	for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			ret = avcodec_copy_context(out_stream->codec, c);
			out_stream->codec->codec_tag = 0;//문서
		}
		else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
			out_stream->codec->codec_tag = 0;
		}

		if (ret < 0) {
			fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
		}
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {}
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
		return 0;
	}

	///> Initialize Codec Context as Decoder
	if (avcodec_open2(pVCtx, pVideoCodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Fail to Initialize Decoder\n");
		return 0;
	}

	///> Find Audio Decoder
	AVCodec *pAudioCodec = avcodec_find_decoder(ifmt_ctx->streams[nASI]->codec->codec_id);
	if (pAudioCodec == NULL) {
		av_log(NULL, AV_LOG_ERROR, "No Audio Decoder was Found\n");
		return 0;
	}

	///> Initialize Codec Context as Decoder
	if (avcodec_open2(pACtx, pAudioCodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Fail to Initialize Decoder\n");
		return 0;
	}

	AVPacket pkt, outpkt;
	AVFrame* pVFrame, *pAFrame;
	int bGotPicture = 0;   // flag for video decoding
	int bGotSound = 0;      // flag for audio decoding

	pVFrame = av_frame_alloc();
	pAFrame = av_frame_alloc();

	av_init_packet(&outpkt);

	while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
		///> Decoding
		if (pkt.stream_index == nVSI) {
			if (avcodec_decode_video2(pVCtx, pVFrame, &bGotPicture, &pkt) >= 0) {
				if (bGotPicture) {

					outpkt.data = NULL;    // packet data will be allocated by the encoder
					outpkt.size = 0;

					ret = avcodec_encode_video2(c, &outpkt, pVFrame, &bGotPicture);
					if (bGotPicture) {
						cout << "Write Frame [pts]=>" << outpkt.pts << " [size]=>" << outpkt.size << endl;
						av_interleaved_write_frame(ofmt_ctx, &outpkt);
						av_free_packet(&outpkt);
					}
				}
			}
		}
		else if (pkt.stream_index == nASI) {
			if (pkt.data != NULL) {
				if (avcodec_decode_audio4(pACtx, pAFrame, &bGotSound, &pkt) >= 0) {
					if (bGotSound) {
						outpkt.data = NULL;    // packet data will be allocated by the encoder
						outpkt.size = 0;
						//ret = avcodec_encode_audio2(a_c, &outpkt, pAFrame, &bGotSound);
						if (bGotSound) {
							cout << "Write Frame [pts]=>" << outpkt.pts << " [size]=>" << outpkt.size << endl;
							av_interleaved_write_frame(ofmt_ctx, &pkt);
							av_free_packet(&outpkt);
						}
					}
				}
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
#ifdef _NETWORK
	///> Undo the initialization done by avformat_network_init.
	avformat_network_deinit();
#endif
	//system("pause");
	return 0;
}