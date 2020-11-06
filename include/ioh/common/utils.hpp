#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ioh
{
	namespace common
	{
		/**
		 * \brief Enum containing minimization = 0 and maximization = 1 flags
		 */
		enum class OptimizationType
		{
			minimization,
			maximization
		};

		namespace log
		{
			/**
			 * \brief Enum containing log levels; info = 0, warning = 1, error = 2
			 */
			enum class Level
			{
				info,
				warning,
				error
			};

			/**
			 * \brief Setting for the log level 
			 */
			inline Level log_level = Level::info;

			/**
			 * \brief Function for logging errors, causes a system exit
			 * \param error_msg The error message
			 */
			inline void error(const std::string& error_msg)
			{
				if (log_level <= Level::error)
				{
					std::cerr << "IOH_ERROR_INFO : " << error_msg << std::endl;
					exit(1);
				}
			}

			/**
			 * \brief Function for logging warnings
			 * \param warning_msg The error message
			 */
			inline void warning(const std::string& warning_msg)
			{
				if (log_level <= Level::warning)
					std::cout << "IOH_WARNING_INFO : " << warning_msg << std::endl;
			}

			/**
			 * \brief Function for logging info messages
			 * \param log_msg The info message
			 */
			inline void info(const std::string& log_msg)
			{
				if (log_level <= Level::info)
					std::cout << "IOH_LOG_INFO : " << log_msg << std::endl;
			}

			/**
			 * \brief Function for logging info messages to a stream
			 * \param log_msg The info message
			 * \param log_stream The stream to log the messages to 
			 */
			inline void info(const std::string& log_msg, std::ofstream& log_stream)
			{
				if (log_level >= Level::info)
					log_stream << "IOH_LOG_INFO : " << log_msg << std::endl;
			}
		}


		/**
		 * \brief Helper function to copy a vector v1 into another vector v2
		 * \tparam T The type of the vectors
		 * \param v1 The vector to copy
		 * \param v2 The vector which receives the values from v2.
		 */
		template <typename T>
		void copy_vector(const std::vector<T> v1, std::vector<T>& v2)
		{
			v2.assign(v1.begin(), v1.end());
		}

		/**
		 * \brief Compares two vectors, returns true if all elements are equal
		 * \tparam T The type of the vectors
		 * \param v1 The first vector
		 * \param v2 The second vector
		 * \return true if all elements of v1 and v2 are equal
		 */
		template <typename T>
		bool compare_vector(const std::vector<T>& v1, const std::vector<T>& v2)
		{
			auto n = v1.size();
			if (n != v2.size())
				log::error("Two compared vectors must be with the same size\n");

			for (auto i = 0; i != n; ++i)
				if (v1[i] != v2[i])
					return false;
			return true;
		}

		/**
		 * \brief Compares two values v1 and v2, and returns true if v1 is better
		 * \tparam T Type of v1 and v2
		 * \param v1 The first value
		 * \param v2 The second value
		 * \param optimization_type Used to determine which value is better,
		 * when optimization_type == \ref OptimizationType::minimization lower elements are better,
		 * otherwise higher elements are better.
		 * \return true if v1 is better than v2
		 */
		template <typename T>
		bool compare_objectives(const T v1, const T v2, const OptimizationType optimization_type)
		{
			if (optimization_type == OptimizationType::maximization)
				return v1 > v2;
			return v1 < v2;
		}

		/**
		 * \brief Compares two vectors v1 and v2, and checks if v1 is better than v2. 
		 * \tparam T The type of the vectors
		 * \param v1 The first vector
		 * \param v2 The second vector
		 * \param optimization_type Used to determine which vector is better,
		 * when optimization_type == \ref OptimizationType::minimization lower elements are better,
		 * otherwise higher elements are better.
		 * \return Return true all of vl's elements are better than v2's.
		 */
		template <typename T>
		bool compare_objectives(const std::vector<T>& v1, const std::vector<T>& v2,
		                        const OptimizationType optimization_type)
		{
			auto n = v1.size();
			if (n != v2.size())
				log::error("Two compared objective vector must be with the same size\n");

			for (auto i = 0; i != n; ++i)
				if (!compare_objectives<T>(v1[i], v2[i], optimization_type))
					return false;
			return true;
		}

		/**
		 * \brief Converts a value v to a string 
		 * \tparam T The type of v
		 * \param v a value
		 * \return the string representation of v
		 */
		template <typename T>
		std::string to_string(const T v)
		{
			std::ostringstream ss;
			ss << v;
			return ss.str();
		}

		/**
		 * \brief Strips whitespace from a string
		 * \param s a string to be stripped
		 * \return the string without whitespace
		 */
		static std::string strip(std::string s)
		{
			if (s.empty())
				return s;
			s.erase(0, s.find_first_not_of(' '));
			s.erase(s.find_last_not_of('\r') + 1);
			s.erase(s.find_last_not_of(' ') + 1);
			return s;
		}

		static std::string to_lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), tolower);
			return s;
		}

		/**
		 * \brief Retrieves an integer vector from a string
		 * \param input a string in one the supported formats:
		 *	'-m', [_min, m]
		 *	'n-', [n, _max]
		 *	'n-m', [n, m]
		 *	'n-x-y-z-m', [n,m]
		 * \param min The maximum value of each element
		 * \param max The minimum value of each element
		 * \return an integer vector
		 */
		static std::vector<int> get_int_vector_parse_string(std::string input, const int min, const int max)
		{
			std::vector<std::string> s;
			std::string tmp;
			int tmp_value, tmp_value2;
			std::vector<int> result;

			input = strip(input);
			for (auto& e : input)
				if (e != ',' && e != '-' && !isdigit(e))
					log::error("The configuration consists of invalid characters.");

			std::stringstream raw(input);
			while (getline(raw, tmp, ',')) s.push_back(tmp);

			auto n = static_cast<int>(s.size());
			for (auto i = 0; i < n; ++i)
			{
				if (s[i][0] == '-')
				{
					/// The condition beginning with "-m"
					if (i != 0)
						log::error("Format error in configuration.");
					else
					{
						tmp = s[i].substr(1);
						if (tmp.find('-') != std::string::npos)
							log::error("Format error in configuration.");

						tmp_value = std::stoi(tmp);

						if (tmp_value < min)
							log::error("Input value exceeds lower bound.");

						for (auto value = min; value <= tmp_value; ++value)
							result.push_back(value);
					}
				}
				else if (s[i][s[i].length() - 1] == '-')
				{
					/// The condition endding with "n-"

					if (i != n - 1)
						log::error("Format error in configuration.");
					else
					{
						tmp = s[i].substr(0, s[i].length() - 1);
						if (tmp.find('-') != std::string::npos)
							log::error("Format error in configuration.");
						tmp_value = std::stoi(tmp);
						if (tmp_value > max)
							log::error("Input value exceeds upper bound.");
						for (auto value = max; value <= tmp_value; --value)
							result.push_back(value);
					}
				}
				else
				{
					/// The condition with "n-m,n-x-m"
					std::stringstream tmp_raw(s[i]);
					std::vector<std::string> tmp_vector;
					while (getline(tmp_raw, tmp, '-'))
						tmp_vector.push_back(tmp);
					tmp_value = std::stoi(tmp_vector[0]);
					tmp_value2 = std::stoi(tmp_vector[tmp_vector.size() - 1]);
					if (tmp_value > tmp_value2)
						log::error("Format error in configuration.");
					if (tmp_value < min)
						log::error("Input value exceeds lower bound.");
					if (tmp_value2 > max)
						log::error("Input value exceeds upper bound.");
					for (auto value = tmp_value; value <= tmp_value2; ++value)
						result.push_back(value);
				}
			}
			return result;
		}

		/**
		 * \brief A nested map container, consisting of two levels. 
		 */
		class Container
		{
			/**
			 * \brief The internal data storage
			 */
			std::unordered_map<std::string,
			                   std::unordered_map<std::string, std::string>> data_;

			/**
			 * \brief Converts a key for the container to a nice format, i.e. lowercase and without trailing spaces
			 * \param key The key
			 * \return The key converted to lowercase and without trailing spaces
			 */
			static std::string nice(const std::string& key)
			{
				return to_lower(strip(key));
			}

		public:
			/**
			 * \brief Sets a value in the container
			 * \param section The first level index of the container
			 * \param key The second level index of the container
			 * \param value The value of the entry
			 */
			void set(const std::string& section, const std::string& key, const std::string& value)
			{
				data_[nice(section)][nice(key)] = value;
			}

			/**
			 * \brief Returns a map (second level) associated with a section (first level index)
			 * \param section The key for the section in the container
			 * \return When the section is found it returns the map associated with it,
			 * if it is not found it returns an empty map. 
			 */
			[[nodiscard]]
			std::unordered_map<std::string, std::string> get(const std::string& section) const
			{
				const auto iterate = data_.find(nice(section));
				if (iterate != data_.end())
					return iterate->second;
				log::warning("Cannot find section: " + section);
				return std::unordered_map<std::string, std::string>();
			}

			/**
			 * \brief Return a value stored in the container associated with a given section (first level index)
			 * and a key (second level index). If it is not found, a nullptr is returned.
			 * \param section The first level index
			 * \param key The second level index
			 * \return The value stored in the container
			 */
			[[nodiscard]]
			std::string get(const std::string& section, const std::string& key) const
			{
				auto map = get(section);
				const auto iterate = map.find(nice(key));
				if (iterate != map.end())
					return iterate->second;

				std::cout << "Cannot find key: " << section << std::endl;
				return nullptr;
			}

			/**
			 * \brief Return a value stored in the container associated with a given section (first level index)
			 * and a key (second level index). Transforms the value returned by the container into an integer.
			 * \param section The first level index
			 * \param key The second level index
			 * \return The value stored in the container
			 */
			[[nodiscard]]
			int get_int(const std::string& section, const std::string& key) const
			{
				return std::stoi(get(section, key));
			}

			/**
			 * \brief Return a value stored in the container associated with a given section (first level index)
			 * and a key (second level index). Transforms the value returned by the container into a boolean.
			 * \param section The first level index
			 * \param key The second level index
			 * \return The value stored in the container
			 */
			[[nodiscard]]
			int get_bool(const std::string& section, const std::string& key) const
			{
				return nice(get(section, key)) == "true";
			}

			/**
			 * \brief Return a value stored in the container associated with a given section (first level index)
			 * and a key (second level index). Transforms the value returned by the container into an integer vector.
			 * \param section The first level index
			 * \param key The second level index
			 * \param min The minimum allowed value for each element in the returned vector
			 * \param max The maximum allowed value for each element in the returned vector
			 * \return The value stored in the container
			 */
			[[nodiscard]]
			std::vector<int> get_int_vector(const std::string& section, const std::string& key, const int min,
			                                const int max) const
			{
				return get_int_vector_parse_string(get(section, key), min, max);
			}
		};
	}
}
