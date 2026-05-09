#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0

#include <WiFiS3.h>
#include <Arduino_JSON.h>
#include <ArduinoHttpClient.h>
#include <FastLED.h>

#define LED_PIN 9           // Arduino pin connected to LED strip data (green wire)
#define NUM_LEDS 60         // Your strip has 60 LEDs
#define COLOR_ORDER GRB     // Many WS2812 strips use GRB
#define LED_TYPE WS2812B    // Adjust if different

CRGB leds[NUM_LEDS];


// ===== WiFi credentials =====
const char* ssid = "*****";
const char* password = "*****";

// API credentials and parameters
const char* server = "www.widgeter.io";
int port = 443; // HTTPS
const char* host = "www.widgeter.io";
const char* path = "/api/control/v1.1/index.php";
const char* userId = "**********";
const char* apiKey = "********************************";
const char* apiSecret = "****************************************************************";
const char* deviceId = "**********";
const char* widgetId = "******";
const char* statusId = "1";


String currentColor = "#000000";
unsigned long lastApiCall = 0;
const unsigned long API_INTERVAL = 500;

WiFiSSLClient wifi;
HttpClient client = HttpClient(wifi, server, port);

void setup() {
  // Serial.begin(9600);
  while (!Serial);

  // Connect to WiFi
  connectToWiFi();
  // Serial.println("R4 WiFi LED Controller Ready");

  // Initialize LED strip
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(64); // Start with ~25% brightness for safety
  FastLED.setMaxRefreshRate(400);

}


void loop() {
  if (millis() - lastApiCall >= API_INTERVAL) {
    String newColor = getColorFromAPI();
    if (newColor != "" && newColor != currentColor) {
      currentColor = newColor;
      // Serial.println("New color received: " + currentColor);
      sendColorToLEDs(currentColor);

    }
    lastApiCall = millis();
  }
}

void connectToWiFi() {
  // Serial.print("Connecting to WiFi: ");
  // Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    // delay(1000);
    // Serial.print(".");
  }
  
  // Serial.println("\nConnected to WiFi!");
  // Serial.print("IP Address: ");
  // Serial.println(WiFi.localIP());
}

String urlEncode(String str) {
  String encoded = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str[i];
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || 
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      encoded += '%';
      char hex[3];
      sprintf(hex, "%02X", (unsigned char)c);
      encoded += hex;
    }
  }
  return encoded;
}

String buildQueryString() {
  String controlsJson = "{\"" + String(deviceId) + "\":[\"" + String(widgetId) + "\"]}";
  String encodedControls = urlEncode(controlsJson);
  
  String statusIdsJson = "[\"" + String(statusId) + "\"]";
  String encodedStatusIds = urlEncode(statusIdsJson);
  
  String query = "?user_id=" + String(userId) + 
                 "&api_key=" + String(apiKey) + 
                 "&api_secret=" + String(apiSecret) + 
                 "&type=widgets" + 
                 "&controls=" + encodedControls + 
                 "&status_ids=" + encodedStatusIds;
  
  // Serial.println("Query string: " + query);
  return query;
}

String getColorFromAPI() {
  String colorValue = "";
  
  String queryString = buildQueryString();
  String fullPath = String(path) + queryString;
  
  // Serial.println("Making HTTPS request with HttpClient...");
  // Serial.println("GET " + fullPath);
  
  // Make GET request
  client.get(fullPath);
  
  // Read the status code
  int statusCode = client.responseStatusCode();
  // Serial.println("Status code: " + String(statusCode));
  
  if (statusCode == 200) {
    String response = client.responseBody();
    // Serial.println("Response: " + response);
    colorValue = parseColorFromResponse(response);
  } else {
    // Serial.println("GET request failed");
  }
  
  return colorValue;
}

String parseColorFromResponse(String response) {
  if (response == "No Updates Found") {
    // Serial.println("API returned: No Updates Found");
    return "";
  }
  
  // Look for JSON data
  int jsonStart = response.indexOf('[');
  if (jsonStart == -1) {
    jsonStart = response.indexOf('{');
  }
  
  if (jsonStart != -1) {
    int jsonEnd = response.lastIndexOf(']');
    if (jsonEnd == -1) {
      jsonEnd = response.lastIndexOf('}');
    }
    
    if (jsonEnd != -1 && jsonEnd > jsonStart) {
      String jsonResponse = response.substring(jsonStart, jsonEnd + 1);
      // Serial.println("Extracted JSON: " + jsonResponse);
      
      // Parse JSON
      JSONVar jsonArray = JSON.parse(jsonResponse);
      
      if (JSON.typeof(jsonArray) == "array" && jsonArray.length() > 0) {
        JSONVar firstItem = jsonArray[0];
        
        if (JSON.typeof(firstItem) == "object" && firstItem.hasOwnProperty("value")) {
          String value = (const char*) firstItem["value"];
          // Serial.println("Found value: " + value);
          
          if (isValidHexColor(value)) {
            return value;
          }
          
          if (value.indexOf("#") != -1) {
            int start = value.indexOf("#");
            if (start + 7 <= value.length()) {
              String potentialColor = value.substring(start, start + 7);
              if (isValidHexColor(potentialColor)) {
                return potentialColor;
              }
            }
          }
        }
      }
    }
  }
  
  return "";
}

bool isValidHexColor(String color) {
  if (color.length() != 7 || color[0] != '#') {
    return false;
  }
  
  for (int i = 1; i < 7; i++) {
    char c = color[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

void sendColorToLEDs(String hexColor) {
  int r = hexToInt(hexColor.substring(1, 3));
  int g = hexToInt(hexColor.substring(3, 5));
  int b = hexToInt(hexColor.substring(5, 7));

  // Serial.print("Setting color: ");
  // Serial.print(hexColor);
  // Serial.print(" -> RGB(");
  // Serial.print(r); Serial.print(", ");
  // Serial.print(g); Serial.print(", ");
  // Serial.print(b); Serial.println(")");

  fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
  FastLED.show();
}


int hexToInt(String hexString) {
  char hex[3];
  hexString.toCharArray(hex, 3);
  return (int) strtol(hex, NULL, 16);
}