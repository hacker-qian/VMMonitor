#include "Monitor.h"
using namespace std;


Monitor::~Monitor() {
	

	delete[] conn_str;

	// need to free domain_ptr
	map<int, VM>::iterator it;

    for(auto it : vm_infos_map)
    	virDomainFree((it.second).dom_ptr);     

    virConnectDomainEventDeregister(conn, virVMEventCallBack);
    virConnectUnregisterCloseCallback(conn, virConnectCloseCallBack);
    virConnectClose(conn); 
}







// return -1 if failed, 0 if succeed
int Monitor::init() {
	virSetErrorFunc(NULL, libvirt_error_handle);
	if (virInitialize() < 0) {
        fprintf(stderr, "Failed to initialize libvirt");
        return -1;
    }

    if (virEventRegisterDefaultImpl() < 0) {
        fprintf(stderr, "Failed to register event implementation: %s\n",
                virGetLastErrorMessage());
         return -1;
    }
	conn = virConnectOpen(conn_str);
	if(conn == NULL) {
		fprintf(stderr, "Failed to open connection to %s\n", conn_str);
		return -1;
	}	

	int         ret = 0;	

	/* Now initialize other resouces */	
	ret = parseConf();
	if(ret == -1)
		exit(1);	

	if (virConnectRegisterCloseCallback(conn,
                                        virConnectCloseCallBack, NULL, NULL) < 0) {
        fprintf(stderr, "Unable to register close callback\n");
        return -1;
    }

    ret = virConnectDomainEventRegister(conn, virVMEventCallBack, this, NULL);
    if(ret != 0) {
		fprintf(stderr, "Unable to register Domain Event callback!\n");
		return -1;
	}

	ret = initVMInfo();
	if(ret != 0) {
		fprintf(stderr, "init_vminfo failed!\n");
		return -1;
	}



	// get the numa node of netdev
	char path_buf[MAX_PATH_LEN];
	snprintf(path_buf, MAX_PATH_LEN, "/sys/class/net/%s/device/numa_node", netdev);
	FILE* netdev_numanode_file = fopen(path_buf, "r");
	if(netdev_numanode_file == NULL) {
		fprintf(stderr, "Failed to open %s\n, error:%s", path_buf, strerror(errno));
		return -1;
	}

	ret = fscanf(netdev_numanode_file, "%d", &netdev_numanode);
	if(ret != 1) {
		fprintf(stderr, "fscanf doesnot match:%s, ret=%d\n", path_buf, ret);
		fclose(netdev_numanode_file);
		return -1;
	}
	fclose(netdev_numanode_file);

	
	return 0;

}



/*

**	Parsing /etc/Monitor.conf. If cannot open config file,

** 	return -1 and exit.

*/

int Monitor::parseConf() {

	const char* conf_file = "/etc/monitor.conf";

	FILE* file = fopen(conf_file, "r");

	if(file == NULL) {

		perror("Open /etc/monitor.conf failed. Please make sure the file exist");

		return -1;

	}

	char line[MAXLINE];

	while(fgets(line, MAXLINE, file) != NULL) {

		if(line[0] == '#') {
			// skip the comments.
			continue;
		} else if(strstr(line, "netdev") != NULL){

			sscanf(line, "netdev=%s", netdev);

		} else if(strstr(line, "pci_bus") != NULL){

			sscanf(line, "pci_bus=%s", netdev_pci_bus_str);
			netdev_pci_bus_val = strtol(netdev_pci_bus_str, NULL, 16);			

		} else if(strstr(line, "pci_slot") != NULL){

			sscanf(line, "pci_slot=%s", netdev_pci_slot_str);
			netdev_pci_slot_val = strtol(netdev_pci_slot_str, NULL, 16);			

		} else if(strstr(line, "pci_function") != NULL){

			sscanf(line, "pci_function=%s", netdev_pci_funct_str);
			netdev_pci_funct_val = strtol(netdev_pci_funct_str, NULL, 16);			

		} else if(strstr(line, "monitor_interval") != NULL){

			sscanf(line, "monitor_interval=%u", &monitor_interval);			

		} else if(strstr(line, "io_threshold") != NULL){

			sscanf(line, "io_threshold=%llu", &io_threshold);			

		} else if(strstr(line, "APM") != NULL){
			int n, m;			
			sscanf(line, "APM=%d %d", &n, &m);
			APM.resize(n, vector<double>(m, 0.0));			
			for(int i = 0; i < n; ++i) {
				for(int j = 0; j < m; ++j) {
					fscanf(file, "%lf", &APM[i][j]);
				}
			}
		} else if(strstr(line, "ANM") != NULL){
			int n, m;			
			sscanf(line, "ANM=%d %d", &n, &m);
			ANM.resize(m, 0.0);			
			for(int j = 0; j < m; ++j) {
				fscanf(file, "%lf", &ANM[j]);
			}
		} else if(strstr(line, "ANP") != NULL){
			int n, m;			
			sscanf(line, "ANP=%d %d", &n, &m);
			ANP.resize(n, vector<double>(m, 0.0));			
			for(int i = 0; i < n; ++i) {
				for(int j = 0; j < m; ++j) {
					fscanf(file, "%lf", &ANP[i][j]);
				}
			}
		} 	

	}

	fclose(file);

	return 0;

}



/* using the virDomainPtr to initialize a VM object */
int Monitor::setVMInfo(virDomainPtr dom_ptr, VM& vm_info, int dom_id) {

		int flag = 0;		
		vm_info.dom_ptr = dom_ptr;
		vm_info.dom_id = dom_id;
		map<int, int> v2pMap = getVMRealCPU(dom_ptr);
		vm_info.vCPU_list.clear();
		vm_info.pCPU_list.clear();
		vm_info.vCPU2pCPU.clear();
		for(auto it : v2pMap) {
			// key=vcpu, value = pCPU;
			vm_info.vCPU_list.push_back(it.first);
			vm_info.pCPU_list.push_back(it.second);
			vm_info.vCPU2pCPU[it.first] = it.second;
		}		

		const char* dom_name = virDomainGetName(dom_ptr);
		int len = strlen(dom_name);
		if(len >= VM_NAME_LEN) {
			fprintf(stderr, "VM name:%s too long! Length:%d\n", dom_name, len);			
			flag = -1;	
		}else {
			strncpy(vm_info.vm_name, dom_name, len);
			vm_info.vm_name[len] = '\0';
		}

		// get the pid according to dom_name
		char path_buf[MAX_PATH_LEN];
		snprintf(path_buf, MAX_PATH_LEN, "/var/run/libvirt/qemu/%s.pid", dom_name);
		FILE* pid_file = fopen(path_buf, "r");
		if(pid_file == NULL) {
			fprintf(stderr, "cannot open %s, error:%s\n", path_buf, strerror(errno));
			flag = -1;
		} else{
			fscanf(pid_file, "%u", &vm_info.pid);
			fclose(pid_file);
		}		

		/* calculate the vf_no */
		char* dom_xml = virDomainGetXMLDesc(dom_ptr, 0);		
		if(dom_xml != NULL) {

			/* the xml format should be like this:
			<address domain='0x0000' bus='0x07' slot='0x00' function='0x4'/>
			*/
			long slot_val = 0, func_val = 0;
			char target_bus[PCI_STR_LEN+3];
			sprintf(target_bus, "bus=\'%s\'", netdev_pci_bus_str);
			char* bus_p = strstr(dom_xml, target_bus);
			if(bus_p == NULL) {
				fprintf(stderr, "Parsing domain xml error: cannot find 'bus' attribute.\n");
				flag = -1;
				free(dom_xml);
				return flag;
			}

			// extract the pci slot value
			char* slot_p = strstr(bus_p, "slot");

			if(slot_p == NULL) {
				fprintf(stderr, "Parsing domain xml error: cannot find 'slot' attribute.\n");
				flag = -1;
				free(dom_xml);
				return flag;
			}

			while(*(slot_p) != '\'')
				slot_p++;
			slot_p++;
			slot_val = strtol(slot_p, &slot_p, 0);
			// extract the pci function value
			char* funct_p = strstr(slot_p, "function");
			if(funct_p == NULL) {
				fprintf(stderr, "Parsing domain xml error: cannot find 'function' attribute.\n");
				flag = -1;
				free(dom_xml);
				return flag;
			}

			while(*(funct_p) != '\'')
				funct_p++;
			funct_p++;
			func_val = strtol(funct_p, &funct_p, 0);


			// !!!Note:Assuming: 
			// slot=0 function=1~7 -> vf 0~6
			// slot=1 function=0~7 -> vf 7~14
			// That is, vf_no = (vfslot - pfslot)*8 + (vf_function-1).
			vm_info.vf_no = (slot_val - netdev_pci_slot_val) * 8 + (func_val - 1);
			free(dom_xml);
		} else{
			fprintf(stderr, "%s\n", "cannot get domain xml");
			flag = -1;
		}
		return flag;
}



/* 
** Initialize the VM info.
** return -1 if failed, 0 if succeed.
*/
int Monitor::initVMInfo() {

	// Using libvirt to get all running domain.
	int 	dom_num = 0;
	int 	*running_dom_ids;	
	int 	ret = 0;
	int 	flag = 0;

	dom_num = virConnectNumOfDomains(conn);
	running_dom_ids = new int[dom_num];
	dom_num = virConnectListDomains(conn, running_dom_ids, dom_num);	
	// get domain ptr
	for(int i = 0; i < dom_num; i++) {
		int dom_id = running_dom_ids[i];
		VM vm_info;
		vm_info.setNetDev(netdev);
		vm_info.setNUMANumber(numa_number);
		vm_infos_map[dom_id] = vm_info;
		virDomainPtr dom_ptr;
		dom_ptr = virDomainLookupByID(conn, dom_id);
		flag = setVMInfo(dom_ptr, vm_infos_map[dom_id], dom_id);
		if(flag == -1) 
			ret = -1;			
	}
	delete[] running_dom_ids;
	return ret;
}



/****** libvirt API related  ***************/

void Monitor::libvirt_error_handle(void* userdata, virErrorPtr err) {
	fprintf(stderr, "Global handler, failure of libvirt library call:\n");
	fprintf(stderr, "Code:%d\n", err->code);
	fprintf(stderr, "Domain:%d\n", err->domain);
	fprintf(stderr, "Message:%s:\n", err->message);
	fprintf(stderr, "Level:%d\n", err->level);
	fprintf(stderr, "str1:%s\n", err->str1);
}

void Monitor::virConnectCloseCallBack(virConnectPtr conn ATTRIBUTE_UNUSED,
             				int reason,
             				void *opaque ATTRIBUTE_UNUSED) {
	switch ((virConnectCloseReason) reason) {
    case VIR_CONNECT_CLOSE_REASON_ERROR:
        fprintf(stderr, "Connection closed due to I/O error\n");
        return;

    case VIR_CONNECT_CLOSE_REASON_EOF:
        fprintf(stderr, "Connection closed due to end of file\n");
        return;

    case VIR_CONNECT_CLOSE_REASON_KEEPALIVE:
        fprintf(stderr, "Connection closed due to keepalive timeout\n");
        return;

    case VIR_CONNECT_CLOSE_REASON_CLIENT:
        fprintf(stderr, "Connection closed due to client request\n");
        return;

   default:
        break;
    };

    fprintf(stderr, "Connection closed due to unknown reason\n");
}

// Libvirt event call back. Handle the VM shutdown/start/reboot event
int Monitor::virVMEventCallBack(virConnectPtr conn ATTRIBUTE_UNUSED,
							  virDomainPtr dom, int event,
							  int detail, void *data) {
    return 0;
}


/*
** Get the real cpu of VM. Return -1 if failed.
*/
map<int,int> Monitor::getVMRealCPU(virDomainPtr dom_ptr) {
	map<int, int> vCPU_pCPU_map;
	virError err;
	int n_vCPU = virDomainGetVcpusFlags(dom_ptr, VIR_DOMAIN_VCPU_LIVE);	
	if(n_vCPU == -1) {
		virCopyLastError(&err);
	    fprintf(stderr, "virDomainGetVcpusFlags failed: %s\n", err.message);
	    virResetError(&err);
	    return vCPU_pCPU_map;
	}
	virVcpuInfoPtr vcpu_info_list = new virVcpuInfo[n_vCPU];	
	n_vCPU = virDomainGetVcpus(dom_ptr, vcpu_info_list, n_vCPU, NULL, 0);
	if(n_vCPU == -1) {
		virCopyLastError(&err);
	    fprintf(stderr, "virDomainGetVcpus failed: %s\n", err.message);
	    virResetError(&err);
	}else{		
		for(int i = 0; i < n_vCPU; ++i) {
			vCPU_pCPU_map[vcpu_info_list[i].number] = vcpu_info_list[i].cpu;			
		}
		
	}

	delete[] vcpu_info_list;
	return vCPU_pCPU_map;
}

/* main procedure of Monitor */


/*
**	Start Monitor.
*/
void Monitor::start() {
	int ret = 0;
	ret = init();
	if(ret == -1) {
		fprintf(stderr, "Monitor initialize failed.\n");
		exit(1);
	}

	// Test: print all the infomation.
	printf("Number of NUMA Nodes:\t%d\n", numa_number);
	printf("Number of CPU Cores:\t%d\n", cpu_number);
	printf("netdev:\t%s\n", netdev);
	printf("netdev_pci_bus_str:\t%s\n", netdev_pci_bus_str);
	printf("netdev_pci_slot_str:\t%s\n", netdev_pci_slot_str);
	printf("netdev_pci_funct_str:\t%s\n", netdev_pci_funct_str);
	printf("netdev is attated to Node:\t%d\n", netdev_numanode);
	printf("monitor_interval:\t%u second\n", monitor_interval);
	printf("io_threshold:\t%llu\n", io_threshold);

	printf("\n====================APM====================\n");
	for(int i = 0; i < APM.size(); ++i) {
		for(auto v : APM[i]) 
			printf("%lf\t", v);
		printf("\n");
	}

	printf("\n====================ANM====================\n");
	for(int i = 0; i < ANM.size(); ++i) {
		printf("%lf\t", ANM[i]);					
	}
	printf("\n");

	printf("\n====================ANP====================\n");
	for(int i = 0; i < ANP.size(); ++i) {
		for(auto v : ANP[i]) 
			printf("%lf\t", v);
		printf("\n");					
	}
	printf("\n");

	printf("\n====================VM Info====================\n");
	map<int, VM>::iterator mit = vm_infos_map.begin();
	for(; mit != vm_infos_map.end(); mit++) {
		VM &vm_info = mit->second;
		printf("dom_id:\t%d\n", vm_info.dom_id);		
		printf("vm_name:\t%s\n", vm_info.vm_name);		
		printf("vf_no:\t%d\n", vm_info.vf_no);
		printf("pid:\t%u\n", vm_info.pid);
		printf("number of vCPU:\t %u\n", vm_info.vCPU_list.size());
		printf("----------------------------------------------\n");
	}
	while(1) {
		if (virEventRunDefaultImpl() < 0) {
            fprintf(stderr, "Failed to run event loop: %s\n",
                    virGetLastErrorMessage());
        }
		for(auto it : vm_infos_map) {
			VM &vm = mit->second;
			vm.start();
			vm.printVMInfo();
		}
		sleep(1);
	}
}
