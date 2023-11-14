#include <iostream>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <time.h>
#include <string.h>
#include <stdexcept>

int main(){
	char buffer[256];
	char *ip = "127.0.0.1";
	char *port = "5678";
	struct rdma_addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = RAI_PASSIVE;
	hints.ai_port_space = RDMA_PS_TCP;

	struct rdma_addrinfo* res = NULL;

	const int getAddrRet = rdma_getaddrinfo(ip, port, &hints, &res);

	if(getAddrRet)
	{
		throw std::runtime_error(std::string("rdma_getaddrinfo failed: ") + std::string(strerror_r(errno, buffer, sizeof(buffer))));
		buffer[sizeof(buffer) - 1] = 0;
	}

	struct ibv_qp_init_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.cap.max_send_wr = 10000;
	attr.cap.max_recv_wr = 10000;
	attr.cap.max_send_sge = 1;
	attr.cap.max_recv_sge = 1;
	attr.cap.max_inline_data = 0;
	attr.sq_sig_all = 1;

	struct rdma_cm_id* listenID = NULL;

	const int createEpRet = rdma_create_ep(&listenID, res, NULL, &attr);
	if(createEpRet == 0)
	{
		std::cout << "rdma created End Point.\n";
	}
	else
	{
		throw std::runtime_error(std::string("rdma_create_ep failed: ") + std::string(strerror_r(errno, buffer, sizeof(buffer))));
	}

	const int listenRet = rdma_listen(listenID, 0);
	if(listenRet == 0)
	{
		std::cout << "Server listening.\n";
	}
	else
	{
		rdma_destroy_ep(listenID);
		throw std::runtime_error(std::string("rdma_listen failed: ") + std::string(strerror_r(errno, buffer, sizeof(buffer))));
	}
	
	struct rdma_cm_id* clientID = NULL;
	std::cout << "Server Start.\n";
	while(1)
	{

		const int ret = rdma_get_request(listenID, &clientID);
		if(ret)
		{
			throw std::runtime_error(std::string("rdma_get_request failed: ") + std::string(strerror_r(errno, buffer, sizeof(buffer))));
		}
	}
	rdma_freeaddrinfo(res);
	rdma_destroy_ep(listenID);
	
	return 0;
}
