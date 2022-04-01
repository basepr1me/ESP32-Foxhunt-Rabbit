/*
 * Copyright (c) 2022 Tracey Emery K7TLE <tle@k7tle.com>
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
 *
 * This is: client<->remote<->rabbit
 *
 * what the remote receives from the rabbit:
 * 	pong response
 * 	transmit delay
 * 	update controls
 *
 * packet types:
 * 	0x00000001:	ping/pong
 * 	0x00000010:	transmit delay
 * 	0x00000011:	update controls
 * 	0x00000100:	control request
 */

#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>

#define DEBUG		 0

#define PORT		 8080
#define TIMEOUT		 30
#define ONE_SECOND	 1000

#define PING		 0x01
#define TRANSDELAY	 0x02
#define UPDATE		 0x03
#define CRQST		 0x04

#define DPRINTF(x,y)	 do { if (DEBUG) { Serial.println(x); \
			    Serial2.println(x); delay(y); }} while(0)

const char		*ssid = "MVARC Rabbit 1";
const char		*pass = "MV@rc1234";

uint8_t			 recon = 0, do_ping = 0, did_pong = 1;
uint8_t			 preval = 0x7E, pre, mpre, type, pcnt, calc, fin;
uint16_t		 sum;

char			 packet[1];

unsigned long		 ping_millis, ping_timeout;

void			 wifievent(WiFiEvent_t);
void			 ping(void);
void			 check_serial(void);
void			 process_packet(void);
void IRAM_ATTR		 ping_now(void);

volatile int		 interruptCounter;
hw_timer_t		*ping_timer = NULL;
portMUX_TYPE		 timer_mux = portMUX_INITIALIZER_UNLOCKED;

IPAddress		 rabbit(10, 0, 50, 1);

WiFiUDP			 server;

void
wifievent(WiFiEvent_t e)
{
	switch(e) {
	case ARDUINO_EVENT_WIFI_STA_CONNECTED:
		recon = 0;
		break;
	case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
		WiFi.reconnect();
		if (DEBUG)
			if (recon)
				Serial.print(".");
			else
				Serial.print("\r\nReconnecting");
		recon = 1;
		break;
	default:
		break;
	}
}

void setup() {
	/* disable brownout for startup */
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
	delay(1000);

	/* setup serial */
	Serial.begin(115200);
	sleep(1);
	Serial.setDebugOutput(DEBUG);

	DPRINTF("", 1);
	DPRINTF("Setup serial", 1000);

	/* setup AP */
	WiFi.mode(WIFI_STA);
	if(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR))
		DPRINTF("LR Mode Failed", 1);
	else
		DPRINTF("LR Mode Enabled", 1);

	WiFi.begin(ssid, pass);
	WiFi.onEvent(wifievent);
	if (DEBUG)
		Serial.print("Connecting to Rabbit");
	while(WiFi.status() != WL_CONNECTED) {
		if (DEBUG) {
			delay(500);
			Serial.print(".");
		}
	}

	server.begin(PORT);

	DPRINTF("", 1000);
	DPRINTF("Connected", 1000);

	ping_timer = timerBegin(0, 80, true);
	timerAttachInterrupt(ping_timer, &ping_now, true);
	timerAlarmWrite(ping_timer, 10000000, true);
	timerAlarmEnable(ping_timer);

	/* re-enable brownout */
	delay(1000);
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);
}

void loop() {
	ping();
	check_serial();
	process_packet();
}

void
process_packet(void)
{
	int pkt_len = server.parsePacket();
	if (pkt_len) {
		int len, i;

		/* check we are receiving good data */
		len = server.read(packet, 1);
		if (len < 1 || len > 1)
			return;
		memcpy(&pre, packet, 1);
		if (pre != preval)
			goto done;

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
		if (type == PING) {
			uint16_t pong_millis = millis() - ping_millis;
			pcnt = 2;
			uint8_t ct = 1 + 1 + 1 + pcnt + 1;
			uint8_t i = 0;

			if (DEBUG) {
				Serial.print("PONG Packet ... ");
				Serial.print(pong_millis);
				Serial.println(" ms");
			}

			did_pong = 1;
			calc = (preval + type + pcnt + pong_millis) & 0xFF;
			uint8_t buf[ct];

			buf[i++] = preval;
			buf[i++] = type;
			buf[i++] = pcnt;
			buf[i++] = (pong_millis >> 8) & 0xFF;
			buf[i++] = pong_millis & 0xFF;
			buf[i++] = calc;

			Serial.write(buf, ct);
			goto done;
		}

		/* send cpkt to app */
		/* hex visual of our packet */
		if (DEBUG) {
			Serial.print("\r\nPkt: ");
			Serial.print(pre, HEX);
			Serial.print("-");
			Serial.print(type, HEX);
			Serial.print("-");
			Serial.print(pcnt, HEX);
			Serial.print("-");
			for (i = 0; i < pcnt; i++) {
				Serial.print(cpkt[i], HEX);
				Serial.print("-");
			}
			Serial.println(calc, HEX);
		}
		Serial.write(pre);
		Serial.write(type);
		Serial.write(pcnt);
		for (i = 0; i < pcnt; i++)
			Serial.write(cpkt[i]);
		Serial.write(calc);
err:
		for (i = 0; i < pcnt; i++)
			memset(&cpkt[i], 0, 1);
	}
done:
	memset(&pre, 0, 1);
	memset(&type, 0, 1);
	memset(&pcnt, 0, 1);
	memset(&calc, 0, 1);
}

void
ping(void)
{
	if (recon || !do_ping)
		return;

	do_ping = 0;

	server.beginPacket(rabbit, 8080);

	uint8_t buf = 0, cnt = 1;

	sum = preval + cnt + buf;
	fin = sum & 0xFF;

	/* send our 3 bytes */
	server.write(preval);
	server.write(cnt);
	server.write(buf);
	server.write(fin);

	server.endPacket();

	if (!did_pong) {
		ping_timeout += millis() - ping_millis;
		if (ping_timeout >= TIMEOUT * ONE_SECOND) {
			DPRINTF("PING TIMEOUT!", 0);
		}
	} else
		ping_timeout = 0;

	ping_millis = millis();
	did_pong = 0;
}

void IRAM_ATTR
ping_now(void)
{
	portENTER_CRITICAL_ISR(&timer_mux);
	do_ping = 1;
	portEXIT_CRITICAL_ISR(&timer_mux);
}

void
check_serial(void)
{
	/* check for serial data and write to rabbit */
	if (Serial.available()) {
		uint8_t spkt;

		server.beginPacket(rabbit, 8080);
		while (Serial.available()) {
			spkt = Serial.read();
			server.write(spkt);
		}
		server.endPacket();
	}
}
