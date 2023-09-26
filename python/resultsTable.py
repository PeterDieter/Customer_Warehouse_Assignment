import os
import pandas as pd


dirName = "testData_08_25"

bestTv = float('inf')
lines, tLams, sLams = [], [], []
dfAll = pd.DataFrame(columns = ['ObjValue', 'tLam', 'sLam', 'scenario'])
for subdir, dirs, files in os.walk("data/experimentData/" + dirName):
    for file in sorted(files):
        penalty = int(file[10:14])
        arrivalRate = int(file[15:17])
        tLam = file[25:29]
        sLam = file[34:38]
        with open(os.path.join(subdir, file)) as fileToOpen:
            df = pd.read_csv(os.path.join(subdir, file), delimiter=" ")
            objValues = df.iloc[:,0].to_list()
            scenario = "base"
            if arrivalRate == 30:
                scenario = "Low demand"
            elif arrivalRate == 20:
                scenario = "High demand"
            elif penalty == 3600:
                scenario = "High rej. costs"
            elif penalty == 1800:
                scenario = "Low rej. costs"

            tempLambdas = [tLam for _i in range(len(objValues))]
            spatLambdas = [sLam for _i in range(len(objValues))]
            scnearios = [scenario for _i in range(len(objValues))]

            df = pd.DataFrame(list(zip(objValues, tempLambdas, spatLambdas, scnearios)),columns =['ObjValue', 'tLam', 'sLam', 'scenario'])
            dfAll = pd.concat((dfAll,df), ignore_index = True)

custom_dict = {'0.00': 0, '0.50': 1, '0.75': 2,'0.95': 3,'0.98': 4,'0.99': 5,'1.00': 6,'base': 7, 'Low demand': 8, 'High demand': 9, 'High rej. costs': 10, 'Low rej. costs': 11 } 
dfAll = dfAll.sort_values(by=['tLam','scenario'], key=lambda x: x.map(custom_dict))
groupBydf = dfAll.groupby(['tLam', "scenario", "sLam"]).mean().round(0).unstack()
groupBydf = groupBydf.sort_values(by=['tLam', 'scenario'], key=lambda x: x.map(custom_dict))
print(groupBydf.applymap(lambda x: str.format("{:0_.0f}", x).replace('_', ',')).to_latex())



