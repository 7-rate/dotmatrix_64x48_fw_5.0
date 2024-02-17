#include <Arduino.h>
#include "openweathermap.h"
#include "mz_weather.h"
#include "pendulum.h"

mz_weather_t mz_weather[3];

/* condition_code                                                  */
/* mean:https://openweathermap.org/weather-conditions              */
/* Some are not supported because there is no corresponding mark.  */
/* e.g. condition_code=800 → ☀                                    */
static String condition_code_to_weather_mark(int condition_code)
{
    int code = condition_code / 100;
    String weather_mark;

    switch(code)
    {
        case 5: /* Rain */
            weather_mark = "☂";
            break;

        case 6: /* Snow */
            weather_mark = "☃";
            break;

        case 8: /* Clear or Clouds*/
            if ( condition_code == 800 ) weather_mark = "☀";
            else weather_mark = "☁";
            break;

        case 2: /* Thunderstorm */
        case 3: /* Drizzle */
        case 7: /* Atmosphere */
        default:
            weather_mark = "　";
            break;
    }

    return weather_mark;
}

void update_weather()
{
    update_OpenWeatherMap();

    if ( weather_info.status == WEATHER_STATUS_NORMAL )
    {
        for ( int i = 0; i < 3; i++ )
        {
            mz_weather[i].weather_mark = condition_code_to_weather_mark(weather_info.weather[i].condition_code);
            mz_weather[i].temp = (int)(weather_info.weather[i].temp);
            mz_weather[i].humidity = weather_info.weather[i].humidity;
            mz_weather[i].wind_10 = (int)(weather_info.weather[i].wind * 10);
        }
    }
    else
    {
        for ( int i = 0; i < 3; i++ )
        {
            mz_weather[i].weather_mark = "　";
            mz_weather[i].temp = 0;
            mz_weather[i].humidity = 0;
            mz_weather[i].wind_10 = 0;
        }
    }

    return;
}
