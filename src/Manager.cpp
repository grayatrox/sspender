#include "Manager.h"

void Manager::setWhatToMonitor(bool suspendIfCpuIdle, bool suspendIfStorageIdle)
{
	m_suspendIfCpuIdle = suspendIfCpuIdle;
	m_suspendIfStorageIdle = suspendIfStorageIdle;
}

void Manager::setIpsToWatch(const vector<string> &ipToWatch)
{
	for(size_t i = 0, size = ipToWatch.size(); i < size; ++i)
	{
		m_ipsToWatch.push_back(ipToWatch[i]);
	}
}

void Manager::setDisksToMonitor(const vector<string> &disksToMonitor)
{
	for(size_t i = 0, size = disksToMonitor.size(); i < size; ++i)
	{
		m_disksToMonitor.push_back(disksToMonitor[i]);
	}
}

void Manager::setCpusToMonitor()
{
	string cpu = "cpu";

	m_cpusToMonitor.push_back(cpu);
}

void Manager::setTimesToWakeAt(const vector<string> &wakeAt)
{
	for(size_t i = 0, size = wakeAt.size(); i < size; ++i)
	{
		m_timesToWakeAt.push_back(wakeAt[i]);
	}
}

void Manager::setSleepMode(SLEEP_MODE sleepMode)
{
	m_sleepMode = sleepMode;
}

void Manager::setTimers(int check_if_idle_every,
				   	    int stop_monitoring_for,
				   	    int reset_monitoring_after,
				   	    int suspend_after)
{
	m_checkIfIdleEvery     = check_if_idle_every;
	m_stopMonitoringFor    = stop_monitoring_for;
	m_resetMonitoringAfter = reset_monitoring_after;
	m_suspendAfter         = suspend_after;
}

void Manager::watchSystem()
{
	int idleTimer = 0;
	int notIdleTimer = 0;

	m_monitor.monitorSystemUsage(m_disksToMonitor, m_cpusToMonitor);

	while(true)
	{
		printHeaderMessage("Checking if clients are online", true);

		bool stayOnline = m_monitor.areClientsConnected(m_ipsToWatch);

		if(stayOnline)
		{
			cout << "Found clients online, will stop monitoring for "
				 << m_stopMonitoringFor
				 << " mins." << endl;

			idleTimer = 0;
			notIdleTimer = 0;

			sleep(m_stopMonitoringFor * 60);

			continue;
		}

		printHeaderMessage("Checking if system is idle", true);

		bool isIdle = isSystemIdle();

		if(isIdle)
		{
			cout << "System is idle (" << idleTimer << ")." << endl;

			notIdleTimer = 0;
			++idleTimer;
		}
		else
		{
			cout << "System is not idle (" << notIdleTimer << ")." << endl;

			++notIdleTimer;

			//if system is busy for # minutes
			if( (notIdleTimer * m_checkIfIdleEvery) > m_resetMonitoringAfter)
			{
				cout << "System was busy for more than " << m_resetMonitoringAfter
					 << " mins, reseting idle timer." << endl;

				idleTimer = 0;
				notIdleTimer = 0;
			}
		}

		//if idle for # minuts
		if( (idleTimer * m_checkIfIdleEvery) > m_suspendAfter)
		{
	        cout << "system was idle for more than "
				 << m_suspendAfter
				 << " mins, will suspend the machine." << endl;

			idleTimer = 0;
			notIdleTimer = 0;

			printHeaderMessage("Suspending system", true);

			suspendServer();
		}

		sleep(m_checkIfIdleEvery);
	}
}

bool Manager::isSystemIdle()
{
	double cpuLoad     = m_monitor.getCpuLoad();
	double storageLoad = m_monitor.getStorageLoad();

	bool isIdle = m_suspendIfCpuIdle || m_suspendIfStorageIdle;

	cout << "Average CPU load: "     << cpuLoad     << " %." << endl;

	if(cpuLoad > CPU_LIMIT)
	{
		if(m_suspendIfCpuIdle)
		{
			isIdle = false;
		}

		cout << "CPU     -- busy." << endl;
	}
	else
	{
		cout << "CPU     -- idle." << endl;
	}


	cout << "Average Storage load (across all monitored drives): " << storageLoad << " KB/s." << endl;

	if(storageLoad > STORAGE_LIMIT)
	{
		if(m_suspendIfStorageIdle)
		{
			isIdle = false;
		}

		cout << "Storage -- busy." << endl;
	}
	else
	{
		cout << "Storage -- idle." << endl;
	}

	return isIdle;
}

void Manager::suspendServer()
{
	double currentTimeInMinutes = 0;  //since 00:00:00

	if(!getCurremtTimeInMinutes(&currentTimeInMinutes))
	{
		return;
	}

	vector<double> suspendUpTo;

	for(size_t i = 0, len = m_timesToWakeAt.size(); i < len; ++i)
	{
		double timeInMinutes;

		if(convertTimeToMinutes(m_timesToWakeAt[i], &timeInMinutes))
		{
			suspendUpTo.push_back(timeInMinutes);
		}
	}

	sort(suspendUpTo.begin(), suspendUpTo.end(), sortVector);

	for(size_t i = 0, len = suspendUpTo.size(); i < len; ++i)
	{
		if(currentTimeInMinutes < suspendUpTo[i])
		{
			suspendUntil(currentTimeInMinutes, suspendUpTo[i]);

			return;
		}
	}

	if(suspendUpTo.size() > 0)
	{
		suspendUntil(currentTimeInMinutes, suspendUpTo[0]);
	}
	else
	{
		//todo suspend for ever
	}
}

void Manager::suspendUntil(double currentTimeInMinutes, double until)
{

	double secondsToSleep = 0;

	if(currentTimeInMinutes < until)
	{
		secondsToSleep = ((until - currentTimeInMinutes) - 5) * 60;
	}
	else
	{
		secondsToSleep = (until + (TOTAL_MINUTS_IN_DAY - currentTimeInMinutes) - 5) * 60;
	}

	cout << "Got: currentTimeInMinutes (" << currentTimeInMinutes << "), until(" << until << ")." << endl;
	cout << "Suspending server for " << secondsToSleep << " seconds." << endl;

	vector<string> output;

	rtcWakeSuspend(secondsToSleep, &output);
	//pmUtilSuspend(secondsToSleep, &output);

	printHeaderMessage("System returned from suspend", true);
	cout << "Suspend output: ";

	for(size_t i = 0, len = output.size(); i < len; ++i)
	{
		cout << output[i] << " , ";
	}

	cout << endl;
}

void Manager::rtcWakeSuspend(double secondsToSleep, vector<string> *output)
{
	string sleepMode = getSleepMode();

	ostringstream oss;
	oss << "rtcwake -m " << sleepMode << " -s " << secondsToSleep;

	runSystemCommand(oss.str(), output);
}

void Manager::pmUtilSuspend(double secondsToSleep, vector<string> *output)
{
	ostringstream oss;

	//Clear previously set wakeup time
	oss << "sh -c \"echo 0 > /sys/class/rtc/rtc0/wakealarm\"";

	runSystemCommand(oss.str());

	oss.str("");

	//Set the wakeup time
	oss << "sh -c \"echo `date '+%s' -d '+ " << (secondsToSleep / 60)
	    << " minutes'` > /sys/class/rtc/rtc0/wakealarm\"";

	runSystemCommand(oss.str());

	//After setting the time, PC can be turned off with this command
	runSystemCommand(getPmUtilCommand(), output);
}

string Manager::getSleepMode()
{
	switch(m_sleepMode)
	{
		case STAND_BY: { return string("standby");}
		case MEM:      { return string("mem");}
		case DISK:     { return string("disk");}
		default:       { return string("disk");}
	}
}

string Manager::getPmUtilCommand()
{
	switch(m_sleepMode)
	{
		case STAND_BY: { return string("pm-suspend");}
		case MEM:      { return string("pm-suspend");}
		case DISK:     { return string("pm-hibernate");}
		default:       { return string("pm-hibernate");}
	}
}