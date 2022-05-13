/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>
*/

#include "argo.hpp"
#include "cohort_lock.hpp"
#include "backend/mpi/persistence.hpp"

#include <cstdlib>
#include <cstdint>
#include <pthread.h>

#include <vector>

#define NUM_SUB_ITEMS 8 
#define NUM_OPS 8192
#define NUM_THREADS 4 

typedef struct item item;

struct sub_item {
	int val;
	sub_item& operator=(sub_item& other) {
		val = other.val;
		return *this;
	}
};

struct item {
	item* next;
	sub_item si[NUM_SUB_ITEMS];
	item() {
		next = NULL;
	};
	item& operator=(item& other) {
		for (int i= 0; i< NUM_SUB_ITEMS; i++) {
			*(si + i) = *(other.si + i);
		}
		return *this;
	}
};

class concurrent_queue {
	item **head;
	item **tail;
	argo::backend::persistence::persistence_lock<argo::globallock::cohort_lock> *enq_lock;
	argo::backend::persistence::persistence_lock<argo::globallock::cohort_lock> *deq_lock;
	int num_sub_items;

	public:
	concurrent_queue();
	~concurrent_queue();
	void push(int);
	bool pop(int&);
	void init();
	void check();
};
