import os
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import pandas as pd
import matplotlib.ticker as mticker
from cycler import cycler
from matplotlib.ticker import StrMethodFormatter, NullFormatter, FuncFormatter

scientific_colors= [
    '#1f77b4',  # Blue
#    '#ff7f0e',  # Orange
    '#2ca02c',  # Green
    '#d62728',  # Red
    '#1f77b4',  # Purple
    '#8c564b',  # Brown
    '#7f7f7f',  # Gray
    '#000000'   # Cyan
]

line_cycler   = (cycler(color=scientific_colors) +
                 cycler(linestyle=[":", "--", "-.", "-", ":", "--", "-."]))

def smooth(scalars, weight):  # Weight between 0 and 1
    last = scalars[0]  # First value in the plot (first timestep)
    smoothed = list()
    for point in scalars:
        smoothed_val = last * weight + (1 - weight) * point  # Calculate smoothed value
        smoothed.append(smoothed_val)                        # Save it
        last = smoothed_val                                  # Anchor the last smoothed value
        
    return smoothed

trainingData = True
if trainingData:
    division = 100
    multiplier = 100
    offset = 3
    fileName = "trainingData"
else:
    division = 1000
    offset = 0
    multiplier = 1
    fileName = "testData"

bestTv = float('inf')
lines, tLams, sLams = [], [], []
for subdir, dirs, files in os.walk("data/experimentData/" + fileName):
    for file in sorted(files):
        penalty = int(file[10+offset:14+offset])
        arrivalRate = int(file[15+offset:17+offset])
        tLam = file[25+offset:30+offset]
        sLam = file[34+offset:38+offset]
        if penalty == 2700 and arrivalRate == 25 and sLam == "1.00" and tLam != "0.980":
            with open(os.path.join(subdir, file)) as fileToOpen:
                df = pd.read_csv(os.path.join(subdir, file), delimiter=" ")
                objValues = df.iloc[:,0].to_list()
                tv = sum(objValues)
                if tv < bestTv:
                    bestTv = tv
                    besttLam, bestslam = tLam, sLam
                lines.append(objValues)
                tLams.append(tLam)
                sLams.append(sLam)
print(besttLam, bestslam, bestTv/division)
x = []
for j in range(len(lines[0])):
    x.append((j+1)*multiplier)

print(len(lines))
fig, ax = plt.subplots()
ax.set_prop_cycle(line_cycler)
plt.style.use('ggplot')
for idx, line in enumerate(lines):
    plt.plot(x, smooth(line, .7), label="$λ_t$ = " + str(round(float(tLams[idx]),3)) + " $λ_s$ = " + sLams[idx],)


plt.xlabel("Iteration")
plt.ylabel("Average costs")
plt.legend()
plt.yscale("log")
ax.yaxis.set_minor_formatter(FuncFormatter(lambda x, p: format(int(x), ',')))
ax.yaxis.set_major_formatter(FuncFormatter(lambda x, p: format(int(x), ',')))
ax.xaxis.set_major_formatter(FuncFormatter(lambda x, p: format(int(x), ',')))
plt.ylim([1800000, 10000000])
plt.xlim([0, 10500])
plt.gcf().subplots_adjust(left=0.2)
plt.savefig("convergenceTemporal_1.png", dpi=1000)
plt.show()
