import os
import pandas as pd


dirName = "testData"

bestTv = float('inf')
lines, tLams, sLams = [], [], []
dfAll = pd.DataFrame(columns = ['ObjValue', 'tLam', 'sLam', 'scenario'])
for subdir, dirs, files in os.walk("data/experimentData/" + dirName):
    for file in sorted(files):
        penalty = int(file[10:14])
        arrivalRate = int(file[15:17])
        tLam = file[25:30]
        sLam = file[34:38]
        if (sLam != "rehso"):
            with open(os.path.join(subdir, file)) as fileToOpen:
                df = pd.read_csv(os.path.join(subdir, file), delimiter=" ")
                objValues = df.iloc[:,2].to_list()
                scenario = "Base"
                if arrivalRate == 30:
                    scenario = "Low demand"
                elif arrivalRate == 20:
                    scenario = "High demand"
                elif penalty == 3600:
                    scenario = "high rej. costs"
                elif penalty == 1800:
                    scenario = "low rej. costs"

                tempLambdas = [tLam for _i in range(len(objValues))]
                spatLambdas = [sLam for _i in range(len(objValues))]
                scnearios = [scenario for _i in range(len(objValues))]

                df = pd.DataFrame(list(zip(objValues, tempLambdas, spatLambdas, scnearios)),columns =['ObjValue', 'tLam', 'sLam', 'scenario'])
                dfAll = pd.concat((dfAll,df), ignore_index = True)

custom_dict = {'0.000': 0, '0.500': 1, '0.750': 2,'0.950': 3,'0.980': 4,'0.990': 5,'0.995':  6,'1.000': 7,'Base': 8, 'Low demand': 9, 'High demand': 10, 'High rej. costs': 11, 'Low rej. costs': 12 } 
dfAll = dfAll.sort_values(by=['tLam','scenario'], key=lambda x: x.map(custom_dict))
groupBydf = dfAll.groupby(['tLam', "sLam", "scenario"]).mean().round(2).unstack()
groupBydf = groupBydf.sort_values(by=['tLam', 'sLam'], key=lambda x: x.map(custom_dict))
print(groupBydf.applymap(lambda x: str.format("{:0_.0f}", x).replace('_', ',')).to_latex())



