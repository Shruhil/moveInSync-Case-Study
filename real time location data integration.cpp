//Central Server

#include "crow.h"
#include <redis-cpp/redis.h>
#include <cmath>
#include <limits>

const double EARTH_RADIUS_KM = 6371.0;

// Haversine formula to calculate the distance between two points
double haversine(double lon1, double lat1, double lon2, double lat2) {
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
               std::sin(dlon / 2) * std::sin(dlon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return EARTH_RADIUS_KM * c;
}

int main() {
    crow::SimpleApp app;
    auto redis = rediscpp::make_connection("localhost", "6379");

    CROW_ROUTE(app, "/update_location")
        .methods("POST"_method)([&redis](const crow::request& req) {
            auto data = crow::json::load(req.body);
            std::string cab_id = data["cab_id"].s();
            double lat = data["latitude"].d();
            double lon = data["longitude"].d();
            redis.set("cab:" + cab_id, std::to_string(lat) + "," + std::to_string(lon));
            return crow::response(200, "success");
        });

    CROW_ROUTE(app, "/allocate_cab")
        .methods("POST"_method)([&redis](const crow::request& req) {
            auto data = crow::json::load(req.body);
            double start_lat = data["start_latitude"].d();
            double start_lon = data["start_longitude"].d();
            auto keys = redis.keys("cab:*");
            std::string nearest_cab;
            double min_distance = std::numeric_limits<double>::max();

            for (const auto& key : keys) {
                std::string cab_data = redis.get(key);
                auto pos = cab_data.find(',');
                double lat = std::stod(cab_data.substr(0, pos));
                double lon = std::stod(cab_data.substr(pos + 1));
                double distance = haversine(lon, lat, start_lon, start_lat);
                if (distance < min_distance) {
                    min_distance = distance;
                    nearest_cab = key.substr(4);  // remove "cab:" prefix
                }
            }
            if (!nearest_cab.empty()) {
                return crow::response(200, crow::json::wvalue{{"cab_id", nearest_cab}});
            } else {
                return crow::response(404, "No cabs available");
            }
        });

    app.port(5000).multithreaded().run();
}



//Local Cluster
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <redis-cpp/redis.h>
#include <cmath>
#include <limits>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

const double EARTH_RADIUS_KM = 6371.0;

// Haversine formula
double haversine(double lon1, double lat1, double lon2, double lat2) {
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
               std::sin(dlon / 2) * std::sin(dlon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return EARTH_RADIUS_KM * c;
}

class http_server {
public:
    http_server(net::io_context& ioc, tcp::endpoint endpoint)
        : acceptor_(ioc, endpoint) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<http_session>(std::move(socket))->start();
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

class http_session : public std::enable_shared_from_this<http_session> {
public:
    explicit http_session(tcp::socket socket)
        : socket_(std::move(socket)) {}

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, request_,
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                boost::ignore_unused(bytes_transferred);
                if (!ec)
                    self->handle_request();
            });
    }

    void handle_request() {
        auto redis = rediscpp::make_connection("localhost", "6379");
        if (request_.method() == http::verb::post) {
            if (request_.target() == "/update_location") {
                auto data = json::parse(request_.body());
                std::string cab_id = data["cab_id"].get<std::string>();
                double lat = data["latitude"].get<double>();
                double lon = data["longitude"].get<double>();
                redis.set("cab:" + cab_id, std::to_string(lat) + "," + std::to_string(lon));
                // Send data to central server
                http::response<http::string_body> res{http::status::ok, request_.version()};
                res.set(http::field::server, "Boost.Beast");
                res.body() = "success";
                res.prepare_payload();
                do_write(std::move(res));
            } else if (request_.target() == "/allocate_cab") {
                auto data = json::parse(request_.body());
                double start_lat = data["start_latitude"].get<double>();
                double start_lon = data["start_longitude"].get<double>();
                auto keys = redis.keys("cab:*");
                std::string nearest_cab;
                double min_distance = std::numeric_limits<double>::max();

                for (const auto& key : keys) {
                    std::string cab_data = redis.get(key);
                    auto pos = cab_data.find(',');
                    double lat = std::stod(cab_data.substr(0, pos));
                    double lon = std::stod(cab_data.substr(pos + 1));
                    double distance = haversine(lon, lat, start_lon, start_lat);
                    if (distance < min_distance) {
                        min_distance = distance;
                        nearest_cab = key.substr(4);  // remove "cab:" prefix
                    }
                }
                http::response<http::string_body> res{http::status::ok, request_.version()};
                res.set(http::field::server, "Boost.Beast");
                if (!nearest_cab.empty()) {
                    res.body() = "{\"cab_id\":\"" + nearest_cab + "\"}";
                } else {
                    res.body() = "{\"error\":\"No cabs available\"}";
                }
                res.prepare_payload();
                do_write(std::move(res));
            }
        }
    }

    void do_write(http::response<http::string_body>&& res) {
        auto self = shared_from_this();
        http::async_write(socket_, res,
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                boost::ignore_unused(bytes_transferred);
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
            });
    }

    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
};

int main() {
    net::io_context ioc;
    tcp::endpoint endpoint{net::ip::make_address("0.0.0.0"), 5001};
    std::make_shared<http_server>(ioc, endpoint)->start();
    ioc.run();
}


//Individual Cab
#include <iostream>
#include <mqtt/async_client.h>
#include <json/json.h>
#include <curl/curl.h>

const std::string BROKER = "tcp://mqtt_broker_address:1883";
const std::string TOPIC = "cab/location/update";
const std::string CENTRAL_SERVER_URL = "http://central_server_address/update_location";
const std::string LOCAL_CLUSTER_URL = "http://local_cluster_address/update_location";

class callback : public virtual mqtt::callback {
    void message_arrived(mqtt::const_message_ptr msg) override {
        Json::CharReaderBuilder rbuilder;
        Json::Value data;
        std::string errs;
        std::istringstream s(msg->to_string());
        std::string raw_data = s.str();
        std::istringstream ss(raw_data);
        std::string doc;
        Json::parseFromStream(rbuilder, ss, &data, &errs);

        std::string cab_id = data["cab_id"].asString();
        double lat = data["latitude"].asDouble();
        double lon = data["longitude"].asDouble();

        // Send data to local cluster
        send_data_to_server(LOCAL_CLUSTER_URL, data);
        // Send data to central server
        send_data_to_server(CENTRAL_SERVER_URL, data);
    }

    static void send_data_to_server(const std::string& url, const Json::Value& data) {
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string json_data = Json::writeString(Json::StreamWriterBuilder(), data);
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
            curl_easy_cleanup(curl);
        }
    }
};

int main() {
    mqtt::async_client client(BROKER, "client_id");
    callback cb;
    client.set_callback(cb);

    mqtt::connect_options conn_opts;
    conn_opts.set_keep_alive_interval(20);
    conn_opts.set_clean_session(true);

    try {
        std::cout << "Connecting to the MQTT server..." << std::flush;
        client.connect(conn_opts)->wait();
        std::cout << "OK\n";

        client.subscribe(TOPIC, 1)->wait();

        std::cout << "Waiting for messages...\n";
        while (true) {}  // Keep the program running to receive messages

    } catch (const mqtt::exception& exc) {
        std::cerr << exc.what() << std::endl;
        return 1;
    }

    return 0;
}
