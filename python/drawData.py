import pandas as pd
import plotly.graph_objects as go
import json



def plotData(clients, warehouses):
    df = pd.DataFrame(clients, columns =['Idx','Latitude', 'Longitude'])
    fig = go.Figure(go.Scattermapbox(lat=df.Latitude, lon=df.Longitude, marker=go.scattermapbox.Marker(
                size=5,
                color='grey',
                opacity=1
            )))

    fig.add_trace(go.Scattermapbox(
            lat=[val.get('latitude') for val in warehouses.values()],
            lon=[val.get('longitude') for val in warehouses.values()],
            mode='markers',
            marker=dict(
                symbol = "circle",
                size=23,
                color='black',
                opacity=1
            ),
            hoverinfo='text'
        ))
    fig.update_layout(mapbox_style="carto-positron", mapbox_center_lon=-87.7493, mapbox_center_lat=41.958, mapbox_zoom=9)
    fig.update_layout(margin={"r":0,"t":0,"l":0,"b":0})
    fig.show()


if __name__ == "__main__":
    with open('data/getirStores.json') as fp:
        getirStores = json.load(fp)

    Stopls = pd.read_csv("data/stopData15Minutes.csv", header=None, names=['Idx','Latitude', 'Longitude']) 

    plotData(Stopls, getirStores)