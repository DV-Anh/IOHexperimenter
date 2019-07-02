/// \file f_leading_ones_dummy2.hpp
/// \brief cpp file for class f_leading_ones_dummy2.
///
/// This file implements a LeadingOnes problem with reduction of dummy variables.
/// The reduction rate is chosen as 0.9.
///
/// \author Furong Ye
/// \date 2019-06-27
#ifndef _F_LEADING_ONES_DUMMYTWI_H
#define _F_LEADING_ONES_DUMMYTWO_H

#include "../IOHprofiler_problem.hpp"
#include "common_used_functions/wmodels.hpp"

class LeadingOnes_Dummy2 : public IOHprofiler_problem<int> {
public:
   LeadingOnes_Dummy2() {
    IOHprofiler_set_problem_name("LeadingOnes_Dummy2");
    IOHprofiler_set_problem_type("pseudo_Boolean_problem");
    IOHprofiler_set_lowerbound(0);
    IOHprofiler_set_upperbound(1);
    IOHprofiler_set_best_variables(1);
    IOHprofiler_set_number_of_objectives(1);
  
  }
  LeadingOnes_Dummy2(int instance_id, int dimension) {
    IOHprofiler_set_instance_id(instance_id);
    IOHprofiler_set_problem_name("LeadingOnes_Dummy2");
    IOHprofiler_set_problem_type("pseudo_Boolean_problem");
    IOHprofiler_set_number_of_objectives(1);
    IOHprofiler_set_lowerbound(0);
    IOHprofiler_set_upperbound(1);
    IOHprofiler_set_best_variables(1);
    Initilize_problem(dimension);
  }
  
  ~LeadingOnes_Dummy2() {}; 

  void Initilize_problem(int dimension) {
    IOHprofiler_set_number_of_variables(dimension);
    IOHprofiler_set_optimal(floor((double)(dimension * 0.9)));
  };

  void update_evaluate_int_info() {
    int length = IOHprofiler_get_number_of_variables();
    IOHprofiler_set_evaluate_int_info(dummy(length,0.9,10000));
  }
  
  std::vector<double> internal_evaluate(std::vector<int> x) {
    std::vector<double> y;
    int n = IOHprofiler_get_evaluate_int_info().size();
    int result = 0;
    for (int i = 0; i != n; ++i) {
      if (x[IOHprofiler_get_evaluate_int_info()[i]] == 1) {
        result = i + 1;
      } else {
        break;
      }
    }
    y.push_back((double)result);
    return y;
  };

  static LeadingOnes_Dummy2 * createInstance() {
    return new LeadingOnes_Dummy2();
  };

  static LeadingOnes_Dummy2 * createInstance(int instance_id, int dimension) {
    return new LeadingOnes_Dummy2(instance_id, dimension);
  };
};

#endif