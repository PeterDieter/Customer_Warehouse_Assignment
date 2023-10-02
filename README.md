# Spatio-temporal Credit Assignment in Reinforcement Learning for Multi-Depot Delivery Services

In the problem at hand, we operate multiple warehouse (depots) in a service region from which couriers start their trip to serve an order. When an order arrives, we need to assign it to a warehouse and a courier. The goal is to minimize the waiting time of customers. We can also reject customers, which comes with certain costs.

Visualization of the problem (made in [visualizeSimulation.py](python/visualizeSimulation.py)):

<p align="center">
<img src="animation_REINFORCE.gif" width="300" height="400" align="center">
</p>


## Data
For the warehouses, we use (the 10) Getir stores in Chicago. For the order data, we use publicly available anonymized customer data of Amazon customers in Chicago. We then draw random orders from these customers. Interarrival times, service times and comission times are all assumed to be exponentially distributed. An instance can be created in [createInstance.py](python/createInstance.py):


## C++ compiling 
Data is prepared in Python and an instance is then passed to C++. The raw and processed data is contained in folder [data](data). All code related to preprocessing data (including Isochrone API and DistanceMatrix API) and creating code to create instances is contained in [python](python).

For neural network stuff, we use Pytorch. So make sure that Pytorch is installed and check [this](https://github.com/pytorch/pytorch/issues/12449) out if cmake has trouble finding Pytorch. Furthermore, we use cmake to create an executable. Run 

```
cmake -DCMAKE_PREFIX_PATH=$PWD/../libtorch
make
```
alternatively, you might want to try:

```
cmake -DCMAKE_PREFIX_PATH=$PWD/../../libtorch
make
```

## Running the program

You can then execute the code with:

```
./onlineAssignment instanceName simulationLength rejectionCosts interArrivalRate methodName
```

where **instanceName** gives the path to the .txt file containing the instance information. The second parameter is the **simulation length** in hours (int). The third parameter is the **rejection costs** in seconds (int). The fourth parameter is the **interarrival rate** in seconds (int). The **methodName** is a string that determines the method which will be applied/trained for the assignment problem In case the REINFORCE method is trained, one additionally needs to specify  **lambdaTemporal** and **lambdaSpatial**, which are float parameters (see section above). For example:

```
./onlineAssignment instances/instance_train.txt 6 3600 25 trainREINFORCE 0.95 0.85
```

Currently, the following assigning strategies are available:
1. nearestWarehouse: In this policy, the nearest warehouse is selected for each order and each courier is also assigned back to his nearest warehouse. Each order is accepted.
2. trainREINFORCE: In this method, we train a neural network with the REINFORCE algorithm to assign orders to warehouses/ to reject orders. The neural network gets saved as "net_REINFORCE.pt".
3. testREINFORCE: We apply the policy net which was trained in the "trainREINFORCE" method.

