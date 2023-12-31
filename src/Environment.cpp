#include <algorithm>
#include <set>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <random>

#include <torch/torch.h>
#include <torch/script.h>
#include "Data.h"
#include "Matrix.h"
#include "Environment.h"


Environment::Environment(Data* data) : data(data)
{   
    std::cout<<"----- Create Environment -----"<<std::endl;
}


void Environment::initialize(int timeLimit)
{
    
    // CONSTRUCTOR: First we initialize the environment by assigning 
    int courierCounter = 0;
    int pickerCounter = 0;
    totalWaitingTime = 0;
    highestWaitingTimeOfAnOrder = 0;
    latestArrivalTime = 0;
    nbOrdersServed = 0;
    rejectCount = 0;
    nextOrderBeingServed = nullptr;
    
    for (size_t ord=0; ord<ordersAssignedToCourierButNotServed.size(); ord++) {
			delete ordersAssignedToCourierButNotServed[ord];
	}
    ordersAssignedToCourierButNotServed = std::vector<Order*>(0);
    
    for (size_t ord=0; ord<couriers.size(); ord++) {
		delete couriers[ord];
	}
    couriers = std::vector<Courier*>(0);
	
    for (size_t ord=0; ord<pickers.size(); ord++) {
		delete pickers[ord];
	}
    pickers = std::vector<Picker*>(0);

	for (size_t ord=0; ord<orders.size(); ord++) {
		delete orders[ord];
	}
    orders = std::vector<Order*>(0);
	
    for (size_t ord=0; ord<routes.size(); ord++) {
		delete routes[ord];
	}
    routes = std::vector<Route*>(0);

    warehouses = std::vector<Warehouse*>(0);

    for (int wID = 0; wID < data->nbWarehouses; wID++)
    {
        Warehouse* newWarehouse = new Warehouse;
        warehouses.push_back(newWarehouse);
        newWarehouse->wareID = data->paramWarehouses[wID].wareID;
        newWarehouse->lat = data->paramWarehouses[wID].lat;
        newWarehouse->lon = data->paramWarehouses[wID].lon;
        newWarehouse->initialNbCouriers = data->paramWarehouses[wID].initialNbCouriers;
        newWarehouse->initialNbPickers = data->paramWarehouses[wID].initialNbPickers;
        newWarehouse->ordersNotAssignedToCourier = std::vector<Order*>(0);
        for (int cID = 0; cID < newWarehouse->initialNbCouriers; cID++)
        {
            Courier* newCourier = new Courier;
            newCourier->courierID = courierCounter;
            newCourier->assignedToWarehouse = warehouses[wID];
            newCourier->assignedToOrder = nullptr;
            newCourier->timeWhenAvailable = 0;
            couriers.push_back(newCourier);
            newWarehouse->couriersAssigned.push_back(newCourier);
            courierCounter ++;    
        }
        for (int pID = 0; pID < newWarehouse->initialNbPickers; pID++)
        {
            Picker* newPicker = new Picker;
            newPicker->pickerID = pickerCounter;
            newPicker->assignedToWarehouse = warehouses[wID];
            newPicker->timeWhenAvailable = 0;
            pickers.push_back(newPicker);
            newWarehouse->pickersAssigned.push_back(newPicker);
            pickerCounter ++;    
        }
    }

    // Now we draw the random numbers
    orderTimes = std::vector<int>(0);
    clientsVector = std::vector<int>(0);
    timesToComission = std::vector<int>(0);
    timesToServe = std::vector<int>(0);
    int currTime = 0;
    int nextTime;
    while (currTime < timeLimit){
        nextTime = drawFromExponentialDistribution(data->interArrivalTime);
        currTime += nextTime;
        if(currTime > 4*3600){
            data->interArrivalTime = 15; 
        }
        orderTimes.push_back(nextTime);
        clientsVector.push_back(data->rng() % data->nbClients);
        timesToComission.push_back(drawFromExponentialDistribution(data->meanCommissionTime));
        timesToServe.push_back(drawFromExponentialDistribution(data->meanServiceTimeAtClient));
    }
}

void Environment::initOrder(int currentTime, Order* o)
{
    o->orderID = orders.size();
    o->timeToComission = timesToComission[o->orderID]; // Follows expoential distribution
    o->assignedCourier = nullptr;
    o->assignedPicker = nullptr;
    o->assignedWarehouse = nullptr;
    o->client = &data->paramClients[clientsVector[o->orderID]];
    o->orderTime = currentTime;
    o->arrivalTime = -1;
}

int Environment::drawFromExponentialDistribution(double lambda)
{
    // Random number generator based on poisson process
    double lambdaInv = 1/lambda;
    std::exponential_distribution<double> exp (lambdaInv);
    return round(exp.operator() (data->rng));
}

void Environment::choosePickerForOrder(Order* newOrder) 
{
    // We choose the picker who is available fastest
    newOrder->assignedPicker = getFastestAvailablePicker(newOrder->assignedWarehouse);
    // We set the time the picker is available again to the maximum of either the previous availability time or the current time, plus the time needed to comission the order
    newOrder->assignedPicker->timeWhenAvailable = std::max(newOrder->assignedPicker->timeWhenAvailable, currentTime) + newOrder->timeToComission;
}

void Environment::chooseCourierForOrder(Order* newOrder)
{
    // We choose the courier who is available fastest
    newOrder->assignedCourier = getFastestAvailableCourier(newOrder->assignedWarehouse);
    // We set the time the courier is arriving at the order to the maximum of either the current time, or the time the picker or couriers are available (comission time for picker has already been accounted for before). We then add the distance to the warehouse
    newOrder->arrivalTime = std::max(currentTime, std::max(newOrder->assignedCourier->timeWhenAvailable, newOrder->assignedPicker->timeWhenAvailable)) + data->travelTime.get(newOrder->client->clientID, newOrder->assignedWarehouse->wareID);
    newOrder->assignedCourier->assignedToOrder = newOrder;
    if(newOrder->arrivalTime<timeNextCourierArrivesAtOrder){
        timeNextCourierArrivesAtOrder = newOrder->arrivalTime;
        nextOrderBeingServed = newOrder;
    }

    if (latestArrivalTime < newOrder->arrivalTime){
        latestArrivalTime = newOrder->arrivalTime;
    }

    saveRoute(std::max(currentTime, std::max(newOrder->assignedCourier->timeWhenAvailable, newOrder->assignedPicker->timeWhenAvailable)), newOrder->arrivalTime, newOrder->assignedCourier->assignedToWarehouse->lat, newOrder->assignedCourier->assignedToWarehouse->lon, newOrder->client->lat, newOrder->client->lon);

    // Remove order from vector of orders that have not been assigned to a courier yet (If applicable)   
    RemoveOrderFromVector(newOrder->assignedWarehouse->ordersNotAssignedToCourier, newOrder);
    // Remove courier from vector of couriers assigned to warehouse
    RemoveCourierFromVector(newOrder->assignedWarehouse->couriersAssigned, newOrder->assignedCourier);
    //newOrder->assignedCourier->assignedToWarehouse = nullptr;
    newOrder->assignedCourier->timeWhenAvailable = currentTime;
}


void Environment::chooseClosestWarehouseForCourier(Courier* courier)
{
    // For now, we just assign the courier back to the warehouse he came from
    // draw service time needed to serve the client at the door
    courier->assignedToOrder->serviceTimeAtClient = timesToServe[courier->assignedToOrder->orderID];   
    // Compute the time the courier is available again, i.e., can leave the warehouse that we just assigned him to
    courier->timeWhenAvailable = courier->assignedToOrder->arrivalTime + courier->assignedToOrder->serviceTimeAtClient + data->travelTime.get(courier->assignedToOrder->client->clientID, courier->assignedToWarehouse->wareID);
    // Add the courier to the vector of assigned couriers at the respective warehouse
    courier->assignedToWarehouse->couriersAssigned.push_back(courier);
    // Increment the number of order that have been served
    nbOrdersServed ++;
    totalWaitingTime += courier->assignedToOrder->arrivalTime - courier->assignedToOrder->orderTime;
    if (highestWaitingTimeOfAnOrder < courier->assignedToOrder->arrivalTime - courier->assignedToOrder->orderTime)
    {
        highestWaitingTimeOfAnOrder = courier->assignedToOrder->arrivalTime - courier->assignedToOrder->orderTime;
    }
    if (latestArrivalTime < courier->timeWhenAvailable){
        latestArrivalTime = courier->timeWhenAvailable;
    }
    
    saveRoute(courier->assignedToOrder->arrivalTime, courier->timeWhenAvailable, courier->assignedToOrder->client->lat, courier->assignedToOrder->client->lon, courier->assignedToWarehouse->lat, courier->assignedToWarehouse->lon);
    
    // Remove the order from the order that have not been served
    RemoveOrderFromVector(ordersAssignedToCourierButNotServed, nextOrderBeingServed);
    // Update the order that will be served next
    updateOrderBeingServedNext();
    courier->assignedToOrder = nullptr;
}

void Environment::saveRoute(int startTime, int arrivalTime, double fromLat, double fromLon, double toLat, double toLon){
    Route* route = new Route;
    route->fromLat = fromLat; route->fromLon = fromLon; route->toLat = toLat; route->tolon = toLon;
    route->startTime = startTime;
    route->arrivalTime = arrivalTime;
    routes.push_back(route);
}

void Environment::writeRoutesAndOrdersToFile(std::string fileNameRoutes, std::string fileNameOrders){
	std::cout << "----- WRITING Routes IN : " << fileNameRoutes << " and Orders IN : " << fileNameOrders << std::endl;
	std::ofstream myfile(fileNameRoutes);
	if (myfile.is_open())
	{
		for (auto route : routes)
		{
            // Here we print the order of customers that we visit 
            myfile << route->startTime << " " << route->arrivalTime << " " << route->fromLat << " " << route->fromLon << " " << route->toLat << " " << route->tolon;
            myfile << std::endl;
		}
	}
	else std::cout << "----- IMPOSSIBLE TO OPEN: " << fileNameRoutes << std::endl;
    // Now the orders
    std::ofstream myfile2(fileNameOrders);
	if (myfile2.is_open())
	{
		for (auto order : orders)
		{
            if (order->accepted){
                if (order->arrivalTime == -1){
                    // Here we print the order of customers that we visit 
                    myfile2 << order->orderTime << " " << latestArrivalTime << " " << order->client->lat << " " << order->client->lon << " " << 1;
                    myfile2 << std::endl;
                }else{
                    myfile2 << order->orderTime << " " << order->arrivalTime << " " << order->client->lat << " " << order->client->lon << " " << 1;
                    myfile2 << std::endl;
                }
            }else{
                myfile2 << order->orderTime << " " << order->orderTime + 180<< " " << order->client->lat << " " << order->client->lon << " " << 0;
                myfile2 << std::endl;
            }
		}
	}
	else std::cout << "----- IMPOSSIBLE TO OPEN: " << fileNameOrders << std::endl;
}

int Environment::getObjValue(){
    int objectiveValue = 0;
    for (Order* order: orders){
        if (order->accepted){
            if (order->arrivalTime == -1){
                objectiveValue += data->penaltyForNotServing;
            }else{
                objectiveValue += (order->arrivalTime-order->orderTime); 
            }
        }else{
            objectiveValue += data->penaltyForNotServing;
        }
    } 
    return objectiveValue;
}

void Environment::writeCostsToFile(std::vector<float> costs, std::vector<float> averageRejectionRateVector, float lambdaTemporal, float lambdaSpatial, bool is_training){
    std::string fileName;
    if (is_training){
        fileName = "data/experimentData/trainingData/averageCosts_" + std::to_string(data->penaltyForNotServing) + "_" + std::to_string(data->interArrivalTime) + "_" + std::to_string(lambdaTemporal) + "_" + std::to_string(lambdaSpatial) +".txt";
    }
    else{
        fileName = "data/experimentData/testData/averageCosts_" + std::to_string(data->penaltyForNotServing) + "_" + std::to_string(data->interArrivalTime) + "_"  + std::to_string(lambdaTemporal) + "_" + std::to_string(lambdaSpatial) +".txt";
    }
 
	std::cout << "----- WRITING COST VECTOR IN : " << fileName << std::endl;
	std::ofstream myfile(fileName);
	if (myfile.is_open())
	{
        int _i = 0;
        myfile << "TotalCosts " << "RejectionRate ";
        myfile << std::endl;
		for (auto cost : costs)
		{
            // Here we print the order of customers that we visit 
            myfile << cost << " " << averageRejectionRateVector[_i];
            myfile << std::endl;
            _i += 1;
		}
	}
	else std::cout << "----- IMPOSSIBLE TO OPEN: " << fileName << std::endl;
}

void Environment::writeStatsToFile(std::vector<float> costs, std::vector<float> averageRejectionRateVector, std::vector<float> averageWaitingTime, std::vector<float> maxWaitingTime, float lambdaTemporal, float lambdaSpatial, bool is_training, bool is_nearest_policy){
    std::string fileName;
    if (is_training){
        fileName = "data/experimentData/trainingData/statsData_" + std::to_string(data->penaltyForNotServing) + "_" + std::to_string(data->interArrivalTime) + "_" + std::to_string(lambdaTemporal) + "_" + std::to_string(lambdaSpatial) +".txt";
    }
    else{
        if (is_nearest_policy){
            fileName = "data/experimentData/testData/statsData_" + std::to_string(data->penaltyForNotServing) + "_" + std::to_string(data->interArrivalTime) + "_NearestWarehousePolicy.txt";
        }else{
            fileName = "data/experimentData/testData/statsData_" + std::to_string(data->penaltyForNotServing) + "_" + std::to_string(data->interArrivalTime) + "_"  + std::to_string(lambdaTemporal) + "_" + std::to_string(lambdaSpatial) +".txt";
        }
    }
 
	std::cout << "----- WRITING COST VECTOR IN : " << fileName << std::endl;
	std::ofstream myfile(fileName);
	if (myfile.is_open())
	{
        int _i = 0;
        myfile << "TotalCosts " << "RejectionRate " <<"MeanWaitingTime " << "MaxWaitingTime ";
        myfile << std::endl;
		for (auto cost : costs)
		{
            // Here we print the order of customers that we visit 
            myfile << cost << " " << averageRejectionRateVector[_i]<< " " << averageWaitingTime[_i]<< " " << maxWaitingTime[_i];
            myfile << std::endl;
            _i += 1;
		}
	}
	else std::cout << "----- IMPOSSIBLE TO OPEN: " << fileName << std::endl;
}


void Environment::RemoveOrderFromVector(std::vector<Order*> & V, Order* orderToDelete) {
    V.erase(
        std::remove_if(V.begin(), V.end(), [&](Order* const & o) {
            return o->orderID == orderToDelete->orderID;
        }),
        V.end());
}

void Environment::AddOrderToVector(std::vector<Order*> & V, Order* orderToAdd) {
    bool inserted = false;
    for (int i = V.size() - 1; i >= 0; --i) {
        Order* obj = V[i]; // Access the object using the index
        if(obj->arrivalTime < orderToAdd->arrivalTime){
            V.insert(V.begin() + i +1, orderToAdd);
            inserted = true;
            break;
        }
    }
    if(!inserted){
       V.insert(V.begin(), orderToAdd); 
    }
}

void Environment::RemoveCourierFromVector(std::vector<Courier*> & V, Courier* courierToDelete) {
    V.erase(
        std::remove_if(V.begin(), V.end(), [&](Courier* const & o) {
            return o->courierID == courierToDelete->courierID;
        }),
        V.end());
}

Picker* Environment::getFastestAvailablePicker(Warehouse* war){
    int timeAvailable = INT_MAX;
    Picker* fastestAvailablePicker = war->pickersAssigned[0];
    for (auto picker : war->pickersAssigned){
        if(picker->timeWhenAvailable < timeAvailable){
            timeAvailable = picker->timeWhenAvailable;
            fastestAvailablePicker = picker;
        }
    }
    return fastestAvailablePicker;
}

int Environment::getNumberOfAvailablePickers(Warehouse* war){
    int availablePickers = 0;
    for (auto picker : war->pickersAssigned){
        if(picker->timeWhenAvailable < currentTime){
            availablePickers += 1;
        }
    }
    return availablePickers;
}

Courier* Environment::getFastestAvailableCourier(Warehouse* war){
    int timeAvailable = INT_MAX;
    Courier* fastestAvailableCourier = war->couriersAssigned[0];
    for (auto courier : war->couriersAssigned){
        if(courier->timeWhenAvailable < timeAvailable){
            timeAvailable = courier->timeWhenAvailable;
            fastestAvailableCourier = courier;
        }
    }
    return fastestAvailableCourier;
}

void Environment::updateOrderBeingServedNext(){
    int highestArrivalTime = INT_MAX;
    if (ordersAssignedToCourierButNotServed.size() < 1){
        nextOrderBeingServed = nullptr;
        timeNextCourierArrivesAtOrder = INT_MAX;
    }else{
        nextOrderBeingServed = ordersAssignedToCourierButNotServed[0];
        timeNextCourierArrivesAtOrder = nextOrderBeingServed->arrivalTime;
    }
}

void Environment::chooseClosestWarehouseForOrder(Order* newOrder)
{
    // For now we just assign the order to the closest warehouse
    int indexClosestWarehouse;
    std::vector<int> distancesToWarehouses = data->travelTime.getRow(newOrder->client->clientID);
    indexClosestWarehouse = std::min_element(distancesToWarehouses.begin(), distancesToWarehouses.end())-distancesToWarehouses.begin();
    
    if (warehouses[indexClosestWarehouse]->couriersAssigned.size() > 0 && getNumberOfAvailablePickers(warehouses[indexClosestWarehouse]) > 0){
        newOrder->assignedWarehouse = warehouses[indexClosestWarehouse];
        newOrder->accepted = true;
    }else{
        newOrder->accepted = false;
        rejectCount++;
    }
}

torch::Tensor Environment::getStateAssignmentProblem(Order* order){
    // For now, the state is only the distances to the warehouses
    std::vector<int> distancesToWarehouses = data->travelTime.getRow(order->client->clientID);
    std::vector<float> state;
    for (int i = 0; i < distancesToWarehouses.size(); i++) {
        state.push_back(distancesToWarehouses[i]);
    }
    
    std::vector<int> wareHouseLoad;
    for (Warehouse* w : warehouses){
        state.push_back(w->couriersAssigned.size());
        state.push_back(getNumberOfAvailablePickers(w));
        state.push_back(std::max(0, getFastestAvailablePicker(w)->timeWhenAvailable - currentTime));
        state.push_back(std::max(0, getFastestAvailableCourier(w)->timeWhenAvailable - currentTime));
    }

    //state.push_back(currentTime);

    // vector to tensor
    auto options = torch::TensorOptions().dtype(at::kFloat);
    torch::Tensor stateTensor = torch::from_blob(state.data(), {1, data->nbWarehouses*5}, options).clone().to(torch::kFloat);
    return stateTensor;
}



void Environment::chooseWarehouseForOrderREINFORCE(Order* newOrder, policyNetwork& n, bool train)
{
    torch::Tensor state = getStateAssignmentProblem(newOrder);
    torch::Tensor prediction = n.forward(state);
    // Prediction tensor to vector
    std::vector<float> predVector(prediction.data_ptr<float>(), prediction.data_ptr<float>() + prediction.numel());
    int indexWarehouse;
    if (train){
        std::discrete_distribution<> discrete_dist(predVector.begin(), predVector.end());
        // Choose based on the distribution
        indexWarehouse = discrete_dist(data->rng);
    }else{
        indexWarehouse = std::max_element(predVector.begin(), predVector.end())-predVector.begin();
    }

    // If the index is nb.warehouses, we reject the order
    if (indexWarehouse >= data->nbWarehouses){
        newOrder->accepted = false;
        rejectCount++;
    }else{
        newOrder->assignedWarehouse = warehouses[indexWarehouse];
        newOrder->accepted = true;
    }

    if (train){
        if (newOrder->orderID == 0){
            assingmentProblemStates = state;
            assingmentProblemActions = torch::tensor({indexWarehouse});
        }else{
            assingmentProblemStates = torch::cat({assingmentProblemStates, state});
            assingmentProblemActions = torch::cat({assingmentProblemActions, torch::tensor({indexWarehouse})});
        }
    }
}

torch::Tensor Environment::getCostsVectorDiscountedAssignmentProblem(float lambdaTemporal, float lambdaSpatial){
    std::vector<float> costsVec;
    int orderCounter = 0;
    double costsForOrder;
    for (Order* order: orders){
        orderCounter ++;
        if (order->accepted){
            if (order->arrivalTime != -1){
                costsForOrder = order->arrivalTime-order->orderTime;  
            }else{
                costsForOrder = data->penaltyForNotServing;
                costsVec.push_back(costsForOrder);
                continue;
            }
            auto start_iter = std::next(orders.begin(), orderCounter);
            for (auto orderAfter = start_iter; orderAfter != orders.end(); ++orderAfter){
                if ((*orderAfter)->arrivalTime != -1){
                    double dist = euclideanDistance((*orderAfter)->assignedWarehouse->lat, order->assignedWarehouse->lat,(*orderAfter)->assignedWarehouse->lon, order->assignedWarehouse->lon);
                    costsForOrder += ((*orderAfter)->arrivalTime-(*orderAfter)->orderTime)*pow(lambdaTemporal, (*orderAfter)->orderTime-order->orderTime) *pow(lambdaSpatial, dist);
                }
                else{
                    double dist = euclideanDistance((*orderAfter)->client->lat, order->assignedWarehouse->lat,(*orderAfter)->client->lon, order->assignedWarehouse->lon);
                    costsForOrder += (data->penaltyForNotServing)*pow(lambdaTemporal, (*orderAfter)->orderTime-order->orderTime)*pow(lambdaSpatial, dist);
                }
            } 
        }else{
            costsForOrder = data->penaltyForNotServing;
        }
        costsVec.push_back(costsForOrder);
    }

    // vector to tensor
    auto options = torch::TensorOptions().dtype(at::kFloat);
    torch::Tensor costs = torch::from_blob(costsVec.data(), {1, orderCounter}, options).clone().to(torch::kFloat);
    return costs;
}

double Environment::euclideanDistance(double latFrom, double latTo, double lonFrom, double lonTo){
    double dx = latFrom - latTo;
    double dy = lonFrom - lonTo;
    return std::sqrt(dx * dx + dy * dy)*100;
}

void Environment::trainREINFORCE(int timeLimit, float lambdaTemporal, float lambdaSpatial)
{
    std::cout<<"----- Training REINFORCE starts with lambda temporal " << lambdaTemporal << " and lambda spatial " << lambdaSpatial << " -----"<<std::endl;
    // Create neural network where each output node is assigned to a warehouse and one extra node for the reject decision
    auto assignmentNet = std::make_shared<policyNetwork>(data->nbWarehouses*5, data->nbWarehouses+1);
    torch::Tensor lossAssignmentNet;
    

    // Create an instance of the custom loss function
    logLoss loss_fn;
    // Instantiate an Adam optimization algorithm to update our Nets' parameters.
    torch::optim::Adam optimizerAssignmentNet(assignmentNet->parameters(), /*lr=*/0.0002);
    double running_costs = 0.0;
    double runningCounter = 0.0;
    double runningRejectedpercentage = 0.0;
    std::vector< float> averageCostVector;
    std::vector< float> averageRejectionRateVector;
    for (int epoch = 1; epoch <= 8000; epoch++) {
        // Initialize data structures
        initialize(timeLimit);
        // Start with simulation
        int counter = 0;
        currentTime = 0;
        timeCustomerArrives = 0;
        timeNextCourierArrivesAtOrder = INT_MAX;
        while (currentTime < timeLimit || ordersAssignedToCourierButNotServed.size() > 0){
            // Keep track of current time
            if (counter == orderTimes.size()-1){
                currentTime = timeNextCourierArrivesAtOrder;
            }else{
                currentTime = std::min(timeCustomerArrives, timeNextCourierArrivesAtOrder);
            }
            if (timeCustomerArrives < timeNextCourierArrivesAtOrder && currentTime <= timeLimit && counter<orderTimes.size()-1){
                timeCustomerArrives += orderTimes[counter];
                currentTime = timeCustomerArrives;
                counter += 1;
                // Draw new order and assign it to warehouse, picker and courier. MUST BE IN THAT ORDER!!!
                Order* newOrder = new Order;
                initOrder(timeCustomerArrives, newOrder);
                orders.push_back(newOrder);
                // We immediately assign the order to a warehouse and a picker
                chooseWarehouseForOrderREINFORCE(newOrder, *assignmentNet, true);
                if (newOrder->accepted){
                    choosePickerForOrder(newOrder);
                    // If there are couriers assigned to the warehouse, we can assign a courier to the order
                    if (newOrder->assignedWarehouse->couriersAssigned.size()>0){
                        chooseCourierForOrder(newOrder);
                        AddOrderToVector(ordersAssignedToCourierButNotServed, newOrder);
                    }else{ // else we add the order to list of orders that have not been assigned to a courier yet
                        newOrder->assignedWarehouse->ordersNotAssignedToCourier.push_back(newOrder);  
                    }
                }

            }else { // when a courier arrives at an order
                if (nextOrderBeingServed){
                    Courier* c = nextOrderBeingServed->assignedCourier;
                    // We choose a warehouse for the courier
                    chooseClosestWarehouseForCourier(c);
                    // If the chosen warehouse has order that have not been assigned to a courier yet, we can now assign the order to a courier
                    if (c->assignedToWarehouse->ordersNotAssignedToCourier.size()>0){
                        Order* orderToAssignToCourier = c->assignedToWarehouse->ordersNotAssignedToCourier[0];
                        chooseCourierForOrder(orderToAssignToCourier);
                        AddOrderToVector(ordersAssignedToCourierButNotServed, orderToAssignToCourier);
                    }
                }
            }
        }
        // Reset gradients of neural network.
        //optimizerAssignmentNet.zero_grad();
        torch::Tensor assignmentCosts = getCostsVectorDiscountedAssignmentProblem(lambdaTemporal, lambdaSpatial);
        torch::Tensor predAsssignment = assignmentNet->forward(assingmentProblemStates);
        auto rowsAssignment = torch::arange(0, predAsssignment.size(0), torch::kLong);
        auto resultAssignment = predAsssignment.index({rowsAssignment, assingmentProblemActions});
        lossAssignmentNet = loss_fn.forward(resultAssignment, assignmentCosts);
        lossAssignmentNet.backward();
        optimizerAssignmentNet.step();       // Update the parameters based on the calculated gradients.
        
        running_costs += getObjValue();
        runningRejectedpercentage += (float)rejectCount/(float)orderTimes.size();
        runningCounter += 1;
       
        if (epoch % 100 == 0) {
            std::cout << "[Iteration: " << epoch << "] Average costs: " << running_costs / runningCounter << " Rejected requests:" << runningRejectedpercentage / runningCounter << std::endl;
            averageCostVector.push_back(running_costs/runningCounter);
            averageRejectionRateVector.push_back(runningRejectedpercentage / runningCounter);
            running_costs = 0.0;
            runningCounter = 0.0;
            runningRejectedpercentage = 0.0;
        }
    
    }
    std::cout<<"----- REINFORCE training finished -----"<<std::endl;
    writeCostsToFile(averageCostVector, averageRejectionRateVector, lambdaTemporal, lambdaSpatial, true);
    torch::save(assignmentNet,"src/assignmentNet_REINFORCE.pt");
    std::cout<<"----- Policy net saved in src/assignmentNet_REINFORCE.pt -----"<<std::endl;
    
}


void Environment::testREINFORCE(int timeLimit, float lambdaTemporal, float lambdaSpatial)
{
    std::cout<<"----- Testing REINFORCE starts -----"<<std::endl;
    // Load neural network
    auto net = std::make_shared<policyNetwork>(data->nbWarehouses*5, data->nbWarehouses+1);
    torch::load(net, "src/assignmentNet_REINFORCE.pt");
    net->eval();
    
    double running_costs = 0.0;
    double runningCounter = 0.0;
    std::vector< float> averageCostVector;
    std::vector< float> averageRejectionRateVector;
    std::vector< float> meanWaitingTimeVector;
    std::vector< float> maxWaitingTimeVector;
    for (int epoch = 1; epoch <= 1000; epoch++) {
        // Initialize data structures
        initialize(timeLimit);
        // Start with simulation
        int counter = 0;
        currentTime = 0;
        timeCustomerArrives = 0;
        timeNextCourierArrivesAtOrder = INT_MAX;
        while (currentTime < timeLimit || ordersAssignedToCourierButNotServed.size() > 0){
            // Keep track of current time
            if (counter == orderTimes.size()-1){
                currentTime = timeNextCourierArrivesAtOrder;
            }else{
                currentTime = std::min(timeCustomerArrives, timeNextCourierArrivesAtOrder);
            }
            if (timeCustomerArrives < timeNextCourierArrivesAtOrder && currentTime <= timeLimit && counter<orderTimes.size()-1){
                timeCustomerArrives += orderTimes[counter];
                currentTime = timeCustomerArrives;
                counter += 1;
                // Draw new order and assign it to warehouse, picker and courier. MUST BE IN THAT ORDER!!!
                Order* newOrder = new Order;
                initOrder(timeCustomerArrives, newOrder);
                orders.push_back(newOrder);
                // We immediately assign the order to a warehouse and a picker
                chooseWarehouseForOrderREINFORCE(newOrder, *net, false);
                if (newOrder->accepted){
                    choosePickerForOrder(newOrder);
                    // If there are couriers assigned to the warehouse, we can assign a courier to the order
                    if (newOrder->assignedWarehouse->couriersAssigned.size()>0){
                        chooseCourierForOrder(newOrder);
                        AddOrderToVector(ordersAssignedToCourierButNotServed, newOrder);
                    }else{ // else we add the order to list of orders that have not been assigned to a courier yet
                        newOrder->assignedWarehouse->ordersNotAssignedToCourier.push_back(newOrder);  
                    }
                }
            }else { // when a courier arrives at an order
                if (nextOrderBeingServed){
                    Courier* c = nextOrderBeingServed->assignedCourier;
                    // We choose a warehouse for the courier
                    chooseClosestWarehouseForCourier(c);
                    // If the chosen warehouse has order that have not been assigned to a courier yet, we can now assign the order to a courier
                    if (c->assignedToWarehouse->ordersNotAssignedToCourier.size()>0){
                        Order* orderToAssignToCourier = c->assignedToWarehouse->ordersNotAssignedToCourier[0];
                        chooseCourierForOrder(orderToAssignToCourier);
                        AddOrderToVector(ordersAssignedToCourierButNotServed, orderToAssignToCourier);
                    }
                }
            }
        }
        running_costs += getObjValue();
        runningCounter += 1;
        averageCostVector.push_back(getObjValue());
        averageRejectionRateVector.push_back((float)rejectCount/(float)orderTimes.size());
        if (nbOrdersServed > 0){
            //std::cout<<"----- Iteration: " << epoch << " Number of orders that arrived: " << orders.size() << " and served: " << nbOrdersServed << " Obj. value: " << getObjValue() << ". Mean wt: " << totalWaitingTime/nbOrdersServed <<" seconds. Highest wt: " << highestWaitingTimeOfAnOrder <<" seconds. -----" <<std::endl;
            meanWaitingTimeVector.push_back(totalWaitingTime/nbOrdersServed);
            maxWaitingTimeVector.push_back(highestWaitingTimeOfAnOrder);
        }else{
            meanWaitingTimeVector.push_back(0);
            maxWaitingTimeVector.push_back(0);
        }
    }
    writeStatsToFile(averageCostVector, averageRejectionRateVector, meanWaitingTimeVector, maxWaitingTimeVector, lambdaTemporal, lambdaSpatial, false, false);
    std::cout<< "Iterations: " << runningCounter << " Average costs: " << running_costs / runningCounter <<std::endl;
    
}

void Environment::nearestWarehousePolicy(int timeLimit)
{
    std::cout<<"----- Simulation starts -----"<<std::endl;
    double running_costs = 0.0;
    double runningCounter = 0.0;
    std::vector< float> averageCostVector;
    std::vector< float> averageRejectionRateVector;
    std::vector< float> meanWaitingTimeVector;
    std::vector< float> maxWaitingTimeVector;
    for (int epoch = 1; epoch <= 1000; epoch++) {
        // Initialize data structures
        initialize(timeLimit);
        
        // Start with simulation
        int counter = 0;
        currentTime = 0;
        timeCustomerArrives = 0;
        timeNextCourierArrivesAtOrder = INT_MAX;
        while (currentTime < timeLimit || ordersAssignedToCourierButNotServed.size() > 0){
            // Keep track of current time
            if (counter == orderTimes.size()-1){
                currentTime = timeNextCourierArrivesAtOrder;
            }else{
                currentTime = std::min(timeCustomerArrives, timeNextCourierArrivesAtOrder);
            }

            if (timeCustomerArrives < timeNextCourierArrivesAtOrder && currentTime <= timeLimit && counter<orderTimes.size()-1){
                timeCustomerArrives += orderTimes[counter];
                currentTime = timeCustomerArrives;
                counter += 1;
                // Draw new order and assign it to warehouse, picker and courier. MUST BE IN THAT ORDER!!!
                Order* newOrder = new Order;
                initOrder(timeCustomerArrives, newOrder);
                orders.push_back(newOrder);
                // We immediately assign the order to a warehouse and a picker
                chooseClosestWarehouseForOrder(newOrder);
                if (newOrder->accepted){
                    choosePickerForOrder(newOrder);
                    // If there are couriers assigned to the warehouse, we can assign a courier to the order
                    if (newOrder->assignedWarehouse->couriersAssigned.size()>0){
                        chooseCourierForOrder(newOrder);
                        AddOrderToVector(ordersAssignedToCourierButNotServed, newOrder);
                    }else{ // else we add the order to list of orders that have not been assigned to a courier yet
                        newOrder->assignedWarehouse->ordersNotAssignedToCourier.push_back(newOrder);    
                    }
                }
            }else { // when a courier arrives at an order
                if (nextOrderBeingServed){
                    Courier* c = nextOrderBeingServed->assignedCourier;
                    // We choose a warehouse for the courier
                    chooseClosestWarehouseForCourier(c);
                    // If the chosen warehouse has order that have not been assigned to a courier yet, we can now assign the order to a courier
                    if (c->assignedToWarehouse->ordersNotAssignedToCourier.size()>0){
                        Order* orderToAssignToCourier = c->assignedToWarehouse->ordersNotAssignedToCourier[0];
                        chooseCourierForOrder(orderToAssignToCourier);
                        AddOrderToVector(ordersAssignedToCourierButNotServed, orderToAssignToCourier);
                    }
                }
            }
        }
        running_costs += getObjValue();
        runningCounter += 1;
        averageCostVector.push_back(getObjValue());
        averageRejectionRateVector.push_back((float)rejectCount/(float)orderTimes.size());
        if (nbOrdersServed > 0){
            //std::cout<<"----- Iteration: " << epoch << " Number of orders that arrived: " << orders.size() << " and served: " << nbOrdersServed << " Obj. value: " << getObjValue() << ". Mean wt: " << totalWaitingTime/nbOrdersServed <<" seconds. Highest wt: " << highestWaitingTimeOfAnOrder <<" seconds. -----" <<std::endl;
            meanWaitingTimeVector.push_back(totalWaitingTime/nbOrdersServed);
            maxWaitingTimeVector.push_back(highestWaitingTimeOfAnOrder);
        }else{
            meanWaitingTimeVector.push_back(0);
            maxWaitingTimeVector.push_back(0);
        }
        //std::cout<<"----- Simulation finished -----"<<std::endl;
        //std::cout<<"----- Number of orders that arrived: " << orders.size() << " and served: " << nbOrdersServed << " Obj. value: " << getObjValue() << ". Mean wt: " << totalWaitingTime/nbOrdersServed <<" seconds. Highest wt: " << highestWaitingTimeOfAnOrder <<" seconds. -----" <<std::endl;
        //writeRoutesAndOrdersToFile("data/animationData/routes.txt", "data/animationData/orders.txt");
    }
    //writeStatsToFile(averageCostVector, averageRejectionRateVector, meanWaitingTimeVector, maxWaitingTimeVector, 0, 0, false, true);
    std::cout<< "Iterations: " << runningCounter <<" Average costs: " << running_costs / runningCounter <<std::endl;
}

void Environment::simulate(char *argv[])
{   
    int timeLimit = std::stoi(argv[2])*3600;
    if (std::string(argv[5]) == "nearestWarehouse"){
        nearestWarehousePolicy(timeLimit);
    }else if (std::string(argv[5]) == "trainREINFORCE"){
        trainREINFORCE(timeLimit, std::stod(argv[6]), std::stod(argv[7]));
    }else if (std::string(argv[5]) == "testREINFORCE"){
        testREINFORCE(timeLimit, std::stod(argv[6]), std::stod(argv[7]));
    }else{
        std::cerr<<"Method: " << argv[5] << " not found."<<std::endl;
    }

}