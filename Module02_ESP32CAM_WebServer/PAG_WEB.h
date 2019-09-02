/* PAG_WEB.h
 * --> Header file with the template of the HTML code of the web server run on ESP32-CAM
 */


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
	<meta charset="utf-8" name="viewport" content="width=device-width, initial-scale=1">
	<link rel="icon" href="data:,">
	
	<!-- Styles -->
	<style>
		body { text-align: center; font-family: "Trebuchet MS", Arial;}
		h1{font-size:64px; text-decoration: underline; font-weight: bold; }
		h3{font-size:36px; font-weight: bold; }
	</style>
	
	
	<!-- Head -->
	<head>
		<title>Web Server ESP32-CAM</title>
	</head>
	
	<!-- Body -->
	<body>
		<h1>Web Server Using the ESP32-CAM:</h1>
		
		<!-- Date and Time of the last photo -->
		<h3><b>Date of the last photo</b>: <span id="date_photo">%DATE_PHOTO%</span></h3>
		<h3><b>Time of the last photo:</b>: <span id="hour_photo">%HOUR_PHOTO%</span></h3>
		<h3><b>Temperature</b>: <span id="temp">%TEMPERATURE%</span>ºC</h3>
            <h3><b>Pressure</b>: <span id="pressure">%PRESSURE%</span>hPa</h3>
            <h3><b>Level of Ilumination from the LDR ([0, 1023])</b>: <span id="adc_ldr">%ADC_LDR%</span></h3>
            
		<!-- Foto -->
		<h3><b>Picture saved in the SPIFFS</b>: </h3>
		<center>
			<img src="photo_cam" height="600" width="800">
		</center>
	</body>
     
	
	<!-- Script em JS para as atualizacoes dos dados -->
	<script>
		setInterval(function ( ) {
			var xhttp = new XMLHttpRequest();
			xhttp.onreadystatechange = function() {
				if (this.readyState == 4 && this.status == 200) {
					document.getElementById("date_photo").innerHTML = this.responseText;
				}
			};
			xhttp.open("GET", "/date_photo", true);
			xhttp.send();
		}, 10000 );

		setInterval(function ( ) {
			var xhttp = new XMLHttpRequest();
			xhttp.onreadystatechange = function() {
				if (this.readyState == 4 && this.status == 200) {
					document.getElementById("hour_photo").innerHTML = this.responseText;
				}
			};
			xhttp.open("GET", "/hour_photo", true);
			xhttp.send();
		}, 10000 ) ;

            setInterval(function ( ) {
                  var xhttp = new XMLHttpRequest();
                  xhttp.onreadystatechange = function() {
                        if (this.readyState == 4 && this.status == 200) {
                              document.getElementById("temp").innerHTML = this.responseText;
                        }
                  };
                  xhttp.open("GET", "/temp", true);
                  xhttp.send();
            }, 10000 ) ;

            setInterval(function ( ) {
                  var xhttp = new XMLHttpRequest();
                  xhttp.onreadystatechange = function() {
                        if (this.readyState == 4 && this.status == 200) {
                              document.getElementById("pressure").innerHTML = this.responseText;
                        }
                  };
                  xhttp.open("GET", "/pressure", true);
                  xhttp.send();
            }, 10000 ) ;

            setInterval(function ( ) {
                  var xhttp = new XMLHttpRequest();
                  xhttp.onreadystatechange = function() {
                        if (this.readyState == 4 && this.status == 200) {
                              document.getElementById("adc_ldr").innerHTML = this.responseText;
                        }
                  };
                  xhttp.open("GET", "/adc_ldr", true);
                  xhttp.send();
            }, 10000 ) ;
	</script>
</html>)rawliteral";
