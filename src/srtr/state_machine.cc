// Copyright 2017-2018 jaholtz@cs.umass.edu
// College of Information and Computer Sciences,
// University of Massachusetts Amherst
//
//
// This software is free: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License Version 3,
// as published by the Free Software Foundation.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// Version 3 in the file COPYING that came with this distribution.
// If not, see <http://www.gnu.org/licenses/>.
// ========================================================================
#include "srtr/state_machine.h"

#include <string>

using std::map;
using std::unique_ptr;
using MinuteBotsProto::StateMachineData;
namespace srtr {

StateMachine::StateMachine(const string& machine_name) :
         machine_name_(machine_name) {}

StateMachine::RepairableParam::RepairableParam(const float& value,
                                                     const string& name,
                                                     StateMachine* parent)
: value_(value),
  name_(name),
  parent_(parent) {
    parent_->SetupMessage();
}

void StateMachine::AddBlock(const bool& is_and) {
  bool found = false;
  // Search for a message with this start->potential combo
  for (int i = 0; i < log_message_.transitions_size(); ++i) {
    MinuteBotsProto::PossibleTransition* transition
        = log_message_.mutable_transitions(i);
    // If our message contains this transition add a block to it
    if (transition->potential_state().compare(potential_state_) == 0 &&
        transition->start_state().compare(state_.name_) == 0) {
      found = true;
      MinuteBotsProto::TransitionBlock block;
      block.set_and_(is_and);
      *(transition->add_blocks()) = block;
      // Set the block index
      block_index_ = transition->blocks_size() - 1;
    }
  }
  // If you didn't find it then we have to add the entire
  // potential transition and the block
  if (!found) {
    MinuteBotsProto::PossibleTransition transition;
    transition.set_potential_state(potential_state_);
    transition.set_start_state(state_.name_);
    transition.set_human_constraint(false);
    MinuteBotsProto::TransitionBlock block;
    block.set_and_(is_and);
    *(transition.add_blocks()) = block;
    *(log_message_.add_transitions()) = transition;
    // Set the block index
    block_index_ = transition.blocks_size() - 1;
  }
}

// Sets the outcome of the currently considered transition block.
void StateMachine::SetTransition(const bool& should_transition) {
  // Search for a message with this start->potential combo
  for (int i = 0 ; i < log_message_.transitions_size(); ++i) {
    MinuteBotsProto::PossibleTransition* transition =
        log_message_.mutable_transitions(i);
    if (transition->potential_state().compare(potential_state_) == 0 &&
        transition->start_state().compare(state_.name_) == 0) {
      transition->set_should_transition(should_transition);
    }
  }
}

bool StateMachine::RepairableParam::operator>(const float& x) {
  // Parents log message to fill in
  StateMachineData* log_message = &parent_->log_message_;
  // Adds the parameter itself to the parameter map when it is used
  MinuteBotsProto::MapFieldEntry entry;
  entry.set_key(name_);
  entry.set_value(value_);
  (*log_message->add_tuneable_params()) = entry;

  MinuteBotsProto::TransitionClause clause;
  clause.set_lhs(x);
  clause.set_rhs(name_);
  clause.set_comparator("<");
  clause.set_and_(parent_->and_clause_);  // How to fill this in?
  *(log_message->add_clauses()) = clause;
  string state = parent_->potential_state_;  // How to identify this
  int block_index = parent_->block_index_;  // How to identify this?
  bool found = false;
  // Looks to see if we already have a partial entry for this transition
  for (int i = 0; i < log_message->transitions_size(); ++i) {
    MinuteBotsProto::PossibleTransition* transition =
        log_message->mutable_transitions(i);
    // If we have a partial entry we add to it
    if (transition->potential_state().compare(state) == 0 &&
        transition->start_state().compare(parent_->state_.name_) == 0) {
      found = true;
      *(transition->add_clauses()) = clause;
      MinuteBotsProto::TransitionBlock* block =
            transition->mutable_blocks(block_index);
      *(block->add_clauses()) = clause;
    }
  }
  // If we don't have a partial entry we make one.
  if (!found) {
    MinuteBotsProto::PossibleTransition transition;
    transition.set_potential_state(state);
    transition.set_start_state(parent_->state_.name_);
    transition.set_human_constraint(false);
    *(transition.add_clauses()) = clause;
    *(log_message->add_transitions()) = transition;
  }

  // Do the actual comparison
  return value_ > x;
}

bool operator>(const float& x, StateMachine::RepairableParam y) {
  return y < x;
}

bool operator<(const float& x, StateMachine::RepairableParam y) {
  return y > x;
}

bool StateMachine::RepairableParam::operator<(const float& x) {
  // Parents log message to fill in
  StateMachineData* log_message = &parent_->log_message_;
  // Adds the parameter itself to the parameter map when it is used
  MinuteBotsProto::MapFieldEntry entry;
  entry.set_key(name_);
  entry.set_value(value_);
  (*log_message->add_tuneable_params()) = entry;

  MinuteBotsProto::TransitionClause clause;
  clause.set_lhs(x);
  clause.set_rhs(name_);
  clause.set_comparator(">");
  clause.set_and_(parent_->and_clause_);  // How to fill this in?
  *(log_message->add_clauses()) = clause;
  string state = parent_->potential_state_;  // How to identify this
  int block_index = parent_->block_index_;  // How to identify this?
  bool found = false;
  // Looks to see if we already have a partial entry for this transition
  for (int i = 0; i < log_message->transitions_size(); ++i) {
    MinuteBotsProto::PossibleTransition* transition =
    log_message->mutable_transitions(i);
    // If we have a partial entry we add to it
    if (transition->potential_state().compare(state) == 0 &&
      transition->start_state().compare(parent_->state_.name_) == 0) {
      found = true;
    *(transition->add_clauses()) = clause;
    MinuteBotsProto::TransitionBlock* block =
    transition->mutable_blocks(block_index);
    *(block->add_clauses()) = clause;
      }
  }
  // If we don't have a partial entry we make one.
  if (!found) {
    MinuteBotsProto::PossibleTransition transition;
    transition.set_potential_state(state);
    transition.set_start_state(parent_->state_.name_);
    transition.set_human_constraint(false);
    *(transition.add_clauses()) = clause;
    *(log_message->add_transitions()) = transition;
  }

  // Do the actual comparison
  return value_ < x;
}

StateMachine::RepairableParam::operator float() {
  return value_;
}

void StateMachine::SetupMessage() {
  log_message_.Clear();
  log_message_.set_machine_name(machine_name_);
  log_message_.set_state(state_.name_);
  log_message_.set_human_constraint(false);
}

void StateMachine::Run() {
  SetupMessage();
  Transition();
  state_();
  // TODO(jaholtz) Add Code to build composite message and write to a file.
}

}  // namespace srtr
