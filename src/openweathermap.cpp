#include "openweathermap.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "settings.h"
#include "config.h"

weather_info_t weather_info = {WEATHER_STATUS_UNDEFINED_ERROR};
const String site_path = "http://api.openweathermap.org/data/2.5/forecast?q=";
static String city_name = OPEN_WEATHER_MAP_CITY_NAME;
static String api_key = OPEN_WEATHER_MAP_API_KEY;

void init_OpenWeatherMap()
{
	//settings_read(F("ui_weather_city_name"), city_name);
	//settings_read(F("ui_weather_api_key"), api_key);
}

void update_OpenWeatherMap()
{
    if ((WiFi.status() == WL_CONNECTED))
    {
        HTTPClient http;
        String request_api = site_path + city_name + ",jp" 
                        + "&cnt=3"
                        + "&units=metric"
                        + "&appid=" + api_key;
        http.begin(request_api);
        int code = http.GET();

        if ( code > 0 )
        {
            String payload = http.getString();
            DynamicJsonDocument json_doc(4096);
            DeserializationError de = deserializeJson(json_doc, payload);
            if ( de == DeserializationError::Ok )
            {
                for (int i = 0; i < 3; i++)
                {
                    weather_info.weather[i].condition_code = json_doc["list"][i]["weather"][0]["id"].as<int>();
                    weather_info.weather[i].temp = json_doc["list"][i]["main"]["temp"].as<double>();
                    weather_info.weather[i].humidity = json_doc["list"][i]["main"]["humidity"].as<int>();
                    weather_info.weather[i].wind = json_doc["list"][i]["wind"]["speed"].as<double>();
                }
                weather_info.status = WEATHER_STATUS_NORMAL;
            }
            else
            {
                printf("openweathermap:DeserializationError\r\n");
                weather_info.status = WEATHER_STATUS_DATA_ERROR;
            }
        }
        else
        {
            printf("openweathermap:http request error code %d\r\n", code);
            weather_info.status = WEATHER_STATUS_HTTP_ERROR;
        }
        http.end();
    }
    else
    {
        printf("openweathermap:WiFi not connected\r\n");
        weather_info.status = WEATHER_STATUS_WIFI_ERROR;
    }

    return;
}

const String & get_weather_city_name()
{
    return city_name;
}

const String & get_weather_api_key()
{
    return api_key;
}
