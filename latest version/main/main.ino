// Captive Portal
#include <AsyncTCP.h> //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h> //https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>          //Used for mpdu_rx_disable android workaround
#include "AsyncJson.h"
#include "FS.h"
#include "SD.h"
#include <ArduinoJson.h>
#include <vector>

//SD
int sdCsPin = 5;

// Pre reading on the fundamentals of captive portals https://textslashplain.com/2022/06/24/captive-portals/
const char *ssid = "Waterput"; // FYI The SSID can't have a space in it.
const char *password = NULL;   // no password

#define MAX_CLIENTS 4  // ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6 // 2.4ghz channel 6 https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

const IPAddress localIP(4, 3, 2, 1);          // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2, 1);        // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0); // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "http://4.3.2.1"; // a string version of the local IP with http, used for redirecting clients to your webpage


// sonar
const int trigPin = 26; // Trigger Pin of Ultrasonic Sensor
const int echoPin = 21; // Echo Pin of Ultrasonic Sensor

// rtc module
const int CLK_PIN = 13; // D13 on ESP32
const int DAT_PIN = 12; // D12 on ESP32
const int RST_PIN = 14; // D14 on ESP32
#include <Ds1302.h>
// DS1302 RTC instance
Ds1302 rtc(RST_PIN, CLK_PIN, DAT_PIN);

// specific values
const int maxWaterHoogte = 200;        // in cm
const int totaleAfstandTotBodem = 300; // in cm

char WeekDays[7][12] = {"Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag", "Zondag"};

// webserver

const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="nl">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
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
        color: rgb(0, 0, 0);
      }

      h2 {
        color: rgb(0, 0, 0);
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

      nav {
        padding: 10px;
        background-color: #8ac4df;
      }

      li {
        width: clamp(8rem, 1.5vw, 12rem);
        text-align: center;
      }

      #currentValueBox {
        display: flex;
        flex-direction: row;
        justify-content: center;
      }

      #waterDrip {
        width: clamp(1rem, 1.5vw, 20px);
        height: clamp(1rem, 1.5vw, 20px);
        margin-right: 0.5vw;
        margin-top: 0.1vw;
      }

      #currentValue {
        /* text-align: right; */
        vertical-align: middle;
        text-align: center;
      }

      #settings {
        text-align: right;
      }

      #time {
        width: fit-content;
        margin: 0;
        font-weight: 500;
        border-radius: 4px;
        padding: 0.5vw 1vw 0.5vw 1vw;
        background-color: #226583;
        color: #000000;
        /* font-size: clamp(0.5rem, 1.5vw, 1.2rem); */
        line-height: 12px;
      }

      /* Styles for the popup container */
      .popup-container {
        display: none;
        position: fixed;
        top: 0;
        left: 0;
        height: 100%;
        width: 100%;
        justify-content: center;
        align-items: center;
        z-index: 1;
        transition: opacity 0.3s ease-in-out, transform 0.3s ease-in-out;
        opacity: 0;
        transform: translateY(-100%);
      }

      /* Add styles for the popup content */
      .popup-content {
        background-color: #fff;
        padding: clamp(0.5rem, 1vw, 1rem);
        border-radius: 5px;
        max-width: clamp(20rem, 30vw, 30rem);
      }

      /* Show the popup when it's open */
      .popup-container.active {
        opacity: 1;
        transform: translateY(0);
      }

      /* Add styles for the close button */
      .popup-close {
        cursor: pointer;
        position: relative;
        margin-right: clamp(0.1rem, 2vw, 0.5rem);
        text-align: right;
        font-size: clamp(2rem, 2vw, 2.5rem);
        user-select: none;
      }

      #closeSubMenu {
        cursor: pointer;
        position: relative;
        margin-right: clamp(0.1rem, 2vw, 0.5rem);
        text-align: left;
        font-size: clamp(2rem, 2vw, 2.5rem);
        user-select: none;
        padding: 25px;
      }

      table#timeSyncTable {
        border-collapse: collapse;
        border: 1px solid black;
        width: 100%;
      }

      table#timeSyncTable tr {
        border: 1px solid black;
      }

      table#timeSyncTable td {
        border: 1px solid black;
        padding: 1rem;
      }

      #time {
        color: #fff;
      }

      .popup-content h2 {
        padding: 1.5rem 0 1rem 0;
      }

      .success {
        color: green;
      }

      .failed {
        color: red;
        display: block;
        visibility: visible;
      }

      #underconstruction {
        text-align: center;
      }

      #settings {
        cursor: pointer;
        user-select: none;
      }

      .custom-loader {
        position: relative;
        z-index: 0;
        width: clamp(25px, 3vw, 50px);
        height: clamp(25px, 3vw, 50px);
        border-radius: 50%;
        background: radial-gradient(farthest-side, #226583 94%, #0000)
            top/clamp(4px, 0.5vw, 16px) clamp(4px, 0.5vw, 16px) no-repeat,
          conic-gradient(#0000 30%, #226583);
        -webkit-mask: radial-gradient(
          farthest-side,
          #0000 calc(100% - clamp(4px, 0.5vw, 16px)),
          #000 0
        );
        animation: s3 1s infinite linear;
      }

      @keyframes s3 {
        100% {
          transform: rotate(1turn);
        }
      }

      #syncTime {
        display: flex;
        flex-direction: row;
        justify-content: space-between;
        align-items: center;
        text-align: center;
      }

      #syncButton {
        cursor: pointer;
        user-select: none;
        border: none;
        border-radius: 4px;
        padding: 0.3rem;
        background-color: #8ac4df;
        color: #000000;
        /* font-weight: bold; */
      }

      .dark-mask {
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.5);
        /* Semi-transparent black overlay */
        display: none;
        /* Initially hidden */
      }

      #generalMenu,
      #customizedMenu,
      #infoMenu,
      #taskMenu {
        background-color: rgb(255, 255, 255);
        padding: clamp(0.5rem, 1vw, 1rem);
        border-radius: 5px;
        position: absolute;
        transition: width 0.5s, height 0.5s, top 0.5s, left 0.5s, right 0.5s,
          bottom 0.5s;
      }

      #generalOpen,
      #customizedOpen,
      #infoOpen,
      #taskOpen {
        cursor: pointer;
        user-select: none;
        border: none;
        padding: 0.3rem;
        border: none;
        background-color: none;
        border-radius: 4px;
        background-color: #8ac4df;
        color: #000000;
        /* font-weight: bold; */
      }

      #generalClose,
      #customizedClose,
      #infoClose,
      #taskClose {
        cursor: pointer;
        user-select: none;
        font-size: clamp(1.5rem, 1.5vw, 1.2rem);
      }

      #submenuButtons {
        display: flex;
        gap: clamp(0.5rem, 1vw, 1rem);
        padding: clamp(0.5rem, 1vw, 1rem);
        flex-direction: column;
        justify-content: space-between;
        align-items: center;
        text-align: center;
      }

      .hidden {
        display: none;
      }

      /* progress bar */
      progress {
        opacity: 0;
      }

      .progress-element {
        width: 100%;
        margin: 0 0 10px;
      }

      .progress-container {
        position: relative;
        background: #eee;
        height: 20px;
        border-radius: 6px;
        overflow: hidden;
      }

      .progress-container::before {
        content: "";
        position: absolute;
        top: 0;
        left: 0;
        height: 100%;
        width: 0;
        background: turquoise;
      }

      .progress-element--ram .progress-container::before {
        animation: progress-ram 1s ease-in forwards;
      }

      .progress-element--sd .progress-container::before {
        animation: progress-sd 1s ease-in forwards;
      }

      .progress-element--javascript .progress-container::before {
        animation: progress-javascript 1s ease-in forwards;
      }

      .progress-label {
        position: relative;
      }

      #ram .progress-label::after {
        content: var(--progress-label-content, "");
        position: absolute;
        top: 0;
        right: 0;
      }

      #sd .progress-label::after {
        content: var(--progress-label-content, "");
        position: absolute;
        top: 0;
        right: 0;
      }

      .progress-element--ram .progress-label::after {
        animation: progress-text-ram 1s ease-in forwards;
      }

      .progress-element--sd .progress-label::after {
        animation: progress-text-sd 1s ease-in forwards;
      }

      /* @keyframes progress-ram {
        to {
          width: 0%;
        }
      } */

      #info-container {
        display: flex;
        flex-direction: column;
        justify-content: space-between;
        align-items: center;
        text-align: center;
        padding: clamp(0.5rem, 1vw, 1rem);
        margin-top: clamp(0.5rem, 1.5vw, 0.8rem);
      }

      #infoheader {
        top: clamp(0.9rem, 1.5vw, 1.2rem);
        font-size: clamp(1.5rem, 1.5vw, 1.2rem);
        position: absolute;
      }

      #info-container label {
        font-weight: bolder;
      }

      #info-group {
        display: flex;
        flex-direction: row;
        justify-content: space-between;
        align-items: center;
        text-align: center;
        width: 100%;
        margin-top: clamp(0.5rem, 1.5vw, 0.8rem);
      }

      /* Styles for the loading screen */
      .container {
        position: relative; /* Position relative for proper stacking context */
      }

      .universal-loading {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.7);
        display: flex;
        justify-content: center;
        align-items: center;
        z-index: 9999;
      }

      .universal-loading .loading-spinner {
        border: 4px solid rgba(255, 255, 255, 0.3);
        border-top: 4px solid #3498db;
        border-radius: 50%;
        width: 50px;
        height: 50px;
        animation: spin 1s linear infinite;
      }

      @keyframes spin {
        0% {
          transform: rotate(0deg);
        }
        100% {
          transform: rotate(360deg);
        }
      }

      /* Hide the loading screen when it's not needed */
      .universal-loading.hidden {
        display: none;
      }
    </style>
  </head>

  <body>
    <div class="dark-mask"></div>

    <div id="popupBox" class="popup-container">
      <div class="popup-content">
        <p class="popup-close">&times;</p>
        <h1>Settings</h1>
        <p>This is a simple text-based popup box!</p>
        <!-- slideButton -->
        <div id="submenuButtons">
          <button id="generalOpen">General</button>
          <button id="customizedOpen">Customized</button>
          <button id="infoOpen">Info</button>
          <button id="taskOpen">Taskmanager</button>
        </div>
        <p>
          Lorem ipsum dolor, sit amet consectetur adipisicing elit. Tenetur aut,
          cupiditate eligendi necessitatibus quos at nostrum minus, ullam quod
          corrupti numquam blanditiis ad, ducimus dicta quidem atque eos unde
          maiores. Lorem ipsum dolor Lorem, ipsum dolor sit amet consectetur
          adipisicing elit. Nemo sapiente quaerat aut harum dignissimos ipsam
          provident placeat, debitis omnis magnam sint nulla, sit consectetur
          quia. Est officiis impedit commodi perferendis! Impedit ipsa, odit iis
          accusantium eum cupiditate dolores eos quasi fugit ullam, dolorum
          atque. Nesciunt, temporibus. Odio dol
        </p>
        <div id="generalMenu" class="hidden">
          <p id="generalClose">&#10094;</p>
          <h2 id="syncTime">
            <div>Sync to time esp32</div>
            <button id="syncButton">Sync</button>
          </h2>
          <table id="timeSyncTable">
            <tr>
              <th>Device</th>
              <th>Time</th>
            </tr>
            <tr>
              <td>Current device</td>
              <td id="deviceTimeSetting">
                <div class="custom-loader"></div>
              </td>
            </tr>
            <tr>
              <td>esp32</td>
              <td id="esp32TimeSetting">
                <div class="custom-loader"></div>
              </td>
            </tr>
          </table>
        </div>
        <div id="customizedMenu" class="hidden">
          <p id="customizedClose">&#10094;</p>
        </div>
        <div id="infoMenu" class="hidden">
          <p id="infoClose">&#10094;</p>
          <!-- <div class="custom-loader"></div> -->
          <div id="info-container">
            <h1 id="infoheader">Info</h1>
            <div id="info-group">
              <label for="Chipid">Chip ID:</label>
              <p id="Chipid">loading</p>
            </div>

            <div id="info-group">
              <label for="Flashchipsize">Flash Chip Size:</label>
              <p id="Flashchipsize">loading</p>
            </div>

            <div id="info-group">
              <label for="Cpufrequency">CPU Frequency:</label>
              <p id="Cpufrequency">loading</p>
            </div>

            <div id="info-group">
              <label for="Chiprevision">Chip Revision:</label>
              <p id="Chiprevision">loading</p>
            </div>

            <div id="info-group">
              <label for="Sdkversion">SDK Version:</label>
              <p id="Sdkversion">loading</p>
            </div>

            <div id="info-group">
              <label for="Chipcores">Chip Cores:</label>
              <p id="Chipcores">loading</p>
            </div>

            <div id="info-group">
              <label for="Chipmodel">Chip Model:</label>
              <p id="Chipmodel">loading</p>
            </div>

            <div id="info-group">
              <label for="Flashchipmode">Flash Chip Mode:</label>
              <p id="Flashchipmode">loading</p>
            </div>
            <div id="info-group">
              <label for="Flashchipspeed">Flash Chip Speed:</label>
              <p id="Flashchipspeed">loading</p>
            </div>
            <div id="info-group">
              <label for="Cardtype">Storagetype:</label>
              <p id="Cardtype">loading</p>
            </div>
            <div id="info-group">
              <label for="Cyclecount">Cycle Count:</label>
              <p id="Cyclecount">loading</p>
            </div>
            <div id="info-group">
              <label for="Cardsize">SD card size:</label>
              <p id="Cardsize">loading</p>
            </div>
          </div>
        </div>

        <div id="taskMenu" class="hidden">
          <p id="taskClose">&#10094;</p>

          <div id="taskmanager-loader" class="universal-loading">
            <div class="loading-spinner"></div>
          </div>
          <div id="ram" class="progress-element progress-element--ram">
            <p class="progress-label">RAM:</p>
            <div class="progress-container">
              <progress max="100"></progress>
            </div>
          </div>
          <div id="sd" class="progress-element progress-element--sd">
            <p class="progress-label">Used SD card space:</p>
            <div class="progress-container">
              <progress max="100"></progress>
            </div>
          </div>
        </div>
      </div>
    </div>

    <nav>
      <ul>
        <li>
          <p id="time">00:00</p>
        </li>
        <li id="currentValueBox">
          <svg
            id="waterDrip"
            xmlns="http://www.w3.org/2000/svg"
            height="15"
            fill="#3244a8"
            viewBox="0 -960 960 960"
            width="15"
          >
            <path
              d="M508-218q10-1 17-8.5t7-18.5q0-11-9.5-19.5T501-271q-58 8-103.5-24T343-387q-1-11-9-17.5t-17-6.5q-13 0-21 9t-6 21q11 78 74 126t144 37ZM480-60q-143 0-242.5-99.5T138-404q0-108 83-227.5T480-902q179 158 260.5 276.5T822-404q0 145-99.5 244.5T480-60Z"
            />
          </svg>
          <p id="currentValue"></p>
        </li>
        <li>
          <p id="settings">Settings</p>
        </li>
      </ul>
    </nav>
    <!-- <p>Distance: <span id="distance">Loading...</span></p> -->
    <h1 id="underconstruction">This site is under construction</h1>
    <svg
      xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
      xmlns="http://www.w3.org/2000/svg"
      xmlns:cc="http://web.resource.org/cc/"
      xmlns:dc="http://purl.org/dc/elements/1.1/"
      xmlns:svg="http://www.w3.org/2000/svg"
      id="svg2"
      viewBox="0 0 200 200"
      version="1.0"
    >
      <g id="layer1" transform="translate(-211.88 -270.59)">
        <g id="g25056" stroke="#000">
          <rect
            id="rect1336"
            stroke-linejoin="round"
            ry="0"
            rx="0"
            transform="matrix(.70742 -.70679 .70679 .70742 0 0)"
            height="138.1"
            width="138.09"
            y="413.54"
            x="-110.35"
            stroke-width="4.7022"
            fill="#ff0"
          />
          <g id="g7457" transform="matrix(.44444 0 0 .44444 156.05 236.16)">
            <path
              id="path2224"
              d="m323.64 396.82h134.93c-7.12-19.07-15.71-5.79-25.34-16.79-1.74-21.77-17.36-23.06-17.36-23.06-15.94-14.23-13.36-11.28-21.06-11.39-20.5-0.28-19.88 26.8-36.72 26.19-16.8-0.57-13.1 16.51-34.45 25.05z"
              stroke-width="1px"
              fill-rule="evenodd"
            />
            <path
              id="path3113"
              d="m253.9 386l11.11-48.1 1.28-41.06"
              stroke-width="22"
              stroke-linecap="round"
              fill="none"
            />
            <path
              id="path3988"
              d="m291.76 386l23.06-48.1-23.06-37.29-13.38-9.39"
              stroke-width="22"
              stroke-linecap="round"
              fill="none"
            />
            <path
              id="path4875"
              d="m259.6 288.65l53.94-58.06s1.49-1.85 6.97-1.85l28.18 33.16-50.1 31.31-38.99-4.56z"
              stroke-width="1px"
              fill-rule="evenodd"
            />
            <path
              id="path5758"
              d="m315.25 232.51l-34.87 0.35-14.59 31.24"
              stroke-width="14"
              stroke-linecap="round"
              fill="none"
            />
            <path
              id="path6637"
              d="m318.31 305.45l25.26-39.55"
              stroke-width="14"
              stroke-linecap="round"
              fill="none"
            />
            <path
              id="path1334"
              d="m258.39 252.5l118.48 109.88v0"
              stroke-width="6"
              stroke-linecap="round"
              fill="none"
            />
            <path
              id="path3082"
              stroke-linejoin="round"
              d="m385.7 216.35a17.079 17.079 0 1 1 -34.16 0 17.079 17.079 0 1 1 34.16 0z"
              transform="translate(-14.517 11.671)"
              stroke-linecap="round"
              stroke-width="6"
            />
          </g>
        </g>
      </g>
    </svg>
    <script>
      let currentIndex = 0;
      let testvalue = 110;
      let maxWaterHoogte = 0;
      let totaleAfstandTotBodem = 0;
      const cycleValues = [0, 1, 2];
      const intervalsFromSubmenus = {};

      //time
      const dagenVanDeWeek = [
        "Zondag",
        "Maandag",
        "Dinsdag",
        "Woensdag",
        "Donderdag",
        "Vrijdag",
        "Zaterdag",
      ];

      // Settings
      let SettingUpdateTimes = null;

      const openButton = document.getElementById("settings");
      const popupBox = document.getElementById("popupBox");
      const closeButton = popupBox.querySelector(".popup-close");

      const getDistance = async () => {
        try {
          const response = await fetch("/distance");
          const data = await response.json();
          const distanceElement = document.getElementById("distance");
          distanceElement.innerText = data.distance + " cm";
          return data.distance;
        } catch (error) {
          console.error("Error fetching distance:", error);
          return null; // or handle the error in an appropriate way
        }
      };

      const getEsp32Time = async () => {
        try {
          const response = await fetch("/time");
          const data = await response.json();
          return parseTime(data);
        } catch (error) {
          console.error("Error fetching time:", error);
          return null; // or handle the error in an appropriate way
        }
      };

      const parseTime = (timeObject) => {
        if (Number(timeObject.month) <= 9) {
          timeObject.month = "0" + String(timeObject.month);
        }
        if (Number(timeObject.day) <= 9) {
          timeObject.day = "0" + String(timeObject.day);
        }
        if (Number(timeObject.hour) <= 9) {
          timeObject.hour = "0" + String(timeObject.hour);
        }
        if (Number(timeObject.minute) <= 9) {
          timeObject.minute = "0" + String(timeObject.minute);
        }
        if (Number(timeObject.second) <= 9) {
          timeObject.second = "0" + String(timeObject.second);
        }
        return timeObject;
      };

      const showCurrentValue = async () => {
        const currentValueElement = document.getElementById("currentValue");
        let specificValue;
        if (cycleValues[currentIndex] == 0) {
          let distance = maxWaterHoogte - (await getDistance()) - 100;
          specificValue = ((maxWaterHoogte + distance) / maxWaterHoogte) * 100;
          specificValue = specificValue.toFixed(2);
          specificValue += "%";
          console.log("percentage vol: " + specificValue);
        }
        if (cycleValues[currentIndex] == 1) {
          specificValue = totaleAfstandTotBodem - (await getDistance());
          console.log("waterhoogte" + specificValue);
        }
        if (cycleValues[currentIndex] == 2) {
          specificValue = await getDistance();
          console.log("afstand tot water:" + specificValue);
        }
        currentValueElement.innerText = specificValue;
      };

      const cycle = () => {
        currentIndex = (currentIndex + 1) % cycleValues.length;
        showCurrentValue();
      };

      const getMaxWaterHoogte = async () => {
        fetch("/maxWaterHoogte")
          .then((response) => response.json())
          .then((data) => {
            maxWaterHoogte = data.maxWaterHoogte;
          })
          .catch((error) =>
            console.error("Error fetching maxWaterHoogte:", error)
          );
      };
      const getTotaleAfstandTotBodem = async () => {
        fetch("/totaleAfstandTotBodem")
          .then((response) => response.json())
          .then((data) => {
            totaleAfstandTotBodem = data.totaleAfstandTotBodem;
          })
          .catch((error) =>
            console.error("Error fetching totaleAfstandTotBodem:", error)
          );
      };

      // Function to show the popup
      const showPopup = async () => {
        // Show the popup background
        const darkMask = document.querySelector(".dark-mask");
        darkMask.style.display = "block";

        // Show the popup
        const popupContainer = document.querySelector(".popup-container");
        popupContainer.style.display = "flex"; // Display the container
        setTimeout(() => {
          popupContainer.classList.add("active"); // Add the 'active' class to show the popup
        }, 10);

        document
          .getElementById("syncButton")
          .addEventListener("click", syncTime);
        const deviceTimeSetting = document.getElementById("deviceTimeSetting");
        const esp32TimeSetting = document.getElementById("esp32TimeSetting");

        popupBox.style.display = "flex";
        setTimeout(() => {
          popupBox.style.opacity = 1;
        }, 10);
      };

      // Function to close the popup
      const closePopup = () => {
        clearInterval(SettingUpdateTimes);
        popupBox.style.opacity = 0;

        // Hide the popup
        const popupContainer = document.querySelector(".popup-container");
        popupContainer.classList.remove("active"); // Remove the 'active' class to hide the popup

        setTimeout(() => {
          popupContainer.style.display = "none"; // Hide the container after the animation
          // Hide the popup background
          const darkMask = document.querySelector(".dark-mask");
          darkMask.style.display = "none";
        }, 300);

        setTimeout(() => {
          popupBox.style.display = "none";
        }, 300);
      };

      const setEventlisteners = () => {
        document
          .getElementById("currentValueBox")
          .addEventListener("click", cycle);
        // Event listener for the open button
        openButton.addEventListener("click", showPopup);

        // Event listener for the close button
        closeButton.addEventListener("click", closePopup);

        document
          .getElementById("generalOpen")
          .addEventListener("click", () => slideSubMenu("general"));
        document
          .getElementById("generalClose")
          .addEventListener("click", () => slideSubMenu("general"));
        document
          .getElementById("customizedOpen")
          .addEventListener("click", () => slideSubMenu("customized"));
        document
          .getElementById("customizedClose")
          .addEventListener("click", () => slideSubMenu("customized"));
        document
          .getElementById("infoOpen")
          .addEventListener("click", () => slideSubMenu("info"));
        document
          .getElementById("infoClose")
          .addEventListener("click", () => slideSubMenu("info"));
        document
          .getElementById("taskOpen")
          .addEventListener("click", () => slideSubMenu("task"));
        document
          .getElementById("taskClose")
          .addEventListener("click", () => slideSubMenu("task"));
      };

      const getDeviceTime = () => {
        const time = new Date();
        const huidigeDagVanDeWeek = time.getDay();
        const timeObject = {
          year: time.getFullYear(),
          month: String(time.getMonth() + 1).padStart(2, "0"),
          day: String(time.getDate()).padStart(2, "0"),
          dayOfWeek: dagenVanDeWeek[huidigeDagVanDeWeek],
          hour: String(time.getHours()).padStart(2, "0"),
          minute: String(time.getMinutes()).padStart(2, "0"),
          second: String(time.getSeconds()).padStart(2, "0"),
        };
        return timeObject;
      };

      const syncTime = async () => {
        // Get the sync button element
        const syncButton = document.getElementById("syncButton");
        const postData = JSON.stringify(getDeviceTime());

        fetch("/setEspTime", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: postData,
        }).then((response) => {
          if (response.ok) {
            // Request was successful (status code 2xx)
            console.log("Success sending post:", response.statusText);
            syncButton.className = "success";
            syncButton.innerHTML = "Sync &#10003;";
            return response.text(); // or response.json() if the server responds with JSON
          } else {
            // Request failed (status code is not 2xx)
            console.error(
              "Request failed:",
              response.status,
              response.statusText
            );
            // Handle the case when the response is empty (or invalid JSON)
            syncButton.innerHTML = "Sync &#10060;";
          }
        });
      };

      const progressBarUpdate = (id, value) => {
        const keyframesCSS = `
          @keyframes progress-${id} {
            to {
              width: ${value}%;
            }
          }
          `;
          // Create a <style> element and set its content to the keyframesCSS
          const styleElement = document.createElement("style");
          styleElement.textContent = keyframesCSS;

          // Append the <style> element to the document's <head> to apply the animation
          document.head.appendChild(styleElement);
          const myElement = document.getElementById(`#${id} .progress-label`);
          // myElement.classList.remove("animate-progress");
          // myElement.classList.add("animate-progress");

      }

      const slideSubMenu = (menu) => {
        const submenu = document.getElementById(menu + "Menu");
        const startPositionY = "100%";
        const endPositionY =
          document.querySelector(".popup-content").offsetTop + -1 + "px";
        const blockWidth =
          document.querySelector(".popup-content").offsetWidth + 4 + "px";
        const blockHeight =
          document.querySelector(".popup-content").offsetHeight + 5 + "px";

        submenu.style.top = startPositionY;
        submenu.style.width = blockWidth;
        submenu.style.height = blockHeight;

        // open submenu
        if (submenu.classList.contains("hidden")) {
          submenu.classList.remove("hidden");

          // run the open function for the current menu
          if (menu == "general") {
            openGeneralMenu();
          }
          if (menu == "customized") {
            openCustomizedMenu();
          }
          if (menu == "info") {
            openInfoMenu();
          }
          if (menu == "task") {
            openTaskMenu();
          }

          setTimeout(() => {
            // Set the left property to center the element horizontally
            submenu.style.left = "50%";

            // Calculate the negative margin to center the element properly
            const elementWidth = submenu.offsetWidth;
            const negativeMargin = -elementWidth / 2;

            // Set the margin to center the element horizontally
            submenu.style.marginLeft = negativeMargin + "px";
            submenu.style.top = endPositionY;
          }, 10);

          // close submenu
        } else {
          // run the close function for the current menu
          if (menu == "general") {
            closeGeneralMenu();
          }
          if (menu == "customized") {
            closeCustomizedMenu();
          }
          if (menu == "info") {
            closeInfoMenu();
          }
          if (menu == "task") {
            closeTaskMenu();
          }

          setTimeout(() => {
            submenu.classList.add("hidden");
          }, 500);
        }
      };

      const openGeneralMenu = () => {
        intervalsFromSubmenus.General = setInterval(async () => {
          const DevicetimeObject = getDeviceTime();
          deviceTimeSetting.innerText = `${DevicetimeObject.year}-${DevicetimeObject.month}-${DevicetimeObject.day} ${DevicetimeObject.dayOfWeek} ${DevicetimeObject.hour}:${DevicetimeObject.minute}:${DevicetimeObject.second}`;
          const Esp32timeObject = await getEsp32Time();
          esp32TimeSetting.innerText = `${Esp32timeObject.year}-${Esp32timeObject.month}-${Esp32timeObject.day} ${Esp32timeObject.dayOfWeek} ${Esp32timeObject.hour}:${Esp32timeObject.minute}:${Esp32timeObject.second}`;
        }, 1000);
      };
      const openCustomizedMenu = () => {};
      const openInfoMenu = async () => {
        try {
          const response = await fetch("/info");
          const data = await response.json();
          document.getElementById("Chipid").innerHTML = data.Chipid;
          document.getElementById("Flashchipsize").innerHTML =
            data.Flashchipsize;
          document.getElementById("Cpufrequency").innerHTML = data.Cpufrequency;
          document.getElementById("Chiprevision").innerHTML = data.Chiprevision;
          document.getElementById("Sdkversion").innerHTML = data.Sdkversion;
          document.getElementById("Chipcores").innerHTML = data.Chipcores;
          document.getElementById("Chipmodel").innerHTML = data.Chipmodel;
          document.getElementById("Flashchipmode").innerHTML =
            data.Flashchip_mode;
          document.getElementById("Flashchipspeed").innerHTML =
            data.Flashchip_speed;
          document.getElementById("Flashchipsize").innerHTML =
            data.Flashchip_size;
          document.getElementById("Cardtype").innerHTML = data.Cardtype;
          document.getElementById("Cyclecount").innerHTML = data.Cycle_count;
          document.getElementById("Cardsize").innerHTML =
            String(data.Cardsize) + " MB";
        } catch (error) {
          console.error("Error fetching info:", error);
          return null; // or handle the error in an appropriate way
        }
      };
      const openTaskMenu = async () => {
        const progressLabel_ram = document.querySelector(
          "#ram .progress-label"
        );
        const progressLabel_sd = document.querySelector("#sd .progress-label");
        intervalsFromSubmenus.taskInterval = setInterval(async () => {
          try {
            const response = await fetch("/taskmanager");
            const data = await response.json();
            const usedRam = data.totalRam - data.ramAvailable;
            const usedRamPercentage = ((usedRam / data.totalRam) * 100).toFixed(2);
            progressBarUpdate("ram", usedRamPercentage);
            const message1 = `${usedRamPercentage}%`;
            progressLabel_ram.style.setProperty(
            "--progress-label-content",
            "'" + message1 + "'"
            );
            const usedSdPercentage = ((data.SdUsedSpace / data.SdTotalSpace) * 100).toFixed(2);
            progressBarUpdate("sd", usedSdPercentage);
            const message2 = `${usedSdPercentage}%`;
            progressLabel_ram.style.setProperty(
            "--progress-label-content",
            "'" + message2 + "'"
            );

            hideLoadingScreen("taskmanager-loader");
          // Create a CSS class with the animation properties
          // const animationClass = document.createElement("style");
          // animationClass.textContent = `
          //   .animate-progress {
          //     animation-name: progress-ram;
          //     animation-duration: 2s;
          //     animation-timing-function: linear;
          //     animation-fill-mode: forwards;
          //   }
          // `;

          // // Append the CSS class to the document's <head>
          // document.head.appendChild(animationClass);

          // Add the animation class to the element (e.g., with the ID "myElement")

          // const root = document.documentElement;

          // // Change the value of the CSS variable
          // root.style.setProperty('--progress-ram-contend', '90%');

          // const element = document.getElementById('ram');

          // // Clone the element to remove and reinsert it, retriggering the animation
          // const cloneElement = element.cloneNode(true);
          // element.parentNode.replaceChild(cloneElement, element);

          // progressLabel_sd.style.setProperty(
          //     "--progress-label-content",
          //     "'old'"
          //   );
          } catch (error) {
            console.log("Error fetching taskmanager:", error);
            return null; // or handle the error in an appropriate way
          }
        }, 1000);
      };

      const showLoadingScreen = (id) => {
        const loadingScreen = document.getElementById(id);
        loadingScreen.classList.remove("hidden");
      };

      const hideLoadingScreen = (id) => {
        const loadingScreen = document.getElementById(id);
        loadingScreen.classList.add("hidden");
      };

      const closeGeneralMenu = () => {
        clearInterval(intervalsFromSubmenus.General);
        console.log(intervalsFromSubmenus.General);
      };
      const closeCustomizedMenu = () => {};
      const closeInfoMenu = () => {};
      const closeTaskMenu = () => {};

      const updateTimeOnHome = async () => {
        const timeObject = await getEsp32Time();
        const timeElement = document.getElementById("time");
        timeElement.innerText = `${timeObject.hour}:${timeObject.minute}`;
      };

      const setup = async () => {
        setEventlisteners();
        await getMaxWaterHoogte();
        await getTotaleAfstandTotBodem();
      };

      const loop = async () => {
        await showCurrentValue();
        await updateTimeOnHome();
      };

      setup();
      const interval = setInterval(loop, 1000);
    </script>
  </body>
</html>
)=====";




// other functions
//      sd card
std::vector<String> listDir(fs::FS &fs, String dirname, uint8_t levels){
  std::vector<String> dirList;
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return dirList;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return dirList;
  }


  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      dirList.push_back("<D>" + String(file.name()));
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
      dirList.push_back(String(file.name()));

    }
    file = root.openNextFile();
  }
  return dirList;
}

void createDir(fs::FS &fs, String path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, String path, String message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}


void createDirIfnotFound(String name, String path) {
    boolean found = false;
    std::vector<String> List = listDir(SD, path, 0);
    for (const String item: List) {
      if (item.charAt(0) == '<' && item.charAt(1) == 'D' && item.charAt(2) == '>' && item.substring(3) == name) {
        found = true;
        Serial.print("Found ");
        Serial.print(name);
        Serial.println(" directory");
        break;
      }
    }
    if (!found) {
      Serial.print("Directory ");
      Serial.print(name);
      Serial.print(" not found in ");
      Serial.print(path);
      Serial.print(" => ");
      createDir(SD, path + name);
    }
}
void createFileIfnotFound(String name, String path) {
    boolean found = false;
    std::vector<String> List = listDir(SD, path, 0);
    for (const String item: List) {
      if (item == name) {
        found = true;
        Serial.print("Found ");
        Serial.print(name);
        Serial.println(" file");
        break;
      }
    }
    if (!found) {
      Serial.print("File ");
      Serial.print(name);
      Serial.print(" not found in ");
      Serial.print(path);
      Serial.print(" => ");
      writeFile(SD, path + "/" + name, " ");
    }
}

void makeSettingsfileIfNotFound() {
  createDirIfnotFound("Settings", "/");
  createFileIfnotFound("Settings.json", "/Settings");
}

DynamicJsonDocument getHardwareInfo() {
  // Create a JSON document with enough capacity
  DynamicJsonDocument doc(512);

    // Add hardware information to the JSON document
  JsonObject root = doc.to<JsonObject>();
  root["Chipid"] = ESP.getEfuseMac();
  root["Flashchipsize"] = ESP.getFlashChipSize();
  root["Cpufrequency"] = getCpuFrequencyMhz();
  root["Chiprevision"] = ESP.getChipRevision();
  root["Sdkversion"] = ESP.getSdkVersion();
  root["Chipcores"] = ESP.getChipCores();
  root["Chipmodel"] = ESP.getChipModel();
  root["Flashchip_mode"] = ESP.getFlashChipMode();
  root["Flashchip_speed"] = ESP.getFlashChipSpeed();
  root["Flashchip_size"] = ESP.getFlashChipSize();
  root["Cycle_count"] = ESP.getCycleCount();
  root["Cardsize"] = int(SD.cardSize() / (1024.0 * 1024.0));

  
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_MMC){
      root["Cardtype"] = "MMC" ;
    } else if(cardType == CARD_SD){
      root["Cardtype"] = "SDSC";
    } else if(cardType == CARD_SDHC){
      root["Cardtype"] = "SDHC";
    } else {
      root["Cardtype"] = "UNKNOWN";
    }
  


  // Serialize the JSON document to a string and return it
  return root;
}

DynamicJsonDocument getTaskmanager() {

  DynamicJsonDocument doc(512);

    // Add hardware information to the JSON document
  JsonObject root = doc.to<JsonObject>();
  root["totalRam"] = int(ESP.getHeapSize());
  root["ramAvailable"] = int(ESP.getFreeHeap());
  root["SdTotalSpace"] = int(SD.totalBytes() / (1024.0 * 1024.0));
  root["SdUsedSpace"] = int(SD.usedBytes() / (1024.0 * 1024.0));
  root["SketchSize"] = ESP.getSketchSize();
  root["FreesketchSpace"] = ESP.getFreeSketchSpace();

  return doc;
}

void printDirList(const std::vector<String>& dirList) {
    for (const String& item : dirList) {
        Serial.print("Item: ");
        Serial.println(item);
    }
}

String getSdStatusAndmount() {
  if(!SD.begin(sdCsPin)){
    return "Card Mount Failed";
  }

  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    return "No SD card attached";
  }

  return "Card mounted successfully";
} 

// DynamicJsonDocument getSdCardInfo() {
//   // Create a JSON object
//   DynamicJsonDocument jsonDoc(200); // Adjust the size as needed

//   // Add data to the JSON object
  

//   return jsonDoc;
// }

//      rtc clok module
Ds1302::DOW convertToDs1302Dow(int dayNumber)
{
    switch (dayNumber)
    {
    case 1:
        return Ds1302::DOW_MON; // Monday
    case 2:
        return Ds1302::DOW_TUE; // Tuesday
    case 3:
        return Ds1302::DOW_WED; // Wednesday
    case 4:
        return Ds1302::DOW_THU; // Thursday
    case 5:
        return Ds1302::DOW_FRI; // Friday
    case 6:
        return Ds1302::DOW_SAT; // Saturday
    case 7:
        return Ds1302::DOW_SUN; // Sunday
    default:
        return Ds1302::DOW_MON; // Default to Monday if the input is out of range
    }
}

// getters
int getDistance()
{
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

// handlers
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

void handleInfo(AsyncWebServerRequest *request){
   String jsonResponse;
   serializeJson(getHardwareInfo(), jsonResponse);

   Serial.print("Get /info: ");
   Serial.println(jsonResponse);

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

void handleGetTime(AsyncWebServerRequest *request)
{
    Ds1302::DateTime now;
    rtc.getDateTime(&now);
    StaticJsonDocument<200> doc;

    doc["year"] = 2000 + now.year;
    doc["month"] = now.month;
    doc["day"] = now.day;
    doc["dayOfWeek"] = WeekDays[now.dow - 1];
    doc["hour"] = now.hour;
    doc["minute"] = now.minute;
    doc["second"] = now.second;

    String jsonResponse;
    serializeJson(doc, jsonResponse);

    Serial.print("Get /time data: ");
    Serial.println(jsonResponse);

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

void handleTaskmanager(AsyncWebServerRequest *request) {
    String jsonResponse;
    serializeJson(getTaskmanager(), jsonResponse);

    // Send the JSON response to the client
    request->send(200, "application/json", jsonResponse);
}

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP)
{
// Define the DNS interval in milliseconds between processing DNS requests
#define DNS_INTERVAL 30

    // Set the TTL for DNS response and start the DNS server
    dnsServer.setTTL(3600);
    dnsServer.start(53, "*", localIP);
}

void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP)
{
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
    vTaskDelay(100 / portTICK_PERIOD_MS); // Add a small delay
}

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP)
{
    //======================== Webserver ========================
    // WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
    // SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
    // SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
    // SAFARI (IOS) popup browser has some severe limitations (javascript disabled, cookies disabled)

    // Required
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request)
              { request->redirect("http://logout.net"); }); // windows 11 captive portal workaround
    server.on("/wpad.dat", [](AsyncWebServerRequest *request)
              { request->send(404); }); // Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

    // Background responses: Probably not all are Required, but some are. Others might speed things up?
    // A Tier (commonly used by modern systems)
    server.on("/generate_204", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // android captive portal redirect
    server.on("/redirect", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // microsoft redirect
    server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // apple call home
    server.on("/canonical.html", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // firefox captive portal call home
    server.on("/success.txt", [](AsyncWebServerRequest *request)
              { request->send(200); }); // firefox captive portal call home
    server.on("/ncsi.txt", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // windows call home

    // B Tier (uncommon)
    //  server.on("/chrome-variations/seed",[](AsyncWebServerRequest *request){request->send(200);}); //chrome captive portal call home
    //  server.on("/service/update2/json",[](AsyncWebServerRequest *request){request->send(200);}); //firefox?
    //  server.on("/chat",[](AsyncWebServerRequest *request){request->send(404);}); //No stop asking Whatsapp, there is no internet connection
    //  server.on("/startpage",[](AsyncWebServerRequest *request){request->redirect(localIPURL);});

    // return 404 to webpage icon
    server.on("/favicon.ico", [](AsyncWebServerRequest *request)
              { request->send(404); }); // webpage icon

    // Serve Basic HTML Page
    server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request)
              {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
		response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("Page ready to view"); });

    // Handler for fetching the temperature value from the server
    server.on("/distance", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleDistance(request); });

    server.on("/totaleAfstandTotBodem", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleTotaleAfstandTotBodem(request); });

    server.on("/maxWaterHoogte", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleMaxWaterHoogte(request); });

    server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleGetTime(request); });

    server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleInfo(request); });
    server.on("/taskmanager", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleTaskmanager(request); });

    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/setEspTime", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                           {
  StaticJsonDocument<200> data;
  if (json.is<JsonArray>())
  {
    data = json.as<JsonArray>();
  }
  else if (json.is<JsonObject>())
  {
    data = json.as<JsonObject>();
  }
  String response;
  serializeJson(data, response);

  const String dataString = response;

  // Create a JSON document
  StaticJsonDocument<200> jsonDocument; // Adjust the size as needed

  // Deserialize the JSON string into the JSON document
  deserializeJson(jsonDocument, dataString);

  // Access the data and convert them to the appropriate types
int year = jsonDocument["year"].as<int>();
int month = jsonDocument["month"].as<int>(); // Convert month to an integer
int day = jsonDocument["day"].as<int>(); // Convert day to an integer
int dayOfWeek = jsonDocument["dayOfWeek"];
int hour = jsonDocument["hour"].as<int>(); // Convert hour to an integer
int minute = jsonDocument["minute"].as<int>(); // Convert minute to an integer
int second = jsonDocument["second"].as<int>(); // Convert second to an integer

// Now you can use the variables year, month, day, dayOfWeek, hour, minute, and second in your Arduino sketch
Ds1302::DOW dayConstant = convertToDs1302Dow(dayOfWeek);

Ds1302::DateTime dt = {
    .year = static_cast<uint16_t>(year - 2000),    // Assuming year can be safely cast to uint16_t
    .month = static_cast<uint8_t>(month),   // Assuming month can be safely cast to uint8_t
    .day = static_cast<uint8_t>(day),       // Assuming day can be safely cast to uint8_t
    .hour = static_cast<uint8_t>(hour),     // Assuming hour can be safely cast to uint8_t
    .minute = static_cast<uint8_t>(minute), // Assuming minute can be safely cast to uint8_t
    .second = static_cast<uint8_t>(second), // Assuming second can be safely cast to uint8_t
    .dow = dayConstant
};

  rtc.setDateTime(&dt);

  request->send(200, "application/json", "Json handled on server");
  
  Serial.print("Post data: ");
  Serial.println(response); });
    server.addHandler(handler);
}

DNSServer dnsServer;
AsyncWebServer server(80);

void setup()
{
    // Set the transmit buffer size for the Serial object and start it with a baud rate of 9600.
    // Serial.setTxBufferSize(1024);
    Serial.begin(115200);

    // Wait for the Serial object to become available.
    while (!Serial)
        ;
    delay(2000);

    Serial.println("setup");
  

    //SD
    if (!SD.begin(5)) {
    Serial.println("SD card initialization failed.");
    return;
    }
    // Serial.println(getSdStatusAndmount());


    printDirList(listDir(SD, "/", 0));
    makeSettingsfileIfNotFound();

    
    // rtc module
    // initialize the RTC
    rtc.init();

    // test if clock is halted and set a date-time (see example 2) to start it
    if (rtc.isHalted())
    {
        Serial.println("RTC is halted. Setting time...");

        Ds1302::DateTime dt = {
            .year = 20,
            .month = Ds1302::MONTH_OCT,
            .day = 1,
            .hour = 1,
            .minute = 1,
            .second = 1,
            .dow = Ds1302::DOW_TUE};

        rtc.setDateTime(&dt);
    }
    // Print a welcome message to the Serial port.
    Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER"); //__DATE__ is provided by the platformio ide
    Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

    

    startSoftAccessPoint(ssid, password, localIP, gatewayIP);

    setUpDNSServer(dnsServer, localIP);

    setUpWebserver(server, localIP);
    server.begin();




    // sonar
    pinMode(echoPin, INPUT);
    pinMode(trigPin, OUTPUT);

    Serial.print("\n");
    Serial.print("Startup Time:"); // should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
    Serial.println(millis());
    Serial.print("\n");
}



void loop()
{
    dnsServer.processNextRequest(); // I call this atleast every 10ms in my other projects (can be higher but I haven't tested it for stability)

    delay(100);                       // seems to help with stability, if you are doing other things in the loop this may not be needed
}
