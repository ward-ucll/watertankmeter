// // Your JavaScript code here
// window.onload = function () {
//     // Function to fetch temperature data from the server
//     function fetchTemperature() {
//         fetch('/temperature')
//             .then(response => response.json())
//             .then(data => {
//                 const temperatureElement = document.getElementById('temperature');
//                 temperatureElement.innerText = data.temperature + ' °C';
//             })
//             .catch(error => console.error('Error fetching temperature:', error));
//     }

//     // Fetch temperature data on page load
//     fetchTemperature();

//     // Update temperature every 5 seconds
//     setInterval(fetchTemperature, 5000);
// };
// const getDistance = async () => {
//     fetch('/distance')
//         .then(response => response.json())
//         .then(data => {
//             const distanceElement = document.getElementById('distance');
//             distanceElement.innerText = data.distance + ' °C';
//         })
//         .catch(error => console.error('Error fetching temperature:', error));
// }
// window.onload = getDistance()








const getDeviceTime = () => {
    const time = new Date();
    const huidigeDagVanDeWeek = time.getDay();
    const year = time.getFullYear();
    const month = String(time.getMonth() + 1).padStart(2, '0');
    const day = String(time.getDate()).padStart(2, '0');
    const hours = String(time.getHours()).padStart(2, '0');
    const minutes = String(time.getMinutes()).padStart(2, '0');
    const seconds = String(time.getSeconds()).padStart(2, '0');
    const huidigeDagNaam = dagenVanDeWeek[huidigeDagVanDeWeek];
    return `${year}-${month}-${day} ${huidigeDagNaam} ${hours}:${minutes}:${seconds}`;
} 