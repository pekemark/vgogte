/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>

This file uses the PersistentCache to enable multi-threaded updates to it.
*/

#include "argo.hpp"
#include "cohort_lock.hpp"
#include "backend/mpi/persistence.hpp"

#include <cstdint>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#include <string>
#include <fstream>
#include <iostream>

#define NUM_ELEMS_PER_DATUM 2 
#define NUM_ROWS 1000
#define NUM_UPDATES 1000
#define NUM_THREADS 4

// Macro for only node0 to do stuff
#define MAIN_PROC(rank, inst) \
do { \
	if ((rank) == 0) { inst; } \
} while (0)

int workrank;
int numtasks;

void distribute(int& beg,
		int& end,
		const int& loop_size,
		const int& beg_offset,
    		const int& less_equal){
	int chunk = loop_size / numtasks;
	beg = workrank * chunk + ((workrank == 0) ? beg_offset : less_equal);
	end = (workrank != numtasks - 1) ? workrank * chunk + chunk : loop_size;
}

// Persistent cache organization. Each Key has an associated Value in the data array.
// Each datum in the data array consists of up to 8 elements. 
// The number of elements actually used by the application is a runtime argument.
struct Element {
	int32_t value_[NUM_ELEMS_PER_DATUM];
};

struct Datum {
	// pointer to the hashmap
	Element* elements_;
	// A lock which protects this Datum
	argo::backend::persistence::persistence_lock<argo::globallock::cohort_lock>* lock_;
};

struct pc {
	Datum* hashmap;
	int num_rows_;
	int num_elems_per_row_;
};

uint32_t pc_rgn_id;
pc* P;

void datum_init(pc* p) {
	for(int i = 0; i < NUM_ROWS; i++) {
		p->hashmap[i].elements_ = argo::conew_<Element>();
		p->hashmap[i].lock_ = new argo::backend::persistence::persistence_lock<argo::globallock::cohort_lock>(new argo::globallock::cohort_lock());
		persistence_registry.get_tracker()->join_apb(); // TODO: Shouldn't be needed. Incorporate APBs in normal barriers?
	}
	MAIN_PROC(workrank, std::cout << "Finished allocating elems & locks" << std::endl);

	int beg, end;
	distribute(beg, end, NUM_ROWS, 0, 0);

	std::cout << "rank: "                << workrank
	          << ", initializing from: " << beg
		  << " to: "                 << end
		  << std::endl;

	for(int i = beg; i < end; i++) {
		for(int j = 0; j < NUM_ELEMS_PER_DATUM; j++)
			p->hashmap[i].elements_->value_[j] = 0;
		persistence_registry.get_tracker()->join_apb();
	}
	MAIN_PROC(workrank, std::cout << "Finished team process initialization" << std::endl);
}

void datum_free(pc* p) {
	for(int i = 0; i < NUM_ROWS; i++) {
		delete p->hashmap[i].lock_->get_lock();
		delete p->hashmap[i].lock_;
		argo::codelete_(p->hashmap[i].elements_);
	}
}

void datum_set(int key, int value) {
	P->hashmap[key].lock_->lock();

	for(int j = 0; j < NUM_ELEMS_PER_DATUM; j++)
		P->hashmap[key].elements_->value_[j] = value+j;

	P->hashmap[key].lock_->unlock();
}

void initialize() {
	P = new pc;
	P->num_rows_ = NUM_ROWS;
	P->num_elems_per_row_ = NUM_ELEMS_PER_DATUM;
	P->hashmap = new Datum[NUM_ROWS];
	datum_init(P);
	argo::backend::persistence::apb_barrier(&argo::barrier, 1UL);

	MAIN_PROC(workrank, fprintf(stderr, "Created hashmap at %p\n", (void *)P->hashmap));
}

void* CacheUpdates(void* arguments) {
	persistence_registry.register_thread();
	int key;
	for (int i = 0; i < NUM_UPDATES/(NUM_THREADS*numtasks); i++) {
		key = rand()%NUM_ROWS;
		datum_set(key, i);
	}
	persistence_registry.unregister_thread();
	return 0;
}

int main (int argc, char* argv[]) {
	argo::init(500*1024*1024UL);
	persistence_registry.register_thread();

	workrank = argo::node_id();
	numtasks = argo::number_of_nodes();

	MAIN_PROC(workrank, std::cout << "In main\n" << std::endl);
	struct timeval tv_start;
	struct timeval tv_end;

	std::ofstream fexec;
	MAIN_PROC(workrank, fexec.open("exec.csv",std::ios_base::app));

	// This contains the Atlas restart code to find any reusable data
	initialize();
	MAIN_PROC(workrank, std::cout << "Done with cache creation" << std::endl);

	pthread_t threads[NUM_THREADS];

	persistence_registry.get_tracker()->allow_apb();

	gettimeofday(&tv_start, NULL);
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, CacheUpdates, NULL);
	}
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	argo::backend::persistence::apb_barrier(&argo::barrier, 1UL);
	gettimeofday(&tv_end, NULL);

	MAIN_PROC(workrank, fprintf(stderr, "time elapsed %ld us\n",
				tv_end.tv_usec - tv_start.tv_usec +
				(tv_end.tv_sec - tv_start.tv_sec) * 1000000));
	MAIN_PROC(workrank, fexec << "PC" << ", " << std::to_string((tv_end.tv_usec - tv_start.tv_usec) + (tv_end.tv_sec - tv_start.tv_sec) * 1000000) << std::endl);
	MAIN_PROC(workrank, fexec.close());

	datum_free(P);
	delete[] P->hashmap;
	delete P;

	MAIN_PROC(workrank, std::cout << "Done with persistent cache" << std::endl);

	persistence_registry.unregister_thread();
	argo::finalize();

	return 0;
}
