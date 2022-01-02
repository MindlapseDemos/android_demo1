#ifndef MUSIC_H_
#define MUSIC_H_

int init_music(void);
void destroy_music(void);

void play_music(void);
void stop_music(void);
void seek_music(long tm);

void set_music_volume(float vol);

#endif	/* MUSIC_H_ */
