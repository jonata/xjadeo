/*  xj-five - message queues

    Copyright (C) 2006 Robin Gareus
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: xj-five.c,v 1.12 2006/09/09 10:55:12 rgareus Exp $
*/
#include "xjadeo.h"

#ifdef HAVE_MQ

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>


#include <mqueue.h>
#include <errno.h>

extern int errno;
extern int loop_run; // display active
extern int loop_flag;// program active
extern int want_quiet;
extern int want_verbose;


/* see xjremote.c for information on this one */
typedef struct {
	int cmd;
	char m[MQLEN];
} mqmsg;

/* some static globals */
mqd_t           mqfd_r;
mqd_t           mqfd_s;
size_t		mq_msgsize_r;
char		*msg_buffer;
int 		priority_of_msg = 20;


void  mymq_init(char *id) {
	struct	 	mq_attr  mqat;
	// TODO use session ID in path.
	// implement session authenticaion? - allow user to specify umask.
	char qname[64];
	snprintf(qname,64,"/xjadeo-request%s%s", id?"-":"", id?(char*)id:"");

	mqfd_r = mq_open(qname, O_RDONLY | O_CREAT| O_NONBLOCK, S_IRWXU , NULL);
	if (mqfd_r == -1) {
		perror("mq_open failure:");
		return;
	}
	if (mq_getattr(mqfd_r, &mqat) == -1) {
		perror("mq_getattr error:");
		return;
	}
	mq_msgsize_r = mqat.mq_msgsize;
	msg_buffer = malloc(mq_msgsize_r);

	snprintf(qname,64,"/xjadeo-reply%s%s", id?"-":"", id?(char*)id:"");

	mqfd_s = mq_open(qname, O_WRONLY | O_CREAT| O_NONBLOCK, S_IRWXU , NULL);
	if (mqfd_s == -1) {
		perror("mq_open failure:");
		mq_close(mqfd_r);
		snprintf(qname,64,"/xjadeo-request%s%s", id?"-":"", id?(char*)id:"");
		mq_unlink(qname);
		return;
	}

	while (mq_receive(mqfd_r, msg_buffer, mq_msgsize_r, 0) > 0) ;

#if 0 
	{	// FLUSH the output Queue
		mqd_t mqfd_sx = mq_open(qname, O_RDONLY | O_NONBLOCK, S_IRWXU , NULL);
		while (mq_receive(mqfd_sx, msg_buffer, mq_msgsize_r, 0) > 0) ;
		mq_close(mqfd_sx);
	}
#endif

	if (!want_quiet)
		printf("activated remote interface. mqID:%s\n",id?id:"[default]");

}

void mymq_close(void) {
	if (mq_close(mqfd_r) == -1)
		perror("mq_close failure on mqfd_r");

	if (mq_close(mqfd_s) == -1)
		perror("mq_close failure on mqfd_s");

	//FIXME : use mqID
	//	snprintf(qname,64,"/xjadeo-request%s%s", id?"-":"", id?(char*)id:"");
	if (mq_unlink("/xjadeo-request") == -1)
		perror("mq_unlink failure");

	if (mq_unlink("/xjadeo-reply") == -1)
		perror("mq_unlink failure");

	if (!want_quiet)
		printf("closed MQ remote control.\n");
}

/* read message from queue and store it in data.
 * data needs to be 'NULL' (ignore msg) or point to a valid char[MQLEN].
 * return values  0: no message in queue
 *               -1: error
 *               >0: data bytes in message.
 */
int mymq_read(char *data) {
	mqmsg *mymsg;
	int num_bytes_received = mq_receive(mqfd_r, msg_buffer, mq_msgsize_r, 0);
	if (num_bytes_received == -1 && errno==EAGAIN) return (0);
	if (num_bytes_received == -1) {
		perror("mq_receive failure on mqfd");
		usleep(40000);
		return (-1);
	}
	if (num_bytes_received != sizeof(mqmsg) )  {
		fprintf(stderr,"MQ: received garbage message\n");
		return (-1);
	}

	mymsg = (mqmsg*) &msg_buffer[0];
	if (data) strncpy(data,mymsg->m,MQLEN);
	data[MQLEN-1]=0;

	return (strlen(data));
}

void mymq_reply(int rv, char *str) {
	int retry=10;
	static int retry_warn=1;
	mqmsg myrpy = {1, "" };
	myrpy.cmd= rv;
	snprintf(myrpy.m,MQLEN,"%s\n",str);
	// until we implement threads we should not waste time trying to re-send..
	// mq_timedsend() might be an alternative..
	while (retry > 0) {
		int rv=mq_send(mqfd_s, (char*) &myrpy, sizeof(mqmsg), priority_of_msg);
		if(!rv) break;
		retry--;
		usleep(1);
	}
	if (retry==0 && retry_warn) { fprintf(stderr,"DROPPED REMOTE MESSAGE\n"); retry_warn=0; }
}

#endif /* HAVE_MQ */