#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <json.h>
#include <mosquitto.h>
#include <sys/wait.h>

static int ALARM_SECONDS = 600;
// static int pfp[2];

static int mqtt_port=1883;
static char mqtt_host[256];
static char *mqtt_user_name,*mqtt_passwd;
static struct mosquitto *mosq;

static void _mosquitto_shutdown(void);



extern char *optarg;
extern int optind, opterr, optopt;

int outputDebug=0;
int milliseconds_timeout=500;
int alarmSeconds=5;

static char *incomingTopic;
static char *outgoingTopic;
static char *command;

typedef struct {
	char *label;
	void (*packet_processor_func)(int serialfd,char *special_handling );
	}	MODES;

uint64_t microtime() {
	struct timeval time;
	gettimeofday(&time, NULL); 
	return ((uint64_t)time.tv_sec * 1000000) + time.tv_usec;
}

int  RunFilter(void *d, char *cmd, void *packet,int packetLen) {
	char	buffer[1024];
	char	temp_name[64] = {};
	int	rd;
	FILE	*in,*out;

	umask(00);
	snprintf(temp_name,sizeof(temp_name),"/tmp/processMQTT_fifo_%d",getpid());
	snprintf(buffer,sizeof(buffer),"%s > %s",cmd,temp_name);
	out = popen(buffer,"w");
	if ( 0 == out ) {
		fprintf(stderr,"# unable to popen(%s,\"w\").  %s\n",temp_name,strerror(errno));
		return	1;
	}
	if ( packetLen != fwrite(packet,1,packetLen,out) ) {
		fprintf(stderr,"# unable to fwrite %s.  %s\n",temp_name,strerror(errno));
	return	1;
	}
	pclose(out);
	in = fopen(temp_name,"r");
	if ( 0 == in ) {
		fprintf(stderr,"# unable to fopen(%s,\"r\").  %s\n",temp_name,strerror(errno));
		return	1;
	}
	rd = fread(buffer,1,sizeof(buffer),in);
	if ( 0 >= rd ) {
		fprintf(stderr,"# unable to fread %s.  %s\n",temp_name,strerror(errno));
		return	1;
	}
	fclose(in);
	memcpy(d,buffer,rd);
	unlink(temp_name);
	return	0;
}


	
		



void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	if ( 5 == result ) {
		fprintf(stderr,"# --mqtt-user-name and --mqtt-passwd required at this site.\n");
	}
	printf("# connect_callback, rc=%d\n", result);
}

static void signal_handler(int signum) {


	if ( SIGALRM == signum ) {
		fprintf(stderr,"\n# Timeout while waiting for NMEA data.\n");
		fprintf(stderr,"# Terminating.\n");
		_mosquitto_shutdown();
		exit(100);
	} else if ( SIGPIPE == signum ) {
		fprintf(stderr,"\n# Broken pipe.\n");
		fprintf(stderr,"# Terminating.\n");
		_mosquitto_shutdown();
		exit(101);
	} else if ( SIGUSR1 == signum ) {
		/* clear signal */
		signal(SIGUSR1, SIG_IGN);

		fprintf(stderr,"# SIGUSR1 triggered data_block dump:\n");
		
		/* re-install alarm handler */
		signal(SIGUSR1, signal_handler);
	} else {
		fprintf(stderr,"\n# Caught unexpected signal %d.\n",signum);
		fprintf(stderr,"# Terminating.\n");
		_mosquitto_shutdown();
		exit(102);
	}

}
#include <sys/time.h>

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
	char	outBuffer[1024] = {};
	int rc = 0;

	static int messageID;

	/* cancel pending alarm */
	alarm(0);
	/* set an alarm to send a SIGALARM if data not received within alarmSeconds */
 	alarm(ALARM_SECONDS);


	if ( outputDebug )
		fprintf(stderr,"got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);

	/* write to the pipe the full packet */
	if ( 0 != RunFilter(outBuffer, command, message->payload,message->payloadlen) ) {
		fprintf(stderr,"# problem piping.  %s\n",strerror(errno));
		exit(1);
	}
	/* now publish the message to mqtt */
	// fprintf(stderr,"# publish packet '%s'\n",outBuffer);
	/* instance, message ID pointer, topic, data length, data, qos, retain */
	rc = mosquitto_publish(mosq, &messageID, outgoingTopic, strlen(outBuffer), outBuffer, 0, 0); 

	/*if (0 != outputDebug)*/ fprintf(stderr,"# mosquitto_publish provided messageID=%d and return code=%d\n",messageID,rc);

	/* check return status of mosquitto_publish */ 
	/* this really just checks if mosquitto library accepted the message. Not that it was actually send on the network */
	if ( MOSQ_ERR_SUCCESS == rc ) {
		/* successful send */
	} else if ( MOSQ_ERR_INVAL == rc ) {
		fprintf(stderr,"# mosquitto error invalid parameters\n");
	} else if ( MOSQ_ERR_NOMEM == rc ) {
		fprintf(stderr,"# mosquitto error out of memory\n");
	} else if ( MOSQ_ERR_NO_CONN == rc ) {
		fprintf(stderr,"# mosquitto error no connection\n");
	} else if ( MOSQ_ERR_PROTOCOL == rc ) {
		fprintf(stderr,"# mosquitto error protocol\n");
	} else if ( MOSQ_ERR_PAYLOAD_SIZE == rc ) {
		fprintf(stderr,"# mosquitto error payload too large\n");
	} else if ( MOSQ_ERR_MALFORMED_UTF8 == rc ) {
		fprintf(stderr,"# mosquitto error topic is not valid UTF-8\n");
	} else {
		fprintf(stderr,"# mosquitto unknown error = %d\n",rc);
	}


}



char	*strsave(char *s )
{
char	*ret_val = 0;

ret_val = malloc(strlen(s)+1);
if ( 0 != ret_val) strcpy(ret_val,s);
return ret_val;	
}
	
static int run = 1;
static int _mosquitto_startup(void) {
	char clientid[24];
	int rc = 0;


	fprintf(stderr,"# mqtt-send-example start-up\n");


	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mqtt-send-example_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		if ( 0 != mosq,mqtt_user_name && 0 != mqtt_passwd ) {
			mosquitto_username_pw_set(mosq,mqtt_user_name,mqtt_passwd);
		}
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
		mosquitto_subscribe(mosq, NULL, incomingTopic, 0);

		while (run) {
			rc = mosquitto_loop(mosq, -1, 1);

			if ( run && rc ) {
				printf("connection error!\n");
				sleep(10);
				mosquitto_reconnect(mosq);
			}
		}
		mosquitto_destroy(mosq);
	}

	fprintf(stderr,"# mosquitto_lib_cleanup()\n");
	mosquitto_lib_cleanup();

	return rc;
}

static void _mosquitto_shutdown(void) {

if ( mosq ) {
	
	/* disconnect mosquitto so we can be done */
	mosquitto_disconnect(mosq);
	/* stop mosquitto network handling loop */
	mosquitto_loop_stop(mosq,0);


	mosquitto_destroy(mosq);
	}

fprintf(stderr,"# mosquitto_lib_cleanup()\n");
mosquitto_lib_cleanup();
}
static void process_topics(char *s ) {
/*  s will have the format  incomingTopic:outgoingTopic:command   */
	char	buffer[512];
	char	*p,*q;

	strncpy(buffer,s,sizeof(buffer));
	q = buffer;
	p = strsep(&q,":");
	if ( 0 == p ) {
		fprintf(stderr,"# -t expects incomingTopic:outgoingTopic:command\n");
		exit (1);
	}
/* p now point to incomingTopic */

	incomingTopic = strsave(p);

	p = strsep(&q,":");
	if ( 0 == p ) {
		fprintf(stderr,"# -t expects incomingTopic:outgoingTopic:command\n");
		exit (1);
	}

	outgoingTopic = strsave(p);
	
	if ( 0 == q ) {
		fprintf(stderr,"# -t expects incomingTopic:outgoingTopic:command\n");
		exit (1);
	}

	command = strsave(q);

}

enum arguments {
	A_PLACEHOLDER = 512,
	A_mqtt_host,
	A_mqtt_topic,
	A_mqtt_port,
	A_mqtt_user_name,
	A_mqtt_password,
	A_verbose,
	A_help,
};
int main(int argc, char **argv) {
	int n;

	while (1) {
		// int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"mqtt-host",                        1,                 0, A_mqtt_host },
		        {"mqtt-topic",                       1,                 0, A_mqtt_topic },
		        {"mqtt-port",                        1,                 0, A_mqtt_port },
		        {"mqtt-user-name",                   1,                 0, A_mqtt_user_name },
		        {"mqtt-passwd",                      1,                 0, A_mqtt_password },
			{"verbose",                          no_argument,       0, A_verbose, },
		        {"help",                             no_argument,       0, A_help, },
			{},
		};

		n = getopt_long(argc, argv, "", long_options, &option_index);

		if (n == -1) {
			break;
		}
		

	/* command line arguments */
		switch (n) {
			case A_mqtt_topic:	
				process_topics(optarg);
				break;
			case A_mqtt_host:	
				strncpy(mqtt_host,optarg,sizeof(mqtt_host));
				break;
			case A_mqtt_port:
				mqtt_port = atoi(optarg);
				break;
			case A_mqtt_user_name:
				mqtt_user_name = strsave(optarg);
				break;
			case A_mqtt_password:
				mqtt_passwd = strsave(optarg);
				break;
			case A_verbose:
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
			case A_help:
				fprintf(stdout,"# --mqtt-topic\t\t\tmqtt topic\n");
				fprintf(stdout,"# --mqtt-host\t\t\tmqtt host\n");
				fprintf(stdout,"# --mqtt-port\t\t\tmqtt port(optional)\n");
				fprintf(stdout,"# --mqtt-user-name\t\tmaybe required depending on system\n");
				fprintf(stdout,"# --mqtt-passwd\t\t\tmaybe required depending on system\n");
				fprintf(stdout,"# --verbose\t\t\tOutput verbose / debugging to stderr\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# --help\t\t\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				exit(0);
		}
	}
	if ( ' ' >= mqtt_host[0] ) { fputs("# <--mqtt-host mqtt_host>	\n",stderr); return(1); } else fprintf(stderr,"# mqtt_host=%s\n",mqtt_host);

	if ( 0 == incomingTopic || 0 == outgoingTopic || 0 == command ) {
		fprintf(stderr,"# --mqtt-topic is missing and is required.\n");
		fprintf(stderr,"# -t expects incomingTopic:outgoingTopic:command\n");
		return	1;
	}
	/* install signal handler */
	signal(SIGALRM, signal_handler); /* timeout */
	signal(SIGUSR1, signal_handler); /* user signal to do data block debug dump */
	signal(SIGPIPE, signal_handler); /* broken TCP connection */

	if ( 0 != _mosquitto_startup() )
		return	1;

	return(0);
}
