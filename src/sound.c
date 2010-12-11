/* sound control functions -- thanks to the SHR project */

#include <linux/input.h>
#include <stdio.h>
#include <poll.h>
#include <glib.h>
#include <alsa/asoundlib.h>

#include <freesmartphone-glib/odeviced/audio.h>
#include <mokosuite/utils/utils.h>
#include <mokosuite/utils/misc.h>

#include "globals.h"
#include "sound.h"

#define HEADPHONE_DEV                   "/dev/input/event4"
#define HEADPHONE_INSERTION_SWITCHCODE  0x02

static const char* sound_settings[] = {
    NULL,                   /* SOUND_STATE_IDLE = 0, */
    "phone_speaker_volume", /* SOUND_STATE_SPEAKER, */
    "phone_handset_volume", /* SOUND_STATE_HANDSET, */
    "phone_headset_volume", /* SOUND_STATE_HEADSET, */
    NULL,                   /* TODO SOUND_STATE_BT, */
    NULL                    /* SOUND_STATE_INIT -- MUST BE LAST */
};

static int sound_defaults[] = {
    -1,       /* SOUND_STATE_IDLE = 0, */
    85,       /* SOUND_STATE_SPEAKER, */
    70,       /* SOUND_STATE_HANDSET, */
    85,       /* SOUND_STATE_HEADSET, */
    -1,       /* TODO SOUND_STATE_BT, */
    -1        /* SOUND_STATE_INIT -- MUST BE LAST */
};

/* The sound state */
static enum SoundState sound_state = SOUND_STATE_IDLE;

/* Controlling sound */
struct SoundControl {
    const char *name;
    snd_hctl_elem_t *element;
    snd_hctl_elem_t *mute_element;
    long min;
    long max;
    unsigned int count; /*number of channels*/
};

static struct SoundControl controls[SOUND_STATE_INIT][CONTROL_END];

/* headset control */
static gboolean headset_present = FALSE;
#if 0
static GPollFD headset_io = {0};
static GSource* headset_src = NULL;
static GSourceFunc headset_func = NULL;
#endif

/* hardware sound controls */
static snd_hctl_t *hctl = NULL;
#if 0
static void (*_sound_volume_changed_callback) (enum SoundControlType type, int volume, void *userdata);
static void *_sound_volume_changed_userdata = NULL;
static void (*_sound_volume_mute_changed_callback) (enum SoundControlType type, int mute, void *userdata);
static void *_sound_volume_mute_changed_userdata = NULL;

static int poll_fd_count = 0;
static struct pollfd *poll_fds = NULL;
#endif

static int* mute_pointer = NULL;

#if 0
static int _sound_volume_changed_cb(snd_hctl_elem_t *elem, unsigned int mask);
static int _sound_volume_mute_changed_cb(snd_hctl_elem_t *elem, unsigned int mask);
#endif

static int _sound_volume_load_stats(struct SoundControl *control)
{
    int err;

    if (!control->element)
        return -1;

    snd_ctl_elem_type_t element_type;
    snd_ctl_elem_info_t *info;
    snd_hctl_elem_t *elem;

    elem = control->element;
    snd_ctl_elem_info_alloca(&info);

    err = snd_hctl_elem_info(elem, info);
    if (err < 0) {
        g_warning("%s", snd_strerror(err));
        return -1;
    }

    /* verify type == integer */
    element_type = snd_ctl_elem_info_get_type(info);

    control->min = snd_ctl_elem_info_get_min(info);
    control->max = snd_ctl_elem_info_get_max(info);
    control->count = snd_ctl_elem_info_get_count(info);
    return 0;
}

long sound_volume_raw_get(enum SoundControlType type)
{
    int err;
    long value;
    unsigned int i,count;

    snd_ctl_elem_value_t *control;
    snd_ctl_elem_value_alloca(&control);

    snd_hctl_elem_t *elem;

    count = controls[sound_state][type].count;
    elem = controls[sound_state][type].element;
    if (!elem || !count) {
        return 0;
    }

    err = snd_hctl_elem_read(elem, control);
    if (err < 0) {
        g_warning("%s", snd_strerror(err));
        return -1;
    }

    value = 0;
    /* FIXME: possible long overflow */
    for (i = 0 ; i < count ; i++) {
        value += snd_ctl_elem_value_get_integer(control, i);
    }
    value /= count;

    return value;
}

int sound_volume_raw_set(enum SoundControlType type, long value)
{
    int err;
    unsigned int i, count;

    snd_hctl_elem_t *elem;
    snd_ctl_elem_value_t *control;


    elem = controls[sound_state][type].element;
    if (!elem) {
        return -1;
    }
    snd_ctl_elem_value_alloca(&control);
    count = controls[sound_state][type].count;

    for (i = 0 ; i < count ; i++) {
        snd_ctl_elem_value_set_integer(control, i, value);
    }

    err = snd_hctl_elem_write(elem, control);
    if (err) {
        g_warning("%s", snd_strerror(err));
        return -1;
    }

    /* FIXME put it somewhere else, this is not the correct place! */
    // TODO phoneui_utils_sound_volume_save(type);
    char* scenario;

    switch (sound_state) {
    case SOUND_STATE_SPEAKER:
        scenario = "gsmspeaker";
        break;
    case SOUND_STATE_HEADSET:
        scenario = "gsmheadset";
        break;
    case SOUND_STATE_HANDSET:
        scenario = "gsmhandset";
        break;
    case SOUND_STATE_BT:
        scenario = "gsmbluetooth";
        break;
    default:
        scenario = NULL;
    }

    if (scenario != NULL)
        odeviced_audio_save_scenario(scenario, NULL, NULL);

    return 0;
}

int sound_volume_get(enum SoundControlType type)
{
    long value;
    long min, max;

    if (!controls[sound_state][type].element) {
        return 0;
    }

    min = controls[sound_state][type].min;
    max = controls[sound_state][type].max;

    value = sound_volume_raw_get(type);
    if (value <= min) {
        value = 0;
    }
    else if (value >= max) {
        value = 100;
    }
    else {
        value = ((double) (value - min) / (max - min)) * 100.0;
    }
    g_debug("Probing volume of control '%s' returned %d",
        controls[sound_state][type].name, (int) value);
    return value;
}

int sound_volume_set(enum SoundControlType type, int percent)
{
    long min, max, value;
    snd_hctl_elem_t *elem;
    snd_ctl_elem_value_t *control;


    elem = controls[sound_state][type].element;
    if (!elem) {
        return -1;
    }
    snd_ctl_elem_value_alloca(&control);
    min = controls[sound_state][type].min;
    max = controls[sound_state][type].max;

    value = min + ((max - min) * percent) / 100;
    sound_volume_raw_set(type, value);
    g_debug("Setting volume for control %s to %d",
            controls[sound_state][type].name, percent);
    return 0;
}

int sound_volume_mute_get(enum SoundControlType type)
{
    int err;

    snd_ctl_elem_value_t *control;
    snd_ctl_elem_value_alloca(&control);

    snd_hctl_elem_t *elem;


    elem = controls[sound_state][type].mute_element;
    if (!elem) {
        return -1;
    }

    err = snd_hctl_elem_read(elem, control);
    if (err < 0) {
        g_warning("%s", snd_strerror(err));
        return -1;
    }

    return !snd_ctl_elem_value_get_boolean(control, 0);
}

int sound_volume_mute_set(enum SoundControlType type, int mute)
{
    int err;

    snd_hctl_elem_t *elem;
    snd_ctl_elem_value_t *control;

    elem = controls[sound_state][type].mute_element;
    if (!elem) {
        return -1;
    }
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_value_set_boolean(control, 0, !mute);

    err = snd_hctl_elem_write(elem, control);
    if (err) {
        g_warning("%s", snd_strerror(err));
        return -1;
    }
    g_debug("Set control %d to %d", type, mute);
    if (mute_pointer)
        *mute_pointer = mute;

    return 0;
}

static snd_hctl_elem_t * _sound_init_get_control_by_name(const char *ctl_name)
{
    snd_ctl_elem_id_t *id;

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, ctl_name);

    return snd_hctl_find_elem(hctl, id);
}

static void
_sound_init_set_volume_control(enum SoundState state, enum SoundControlType type)
{
    const char *ctl_name = controls[state][type].name;
    snd_hctl_elem_t *elem;

    /*if an empty string, return */
    if (*ctl_name == '\0') {
        controls[state][type].element = NULL;
        return;
    }
    elem = controls[state][type].element =
        _sound_init_get_control_by_name(ctl_name);
    if (elem) {
        #if 0
        snd_hctl_elem_set_callback(elem, _sound_volume_changed_cb);
        #endif
    }
    else {
        g_critical("ALSA: No control named '%s' found - "
            "Sound state: %d type: %d", ctl_name, state, type);
    }

}

static void _sound_init_set_volume_mute_control(enum SoundState state, enum SoundControlType type, const char *ctl_name)
{
    snd_hctl_elem_t *elem;

    /*if an empty string, return */
    if (*ctl_name == '\0') {
        controls[state][type].mute_element = NULL;
        return;
    }
    elem = controls[state][type].mute_element =
        _sound_init_get_control_by_name(ctl_name);
    if (elem) {
        #if 0
        snd_hctl_elem_set_callback(elem, _sound_volume_mute_changed_cb);
        #endif
    }
    else {
        g_critical("ALSA: No control named '%s' found - "
            "Sound state: %d type: %d", ctl_name, state, type);
    }

}

static void _sound_init_set_control(const char *_field, enum SoundState state)
{
    char *field;
    char *speaker = NULL;
    char *microphone = NULL;
    char *speaker_mute = NULL;
    char *microphone_mute = NULL;

    /*FIXME: split to a generic function for both speaker and microphone */
    field = malloc(strlen(_field) + strlen("alsa_control_") + 1);
    if (field) {
        /* init for now and for the next if */
        strcpy(field, "alsa_control_");
        strcat(field, _field);

        remote_config_service_get_string(phone_config, field, "speaker", &speaker);
        remote_config_service_get_string(phone_config, field, "microphone", &microphone);
        remote_config_service_get_string(phone_config, field, "speaker_mute", &speaker_mute);
        remote_config_service_get_string(phone_config, field, "microphone_mute", &microphone_mute);

        /* does not yet free field because of the next if */
    }
    if (!speaker) {
        g_message("No speaker value for %s found, using none", _field);
        speaker = g_strdup("");
    }
    if (!microphone) {
        g_message("No microphone value for %s found, using none", _field);
        microphone = g_strdup("");
    }
    controls[state][CONTROL_SPEAKER].mute_element = NULL;
    controls[state][CONTROL_MICROPHONE].mute_element = NULL;
    if (speaker_mute) {
        _sound_init_set_volume_mute_control(state, CONTROL_SPEAKER, speaker_mute);
    }
    if (microphone_mute) {
        _sound_init_set_volume_mute_control(state, CONTROL_MICROPHONE, microphone_mute);
    }

    controls[state][CONTROL_SPEAKER].name = strdup(speaker);
    controls[state][CONTROL_MICROPHONE].name = strdup(microphone);
    _sound_init_set_volume_control(state, CONTROL_SPEAKER);
    _sound_init_set_volume_control(state, CONTROL_MICROPHONE);

    /* Load min, max and count from alsa and init to zero before instead
     * of handling errors, hackish but fast. */
    controls[state][CONTROL_SPEAKER].min = controls[state][CONTROL_SPEAKER].max = 0;
    controls[state][CONTROL_MICROPHONE].min = controls[state][CONTROL_MICROPHONE].max = 0;
    controls[state][CONTROL_MICROPHONE].count = 0;
    /* The function handles the case where the control has no element */
    _sound_volume_load_stats(&controls[state][CONTROL_SPEAKER]);
    _sound_volume_load_stats(&controls[state][CONTROL_MICROPHONE]);

    if (field) {
        int tmp;

        /* If the user specifies his own min and max for that control,
         * check values for sanity and then apply them. */
        if (remote_config_service_get_int(phone_config, field, "microphone_min", &tmp)) {
            if (tmp > controls[state][CONTROL_MICROPHONE].min &&
                tmp < controls[state][CONTROL_MICROPHONE].max) {

                controls[state][CONTROL_MICROPHONE].min = tmp;
            }
        }
        if (remote_config_service_get_int(phone_config, field, "microphone_max", &tmp)) {
            if (tmp > controls[state][CONTROL_MICROPHONE].min &&
                tmp < controls[state][CONTROL_MICROPHONE].max) {

                controls[state][CONTROL_MICROPHONE].max = tmp;
            }
        }
        if (remote_config_service_get_int(phone_config, field, "speaker_min", &tmp)) {
            if (tmp > controls[state][CONTROL_SPEAKER].min &&
                tmp < controls[state][CONTROL_SPEAKER].max) {

                controls[state][CONTROL_SPEAKER].min = tmp;
                g_debug("setting speaker_min to %d", (int) tmp);
            }
        }
        if (remote_config_service_get_int(phone_config, field, "speaker_max", &tmp)) {
            if (tmp > controls[state][CONTROL_SPEAKER].min &&
                tmp < controls[state][CONTROL_SPEAKER].max) {

                controls[state][CONTROL_SPEAKER].max = tmp;
            }
        }
        free(field);
    }

    g_free(speaker);
    g_free(microphone);
    g_free(speaker_mute);
    g_free(microphone_mute);
}

#if 0
static enum SoundControlType _sound_volume_element_to_type(snd_hctl_elem_t *elem)
{
    int i;

    for (i = 0 ; i < CONTROL_END ; i++) {
        if (controls[sound_state][i].element == elem) {
            return i;
        }
    }
    return CONTROL_END;
}

static enum SoundControlType _sound_volume_mute_element_to_type(snd_hctl_elem_t *elem)
{
    int i;

    for (i = 0 ; i < CONTROL_END ; i++) {
        if (controls[sound_state][i].mute_element == elem) {
            return i;
        }
    }
    return CONTROL_END;
}
#endif

#if 0
static int _sound_volume_changed_cb(snd_hctl_elem_t *elem, unsigned int mask)
{
    snd_ctl_elem_value_t *control;
    enum SoundControlType type;
    int volume;


        if (mask == SND_CTL_EVENT_MASK_REMOVE)
                return 0;
        if (mask & SND_CTL_EVENT_MASK_VALUE) {
                snd_ctl_elem_value_alloca(&control);
                snd_hctl_elem_read(elem, control);
                type = _sound_volume_element_to_type(elem);
                if (type != CONTROL_END) {
            volume = sound_volume_get(type);
            g_debug("Got alsa volume change for control '%s', new value: %d%%",
                controls[sound_state][type].name, volume);
            if (_sound_volume_changed_callback) {
                _sound_volume_changed_callback(type, volume, _sound_volume_changed_userdata);
            }
        }
        }
    return 0;
}

static int _sound_volume_mute_changed_cb(snd_hctl_elem_t *elem, unsigned int mask)
{
    snd_ctl_elem_value_t *control;
    enum SoundControlType type;
    int mute;


        if (mask == SND_CTL_EVENT_MASK_REMOVE)
                return 0;
        if (mask & SND_CTL_EVENT_MASK_VALUE) {
                snd_ctl_elem_value_alloca(&control);
                snd_hctl_elem_read(elem, control);
                type = _sound_volume_mute_element_to_type(elem);
                if (type != CONTROL_END) {
            mute = sound_volume_mute_get(type);
            g_debug("Got alsa mute change for control type '%d', new value: %d",
                type, mute);
            if (_sound_volume_mute_changed_callback) {
                _sound_volume_mute_changed_callback(type, mute, _sound_volume_mute_changed_userdata);
            }
        }
        }
    return 0;
}
#endif

// callback settaggio remoto volume handset
static void _sound_speaker_volume(RemoteConfigService* config, const char* section, const char* key, GValue* value)
{
    if (!strcmp(section, "phone")) {
        int i;
        for (i = 0; i <= SOUND_STATE_INIT; i++)
            if (sound_settings[i] != NULL && strcmp(key, sound_settings[i])) return;

        int percent = g_value_get_int(value);
        g_debug("Speaker(%s) volume changed to %p", key, value);
        sound_volume_set(CONTROL_SPEAKER, percent);
    }
}

static void _sound_volume_setting_init(RemoteConfigService* config, const char* section, const char* key, int default_val)
{
    g_debug("Init sound volume for %s to %d", key, default_val);

    int value = -1;
    if (!remote_config_service_get_int(config, section, key, &value))
        remote_config_service_set_int(config, section, key, default_val);
}

#if 0
static gboolean _sourcefunc_prepare(GSource *source, gint *timeout)
{
    (void) source;
    (void) timeout;

    return (FALSE);
}

static gboolean _sourcefunc_check(GSource *source)
{
    int f;
    (void) source;

    for (f = 0; f < poll_fd_count; f++) {
        if (poll_fds[f].revents & G_IO_IN)
            return (TRUE);
    }
    return (FALSE);
}

static gboolean _sourcefunc_dispatch(GSource *source, GSourceFunc callback, gpointer userdata)
{
    (void) source;
    (void) callback;
    (void) userdata;

    snd_hctl_handle_events(hctl);

    return (TRUE);
}

/*
 *** Thanks to jdd ***
# Hexdump of /dev/input/event0
# When jack inserted
#     9131 48be a594 0001 0005 0002 0001 0000
#     9131 48be a5f9 0001 0000 0000 0000 0000
# When jack removed
#     9131 48be a594 0001 0005 0002 0000 0000
#     9131 48be a5f9 0001 0000 0000 0000 0000
*/

static gboolean _headset_prepare( GSource* source, gint* timeout )
{
    return FALSE;
}


static gboolean _headset_check( GSource* source )
{
    return (headset_io.revents & G_IO_IN);
}


static gboolean _headset_dispatch( GSource* source, GSourceFunc callback, gpointer data )
{
    //g_debug("Dispatching headphone insertion");

    if ( headset_io.revents & G_IO_IN ) {

        struct input_event event;
        if (read(headset_io.fd, &event, sizeof( struct input_event )) > 0) {
            //g_debug("Input event [type = %d, code = %d, value = %d]", event.type, event.code, event.value);

            if (event.type == 5 && event.code == HEADPHONE_INSERTION_SWITCHCODE) {
                //g_debug("Headphone insertion! event.value = %d", event.value);

                gboolean presence = ( event.value == 1 ); /* inserted */
                //else if ( event.value == 0 ) /* released */

                if (presence ^ headset_present && headset_func != NULL)
                    headset_func(GINT_TO_POINTER(presence));

                headset_present = presence;
            }
        }
    }

    return TRUE;
}
#endif

static void _update_volume(GError *e, gpointer data)
{
    // recupera il volume da usare e impostalo
    if (sound_settings[sound_state] != NULL && e == NULL) {
        int value = -1;
        remote_config_service_get_int(phone_config, "phone", sound_settings[sound_state], &value);

        GValue* gv = g_value_from_int(value >= 0 ? value : sound_defaults[sound_state]);
        _sound_speaker_volume(phone_config, "phone", sound_settings[sound_state], gv);
        g_value_free(gv);
    }
}

int sound_state_set(enum SoundState state)
{
    const char *scenario = NULL;
    /* if there's nothing to do, abort */
    if (state == sound_state) {
        return 0;
    }

    /* allow INIT only if sound_state was IDLE */
    if (state == SOUND_STATE_INIT) {
        if (sound_state == SOUND_STATE_IDLE) {
            if (headset_present)
                state = SOUND_STATE_HEADSET;
            else
                state = SOUND_STATE_HANDSET;

            //state = SOUND_STATE_HANDSET;
        }
        else {
            return 1;
        }
    }

    g_debug("Setting sound state to %d", state);

    switch (state) {
    case SOUND_STATE_SPEAKER:
        scenario = "gsmspeaker";
        break;
    case SOUND_STATE_HEADSET:
        scenario = "gsmheadset";
        break;
    case SOUND_STATE_HANDSET:
        scenario = "gsmhandset";
        break;
    case SOUND_STATE_BT:
        scenario = "gsmbluetooth";
        break;
    case SOUND_STATE_IDLE:
        /* return to the last active scenario */
        g_debug("Pulled last controlled scenario");
        odeviced_audio_pull_scenario(NULL, NULL);
        goto end;
        break;
    default:
        break;
    }

    /* if the previous state was idle (i.e not controlled by us)
     * we should push the scenario */
    /*FIXME: fix casts, they are there just because frameworkd-glib
     * is broken there */

    if (sound_state == SOUND_STATE_IDLE) {
        odeviced_audio_push_scenario((char *) scenario, _update_volume, NULL);
    }
    else {
        odeviced_audio_set_scenario((char *) scenario, _update_volume, NULL);
    }

end:
    sound_state = state;
    return 0;

}

enum SoundState sound_state_get(void)
{
    return sound_state;
}

gboolean sound_headset_present(void)
{
    return headset_present;
}

void sound_headset_presence_callback(GSourceFunc func)
{
#if 0
    headset_func = func;
#endif
}

void sound_set_mute_pointer(int* ptr)
{
    mute_pointer = ptr;
}

int sound_init(void)
{
    int err;
    char *device_name = NULL;

    sound_state = SOUND_STATE_IDLE;
    remote_config_service_get_string(phone_config, "alsa", "hardware_control_name", &device_name);
    if (!device_name) {
        g_message("No hw control found, using default");
        device_name = g_strdup("hw:0");
    }

    if (hctl) {
        snd_hctl_close(hctl);
    }
    err = snd_hctl_open(&hctl, device_name, 0);
    if (err) {
        g_critical("%s", snd_strerror(err));
        return err;
    }

    g_free(device_name);

    err = snd_hctl_load(hctl);
    if (err) {
        g_critical("%s", snd_strerror(err));
    }

    _sound_init_set_control("idle", SOUND_STATE_IDLE);
    _sound_init_set_control("bluetooth", SOUND_STATE_BT);
    _sound_init_set_control("handset", SOUND_STATE_HANDSET);
    _sound_init_set_control("headset", SOUND_STATE_HEADSET);
    _sound_init_set_control("speaker", SOUND_STATE_SPEAKER);

    snd_hctl_nonblock(hctl, 1);

    // TODO input event for headset

    // recupero impostazioni iniziali
    _sound_volume_setting_init(phone_config, "phone", sound_settings[SOUND_STATE_HANDSET], sound_defaults[SOUND_STATE_HANDSET]);
    _sound_volume_setting_init(phone_config, "phone", sound_settings[SOUND_STATE_HEADSET], sound_defaults[SOUND_STATE_HEADSET]);
    _sound_volume_setting_init(phone_config, "phone", sound_settings[SOUND_STATE_SPEAKER], sound_defaults[SOUND_STATE_SPEAKER]);
    g_signal_connect(G_OBJECT(phone_config), "changed", G_CALLBACK(_sound_speaker_volume), NULL);

    return err;
}

void sound_deinit(void)
{
    sound_state = SOUND_STATE_IDLE;
    snd_hctl_close(hctl);
    hctl = NULL;

    #if 0
    if (headset_src != NULL) {
        g_source_destroy(headset_src);
        headset_src = NULL;
    }

    // chiusura headset_io
    close(headset_io.fd);
    headset_io.fd = 0;
    #endif
}
