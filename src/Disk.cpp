#include "Disk.h"

namespace
{
	void monitorStorageUsage(Disk *deviceToMonitor, shared_ptr<WatchDog> watchDog)
	{
		ifstream statesFile (deviceToMonitor->getStatesFileName());

		while(watchDog->shouldStillMonitor())
		{
			DeviceUsage diskUsage (0, 0, 0);

			deviceToMonitor->calculateUsage(statesFile, &diskUsage);

			deviceToMonitor->setUsage(diskUsage);
		}

		statesFile.close();
	}
}

void Disk::monitorUsage()
{
	if(!isDeviceInitialized())
	{
		initDevice();
	}

	std::thread diskMonitorThread (monitorStorageUsage, this, getWatchDogCopy());
	diskMonitorThread.detach();
}

void Disk::calculateUsage(ifstream &statesFile, DeviceUsage *diskUsage)
{
	int prevNumOfReadSectors = 0, prevNumOfWroteSectors = 0;

	DiskStats prevDiskStates, newDiskStates;

	getDiskStats( statesFile, &prevDiskStates);

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	int currentNumOfReadSectors = 0, currentNumOfWroteSectors = 0;

	getDiskStats( statesFile, &newDiskStates);

	diskUsage->totalRead = ( (newDiskStates.num_r_sectors - prevDiskStates.num_r_sectors) * m_sectorSize ) / 1000;
	diskUsage->totalWritten = ( (newDiskStates.num_w_sectors - prevDiskStates.num_w_sectors) * m_sectorSize ) / 1000;
}

string Disk::getStatesFileName()
{
	stringstream ss;
	ss << "/sys/block/" << getDeviceName() << "/stat";

	return ss.str();
}

void Disk::initDevice()
{
	m_sectorSize = getDiskSectorSize(getDeviceName());

	isDeviceInitialized(true);
}

void Disk::parseDiskStats(const vector<string> &fileOutput, DiskStats *stats)
{
	if(fileOutput.size() > 0)
	{
		string statesLine = fileOutput[0];
		vector<string> tokens;

		splitByEmptySpace(statesLine, &tokens);

		if(tokens.size() == 11)
		{
			stats->num_r_io_processed = atoi(tokens[0].c_str());  //number of read I/Os processed
			stats->num_r_io_merged    = atoi(tokens[1].c_str());  //number of read I/Os merged
			stats->num_r_sectors      = atoi(tokens[2].c_str());  //number of sectors read
			stats->time_r_ticks       = atoi(tokens[3].c_str());  //total wait time for read requests
			stats->num_w_io_processed = atoi(tokens[4].c_str());  //number of write I/Os processed
			stats->num_w_io_merged    = atoi(tokens[5].c_str());  //number of write I/Os merged
			stats->num_w_sectors      = atoi(tokens[6].c_str());  //number of sectors written
			stats->time_w_ticks       = atoi(tokens[7].c_str());  //total wait time for write requests
			stats->num_in_flight      = atoi(tokens[8].c_str());  //number of I/Os currently in flight
			stats->time_io_ticks      = atoi(tokens[9].c_str());  //total time this block device has been active
			stats->time_in_queue      = atoi(tokens[10].c_str()); //total wait for all requests
		}
	}
}

void Disk::getDiskStats(ifstream &statesFile, DiskStats *stats)
{
	vector<string> fileOutput;

	parseFile(statesFile, &fileOutput);

	parseDiskStats(fileOutput, stats);
}

int Disk::getDiskSectorSize(const string &diskName)
{
	int sectorSize = 0;

	vector<string> fileOutput;

	parseFile("/sys/block/" + diskName + "/queue/hw_sector_size", &fileOutput);

	if(fileOutput.size() > 0)
	{
		sectorSize = atoi(fileOutput[0].c_str());
	}

	return sectorSize;
}


