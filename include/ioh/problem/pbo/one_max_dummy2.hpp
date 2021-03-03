#pragma once
#include "ioh/problem/utils.hpp"
#include "pbo_problem.hpp"

namespace ioh
{
    namespace problem
    {
        namespace pbo
        {
            class OneMaxDummy2 final: public PBOProblem<OneMaxDummy2>
            {
            protected:
                std::vector<int> info_;

                std::vector<double> evaluate(const std::vector<int> &x) override
                {
                    auto result = 0.0;
                    for (auto i = 0; i != info_.size(); ++i)
                        result += x[info_[i]];

                    return {result};
                }

            public:
                /**
                 * \brief Construct a new OneMax_Dummy2 object. Definition refers to
                 *https://doi.org/10.1016/j.asoc.2019.106027
                 *
                 * \param instance The instance number of a problem, which controls the transformation
                 * performed on the original problem.
                 * \param n_variables The dimensionality of the problem to created, 4 by default.
                 **/
                OneMaxDummy2(const int instance, const int n_variables) :
                    PBOProblem(5, instance, n_variables, "OneMaxDummy2")
                {
                    info_ = utils::dummy(n_variables, 0.9, 10000);
                    objective_.x = std::vector<int>(n_variables,1);
                    objective_.y = evaluate(objective_.x);
                }
            };
        } // namespace pbo
    } // namespace problem
} // namespace ioh
