#! /usr/bin/python
import sys
import pprint
sys.path.append('build/')
sys.path.append('../../build/')
import tuning_data_pb2
import json
from google.protobuf import text_format
from z3 import *

def GetParameters(trace, base_params, param_names, tuning, absolutes):
  for machine in trace.trace_elements:
    for param in machine.tuneable_params:
      if (not param.key in base_params):
        temp = Real(param.key)
        base_params[param.key] = param
        param_names.append(param.key)
        absolute = Real('absolute')
        absolute = If(temp >= 0, temp / param.value, -temp / param.value)
        absolutes[param.key] = absolute
        tuning[param.key] = temp

def BuildProblem(transitions, base_params, param_names, tuning, absolutes):
  opt = Optimize()
  tuned_params = set()
  # Build the problem
  for trans in transitions:
    z3_transition = BoolVal('transition')
    constrained = trans.human_constraint
    first_block = True
    for block in trans.blocks:
      z3_block = BoolVal('block')
      first_clause = True
      for clause in block.clauses:
        lhs = clause.lhs
        rhs = clause.rhs
        if (constrained):
          tuned_params.add(rhs)
        else:
          continue
        z3_clause = BoolVal('clause')
        # Identify which comparator is used to add statement
        if (clause.comparator == '>'):
          z3_clause = RealVal(lhs) > \
            RealVal(base_params[rhs].value) + tuning[rhs]
        elif (clause.comparator == '<'):
          z3_clause = RealVal(lhs) < \
            RealVal(base_params[rhs].value) + tuning[rhs]
        # Need to add additional comparator handling
        if (first_clause):
          z3_block = z3_clause
          first_clause = False
        else:
          if (clause.and_):
            z3_block = And(z3_block, z3_clause)
          else:
            z3_block = Or(z3_block, z3_clause)
    # Combine the blocks
    if (first_block):
      z3_transition = z3_block
      first_block = False
    else:
      if (block.and_):
        z3_transition = And(z3_transition, z3_block)
      else:
        z3_transition = Or(z3_transition, z3_block)
    if (constrained):
      if (trans.should_transition):
        opt.add_soft(z3_transition)
      else:
        opt.add_soft(Not(z3_transition))

  # Set min and max range on tuned_params
  for param in tuned_params:
    opt.add(tuning[param] > RealVal(-5000.0))
    opt.add(tuning[param] < RealVal(5000.0))

  return opt, tuned_params

def my_safe_repr(object, context, maxlevels, level):
    typ = pprint._type(object)
    if typ is unicode:
        object = str(object)
    return pprint._safe_repr(object, context, maxlevels, level)

def Solve(opt, base_params, tuned_params, tuning):
  if(opt.check()):
    config_map = {}
    kParamMultiplier = 1.0
    for key, param in base_params.iteritems():
      value = param.value
      range_min = param.min
      range_max = param.max
      if (param.key in tuned_params):
        z3_value = opt.model()[tuning[param.key]]
        numerator = z3_value.numerator_as_long()
        denom = z3_value.denominator_as_long()
        if (denom > 0.0):
          delta = float(numerator) / float(denom)
          value += delta
      config_map[param.key] = value
    pp = pprint.PrettyPrinter(indent=4)
    # print(opt.model)
    # print(opt.objectives())
    print(json.dumps(config_map, sort_keys = True, 
              indent = 4, ensure_ascii = False))

def ReadTraceFromFile(filename):
  trace = tuning_data_pb2.Trace()
  try:
    f = open(filename, "rb")
    text_format.Parse(f.read(), trace)
  except IOError:
    print("Could not open file: " + filename)
  return trace

def TransitionsFromTrace(trace, machine_name):
  transitions = []
  for machine in trace.trace_elements:
    if (machine_name == machine.machine_name):
      transitions.extend(machine.transitions)
  return transitions

def SRTR(filename, machine_name):
  trace = ReadTraceFromFile(filename)
  transitions = TransitionsFromTrace(trace, machine_name)

  base_params = {}
  tuning = {}
  absolutes = {}
  param_names = []
  GetParameters(trace, base_params, param_names, tuning, absolutes)
  opt, tuned_params = \
    BuildProblem(transitions, base_params, param_names, tuning, absolutes)
  # Add Minimization
  # Maximizing the change
  # opt.push()
  # max_sum = Real('min_sum')
  # first_sum = True
  # for param in tuned_params:
  #   if (first_sum):
  #     max_sum = (tuning[param] / base_params[param].value)
  #     first_sum = False
  #   else:
  #     max_sum += (tuning[param] / base_params[param].value)
  # opt.maximize(max_sum)
  # Solve(opt, base_params, tuned_params, tuning)
  # opt.pop()
  # opt.push()
  # # Minimizing the change
  # min_sum = Real('min_sum')
  # first_sum = True
  # for param in tuned_params:
  #   if (first_sum):
  #     min_sum = tuning[param] / base_params[param].value
  #     first_sum = False
  #   else:
  #     min_sum += tuning[param] / base_params[param].value
  # opt.minimize(min_sum)
  # Solve(opt, base_params, tuned_params, tuning)
  # opt.pop()
  # Minimizing the absolute percent change
  min_change = Real('min_change')
  first_sum = True
  for param in tuned_params:
    if (first_sum):
      min_change = absolutes[param]
      first_sum = False
    else:
      min_change += absolutes[param]
  opt.minimize(min_change)

  Solve(opt, base_params, tuned_params, tuning)

# Check for input file
if (len(sys.argv) != 3):
  print("Expects 2 arguments: path to input correction trace, and a string \
         for the state machine to repair.")
  raise SystemExit

SRTR(sys.argv[1], sys.argv[2])
