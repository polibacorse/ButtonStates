#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>
#include <string.h>
#include <mosquitto.h>
#include <json-c/json.h>
#include <stdbool.h>

//interruttori
const int accelerationButton = 11;
const int debugButton = 13;
const int datalogButton = 15;
const int telemetryButton = 4;
const int lapEndButton = 26;
const int N_GR = 9;
const int SH_LT = 8;

uint8_t acceleration = 0;
uint8_t debug = 0;
uint8_t datalog = 0;
uint8_t telemetry = 0;

const int CD_B = 25;
const int CD_C = 21;
const int CD_D = 23;
const int CD_A = 24;
const int LE = 22;

uint8_t Rpm = 0;
uint32_t lap_time = 0; //millisecondi
uint32_t time0_75m = 0;
uint32_t time50_75m = 0;
uint32_t ultimaCambiataTime = 0;
uint32_t accStartTime;
bool aggiorna0_100time = false;
uint8_t lapNumber = 0;
float Speed = 0;
uint8_t gear = 0;
const char* host= "localhost";
const int port = 1883;
const char* GEAR = "$SYS/formatted/GEAR";
const char* rpm = "$SYS/formatted/rpm";
const char* VhSpeed = "$SYS/formatted/VhSpeed";
struct mosquitto *mosq = NULL;
char *topic = NULL;

struct can_frame {
	unsigned short int id;
	unsigned int time;
	char data[8];
};

struct can_frame frame750;
struct can_frame frame751;

void send_Frame(struct can_frame frame)
{
	char json[100]; // da fare l'alloc della memoria ad hoc
	sprintf(json, "{\"id\":%u,\"time\":%u,\"data\":[", frame.id, frame.time);
	char tmp[10];
	int i = 0;
	for (i = 0; i < 8; i++) {
		sprintf(tmp, "0x%02hhX,", frame.data[i]);
		strcat(json, tmp);
	}

	strcat(json, "]}");
 	// printf("SEND: %s\n", json); // PER DEBUG
	mosquitto_publish(mosq, NULL, "$SYS/raw", 100, &json, 0, false);
}

void accelerationButtonValueChanged()
{
	delayMicroseconds(50000);
	uint32_t timer = lap_time - (uint32_t)millis(); //dubbio
	acceleration = !(bool)digitalRead(accelerationButton);
	debug = !(bool)digitalRead(debugButton);
	datalog = !(bool)digitalRead(datalogButton);
	telemetry = !(bool)digitalRead(telemetryButton);
	frame750.data[0] = (((bool)acceleration << 7) || ((bool) debug << 6) || ((bool)datalog << 5) || ((bool) telemetry << 4) ) || 0x00;
	frame750.time = timer;
	frame750.id = 750;
	send_Frame(frame750);

	if (acceleration) {
		printf("modalità accelerazione /n");
		/*
		avvio timer
		stopTimer(TC1, 0, TC3_IRQn);
		startTimer(TC1, 0, TC3_IRQn, FREQ_100Hz);
		*/
		ultimaCambiataTime = (uint32_t)millis();
		aggiorna0_100time=true;
	/*
		Serial.println("start timer");
		delay(500);
    	} else {
		stopTimer(TC1, 0, TC3_IRQn);
		Serial.println("stop timer");
		delay(500);
		mandare pacchetto con id 750
	*/
	}
}

void showGear()
{
	switch (gear) {
	case 0:
		digitalWrite(CD_D,LOW);
		digitalWrite(CD_C,LOW);
		digitalWrite(CD_B,LOW);
		digitalWrite(CD_A,LOW);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;

	case 1:
		digitalWrite(CD_D,LOW);
		digitalWrite(CD_C,LOW);
		digitalWrite(CD_B,LOW);
		digitalWrite(CD_A,HIGH);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;

	case 2:
		digitalWrite(CD_D,LOW);
		digitalWrite(CD_C,LOW);
		digitalWrite(CD_B,HIGH);
		digitalWrite(CD_A,LOW);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;

	case 3:
		digitalWrite(CD_D,LOW);
		digitalWrite(CD_C,LOW);
		digitalWrite(CD_B,HIGH);
		digitalWrite(CD_A,HIGH);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;

	case 4:
		digitalWrite(CD_D,LOW);
		digitalWrite(CD_C,HIGH);
		digitalWrite(CD_B,LOW);
		digitalWrite(CD_A,LOW);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;

	case 5:
		digitalWrite(CD_D,LOW);
		digitalWrite(CD_C,HIGH);
		digitalWrite(CD_B,LOW);
		digitalWrite(CD_A,HIGH);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;

	case 6:
		digitalWrite(CD_D,LOW);
		digitalWrite(CD_C,HIGH);
		digitalWrite(CD_B,HIGH);
		digitalWrite(CD_A,LOW);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;

	default:
		digitalWrite(CD_D,HIGH);
		digitalWrite(CD_C,HIGH);
		digitalWrite(CD_B,HIGH);
		digitalWrite(CD_A,HIGH);
		delay(1);
		digitalWrite(LE,HIGH);
		digitalWrite(LE,LOW);
		break;
	}
}

void buttonStates()
{
	//legge lo stato degli interruttori e imposta dei flag, dobbiamo creare pacchetto da inviare in memoria dinamica
	delayMicroseconds(50000);
	uint32_t timer = /*lap_time-*/(uint32_t)millis();//? sicuro?
	acceleration = !(bool)digitalRead(accelerationButton);
	debug = !(bool)digitalRead(debugButton);
	datalog = !(bool)digitalRead(datalogButton);
	telemetry = !(bool)digitalRead(telemetryButton);
	frame750.data[0] = (((bool)acceleration << 7) || ((bool) debug << 6) || ((bool)datalog << 5) || ((bool) telemetry << 4) ) || 0x00;
	frame750.time = timer;
	frame750.id = 750;
	send_Frame(frame750);
	if (debug == 1)printf("Debug Mode /n");
	if (datalog == 1)printf("Datalog Mode /n");
	if (telemetry == 1)printf("Telemetry Mode /n");
}

void closeLap()
{
	printf("lap closed /n");
	lap_time=(uint32_t)millis();
	lapNumber++;
	frame750.data[1] = 1;
	frame750.id = 750;
	frame750.time= lap_time;
	send_Frame(frame750);
	if (acceleration) {
		lapNumber = 1;
		time50_75m = (uint32_t)millis() - accStartTime;
		frame751.data[6] = time0_75m >>8;
		frame751.data[7] = time0_75m;
		frame751.time=lap_time;
		frame751.id=751;
		send_Frame(frame751);
	}
}

void NgearvalueChanged()
{
	if (!(bool)digitalRead(N_GR))
		gear=0;
}

/*void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
    int rc=0;
    rc = mosquitto_connect(mosq, host, port, 60);
    printf("connessione avvenuta");
}*/

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	/*if(message->topic == marcia){
		struct json_object* json_message;
		json_message =  json_object_new_string  ((char*)message -> payload);
		gear = (int)json_message["data"];
	}
	if(message->topic == rpm){
		struct json_object* json_message;
		json_message =  json_object_new_string  ((char*)message -> payload);
		rpm = (int)json_message["data"];
	}
	if(message->topic == VhSpeed){
		struct json_object* json_message;
		json_message =  json_object_new_string  ((char*)message -> payload);
		vh_speed = (int)json_message["data"];
	}*/

	json_object *jobj = json_tokener_parse(message->payload);
	enum json_type type;

	jobj = json_object_object_get(jobj, "data");
	int data = json_object_get_int(jobj);

	if (message->topic == GEAR)
		gear = (uint8_t)data;

	if (message->topic == rpm)
		Rpm = (uint8_t)data;

	if (message->topic == VhSpeed)
		Speed = (float)data;
}

int Setup(void)
{
	//inizializzazione interruttori
	pinMode(accelerationButton, INPUT);
	pinMode(debugButton, INPUT);
	pinMode(datalogButton, INPUT);
	pinMode(telemetryButton, INPUT);
	pinMode(lapEndButton, INPUT);

	acceleration = !(bool)digitalRead(accelerationButton);
	debug = !(bool)digitalRead(debugButton);
	datalog = !(bool)digitalRead(datalogButton);
	telemetry = !(bool)digitalRead(telemetryButton);
	accStartTime = (uint32_t)millis();

	//gestione degli interrupt della libreria wiringPi
	//int wiringPiISR ( int pin, int edgeType, void (*function)(void) )
	wiringPiISR(accelerationButton, INT_EDGE_BOTH, accelerationButtonValueChanged) ;
	wiringPiISR(debugButton, INT_EDGE_BOTH, buttonStates);
	wiringPiISR(datalogButton, INT_EDGE_BOTH, buttonStates);
	wiringPiISR(lapEndButton, INT_EDGE_BOTH, closeLap);

	//inizializzazione CD4511 per codifica display 7 segmenti
	pinMode(CD_A, OUTPUT);
	pinMode(CD_B, OUTPUT);
	pinMode(CD_C, OUTPUT);
	pinMode(CD_D, OUTPUT);
	pinMode(LE, OUTPUT);

	//shift light e folle
	pinMode(SH_LT, INPUT);
	pinMode(N_GR, INPUT);
	wiringPiISR(N_GR, INT_EDGE_BOTH, NgearvalueChanged);

	showGear(); //non è meglio spostare nel loop?
}

int main()
{
	wiringPiSetup();
	Setup();

	mosquitto_lib_init();
	mosq = mosquitto_new(NULL,true,NULL);
	mosquitto_connect(mosq, host, port, 60);
	mosquitto_message_callback_set(mosq, message_callback);
	mosquitto_subscribe(mosq, NULL, GEAR, 0);
	mosquitto_subscribe(mosq, NULL, rpm,0);
	mosquitto_subscribe(mosq, NULL, VhSpeed,0);

	mosquitto_loop_forever(mosq, -1, 1);
}
