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
 */

#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <XT_DAC_Audio.h>
#include <Morse.h>

#include "audio_call.h"
#include "html.h"

#define DEBUG		 0
#define TIMER		 0	/* save some power */

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

#define ONE_SECOND	 1000

#define MORSE_OUT	 26
#define AUDIO_OUT	 25
#define AUDIO_IN	 33

#define DPRINTF(x,y)	 do { if (DEBUG) { Serial.println(x); \
			    Serial2.println(x); sleep(y); }} while(0)

#define WPM		 15

/* Morse */
Morse			 morse_led(M_GPIO, RED_LED, WPM);
Morse			 morse_dac(M_DAC, MORSE_OUT, WPM);

String			 led_morse = "`ar`";
String			 dac_morse = "de the k7mva rabbit";

/* local_ssid will be used for the web interface header */
const char		*local_ssid = "MVARC Rabbit 1";
const char		*local_pass = "MV@rc1234";
IPAddress		 local_ip(10, 0, 50, 1);
IPAddress		 local_gw(10, 0, 50, 1);
IPAddress		 local_net(255, 255, 255, 0);

AsyncWebServer		 server(80);
AsyncEventSource	 events("/event");

XT_Wav_Class		 Sound(audio);
XT_DAC_Audio_Class	 DacAudio(AUDIO_OUT, 0);

void			 setup(void);
void			 loop(void);
void			 set_green(void);
void			 set_red(void);
void			 handle_transmission(void);

String			 handle_connect(const String&);

unsigned long		 transmit_start_millis, green_start_millis;
unsigned long		 tx_current_millis, countdown_timer_millis;
unsigned long		 countdown_millis, red_start_millis;

int			 transmit_now = 0; /* don't delay first */
int			 hunting = 0, playing = 0, play = 0, voice = 1;
int			 transmit_end = 0, powered = 1, rand_num = 1, cw = 1;
int			 send_handle_events = 0, transmit_new, alt_cw = 1;

uint8_t channel =	 3;

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

#ifdef DEBUG
	/* setup serial */
	Serial.begin(115200);
	Serial2.begin(115200);
	sleep(1);
#endif

	DPRINTF("", 1);
	DPRINTF("Setup serial", 0);
	DPRINTF("Setup transmitter", 0);

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
	/*
	 * Had to disable these in sdkconfig.h and use simple WiFi to keep
	 * Android 10 connected.
	 *
	 * #define CONFIG_ESP32_WIFI_AMPDU_TX_ENABLED 0
	 * #define CONFIG_ESP32_WIFI_AMPDU_RX_ENABLED 0
	 */

	/* WiFi.mode(WIFI_AP); */
	/* WiFi.softAP(local_ssid, local_pass); */

	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(local_ip, local_gw, local_net);
	WiFi.setTxPower(WIFI_POWER_19_5dBm);
	WiFi.softAP(local_ssid, local_pass, channel, false, MAX_CLIENTS);

	DPRINTF("Setup AP", 0);

	/* setup our web server */
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send_P(200, "text/html", index_html, handle_connect);
	});
	server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
		String input_message;

		if (request->hasParam("hunting") && powered) {
			DPRINTF("Hunting toggled", 0);
			input_message = request->getParam("hunting")->value();
			hunting = input_message.toInt();
			transmit_start_millis = tx_current_millis;
			if (hunting)
				digitalWrite(GREEN_LED, HIGH);
			else {
				morse_led.gpio_tx_stop();
				morse_dac.dac_tx_stop();
				digitalWrite(GREEN_LED, LOW);
			}
			countdown_millis = millis();
		}
		if (request->hasParam("power")) {
			DPRINTF("Power toggled", 0);
			input_message = request->getParam("power")->value();
			powered = input_message.toInt();
			if (!powered) {
				morse_led.gpio_tx_stop();
				morse_dac.dac_tx_stop();
				digitalWrite(RED_LED, LOW);
				digitalWrite(GREEN_LED, LOW);
			} else {
				digitalWrite(RED_LED, HIGH);
				digitalWrite(GREEN_LED, LOW);
			}

		}
		if (request->hasParam("delay")) {
			DPRINTF("Delay changed", 0);
			input_message = request->getParam("delay")->value();
			if (input_message.toInt() >= TRANSMIT_MIN &&
			    input_message.toInt() <= TRANSMIT_MAX)
				transmit_new = input_message.toInt();
		}
		if (request->hasParam("rand")) {
			DPRINTF("Random delay toggled", 0);
			input_message = request->getParam("rand")->value();
			rand_num = input_message.toInt();
		}
		if (request->hasParam("altcw")) {
			DPRINTF("Random cw toggled", 0);
			input_message = request->getParam("altcw")->value();
			alt_cw = input_message.toInt();
		}
		if (request->hasParam("cw")) {
			digitalWrite(GREEN_LED, HIGH);
			DPRINTF("CW toggled", 0);
			input_message = request->getParam("cw")->value();
			cw = input_message.toInt();
		}
		if (request->hasParam("trans_now")) {
			DPRINTF("PTT pressed", 0);
			input_message = request->getParam("trans_now")->value();
			transmit_now = 0;
		}

		request->send_P(200, "text/plain", "OK");
	});
	server.addHandler(&events);
	server.begin();
	DPRINTF("Setup web server", 0);

	/* turning on the tranmitter is the final step */
	digitalWrite(TRANSMIT_PWR, HIGH);
	digitalWrite(RED_LED, HIGH);
	DPRINTF("", 0);
	DPRINTF("Transmitter powered on", 1);
	DPRINTF("", 0);
	DPRINTF("The rabbit is ready to run!", 0);
	DPRINTF("", 0);

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

	if (cw)
		playing = morse_dac.dac_transmitting();
	else
		playing = Sound.Playing;

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
	if (hunting && !play && !playing && transmit_now > 0 && powered &&
	    tx_current_millis - countdown_timer_millis >= ONE_SECOND &&
	    TIMER) {
		events.send(String((transmit_now - (millis() -
		    countdown_millis)) / 1000).c_str(), "count", millis());
		countdown_timer_millis = tx_current_millis;
	}

	if (cw)
		morse_dac.dac_watchdog();
	else
		DacAudio.FillBuffer();

	if (hunting && !play && !playing && powered &&
	    (tx_current_millis - transmit_start_millis >= transmit_now)) {
		digitalWrite(BLUE_LED, HIGH);
		digitalWrite(TRANSMIT, HIGH);
		DPRINTF("Transmitting", 0);
		transmit_start_millis = tx_current_millis;
		events.send("disable", "tnbutton", millis());
		events.send("disable", "cwbox", millis());
		events.send("disable", "altcwbox", millis());
		if (TIMER)
			events.send("TX", "count", millis());
		if (rand_num) {
			transmit_now = random(TRANSMIT_MIN,
			    TRANSMIT_MAX) * ONE_SECOND;
			events.send(String(transmit_now / ONE_SECOND).c_str(),
			    "rand", millis());
		} else {
			/* setup our transmit delay now */
			if (transmit_new)
				transmit_now = transmit_new * ONE_SECOND;
			else
				transmit_now = TRANSMIT_DELAY * ONE_SECOND;
			events.send(String(transmit_now / ONE_SECOND).c_str(),
			    "rand", millis());
		}
		play = 1;
	}

	if (hunting && play && !playing && !cw) {
		if (tx_current_millis - transmit_start_millis >= ONE_SECOND) {
			DPRINTF("Play audio", 0);
			DacAudio.Play(&Sound);
			play = 0;
			transmit_end = 1;
		}
	} else if (hunting && play && !playing && cw) {
		if (tx_current_millis - transmit_start_millis >= ONE_SECOND) {
			DPRINTF("Send CW", 0);
			morse_dac.dac_tx(dac_morse);
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
			events.send("enable", "tnbutton", millis());
			events.send("enable", "altcwbox", millis());
			if (!alt_cw)
				events.send("enable", "cwbox", millis());
			else
				cw = !cw;
			DPRINTF("End transmission", 0);
			DPRINTF("", 0);
			digitalWrite(BLUE_LED, LOW);
			digitalWrite(TRANSMIT, LOW);
			transmit_start_millis = tx_current_millis;
			playing = 0;
			transmit_end = 0;
		}
	}
}

String
handle_connect(const String& var)
{
	int power = digitalRead(TRANSMIT_PWR);

	String button1 = "<button id=\"time_b\" onclick=\"set_time()\"";
	if (rand_num)
		button1 += " disabled";
	button1 += ">Update Transmission Delay</button>";

	String button2 = "<button id=\"start\" onclick=\"start_hunt(this)\"";
	if (hunting || !power)
		button2 += " disabled";
	button2 += ">Start Hunt</button>";

	String button3 = "<button id=\"stop\" onclick=\"stop_hunt(this)\"";
	if (!hunting || !power)
		button3 += " disabled";
	button3 += ">Stop Hunt</button>";

	String button4 = "<button id=\"on\" onclick=\"power_on(this)\"";
	if (power)
		button4 += " disabled";
	button4 += ">Power On Radio</button>";

	String button5 = "<button id=\"off\" onclick=\"power_off(this)\"";
	if (!power)
		button5 += " disabled";
	button5 += ">Power Off Radio</button>";

	String button6 = "<button id=\"trans_now\" onclick=\"trans_now(this)\"";
	if (!hunting || !power)
		button6 += " disabled";
	button6 += ">Transmit Now</button>";

	String box1 = "<input id=\"randbox\" type=\"checkbox\"";
	if (rand_num)
		box1 += " checked";
	box1 += " onchange=\"toggle_random(this)\" />";

	String box2 = "<input id=\"cwbox\" type=\"checkbox\"";
	if (cw)
		box2 += " checked";
	if (alt_cw)
		box2 += " disabled";
	box2 += " onchange=\"toggle_cw(this)\" />";

	String box3 = "<input id=\"altcwbox\" type=\"checkbox\"";
	if (alt_cw)
		box3 += " checked";
	box3 += " onchange=\"toggle_alt_cw(this)\" />";

	if (var == "HUNTING") {
		if (hunting)
			return "Hunt is on";
		else
			return "Hunt is off";
	}
	if (var == "POWER") {
		if (digitalRead(TRANSMIT_PWR))
			return "Powered On";
		else {
			transmit_now = 0;
			return "Powered Off";
		}
	}
	if (var == "BUTTON1")
		return button1;
	if (var == "BUTTON2")
		return button2;
	if (var == "BUTTON3")
		return button3;
	if (var == "BUTTON4")
		return button4;
	if (var == "BUTTON5")
		return button5;
	if (var == "BUTTON6")
		return button6;
	if (var == "DELAY") {
		if (transmit_now)
			return String(transmit_now / ONE_SECOND);
		else if (transmit_new)
			return String(transmit_new);
		else
			return String(TRANSMIT_DELAY);
	}
	if (var == "TRANSMIT_MIN")
		return String(TRANSMIT_MIN);
	if (var == "TRANSMIT_MAX")
		return String(TRANSMIT_MAX);
	if (var == "SSID")
		return String(local_ssid);
	if (var == "RANDSTAT") {
		if (rand_num)
			return "On";
		else
			return "Off";
	}
	if (var == "RANDBOX")
		return box1;
	if (var == "ALTCWSTAT") {
		if (alt_cw)
			return "On";
		else
			return "Off";
	}
	if (var == "ALTCWBOX")
		return box3;
	if (var == "CWSTAT") {
		if (cw)
			return "Yes";
		else
			return "No";
	}
	if (var == "CWBOX")
		return box2;
	if (var == "TX") {
		if (TIMER)
			return "---";
		else
			return "TX OFF";
	}

	return String();
}

void
set_green(void)
{
	if (cw && tx_current_millis - green_start_millis >= ONE_SECOND) {
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
