#ifndef Monitor_H

#define Monitor_H
/*
**	the main class of monitor
**	------------written by Jenson
*/

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <string>
#include <limits.h>
#include <errno.h>
#include <vector>
#include <list>
#include <set>
#include <algorithm>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <numa.h>
#include <sys/time.h>
#include <sys/types.h>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>

#include <sys/socket.h>
#include <netinet/in.h>

#include "VM.h"

#define  NETDEV_LEN 32
#define  MAXLINE 128
#define  MAX_PATH_LEN 64
#define PCI_STR_LEN 8
#ifndef ATTRIBUTE_UNUSED
# define ATTRIBUTE_UNUSED __attribute__((__unused__))
#endif

class IOMonitor;
class VM;

class Monitor {
public:
	Monitor(int numa_num, int cpu_num, const char* connect_str):
						boundedBuffer(128),	numa_number(numa_num), cpu_number(cpu_num)
									 								 

	{
		int len = strlen(connect_str);
		conn_str = new char[len + 1];
		strncpy(conn_str, connect_str, len);
		conn_str[len] = '\0';
		io_threshold = 0;
	}

	~Monitor();
	// return -1 if failed; 0 if succeed.
	int init();
	void start();
private:
	// 有多少个NUMA节点
	int						numa_number;
	// 有多少个CPU
	int 					cpu_number;
	
	// cpu_list_in_node[0]表示节点0有哪些CPU
	std::map<int, std::vector<int>>		cpu_list_in_node;
	// 保存CPU->该CPU属于哪个节点的映射
	std::map<int, int>		cpu_to_node_map;

	// which net device should be monitored
	char					netdev[NETDEV_LEN];
	char					netdev_pci_bus_str[PCI_STR_LEN];
	char					netdev_pci_slot_str[PCI_STR_LEN];
	char					netdev_pci_funct_str[PCI_STR_LEN];

	// corresponding integer value
	unsigned int			netdev_pci_bus_val;
	unsigned int			netdev_pci_slot_val;
	unsigned int			netdev_pci_funct_val;

	// which numa node the NIC attached to 
	int 					netdev_numanode;

	// 每次采样的间隔(ms)
	double		 			monitor_interval;
	// 每次采样持续的时间 （ms)
	double 					sampling_duration;

	// I/O intensive threshold
	unsigned long long 		io_threshold;
	unsigned long long		mapki;

	// Containing all running vm. Key=dom_id.
	std::map<int, VM> 		vm_infos_map;	

	std::vector<std::vector<double>> 	APM;
	std::vector<std::vector<double>> 	ANP;
	std::vector<double>		ANM;

	

	/* init vm_info */
	int 					initVMInfo();

	/* using the virDomainPtr to initialize a VM object */
	int 					setVMInfo(virDomainPtr, VM&, int dom_id);

	/* parsing config file */
	int 					parseConf();

	/********************** libvirt API related  **********************/	
	char					*conn_str;
	virConnectPtr 			conn;
	static void 			libvirt_error_handle(void* userdata, virErrorPtr err);

	// get the vCPU and pCPU mapping of this VM. key=vCPU, value= pCPU
	std::map<int,int> 		getVMRealCPU(virDomainPtr);

	// Libvirt event call back. Handle the VM shutdown/start/reboot event
	static int 				virVMEventCallBack(virConnectPtr conn ATTRIBUTE_UNUSED,
												virDomainPtr dom, int event,
												int detail, void *Monitor);
	// Handle the libvirt connection close event
	static void 			virConnectCloseCallBack(virConnectPtr conn ATTRIBUTE_UNUSED,
             								int reason,
             								void *opaque ATTRIBUTE_UNUSED);


	// 用来与界面程序通信的变量和函数
	BoundedBuffer		 boundedBuffer;
	void startServer();


};







#endif
