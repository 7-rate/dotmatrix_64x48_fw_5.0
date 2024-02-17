#pragma once
#include <Arduino.h>
#include "settings.h"

typedef enum
{
    WEATHER_STATUS_NORMAL,
    WEATHER_STATUS_WIFI_ERROR,
    WEATHER_STATUS_HTTP_ERROR,
    WEATHER_STATUS_DATA_ERROR,
    WEATHER_STATUS_UNDEFINED_ERROR

} WEATHER_STATUS;

struct weather_t
{
    int condition_code;
    double temp;
    int humidity;
    double wind;
};

struct weather_info_t
{
    WEATHER_STATUS status;
    weather_t weather[3];
};

extern weather_info_t weather_info;

void init_OpenWeatherMap();
void update_OpenWeatherMap();

const String & get_weather_city_name();
const String & get_weather_api_key();