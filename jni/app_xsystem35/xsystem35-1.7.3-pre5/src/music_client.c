/*
 * music_client.c  music client ��
 *
 * Copyright (C) 1997-1998 Masaki Chikama (Wren) <chikama@kasumi.ipl.mech.nagoya-u.ac.jp>
 *               1998-                           <masaki-c@is.aist-nara.ac.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/
/* $Id: music_client.c,v 1.18 2004/10/31 04:18:06 chikama Exp $ */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <glib.h>

#include "portab.h"
#include "music.h"
#include "music_client.h"
#include "counter.h"
#include "pcmlib.h"
#include "nact.h"
#include "ald_manager.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <android/log.h>

/* ���줾��� sub system �����Ѳ�ǽ���ɤ��� */
static boolean cdrom_available;
static boolean midi_available;
static boolean audio_available;

static boolean mixer_available = FALSE;
static Mix_Music *mixer_music = NULL;
static int mixer_music_cdrom_track = -1;

static int mixer_music_stop() {
	
	if(mixer_music){
		Mix_HaltMusic();
		Mix_FreeMusic(mixer_music);
		mixer_music = NULL;
	}
	mixer_music_cdrom_track = -1;
	
	return OK;
}

int musclient_init() {
	void *data;
	int i;
	
	cdrom_available = FALSE;
	midi_available  = FALSE;
	audio_available = FALSE;

	if( SDL_InitSubSystem(SDL_INIT_AUDIO) < 0 ){
		__android_log_print(ANDROID_LOG_DEBUG, "cdrom", "failed : SDL_InitSubSystem(SDL_INIT_AUDIO)");
		return NG;		
	}

    if(Mix_OpenAudio(44100/*22050*/, AUDIO_S16, 2, 1024/*4096*/) < 0){
		__android_log_print(ANDROID_LOG_DEBUG, "cdrom", "failed : Mix_OpenAudio()");
        return NG;
    }
	
	Mix_AllocateChannels(3);
	
	mixer_available = TRUE;
	cdrom_available = TRUE;
	midi_available  = FALSE;
	audio_available = TRUE;

	return OK;
}

int musclient_exit() {
	if(mixer_available){
		mixer_music_stop();
		Mix_CloseAudio();
		mixer_available = FALSE;
	}
	return OK;
}

/*
 * cdrom �α��ճ��� 
 *   track: �ȥ�å��ֹ� (���ȥ�å��� 1)
 *   loop : �����֤���� (0�ξ���̵��)
 */
int mus_cdrom_start(int track, int loop) {
	int v[2];
	int fd;
	
	if (!cdrom_available) return NG;
	
	mus_cdrom_stop();

	char music_path[256];
	
	//MP3
	if(mixer_music == NULL){
		sprintf(music_path, "cd/track%02d.mp3", track);
		mixer_music = Mix_LoadMUS(music_path);
	}
	
	//OGG
	if(mixer_music == NULL){
		sprintf(music_path, "cd/track%02d.ogg", track);
		mixer_music = Mix_LoadMUS(music_path);
	}

	//WAV
	if(mixer_music == NULL){
		sprintf(music_path, "cd/track%02d.wav", track);
		mixer_music = Mix_LoadMUS(music_path);
	}
	
	if(mixer_music){
		//__android_log_print(ANDROID_LOG_DEBUG, "cdrom", "Mix_PlayMusic() : name=%s track=%d", music_path, track);
		mixer_music_cdrom_track = track;
		Mix_PlayMusic(mixer_music, (loop - 1));
		return OK;
	} else {
		//__android_log_print(ANDROID_LOG_DEBUG, "cdrom", "audio_music is NULL");
		return NG;
	}
	
	return OK;
}

/*
 * cdrom �α������
 */
int mus_cdrom_stop() {
	if (!cdrom_available) return NG;
	
	return mixer_music_stop();
}

/*
 * cdrom �α��վ��֤μ���
 *   info: ���ջ���(track/min/sec/frame)�ξ��֤��Ǽ������
 *         ��ߤ��Ƥ������ 999/999/999/999 ���֤�
 */
int mus_cdrom_get_playposition(cd_time *tm) {
	void *data;
	
	if (!cdrom_available) return NG;
	
	tm->t = (mixer_music_cdrom_track < 0 ? 999 : mixer_music_cdrom_track);
	tm->m = tm->s = tm->f = 999;
	
	return NG;

	return OK;
}

/*
 * cdrom �κ���ȥ�å����μ���
 *   
 */
int mus_cdrom_get_maxtrack() {
	int trk;
	
	if (!cdrom_available) return 0;
	
	trk = 999;
	
	return trk;
}

/*
 * CDROM ��ͭ��/̵�� �ե饰�μ���
 *   return: FALASE -> ̵��
 *           TRUE   -> ͭ��
 */
boolean mus_cdrom_get_state() {
	return cdrom_available;
}

/*
 * midi �α��ճ��� 
 *   no  : �ե������ֹ�( no >= 1)
 *   loop: �����֤���� (0�ξ���̵��)
 */
int mus_midi_start(int no, int loop) {
	dridata *dfile;

	if (!midi_available) return NG;
/*
	mus_midi_stop();
	dfile = ald_getdata(DRIFILE_MIDI, (no - 1));
	if(dfile == NULL) {
		return NG;
	}
	
	FILE* fp = fopen("tmp.mid", "wb");
	if(fp == NULL){
		ald_freedata(dfile);
		return NG;
	}
	fwrite(dfile->data, 1, dfile->size, fp);
	fclose(fp);
	
	ald_freedata(dfile);
	
	mixer_music = Mix_LoadMUS("tmp.mid");
	
	if(mixer_music == NULL){
		return NG;
	}
	
	Mix_PlayMusic(mixer_music, (loop - 1));
*/
	return OK;
}

/*
 * midi �α������
 */
int mus_midi_stop(void) {
	if (!midi_available) return NG;
	
	return mixer_music_stop();
}

/*
 * midi �ΰ�����
 */
int mus_midi_pause(void) {
	if (!midi_available) return NG;
/*	
	Mix_PauseMusic();
*/
	return OK;
}

/*
 * midi �ΰ����߲��
 */
int mus_midi_unpause(void) {
	if (!midi_available) return NG;
/*
	Mix_ResumeMusic();
*/
	return OK;
}

/*
 * midi �α��վ��֤μ���
 *  state: ���ջ��֤��ֹ�ξ��֤��Ǽ������
 *         ��ߤ��Ƥ������ 0 ������
 */
int mus_midi_get_playposition(midiplaystate *state) {
	if (!midi_available) return NG;

	return OK;
}

/*
 * midi �α��� flag/variable �ξ��֤����ꤹ��
 *   mode : 0 -> flag mode
 *          1 -> variable mode
 *   index: flag/variable �ֹ�
 *   val  : �񤭹�����
 */
int mus_midi_set_flag(int mode, int index, int val) {

	if (!midi_available) return NG;

	return OK;
}

/*
 * midi �α��� flag/variable �ξ��֤��������
 *   mode : 0 -> flag mode
 *          1 -> variable mode
 *   index: flag/variable �ֹ�
 *
 *   return : flag/variable ����
 */
int mus_midi_get_flag(int mode, int index) {

	int val = 0;

	if (!midi_available) return NG;

	return val;
}

/*
 * MIDI ��ͭ��/̵�� �ե饰�μ���
 *   return: FALASE -> ̵��
 *           TRUE   -> ͭ��
 */
boolean mus_midi_get_state() {
	return midi_available;
}

static dridata* mixer_pcm_dfiles[2] = {NULL, NULL};
static Mix_Chunk* mixer_pcm_chunks[2] = {NULL, NULL};

/*
 * WAV �α��ճ��� (command S?)
 *   no  : �ե������ֹ�( no >= 1)
 *   loop: �����֤���� (0�ξ���̵��)
 */
int mus_pcm_start(int no, int loop) {

	if (!audio_available) return NG;

	mus_pcm_stop(0);
	
	mixer_pcm_dfiles[0] = ald_getdata(DRIFILE_WAVE, (no - 1));
	if (mixer_pcm_dfiles[0] == NULL) {
		//WARNING("DRIFILE_WAVE fail to open %d\n", (no - 1));
		return NG;
	}
	
	mixer_pcm_chunks[0] = Mix_LoadWAV_RW(SDL_RWFromMem(mixer_pcm_dfiles[0]->data, mixer_pcm_dfiles[0]->size), 1);
	if(mixer_pcm_chunks[0] == NULL){
		ald_freedata(mixer_pcm_dfiles[0]);
		mixer_pcm_dfiles[0] = NULL;
		return NG;
	}
	
	Mix_SetPanning(0, 255, 255);
	Mix_PlayChannel(0, mixer_pcm_chunks[0], (loop - 1));

	return OK;
}

/*
 * WAV �򺸱� mix ���Ʊ���
 *   noL : ���ѤΥե������ֹ�(noL >= 1)
 *   noR : ���ѤΥե������ֹ�(noR >= 1)
 *   loop: �����֤���(0�ξ���̵�¥롼��)
 */
int mus_pcm_mix(int noL, int noR, int loop) {

	if (!audio_available) return NG;

	mus_pcm_stop(0);
	
	mixer_pcm_dfiles[0] = ald_getdata(DRIFILE_WAVE, (noL - 1));
	if (mixer_pcm_dfiles[0] == NULL) {
		//WARNING("DRIFILE_WAVE fail to open %d\n", (no - 1));
		return NG;
	}
	mixer_pcm_dfiles[1] = ald_getdata(DRIFILE_WAVE, (noR - 1));
	if (mixer_pcm_dfiles[1] == NULL) {
		//WARNING("DRIFILE_WAVE fail to open %d\n", (no - 1));
		ald_freedata(mixer_pcm_dfiles[0]);
		mixer_pcm_dfiles[0] = NULL;
		return NG;
	}
	
	mixer_pcm_chunks[0] = Mix_LoadWAV_RW(SDL_RWFromMem(mixer_pcm_dfiles[0]->data, mixer_pcm_dfiles[0]->size), 1);
	if(mixer_pcm_chunks[0] == NULL){
		ald_freedata(mixer_pcm_dfiles[1]);
		mixer_pcm_dfiles[1] = NULL;
		ald_freedata(mixer_pcm_dfiles[0]);
		mixer_pcm_dfiles[0] = NULL;
		return NG;
	}
	mixer_pcm_chunks[1] = Mix_LoadWAV_RW(SDL_RWFromMem(mixer_pcm_dfiles[1]->data, mixer_pcm_dfiles[1]->size), 1);
	if(mixer_pcm_chunks[1] == NULL){
		Mix_FreeChunk(mixer_pcm_chunks[0]);
		mixer_pcm_chunks[0] = NULL;
		ald_freedata(mixer_pcm_dfiles[1]);
		mixer_pcm_dfiles[1] = NULL;
		ald_freedata(mixer_pcm_dfiles[0]);
		mixer_pcm_dfiles[0] = NULL;
		return NG;
	}
	
	Mix_SetPanning(0, 255, 0);
	Mix_SetPanning(1, 0, 255);
	
	Mix_PlayChannel(0, mixer_pcm_chunks[0], (loop - 1));
	Mix_PlayChannel(1, mixer_pcm_chunks[1], (loop - 1));

	return OK;
}

/*
 * WAV �α������ (command S?)
 *   msec: �ߤޤ�ޤǤλ���(msec), 0�ξ��Ϥ����˻ߤޤ�
 */
int mus_pcm_stop(int msec) {

	if (!audio_available) return NG;
	
	int i;
	for(i=0; i<2; i++){
		if(mixer_pcm_chunks[i]){
			Mix_HaltChannel(i);
			Mix_FreeChunk(mixer_pcm_chunks[i]);
			mixer_pcm_chunks[i] = NULL;
		}
		if(mixer_pcm_dfiles[i]){
			ald_freedata(mixer_pcm_dfiles[i]);
			mixer_pcm_dfiles[i] = NULL;
		}
	}

	return OK;
}

/*
 * WAV �ե����������˺ܤ���
 *   no  : �ե������ֹ�( no >= 1)
 */
int mus_pcm_load(int no) {

	if (!audio_available) return NG;

	return OK;
}

/*
 * WAV �α��վ��֤μ���
 *   pos: ���ջ��֤��Ǽ������(msec)
 *        ��ߤ��Ƥ������ 0 ������
 *        loop���Ƥ�����Ϲ�׻���
 */
int mus_pcm_get_playposition(int *pos) {

	if (!audio_available) return NG;
	
	return OK;
}


/*
 * �ե����ɳ���
 *   device: �ե����ɤ���ǥХ���(MIX_MAXTER/MIX_PCM/....)
 *   time  : �ǽ��ܥ�塼��ޤǤ�ã�������(msec)
 *   volume: �ǽ��ܥ�塼��
 *   stop:   �ե����ɽ�λ���˱��դ򥹥ȥåפ��뤫�ɤ�����
 *           0: ���ʤ�
 *           1: ����
 */ 
int mus_mixer_fadeout_start(int device, int time, int volume, int stop) {

	if (!audio_available) return NG;
	
	return OK;
}

/*
 * ����ΥǥХ��������ߥե������椫�ɤ�����Ĵ�٤�
 *   device: ����ǥХ���
 *
 *   return: TRUE  -> �ե�������
 *           FALSE -> �ե�������Ǥʤ�
 */
boolean mus_mixer_fadeout_get_state(int device) {

	boolean bool = FALSE;

	return bool;
}

/*
 * ����ΥǥХ����Υե����ɤ�����ǻߤ��
 *   device: ����ǥХ���
 */
int mus_mixer_fadeout_stop(int device) {

	if (!audio_available) return NG;

	return OK;
}

/*
 * ����ΥǥХ����Υߥ�������٥���������
 *   device: ����ǥХ���
 *
 *   return: �ߥ�������٥�(0 - 100) (������������ꤵ�줿��)
 */
int mus_mixer_get_level(int device) {
	int lv = 0;
	
	if (!audio_available) return 0;

	return lv;
}

/*
 * ����Υե����ޥåȤǺ�����ǽ���ɤ���Ĵ�٤�
 *   bit : 8 or 16 bit
 *   rate: frequency
 *   ch  : Mono or Stereo
 *   able: ��ǽ���ɤ����ξ��֤���������
 */
int mus_pcm_check_ability(int bit, int rate, int ch, boolean *able) {
	if (!audio_available) {
		printf("not available\n");
		*able = FALSE;
		return NG;
	}
	
	*able = TRUE;
	return OK;
}

/*
 * ����Υ����ͥ�� wave file �����
 *   ch : channel (0-127)
 *   num: �ե������ֹ� (1-65535)
 */
int mus_wav_load(int ch, int num) {

	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	return OK;
}

/*
 * ����Υ����ͥ뤫�� wave file ���˴�
 *   ch : channel
 */
int mus_wav_unload(int ch) {
	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	return OK;
}

/*
 * WAV �α��ճ��� (wavXXXX)
 *   ch  : ������������ͥ� (0-127)
           (���餫���� mus_wav_load��load���Ƥ���)
 *   loop: �����֤����       (0�ξ���̵��, ����ʳ��ϣ���Τ�)
 */
int mus_wav_play(int ch, int loop) {

	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	return OK;
}

/*
 * ����Υ����ͥ��WAV�α������ (wavXXX)
 *   ch: channel
 */
int mus_wav_stop(int ch) {
	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	return OK;
}

/*
 * ����Υ����ͥ�α��վ��֤μ���
 *   ch: channel (0-127)
 *   
 *   return: ���ջ���(msec) 65535ms ��˰��
 */
int mus_wav_get_playposition(int ch) {

	int ret = 0;
	
	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	
	return ret;
}

 
/*
 * ����Υ����ͥ��WAV�Υե�����
 *   ch: channel(0-127)
 *   time  : �ǽ��ܥ�塼��ޤǤ�ã�������(msec)
 *   volume: �ǽ��ܥ�塼��
 *   stop  : �ե����ɽ�λ���˱��դ򥹥ȥåפ��뤫�ɤ�����
 *             0: ���ʤ�
 *             1: ����
 */
int mus_wav_fadeout_start(int ch, int time, int volume, int stop) {

	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;

	return OK;
}

/*
 * ����Υ����ͥ�Υե����ɤ�����ǻߤ��
 *   ch: channel (0-127)
 */
int mus_wav_fadeout_stop(int ch) {

	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	return OK;
}

/*
 * ����Υ����ͥ뤬���ߥե������椫�ɤ�����Ĵ�٤�
 *   ch: channel
 *
 *   return: TRUE  -> �ե�������
 *           FALSE -> �ե�������Ǥʤ�
 */
boolean mus_wav_fadeout_get_state(int ch) {

	boolean bool = FALSE;
	
	if (!audio_available) return FALSE;
	
	if (ch < 0 || ch > 128) return FALSE;
	
	
	return bool;
}


/*
 * ����Υ����ͥ�κ�������λ����ޤ��Ԥ�
 *   ch: channel (0-127)
 */
int mus_wav_waitend(int ch) {

	
	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;

	return OK;
}

/*
 * ����Υ����ͥ�ǻ����Ԥ�
 *     �������Ƥ��ʤ��ʤ餹������롣���ޥ�ɤ�ȯ�Ԥ��줿�ִ֤˱������
 *     ����С����դ����äƤ������ַв᤹��ޤ��Ԥġ�
 *   ch  : channel (0-127)
 *   time: �Ԥ�����(msec)
 */
int mus_wav_waittime(int ch, int time) {

	if (!audio_available) return NG;

	if (ch < 0 || ch > 128) return NG;
	

	return OK;
}

/*
 * PCM ��ͭ��/̵�� �ե饰�μ���
 *   return: FALASE -> ̵��
 *           TRUE   -> ͭ��
 */
boolean mus_pcm_get_state() {
	return audio_available;
}

/*
 * ����Υ����ͥ��WAV�ǡ����α��ջ��֤μ���
 *   ch: channel
 *   
 *   return: ����(msec) 65535ms ��˰��
 */
int mus_wav_wavtime(int ch) {

	int ret = 0;
	
	if (!audio_available) return 0;
	
	if (ch < 0 || ch > 128) return NG;

	if (ret > 65565) ret = 65535;
	return ret;

}

/*
 * ����� channel �� WAVFILE �򥻥å�
 *   ch:    channel
 *   wfile: WAVFILE
 */
int mus_wav_sendfile(int ch, WAVFILE *wfile) {

	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	return OK;
}

/*
 * ����Υ����ͥ�� wave file ��LRȿž���ƥ���
 *   ch : channel (0-127)
 *   num: �ե������ֹ� (1-65535)
 */
int mus_wav_load_lrsw(int ch, int num) {

	if (!audio_available) return NG;
	
	if (ch < 0 || ch > 128) return NG;
	
	return OK;
}

int mus_bgm_play(int no, int time, int vol) {

	if (!audio_available) return NG;
	
	return OK;
}

int mus_bgm_stop(int no, int time) {

	if (!audio_available) return NG;

	return OK;
}

int mus_bgm_fade(int no, int time, int vol) {

	if (!audio_available) return NG;

	return OK;
}

int mus_bgm_getpos(int no) {

	int ret = 0;
	
	if (!audio_available) return NG;
	
	if (ret > 65565) ret = 65535;
	
	return ret;
}

int mus_bgm_wait(int no, int timeout) {

	if (!audio_available) return NG;

	
	return OK;
}

int mus_bgm_waitpos(int no, int index) {

	if (!audio_available) return NG;

	return OK;
}

int mus_bgm_stopall(int time) {
	if (!audio_available) return NG;
	
	return OK;
}

int mus_bgm_getlength(int no) {

	int ret = 0;
	
	if (!audio_available) return NG;
	
	if (ret > 65565) ret = 65535;
	
	return ret;
}

int mus_vol_set_valance(int *vols, int num) {

	if (!audio_available) return NG;
	
	return OK;
}
