#include <Arduino.h>
#include <vector>
#include "pendulum.h"
#include "interval.h"
#include <time.h>
#include "mz_weather.h"


int tm_min_old;
void check_weather()
{
    struct tm tm;
    time_t timeval;
    (void)time(&timeval);
    (void)localtime_r(&timeval, &tm);
    if ((tm.tm_min / 10) != (tm_min_old / 10))
    {
        update_weather();
    }
    tm_min_old = tm.tm_min;
}


/**
 * pendulum scheduling class
 * */
class pendulum_scheduler_t
{
	std::vector<pendulum_t *> pendulums; // array of pendulums
public:
	/**
	 * check all pendulum interval and fire event if the period is reached.
	 * this method should be called 1ms intervally.
	 * */
	void check()
	{
		size_t i;
		// scan for all pendulums for the timeout.
		// this is not optimal. but it works well if the total timer consumer
		// is small.
		uint32_t tick = millis();
		for(i = 0; i < pendulums.size(); ++i)
		{
			pendulum_t * pen = pendulums[i];
			// always use [] operator; not iterator.
			// because the array will be changed during pen->check()
			if(pen) pen->check(tick);
		} 
	}

protected:
	/**
	 * register pendulum instance.
	 * */
	void add(pendulum_t * pendulum)
	{
//		printf("pendulum add : %p\n", pendulum);
		// search for nullptr slot
		for(auto && v : pendulums)
		{
			if(v == nullptr) v = pendulum;
			return;
		}
		// nullptr slot not found; append one
		pendulums.push_back(pendulum);
	}

	/**
	 * remove pendulum instance
	 * */
	void remove(pendulum_t *pendulum)
	{
//		printf("pendulum remove : %p\n", pendulum);
		// search for the pendulum;
		// if found, do not erase, instead, put nullptr;
		// because the instance will be removed during the iteration.
		for(auto && v : pendulums)
		{
			if(v == pendulum)
			{
				v = nullptr;
				return;
			}
		}
	}

	friend class pendulum_t;

};
static pendulum_scheduler_t pendulum_scheduler;


void poll_pendulum()
{
	EVERY_MS(1)
	{
		pendulum_scheduler.check();
		check_weather();
	}
	END_EVERY_MS
}

pendulum_t::pendulum_t(pendulum_t::callback_t _callback, uint32_t _interval_ms) :
	 interval(_interval_ms), next_tick(millis() + _interval_ms), callback(_callback)
{
	pendulum_scheduler.add(this);
}

pendulum_t::~pendulum_t()
{
	pendulum_scheduler.remove(this);
}

void pendulum_t::check(uint32_t tick)
{
//	printf("pendulum: id:%p interval:%lu, next_tick:%lu, tick:%lu\n",
//		this, (unsigned long)interval, (unsigned long)next_tick, (unsigned long)tick);
	if((int32_t)(next_tick - tick) <= 0)
	{
		// timed out
		callback();
		next_tick += interval;
		while((int32_t)(next_tick - tick) <= 0) next_tick += interval; // handles next tick has already been past
	}
}

