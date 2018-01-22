#include <cstdlib>
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include "xtensor/xarray.hpp"
#include "xtensor/xeval.hpp"
#include "xtensor/xtensor.hpp"
#include "xtensor/xio.hpp"
#include "xtensor/xindex_view.hpp"
#include "xtensor/xmath.hpp"
#include "../lib/json/json.h"
#include "../lib/signaldata/technical_indicator/simple_moving_average.h"
#include "http.h"

namespace ti = signaldata::technical_indicator;
namespace cs = cryptocurrency_strategy;
using boost::lexical_cast;

const int ticked_count = 60;
const int deadline_timer_interval = 60; // seconds

const xt::xtensor<double, 1> add(const xt::xtensor<double, 1> data, const double val) {
    auto data_size = static_cast<int>(data.shape()[0]);
    // A new xtensor always starts with a single value, so handle that here
    if (data_size == 1 && data(1, 0) == 0.0)
        return xt::xtensor<double, 1>({val});

    xt::xtensor<double, 1> new_data = xt::zeros<double>({1, data_size + 1});
    std::copy(data.begin(), data.end(), new_data.begin());
    new_data(1, data_size) = val;
    return new_data;
}

void tick(const boost::system::error_code& sec, boost::asio::deadline_timer& timer, int& count, xt::xtensor<double, 1>& last_data) {
    if (count >= ticked_count)
        return;

    ++count;

    // TODO: Add a retry mechanism based on returned error code
    auto[ec, js] = cs::get_poloniex_ticker("BTC_ETH");

    std::cout << "last: " << lexical_cast<double>(js["last"].get<std::string>()) << std::endl;

    last_data = add(last_data, lexical_cast<double>(js["last"].get<std::string>()));
    std::cout << "last_data: " << last_data << std::endl;

    // This does a simple moving average, but could use any of the time series algos
    // in ../lib/signaldata/technical_indicator (I wrote that lib!)
    auto last_sma_data = ti::simple_moving_average(last_data, 3);
    std::cout << "last_sma_data: " << last_sma_data << std::endl;

    timer.expires_at(timer.expires_at() + boost::posix_time::seconds(deadline_timer_interval));
    timer.async_wait(boost::bind(tick, boost::asio::placeholders::error, boost::ref(timer), boost::ref(count), boost::ref(last_data)));
}

int main() {
    auto count = 0;
    xt::xtensor<double, 1> last_data;
    boost::asio::io_service ios;
    boost::asio::deadline_timer timer(ios, boost::posix_time::seconds(deadline_timer_interval));

    timer.async_wait(boost::bind(tick, boost::asio::placeholders::error, boost::ref(timer), boost::ref(count), boost::ref(last_data)));
    ios.run();

    std::cout << "Done." << std::endl;

    return 0;
}