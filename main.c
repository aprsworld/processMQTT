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
#include <string.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <json.h>
#include <mosquitto.h>

static int mqtt_port=1883;
static char mqtt_host[256];
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

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
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
	char data[4];

	/* cancel pending alarm */
	alarm(0);
	/* set an alarm to send a SIGALARM if data not received within alarmSeconds */
 	alarm(ALARM_SECONDS);


	if ( outputDebug )
		fprintf(stderr,"got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);

	/* write to the pipe the full packet */
}



char	*strsave(char *s )
{
char	*ret_val = 0;

ret_val = malloc(strlen(s)+1);
if ( 0 != ret_val) strcpy(ret_val,s);
return ret_val;	
}
	
static struct mosquitto *_mosquitto_startup(void) {
	char clientid[24];
	int rc = 0;


	fprintf(stderr,"# mqtt-send-example start-up\n");


	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mqtt-send-example_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
		mosquitto_subscribe(mosq, NULL, incomingTopic, 0);
		// if ( 0 != rc )	what do I do?

		/* start mosquitto network handling loop */
		mosquitto_loop_start(mosq);
		}

return	mosq;
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
#if 0
static int serToMQTT_pub(const char *message ) {
	int rc = 0;

	static int messageID;
	/* instance, message ID pointer, topic, data length, data, qos, retain */
	rc = mosquitto_publish(mosq, &messageID, mqtt_topic, strlen(message), message, 0, 0); 

	if (0 != outputDebug) fprintf(stderr,"# mosquitto_publish provided messageID=%d and return code=%d\n",messageID,rc);

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


return	rc;
}
#endif
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

int main(int argc, char **argv) {
	int n;

	/* command line arguments */
	while ((n = getopt (argc, argv, "t:H:p:vnh")) != -1) {
		switch (n) {
			case 't':
				process_topics(optarg);
				break;
			case 'H':	
				strncpy(mqtt_host,optarg,sizeof(mqtt_host));
				break;
			case 'p':
				mqtt_port = atoi(optarg);
				break;
			case 'v':
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
			case 'h':
				fprintf(stdout,"# -h\t\tmqtt host\n");
				fprintf(stdout,"# -p\t\tmqtt port\n");
				fprintf(stdout,"# -v\t\tOutput verbose / debugging to stderr\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# -h\t\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				return(0);
		}
	}
	if ( ' ' >= mqtt_host[0] ) { fputs("# <-H mqtt_host>	\n",stderr); return(1); } else fprintf(stderr,"# mqtt_host=%s\n",mqtt_host);

	if ( 0 == incomingTopic || 0 == outgoingTopic || 0 == command ) {
		fprintf(stderr,"# -t is missing and is required.\n");
		fprintf(stderr,"# -t expects incomingTopic:outgoingTopic:command\n");
		return	1;
	}

	if ( 0 == _mosquitto_startup() )
		return	1;


	/* install signal handler */
	signal(SIGALRM, signal_handler); /* timeout */
	signal(SIGUSR1, signal_handler); /* user signal to do data block debug dump */
	signal(SIGPIPE, signal_handler); /* broken TCP connection */

	_mosquitto_shutdown();

	return(0);
}
