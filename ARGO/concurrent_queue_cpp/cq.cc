/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>
*/


#include "cq.h"
#include <iostream>
/******************************************
 * tail-> item2 <- item1 <- item0 <- head *
 *        next<-item               *
 *     push > tail...head > pop      *
 ******************************************/


void concurrent_queue::push(int val) {
	item *new_item = argo::new_<item>();

	enq_lock->lock();
	for (int i = 0; i < num_sub_items; i++) {
		(new_item->si + i)->val = val;
	}
	new_item->next = NULL;
	tail->next = new_item;
	tail = new_item;
	enq_lock->unlock();
}

bool concurrent_queue::pop(int &out) {
	deq_lock->lock();
	item *node = head;
	item *new_head = node->next;
	if (new_head == NULL) {
		deq_lock->unlock();
		return false;
	}
	out = (new_head->si)->val;

	head = new_head;
	deq_lock->unlock();

	argo::delete_(node);
	return true;
}


void concurrent_queue::init(int n) {

	num_sub_items = n;
	item *new_item = argo::new_<item>();
	for (int i = 0; i < num_sub_items; i++) {
		(new_item->si + i)->val = -1;
	}
	new_item->next = NULL;
	head = new_item;
	tail = new_item;
}


concurrent_queue::concurrent_queue() {
	head = NULL; 
	tail = NULL;
	enq_lock = argo::new_<argo::globallock::cohort_lock>();
	deq_lock = argo::new_<argo::globallock::cohort_lock>();
	num_sub_items = 0;
}

concurrent_queue::~concurrent_queue() {
	int temp;
	while(pop(temp));
	
	argo::delete_(enq_lock);
	argo::delete_(deq_lock);
}
