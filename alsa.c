#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>


#include <string.h>
#include<fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>


#include "hitinga.h"

#define HITINGA_ALSA_PLAY_ALWAYS 0

static snd_pcm_t * g_audio_handler=NULL;
static int sample_rate;
static int sample_format;
static int sample_channels;
static int sample_size;

int alsa_close()
{
    if( g_audio_handler != NULL ){
        snd_pcm_drain(g_audio_handler);
        snd_pcm_close(g_audio_handler);
        g_audio_handler = NULL;
    debugline;

        return 1;
    }

    return 0;
}

int alsa_open(int rate,int format,int channels , int ismp3)
{
    int rc;
    int dir = 0;
    int again=0;
    snd_pcm_hw_params_t*   params = NULL;

    debugline;
    if(g_audio_handler!=NULL){
    debugline;
        return 1;
    }

    sample_rate = rate;
    sample_format = format;
    sample_channels = channels;
    sample_size = sample_format * sample_channels;

    snd_pcm_hw_params_alloca(&params); 

ALSA_REOPEN:
    again++;

    if( again > 2 ){
        if( g_audio_handler != NULL ){
            snd_pcm_drain(g_audio_handler);
            snd_pcm_close(g_audio_handler);
            g_audio_handler = NULL;
        }
        ERROR_PRINTF("Error to connect alsa output\n");
        return -2;
    }

    if( g_audio_handler != NULL ){
        snd_pcm_drain(g_audio_handler);
        snd_pcm_close(g_audio_handler);
        g_audio_handler = NULL;
    }

    rc = snd_pcm_open(&g_audio_handler, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if(rc < 0) {
        ERROR_PRINTF("snd_pcm_open %s\n", snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }

    rc = snd_pcm_hw_params_any(g_audio_handler, params); 
    if(rc < 0) {
        ERROR_PRINTF("snd_pcm_hw_params_any %s\n", snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }

    rc = snd_pcm_hw_params_set_access(g_audio_handler, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(rc < 0) {
        ERROR_PRINTF("snd_pcm_hw_params_set_access %s\n", snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }

    rc = snd_pcm_hw_params_set_format(g_audio_handler, params, sample_format);
    if(rc < 0) {
        ERROR_PRINTF("snd_pcm_hw_params_set_format %s\n", snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }


    rc = snd_pcm_hw_params_set_rate_near(g_audio_handler, params, &sample_rate, &dir); 
    if(rc < 0) {
        ERROR_PRINTF("snd_pcm_hw_params_set_rate_near %s\n", snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }
	
    /*
     * This is temp patch
     **/
    if(ismp3==0){
        DEBUG_PRINTF("Set sample_channels:%d\n",sample_channels);
        rc = snd_pcm_hw_params_set_channels(g_audio_handler, params, sample_channels); 
        if(rc < 0) {
            ERROR_PRINTF("snd_pcm_hw_params_set_channels %s\n", snd_strerror(rc));
            sleep(1);
            goto ALSA_REOPEN;
        }
    }
    else{
        DEBUG_PRINTF("No Set sample_channels:%d\n",sample_channels);
    }

    int frames = 1024;
    int periodsize = frames * sample_channels;
    rc = snd_pcm_hw_params_set_buffer_size_near(g_audio_handler, params, &periodsize);
    if (rc < 0) {
        ERROR_PRINTF("Unable to set buffer size %li : %s\n", frames * 2, snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }

    periodsize /= 2;
    rc = snd_pcm_hw_params_set_period_size_near(g_audio_handler, params, &periodsize, 0);
    if (rc < 0) {
        ERROR_PRINTF("Unable to set period size %li : %s\n", periodsize,  snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }

    rc = snd_pcm_hw_params(g_audio_handler, params);
    if(rc < 0) {
        ERROR_PRINTF("snd_pcm_hw_params %li : %s\n", periodsize,  snd_strerror(rc));
        sleep(1);
        goto ALSA_REOPEN;
    }

    return 0;
}

int alsa_play(unsigned char * data , unsigned int len)
{
    int r = 0,err=0;

    if( g_audio_handler == NULL ){
        ERROR_PRINTF("audio handler is null\n");
        return -1;
    }

//    printf("%d sample_size %d len\n",sample_size,len);

    do{
        r = snd_pcm_writei ( g_audio_handler , data , len/sample_size);
        if (r == -EAGAIN /*|| (r >= 0 && (size_t)r < len/2) */) {
            snd_pcm_wait(g_audio_handler, 1000);
        } else if (r == -EPIPE) {
            snd_pcm_prepare(g_audio_handler);
        } else if (r == -ESTRPIPE) {
            int iresume=0;
            while ((r = snd_pcm_resume(g_audio_handler)) == -EAGAIN){
                sleep(100*1000);
                if( iresume>4 ) break;
                iresume++;
            }

            if (err < 0) {  
                err = snd_pcm_prepare(g_audio_handler);  
                if (err < 0){
                    ERROR_PRINTF("Can't recovery from suspend, prepare failed: %s\n",  
                                snd_strerror(err));
                    break;
                }
            }
        } else if (r < 0) {
            ERROR_PRINTF( "Error snd_pcm_writei: [%s]", snd_strerror(r));
            break;
        }

        if (r > 0){
            len -= r*sample_size;
            data+=r*sample_size;
        }

    }while(len>0);

    return 0;
}

