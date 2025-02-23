from pyglet.math import Vec2, Vec3
from pyglet.window import key
import math
import pyglet

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from typing import override
else:
    override = lambda m, /: m


class Line:
    HEIGHT = 20

    def __init__(
        self,
        parent: "Timelines",
        index: int,  # ie baseline vertical placement
        tag: str,
        timepoints: "list[float]",
        batch: "pyglet.graphics.Batch | None" = None,
        group: "pyglet.graphics.Group | None" = None,
    ):
        self.label = pyglet.text.Label(
            f"{tag} ({len(timepoints)} points)",
            0,
            parent.x + index * Line.HEIGHT,
            font_name="monospace",
            font_size=16,
            anchor_x="center",
            anchor_y="center",
            batch=batch,
            group=group,
        )


class Timelines(pyglet.gui.WidgetBase):
    def __init__(
        self,
        x: int,
        y: int,
        width: int,
        height: int,
        batch: "pyglet.graphics.Batch | None" = None,
        group: "pyglet.graphics.Group | None" = None,
    ):
        super().__init__(x, y, width, height)

        self.time = 0
        self.drag_dist = 0

        bidoof = [
            ("a", [1.0, 3.0, 6.0]),
            ("b", [2.0, 3.0, 8.0]),
        ]
        self.lines = {
            tag: Line(self, k, tag, points, batch=batch, group=group)
            for k, (tag, points) in enumerate(bidoof)
        }

        self._set_enabled(self._enabled)

    # {{{ events
    # TODO(multiple below): self._check_hit, and is there a concept of focused?

    @override
    def on_mouse_press(self, x: int, y: int, buttons: int, modifiers: int):
        self.press_at = self.time
        self.drag_dist = 0

    @override
    def on_mouse_release(self, x: int, y: int, buttons: int, modifiers: int):
        if 1 < self.time - self.press_at or 50 < self.drag_dist:
            return

        print("click")

    @override
    def on_mouse_drag(self, x: int, y: int, dx: int, dy: int, buttons: int, modifiers: int):
        self.drag_dist += dx * dx + dy * dy
        if 50 < self.drag_dist:
            print("drag")

    @override
    def on_mouse_scroll(self, x: int, y: int, scroll_x: float, scroll_y: float):
        if scroll_y:
            print("scroll")

    # }}}


class App(pyglet.window.Window):
    def __init__(self):
        super().__init__(720, 480, resizable=True, caption="hi :3")
        self.batch = pyglet.graphics.Batch()
        self.lines = Timelines(0, 0, 720, 480, batch=self.batch)
        self.push_handlers(self.lines)

    @override
    def on_draw(self):
        self.clear()
        self.batch.draw()


if __name__ == "__main__":
    app = App()
    pyglet.app.run()

# vim: se tw=99:
