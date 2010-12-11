#ifndef __SOUND_H
#define __SOUND_H

enum SoundControlType {
    CONTROL_SPEAKER = 0,
    CONTROL_MICROPHONE,
    CONTROL_END /* used to mark end */
};

enum SoundState {
    SOUND_STATE_IDLE = 0,
    SOUND_STATE_SPEAKER,
    SOUND_STATE_HANDSET,
    SOUND_STATE_HEADSET,
    SOUND_STATE_BT,
    SOUND_STATE_INIT /* MUST BE LAST */
};

long sound_volume_raw_get(enum SoundControlType type);
int sound_volume_raw_set(enum SoundControlType type, long value);

int sound_volume_get(enum SoundControlType type);
int sound_volume_set(enum SoundControlType type, int percent);

int sound_volume_mute_get(enum SoundControlType type);
int sound_volume_mute_set(enum SoundControlType type, int mute);

int sound_state_set(enum SoundState state);
enum SoundState sound_state_get(void);

gboolean sound_headset_present(void);
void sound_headset_presence_callback(GSourceFunc func);

void sound_set_mute_pointer(int* ptr);

int sound_init(void);
void sound_deinit(void);

#endif  /* __SOUND_H */
