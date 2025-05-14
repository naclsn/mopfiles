assert not "done", "matplotlib sucks, change that or away with python"

from matplotlib.dates import DateFormatter
import argparse
import json
import matplotlib.pyplot as plt
import mplcursors
import numpy as np


def timestep(value: str):
    """Y year / M month / W week / D day / h hour / m minute / s second"""
    unit = value.lstrip("0123456789")
    return np.timedelta64(value[: -len(unit)], unit)


parser = argparse.ArgumentParser()
parser.add_argument("-t", "--timestep", type=timestep, required=True)
parser.add_argument("files", nargs="+", type=argparse.FileType("r"))
opts = parser.parse_args()

data = {
    label: np.array(timestamps, dtype="datetime64[s]")
    for file in opts.files
    for label, timestamps in json.load(file).items()
}

for label, datapoints in data.items():
    t_min, t_max = datapoints[0], datapoints[-1]
    time = np.arange(t_min, t_max + opts.timestep, opts.timestep)
    hist, _ = np.histogram(datapoints, bins=time)
    width = min(opts.timestep, t_max - t_min)
    plt.bar(time[:-1], hist, label=label, width=width, alpha=0.5)

plt.gca().xaxis.set_major_formatter(DateFormatter("%a (%Y-%m-%d)"))
plt.gcf().autofmt_xdate()
plt.tight_layout()
# plt.legend()

if 0:
    mplcursors.cursor(hover=True).connect(
        "add",
        lambda sel: sel.annotation.set(
            text=sel.artist.get_label(),
            ha="center",
            va="bottom",
        ),
    )

plt.show()
