#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>

const double EARTH_RADIUS = 6371.0; // Earth's radius in kilometers

// Cab data structure
struct Cab {
    int id;
    double latitude;
    double longitude;
    bool engaged;
};

// Global variables
std::queue<Cab> cabQueue;
std::mutex mtx;
std::condition_variable cv;
bool done = false;

// Function to calculate distance between two points using Haversine formula
double haversine(double lat1, double lon1, double lat2, double lon2) {
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dlon / 2) * std::sin(dlon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return EARTH_RADIUS * c;
}

// Producer thread to simulate real-time data stream
void producer() {
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Cab cab = {i, 40.0 + i * 0.1, -74.0 + i * 0.1, i % 2 == 0};
        std::unique_lock<std::mutex> lock(mtx);
        cabQueue.push(cab);
        lock.unlock();
        cv.notify_one();
    }
    {
        std::unique_lock<std::mutex> lock(mtx);
        done = true;
        cv.notify_one();
    }
}

// Consumer thread to process the cab data
void consumer(double employeeLat, double employeeLon, double maxDistance) {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, []{ return !cabQueue.empty() || done; });

        if (done && cabQueue.empty()) {
            break;
        }

        Cab cab = cabQueue.front();
        cabQueue.pop();
        lock.unlock();

        // Filter based on engagement and distance
        if (cab.engaged) {
            double distance = haversine(employeeLat, employeeLon, cab.latitude, cab.longitude);
            if (distance <= maxDistance) {
                std::cout << "Cab ID: " << cab.id
                          << ", Distance: " << distance
                          << " km, Engaged: " << (cab.engaged ? "Yes" : "No")
                          << std::endl;
            }
        }
    }
}

int main() {
    double employeeLat = 40.0;
    double employeeLon = -74.0;
    double maxDistance = 10.0; // Maximum distance in kilometers

    std::thread producerThread(producer);
    std::thread consumerThread(consumer, employeeLat, employeeLon, maxDistance);

    producerThread.join();
    consumerThread.join();

    return 0;
}

