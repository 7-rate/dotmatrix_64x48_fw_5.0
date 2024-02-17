#pragma once

void update_weather();

struct mz_weather_t
{
    String weather_mark;
    int temp;
    int wind_10;
    int humidity;
};

extern mz_weather_t mz_weather[3];