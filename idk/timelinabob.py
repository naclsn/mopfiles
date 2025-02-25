from pyglet import *
from typing import TypedDict
from colorsys import hsv_to_rgb

# timepoints series assumed sorted
data = {
    "hello": [1.0, 3.0, 6.0],
    "what": [2.0, 3.0, 8.0],
    "doin": [1.5, 2.5, 8.5, 9.0],
}

win = window.Window(720, 480, resizable=True, caption="hi :3")
batch = graphics.Batch()


class Baba(TypedDict):
    index: int
    label: str
    color: tuple[int, int, int]
    timepoints: list[float]
    textlabel: text.Label
    polygon: shapes.Polygon


LINE_THICK = 36
LINE_LENGTH = max(l[-1] for l in data.values())


def line(index: int, label: str, timepoints: list[float]):
    rf, gf, bf = hsv_to_rgb(hash(label) % 360 / 360, 1, 1)
    color = (int(rf * 255), int(gf * 255), int(bf * 255))

    #ymid = (index + 0.5) * LINE_THICK
    #ymax = ymid + LINE_THICK / 2.5
    #ymin = ymid - LINE_THICK / 2.5
    ymin = (index + 0.2) * LINE_THICK
    ymax = ymin + 0.6 * LINE_THICK

    return Baba(
        index=index,
        label=label,
        timepoints=timepoints,
        color=color,
        textlabel=text.Label(
            f"{label} ({len(timepoints)} points)",
            font_size=LINE_THICK / 2,
            x=0,
            y=ymid,
            anchor_x="left",
            anchor_y="center",
            color=color,
            batch=batch,
        ),
        polygon=shapes.Polygon(
            *(
                (0, ymin),
                (win.width / 2, ymax),
                (win.width, ymid),
                (win.width / 2, ymin),
            ),
            color=color,
            # group=polygon_group,
            batch=batch,
        ),
    )


hold = {item[0]: line(index, *item) for index, item in enumerate(data.items())}


@win.event
def on_draw():
    win.clear()
    batch.draw()


app.run()
