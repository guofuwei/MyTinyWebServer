#include "timer_list.h"

TimerList::~TimerList() {
  Timer* tmp = head_;
  while (tmp) {
    head_ = tmp->next_;
    delete tmp;
    tmp = head_;
  }
}

void TimerList::Insert(Timer* timer, Timer* head) {
  Timer* prev = head;
  Timer* tmp = prev->next_;
  while (tmp) {
    if (timer->expire_time_ < tmp->expire_time_) {
      prev->next_ = timer;
      timer->next_ = tmp;
      tmp->prev_ = timer;
      timer->prev_ = prev;
      break;
    }
    prev = tmp;
    tmp = tmp->next_;
  }
  if (!tmp) {
    prev->next_ = timer;
    timer->prev_ = prev;
    timer->next_ = NULL;
    tail_ = timer;
  }
}

void TimerList::Insert(Timer* timer) {
  if (!timer) {
    return;
  }
  if (!head_) {
    head_ = tail_ = timer;
    return;
  }
  if (timer->expire_time_ < head_->expire_time_) {
    timer->next_ = head_;
    head_->prev_ = timer;
    head_ = timer;
    return;
  }
  Insert(timer, head_);
}

void TimerList::Adjust(Timer* timer) {
  if (!timer) {
    return;
  }
  Timer* tmp = timer->next_;
  if (!tmp || (timer->expire_time_ < tmp->expire_time_)) {
    return;
  }
  if (timer == head_) {
    head_ = head_->next_;
    head_->prev_ = NULL;
    timer->next_ = NULL;
    Insert(timer, head_);
  } else {
    timer->prev_->next_ = timer->next_;
    timer->next_->prev_ = timer->prev_;
    Insert(timer, timer->next_);
  }
}

void TimerList::Delete(Timer* timer) {
  if (!timer) {
    return;
  }
  if ((timer == head_) && (timer == tail_)) {
    delete timer;
    head_ = NULL;
    tail_ = NULL;
    return;
  }
  if (timer == head_) {
    head_ = head_->next_;
    head_->prev_ = NULL;
    delete timer;
    return;
  }
  if (timer == tail_) {
    tail_ = tail_->prev_;
    tail_->next_ = NULL;
    delete timer;
    return;
  }
  timer->prev_->next_ = timer->next_;
  timer->next_->prev_ = timer->prev_;
  delete timer;
}

void TimerList::Tick() {
  if (!head_) {
    return;
  }
  Timer* tmp = head_;
  time_t now_time = time(NULL);
  while (tmp && tmp->expire_time_ <= now_time) {
    tmp->callback_func_(tmp->client_data_);
    head_ = tmp->next_;
    if (head_) {
      head_->prev_ = NULL;
    }
    delete tmp;
    tmp = head_;
  }
}