/* xjadeo -  lash interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 *
 * (c) 2006 
 *  Robin Gareus <robin@gareus.org>
 *  Luis Garrido <luisgarrido@users.sourceforge.net>
 *
 */

#include "xjadeo.h"

#ifdef HAVE_LASH

//------------------------------------------------
// extern Globals (main.c)
//------------------------------------------------
extern int       loop_flag;
/* Option flags and variables */
extern char *current_file;
extern char *smpte_offset;
extern long	ts_offset;
extern long	userFrame;
extern int want_quiet;
extern int want_debug;
extern int want_verbose;
extern int want_letterbox;
extern int remote_en;
extern int remote_mode;

#ifdef HAVE_MIDI
extern char midiid[32];
extern int midi_clkconvert;	/* --midifps [0:MTC|1:VIDEO|2:RESAMPLE] */
extern int midi_clkadj;	
#endif

extern double 		delay;
extern double 		filefps;
extern int		videomode;
extern int 		seekflags;

extern double		framerate;
extern double		duration;
extern long		frames;

// On screen display
extern char		OSD_fontfile[1024]; 
extern char		OSD_text[128];
extern int		OSD_mode;

extern int OSD_fx, OSD_tx, OSD_sx, OSD_fy, OSD_sy, OSD_ty;

#if (HAVE_LIBXV || HAVE_IMLIB || HAVE_IMLIB2)
 extern int xj_ontop;
 extern int xj_fullscreen;
#else 
# define xj_ontop (0)
# define xj_fullscreen (0)
#endif

// defined in main.c
extern lash_client_t *lash_client;

void xapi_open(void *d);
void xapi_close(void *d);

long int lash_config_get_value_long (const lash_config_t * config) {
	const void *data = lash_config_get_value(config);
	return(*((long*) data));
}

/*************************
 * private lash functions
 */
void handle_event(lash_event_t* ev) {
	int type = lash_event_get_type(ev);
	const char*	str = lash_event_get_string(ev);
	
	
	if (type == LASH_Restore_Data_Set) {
		// FIXME - send this AFTER recv. config
		if (!want_quiet)
			printf("LASH restore data set\n");
		//lash_send_event(lash_client, lash_event_new_with_type(LASH_Restore_Data_Set));
	} else if (type == LASH_Save_Data_Set) {
		if (!want_quiet)
			printf("LASH saving data set\n");
		unsigned int w,h;
		int x,y;
		Xgetsize(&w,&h); 
		lcs_int("window_size",w<<16|h);
		Xgetpos(&x,&y); 
		// lcs_int("window_position",x<<16|y);
		lcs_int("x11_ontop",xj_ontop);
		lcs_int("x11_fullscreen",xj_fullscreen); 
		lcs_int("want_letterbox",want_letterbox); 
#ifdef HAVE_MIDI
		if (midi_connected())  lcs_int("syncsource",2);
		else
#endif
		if (jack_connected())  lcs_int("syncsource",1);
		else lcs_int("syncsource",0);

		lcs_str("current_file",current_file?current_file:"");
		lcs_int("seekflags",seekflags);
		lcs_long("ts_offset",ts_offset);
		lcs_str("smpte_offset",smpte_offset?smpte_offset:"");
		lcs_long("userFrame",userFrame);
		lcs_dbl("update_fps",delay);
		lcs_dbl("file_fps",filefps);
		lcs_int("OSD_mode",OSD_mode);
	//	lcs_int("OSD_sx",OSD_sx);
		lcs_int("OSD_sy",OSD_sy);
		lcs_int("OSD_mode",OSD_mode);
	//	lcs_int("OSD_fx",OSD_fx);
		lcs_int("OSD_fy",OSD_fy);
		lcs_int("OSD_mode",OSD_mode);
		lcs_int("OSD_mode",OSD_mode);
		lcs_str("OSD_text",OSD_text?OSD_text:"");
		lcs_str("OSD_fontfile",OSD_fontfile);
		lcs_int("OSD_mode",OSD_mode);
		lcs_int("OSD_mode",OSD_mode);
		lcs_int("OSD_tx",OSD_tx);
		lcs_int("OSD_ty",OSD_ty);
	#ifdef HAVE_MIDI
		lcs_int("MIDI_clkconvert",midi_clkconvert);
		lcs_int("MIDI_clkadj",midi_clkadj);
		lcs_str("MIDI_ID",(midiid && midi_connected())?midiid:"-2");
	#endif
		lash_send_event(lash_client, lash_event_new_with_type(LASH_Save_Data_Set));
	} else if (type == LASH_Quit) {
		loop_flag=0;
	} else 
		if (want_debug)
			printf ("WARNING: unhandled LASH Event t:%i s:'%s'\n",type,str);
}

void handle_config(lash_config_t* conf) {
	const char*    key      = NULL;
	key      = lash_config_get_key(conf);

	if (!strcmp(key,"current_file")) {
		const char *mfile = lash_config_get_value_string (conf);
		//printf("LASH config: open movie: %s\n",mfile);
		if (strlen(mfile))
			xapi_open((char *) lash_config_get_value_string (conf));
		else 
			xapi_close(NULL);
	} else if (!strcmp(key,"seekflags")) {
		seekflags =  lash_config_get_value_int(conf);
	} else if (!strcmp(key,"update_fps")) {
		delay =  lash_config_get_value_double(conf);
	} else if (!strcmp(key,"userFrame")) {
		userFrame= lash_config_get_value_long(conf);
	} else if (!strcmp(key,"ts_offset")) {
		ts_offset= lash_config_get_value_long(conf);
	} else if (!strcmp(key,"smpte_offset")) {
		const char *moff = lash_config_get_value_string (conf);
		if (smpte_offset) free(smpte_offset); smpte_offset=NULL;
		if (strlen(moff))
			smpte_offset=strdup(moff);
//	} else if (!strcmp(key,"framerate")) {
//		framerate =  lash_config_get_value_double(conf); 
	} else if (!strcmp(key,"file_fps")) {
		filefps =  lash_config_get_value_double(conf);
		if (filefps > 0) {
			framerate =  filefps;
			frames = (long) (framerate * duration); ///< TODO: check if we want that 
		}

// remote_en needs to be set on startup! - TODO
// or we'd need to call remote_open here...
//	} else if (!strcmp(key,"remote_en")) {
//#if HAVE_MQ
//		remote_en=1;
//#endif
//	notify frame??  No.
//		remote_mode = lash_config_get_value_int(conf);
//
			/* OSD -settings */
	} else if (!strcmp(key,"OSD_text")) {
		strncpy(OSD_text,lash_config_get_value_string (conf),127);
		OSD_text[127]=0;
	} else if (!strcmp(key,"OSD_fontfile")) {
		strncpy(OSD_fontfile,lash_config_get_value_string (conf),1024);
		OSD_fontfile[1023]=0;
	} else if (!strcmp(key,"OSD_mode")) {
		OSD_mode = lash_config_get_value_int(conf);
	} else if (!strcmp(key,"OSD_fx")) { OSD_fx = lash_config_get_value_int(conf);
	} else if (!strcmp(key,"OSD_tx")) { OSD_tx = lash_config_get_value_int(conf);
	} else if (!strcmp(key,"OSD_sx")) { OSD_sx = lash_config_get_value_int(conf);
	} else if (!strcmp(key,"OSD_fy")) { OSD_fy = lash_config_get_value_int(conf);
	} else if (!strcmp(key,"OSD_ty")) { OSD_ty = lash_config_get_value_int(conf);
	} else if (!strcmp(key,"OSD_sy")) { OSD_sy = lash_config_get_value_int(conf);
			/* jack/midi/offline */
	} else if (!strcmp(key,"syncsource")) {
		switch(lash_config_get_value_int(conf)) {
			case 1: 
			printf("LASH: setting sync source to JACK\n");
				open_jack();
	#ifdef HAVE_MIDI
				midi_close();
	#endif
				break;
			case 2: 
			printf("LASH: setting sync source to midi\n");
				close_jack();
				//we'll connect to MIDI later when we know the port/channel
				break;
			default:
			printf("LASH: setting no sync source. (manual seek)\n");
				close_jack();
	#ifdef HAVE_MIDI
				midi_close();
	#endif
		}
			/* MIDI */
	} else if (!strcmp(key,"MIDI_clkadj")) {
	#ifdef HAVE_MIDI
		midi_clkadj = lash_config_get_value_int(conf);
	#endif
	} else if (!strcmp(key,"MIDI_clkconvert")) {
	#ifdef HAVE_MIDI
		midi_clkconvert = lash_config_get_value_int(conf);
	#endif
	} else if (!strcmp(key,"MIDI_ID")) {
	#ifdef HAVE_MIDI
		// TODO: check if we got the same midi library (alsa,portmidi) as the Lash session.
		// can we use LASH to remember MIDI connections ?!?
		strncpy(midiid,lash_config_get_value_string (conf),32);
		midiid[31]=0;
		if (strlen(midiid) > 0) 
			if (atoi(midiid)>-2) midi_open(midiid);
	#endif
			/* Window Settings  */
	} else if (!strcmp(key,"want_letterbox")) {
		unsigned int x,y;
		want_letterbox= lash_config_get_value_long(conf);
		Xgetsize(&x,&y); 
		Xresize(x,y);
	} else if (!strcmp(key,"window_size")) {
	//	printf("LASH config: window size %ix%i\n", (lash_config_get_value_int(conf)>>16)&0xffff,lash_config_get_value_int(conf)&0xffff);
		Xresize((lash_config_get_value_int(conf)>>16)&0xffff,lash_config_get_value_int(conf)&0xffff);
	} else if (!strcmp(key,"window_position")) {
		Xposition((lash_config_get_value_int(conf)>>16)&0xffff,lash_config_get_value_int(conf)&0xffff);
	} else if (!strcmp(key,"x11_ontop")) {
		Xontop(lash_config_get_value_int(conf));
	} else if (!strcmp(key,"x11_fullscreen")) {
		Xfullscreen(lash_config_get_value_int(conf));
	} else {
		unsigned long val_size = lash_config_get_value_size(conf);
	//	if (want_debug)
		printf ("WARNING: unhandled LASH Config.  Key = %s size: %ld\n",key,val_size);
	}
}


/*************************
 *  public lash functions
 */

void lash_setup() {
	lash_event_t *event;
	//lash_config_t *lc;
	event = lash_event_new_with_type(LASH_Client_Name);
	lash_event_set_string(event, "Xjadeo");
	lash_send_event(lash_client, event);
/*
	lc = lash_config_new_with_key("current_file");
	lash_config_set_value_string (lc, current_file);
	lash_send_config(lash_client, lc);

	lc = lash_config_new_with_key("ts_offset");
	lash_config_set_value_int (lc, ts_offset);
	lash_send_config(lash_client, lc);

	lc = lash_config_new_with_key("fps");
	lash_config_set_value_double (lc, filefps);
	lash_send_config(lash_client, lc);
*/
}
#endif

void lash_process() {
#ifdef HAVE_LASH
	lash_event_t*  ev = NULL;
	lash_config_t* conf = NULL;
	while ((ev = lash_get_event(lash_client)) != NULL) {
		handle_event(ev);
		lash_event_destroy(ev);
	}
	while ((conf = lash_get_config(lash_client)) != NULL) {
		handle_config(conf);
		lash_config_destroy(conf);
	}
#endif
}

void lcs_str(char *key, char *value) {
#ifdef HAVE_LASH
	lash_config_t *lc = lash_config_new_with_key(key);
	lash_config_set_value_string (lc, value);
	lash_send_config(lash_client, lc);
	//printf("DEBUG - LASH config str: %s -> %i\n",key,value);
#endif
}

void lcs_long(char *key, long int value) {
#ifdef HAVE_LASH
	lash_config_t *lc;
	lc = lash_config_new_with_key(key);
	lash_config_set_value (lc, (void*) &value, sizeof(long int));
	lash_send_config(lash_client, lc);
	//printf("DEBUG - LASH config long %ld -> %i\n",key,value);
#endif
}
void lcs_int(char *key, int value) {
#ifdef HAVE_LASH
	lash_config_t *lc;
	lc = lash_config_new_with_key(key);
	lash_config_set_value_int (lc, value);
	lash_send_config(lash_client, lc);
	//printf("DEBUG - LASH config int: %i -> %i\n",key,value);
#endif
}

void lcs_dbl(char *key, double value) {
#ifdef HAVE_LASH
	lash_config_t *lc;
	lc = lash_config_new_with_key(key);
	lash_config_set_value_double (lc, value);
	lash_send_config(lash_client, lc);
	//printf("DEBUG - LASH config dbl %g -> %i\n",key,value);
#endif
}

