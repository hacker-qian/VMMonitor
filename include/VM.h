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
#include <list>
#include <map>


#define VM_NAME_LEN 64
#define PCI_STR_LEN 8
#define  NETDEV_LEN 32
#define  MAX_PATH_LEN 64

// 模型需要的值
struct ModelValue{
	double 				value;
	unsigned long 		timestamp;
    std::string 				date;
};

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
		io_timestamp_usec = 0;
		vm_event_state = VIR_DOMAIN_EVENT_UNDEFINED;
		netdev = "enp6s0";
	}
	const char* 						getVMName() const { return vm_name;}	

	void setNUMANumber(int num) {
		numa_number = num;
	}
	
	void start();
	void printVMInfo();


private:

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
	std::vector<int>			pCPU_list;
	std::map<int, int>			vCPU2pCPU;

	// the memory page distribution in each NUMA node of this VM.
	std::vector<unsigned long> 	memory_on_each_node;
	int 						numa_number;

	
	// The corresponding process id of this VM.
	unsigned int 				pid;

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
	
};





















#endif

