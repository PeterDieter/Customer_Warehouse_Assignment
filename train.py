import subprocess


lambdasTemporal = [0.99]
lambdasSpatial = [0,0.5]
penalties = [2700]
interArrivalTimes = [25]

# for complex commands, with many args, use string + `shell=True`:
for interArrivalTime in interArrivalTimes:
    for penalty in penalties:
        for lamT in lambdasTemporal:
            for lamS in lambdasSpatial:
                subprocess.run(["./onlineAssignment", "instances/instance_train.txt", str(12), str(penalty), str(interArrivalTime), "trainREINFORCE", str(lamT), str(lamS)])
                subprocess.run(["./onlineAssignment", "instances/instance_test.txt", str(12), str(penalty), str(interArrivalTime), "testREINFORCE", str(lamT), str(lamS)])