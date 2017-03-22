#include <stdio.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <alsa/asoundlib.h>
#include <pthread.h>

#include "hitinga.h"
#include "server.h"
#include "common.h"
#include "stream.h"
#include "queue.h"

char ghtpurl[1024] ;
htp_status_e  gstatus;


static void
copy_interleave_frame2(uint8_t *dest, uint8_t **src,
		       unsigned nframes, unsigned nchannels,
		       unsigned sample_size)
{
    unsigned frame,channel;

	for (frame = 0; frame < nframes; ++frame) {
		for (channel = 0; channel < nchannels; ++channel) {
			memcpy(dest, src[channel] + frame * sample_size,
			       sample_size);
			dest += sample_size;
		}
	}
}

/**
 * Copy PCM data from a AVFrame to an interleaved buffer.
 */
static int
copy_interleave_frame(AVCodecContext *codec_context,
		      AVFrame *frame,
		      uint8_t **output_buffer,
		      uint8_t **global_buffer, int *global_buffer_size)
{
	int plane_size;
	const int data_size =
		av_samples_get_buffer_size(&plane_size,
					   codec_context->channels,
					   frame->nb_samples,
					   codec_context->sample_fmt, 1);
	if (data_size <= 0)
		return data_size;

	if (av_sample_fmt_is_planar(codec_context->sample_fmt) &&
	    codec_context->channels > 1) {
		if(*global_buffer_size < data_size) {
			av_freep(global_buffer);

			*global_buffer = (uint8_t*)av_malloc(data_size);

			if (!*global_buffer)
				/* Not enough memory - shouldn't happen */
				return AVERROR(ENOMEM);
			*global_buffer_size = data_size;
		}
		*output_buffer = *global_buffer;
		copy_interleave_frame2(*output_buffer, frame->extended_data,
				       frame->nb_samples,
				       codec_context->channels,
				       av_get_bytes_per_sample(codec_context->sample_fmt));
	} else {
		*output_buffer = frame->extended_data[0];
	}

	return data_size;
}

static int ffmpeg_to_alsa_fmtval(AVCodecContext *pCodecCtx)
{
    printf("name=%s\n" , av_get_sample_fmt_name(pCodecCtx->sample_fmt));
    switch(pCodecCtx->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            return SND_PCM_FORMAT_S8;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_FLTP:
        case AV_SAMPLE_FMT_S16P:
            return SND_PCM_FORMAT_S16;
        case AV_SAMPLE_FMT_S32:
            return SND_PCM_FORMAT_S32;
        default:
            return -1;
    }
}

static double time_from_ffmpeg(int64_t t, const AVRational time_base)
{
    assert(t != (int64_t)AV_NOPTS_VALUE);
    return (double)av_rescale_q(t, time_base, (AVRational){1, 1024}) / (double)1024;
}

void ffmpeg_print_error(int err,int line,const char *fname)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));

    ERROR_PRINTF("[%s:%d][%s]\n",fname,line,errbuf_ptr);

    return;
}

static int64_t
timestamp_fallback(int64_t t)
{
	return (t != AV_NOPTS_VALUE) ? t : 0;
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base =  &fmt_ctx->streams[pkt->stream_index]->time_base;
    printf("time_base:%lf\n",  time_from_ffmpeg(pkt->pts, *time_base));
}

int read_err_count = 0;
int g_av_debug = 0;
static int interrupt_cb( void   *ctx)
{	
	if(g_av_debug == 1){
		DEBUG_PRINTF("===========>av_read_frame block  read_err_count=[%d]\n", read_err_count);
		read_err_count++;
	}
	else{
		read_err_count = 0;
	}
	if(read_err_count >= 200){
		read_err_count = 0;
		DEBUG_PRINTF("====================>need return to user\n");
		ModMsgQPutMsg(PLAY_MSG_KEY,"pnext");
		return 1;
	}
	return 0;
}

int HTPFFmpegProc(char * fileName)
{
    AVFormatContext *pFmtCtx = NULL;
    AVCodec *pCodec;
    AVPacket packet;
    AVFrame *pFrame;
    AVCodecContext *pCodeCtx;
    int ret=0;
    int len=0;
    int gotFrames;
    int i, audioStream = -1;

    int g_global_buffer_size=0;
    uint8_t * g_global_buffer=NULL;

    printf("url:[ %s] \n",fileName);

    duration = 0;
    position = 0;

    av_register_all();
    avformat_network_init();
    pFmtCtx = avformat_alloc_context();
    pFmtCtx->interrupt_callback.callback = interrupt_cb;
    pFmtCtx->interrupt_callback.opaque = pFmtCtx;

    ret = avformat_open_input(&pFmtCtx, fileName, NULL, NULL);
    if(ret!=0){
        ffmpeg_print_error(ret,__LINE__,__FUNCTION__);
        return 3;
    }

    ret = avformat_find_stream_info(pFmtCtx, NULL);
    if(ret!=0){
        ffmpeg_print_error(ret,__LINE__,__FUNCTION__);
        return 3;
    }

    for (i = 0; i < pFmtCtx->nb_streams; ++i) {
        if ((*(pFmtCtx->streams + i))->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = i;
            break;
        }
    }

    if(audioStream==-1){
        ERROR_PRINTF("No audio stream\n");
        return 3;
    }

    AVStream *st=pFmtCtx->streams[audioStream];
    pCodeCtx = st->codec;
    pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
    if(pCodec==NULL){
        ffmpeg_print_error(ret,__LINE__,__FUNCTION__);
        return 3;
    }

    avcodec_open2(pCodeCtx, pCodec, NULL);

#if LIBAVUTIL_VERSION_MAJOR >= 53
    pFrame = av_frame_alloc();
#else
    pFrame = avcodec_alloc_frame();
#endif

    int sample_rate = pCodeCtx->sample_rate;
    int channels = pCodeCtx->channels;
    int sample_fmt = ffmpeg_to_alsa_fmtval( pCodeCtx );
    int sample_size = av_get_bytes_per_sample(sample_fmt);
	if(channels == 1){
		sample_size = sample_size/2;
		DEBUG_PRINTF("===========>sample_size=[%d]\n" , sample_size);
	}

    duration = st->duration * av_q2d(st->time_base);
    int64_t start_time = timestamp_fallback(st->start_time);

    if(duration<=1){
		duration = 1;
	    goto EXIT;
    }

    unsigned int cmd=HTP_COMMAND_NONE;
    char param[136]={0};
    unsigned handle;
    printf("fmt: %d\n", pCodeCtx->sample_fmt);
    printf("rate: %d\n", pCodeCtx->sample_rate);
    printf("channels: %d\n", pCodeCtx->channels);
    printf("bitrate:%ld\n", pFmtCtx->bit_rate);
    printf("codec name:%s\n", pCodeCtx->codec->long_name);
    printf("time_base:%lld\n",pCodeCtx->time_base);
    printf("fmtname:%s\n", pFmtCtx->iformat->name);
    printf("audio streamid:%d\n", audioStream);
    printf("sample_fmt:%d\n" , sample_fmt);
    printf("sample_size:%d\n" , sample_size);
    printf("duration:%d\n", duration);
    printf("start time:%d\n", start_time);

    HTPBQCreate();

    if(HTPStreamStart(sample_rate , sample_fmt , channels , ismp3 )<0)
        return -1;

    int result=0;

    gstatus = HTP_STATUS_PLAY;
	char msg[136] = {0};

    while (1) {
        /*
         *Receive msg
         */
        cmd = HTP_COMMAND_NONE;
        HTPGetCommand(&cmd , param , &handle);

        if(cmd==HTP_COMMAND_DESTORY){
            gstatus = HTP_STATUS_STOP;
            result=2;
            break;
        }
        else if(cmd==HTP_COMMAND_STOP){
            gstatus = HTP_STATUS_STOP;
            break;
        }
        else if(cmd==HTP_COMMAND_ADDPLAY){
            strcpy(ghtpurl , param);
            result=1;
            break;
        }
        else if(cmd==HTP_COMMAND_PAUSE){
            if(gstatus==HTP_STATUS_PLAY){
                ret = av_read_pause(pFmtCtx);
                if(ret<0){
                    ffmpeg_print_error(ret,__LINE__,__FUNCTION__);
                    //return -1;
                }
                HTPStreamPause();
                gstatus = HTP_STATUS_PAUSE;
            }
        }
        else if(cmd==HTP_COMMAND_PLAY){
            if(gstatus!=HTP_STATUS_PLAY){
                ret = av_read_play(pFmtCtx);
                if(ret<0){
                    ffmpeg_print_error(ret,__LINE__,__FUNCTION__);
                    //return -1;
                }

                HTPStreamResume();
                gstatus = HTP_STATUS_PLAY;
            }
        }
        else if(cmd==HTP_COMMAND_SEEK){
            printf("atoi param:%d\n",atoi(param));
            printf("starttime:%d\n",start_time);
            printf("den:%d\n",st->time_base.den);
            printf("num:%d\n",st->time_base.num);
            printf("AV_TIME_BASE:%d\n",AV_TIME_BASE);
            //            int64_t timestamp = atoi(param) * 1000 * AV_TIME_BASE; //+ start_time; 
            int64_t timestamp = av_rescale(atoi(param)*1000*1000 , st->time_base.den, AV_TIME_BASE * (int64_t)st->time_base.num); //+ start_time ;

            printf("timestamp:%lld param:%d\n", timestamp , atoi(param));
            av_seek_frame(pFmtCtx, audioStream, timestamp, AVSEEK_FLAG_BACKWARD|AVSEEK_FLAG_ANY);
            gstatus = HTP_STATUS_PLAY;

            printf("seek over........\n");
        }
        else if(cmd==HTP_COMMAND_GET_STATUS){
            HTPReqStatus(handle,gstatus,position,duration);
        }
        else if(cmd==HTP_COMMAND_GET_VOLUME){
            //            int vol = mixer_get_vol(mixer);
            //            HTPReqGetVolume(handle,vol);
        }
        else if(cmd==HTP_COMMAND_SET_VOLUME){
            //            HTPReqSetVolume(handle,atoi(param));
        }

        /*
         * If pause status , we will be sleep and continue;
         */
        if(gstatus==HTP_STATUS_PAUSE){
            usleep(20*1000);
            continue;
        }

        /*
         * read frame buffer from ffmpeg
         */
        ret = av_read_frame(pFmtCtx, &packet);
        if(ret<0){
            ffmpeg_print_error(ret,__LINE__,__FUNCTION__);
            break;
        }

        /* 
         * analysis and decode
         **/
        if (packet.stream_index == audioStream) {

            //          log_packet(pFmtCtx,&packet);

            position = time_from_ffmpeg(packet.pts, st->time_base);
            int64_t curtimestamp = av_rescale_q(packet.pts, st->time_base, (AVRational){1, 1024});

            if (!strcmp(pFmtCtx->iformat->name, "ape")) {
                for (i = 0; i < packet.size;) {
                    gotFrames = 0;
                    len = avcodec_decode_audio4(pCodeCtx, pFrame, &gotFrames, &packet);
                    HTPBQPut((unsigned char *)pFrame->data[0] , len ,curtimestamp); 
                    i += len;
                }
            } 
            else 
            {
                int audio_size = 0;
                uint8_t *output_buffer;
                gotFrames = 0;

                int len = avcodec_decode_audio4(pCodeCtx, pFrame, &gotFrames, &packet);
                if (len > 0 && gotFrames) {
                    audio_size = copy_interleave_frame(pCodeCtx, pFrame, &output_buffer,
                            &g_global_buffer, &g_global_buffer_size);
                    if(audio_size>0){
                        HTPBQPut(output_buffer,audio_size,curtimestamp); 
                    }
                }
            }
        }

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 25, 100)
        av_packet_unref(&packet);
#else
        av_free_packet(&packet);
#endif
    }

    av_freep(&pFrame);
    avcodec_close(pCodeCtx);
    avformat_close_input(&pFmtCtx);
    HTPStreamStop();
    HTPBQDestory();

    return result;
}

int HTPInit()
{
    return 0;
}

int HTPDestory()
{
    return 0;
}


int MainProc()
{
    int ret;
    unsigned int cmd;
    char param[1024] ;
    unsigned int handle;

    HTPMsgQCreate();
    HTPStreamInit();

    do{
        gstatus = HTP_STATUS_STOP;

        usleep(20*1000);
        /*
         *Receive msg
         */
        cmd = HTP_COMMAND_NONE;
        HTPGetCommand(&cmd , param , &handle);

        if(cmd==HTP_COMMAND_DESTORY){
            break;
        }
        else if(cmd==HTP_COMMAND_ADDPLAY){
            strcpy(ghtpurl , param);
ADDNEW:
            ret = HTPFFmpegProc( ghtpurl );
            if( ret == 2 ){
                break;
            }
            else if( ret == 1){
                goto ADDNEW;
            }
        }
        else if(cmd==HTP_COMMAND_GET_STATUS){
            HTPReqStatus(handle,gstatus,0,0);
        }
        else if(cmd==HTP_COMMAND_GET_VOLUME){
            //    int vol = mixer_get_vol(mixer);
            //    HTPReqGetVolume(handle,vol);
        }
        else if(cmd==HTP_COMMAND_SET_VOLUME){
            //    HTPReqSetVolume(handle,atoi(param));
        }
    }while(1); 

    HTPStreamDestory();

    return 0;
}

int main()
{
    HTPInit();

    /* create mainloop */
    MainProc();

    HTPDestory();

    return 0;
}

