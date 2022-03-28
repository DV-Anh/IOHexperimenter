#pragma once
#include <fstream>
#include <stdexcept>
#include "ioh/problem/problem.hpp"
#include "ioh/problem/transformation.hpp"

namespace ioh
{
    namespace problem
    {
        namespace submodular

        {
            class PackWhileTravel final : public Integer
            {
                double velocity_gap, velocity_max, capacity, penalty;
                std::vector<double> *distances;
                std::vector<std::vector<double>> *weights, *profits;
                std::vector<std::vector<int>> *index_map;
                bool is_initialized;

                
                // Read instance list from file
                static std::vector<std::string>
                read_meta_list_instance(const std::string &path_to_meta_list_instance = "example_list_pwt")
                {
                    std::vector<std::string> l{};
                    std::ifstream list_data(path_to_meta_list_instance);
                    if (!list_data)
                    {
                        std::cout << "Fail to instance list file for PWT: " << path_to_meta_list_instance << std::endl;
                        std::cout << "Skip reading instance list file" << std::endl;
                        return {};
                    }
                    std::string str{};
                    while (std::getline(list_data, str))
                    {
                        if (str.empty()) // Skip over empty lines
                            continue;
                        l.push_back(str);
                    }
                    return l;
                }


                // Read TTP instance from file, convert to PWT instance, return problem dimension
                int read_instance_by_id(const int instance, const std::string &instance_list_file)
                {
                    is_initialized = false;
                    std::vector<std::string> instance_list = read_meta_list_instance(instance_list_file);
                    if (instance_list.size() <= instance || instance < 0) // If instance id is invalid,
                                               // return a valid dummy size
                        return 1;
                    std::ifstream ttp_data(instance_list[instance]);
                    if (!ttp_data)
                        throw std::invalid_argument("Fail to open instance file: " + (instance_list[instance]));
                    std::string str, tstr;
                    int index_line = 0;
                    while (std::getline(ttp_data, str) && index_line++ < 2) // Skip 2 lines, to line 3
                        ;
                    int n_cities = std::stoi(str.substr(str.find_last_of(':') + 1));
                    std::getline(ttp_data, str);
                    int n_items = std::stoi(str.substr(str.find_last_of(':') + 1));
                    std::getline(ttp_data, str);
                    capacity = std::stod(str.substr(str.find_last_of(':') + 1));
                    std::getline(ttp_data, str);
                    velocity_gap = std::stod(str.substr(str.find_last_of(':') + 1));
                    std::getline(ttp_data, str);
                    velocity_max = std::stod(str.substr(str.find_last_of(':') + 1));
                    velocity_gap = velocity_max - velocity_gap;
                    std::getline(ttp_data, str);
                    double rent_ratio = std::stod(str.substr(str.find_last_of(':') + 1));
                    while (std::getline(ttp_data, str) && index_line++ < 5) // Skip 2 lines, to line 11
                        ;
                    double cur_x, cur_y, next_x, next_y, init_x, init_y, distance;
                    int first_space = str.find_first_of('	'), second_space = str.find_last_of('	');
                    init_x = std::stod(str.substr(first_space + 1, second_space - first_space - 1));
                    init_y = std::stod(str.substr(second_space + 1));
                    cur_x = init_x;
                    cur_y = init_y;
                    index_line = 0;
                    penalty = 0;
                    distances = new std::vector<double>();
                    weights = new std::vector<std::vector<double>>();
                    profits = new std::vector<std::vector<double>>();
                    index_map = new std::vector<std::vector<int>>();
                    while (std::getline(ttp_data, str) && index_line++ < n_cities - 1) // Read until next header
                    {
                        first_space = str.find_first_of('	'), second_space = str.find_last_of('	');
                        next_x = std::stod(str.substr(first_space + 1, second_space - first_space - 1));
                        next_y = std::stod(str.substr(second_space + 1));
                        distance =
                            std::ceil(std::sqrt(std::pow(next_x - cur_x, 2) + std::pow(next_y - cur_y, 2))); // CEIL_2D
                        distances->push_back(distance);
                        penalty -= distance;
                        cur_x = next_x;
                        cur_y = next_y;
                    }
                    distance =
                        std::ceil(std::sqrt(std::pow(init_x - cur_x, 2) + std::pow(init_y - cur_y, 2))); // CEIL_2D
                    distances->push_back(distance);
                    penalty =
                        (penalty - distance) * rent_ratio / (velocity_max - velocity_gap); // Complete penalty term
                    index_line = 0;
                    int city_index;
                    while (std::getline(ttp_data, str) && index_line < n_items)
                    {
                        tstr = str.substr(str.find_first_of('	') + 1);
                        first_space = tstr.find_first_of('	');
                        second_space = tstr.find_last_of('	');
                        city_index = std::stoi(tstr.substr(second_space + 1)) - 1;
                        while (weights->size() <= city_index)
                        {
                            weights->push_back({});
                            profits->push_back({});
                            index_map->push_back({});
                        }
                        (*index_map)[city_index].push_back(index_line++);
                        (*profits)[city_index].push_back(std::stod(tstr.substr(0, first_space)));
                        (*weights)[city_index].push_back(
                            std::stod(tstr.substr(first_space + 1, second_space - first_space - 1)));
                    }
                    is_initialized = true;
                    return n_items; // Dimension is number of items
                }

            protected:
                //! Variables transformation method
                std::vector<int> transform_variables(std::vector<int> x) override { return x; }
                //! Objectives transformation method
                double transform_objectives(const double y) override { return y; }
                // [mandatory] The evaluate method is mandatory to implement
                double evaluate(const std::vector<int> &x) override
                {
                    double profit_sum = 0, cons = 0, time = 0;
                    for (auto i = 0; i < weights->size(); i++)
                    {
                        for (auto j = 0; j < (*weights)[i].size(); j++)
                        {
                            if (x[(*index_map)[i][j]] >= 1)
                            {
                                cons += (*weights)[i][j];
                                profit_sum += (*profits)[i][j];
                            }
                        }
                        if (cons > capacity) // Assuming weights are non-negative
                            continue; // Keep adding weights, but ignore time to avoid division by 0
                        time += (*distances)[i] / (velocity_max - (velocity_gap * cons / capacity));
                    }
                    if (cons > capacity)
                        return capacity - cons + penalty;
                    return profit_sum - time;
                }

            public:
                // Check if instance is null
                bool is_null() { return (!is_initialized) || distances->empty(); }

                // Constructor
                PackWhileTravel(const int instance = 1, const int n_variables = 1,
                                const std::string &instance_list_file = "example_list_pwt") :
                    Integer(MetaData(instance + 3000000,// problem id, starting at 3000000
                        instance, "PackWhileTravel" + std::to_string(instance),
                        read_instance_by_id(instance - 1, instance_list_file), // n_variables will be updated based on the given instance.
                        common::OptimizationType::Maximization),
                        Constraint<int>(n_variables, 0, 1)
                    )
                {
                    if (is_null())
                    {
                        std::cout << "Instance not created properly (e.g. invalid id)." << std::endl;
                        return;
                    }
                    if (velocity_gap >= velocity_max || velocity_gap <= 0 || velocity_max <= 0)
                        throw std::invalid_argument(
                            "Minimum velocity must be positive and smaller than maximum velocity");
                    if (capacity <= 0)
                        throw std::invalid_argument("Capacity must be positive");
                    if (weights->size() != profits->size() || weights->size() != distances->size())
                        throw std::invalid_argument("Weights, profits, and number of cities don't match");
                    for (int i = 0; i < weights->size(); i++)
                    {
                        if ((*weights)[i].size() != (*profits)[i].size())
                            throw std::invalid_argument("Weights and profits don't match");
                    }
                    for (int i = 0; i < weights->size(); i++)
                    {
                        for (int j = 0; j < (*weights)[i].size(); j++)
                        {
                            if ((*weights)[i][j] < 0 || (*profits)[i][j] < 0)
                                throw std::invalid_argument("Weights and profits must be non-negative");
                        }
                    }
                    objective_.x = std::vector<int>(meta_data_.n_variables, 1);
                    objective_.y = evaluate(objective_.x);
                }
            };
        } // namespace submodular
    } // namespace problem
} // namespace ioh