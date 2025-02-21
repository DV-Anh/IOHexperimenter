#include <fmt/ranges.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <utility>
#include "ioh.hpp"


namespace py = pybind11;
using namespace ioh;

class PyProperty : public logger::Property
{
    const py::object container_;
    const std::string attribute_;

public:
    PyProperty(const py::object &container, const std::string &attribute) :
        Property(attribute), container_(container), attribute_(attribute)
    {
    }

    py::object container() const { return container_; }

    std::optional<double> operator()(const logger::Info &) const override
    {
        if (py::hasattr(container_, attribute_.c_str())){
            auto pyobj = container_.attr(attribute_.c_str()).ptr();
            if (pyobj != Py_None)
                return std::make_optional<double>(PyFloat_AsDouble(pyobj));
        }
        return {};
    }
};

template <typename WatcherType>
class PyWatcher : public WatcherType
{
protected:
    bool alive = true;
    std::vector<PyProperty *> property_ptrs_;

public:
    template <typename... Args>
    PyWatcher(Args &&...args) : WatcherType(std::forward<Args>(args)...)
    {
        py::module::import("atexit").attr("register")(py::cpp_function{[self = this]() -> void {
            // type-pun alive bool in order to check if is still a boolean 1, if so, delete.
            // in some cases this might cause a segfault, only happens in a very small prob. (1/MAX_INT)
            int alive_int = (int)(*(char *)(&self->alive));
            if (alive_int == 1)
            {
                self->close();
            }
        }});
    }

    void close() override
    {
        if (alive)
        {
            WatcherType::close();
            alive = false;
            for (auto ptr : property_ptrs_)
                delete ptr;
            property_ptrs_.clear();
        }
    }

    virtual ~PyWatcher() { close(); }

    void watch(logger::Property &property) override { PYBIND11_OVERRIDE(void, WatcherType, watch, property); }

    void watch(const py::object &container, const std::string &attribute)
    {
        auto *p = new PyProperty(container, attribute);
        watch(*p);
        property_ptrs_.push_back(p);
    }

    void watch(const py::object &container, const std::vector<std::string> &attributes)
    {
        for (const auto &attr : attributes)
            watch(container, attr);
    }

    void log(const logger::Info &log_info) override { PYBIND11_OVERRIDE(void, WatcherType, log, log_info); }

    void attach_problem(const problem::MetaData &problem) override
    {
        PYBIND11_OVERRIDE(void, WatcherType, attach_problem, problem);
    }

    void attach_suite(const std::string &suite_name) override
    {
        PYBIND11_OVERRIDE(void, WatcherType, attach_suite, suite_name);
    }

    void call(const logger::Info &log_info) override { PYBIND11_OVERRIDE(void, WatcherType, call, log_info); }
};

class PyAnalyzer : public PyWatcher<logger::Analyzer>
{
    std::vector<double *> double_ptrs_;
    std::vector<PyProperty *> prop_ptrs_;

public:
    using Analyzer = PyWatcher<logger::Analyzer>;
    using Analyzer::Analyzer;

    virtual void close() override
    {
        if (alive)
        {
            clear_ptrs();
            Analyzer::close();
        }
    }

    virtual ~PyAnalyzer() { close(); }

    void add_run_attribute_python(const py::object &container, const std::string &name)
    {
        auto *p = new PyProperty(container, name);
        add_run_attribute_python(name, (*p)(logger::Info{}).value());
        prop_ptrs_.push_back(p);
    }

    void add_run_attributes_python(const py::object &container, const std::vector<std::string> &attributes)
    {
        for (const auto &attr : attributes)
            add_run_attribute_python(container, attr);
    }

    void add_run_attribute_python(const std::string &name, double value)
    {
        double *ptr = new double(value);
        Analyzer::add_run_attribute(name, ptr);
        double_ptrs_.push_back(ptr);
    }

    void set_run_attribute_python(const std::string &name, double value) { *(attributes_.run.at(name)) = value; }

    void set_run_attributes_python(const std::map<std::string, double> &attributes)
    {
        clear_ptrs();
        for (auto &[key, value] : attributes)
            add_run_attribute_python(key, value);
    }

    void clear_ptrs()
    {
        for (auto ptr : double_ptrs_)
            delete ptr;

        for (auto ptr : prop_ptrs_)
            delete ptr;
        double_ptrs_.clear();
        prop_ptrs_.clear();
    }

    void attach_problem(const problem::MetaData &problem) override
    {
        for (auto ptr : prop_ptrs_)
            set_run_attribute_python(ptr->name(), (*ptr)(logger::Info{}).value());
        Analyzer::attach_problem(problem);
    }
};

void define_triggers(py::module &m)
{
    py::module t = m.def_submodule("trigger");

    py::class_<logger::Trigger, std::shared_ptr<logger::Trigger>>(t, "Trigger", "Base class for all Triggers")
        .def("__call__", &logger::Trigger::operator())
        .def("reset", &logger::Trigger::reset);

    py::class_<trigger::Always, logger::Trigger, std::shared_ptr<trigger::Always>>(
        t, "Always", "Trigger that always evaluates to true")
        .def(py::init<>())
        .def(py::pickle([](const trigger::Always &) { return py::make_tuple(); },
                        [](py::tuple) { return trigger::Always{}; }));

    t.attr("ALWAYS") = py::cast(trigger::always);

    py::class_<trigger::OnImprovement, logger::Trigger, std::shared_ptr<trigger::OnImprovement>>(
        t, "OnImprovement", "Trigger that evaluates to true when improvement of the objective function is observed")
        .def(py::init<>())
        .def(py::pickle([](const trigger::OnImprovement &t) { return py::make_tuple(t.best(), t.type()); },
                        [](py::tuple t) {
                            return trigger::OnImprovement{t[0].cast<double>(), t[1].cast<common::OptimizationType>()};
                        }));

    ;
    t.attr("ON_IMPROVEMENT") = py::cast(trigger::on_improvement);

    py::class_<trigger::At, logger::Trigger, std::shared_ptr<trigger::At>>(t, "At")
        .def(py::init<std::set<size_t>>(), py::arg("time_points"),
             R"pbdoc(
                Trigger that evaluates to true at specific time points

                Parameters
                ----------
                time_points: list[int]
                    The time points at which to trigger
            )pbdoc")
        .def_property_readonly("time_points", &trigger::At::time_points)
        .def(py::pickle([](const trigger::At &t) { return py::make_tuple(t.time_points()); },
                        [](py::tuple t) { return trigger::At{t[0].cast<std::set<size_t>>()}; }));

    py::class_<trigger::Each, logger::Trigger, std::shared_ptr<trigger::Each>>(t, "Each")
        .def(py::init<size_t, size_t>(), py::arg("interval"), py::arg("starting_at") = 0,
             R"pbdoc(
                Trigger that evaluates to true at a given interval

                Parameters
                ----------
                interval: int
                    The interval at which to trigger
                starting_at: int
                    The starting time of the interval, defaults to 0.
            )pbdoc")
        .def_property_readonly("interval", &trigger::Each::interval)
        .def_property_readonly("starting_at", &trigger::Each::starting_at)
        .def(py::pickle([](const trigger::Each &t) { return py::make_tuple(t.interval(), t.starting_at()); },
                        [](py::tuple t) {
                            return trigger::Each{t[0].cast<size_t>(), t[1].cast<size_t>()};
                        }));

    py::class_<trigger::During, logger::Trigger, std::shared_ptr<trigger::During>>(t, "During")
        .def(py::init<std::set<std::pair<size_t, size_t>>>(), py::arg("time_ranges"),
             R"pbdoc(
                Trigger that evaluates to true during a given interval.

                Parameters
                ----------
                time_ranges: set[tuple[int, int]]
                    The ranges/intervals at which the log the data.                    
            )pbdoc"

             )
        .def_property_readonly("time_ranges", &trigger::During::time_ranges)
        .def(py::pickle([](const trigger::During &t) { return py::make_tuple(t.time_ranges()); },
                        [](py::tuple t) { return trigger::During{t[0].cast<std::set<std::pair<size_t, size_t>>>()}; }));
}

void define_properties(py::module &m)
{
    py::module t = m.def_submodule("property");

    py::class_<logger::Property, std::shared_ptr<logger::Property>>(t, "AbstractProperty",
                                                                    "Base class for all Properties")
        .def("__call__", &logger::Property::operator())
        .def("name", &logger::Property::name)
        .def("call_to_string", &logger::Property::call_to_string);


    py::class_<PyProperty, logger::Property, std::shared_ptr<PyProperty>>(t, "Property")
        .def(py::init<py::object, std::string>(), py::arg("container"), py::arg("attribute"),
             R"pbdoc(
                Wrapper for properties with a Python container

                Parameters
                ----------
                container: object
                    The object which contains the property to be logged
                attribute: str
                    The name of the property to be logger, must be an attribute on container          
            )pbdoc"

             )
        .def(py::pickle([](const PyProperty &t) { return py::make_tuple(t.container(), t.name()); },
                        [](py::tuple t) {
                            return PyProperty{t[0].cast<py::object>(), t[1].cast<std::string>()};
                        }));

    py::class_<watch::Evaluations, logger::Property, std::shared_ptr<watch::Evaluations>>(t, "Evaluations")
        .def(py::init<std::string, std::string>(), py::arg("name"), py::arg("format"),
             "Property which tracks the number of evaluations")
        .def(py::pickle([](const watch::Evaluations &t) { return py::make_tuple(t.name(), t.format()); },
                        [](py::tuple t) {
                            return watch::Evaluations{t[0].cast<std::string>(), t[1].cast<std::string>()};
                        }));
    t.attr("EVALUATIONS") = py::cast(watch::evaluations);

    py::class_<watch::RawYBest, logger::Property, std::shared_ptr<watch::RawYBest>>(t, "RawYBest")
        .def(py::init<std::string, std::string>(), py::arg("name"), py::arg("format"),
             "Property which tracks the raw best y value")
        .def(py::pickle([](const watch::RawYBest &t) { return py::make_tuple(t.name(), t.format()); },
                        [](py::tuple t) {
                            return watch::RawYBest{t[0].cast<std::string>(), t[1].cast<std::string>()};
                        }));
    t.attr("RAW_Y_BEST") = py::cast(watch::raw_y_best);

    py::class_<watch::CurrentY, logger::Property, std::shared_ptr<watch::CurrentY>>(t, "CurrentY")
        .def(py::init<std::string, std::string>(), py::arg("name"), py::arg("format"),
             "Property which tracks the current y value")
        .def(py::pickle([](const watch::CurrentY &t) { return py::make_tuple(t.name(), t.format()); },
                        [](py::tuple t) {
                            return watch::CurrentY{t[0].cast<std::string>(), t[1].cast<std::string>()};
                        }));
    t.attr("CURRENT_Y_BEST") = py::cast(watch::current_y);

    py::class_<watch::TransformedY, logger::Property, std::shared_ptr<watch::TransformedY>>(t, "TransformedY")
        .def(py::init<std::string, std::string>(), py::arg("name"), py::arg("format"),
             "Property which tracks the current transformed y value")
        .def(py::pickle([](const watch::TransformedY &t) { return py::make_tuple(t.name(), t.format()); },
                        [](py::tuple t) {
                            return watch::TransformedY{t[0].cast<std::string>(), t[1].cast<std::string>()};
                        }));
    t.attr("TRANSFORMED_Y") = py::cast(watch::transformed_y);

    py::class_<watch::TransformedYBest, logger::Property, std::shared_ptr<watch::TransformedYBest>>(t,
                                                                                                    "TransformedYBest")
        .def(py::init<std::string, std::string>(), py::arg("name"), py::arg("format"),
             "Property which tracks the current best transformed y value")
        .def(py::pickle([](const watch::TransformedYBest &t) { return py::make_tuple(t.name(), t.format()); },
                        [](py::tuple t) {
                            return watch::TransformedYBest{t[0].cast<std::string>(), t[1].cast<std::string>()};
                        }));

    t.attr("TRANSFORMED_Y_BEST") = py::cast(watch::transformed_y_best);
}

void define_bases(py::module &m)
{
    py::class_<Logger, std::shared_ptr<Logger>>(m, "Logger", "Base class for all loggers")
        .def("add_trigger", &Logger::trigger, "Add a trigger to the logger")
        .def("log", &Logger::log, "Performs logging behaviour")
        .def("attach_problem", &Logger::attach_problem, "Attach a problem to the logger")
        .def("attach_suite", &Logger::attach_suite, "Attach a suite to the logger")
        .def("call", &Logger::call, "Calls log if any of the triggers evaluate to true")
        .def("reset", &Logger::reset, "Reset the state of the logger")
        .def_property_readonly("problem", &Logger::problem, "Reference to the currently attached problem");

    using namespace logger;
    py::class_<Watcher, Logger, std::shared_ptr<Watcher>>(m, "AbstractWatcher",
                                                          "Base class for loggers which track properties")
        .def("watch", &Watcher::watch);
}


void define_flatfile(py::module &m)
{
    using namespace logger;
    const std::vector<std::string> common_headers = {
        "suite_name", "problem_name", "problem_id", "problem_instance", "optimization_type", "dimension", "run"};

    py::class_<PyWatcher<FlatFile>, Watcher, std::shared_ptr<PyWatcher<FlatFile>>>(m, "FlatFile")
        .def(py::init<Triggers, Properties, std::string, fs::path, std::string, std::string, std::string, std::string,
                      bool, bool, std::vector<std::string>>(),
             py::arg("triggers"), py::arg("properties"), py::arg("filename") = "IOH.dat",
             py::arg("output_directory") = "./", py::arg("separator") = "\t", py::arg("comment") = "#",
             py::arg("no_value") = "None", py::arg("end_of_line") = "\n", py::arg("repeat_header") = false,
             py::arg("store_positions") = false, py::arg("common_header_titles") = common_headers,
             R"pbdoc(
                A logger which stores all tracked properties to a file.

                Parameters
                ----------
                triggers: list[Trigger]
                    List of triggers, i.e. when to trigger logging
                properties: list[Property]
                    The list of properties to keep track of
                filename: str = "IOH.dat"
                    The file to log to
                output_directory: str = "./"
                    The path to which to write the file
                separator: str = "\t"
                    The column seperator
                comment: str = "#"
                    The comment character
                no_value: str = "None"
                    The NaN character
                end_of_line: str = "\n"
                    The eol character
                repeat_header: bool = false
                    Boolean value which indicates whether to repeat the header for every new run.
                store_positions: bool = false
                    Boolean value which indicates whether to store the x-positions in the file
            )pbdoc"

             )
        .def_property_readonly("filename", &FlatFile::filename)
        .def_property_readonly(
            "output_directory",
            [](PyWatcher<FlatFile> &f) { return fs::absolute(f.output_directory()).generic_string(); })
        .def("watch", py::overload_cast<Property &>(&PyWatcher<FlatFile>::watch))
        .def("watch", py::overload_cast<const py::object &, const std::string &>(&PyWatcher<FlatFile>::watch))
        .def("watch",
             py::overload_cast<const py::object &, const std::vector<std::string> &>(&PyWatcher<FlatFile>::watch))
        .def("__repr__", [](const PyWatcher<FlatFile> &f) {
            return fmt::format("<FlatFile {}>", (f.output_directory() / f.filename()).generic_string());
        });
}

void define_store(py::module &m)
{
    using namespace logger;

    using PyStore = PyWatcher<Store>;
    py::class_<PyStore, Watcher, std::shared_ptr<PyStore>>(m, "Store")
        .def(py::init<Triggers, Properties>(), py::arg("triggers"), py::arg("properties"),
             R"pbdoc(
                A logger which stores all tracked properties in memory.

                Parameters
                ----------
                triggers: list[Trigger]
                    List of triggers, i.e. when to trigger logging
                properties: list[Property]
                    The list of properties to keep track of
            )pbdoc"

             )
        .def("data", py::overload_cast<>(&PyStore::data), "Accessor to the internal data container")
        .def(
            "at",
            [](PyStore &f, std::string suite_name, int pb, int dim, int inst, size_t run, size_t evaluation) {
                const auto cursor = Store::Cursor(suite_name, pb, dim, inst, run, evaluation);
                return f.data(cursor);
            },
            "Accessor for a specific stored record")
        .def("watch", py::overload_cast<Property &>(&PyStore::watch), "Add a property")
        .def("watch", py::overload_cast<const py::object &, const std::string &>(&PyStore::watch), "Add a property")
        .def("watch", py::overload_cast<const py::object &, const std::vector<std::string> &>(&PyStore::watch),
             "Add multliple properties")
        .def("__repr__", [](PyStore &f) {
            return fmt::format("<Store (suites: ({}),)>", fmt::join(ioh::common::keys(f.data()), ","));
        });
}

void define_analyzer(py::module &m)
{
    using namespace logger;
    Triggers def_trigs{trigger::on_improvement};
    Properties def_props{};
    py::class_<PyAnalyzer, Watcher, std::shared_ptr<PyAnalyzer>>(m, "Analyzer")
        .def(py::init<Triggers, Properties, fs::path, std::string, std::string, std::string, bool>(),
             py::arg("triggers") = def_trigs, py::arg("additional_properties") = def_props,
             py::arg("root") = fs::current_path(), py::arg("folder_name") = "ioh_data",
             py::arg("algorithm_name") = "algorithm_name", py::arg("algorithm_info") = "algorithm_info",
             py::arg("store_positions") = false,
             R"pbdoc(
                A logger which stores all tracked properties to a file.

                Parameters
                ----------
                triggers: list[Trigger]
                    List of triggers, i.e. when to trigger logging
                additional_properties: list[Property]
                    The list of additional properties to keep track of (additional to the default properties for this logger type)
                root: str = "./"
                    The path to which to write the files
                folder_name: str = "./"
                    The name of the folder to which to write the files
                algorithm_name: str = "algorithm_name"
                    Optional name parameter for the algorithm to be added to the info files
                algorithm_info: str = "algorithm_info"
                    Optional info parameter for the algorithm to be added to the info files
                store_positions: bool = false
                    Boolean value which indicates whether to store the x-positions in the file
            )pbdoc")
        .def("add_experiment_attribute", &PyAnalyzer::add_experiment_attribute)
        .def("set_experiment_attributes", &PyAnalyzer::set_experiment_attributes)
        .def("add_run_attributes",
             py::overload_cast<const std::string &, double>(&PyAnalyzer::add_run_attribute_python))
        .def("add_run_attributes",
             py::overload_cast<const py::object &, const std::string &>(&PyAnalyzer::add_run_attribute_python))
        .def("add_run_attributes",
             py::overload_cast<const py::object &, const std::vector<std::string> &>(
                 &PyAnalyzer::add_run_attributes_python))
        .def("set_run_attributes", &PyAnalyzer::set_run_attributes_python)
        .def("set_run_attribute", &PyAnalyzer::set_run_attribute_python)
        .def_property_readonly("output_directory", &PyAnalyzer::output_directory)
        .def("watch", py::overload_cast<Property &>(&PyAnalyzer::watch))
        .def("watch", py::overload_cast<const py::object &, const std::string &>(&PyAnalyzer::watch))
        .def("watch", py::overload_cast<const py::object &, const std::vector<std::string> &>(&PyAnalyzer::watch))
        .def("__repr__",
             [](const PyAnalyzer &f) { return fmt::format("<Analyzer {}>", f.output_directory().generic_string()); });
}

template <typename T>
void define_eah_scale(py::module &m, const std::string &name)
{
    using namespace logger::eah;

    py::class_<Scale<T>, std::shared_ptr<Scale<T>>>(m, name.c_str())
        .def_property_readonly("min", &Scale<T>::min)
        .def_property_readonly("max", &Scale<T>::max)
        .def_property_readonly("size", &Scale<T>::size)
        .def_property_readonly("length", &Scale<T>::length)
        .def("index", &Scale<T>::index)
        .def("bounds", &Scale<T>::bounds)
        .def("__repr__", [name](const Scale<T> &s) {
            return fmt::format("<{} (({}, {}), {})>", name, s.min(), s.max(), s.size());
        });

    py::class_<LinearScale<T>, Scale<T>, std::shared_ptr<LinearScale<T>>>(m, ("Linear" + name).c_str())
        .def(py::init<T, T, size_t>())
        .def("step", &LinearScale<T>::step);
    py::class_<Log2Scale<T>, Scale<T>, std::shared_ptr<Log2Scale<T>>>(m, ("Log2" + name).c_str())
        .def(py::init<T, T, size_t>());

    py::class_<Log10Scale<T>, Scale<T>, std::shared_ptr<Log10Scale<T>>>(m, ("Log10" + name).c_str())
        .def(py::init<T, T, size_t>());
}
void define_eah(py::module &m)
{
    using namespace logger;
    auto eah = m.def_submodule("eah");
    define_eah_scale<double>(eah, "RealScale");
    define_eah_scale<size_t>(eah, "IntegerScale");

    py::class_<EAH, Logger, std::shared_ptr<EAH>>(m, "EAH", "Emperical Attainment Histogram Logger")
        .def(py::init<double, double, size_t, size_t, size_t, size_t>(), py::arg("error_min"), py::arg("error_max"),
             py::arg("error_buckets"), py::arg("evals_min"), py::arg("evals_max"), py::arg("evals_buckets"))
        .def(py::init<eah::LinearScale<double> &, eah::LinearScale<size_t> &>(), py::arg("error_scale"),
             py::arg("eval_scale"))
        .def(py::init<eah::Log2Scale<double> &, eah::Log2Scale<size_t> &>(), py::arg("error_scale"),
             py::arg("eval_scale"))
        .def(py::init<eah::Log10Scale<double> &, eah::Log10Scale<size_t> &>(), py::arg("error_scale"),
             py::arg("eval_scale"))
        .def("at", &logger::EAH::at)
        .def_property_readonly("data", &logger::EAH::data)
        .def_property_readonly("size", &logger::EAH::size)
        .def_property_readonly("error_range", &logger::EAH::error_range, py::return_value_policy::reference)
        .def_property_readonly("eval_range", &logger::EAH::eval_range, py::return_value_policy::reference)
        .def("__repr__", [](const logger::EAH &l) { return fmt::format("<EAH {}>", l.size()); });
}

void define_eaf(py::module &m)
{
    using namespace logger;

    auto eaf = m.def_submodule("eaf");
    py::class_<eaf::Point, std::shared_ptr<eaf::Point>>(eaf, "Point")
        .def(py::init<double, size_t>())
        .def("__repr__", [](const eaf::Point &p) { return fmt::format("<Point {} {}>", p.qual, p.time); });

    py::class_<eaf::RunPoint, eaf::Point, std::shared_ptr<eaf::RunPoint>>(eaf, "RunPoint")
        .def(py::init<double, size_t, size_t>())
        .def("__repr__",
             [](const eaf::RunPoint &p) { return fmt::format("<RunPoint {} {} {}>", p.qual, p.time, p.run); });

    py::class_<EAF, Logger, std::shared_ptr<EAF>>(m, "EAF", "Emperical Attainment Function Logger")
        .def(py::init<>())
        .def_property_readonly("data", py::overload_cast<>(&EAF::data, py::const_))
        .def("at", [](EAF &f, std::string suite_name, int pb, int dim, int inst, size_t run) {
            const auto cursor = EAF::Cursor(suite_name, pb, dim, inst, run);
            return f.data(cursor);
        });
}


void define_loggers(py::module &m)
{
    using namespace logger;
    py::class_<Combine, Logger, std::shared_ptr<Combine>>(m, "Combine", "Utility class to combine multiple loggers")
        .def(py::init<std::vector<std::reference_wrapper<Logger>>>(), py::arg("loggers"))
        .def(py::init<std::reference_wrapper<Logger>>(), py::arg("logger"))
        .def("append", &Combine::append);

    define_flatfile(m);
    define_store(m);
    define_analyzer(m);
    define_eah(m);
    define_eaf(m);
}


void define_logger(py::module &m)
{
    py::class_<fs::path>(m, "Path").def(py::init<std::string>());
    py::implicitly_convertible<std::string, fs::path>();

    define_triggers(m);
    define_properties(m);
    define_bases(m);
    define_loggers(m);
}
