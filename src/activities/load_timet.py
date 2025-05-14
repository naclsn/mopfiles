"""usage: $0 start-day [end-day]

'day' may be:
  * YYYY-MM-DD
  * yest[erday]
  * [today]
  * -<n><unit> (Y year / M month / W week / D day)

end is exclusive
"""

from datetime import date, datetime, timedelta
from pathlib import Path
import json
import re
import sys

TIMET_STATE = Path.home() / ".cache/timet/"

if "-h" in sys.argv or "--help" in sys.argv or len(sys.argv) not in {2, 3}:
    print(str(__doc__).replace("$0", sys.argv[0])), exit(1)


def day(a: str):
    if not a or "today" == a:
        return date.today()
    if "yest" == a or "yesterday" == a:
        return date.today() - timedelta(days=1)
    if ymd := re.match(r"^(\d{4})-(\d{2})-(\d{2})$", a):
        return date(*map(int, ymd.groups()))
    if "-" == a[:1] and a[-1] in "YMWD":
        r, n = date.today(), int(a[1:-1])
        if "Y" == a[-1]:
            r.replace(year=r.year - n)
        elif "M" == a[-1]:
            r.replace(month=(r.month - n - 1) % 12 + 1)
        else:
            r -= n * timedelta(days={"W": 7, "D": 1}[a[-1]])
        return r
    print(str(__doc__).replace("$0", sys.argv[0])), exit(1)


def turd(start: date, end: date):
    result: "dict[str, list[int]]" = {}
    one, it = timedelta(days=1), start
    while it < end:
        try:
            for line in (TIMET_STATE / str(it)).read_text().splitlines():
                proj, _event, time = line.split("\t")
                dt = datetime.fromisoformat(f"{it} {time}")
                result.setdefault(proj, []).append(int(dt.timestamp()))
        except FileNotFoundError:
            pass
        it += one
    return result


print(json.dumps(turd(*map(day, (sys.argv[1:] + ["", ""])[:2]))))
