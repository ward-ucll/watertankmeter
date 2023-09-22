void handleTemperature(AsyncWebServerRequest *request)
{
    // Read the temperature value from the sensor (replace this with your actual sensor reading)
    float temperature = readTemperatureFromSensor();

    // Create a JSON object to store the temperature data
    StaticJsonDocument<64> jsonDocument;
    jsonDocument["temperature"] = temperature;

    // Serialize the JSON object to a string
    String jsonResponse;
    serializeJson(jsonDocument, jsonResponse);

    // Send the JSON response to the client
    request->send(200, "application/json", jsonResponse);
}