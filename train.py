import subprocess


nearest_policy = True
lambdasTemporal = [0,0.5,0.75, 0.95, 0.98, 0.99, 1]
lambdasSpatial = [0,0.25, 0.5, 0.75, 1]
scenarios = [[2700, 25],[1800,25],[3600,25],[2700,20],[2700,30]]


# for complex commands, with many args, use string + `shell=True`:
for scenario in scenarios:
    if nearest_policy:
        subprocess.run(["./onlineAssignment", "instances/instance_test.txt", str(12), str(scenario[0]), str(scenario[1]), "nearestWarehouse"])
    else:
        for lamT in lambdasTemporal:
            if lamT == 0:
                lamS = 0
                subprocess.run(["./onlineAssignment", "instances/instance_train.txt", str(12), str(scenario[0]), str(scenario[1]), "trainREINFORCE", str(lamT), str(lamS)])
                subprocess.run(["./onlineAssignment", "instances/instance_test.txt", str(12), str(scenario[0]), str(scenario[1]), "testREINFORCE", str(lamT), str(lamS)])
            else:
                for lamS in lambdasSpatial:
                    subprocess.run(["./onlineAssignment", "instances/instance_train.txt", str(12), str(scenario[0]), str(scenario[1]), "trainREINFORCE", str(lamT), str(lamS)])
                    subprocess.run(["./onlineAssignment", "instances/instance_test.txt", str(12), str(scenario[0]), str(scenario[1]), "testREINFORCE", str(lamT), str(lamS)])
