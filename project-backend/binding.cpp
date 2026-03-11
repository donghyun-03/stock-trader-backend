// binding.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "backend.h"

namespace py = pybind11;

PYBIND11_MODULE(project_backend, m) {
    m.doc() = "C++ backend for MA backtest";

    py::class_<BarData>(m, "BarData")
        .def(py::init<>())
        .def_readwrite("date", &BarData::date)
        .def_readwrite("open", &BarData::open)
        .def_readwrite("high", &BarData::high)
        .def_readwrite("low", &BarData::low)
        .def_readwrite("close", &BarData::close)
        .def_readwrite("volume", &BarData::volume)
        .def_readwrite("signal", &BarData::signal);

    py::class_<SignalData>(m, "SignalData")
        .def(py::init<>())
        .def_readwrite("date", &SignalData::date)
        .def_readwrite("signal", &SignalData::signal);

    py::class_<PerformanceMetric>(m, "PerformanceMetric")
        .def(py::init<>())
        .def_readwrite("cumulative_return", &PerformanceMetric::cumulative_return)
        .def_readwrite("annualized_return", &PerformanceMetric::annualized_return)
        .def_readwrite("max_drawdown", &PerformanceMetric::max_drawdown)
        .def_readwrite("sharpe_ratio", &PerformanceMetric::sharpe_ratio)
        .def_readwrite("win_rate", &PerformanceMetric::win_rate)
        .def_readwrite("total_trades", &PerformanceMetric::total_trades);

    py::class_<StrategyFacade>(m, "StrategyFacade")
        .def(py::init<>())
        .def("RunBacktest", &StrategyFacade::RunBacktest, py::arg("short_ma"), py::arg("long_ma"), py::arg("fee_rate"), py::arg("slippage"), py::arg("cash"), py::arg("data_path"))
        .def("GetResults", &StrategyFacade::GetResults)
        .def("GetChartData", &StrategyFacade::GetChartData);
}
