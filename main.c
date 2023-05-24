#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <stdio.h>
#include <wiringPi.h>
#include <softTone.h>

#define MQTT_HOSTNAME "192.168.0.41"
#define MQTT_PORT 1883
#define MQTT_USERNAME "kjy"
#define MQTT_PASSWORD "kjy"
#define MQTT_TOPIC "kjy/601"
#define TP 20
#define EP 21
#define LIMIT 2
#define BUZZER 18

int compare(void*first, void*second);
float getDistance(void);
int discriminatePerson(float detectedDistance);
void sendMqtt(struct mosquitto *mosq, char *text);

int main(int argc, char **argv) {
	
	
	int count = 0;
	int discriminate;
	int alarm = 0;
	int prevent = 0;
	char text[20] = "Now Operated!";
	char text2[20] = "Now, It's Okay";

	if(wiringPiSetupGpio() == -1)
		return 1;

	pinMode(TP, OUTPUT);
	pinMode(EP, INPUT);
	pinMode(BUZZER, OUTPUT);
	if(softToneCreate(BUZZER) == -1)
		return 1;

	struct mosquitto *mosq = NULL;

	//initalizing
	mosquitto_lib_init();

	//make mosquitto runtime object and clinet's random id
	mosq = mosquitto_new(NULL, true, NULL);
	if(!mosq) {
		printf("Cant initailize mosquitto library\n");
		exit(-1);
	}

	mosquitto_username_pw_set(mosq, MQTT_USERNAME, MQTT_PASSWORD);
	
	// sullib MQTT server connection, keep-alive massage is not used

	float firstDistanceArr[5];
	for(int i = 0; i < 5; i++) {
		firstDistanceArr[i] = getDistance();
		delay(100);
	}
	qsort(firstDistanceArr, 5, sizeof(firstDistanceArr[0]),compare);
	float firstDistance = firstDistanceArr[2];
	printf("Fisrt Distance is %.2lf \n", firstDistance);

	sendMqtt(mosq, text);

	sprintf(text, "Warning!");

	while(1) {
		float fDistance = getDistance();
		
		delay(100);
		if (fDistance < firstDistance  * 0.8) prevent++; 
		if ( prevent == 2 ) {
			prevent = 0;
			discriminate = discriminatePerson(fDistance);
			if(discriminate == 0) {
				if(count == 0) printf("Count Error!\n");
				else {
					count--;
					printf("One Person Left. Now %d \n", count);
					
				}
				delay(2000);
			}
			else if(discriminate == 1) {
				count++;
				printf("One Person Came In. Now %d \n", count);
				delay(2000);
			}
			else {
				printf("There is not changed. Now %d \n", count);
				delay(500);
			}
		}
		if (count > LIMIT && alarm == 0) {
			sendMqtt(mosq, text);
			printf("%s\n", text);
			softToneWrite(BUZZER, 800);

			alarm = 1;
		}
		else if (count <= LIMIT && alarm == 1) {
			sendMqtt(mosq, text2);
			alarm = 0;
			softToneWrite(BUZZER, 0);
		}

	}


	mosquitto_disconnect(mosq);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	return 0;
}

int compare(void*first, void*second)
{
	if (*(int*)first > *(int*)second)
		return 1;
	else if (*(int*)first < *(int*)second)
		return -1;
	else
		return 0;
}

float getDistance(void)
{
        float fDistance;
        int nStartTime, nEndTime;

        digitalWrite(TP, LOW);
        delayMicroseconds(2);
        // pull the Trig pin to high level for more than 10us impulse
        digitalWrite(TP, HIGH);
        delayMicroseconds(10);
        digitalWrite(TP, LOW);

        while(digitalRead(EP)==LOW);
        nStartTime = micros();

        while(digitalRead(EP) == HIGH);
        nEndTime = micros();

        fDistance = (nEndTime - nStartTime) / 29. / 2.;

        return fDistance;
}

int discriminatePerson(float detectedDistance)
{
	float distance = 0;
	float avgDistance = 0;
	int i;
	
	printf("\n%.2f cm first\n", detectedDistance);
	for(i = 0; i < 5; i++) {
		distance = getDistance();
		printf("%.2f cm\n", distance);
		avgDistance += distance;
		delay(100);
	}
	
	avgDistance = avgDistance / 5;
	if ( detectedDistance  * 1.1 < avgDistance ) return 0;
	else if ( detectedDistance * 0.9 > avgDistance ) return 1;
	else return 2;
}

void sendMqtt(struct mosquitto *mosq, char *text)
{
	int ret = mosquitto_connect(mosq, MQTT_HOSTNAME, MQTT_PORT, 10);
        if(ret) {
                printf("Cant connect to mosqiotto server\n");
                exit(-1);
        }

	ret = mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(text), text, 0, false);
        if(ret) {
                printf("Cant connect to mosquitto server\n");
                exit(-1);
        }


}
