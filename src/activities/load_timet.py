from datetime import datetime
from pathlib import Path
import json
import sys

TIMET_STATE = Path.home() / ".cache/timet/"

# YYYY-MM-DD
ls = [TIMET_STATE / day for day in sys.argv[1:]] or TIMET_STATE.iterdir()

result: "dict[str, list[int]]" = {}
for date in sorted(ls):
    for line in date.read_text().splitlines():
        proj, event, time = line.split("\t")
        dt = datetime.fromisoformat(f"{date.name} {time}")
        result.setdefault(proj, []).append(int(dt.timestamp()))

print(json.dumps(result))
