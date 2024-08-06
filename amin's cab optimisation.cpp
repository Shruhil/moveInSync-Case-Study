#include <iostream>
#include <vector>
#include <unordered_map>
#include <cmath>

// Define a Point structure
struct Point {
    float x, y;
};

// Define a GridCell structure
struct GridCell {
    std::vector<Point> cabs;
};

// Define the GridManager class
class GridManager {
public:
    GridManager(float gridSize) : gridSize(gridSize) {}

    // Function to map a point to a grid cell
    std::pair<int, int> mapToGrid(float x, float y) {
        int cellX = static_cast<int>(x / gridSize);
        int cellY = static_cast<int>(y / gridSize);
        return {cellX, cellY};
    }

    // Function to insert or update cab location
    void updateCabLocation(float oldX, float oldY, float newX, float newY) {
        auto oldCell = mapToGrid(oldX, oldY);
        auto newCell = mapToGrid(newX, newY);

        if (oldCell != newCell) {
            removeCabFromCell(oldCell.first, oldCell.second, oldX, oldY);
            addCabToCell(newCell.first, newCell.second, newX, newY);
        }
    }

    // Function to find the nearest cab to a given location
    Point findNearestCab(float x, float y) {
        auto cell = mapToGrid(x, y);
        float minDist = std::numeric_limits<float>::max();
        Point nearestCab = {0, 0};

        // Check the current cell and neighboring cells
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                auto neighborCell = std::make_pair(cell.first + dx, cell.second + dy);
                if (grid.find(neighborCell) != grid.end()) {
                    for (const auto& cab : grid[neighborCell].cabs) {
                        float dist = std::sqrt((cab.x - x) * (cab.x - x) + (cab.y - y) * (cab.y - y));
                        if (dist < minDist) {
                            minDist = dist;
                            nearestCab = cab;
                        }
                    }
                }
            }
        }
        return nearestCab;
    }

private:
    float gridSize;
    std::unordered_map<std::pair<int, int>, GridCell, pair_hash> grid;

    // Helper function to add a cab to a grid cell
    void addCabToCell(int cellX, int cellY, float x, float y) {
        auto cell = std::make_pair(cellX, cellY);
        grid[cell].cabs.push_back({x, y});
    }

    // Helper function to remove a cab from a grid cell
    void removeCabFromCell(int cellX, int cellY, float x, float y) {
        auto cell = std::make_pair(cellX, cellY);
        auto& cabs = grid[cell].cabs;
        cabs.erase(std::remove_if(cabs.begin(), cabs.end(),
                                  [x, y](const Point& p) { return p.x == x && p.y == y; }),
                   cabs.end());
    }
};

// Hash function for pair<int, int>
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator ()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ h2;
    }
};

int main() {
    GridManager manager(1.0f); // Grid size of 1.0 unit

    // Add some cabs
    manager.updateCabLocation(2.5f, 3.5f, 2.5f, 3.5f);
    manager.updateCabLocation(5.0f, 7.0f, 5.0f, 7.0f);

    // Find the nearest cab to a given location
    Point nearest = manager.findNearestCab(5.1f, 7.1f);
    std::cout << "Nearest cab at: (" << nearest.x << ", " << nearest.y << ")\n";

    return 0;
}
