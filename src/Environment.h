#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <assert.h>
#include <string>
#include <vector>
#include <limits.h>
#include <iostream>
#include <ctime>
#include <chrono>
#include <random>

#include <torch/torch.h>
#include <torch/script.h>
#include "Matrix.h"
#include "Data.h"
#include "Environment.h"

struct policyNetwork;

class Environment
{
public:
	// Constructor 
	Environment(Data* data);

	// Function to perform a simulation
	void simulate(char * argv[]);

private:
	Data* data;													// Problem parameters
	std::vector<Order*> orders;									// Vector of pointers to orders. containing information on each order
	std::vector<Order*> ordersAssignedToCourierButNotServed;	// Vector of orders that have not been served yet
	std::vector<Warehouse*> warehouses;							// Vector of pointers containing information on each warehouse
	std::vector<Courier*> couriers;								// Vector of pointers containing  information on each courier
	std::vector<Picker*> pickers;								// Vector of pointers  containing information on each picker
	std::vector<Route*> routes;									// Vector of pointers  containing information on each route
	Order* nextOrderBeingServed;								// Order that will be served next. Needed as we have two types of decision epoch: Order arriving and order being served (courier needs to be reassigned)
	std::vector<int> orderTimes;								// Vector of times at which clients arrive. Will be created upon initialization
	std::vector<int> clientsVector;								// Vector of clients that arrive. Same length as orderTimes vector. Will be created upon initialization
	std::vector<int> timesToComission;							// Vector of times to comission. Same length as orderTimes vector. Will be created upon initialization
	std::vector<int> timesToServe;								// Vector of times how long it takes to serve a client at his house. Same length as orderTimes vector. Will be created upon initialization
	int currentTime;
	int nbOrdersServed;
	int rejectCount;
	int timeCustomerArrives;
	int timeNextCourierArrivesAtOrder;
	int totalWaitingTime;
	int highestWaitingTimeOfAnOrder;
	int latestArrivalTime;
	torch::Tensor assingmentProblemStates;
	torch::Tensor assingmentProblemActions;

	// In this method we apply the nearest warehouse policy.
	void nearestWarehousePolicy(int timelimit);
	// In these methods we train and test a REINFORCE algorithm
	void trainREINFORCE(int timelimit, float lambdaTemporal, float lambdaSpatial);
	void testREINFORCE(int timeLimit, float lambdaTemporal, float lambdaSpatial);

	// In this method we initialize the rest of the Data, such as warehouses, couriers, etc.
	void initialize(int timeLimit);

	// Function to initialize the values of an order
	void initOrder(int currentTime, Order* o);

	// Functions that assigns order to a warehouse, picker and courier, respectively
	void chooseClosestWarehouseForOrder(Order* newOrder);
	void choosePickerForOrder(Order* newOrder);
	void chooseCourierForOrder(Order* newOrder);
	
	// Function that assigns order to a warehouse with the REINFORCE algorithm
	void chooseWarehouseForOrderREINFORCE(Order* newOrder, policyNetwork& n, bool train);

	// Function that assigns a courier to the closest warehouse
	void chooseClosestWarehouseForCourier(Courier* courier);

	// Function that deletes order from ordersNotServed vector
	void RemoveOrderFromVector(std::vector<Order*> & V, Order* orderToDelete);

	// Function that adds order to a vector of orders based on the (expected) arrival time
	void AddOrderToVector(std::vector<Order*> & V, Order* orderToAdd);

	// Function that returns the euclidean distance between two locations
	double euclideanDistance(double latFrom, double latTo, double lonFrom, double lonTo);

	// Function that deletes order from ordersNotServed vector
	void RemoveCourierFromVector(std::vector<Courier*> & V, Courier* courierToDelete);

	// Function that returns the fastest available picker at a warehouse
	Picker* getFastestAvailablePicker(Warehouse* warehouse);

	// Function that returns the fastest available courier assigned to a warehouse
	Courier* getFastestAvailableCourier(Warehouse* warehouse);

	// 
	int getNumberOfAvailablePickers(Warehouse* warehouse);

	// Function that updates the order that will be served next
	void updateOrderBeingServedNext();

	// Function that saves a route to the list of routes
	void saveRoute(int startTime, int arrivalTime, double fromLat, double fromLon, double toLat, double toLon);

	// Functions that writes routes/orders and costs to file
	void writeRoutesAndOrdersToFile(std::string fileNameRoutes, std::string fileNameOrders);
	void writeCostsToFile(std::vector<float> costs, std::vector<float> averageRejectionRateVector, float lambdaTemporal, float lambdaSpatial, bool is_training);
	void writeStatsToFile(std::vector<float> costs, std::vector<float> averageRejectionRateVector, std::vector<float> averageWaitingTime, std::vector<float> maxWaitingTime, float lambdaTemporal, float lambdaSpatial, bool is_training, bool is_nearest_policy);
	// Function to draw an inter arrival time based on rate specified in data
	int drawFromExponentialDistribution(double lambda);

	// Function that returns the objective value (waiting time + penalty)
	int getObjValue();


	// Function that returns the state as a tensor
	torch::Tensor getStateAssignmentProblem(Order* order);
	// Function that returns the costs of each action
	torch::Tensor getCostsVectorDiscountedAssignmentProblem(float lambdaTemporal, float lambdaSpatial);
};

struct policyNetwork : torch::nn::Module {
	policyNetwork(int64_t inputSize, int64_t outputSize) {
		fc1 = register_module("fc1", torch::nn::Linear(inputSize, 1024));
		fc2 = register_module("fc2", torch::nn::Linear(1024, 512));
		fc3 = register_module("fc3", torch::nn::Linear(512, 256));
		fc4 = register_module("fc4", torch::nn::Linear(256, outputSize));
	}

	// Implement the Net's algorithm.
	torch::Tensor forward(torch::Tensor x) {	
		// Use one of many tensor manipulation functions.
		x = torch::layer_norm(x, (x.size(1)));
		x = torch::relu(fc1->forward(x));
		// x = torch::layer_norm(x, (x.size(1)));
		x = torch::relu(fc2->forward(x));
		x = torch::relu(fc3->forward(x));
		x = fc4->forward(x);
		x = torch::softmax(x, /*dim=*/1);
		return x;
	}

	// Use one of many "standard library" modules.
	torch::nn::Linear fc1{nullptr}, fc2{nullptr}, fc3{nullptr}, fc4{nullptr};
};


struct logLoss : public torch::nn::Module {
public:
    logLoss() {}

    torch::Tensor forward(torch::Tensor probabilities, torch::Tensor costs) {
        // Calculate the log loss function
        torch::Tensor loss = (torch::mul(torch::log(probabilities),costs)).mean();
        return loss;
    }
};

struct entropyLoss : public torch::nn::Module {
public:
    entropyLoss() {}

    torch::Tensor forward(torch::Tensor probabilities, torch::Tensor costs, torch::Tensor entropies) {
        // Calculate the log loss function
        torch::Tensor loss = (torch::add(torch::mul(torch::log10(probabilities),costs),-entropies)).mean();
        return loss;
    }
};


#endif