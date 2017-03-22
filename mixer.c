#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <linux/input.h>

#include <alsa/asoundlib.h>


#include "hitinga.h"
#include "mixer.h"

#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT2	"Headphone"
#define VOLUME_MIXER_ALSA_INDEX_DEFAULT		0

struct alsa_mixer {
	/** the base mixer class */
	struct mixer  base;

	const char *device;
	const char *control;
	unsigned int index;

	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	long volume_min;
	long volume_max;
	int volume_set;
};

struct mixer * mixer_init()
{
	struct alsa_mixer *am = malloc(sizeof(struct alsa_mixer));

	am->device = VOLUME_MIXER_ALSA_DEFAULT;
	am->control = VOLUME_MIXER_ALSA_CONTROL_DEFAULT; 
	am->index = VOLUME_MIXER_ALSA_INDEX_DEFAULT;

    debugline;

	return &am->base;
}

void mixer_finish(struct mixer *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;

    if(am!=NULL){
        free(am);
        am=NULL;
    }

	/* free libasound's config cache */
	snd_config_update_free_global();
}

void mixer_close(struct mixer *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;

    if(am->handle != NULL){
        snd_mixer_close(am->handle);
        am->handle=NULL;
    }
}

int mixer_open(struct mixer *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;
	int err;
	snd_mixer_elem_t *elem;

	am->volume_set = -1;

	err = snd_mixer_open(&am->handle, 0);
	if (err < 0) {
		ERROR_PRINTF(
			    "snd_mixer_open() failed: %s\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_mixer_attach(am->handle, am->device)) < 0) {
		mixer_close(data);
		ERROR_PRINTF(
			    "failed to attach to %s: %s\n",
			    am->device, snd_strerror(err));
		return 0;
	}

	if ((err = snd_mixer_selem_register(am->handle, NULL,
		    NULL)) < 0) {
		mixer_close(data);
		ERROR_PRINTF(
			    "snd_mixer_selem_register() failed: %s\n",
			    snd_strerror(err));
		return 0;
	}

	if ((err = snd_mixer_load(am->handle)) < 0) {
		mixer_close(data);
		ERROR_PRINTF(
			    "snd_mixer_load() failed: %s\n",
			    snd_strerror(err));
		return 0;
	}

	elem = snd_mixer_first_elem(am->handle);

	while (elem) {
        DEBUG_PRINTF("mixer name:[%s]\n",snd_mixer_selem_get_name(elem));
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE) {

			if ((strcasecmp(am->control,
						snd_mixer_selem_get_name(elem)) == 0) &&
			    (am->index == snd_mixer_selem_get_index(elem))) {
				break;
			}
		}
		elem = snd_mixer_elem_next(elem);
	}

	if (elem) {
		am->elem = elem;
		snd_mixer_selem_get_playback_volume_range(am->elem,
							  &am->volume_min,
							  &am->volume_max);
#ifdef ENABLE_CDB
		int ret;
		char val[4];
		ret = cdb_get("$hw_vol_max", &val);
		if(ret >= 0)
			am->volume_max = atoi(val);
		else
			am->volume_max = 0xff;

		ret = cdb_get("$hw_vol_min", &val);
		if(ret >= 0)
			am->volume_min = atoi(val);
		else
			am->volume_min = 0;
#endif

		return 1;
	}

	mixer_close(data);
    ERROR_PRINTF("no such mixer control: %s\n", am->control);

	return 0;
}

int mixer_get_vol(struct mixer *mixer)
{
	struct alsa_mixer *am = (struct alsa_mixer *)mixer;
	int err;
	int ret;
	long level;

	if(am->handle == NULL)
        return -1;

	err = snd_mixer_handle_events(am->handle);
	if (err < 0) {
		ERROR_PRINTF(
			    "snd_mixer_handle_events() failed: %s\n",
			    snd_strerror(err));
		return 0;
	}

	err = snd_mixer_selem_get_playback_volume(am->elem,
						  SND_MIXER_SCHN_FRONT_LEFT,
						  &level);
	if (level == 0)
		level = am->volume_min;
	else if (level == 0xff)
		level = am->volume_max;

	if (err < 0) {
		ERROR_PRINTF(
			    "failed to read ALSA volume: %s\n",
			    snd_strerror(err));
		return 0;
	}

	ret = ((am->volume_set / 100.0) * (am->volume_max - am->volume_min)
	       + am->volume_min) + 0.5;
	if (am->volume_set > 0 && ret == level) {
		ret = am->volume_set;
	} else {
		ret = (int)(100 * (((float)(level - am->volume_min)) /
				   (am->volume_max - am->volume_min)) + 0.5);
	}

	return ret;
}

int mixer_set_vol(struct mixer *mixer, unsigned volume)
{
	struct alsa_mixer *am = (struct alsa_mixer *)mixer;
	float vol;
	long level;
	int err;

	if(am->handle == NULL)
        return -1;

	vol = volume;

	am->volume_set = vol + 0.5;

	level = (long)(((vol / 100.0) * (am->volume_max - am->volume_min) +
			am->volume_min) + 0.5);
	level = level > am->volume_max ? am->volume_max : level;
	level = level < am->volume_min ? am->volume_min : level;

	if(level > am->volume_min)
		err = snd_mixer_selem_set_playback_volume_all(am->elem, level);
	else
		err = snd_mixer_selem_set_playback_volume_all(am->elem, 0);

	if (err < 0) {
		ERROR_PRINTF(
			    "failed to set ALSA volume: %s\n",
			    snd_strerror(err));
		return 0;
	}

	return 1;
}


