/*
 * Copyright (c) 2021, 2022 Tracey Emery K7TLE <tle@k7tle.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Watch the order of pins on the board!
 *
 * D14 - Red LED	Power/AR			#Solid/cw
 * D12 - Green LED	Transmitting Voice/cw/Off	#Solid/Blink/Off
 * D13 - Blue LED	Transmitting			#On during transmit
 *
 * D5  - Transmit optical relay
 * D18 - Transmitter POWER relay
 * D25 - Audio out
 * D33 - Audio in
 * D26 - Morse out
 *
 * # If we have enough memory leftover
 * CLUB/R OFF		# Power Transmitter Off
 * CLUB/R ON		# Power Transmitter On
 * CLUB/R START		# Start Transmitting Audio
 * CLUB/R STOP		# Stop Transmitting Audio
 *
 * packet types:
 *	0x00000001:	ping/pong
 *	0x00000010:	transmit delay
 *	0x00000011:	update controls
 * 	0x00000100:	control request
 */

#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <WiFi.h>
#include <XT_DAC_Audio.h>
#include <Morse.h>
#include <esp_wifi.h>

#include <EEPROM.h>

#include "audio_call.h"

#define DEBUG		 0

#define MAX_CLIENTS	 8

#define RED_LED		 14
#define	GREEN_LED	 12
#define BLUE_LED	 13

#define TRANSMIT	 5
#define TRANSMIT_PWR	 18	/* LOW - OFF, HIGH - ON (switch LOW - ON) */
/* #define OPTIONAL	 21 */
#define TRANSMIT_DELAY	 10	/* seconds */
#define TRANSMIT_MIN	 10	/* seconds */
#define TRANSMIT_MAX	 30	/* seconds */

#define BROADCAST	 10,0,50,255
#define IP		 10,0,50,1
#define SUBNET		 255,255,255,0
#define PORT		 8080

#define ONE_SECOND	 1000

#define DAC_CHANNEL	 2
#define AUDIO_OUT	 25
#define AUDIO_IN	 33

#define PING		 0x01
#define TRANSDELAY	 0x02
#define UPDATE		 0x03
#define CRQST		 0x04
#define ENABLE		 0x05
#define DISABLE		 0x06
#define ADDBUF		 4
#define CALLS		 5

#define DPRINTF(x,y)	 do { if (DEBUG) { Serial.println(x); \
			     sleep(y); }} while(0)

#define WPM		 15

#define GETCW		 1 << 0
#define GETALT		 1 << 1
#define GETPWR		 1 << 2
#define GETHUNT		 1 << 3
#define GETPTT		 1 << 4
#define GETRND		 1 << 5
#define GETAUTO		 1 << 6

#define SETALT		 1 >> 0
#define SETPWR		 1 >> 1
#define SETHUNT		 1 >> 2
#define SETPTT		 1 >> 3
#define SETRND		 1 >> 4
#define SETAUTO		 1 >> 5

#define EEPROM_S	 1

Morse			 morse_led(M_GPIO, RED_LED, WPM);
Morse			 morse_dac(M_DAC, DAC_CHANNEL, WPM);

String			 led_morse = "`ar`";
String			 dac_morse[CALLS] = {
				"de the k7mva rabbit",
				"come find me k7mva",
				"the k7mva rabbit is loose",
				"catch me if you can k7mva",
				"k7mva rabbit hiding out",
			 };

const char		*local_ssid = "MVARC Rabbit 1";
const char		*local_pass = "MV@rc1234";
IPAddress		 local_ip(IP);
IPAddress		 local_subnet(SUBNET);
IPAddress		 local_gateway(IP);
IPAddress		 local_broadcast(BROADCAST);

WiFiUDP			 server;

XT_Wav_Class		 Sound(audio);
XT_DAC_Audio_Class	 DacAudio(AUDIO_OUT, 0);

void			 setup(void);
void			 loop(void);
void			 set_green(void);
void			 set_red(void);
void			 handle_transmission(void);
void			 process_packets(void);
void			 wifievent(WiFiEvent_t);
void			 pong(void);
void			 send_delay(int);
void			 send_control(void);
void			 send_enable(void);
void			 send_disable(void);
void			 update_control(uint8_t []);

unsigned long		 transmit_start_millis, green_start_millis;
unsigned long		 tx_current_millis, countdown_timer_millis;
unsigned long		 countdown_millis, red_start_millis;

int			 transmit_now = 0; /* don't delay first */
int			 autoh, hunting, playing = 0, play = 0, voice = 1;
int			 transmit_end = 0, powered = 1, rand_num = 1, cw = 1;
int			 send_handle_events = 0, transmit_new = 1, alt_cw = 1;
int			 connected = 0, cw_ctl = 1;

uint8_t			 channel = 6;
uint8_t			 preval = 0x7E, pre, type, pcnt, cpkt, calc, fin;

char			 packet[1];
dac_cw_config_t		 dac_cw_config;

void
wifievent(WiFiEvent_t e)
{
	switch(e) {
	case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
		DPRINTF("Rabbit remote connected", 0);
		connected = 1;
		break;
	case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
		DPRINTF("Rabbit remote disconnected", 0);
		connected = 0;
		break;
	default:
		break;
	}
}

void
setup()
{
	/* disable brownout for startup */
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
	delay(1000);

	/* immediate pin setup, transmitter shouldn't be on */
	pinMode(TRANSMIT, OUTPUT);
	pinMode(TRANSMIT_PWR, OUTPUT);
	digitalWrite(TRANSMIT, LOW);
	digitalWrite(TRANSMIT_PWR, LOW);

	/* setup serial */
	Serial.begin(115200);
	Serial.setDebugOutput(DEBUG);
	sleep(1);

	DPRINTF("", 1);
	DPRINTF("Setup serial", 0);
	DPRINTF("Setup transmitter", 0);

	/* start EEPROM and get state */
	EEPROM.begin(EEPROM_S);
	autoh = EEPROM.read(0);
	hunting = autoh;

	if (autoh)
		DPRINTF("Auto hunt enabled", 0);
	else
		DPRINTF("Auto hunt disabled", 0);

	/* setup the DAC */
	dac_cw_config.scale = DAC_CW_SCALE_2;
	dac_cw_config.freq = 550;

	morse_dac.dac_cw_setup(&dac_cw_config);
	DPRINTF("DAC configured", 0);

	/* setup LEDs */
	pinMode(RED_LED, OUTPUT);
	pinMode(GREEN_LED, OUTPUT);
	pinMode(BLUE_LED, OUTPUT);
	digitalWrite(RED_LED, LOW);
	digitalWrite(GREEN_LED, LOW);
	digitalWrite(BLUE_LED, LOW);
	DPRINTF("Setup LEDs", 0);

	/* audio setup */
	pinMode(AUDIO_OUT, OUTPUT);
	pinMode(AUDIO_IN, INPUT);
	DPRINTF("Setup audio", 0);
	/* DacAudio.DacVolume = 35; */

	/* setup our AP */
	WiFi.mode(WIFI_AP);
	if (esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR))
		DPRINTF("LR mode failed", 1);
	else
		DPRINTF("LR mode enabled", 1);
	WiFi.softAPConfig(local_ip, local_gateway, local_subnet);
	WiFi.softAP(local_ssid, local_pass, channel, false, MAX_CLIENTS);
	WiFi.onEvent(wifievent);

	DPRINTF("Setup AP", 0);

	server.begin(PORT);
	DPRINTF("Started UDP mode", 0);

	/* turning on the tranmitter is the final step */
	digitalWrite(TRANSMIT_PWR, HIGH);
	digitalWrite(RED_LED, HIGH);
	DPRINTF("Transmitter powered on", 1);
	DPRINTF("The rabbit is ready to run!", 0);

	/* setup millis timers */
	transmit_start_millis = millis();
	green_start_millis = millis();
	red_start_millis = millis();

	/* re-enable brownout */
	delay(1000);
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);
}

void
loop()
{
	tx_current_millis = millis();

	if (cw_ctl)
		playing = morse_dac.dac_transmitting();
	else
		playing = Sound.Playing;

	process_packets();
	handle_transmission();

	if (hunting && powered)
		set_green();

	if (!powered) {
		set_red();
		digitalWrite(TRANSMIT_PWR, LOW);
	}

	if (powered)
		digitalWrite(TRANSMIT_PWR, HIGH);
}

void
handle_transmission(void)
{
	if (!powered && playing) {
		DacAudio.StopAllSounds();
		playing = 0;
		transmit_end = 2;
	}

	if (hunting && !play && !playing && transmit_now > 0 && powered &&
	    tx_current_millis - countdown_timer_millis >= ONE_SECOND) {
		countdown_timer_millis = tx_current_millis;
	}

	morse_dac.dac_watchdog();
	DacAudio.FillBuffer();

	if (hunting && !play && !playing && powered &&
	    (tx_current_millis - transmit_start_millis >= transmit_now)) {
		digitalWrite(BLUE_LED, HIGH);
		DPRINTF("Transmitting", 0);
		transmit_start_millis = tx_current_millis;
		if (rand_num) {
			transmit_now = random(TRANSMIT_MIN,
			    TRANSMIT_MAX) * ONE_SECOND;
		} else {
			/* setup our transmit delay now */
			if (transmit_new) {
				transmit_now = ONE_SECOND;
				transmit_new = 0;
			} else
				transmit_now = TRANSMIT_DELAY * ONE_SECOND;
		}
		send_delay(transmit_now / ONE_SECOND);
		send_disable();
		play = 1;
	}

	if (hunting && play && !playing && !cw_ctl) {
		digitalWrite(TRANSMIT, HIGH);
		if (tx_current_millis - transmit_start_millis >= ONE_SECOND) {
			DPRINTF("Play audio", 0);
			DacAudio.Play(&Sound);
			play = 0;
			transmit_end = 1;
		}
	} else if (hunting && play && !playing && cw_ctl) {
		digitalWrite(TRANSMIT, HIGH);
		if (tx_current_millis - transmit_start_millis >= ONE_SECOND) {
			DPRINTF("Send CW", 0);
			morse_dac.dac_tx(dac_morse[random(0, CALLS)]);
			play = 0;
			transmit_end = 1;
		}
	}

	if (transmit_end && playing) {
		transmit_start_millis = tx_current_millis;
		countdown_timer_millis = tx_current_millis;
		countdown_millis = tx_current_millis;
		transmit_end = 2;
	}

	if (!playing && transmit_end == 2) {
		if (tx_current_millis - transmit_start_millis >= ONE_SECOND) {
			if (!alt_cw && cw)
				cw_ctl = 1;
			else if (!cw)
				cw_ctl = 0;
			else if (alt_cw)
				cw_ctl = !cw_ctl;
			DPRINTF("End transmission", 0);
			digitalWrite(BLUE_LED, LOW);
			digitalWrite(TRANSMIT, LOW);
			transmit_start_millis = tx_current_millis;
			playing = 0;
			transmit_end = 0;
			send_enable();
		}
	}
}

void
process_packets(void)
{
	int pkt_len = server.parsePacket();

	if (pkt_len) {
		int len, i;
		uint16_t sum;

		/* check we are receiving good data */
		len = server.read(packet, 1);
		if (len < 1 || len > 1)
			goto done;
		memcpy(&pre, packet, 1);
		if (pre != preval) {
			memset(&pre, 0, 1);
			goto done;
		}

		/* get our packet type */
		len = server.read(packet, 1);
		if (len < 1 || len > 1)
			goto done;
		memcpy(&type, packet, 1);

		/* get our data len */
		len = server.read(packet, 1);
		if (len < 1 || len > 1)
			goto done;
		memcpy(&pcnt, packet, 1);

		/* get our data */
		char mpkt[pcnt];
		uint8_t cpkt[pcnt];

		len = server.read(mpkt, pcnt);
		if (len != pcnt)
			goto done;
		memcpy(&cpkt, mpkt, pcnt);

		/* get our calculation */
		len = server.read(packet, 1);
		if (len < 1 || len > 1)
			goto done;
		memcpy(&calc, packet, 1);

		/* get our simple checksum */
		sum = pre + type + pcnt;
		for (i = 0; i < pcnt; i++)
			sum += cpkt[i];

		fin = sum & 0xFF;

		if (calc != fin)
			goto err;

		/* process ping */
		switch (type) {
		case (PING):
			pong();
			break;
		case (TRANSDELAY):
			transmit_now = cpkt[0] * 1000;
			break;
		case (UPDATE):
			update_control(cpkt);
			break;
		case (CRQST):
			send_control();
			if (playing)
				send_disable();
			break;
		default:
			break;
		}
err:
		for (i = 0; i < pcnt; i++)
			memset(&cpkt[i], 0, 1);
	}
done:
	server.flush();
	memset(&pre, 0, 1);
	memset(&pcnt, 0, 1);
	memset(&cpkt, 0, 1);
	memset(&fin, 0, 1);
}

void
update_control(uint8_t pkt[])
{
	if (DEBUG) {
		Serial.print("Control received: ");
		Serial.println(pkt[0], BIN);
	}

	/* always power off when requested */
	powered = (pkt[0] >> SETPWR) & 1;

	/* if (playing) */
	/* 	return; */

	play = (pkt[0] >> SETPTT) & 1;
	/* if (play) */
	/* 	return; */

	cw = pkt[0] & 1;

	alt_cw = (pkt[0] >> SETALT) & 1;
	if (cw)
		cw_ctl = 1;
	else if (!cw)
		cw_ctl = 0;

	hunting = (pkt[0] >> SETHUNT) & 1;
	if (!hunting) {
		transmit_new = 1;
		transmit_now = 0;
	}
	rand_num = (pkt[0] >> SETRND) & 1;

	if (autoh != (pkt[0] >> SETAUTO) & 1) {
		autoh = (pkt[0] >> SETAUTO) & 1;
		EEPROM.write(0, autoh);
		EEPROM.commit();
		if (autoh)
			DPRINTF("Auto hunt enabled", 0);
		else
			DPRINTF("Auto hunt disabled", 0);
	}
}

void
set_green(void)
{
	if (cw_ctl && tx_current_millis - green_start_millis >= ONE_SECOND) {
		digitalWrite(GREEN_LED, !digitalRead(GREEN_LED));
		green_start_millis = millis();
	}
}

void
set_red(void)
{
	morse_led.gpio_watchdog();
	if (morse_led.gpio_transmitting())
		red_start_millis = millis();

	if (!morse_led.gpio_transmitting() && tx_current_millis -
	    red_start_millis >= ONE_SECOND * 2) {
		morse_led.gpio_tx(led_morse);
	}
}

void
send_enable(void)
{
	if (!connected) {
		DPRINTF("Connection failed", 0);
		return;
	}

	DPRINTF("Manager enable", 0);

	uint16_t sum;
	uint8_t i = 0, j, cnt = 1;
	uint8_t buf[cnt + ADDBUF];

	buf[i++] = preval;
	buf[i++] = ENABLE;
	buf[i++] = cnt;
	buf[i++] = 0;

	for (j = 0; j < i; j++)
		sum += buf[j];
	fin = sum & 0xFF;

	buf[i++] = fin;

	server.beginPacket(server.remoteIP(), server.remotePort());
	server.write(buf, cnt + ADDBUF);
	server.endPacket();
}

void
send_disable(void)
{
	if (!connected) {
		DPRINTF("Connection failed", 0);
		return;
	}

	DPRINTF("Manager disable", 0);

	uint16_t sum;
	uint8_t i = 0, j, cnt = 1;
	uint8_t buf[cnt + ADDBUF];

	buf[i++] = preval;
	buf[i++] = DISABLE;
	buf[i++] = cnt;
	buf[i++] = 0;

	for (j = 0; j < i; j++)
		sum += buf[j];
	fin = sum & 0xFF;

	buf[i++] = fin;

	server.beginPacket(server.remoteIP(), server.remotePort());
	server.write(buf, cnt + ADDBUF);
	server.endPacket();
}

void
pong(void)
{
	if (!connected) {
		DPRINTF("Connection failed", 0);
		return;
	}

	DPRINTF("PING", 0);

	uint16_t sum;
	uint8_t i = 0, j, cnt = 1;
	uint8_t buf[cnt + ADDBUF];

	buf[i++] = preval;
	buf[i++] = PING;
	buf[i++] = cnt;
	buf[i++] = 0;

	for (j = 0; j < i; j++)
		sum += buf[j];
	fin = sum & 0xFF;

	buf[i++] = fin;

	server.beginPacket(server.remoteIP(), server.remotePort());
	server.write(buf, cnt + ADDBUF);
	server.endPacket();
}

void
send_control(void)
{
	if (!connected) {
		DPRINTF("Connection failed", 0);
		return;
	}

	uint16_t sum;
	uint8_t i = 0, j, pkt = 0, cnt = 1;
	uint8_t buf[cnt + ADDBUF];

	if (cw)
		pkt |= GETCW;
	if (alt_cw)
		pkt |= GETALT;
	if (powered)
		pkt |= GETPWR;
	if (hunting)
		pkt |= GETHUNT;
	if (play)
		pkt |= GETPTT;
	if (rand_num)
		pkt |= GETRND;
	if (autoh)
		pkt |= GETAUTO;

	if (DEBUG) {
		Serial.print("Control requested: ");
		Serial.println(pkt, BIN);
	}

	buf[i++] = preval;
	buf[i++] = CRQST;
	buf[i++] = cnt;
	buf[i++] = pkt;

	for (j = 0; j < i; j++)
		sum += buf[j];
	fin = sum & 0xFF;

	buf[i++] = fin;

	server.beginPacket(server.remoteIP(), server.remotePort());

	server.beginPacket(server.remoteIP(), server.remotePort());
	server.write(buf, cnt + ADDBUF);
	server.endPacket();
}

void
send_delay(int trans_delay)
{
	if (!connected) {
		DPRINTF("Connection failed", 0);
		return;
	}

	DPRINTF("Transmit delay", 0);

	uint16_t sum;
	uint8_t i = 0, j, cnt = 1;
	uint8_t buf[cnt + ADDBUF];

	buf[i++] = preval;
	buf[i++] = TRANSDELAY;
	buf[i++] = cnt;
	buf[i++] = trans_delay & 0xFF;

	for (j = 0; j < i; j++)
		sum += buf[j];
	fin = sum & 0xFF;

	buf[i++] = fin;

	server.beginPacket(server.remoteIP(), server.remotePort());
	server.write(buf, cnt + ADDBUF);
	server.endPacket();
}
