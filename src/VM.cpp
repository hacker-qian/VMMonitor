#include "VM.h"

using namespace std;

void VM::getNetworkIOStat() {
	int  ret = 0;
	char rx_pack_path[MAX_PATH_LEN];
	char tx_pack_path[MAX_PATH_LEN];
	char rx_bytes_path[MAX_PATH_LEN];
	char tx_bytes_path[MAX_PATH_LEN];

	snprintf(rx_pack_path, MAX_PATH_LEN, "/sys/class/net/%s/vf%d/statistics/rx_packets", netdev, vf_no);
	snprintf(tx_pack_path, MAX_PATH_LEN, "/sys/class/net/%s/vf%d/statistics/tx_packets", netdev, vf_no);
	snprintf(rx_bytes_path, MAX_PATH_LEN, "/sys/class/net/%s/vf%d/statistics/rx_bytes", netdev, vf_no);
	snprintf(tx_bytes_path, MAX_PATH_LEN, "/sys/class/net/%s/vf%d/statistics/tx_bytes", netdev, vf_no);

	unsigned long long		last_rx_packets;
	unsigned long long		last_tx_packets;
	unsigned long long		last_rx_bytes;
	unsigned long long		last_tx_bytes;
	unsigned long long		last_io_timestamp;

	last_rx_packets   = rx_packets;
	last_tx_packets   = tx_packets;
	last_rx_bytes     = rx_bytes;
	last_tx_bytes     = tx_bytes;
	last_io_timestamp = io_timestamp_usec;

	FILE* rx_pack_file = fopen(rx_pack_path, "r");
	if(rx_pack_file == NULL) {
		fprintf(stderr, "cannot open %s, error:%s\n", rx_pack_path, strerror(errno));
		ret = -1;
		return;

	}
	fscanf(rx_pack_file, "%llu", &rx_packets);
	fclose(rx_pack_file);

	FILE* tx_pack_file = fopen(tx_pack_path, "r");
	if(tx_pack_file == NULL) {
		fprintf(stderr, "cannot open %s, error:%s\n", tx_pack_path, strerror(errno));
		ret = -1;
		return;

	}
	fscanf(tx_pack_file, "%llu", &tx_packets);
	fclose(tx_pack_file);
	FILE* rx_bytes_file = fopen(rx_bytes_path, "r");
	if(rx_bytes_file == NULL) {
		fprintf(stderr, "cannot open %s, error:%s\n", rx_bytes_path, strerror(errno));
		ret = -1;
		return;

	}
	fscanf(rx_bytes_file, "%llu", &rx_bytes);
	fclose(rx_bytes_file);

	FILE* tx_bytes_file = fopen(tx_bytes_path, "r");
	if(tx_bytes_file == NULL) {
		fprintf(stderr, "cannot open %s, error:%s\n", tx_bytes_path, strerror(errno));
		ret = -1;
		return;

	}

	fscanf(tx_bytes_file, "%llu", &tx_bytes);
	fclose(tx_bytes_file);

	struct timeval timestamp;
	gettimeofday(&timestamp, NULL);
	io_timestamp_usec = timestamp.tv_sec * 1000000 + timestamp.tv_usec;
	double elapsedTime = (io_timestamp_usec - last_io_timestamp) / 1000000.0;
		
	total_packets = (rx_packets - last_rx_packets) + (tx_packets - last_tx_packets);
	total_KB = (rx_bytes - last_rx_bytes)/1024.0 + (tx_bytes - last_tx_bytes)/1024.0;
	// 历史值占0.4
	double p = 0.4;
	packets_per_sec = packets_per_sec * p + (total_packets / elapsedTime) * (1-p);	
	KB_per_sec = KB_per_sec * p + (total_KB / elapsedTime) * (1-p);
}


// 获取内存分布数据
void VM::getMemoryStat() {
	char file_name[MAX_PATH_LEN];
	snprintf(file_name, MAX_PATH_LEN, "/proc/%d/numa_maps", pid);

	FILE* file = fopen(file_name, "r");
	if (file == NULL) {
	    fprintf(stderr, "Tried to open PID %d numamaps, but it apparently went away.\n", pid);
		return;  // Assume the process terminated?
	}
	memory_on_each_node.clear();
	// push 'numa_number' elements in vector first.
	for(int i = 0; i < numa_number; i++)
		memory_on_each_node.push_back(0);

	char *buf = file_name;
	while(fgets(buf, MAX_PATH_LEN, file)) {
		const char* delimiters = " \t\r\n";
		char *p = strtok(buf, delimiters);
		while(p) {
			if(p[0] == 'N') {
				int node = (int)strtol(&p[1], &p, 10);
				if(node >= numa_number) {
					fprintf(stderr, "numa_maps node number parse error:node %d is too large\n", node);
					fclose(file);
					return;
				}
				if(p[0] != '=') {
					fprintf(stderr, "numa_maps node number parse error\n");
					fclose(file);
					return;
				}
				unsigned int pages = (unsigned int)strtol(&p[1], &p, 10);				
				memory_on_each_node[node] += (pages);
			}
			// get next token on the line
			p = strtok(NULL, delimiters);
		}
	}
}

// 获取CPU使用率数据
void VM::getCPUStat() {

}

// 获取perf数据
void VM::getPerfEventStat() {

}

void VM::start() {
	getNetworkIOStat();
	getMemoryStat();
	getCPUStat();
	getPerfEventStat();
}

void VM::printVMInfo() {
	printf("%s:\n", vm_name);
	printf("PPS:\t%lf\n", packets_per_sec);
	printf("KBPS:\t%lf\n", KB_per_sec);
	printf("Memory distribution:\n");
	for(int i = 0; i < numa_number; ++i) {
		printf("Node[%d]: %llu\n", i, memory_on_each_node[i]);
	}
}

