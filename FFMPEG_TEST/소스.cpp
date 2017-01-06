//
///* ********************************* */
//
//av_register_all();
//
//> Do global initialization of network components.
//avformat_network_init();
//
//int ret;
//AVFormatContext *pFmtCtx = NULL;
//open_input_file(szFilePath);
//
//cout << fmt_ctx->nb_streams << endl;
//cout << fmt_ctx->streams[0]->codec->codec_id << endl;
//cout << AVCodecID2Str(fmt_ctx->streams[0]->codec->codec_id) << endl;
//cout << fmt_ctx->streams[0]->codec->codec_type << endl;
//cout << AVMediaType2Str(fmt_ctx->streams[0]->codec->codec_type) << endl;
//
//av_log(NULL, AV_LOG_ERROR, "Hello FFmpeg\n");
//> Open an input stream and read the header. 
//ret = avformat_open_input(&pFmtCtx, szFilePath, NULL, NULL);
//if (ret != 0) {
//	av_log(NULL, AV_LOG_ERROR, "File [%s] Open Fail (ret: %d)\n", ret);
//	exit(-1);
//}
//av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", szFilePath);
//av_log(NULL, AV_LOG_INFO, "Format: %s\n", pFmtCtx->iformat->name);
//
//
//
//> Read packets of a media file to get stream information. 
//ret = avformat_find_stream_info(pFmtCtx, NULL);
//if (ret < 0) {
//	av_log(NULL, AV_LOG_ERROR, "Fail to get Stream Information\n");
//	exit(-1);
//}
//av_log(NULL, AV_LOG_INFO, "Get Stream Information Success\n");
//
//> Get Stream Duration
//if (pFmtCtx->duration > 0) {
//	int tns, thh, tmm, tss;
//	tns = pFmtCtx->duration / 1000000LL;
//	thh = tns / 3600;
//	tmm = (tns % 3600) / 60;
//	tss = (tns % 60);
//
//	if (tns > 0) {
//		av_log(NULL, AV_LOG_INFO, "Duration : %2d:%02d:%02d\n", thh, tmm, tss);
//	}
//}
//
//av_log(NULL, AV_LOG_INFO, "Number of Stream: %d\n", pFmtCtx->nb_streams);
//
//> Print Stream Information
//int i;
//for (i = 0; i < pFmtCtx->nb_streams; i++) {		// Stream num 만큼 반복
//	AVStream *pStream = pFmtCtx->streams[i];	// Stream을 가져옴
//	const char *szType = AVMediaType2Str(pStream->codec->codec_type);	// Stream의 Type : stream -> codec -> codec_type (Video or Audio)
//	const char *szCodecName = AVCodecID2Str(pStream->codec->codec_id);	// Codec의 Name  : stream -> codec -> codec_id (ex H264)
//	av_log(NULL, AV_LOG_INFO, "    > Stream[%d]: %s: %s ", i, szType, szCodecName);	// 해당 정보를 출력
//	if (pStream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {		// 비디오 일 경우 비디오 정보 출력
//		av_log(NULL, AV_LOG_INFO, "%dx%d (%.2f fps)", pStream->codec->width, pStream->codec->height, av_q2d(pStream->r_frame_rate));
//	}
//	else if (pStream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {// 오디오 일 경우 오디오 정보 출력
//		av_log(NULL, AV_LOG_INFO, "%d Hz", pStream->codec->sample_rate);
//	}
//	av_log(NULL, AV_LOG_INFO, "\n");
//}
//
//
//int j;
//int nVSI = -1; ///> Video Stream Index
//int nASI = -1; ///> Audio Stream Index
//nVSI = av_find_best_stream(pFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
//nASI = av_find_best_stream(pFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
//for (j = 0; j < pFmtCtx->nb_streams; j++) {
//	if (pFmtCtx->streams[j]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
//		nVSI = nVSI < 0 ? j : nVSI;
//	}
//	else if (pFmtCtx->streams[j]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
//		nASI = nASI < 0 ? j : nASI;
//	}
//}