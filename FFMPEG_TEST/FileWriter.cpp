#include "FileWriter.h"




FileWriter::FileWriter()

{

	static int init = 0;

	if (init == 0)

	{

		av_register_all();

		init = 1;

	}




	m_pFormatContext = NULL;

	m_pVideoStream = NULL;

}




FileWriter::~FileWriter()

{

	closeFile();

}




int FileWriter::openFile(char *filename, int codec, int width, int height)

{

	int err;




	m_nFrameCount = 0;

	m_nCodecID = CODEC_ID_NONE;




	err = avformat_alloc_output_context2(&m_pFormatContext, NULL, NULL, filename);

	if (!m_pFormatContext || err < 0) {

		printf("[%s] avformat_alloc_output_context2: error %d\n", __FUNCTION__, err);

		return -1;

	}




	av_init_packet(&m_avPacket);




	AVOutputFormat *fmt = m_pFormatContext->oformat;




	if (codec == 0) {

		fmt->video_codec = CODEC_ID_H264;

		m_nCodecID = CODEC_ID_H264;

	}
	else if (codec == 1) {

		fmt->video_codec = CODEC_ID_MPEG4;

		m_nCodecID = CODEC_ID_MPEG4;

	}




	m_pVideoStream = addVideoStream(fmt->video_codec, width, height);

	if (!m_pVideoStream) {

		printf("[%s] addVideoStream failed\n", __FUNCTION__);

		return -1;

	}




	err = avio_open(&m_pFormatContext->pb, filename, AVIO_FLAG_WRITE);

	if (err < 0) {

		printf("[%s] avio_open failed: error %d\n", __FUNCTION__, err);

		closeFile();

		return -1;

	}




#ifdef FFMPEG_1_2_1

	err = avformat_write_header(m_pFormatContext, NULL);

#else

	err = av_write_header(m_pFormatContext);

#endif




	return 0;

}




void FileWriter::closeFile()

{

	if (m_pFormatContext) {

		av_write_trailer(m_pFormatContext);
		if (m_pVideoStream) {
			if (m_pVideoStream->codec) {
				avcodec_close(m_pVideoStream->codec);
			}
		}




		av_free_packet(&m_avPacket);

		avio_close(m_pFormatContext->pb);

		avformat_free_context(m_pFormatContext);

		m_pFormatContext = NULL;

	}

}




int FileWriter::writeFile(enum AVMediaType type, char *buf, int len)

{

	if (type == AVMEDIA_TYPE_VIDEO)

		return writeVideo(buf, len);

	else if (type == AVMEDIA_TYPE_AUDIO)

		return writeAudio(buf, len);




	return -1;

}




int FileWriter::writeVideo(char *buf, int len)

{

	if (!m_pFormatContext)

		return -1;




	int ret = 0;




	av_init_packet(&m_avPacket);




	m_avPacket.stream_index = m_pVideoStream->index;

	m_avPacket.data = (uint8_t *)buf;

	m_avPacket.size = len;




	if (checkKeyFrame(buf, len) == 1)

		m_avPacket.flags |= AV_PKT_FLAG_KEY;




	ret = av_interleaved_write_frame(m_pFormatContext, &m_avPacket);

	//ret = av_write_frame(m_pFormatContext, &m_avPacket);




	m_nFrameCount++;



	return ret;

}




AVStream* FileWriter::addVideoStream(enum CodecID codec_id, int width, int height)

{

	AVStream *st;

	AVCodecContext *c;




	st = av_new_stream(m_pFormatContext, 0);

	if (!st) {

		printf("[%s] Could not alloc stream\n", __FUNCTION__);

		return NULL;

	}




	c = st->codec;

	c->codec_id = codec_id;

	c->codec_type = AVMEDIA_TYPE_VIDEO;




	c->width = width;

	c->height = height;




	c->time_base.den = 30;

	c->time_base.num = 1;

	c->gop_size = 30;




	c->pix_fmt = PIX_FMT_YUV420P;




	// some formats want stream headers to be separate

	if (m_pFormatContext->oformat->flags & AVFMT_GLOBALHEADER)

		c->flags |= CODEC_FLAG_GLOBAL_HEADER;




	return st;

}




int FileWriter::writeAudio(char *buf, int len)

{

	return -1;

}