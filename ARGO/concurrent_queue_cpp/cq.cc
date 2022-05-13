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
	(*tail)->next = new_item;
	*tail = new_item;
	enq_lock->unlock();
}

bool concurrent_queue::pop(int &out) {
	deq_lock->lock();
	item *node = *head;
	item *new_head = node->next;
	if (new_head == NULL) {
		deq_lock->unlock();
		return false;
	}
	out = (new_head->si)->val;
	*head = new_head;
	deq_lock->unlock();

	argo::delete_(node);
	return true;
}

void concurrent_queue::init() {
	item *new_item = argo::new_<item>();
	for (int i = 0; i < num_sub_items; i++) {
		(new_item->si + i)->val = -1;
	}
	new_item->next = NULL;
	*head = new_item;
	*tail = new_item;
}

void concurrent_queue::check() {
	std::size_t checksum = 0;
	for (item* check = *head; check != nullptr; check = check->next) {
		for (int i = 0; i < num_sub_items; i++) {
			checksum += (check->si + i)->val;
		}
	}

	std::size_t expectsum = 0;
	std::size_t nodechunk = NUM_OPS / argo::number_of_nodes();
	for (int i = 0; i < argo::number_of_nodes(); ++i) {
		expectsum += (9+i) * nodechunk*num_sub_items;
	}
	expectsum -= num_sub_items;
	assert(checksum == expectsum);
}

concurrent_queue::concurrent_queue() {
	head = argo::conew_<item*>();
	tail = argo::conew_<item*>();
	enq_lock = new argo::backend::persistence::persistence_lock<argo::globallock::cohort_lock>(new argo::globallock::cohort_lock());
	deq_lock = new argo::backend::persistence::persistence_lock<argo::globallock::cohort_lock>(new argo::globallock::cohort_lock());
	num_sub_items = NUM_SUB_ITEMS;
}

concurrent_queue::~concurrent_queue() {
	int temp;
	while(pop(temp));
	
	delete enq_lock->get_lock();
	delete deq_lock->get_lock();
	delete enq_lock;
	delete deq_lock;
	argo::codelete_(head);
	argo::codelete_(tail);
}
