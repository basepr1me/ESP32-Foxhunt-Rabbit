/*
 * Copyright (c) 2021, 2022 Tracey Emery K7TLE
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
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html><html>
<head>
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<style>
		html {
			font-family:Helvetica;
			display: inline-block;
			margin: 0px auto;
			text-align: center;
		}
		p {
			font-size: 1.2rem;
		}
		body {
			margin: 0;
		}
		.topbar {
			overflow: hidden;
			background-color: #000077;
			color: white;
			font-size: 1rem;
		}
		.content {
			padding: 20px;
		}
		.card {
			background-color: white;
			box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
		}
		.cards {
			max-width: 300px;
			margin: 0 auto;
			display: grid;
			grid-gap: 2rem;
			grid-template-columns: repeat(auto-fit,
			    minmax(200px, 1fr));
		}
	        .status {
			font-size: 1.4rem;
		}
		button {
			border: 1px solid #0066cc;
			background-color: #0099cc;
			color: #ffffff;
			padding: 5px 10px;
		}
		button:hover {
			border: 1px solid #0099cc;
			background-color: #00aacc;
			color: #ffffff;
			padding: 5px 10px;
		}
		button:onpress {
			border: 1px solid #0099cc;
			background-color: #ff0000;
			color: #ffffff;
			padding: 5px 10px;
		}
		button:disabled,
		button[disabled] {
			border: 1px solid #999999;
			background-color: #cccccc;
			color: #666666;
		}
	</style>
	<script>
		var xhr;

		if (!!window.EventSource) {
			var source = new EventSource("/event");
			source.addEventListener("rand", function (e) {
				document.getElementById("delay").innerHTML = e.data;
			}, false);
			source.addEventListener("rand_on", function (e) {
				document.getElementById("rand").innerHTML = "On";
				document.getElementById("randbox").checked = true;
				document.getElementById("time").setAttribute("disabled", "disabled");
				document.getElementById("time_b").setAttribute("disabled", "disabled");
			}, false);
			source.addEventListener("cw_on", function (e) {
				document.getElementById("cw").innerHTML = "Yes";
				document.getElementById("cwbox").checked = true;
			}, false);
			source.addEventListener("count", function (e) {
				document.getElementById("transmit").innerHTML = e.data;
			}, false);
			source.addEventListener("tnbutton", function (e) {
				if (e.data == "enable")
					document.getElementById("trans_now").removeAttribute("disabled");
				else if (e.data == "disable")
					document.getElementById("trans_now").setAttribute("disabled", "disabled");
			}, false);
			source.addEventListener("cwbox", function (e) {
				if (e.data == "enable")
					document.getElementById("cwbox").removeAttribute("disabled");
				else if (e.data == "disable")
					document.getElementById("cwbox").setAttribute("disabled", "disabled");
			}, false);
		}

		function start_hunt(element) {
			xhr = new XMLHttpRequest();
			xhr.open("GET", "/update?hunting=1", false);
			xhr.send();
			element.setAttribute("disabled", "disabled");
			document.getElementById("stop").removeAttribute("disabled");
			document.getElementById("hunting").innerHTML = "Hunt is on";
			document.getElementById("trans_now").removeAttribute("disabled");
		}

		function stop_hunt(element) {
			xhr = new XMLHttpRequest();
			xhr.open("GET", "/update?hunting=0", false);
			xhr.send();
			element.setAttribute("disabled" ,"disabled");
			document.getElementById("start").removeAttribute("disabled");
			document.getElementById("hunting").innerHTML = "Hunt is off";
			document.getElementById("transmit").innerHTML = "---";
			document.getElementById("trans_now").setAttribute("disabled", "disabled");
		}

		function power_on(element) {
			xhr = new XMLHttpRequest();
			xhr.open("GET", "/update?power=1", false);
			xhr.send();
			element.setAttribute("disabled", "disabled");
			document.getElementById("off").removeAttribute("disabled");
			document.getElementById("power").innerHTML = "Powered on";
			document.getElementById("start").removeAttribute("disabled");
		}

		function power_off(element) {
			xhr = new XMLHttpRequest();
			xhr.open("GET", "/update?power=0&hunting=0", false);
			xhr.send();
			element.setAttribute("disabled", "disabled");
			document.getElementById("on").removeAttribute("disabled");
			document.getElementById("power").innerHTML = "Powered off";
			document.getElementById("start").setAttribute("disabled", "disabled");
			document.getElementById("stop").setAttribute("disabled", "disabled");
			document.getElementById("hunting").innerHTML = "Hunt is off";
			document.getElementById("transmit").innerHTML = "---";
			document.getElementById("trans_now").setAttribute("disabled", "disabled");
		}

		function set_time() {
			var delay_t = parseInt(document.getElementById("time").value);
			if (isNaN(delay_t)) {
				alert("That is not a number!");
				document.getElementById("time").value = "";
			} else if (delay_t < %TRANSMIT_MIN% || delay_t > %TRANSMIT_MAX%) {
				alert("Number not within allotted range (" +
				    %TRANSMIT_MIN% + "-" + %TRANSMIT_MAX% + ")");
			} else {
				xhr = new XMLHttpRequest();
				xhr.open("GET", "/update?delay=" + delay_t);
				xhr.send();
				document.getElementById("delay").innerHTML = delay_t;
			}
		}

		function trans_now(element) {
			xhr = new XMLHttpRequest();
			xhr.open("GET", "/update?trans_now=1", false);
			xhr.send();
			element.setAttribute("disabled", "disabled");
		}

		function toggle_random(element) {
			if (element.checked) {
				xhr = new XMLHttpRequest();
				xhr.open("GET", "/update?rand=1", false);
				xhr.send();
				document.getElementById("rand").innerHTML = "On";
				document.getElementById("time").setAttribute("disabled", "disabled");
				document.getElementById("time_b").setAttribute("disabled", "disabled");
			} else {
				xhr = new XMLHttpRequest();
				xhr.open("GET", "/update?rand=0", false);
				xhr.send();
				document.getElementById("rand").innerHTML = "Off";
				document.getElementById("time").removeAttribute("disabled");
				document.getElementById("time_b").removeAttribute("disabled");
			}
		}

		function toggle_cw(element) {
			if (element.checked) {
				xhr = new XMLHttpRequest();
				xhr.open("GET", "/update?cw=1", false);
				xhr.send();
				document.getElementById("cw").innerHTML = "Yes";
			} else {
				xhr = new XMLHttpRequest();
				xhr.open("GET", "/update?cw=0", false);
				xhr.send();
				document.getElementById("cw").innerHTML = "No";
			}
		}
	</script>
</head>
<body>
	<div class="topbar">
		<h1>%SSID%</h1>
	</div>
	<div class="content">
		<div class="cards">
			<div class="card">
				<p>DELAY</p>
				<p><span class="status">
					<span id="delay">%DELAY%</span>
				</span></p>
				<p>
					<input onfocus="this.value=''" name="time" id="time" value="%DELAY%" type="number" />
				%BUTTON1%
				</p>
			</div>
			<div class="card">
				<p>RANDOM DELAY</p>
				<p><span class="status">
					<span id="rand">%RANDSTAT%</span>
				</span></p>
				<p>%RANDBOX%</p>
			</div>
			<div class="card">
				<p>SEND CW</p>
				<p><span class="status">
					<span id="cw">%CWSTAT%</span>
				</span></p>
				<p>%CWBOX%</p>
			</div>
			<div class="card">
				<p>HUNTING</p>
				<p><span class="status">
					<span id="hunting">%HUNTING%</span>
				</span></p>
				<p>%BUTTON2% %BUTTON3%</p>
			</div>
			<div class="card">
				<p>RADIO</p>
				<p><span class="status">
					<span id="power">%POWER%</span>
				</span></p>
				<p>%BUTTON4% %BUTTON5%</p>
			</div>
			<div class="card">
				<p>TRANSMIT IN</p>
				<p><span class="status">
					<span id="transmit">---</span>
				</span></p>
			</div>
			<div class="card">
				<p>PTT</p>
				<p><span class="status">
					%BUTTON6%
				</span></p>
			</div>
		</div>
	</div>
</body>
</html>
)rawliteral";
