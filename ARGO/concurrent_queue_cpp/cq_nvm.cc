/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>
*/

#include "cq.h"

#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>

#include <string>
#include <fstream>
#include <iostream>

// Macro for only node0 to do stuff
#define MAIN_PROC(rank, inst) \
do { \
	if ((rank) == 0) { inst; } \
} while (0)

int workrank;
int numtasks;

concurrent_queue* CQ;

void initialize() {
	CQ = new concurrent_queue;
	MAIN_PROC(workrank, CQ->init());
	argo::backend::persistence::apb_barrier(&argo::barrier, 1UL);

	MAIN_PROC(workrank, fprintf(stderr, "Created cq at %p\n", (void *)CQ));
}

void* run_stub(void* ptr) {
	persistence_registry.register_thread();
	int ret;
	for (int i = 0; i < NUM_OPS/(NUM_THREADS*numtasks); ++i) {
		CQ->push(9+workrank);
	}
	persistence_registry.unregister_thread();
	return NULL;
}

int main(int argc, char** argv) {
	argo::init(256*1024*1024UL);
	persistence_registry.register_thread();

	workrank = argo::node_id();
	numtasks = argo::number_of_nodes();

	MAIN_PROC(workrank, std::cout << "In main\n" << std::endl);
	struct timeval tv_start;
	struct timeval tv_end;
	
	std::ofstream fexec;
	MAIN_PROC(workrank, fexec.open("exec.csv",std::ios_base::app));

	initialize();

	pthread_t threads[NUM_THREADS];

	persistence_registry.get_tracker()->allow_apb();

	gettimeofday(&tv_start, NULL);
	for (int i = 0; i < NUM_THREADS; ++i) {
		pthread_create(&threads[i], NULL, &run_stub, NULL);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		pthread_join(threads[i], NULL);
	}
	argo::backend::persistence::apb_barrier(&argo::barrier, 1UL);
	gettimeofday(&tv_end, NULL);

	MAIN_PROC(workrank, fprintf(stderr, "time elapsed %ld us\n",
				tv_end.tv_usec - tv_start.tv_usec +
				(tv_end.tv_sec - tv_start.tv_sec) * 1000000));
	MAIN_PROC(workrank, fexec << "CQ" << ", " << std::to_string((tv_end.tv_usec - tv_start.tv_usec) + (tv_end.tv_sec - tv_start.tv_sec) * 1000000) << std::endl);
	MAIN_PROC(workrank, fexec.close());

	MAIN_PROC(workrank, CQ->check());
	argo::backend::persistence::apb_barrier(&argo::barrier, 1UL);

	delete CQ;

	persistence_registry.unregister_thread();
	argo::finalize();

	return 0;
}
