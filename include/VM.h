#ifndef VMINFO_H

#define VMINFO_H


#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <cstring>
#include <set>
#include "Monitor.h"
#include <vector>
#include <set>
#include <list>
#include <map>
#include "ModelValue.h"
#include <ctime>
#include <chrono>

#define VM_NAME_LEN 64
#define PCI_STR_LEN 8
#define  NETDEV_LEN 32
#define  MAX_PATH_LEN 64



/*
**	Virtual machine object.
**
*/
class VM {
public:
	VM(): modelValListLimit(100){
		vf_no = pid = dom_id  = -1;
		rx_packets = tx_packets = total_packets = 0;
		rx_bytes = tx_bytes = total_KB = 0;
		dom_ptr = NULL;
		io_timestamp_usec = -1;
		vm_event_state = VIR_DOMAIN_EVENT_UNDEFINED;	
		then_params = now_params = NULL;
		vcpu_timestamp_usec = last_vcpu_timestamp_usec = -1;
		KB_per_sec = packets_per_sec = 0;
		boundedBuffer = NULL;
	}
	const char* 						getVMName() const { return vm_name;}	

	void setNUMANumber(int num) {
		numa_number = num;
	}
	void setNetDev(const char *nd) {
		int len = strlen(nd);
		strcpy(netdev, nd);
		netdev[len] = '\0';
	}

	void setSampleData(double sd, int sp) {
		sampling_duration = sd;
		sampling_period = sp;
	}

	void setStdPPS(unsigned long long pps){
		standard_pps = pps;
	}
	void setStdMAPKI(unsigned long long mapki){
		standard_mapki = mapki;
	}

	void setNodeCPUListMap(std::map<int, std::vector<int>> nclm){
		cpu_list_in_node = nclm;
	}

	void setCPU2NodeMap(std::map<int, int> c2nm) {
		cpu_to_node_map = c2nm;
	}

	void setAPM(std::vector<std::vector<double>> apm) {
			APM = apm;
	}	

	void setANM(std::vector<double>	anm) {
		ANM = anm;
	}
	
	void setANP(std::vector<std::vector<double>> anp) {
		ANP = anp;
	}

	void setBoundBuffer(BoundedBuffer	&bb) {
		boundedBuffer = &bb;
	}

	void start();
	void printVMInfo();
	void putModelValue();

	~VM() {
		if(then_params)
			virTypedParamsFree(then_params, params_size);
		if(now_params)
			virTypedParamsFree(now_params, params_size);
	}


private:
	// 队列
	BoundedBuffer		 	   *boundedBuffer;

	friend class 				Monitor;
	
	char						vm_name[VM_NAME_LEN];
	char						netdev_pci_slot_str[PCI_STR_LEN];
	char						netdev_pci_funct_str[PCI_STR_LEN];
	// 宿主机里需要监控的网卡的名字
	char						netdev[NETDEV_LEN];
	// the VF number, -1 if not assigned one.
	int							vf_no;

	std::list<ModelValue>		modelValList;
	// modelValList最多保存多少个历史值
	int 					modelValListLimit;
	double 					alpha;
	double 					beta;
	double 					gama;
	double 					raie;
	// 作为阈值标准的pps
	double					standard_pps;
	double 					standard_mapki;

	std::vector<std::vector<double>> 	APM;
	std::vector<std::vector<double>> 	ANP;
	std::vector<double>		ANM;


	// cpu_list_in_node[0]表示节点0有哪些CPU
	std::map<int, std::vector<int>>		cpu_list_in_node;
	// 保存CPU->该CPU属于哪个节点的映射
	std::map<int, int>		cpu_to_node_map;



	/*********** I/O statistics data ********start *********/
	unsigned long long			rx_packets;
	unsigned long long			tx_packets;
	unsigned long long			total_packets;	
	
	// packets per second
	double						packets_per_sec;
	unsigned long long			rx_bytes;
	unsigned long long			tx_bytes;

	// KB
	double						total_KB;
	double						KB_per_sec;
	unsigned long long			io_timestamp_usec;

	/*********** I/O statistics data ********end *********/


	/*********** Performance data ********start *********/

	// Number of CPU cycles captured during monitoring period;
	unsigned long long			cycles;
	// Number of CPU Instructions captured during monitoring period;
	unsigned long long			instructions;	
	unsigned long long			cache_references;	
	unsigned long long			cache_misses;
	unsigned long long			mem_load_uops_retired_llc_miss;

	/*********** Performance data ********end *********/

	/*********** libvirt API related ********************/
	virDomainPtr 				dom_ptr;

	// domain id, -1 if offlin
	int							dom_id;

	// vcpu affinity sets. Containing which real cpu number could run on.
	std::set<unsigned int>		vcpu_affi_maps;

	// 当前虚拟机的vCPU的列表。
	std::vector<int> 			vCPU_list;
	// 当前虚拟机实际运行在的物理CPU的列表
	std::set<int>			pCPU_set;
	std::map<int, int>			vCPU2pCPU;
	std::map<int, int>			pCPU2vCPU;
	// 保存某个vCPU对应的使用率
	std::map<int, double> 		vCPU_usage_map;
	unsigned long long			vcpu_timestamp_usec;
	unsigned long long			last_vcpu_timestamp_usec;

	// the memory page distribution in each NUMA node of this VM.
	std::vector<unsigned long> 	memory_on_each_node;
	int 						numa_number;

	virTypedParameterPtr 		then_params, now_params;
	int 						params_size;

	
	// The corresponding process id of this VM.
	unsigned int 				pid;

	// 每次采样持续的时间 （s)
	double			 			sampling_duration;
	// 每隔多久采样一次
	int 						sampling_period; 

	/* valid state include:
	** 1,Suspended; 2,Resumed;
	** 3,Stopped;	4,Shutdown;
	** 5,PMSuspended; 6,Crashed
	*/ 
	virDomainEventType			vm_event_state;

	// helper function
	// 获取网络IO数据
	void getNetworkIOStat();
	// 获取内存分布数据
	void getMemoryStat();
	// 获取CPU使用率数据
	void getCPUStat();
	// 获取perf数据
	void getPerfEventStat();

	void extractPerfEvents(virTypedParameter *params, int nparams);

	// 使用模型计算值
	void calculateModelValue();

	void execute();
	
};





















#endif

