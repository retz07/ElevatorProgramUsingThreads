#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <chrono>
#include <random>
#include <iomanip>
#include <condition_variable>
#include <limits>

using namespace std;

const int MAX_FLOORS = 10;        // Ground (0) to 9th floor
const int MAX_CAPACITY = 9;       // Maximum people in elevator
const int FLOOR_DELAY_SEC = 2;    // Delay between floors

enum Direction { UP, DOWN, IDLE };

class Passenger {
public:
    int currentFloor;
    int targetFloor;
    int id;
    static int nextId;
    
    Passenger(int curr, int target) : 
        currentFloor(curr), 
        targetFloor(target), 
        id(nextId++) {}
};
int Passenger::nextId = 1;

class Elevator {
private:
    vector<Passenger> passengers;
    queue<Passenger> waitingPassengers[MAX_FLOORS];
    int currentFloor;
    Direction direction;
    mutex mtx;
    condition_variable cv;
    bool running;
    int totalPassengers;
    int processedPassengers;
    bool hasDestinationAbove;
    bool hasDestinationBelow;

    void updateDestinationFlags() {
        hasDestinationAbove = false;
        hasDestinationBelow = false;

        // Check passengers in elevator
        for (const auto& p : passengers) {
            if (p.targetFloor > currentFloor) hasDestinationAbove = true;
            if (p.targetFloor < currentFloor) hasDestinationBelow = true;
        }

        // Check waiting passengers
        for (int i = 0; i < MAX_FLOORS; i++) {
            if (!waitingPassengers[i].empty()) {
                if (i > currentFloor) hasDestinationAbove = true;
                if (i < currentFloor) hasDestinationBelow = true;
            }
        }
    }

    Direction determineDirection() {
        updateDestinationFlags();

        if (direction == UP) {
            if (hasDestinationAbove) return UP;
            if (hasDestinationBelow) return DOWN;
            return IDLE;
        }
        if (direction == DOWN) {
            if (hasDestinationBelow) return DOWN;
            if (hasDestinationAbove) return UP;
            return IDLE;
        }
        // If IDLE, choose based on waiting passengers
        if (hasDestinationBelow) return DOWN;
        if (hasDestinationAbove) return UP;
        return IDLE;
    }

public:
    Elevator(int startingFloor) : 
        currentFloor(startingFloor),
        direction(IDLE),
        running(true),
        totalPassengers(0),
        processedPassengers(0),
        hasDestinationAbove(false),
        hasDestinationBelow(false) {}

    void clearScreen() {
        cout << "\033[2J\033[1;1H";
    }

    void displayStatus() {
        clearScreen();
        cout << "=== Elevator Status ===" << endl;
        cout << "Current Floor: " << (currentFloor == 0 ? "Ground" : to_string(currentFloor)) << endl;
        cout << "Direction: " << (direction == UP ? "UP" : direction == DOWN ? "DOWN" : "IDLE") << endl;
        cout << "Passengers in elevator: " << passengers.size() << "/" << MAX_CAPACITY << endl;
        cout << "Total passengers processed: " << processedPassengers << "/" << totalPassengers << endl;
        
        cout << "\nWaiting Passengers:" << endl;
        for (int i = MAX_FLOORS - 1; i >= 0; i--) {
            cout << "Floor " << (i == 0 ? "G" : to_string(i)) << ": ";
            if (!waitingPassengers[i].empty()) {
                cout << waitingPassengers[i].size() << " waiting";
            } else {
                cout << "None";
            }
            cout << endl;
        }
        cout << "===================" << endl;
    }

    void addPassenger(int fromFloor, int toFloor) {
        lock_guard<mutex> lock(mtx);
        waitingPassengers[fromFloor].push(Passenger(fromFloor, toFloor));
        totalPassengers++;
        cout << "Added passenger " << Passenger::nextId - 1 
             << " waiting at floor " << (fromFloor == 0 ? "G" : to_string(fromFloor))
             << " going to floor " << (toFloor == 0 ? "G" : to_string(toFloor)) << endl;
    }

    bool shouldStopAtFloor(int floor) {
        // Check if any passengers want to get off
        for (const auto& passenger : passengers) {
            if (passenger.targetFloor == floor) return true;
        }

        // Check if any waiting passengers can be picked up
        if (!waitingPassengers[floor].empty() && passengers.size() < MAX_CAPACITY) {
            Passenger front = waitingPassengers[floor].front();
            if ((direction == UP && front.targetFloor > floor) ||
                (direction == DOWN && front.targetFloor < floor) ||
                direction == IDLE) {
                return true;
            }
        }

        return false;
    }

    bool isSimulationComplete() {
        lock_guard<mutex> lock(mtx);
        return processedPassengers >= totalPassengers && passengers.empty();
    }

    void run() {
        while (running && !isSimulationComplete()) {
            unique_lock<mutex> lock(mtx);
            
            // Handle passengers getting off
            auto it = passengers.begin();
            while (it != passengers.end()) {
                if (it->targetFloor == currentFloor) {
                    cout << "Passenger " << it->id << " getting off at floor " 
                         << (currentFloor == 0 ? "G" : to_string(currentFloor)) << endl;
                    processedPassengers++;
                    it = passengers.erase(it);
                } else {
                    ++it;
                }
            }

            // Pick up waiting passengers
            while (!waitingPassengers[currentFloor].empty() && passengers.size() < MAX_CAPACITY) {
                Passenger front = waitingPassengers[currentFloor].front();
                if ((direction == UP && front.targetFloor > currentFloor) ||
                    (direction == DOWN && front.targetFloor < currentFloor) ||
                    direction == IDLE) {
                    passengers.push_back(front);
                    waitingPassengers[currentFloor].pop();
                    cout << "Passenger " << front.id << " boarding at floor "
                         << (currentFloor == 0 ? "G" : to_string(currentFloor))
                         << " going to floor " 
                         << (front.targetFloor == 0 ? "G" : to_string(front.targetFloor)) << endl;
                } else {
                    break;
                }
            }

            // Update direction
            direction = determineDirection();
            displayStatus();

            // Move elevator
            if (direction != IDLE) {
                lock.unlock();
                this_thread::sleep_for(chrono::seconds(FLOOR_DELAY_SEC));
                lock.lock();
                
                if (direction == UP && currentFloor < MAX_FLOORS - 1) {
                    currentFloor++;
                } else if (direction == DOWN && currentFloor > 0) {
                    currentFloor--;
                }
            } else {
                lock.unlock();
                this_thread::sleep_for(chrono::seconds(1));
                lock.lock();
            }
        }
    }

    void stop() {
        running = false;
        cv.notify_all();
    }
};

int getValidFloor(const string& prompt) {
    int floor;
    while (true) {
        cout << prompt;
        if (cin >> floor && floor >= 0 && floor <= 9) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return floor;
        }
        cout << "Invalid floor! Please enter a number between 0 (Ground Floor) and 9.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

int getValidPassengerCount() {
    int numPassengers;
    string input;
    while (true) {
        cout << "Enter the number of passengers (1-20): ";
        getline(cin, input);
        
        // Check if input is empty
        if (input.empty()) {
            cout << "Please enter a number between 1 and 20.\n";
            continue;
        }
        
        // Check if input contains only digits
        bool isValid = true;
        for (char c : input) {
            if (!isdigit(c)) {
                isValid = false;
                break;
            }
        }
        
        if (!isValid) {
            cout << "Invalid input! Please enter a numerical value between 1 and 20.\n";
            continue;
        }
        
        // Convert string to integer
        try {
            numPassengers = stoi(input);
            if (numPassengers >= 1 && numPassengers <= 20) {
                return numPassengers;
            } else {
                cout << "Number must be between 1 and 20!\n";
            }
        } catch (const exception& e) {
            cout << "Invalid input! Please enter a numerical value between 1 and 20.\n";
        }
    }
}

int main() {
    cout << "Welcome to the Elevator Simulation!\n\n";
    
    // Get starting floor
    int startingFloor = getValidFloor("Enter the starting floor for the elevator (0-9, 0 = Ground Floor): ");
    
    // Get number of passengers
    int numPassengers = getValidPassengerCount();

    // Create elevator with specified starting floor
    Elevator elevator(startingFloor);
    
	    // Get passenger details
    cout << "\nEnter passenger details:\n";
    for (int i = 0; i < numPassengers; i++) {
        cout << "\nPassenger " << i + 1 << ":\n";
        int fromFloor = getValidFloor("Starting floor (0-9, 0 = Ground Floor): ");
        int toFloor;
        do {
            toFloor = getValidFloor("Destination floor (0-9, 0 = Ground Floor): ");
            if (toFloor == fromFloor) {
                cout << "Destination floor must be different from starting floor!\n";
            }
        } while (toFloor == fromFloor);
        
        elevator.addPassenger(fromFloor, toFloor);
    }

    cout << "\nStarting elevator simulation...\n";
    this_thread::sleep_for(chrono::seconds(2));

    // Create thread for elevator operation
    thread elevatorThread(&Elevator::run, &elevator);

    // Wait for elevator thread to complete
    elevatorThread.join();

    cout << "\nSimulation complete! All passengers have reached their destinations.\n";

    return 0;
}