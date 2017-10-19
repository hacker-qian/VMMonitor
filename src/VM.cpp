#include "VM.h"

using namespace std;

void VM::getNetworkIOStat() {
	int  ret = 0;
	char rx_pack_path[MAX_PATH_LEN];
	char tx_pack_path[MAX_PATH_LEN];
	char rx_bytes_path[MAX_PATH_LEN];
	char tx_bytes_path[MAX_PATH_LEN];
	printf("netdev:%s\tvf_node:%d\n", netdev, vf_no);
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
	if(last_io_timestamp < 0) {
		// 第一次数据丢掉
		return;
	}

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
	virError err;
	int n_vCPU = virDomainGetVcpusFlags(dom_ptr, VIR_DOMAIN_VCPU_LIVE);	
	if(n_vCPU == -1) {
		virCopyLastError(&err);
	    fprintf(stderr, "virDomainGetVcpusFlags failed: %s\n", err.message);
	    virResetError(&err);
	    return;
	}
	// 更新当前虚拟机的vCPU信息
	virVcpuInfoPtr vcpu_info_list = new virVcpuInfo[n_vCPU];	
	n_vCPU = virDomainGetVcpus(dom_ptr, vcpu_info_list, n_vCPU, NULL, 0);
	if(n_vCPU == -1) {
		virCopyLastError(&err);
	    fprintf(stderr, "virDomainGetVcpus failed: %s\n", err.message);
	    virResetError(&err);
	}else{
		vCPU_list.clear();
		vCPU2pCPU.clear();
		pCPU2vCPU.clear();
		vCPU_list.resize(n_vCPU);
		for(int i = 0; i < n_vCPU; ++i) {
			int vcpu = vcpu_info_list[i].number;
			int pcpu = vcpu_info_list[i].cpu;
			vCPU_list.push_back(vcpu);
			pCPU_set.insert(pcpu);
			vCPU2pCPU[vcpu] = pcpu;
			pCPU2vCPU[pcpu] = vcpu;
		}
		
	}
	delete[] vcpu_info_list;

	int max_id = 0;
    int nparams = 0, then_nparams = 0, now_nparams = 0;
	/* and see how many vCPUs can we fetch stats for */
    if ((max_id = virDomainGetCPUStats(dom_ptr, NULL, 0, 0, 0, 0)) < 0) {
        virCopyLastError(&err);
	    fprintf(stderr, "virDomainGetCPUStats failed: %s\n", err.message);
	    virResetError(&err);
        return;
    }

    /* how many stats can we get for a vCPU? */
    if ((nparams = virDomainGetCPUStats(dom_ptr, NULL, 0, 0, 1, 0)) < 0) {
        virCopyLastError(&err);
	    fprintf(stderr, "virDomainGetCPUStats failed: %s\n", err.message);
	    virResetError(&err);
        return;
    }

    params_size = nparams * max_id;
    if(now_params == NULL)
    	now_params = (virTypedParameterPtr)calloc(nparams * max_id, sizeof(*now_params));
    if(then_params == NULL)
    	then_params = (virTypedParameterPtr)calloc(nparams * max_id, sizeof(*then_params));
    else{
    	virTypedParamsFree(then_params, then_nparams * max_id);
    	then_params = now_params;
    }
    /* And current stats */
    if ((now_nparams = virDomainGetCPUStats(dom_ptr, now_params,
                                        nparams, 0, max_id, 0)) < 0) {
            virCopyLastError(&err);
		    fprintf(stderr, "virDomainGetCPUStats failed: %s\n", err.message);
		    virResetError(&err);
	        return;
    }
    struct timeval timestamp;
	gettimeofday(&timestamp, NULL);
	last_vcpu_timestamp_usec = vcpu_timestamp_usec;
	vcpu_timestamp_usec = timestamp.tv_sec * 1000000 + timestamp.tv_usec;
	if(last_vcpu_timestamp_usec < 0) {
		// 第一次数据丢掉
		return;
	}
	// 计算当前VM的vCPU的使用率
	then_nparams = nparams;
	if (then_nparams != now_nparams) {
        /* this should not happen (TM) */
        printf("parameters counts don't match\n");
        return;
    }
    int j = 0;
    for(auto i : pCPU_set) {
    	size_t pos;
        double usage;

        /* check if the vCPU is in the maps */
        if (now_params[i * nparams].type == 0 ||
            then_params[i * then_nparams].type == 0)
            continue;

        for (j = 0; j < nparams; j++) {
            pos = i * nparams + j;
            if (!strcmp(then_params[pos].field, VIR_DOMAIN_CPU_STATS_CPUTIME) ||
                !strcmp(then_params[pos].field, VIR_DOMAIN_CPU_STATS_VCPUTIME))
                break;
        }

        if (j == nparams) {
            fprintf(stderr, "unable to find %s\n", VIR_DOMAIN_CPU_STATS_CPUTIME);
            return;
        }

       
        double elapsedTime = (vcpu_timestamp_usec - last_vcpu_timestamp_usec) / 1000000.0;
        usage = (now_params[pos].value.ul - then_params[pos].value.ul) / elapsedTime;
        usage *= 100;

        int vcpu = pCPU2vCPU[i];
        vCPU_usage_map[vcpu] = usage;
    }

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
	printf("VM has %d\n vCPU.\n", vCPU_list.size());
	for(auto it : vCPU_usage_map) {
		printf("vCPU%d usage:%lf\n", it.first, it.second);
	}
}

