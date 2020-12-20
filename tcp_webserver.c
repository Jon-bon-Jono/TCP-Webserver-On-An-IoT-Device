//By Jonathan Williams, October 2020
/*TCP server to control the sensor-tag's LED, buzzer, pressure sensor,
  and humidity sensor.*/
#include "contiki-net.h"
#include "sys/cc.h"
#include "sys/ctimer.h"
#include "dev/leds.h"
#include "buzzer.h"
#include "board-peripherals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool green_blink = false;
bool red_blink = false;
bool all_blink = false;
bool buzzer_on = false;


#define SERVER_PORT 80
static struct tcp_socket socket;
#define INPUTBUFSIZE 400
static uint8_t inputbuf[INPUTBUFSIZE];
#define OUTPUTBUFSIZE 400
static uint8_t outputbuf[OUTPUTBUFSIZE];

unsigned int sensor_iterations = -1;
static struct ctimer sensor_timer;
static void sensor_reading(void *sensor);

PROCESS(tcp_server_process, "TCP echo process");
AUTOSTART_PROCESSES(&tcp_server_process);
//tcp_socket_close(&socket)
/*---------------------------------------------------------------------------*/
//Input data handler
static int input(struct tcp_socket *s, void *ptr, const uint8_t *inputptr, int inputdatalen) {    
	//printf("input %d bytes '%s'\n\r", inputdatalen, inputptr);
		
	char http_req[INPUTBUFSIZE];
	char buf[256];
	unsigned long t;
	strcpy(http_req, (char*)inputptr);
	//extract request string	
	strtok(http_req, " ");
	char* request = strtok(NULL, " ");
	//process request
	char* arg = strtok(request, "/");	
	if(strcmp(arg, "leds")==0){
		arg = strtok(NULL, "/");
		if(strcmp(arg, "r")==0){
			arg = strtok(NULL, "/");
		    if(strcmp(arg, "1")==0){
				if(red_blink){
					tcp_socket_send_str(&socket, "Red LED off\n");
					red_blink = false;
				}else{
					tcp_socket_send_str(&socket, "Red LED on\n");
					red_blink = true;
					
				}
				leds_toggle(LEDS_RED);	
				t = clock_seconds();
				sprintf(buf, "Time: %lu seconds\n",t);			
				tcp_socket_send_str(&socket, buf);
				tcp_socket_send_str(&socket, "NodeId: aaaa::212:4b00:1205:1463");
				tcp_socket_close(&socket);		
			}
		}else if(strcmp(arg, "g")==0){
			arg = strtok(NULL, "/");			
			if(strcmp(arg, "1")==0){
				if(green_blink){
					tcp_socket_send_str(&socket, "Green LED off\n");
					green_blink = false;
				}else{
					tcp_socket_send_str(&socket, "Green LED on\n");
					green_blink = true;
					
				}
				leds_toggle(LEDS_GREEN);	
				t = clock_seconds();
				sprintf(buf, "Time: %lu seconds\n",t);			
				tcp_socket_send_str(&socket, buf);
				tcp_socket_send_str(&socket, "NodeId: aaaa::212:4b00:1205:1463");
				tcp_socket_close(&socket);		
			}
		}else if(strcmp(arg, "a")==0){
			arg = strtok(NULL, "/");			
			if(strcmp(arg, "1")==0){
				if(all_blink){
					tcp_socket_send_str(&socket, "All LEDs off\n");
					all_blink = false;
				}else{
					tcp_socket_send_str(&socket, "All LEDs on\n");
					all_blink = true;
					
				}
				leds_toggle(LEDS_ALL);	
				t = clock_seconds();
				sprintf(buf, "Time: %lu seconds\n",t);			
				tcp_socket_send_str(&socket, buf);
				tcp_socket_send_str(&socket, "NodeId: aaaa::212:4b00:1205:1463");
				tcp_socket_close(&socket);		
			}
		}else{
			//tcp_socket_send_str(&socket, "Invalid led option\n");
		}
		tcp_socket_close(&socket);
	}else if(strcmp(arg, "buzzer")==0){
		arg = strtok(NULL, "/");
		int buz_freq = atoi(arg);
		if(buzzer_on){
			tcp_socket_send_str(&socket, "Stop buzzer\n");			
			buzzer_stop();
			buzzer_on = false;
		}else{
			sprintf(buf, "Start buzzer at %s Hz\n",arg);		
			tcp_socket_send_str(&socket, buf);			
			buzzer_start(buz_freq);			
			buzzer_on = true;
		}
		t = clock_seconds();
		sprintf(buf, "Time: %lu seconds\n",t);			
		tcp_socket_send_str(&socket, buf);
		tcp_socket_send_str(&socket, "NodeId: aaaa::212:4b00:1205:1463");
		tcp_socket_close(&socket);	
	}else if(strcmp(arg, "pressure")==0){
		arg = strtok(NULL, "/");
		//sprintf(buf, "Take next %s pressure readings\n",arg);		
		//tcp_socket_send_str(&socket, buf);
		//strcpy(outputbuf, "");
		sensor_iterations = atoi(arg);
		SENSORS_ACTIVATE(bmp_280_sensor);
		ctimer_set(&sensor_timer,CLOCK_SECOND,sensor_reading,"p");
	}else if(strcmp(arg, "humidity")==0){
		arg = strtok(NULL, "/");
		//sprintf(buf, "Take next %s humidity readings\n",arg);		
		//tcp_socket_send_str(&socket, buf);
		//strcpy(outputbuf, "");
		sensor_iterations = atoi(arg);
		SENSORS_ACTIVATE(hdc_1000_sensor);
		ctimer_set(&sensor_timer,CLOCK_SECOND,sensor_reading,"h");
	}else{
		//tcp_socket_send_str(&socket, "Error\n");
	}
	//tcp_socket_send_str(&socket, inputptr);	//Reflect byte
	//tcp_socket_close(&socket);
	//Clear buffer
	memset(inputptr, 0, inputdatalen);
    return 0; // all data consumed 
}

/*---------------------------------------------------------------------------*/
//Event handler
static void event(struct tcp_socket *s, void *ptr, tcp_socket_event_t ev) {
  //printf("event %d\n", ev);
}

/*---------------------------------------------------------------------------*/
//TCP Server process
PROCESS_THREAD(tcp_server_process, ev, data) {

  	PROCESS_BEGIN();

	//Register TCP socket
  	tcp_socket_register(&socket, NULL,
               inputbuf, sizeof(inputbuf),
               outputbuf, sizeof(outputbuf),
               input, event);
  	tcp_socket_listen(&socket, SERVER_PORT);

	printf("Listening on %d\n", SERVER_PORT);
	
	while(1) {
		//Wait for event to occur
		PROCESS_PAUSE();
	}
	PROCESS_END();
}


static void sensor_reading(void *sensor) {
	int val;	
	unsigned long t;
	char buf[56];
	if(strcmp((char*)sensor, "h") == 0 && sensor_iterations > 0){
		sensor_iterations--;
		val = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMIDITY);
		sprintf(buf,"Humidity=%d.%02d %%RH ::: ",val/100,val%100);		
		//strcat(outputbuf, buf);
		tcp_socket_send_str(&socket, buf);
		t = clock_seconds();
		sprintf(buf, "Time: %lu seconds ::: ",t);			
		tcp_socket_send_str(&socket, buf);
		tcp_socket_send_str(&socket, "NodeId: aaaa::212:4b00:1205:1463\n");
		if (sensor_iterations == 0){
			//tcp_socket_send_str(&socket, outputbuf);
			tcp_socket_close(&socket);
			//memset(outputbuf, 0, OUTPUTBUFSIZE);
		}
		SENSORS_ACTIVATE(hdc_1000_sensor);
		ctimer_reset(&sensor_timer);	
	}else if(strcmp((char*)sensor, "p") == 0 && sensor_iterations > 0){
		sensor_iterations--;		
		val = bmp_280_sensor.value(BMP_280_SENSOR_TYPE_TEMP);
		sprintf(buf, "Pressure=%d.%02d %%Pa ::: ",val/100,val%100);			
		//strcat(outputbuf, buf);
		tcp_socket_send_str(&socket, buf);
		t = clock_seconds();
		sprintf(buf, "Time: %lu seconds ::: ",t);			
		tcp_socket_send_str(&socket, buf);
		tcp_socket_send_str(&socket, "NodeId: aaaa::212:4b00:1205:1463\n");
		if (sensor_iterations == 0){
			//tcp_socket_send_str(&socket, outputbuf);
			tcp_socket_close(&socket);
			//memset(outputbuf, 0, OUTPUTBUFSIZE);
		}
		SENSORS_ACTIVATE(bmp_280_sensor);
		ctimer_reset(&sensor_timer);
	}else{		
		return 0;
	}
}
