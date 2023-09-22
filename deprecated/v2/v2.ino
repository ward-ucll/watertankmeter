// Captive Portal
#include <AsyncTCP.h>  //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>	//https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>			//Used for mpdu_rx_disable android workaround

// Pre reading on the fundamentals of captive portals https://textslashplain.com/2022/06/24/captive-portals/

const char *ssid = "Waterput";  // FYI The SSID can't have a space in it.
const char *password = "n";  // no password

#define MAX_CLIENTS 4	// ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6	// 2.4ghz channel 6 https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

const IPAddress localIP(4, 3, 2, 1);		   // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2, 1);		   // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0);  // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "http://4.3.2.1";	 // a string version of the local IP with http, used for redirecting clients to your webpage

//sonar
const int trigPin = 19; // Trigger Pin of Ultrasonic Sensor
const int echoPin = 21; // Echo Pin of Ultrasonic Sensor

//rtc module
const int CLK_PIN = 13; // D13 on ESP32
const int DAT_PIN = 12; // D12 on ESP32
const int RST_PIN = 14; // D14 on ESP32

//specific values
const int maxWaterHoogte = 200; // in cm
const int totaleAfstandTotBodem = 300; //in cm 

char daysOfTheWeek[7][12] = {"Zondag", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag"};

//webserver
#include <ArduinoJson.h>


const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="nl">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Waterput</title>
    <style>
        /* Your CSS styles here */
        * {
        box-sizing: border-box;
        padding: 0;
        margin: 0;
        font-family: "Sono", sans-serif;
        font-size: 1.2rem;
        font-size: clamp(0.9rem, 1.5vw, 1.2rem);
        }
        body {
            background-color: white;
        }

        h1 {
            color: white;
        }

        h2 {
            color: white;
        }
        nav ul {
            padding: 10px;
            background-color: #8ac4df;
            list-style: none;
            list-style-type: none;
            list-style: none;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        nav{
            padding: 10px;
            background-color: #8ac4df;
        }

        
        li{
            width: clamp(8rem, 1.5vw, 12rem);
            text-align: center;
        }
        
        #currentValueBox{
            display: flex;
            flex-direction: row;
            justify-content: center;
        }
        #waterDrip{
            width: clamp(1rem, 1.5vw, 20px);
            height: clamp(1rem, 1.5vw, 20px);
            margin-right: 0.5vw;
            margin-top: 0.1vw;
        }
        #currentValue{
            /* text-align: right; */
            vertical-align: middle;
           text-align: center;
        }

        #settings{
            text-align: right;
        }
        
        #time{
            width: fit-content;
            margin: 0;
            font-weight: 500;
            border-radius: 4px;
            padding: 0.5vw 1vw 0.5vw 1vw;
            background-color: #226583;
            color: #def0fa;
            /* font-size: clamp(0.5rem, 1.5vw, 1.2rem); */
            line-height: 12px;
        }
        
    </style>
</head>

<body>
    <nav>
        <ul>
            <li>
                <p id="time">12:56</p>
            </li>
            <li id="currentValueBox">
                <svg id="waterDrip" xmlns="http://www.w3.org/2000/svg" height="15" fill="#3244a8" viewBox="0 -960 960 960" width="15">
                    <path
                        d="M508-218q10-1 17-8.5t7-18.5q0-11-9.5-19.5T501-271q-58 8-103.5-24T343-387q-1-11-9-17.5t-17-6.5q-13 0-21 9t-6 21q11 78 74 126t144 37ZM480-60q-143 0-242.5-99.5T138-404q0-108 83-227.5T480-902q179 158 260.5 276.5T822-404q0 145-99.5 244.5T480-60Z" />
                </svg>
                <p id="currentValue"></p>
            </li>
            <li>
                <p id="settings">Settings</p>
            </li>
        </ul>
    </nav>
    <h1>Hello World!</h1>
    <h2>This is a captive portal example. All requests will be redirected here</h2>
    <p>Distance: <span id="distance">Loading...</span></p>
    <script>
        let currentIndex = 0;
        let testvalue = 110;
        let maxWaterHoogte = 0;
        let totaleAfstandTotBodem = 0;
        const cycleValues = [0, 1, 2];

        const getDistance = async () => {
                try {
                    const response = await fetch('/distance');
                    const data = await response.json();
                    const distanceElement = document.getElementById('distance');
                    distanceElement.innerText = data.distance + ' cm';
                    return data.distance;
                } catch (error) {
                    console.error('Error fetching distance:', error);
                    return null; // or handle the error in an appropriate way
                }
            };


        const getTime = async () => {
            fetch('/time')
                .then(response => response.json())
                .then(data => {
                    const timeElement = document.getElementById('time');
                    timeElement.innerText = data.time;
                })
                .catch(error => console.error('Error fetching time:', error));
        };

        const showCurrentValue = async () => {
            const currentValueElement = document.getElementById('currentValue');
            let specificValue;
            if (cycleValues[currentIndex] == 0) {
                let distance = maxWaterHoogte - (await getDistance()) - 100;
                specificValue = ((maxWaterHoogte + distance) / maxWaterHoogte) * 100;
                specificValue = specificValue.toFixed(2);
                specificValue += '%';
                console.log("percentage vol: " + specificValue)
            }
            if (cycleValues[currentIndex] == 1) {
                specificValue = totaleAfstandTotBodem - await getDistance();
                console.log('waterhoogte' + specificValue)
            }
            if (cycleValues[currentIndex] == 2) {
                specificValue = await getDistance();
                console.log('afstand tot water:' + specificValue)

            }
            currentValueElement.innerText = specificValue;
        };
        
        const cycle = () => {
            currentIndex = (currentIndex + 1) % cycleValues.length;
            showCurrentValue();	
        }

        document.getElementById('currentValueBox').addEventListener('click', cycle);


        const getMaxWaterHoogte = async () => {
            fetch('/maxWaterHoogte')
                .then(response => response.json())
                .then(data => {
                    maxWaterHoogte = data.maxWaterHoogte;
                })
                .catch(error => console.error('Error fetching maxWaterHoogte:', error));
        }  
        const getTotaleAfstandTotBodem = async () => {
            fetch('/totaleAfstandTotBodem')
                .then(response => response.json())
                .then(data => {
                    totaleAfstandTotBodem = data.totaleAfstandTotBodem;
                })
                .catch(error => console.error('Error fetching totaleAfstandTotBodem:', error));
        }  

        const setup = async () => {
            await getMaxWaterHoogte();
            await getTotaleAfstandTotBodem();
        };

        const loop = async () => {
            await showCurrentValue();
            // await getTime();
        };

        setup();
        const interval = setInterval(loop, 1000);
        
    </script>
</body>
</html>
)=====";




















// getters
int getDistance() {
  int distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  int duration = pulseIn(echoPin, HIGH);
  distance = duration / 29 / 2;
  return distance;
}

//handlers
void handleDistance(AsyncWebServerRequest *request)
{
    int distance = getDistance();
    // Create a JSON object to store the temperature data
    StaticJsonDocument<64> jsonDocument;
    jsonDocument["distance"] = distance;
    Serial.print("distance: ");
    Serial.println(distance);
    // Serialize the JSON object to a string
    String jsonResponse;
    serializeJson(jsonDocument, jsonResponse);

    // Send the JSON response to the client
    request->send(200, "application/json", jsonResponse);
}

void handleTotaleAfstandTotBodem(AsyncWebServerRequest *request)
{
    // Create a JSON object to store the temperature data
    StaticJsonDocument<64> jsonDocument;
    jsonDocument["totaleAfstandTotBodem"] = totaleAfstandTotBodem;
    Serial.print("totaleAfstandTotBodem: ");
    Serial.println(totaleAfstandTotBodem);

    // Serialize the JSON object to a string
    String jsonResponse;
    serializeJson(jsonDocument, jsonResponse);

    // Send the JSON response to the client
    request->send(200, "application/json", jsonResponse);
}

void handleMaxWaterHoogte(AsyncWebServerRequest *request)
{
    // Create a JSON object to store the temperature data
    StaticJsonDocument<64> jsonDocument;
    jsonDocument["maxWaterHoogte"] = maxWaterHoogte;
    Serial.print("maxWaterHoogte: ");
    Serial.println(maxWaterHoogte);


    // Serialize the JSON object to a string
    String jsonResponse;
    serializeJson(jsonDocument, jsonResponse);

    // Send the JSON response to the client
    request->send(200, "application/json", jsonResponse);
}

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
// Define the DNS interval in milliseconds between processing DNS requests
#define DNS_INTERVAL 30

	// Set the TTL for DNS response and start the DNS server
	dnsServer.setTTL(3600);
	dnsServer.start(53, "*", localIP);
}

void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP) {
// Define the maximum number of clients that can connect to the server
#define MAX_CLIENTS 4
// Define the WiFi channel to be used (channel 6 in this case)
#define WIFI_CHANNEL 6

	// Set the WiFi mode to access point and station
	WiFi.mode(WIFI_MODE_AP);

	// Define the subnet mask for the WiFi network
	const IPAddress subnetMask(255, 255, 255, 0);

	// Configure the soft access point with a specific IP and subnet mask
	WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

	// Start the soft access point with the given ssid, password, channel, max number of clients
	WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

	// Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
	esp_wifi_stop();
	esp_wifi_deinit();
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
	my_config.ampdu_rx_enable = false;
	esp_wifi_init(&my_config);
	esp_wifi_start();
	vTaskDelay(100 / portTICK_PERIOD_MS);  // Add a small delay
}

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
	//======================== Webserver ========================
	// WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
	// SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
	// SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
	// SAFARI (IOS) popup browser has some severe limitations (javascript disabled, cookies disabled)

	// Required
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								// Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

	// Background responses: Probably not all are Required, but some are. Others might speed things up?
	// A Tier (commonly used by modern systems)
	server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });		   // android captive portal redirect
	server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // microsoft redirect
	server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
	server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });	   // firefox captive portal call home
	server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
	server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // windows call home

	// B Tier (uncommon)
	//  server.on("/chrome-variations/seed",[](AsyncWebServerRequest *request){request->send(200);}); //chrome captive portal call home
	//  server.on("/service/update2/json",[](AsyncWebServerRequest *request){request->send(200);}); //firefox?
	//  server.on("/chat",[](AsyncWebServerRequest *request){request->send(404);}); //No stop asking Whatsapp, there is no internet connection
	//  server.on("/startpage",[](AsyncWebServerRequest *request){request->redirect(localIPURL);});

	// return 404 to webpage icon
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });	// webpage icon

	// Serve Basic HTML Page
	server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
		response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("Page ready to view");
	});

  // Handler for fetching the temperature value from the server
  server.on("/distance", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleDistance(request);
  });
  server.on("/totaleAfstandTotBodem", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleTotaleAfstandTotBodem(request);
  });

  server.on("/maxWaterHoogte", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleMaxWaterHoogte(request);
  });

	// the catch all
	server.onNotFound([](AsyncWebServerRequest *request) {
		request->redirect(localIPURL);
		Serial.print("onnotfound ");
		Serial.print(request->host());	// This gives some insight into whatever was being requested on the serial monitor
		Serial.print(" ");
		Serial.print(request->url());
		Serial.print(" sent redirect to " + localIPURL + "\n");
	});
}

DNSServer dnsServer;
AsyncWebServer server(80);


void setup() {
	// Set the transmit buffer size for the Serial object and start it with a baud rate of 9600.
	// Serial.setTxBufferSize(1024);
	Serial.begin(9600);

	// Wait for the Serial object to become available.
	while (!Serial);

	// Print a welcome message to the Serial port.
	Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER");  //__DATE__ is provided by the platformio ide
	Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

	startSoftAccessPoint(ssid, password, localIP, gatewayIP);

	setUpDNSServer(dnsServer, localIP);

	setUpWebserver(server, localIP);
	server.begin();

  //sonar
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);

  
	Serial.print("\n");
	Serial.print("Startup Time:");	// should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
	Serial.println(millis());
	Serial.print("\n");

}

void loop() {
	dnsServer.processNextRequest();	 // I call this atleast every 10ms in my other projects (can be higher but I haven't tested it for stability)
	delay(1);			 // seems to help with stability, if you are doing other things in the loop this may not be needed
}




